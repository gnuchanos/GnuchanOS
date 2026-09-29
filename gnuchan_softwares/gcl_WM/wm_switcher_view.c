/*
 * wm_switcher_view.c — the overlay that is drawn, and the pictures in it.
 *
 * The window is one override-redirect window covering the screen, with the
 * grid of cells drawn on it:
 *
 *     +---------+  +---------+  +---------+
 *     | picture |  | picture |  | picture |
 *     +---------+  +---------+  +---------+
 *     |  name   |  |  name   |  |  name   |
 *     +---------+  +---------+  +---------+
 *
 * Override-redirect because the manager must not frame its own overlay, and
 * because the server maps it without asking — the same reason the bar and the
 * menu are override-redirect.
 *
 * --- where the pictures come from -----------------------------------------
 *
 * The pictures are read off the ROOT before the overlay is mapped, one per
 * window, and each is scaled into its cell and kept as a pixmap. That is a
 * decision and not an accident:
 *
 *   Asking a window for its own pixels — XGetImage on the client — returns
 *   undefined content for every part of it that something else is covering,
 *   and in a switcher almost every window is covered by something. What is on
 *   the screen is what the user was looking at a moment ago, and it is the
 *   only source that is right for a picture of it.
 *
 *   Reading the root once per window rather than capturing the screen once and
 *   cropping is the same number of server reads either way — Imlib2 grabs the
 *   rectangle it is asked for — and a crop that keeps the window's own aspect
 *   ratio cannot be expressed as an offset into a single full-screen image
 *   without doing the scaling by hand. So Imlib2 is asked for each window's
 *   rectangle and renders it into the cell at the size the cell is, which is
 *   the one part of this that is easier to read than the alternative.
 *
 * A window that is minimised is not on the screen at all, so there is nothing
 * to read: it is drawn as its own icon instead, which is what wm_icon.c read
 * from _NET_WM_ICON when the window opened. A window whose picture could not
 * be read is drawn as the flat cell colour, which is a cell that is empty and
 * labelled rather than a cell that is missing.
 *
 * The pictures are read once, at the start of the gesture, and are not read
 * again while the modifier is held. A switcher whose pictures followed the
 * screen would be a switcher with no stable thing to point at, and the desktop
 * it is showing is the one the user pressed the key on.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <Imlib2.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_style.h"
#include "wm_switcher.h"

/* The room left between the grid and the edge of the workarea. */
#define WM_SWITCHER_MARGIN 24

/* The room between two cells, and between a picture and its name. */
#define WM_SWITCHER_GAP 10

/* The room a cell's name gets under its picture: one line of text and the
   room above and below it. */
#define WM_SWITCHER_CAPTION 22

/* The most columns the grid is laid out in, and the widest a cell may be.
   Both exist so a two-window session does not get two cells the width of the
   screen, which is a grid that reads as two windows with nothing between
   them. */
#define WM_SWITCHER_MAX_COLS 4
#define WM_SWITCHER_MAX_CELL_WIDTH 420
#define WM_SWITCHER_MAX_CELL_HEIGHT 340

/* The whole view: the window, the grid it holds, and the pictures. One view
   exists at a time, so it lives here rather than on the switcher — the
   switcher is which windows and which one is chosen, and this is where they
   are drawn. */
typedef struct SwitcherView {
    /* The display is kept here because the raise below is called from the tray
       module, which has a display of its own to give but no reason to be
       handed one: the view already has the one it made its window on. */
    Display *display;
    Window window;
    int open;

    int count;              /* how many cells were laid out and drawn      */
    int columns;
    int rows;
    int cell_width;
    int cell_height;
    int picture_height;
    int origin_x;           /* the grid's top-left corner on the screen    */
    int origin_y;

    int hover;              /* the cell the pointer is over, or -1         */

    /* One picture per cell, made at open. None when the window had no
       picture to give — a minimised one, or one that could not be read — and
       the cell then falls back to the window's icon. */
    Pixmap pictures[WM_SWITCHER_MAX_CELLS];
} SwitcherView;

static SwitcherView view;

/* --- small helpers -------------------------------------------------------- */

/* Shorten text until it fits, ending it with an ellipsis. A window title can
   be any length and a cell is a fixed width, so the name has to give.
 *
 * The cut is made at a character boundary rather than at a byte: a UTF-8
 * string cut mid-character is an invalid sequence, and Xft draws its pieces as
 * nothing at all. A continuation byte — 10xxxxxx — is the middle of a
 * character and is stepped over rather than counted. */
static void fit_text(Display *display, XftFont *font, const char *text,
                     int room, char *out, unsigned int size) {
    out[0] = '\0';
    if (!display || !font || !text || room <= 0) {
        return;
    }
    if (wm_style_text_width(display, font, text) <= room) {
        snprintf(out, size, "%s", text);
        return;
    }

    size_t length = strlen(text);
    while (length > 0) {
        length--;
        if (((unsigned char)text[length] & 0xc0) == 0x80) {
            continue;
        }
        char attempt[WM_SWITCHER_NAME_LENGTH + 4];
        snprintf(attempt, sizeof(attempt), "%.*s...", (int)length, text);
        if (wm_style_text_width(display, font, attempt) <= room) {
            snprintf(out, size, "%s", attempt);
            return;
        }
    }
    snprintf(out, size, "...");
}

static int cell_index_at(int x, int y) {
    int step_x;
    int step_y;
    int column;
    int row;

    if (!view.open || view.count <= 0) {
        return -1;
    }
    step_x = view.cell_width + WM_SWITCHER_GAP;
    step_y = view.cell_height + WM_SWITCHER_GAP;
    if (x < view.origin_x || y < view.origin_y) {
        return -1;
    }

    column = (x - view.origin_x) / step_x;
    row = (y - view.origin_y) / step_y;
    if (column < 0 || column >= view.columns || row < 0 || row >= view.rows) {
        return -1;
    }
    /* The gap between two cells belongs to neither of them: a click in it is a
       click on the overlay, which is the same as a click that meant nothing
       and is read as such by the module above. */
    if ((x - view.origin_x) - column * step_x >= view.cell_width ||
        (y - view.origin_y) - row * step_y >= view.cell_height) {
        return -1;
    }

    int index = row * view.columns + column;
    if (index < 0 || index >= view.count) {
        return -1;
    }
    return index;
}

static void cell_origin(int index, int *x, int *y) {
    int column = index % view.columns;
    int row = index / view.columns;
    *x = view.origin_x + column * (view.cell_width + WM_SWITCHER_GAP);
    *y = view.origin_y + row * (view.cell_height + WM_SWITCHER_GAP);
}

/* --- the pictures --------------------------------------------------------- */

/* Read one window's picture off the screen and scale it into a pixmap of the
   picture area's size.
 *
 * The rectangle is the window's own place and size on the screen, clipped to
 * it — a window half off the edge is pictured as the part of it that was
 * visible, which is the honest answer and the one that cannot be drawn outside
 * the pixmap. The scale keeps the window's own proportions and centres what is
 * left over, so a wide window in a square cell is letterboxed rather than
 * stretched into a shape it was never in.
 *
 * Returns 0 when there was nothing to read — the window is not on the screen,
 * or Imlib2 could not take the rectangle — in which case the caller falls back
 * to the window's icon. */
static int picture_read(WmCore *core, const WmSwitcherCell *cell,
                        int width, int height, Pixmap target) {
    XWindowAttributes attributes;
    int screen_width;
    int screen_height;
    int left;
    int top;
    int right;
    int bottom;
    int source_width;
    int source_height;
    int draw_width;
    int draw_height;
    int draw_x;
    int draw_y;
    double scale;
    Imlib_Image grabbed;

    if (cell->frame == None || width <= 0 || height <= 0) {
        return 0;
    }
    if (!XGetWindowAttributes(core->display, cell->frame, &attributes)) {
        return 0;
    }
    if (attributes.map_state != IsViewable) {
        return 0;   /* put away, or on another workspace: no pixels anywhere */
    }
    /* The frame's parent is the root, so its own x and y ARE its place on the
       screen. */
    screen_width = core->width;
    screen_height = core->height;
    left = attributes.x < 0 ? 0 : attributes.x;
    top = attributes.y < 0 ? 0 : attributes.y;
    right = attributes.x + attributes.width;
    bottom = attributes.y + attributes.height;
    if (right > screen_width) {
        right = screen_width;
    }
    if (bottom > screen_height) {
        bottom = screen_height;
    }
    source_width = right - left;
    source_height = bottom - top;
    if (source_width <= 0 || source_height <= 0) {
        return 0;
    }

    /* The background first, so a picture whose proportions leave room at the
       sides is letterboxed against the panel rather than against whatever the
       server had in the pixmap. */
    XSetForeground(core->display, core->gc, core->style.panel);
    XFillRectangle(core->display, target, core->gc, 0, 0,
                   (unsigned int)width, (unsigned int)height);

    imlib_context_set_display(core->display);
    imlib_context_set_visual(DefaultVisual(core->display, core->screen));
    imlib_context_set_colormap(DefaultColormap(core->display, core->screen));
    imlib_context_set_drawable(core->root);

    /* The grab is the root's own pixels at this rectangle, which is what the
       user saw there a moment ago. It is taken before the overlay is mapped —
       see wm_switcher_view_open — so what it holds is the desktop and not the
       picture being built. */
    /* A zero mask, not a NULL one: the first argument is a Pixmap, which is an
       integer resource id, and NULL is a pointer. None is the mask that means
       "no mask", which is what is wanted — the picture is the rectangle as it
       stands. */
    grabbed = imlib_create_image_from_drawable(0, left, top,
                                               source_width, source_height, 1);
    if (!grabbed) {
        return 0;
    }

    double by_width = (double)width / (double)source_width;
    double by_height = (double)height / (double)source_height;
    scale = by_width < by_height ? by_width : by_height;
    draw_width = (int)((double)source_width * scale);
    draw_height = (int)((double)source_height * scale);
    if (draw_width < 1) draw_width = 1;
    if (draw_height < 1) draw_height = 1;
    draw_x = (width - draw_width) / 2;
    draw_y = (height - draw_height) / 2;

    imlib_context_set_image(grabbed);
    imlib_context_set_drawable(target);
    imlib_context_set_blend(1);
    imlib_render_image_on_drawable_at_size(draw_x, draw_y,
                                           draw_width, draw_height);
    imlib_free_image_and_decache();
    return 1;
}

/* Build every cell's picture. Called once, before the overlay is mapped: the
   screen still shows the desktop, and every read would otherwise find the
   overlay over the very windows it is picturing. */
static void pictures_build(WmCore *core, WmSwitcher *switcher) {
    for (int i = 0; i < switcher->count && i < WM_SWITCHER_MAX_CELLS; i++) {
        Pixmap pixmap;

        view.pictures[i] = None;
        if (switcher->cells[i].minimized) {
            continue;   /* nothing on the screen to read; the icon stands in */
        }

        pixmap = XCreatePixmap(core->display, core->root,
                               (unsigned int)view.cell_width,
                               (unsigned int)view.picture_height,
                               (unsigned int)DefaultDepth(core->display,
                                                          core->screen));
        if (pixmap == None) {
            continue;
        }
        if (picture_read(core, &switcher->cells[i], view.cell_width,
                         view.picture_height, pixmap)) {
            view.pictures[i] = pixmap;
        } else {
            XFreePixmap(core->display, pixmap);
        }
    }
}

static void pictures_free(WmCore *core) {
    for (int i = 0; i < WM_SWITCHER_MAX_CELLS; i++) {
        if (view.pictures[i] != None) {
            XFreePixmap(core->display, view.pictures[i]);
            view.pictures[i] = None;
        }
    }
}

/* --- the grid ------------------------------------------------------------- */

/* Work out the grid: how many columns, how big a cell is, and where the whole
   thing sits.
 *
 * The room is the workarea and not the screen, so the grid never lands under
 * the bar — the same rule every window follows. The columns are the square
 * root of the count, which is the arrangement that makes the cells as large as
 * they can be for any number of them, and the cell is capped in both directions
 * so two windows do not become two cells the size of the screen.
 *
 * The grid is centred in what it was given, which is what makes a count that
 * does not fill its last row read as a grid rather than as a row that ran
 * out. */
static void layout_compute(WmCore *core, const WmSwitcher *switcher) {
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    int room_width;
    int room_height;
    int columns;
    int rows;
    int cell_width;
    int cell_height;
    int grid_width;
    int grid_height;

    wm_config_workarea(&core->config, core->width, core->height,
                       &area_x, &area_y, &area_width, &area_height);

    view.count = switcher->count;
    if (view.count < 1) {
        view.count = 1;
    }

    /* The columns are the ceiling of the square root, found by walking up
       rather than by calling sqrt: the numbers here are small, the walk is
       exact, and it cannot be wrong in the last bit the way a real square root
       can. */
    columns = 1;
    while (columns * columns < view.count) {
        columns++;
    }
    if (columns > WM_SWITCHER_MAX_COLS) {
        columns = WM_SWITCHER_MAX_COLS;
    }
    if (columns > view.count) {
        columns = view.count;
    }
    rows = (view.count + columns - 1) / columns;
    view.columns = columns;
    view.rows = rows;

    room_width = area_width - 2 * WM_SWITCHER_MARGIN;
    room_height = area_height - 2 * WM_SWITCHER_MARGIN;
    if (room_width < 60) room_width = 60;
    if (room_height < 60) room_height = 60;

    cell_width = (room_width - (columns - 1) * WM_SWITCHER_GAP) / columns;
    cell_height = (room_height - (rows - 1) * WM_SWITCHER_GAP) / rows;
    if (cell_width > WM_SWITCHER_MAX_CELL_WIDTH) {
        cell_width = WM_SWITCHER_MAX_CELL_WIDTH;
    }
    if (cell_height > WM_SWITCHER_MAX_CELL_HEIGHT) {
        cell_height = WM_SWITCHER_MAX_CELL_HEIGHT;
    }
    if (cell_width < 40) cell_width = 40;
    if (cell_height < WM_SWITCHER_CAPTION + 20) {
        cell_height = WM_SWITCHER_CAPTION + 20;
    }
    view.cell_width = cell_width;
    view.cell_height = cell_height;
    view.picture_height = cell_height - WM_SWITCHER_CAPTION;
    if (view.picture_height < 1) {
        view.picture_height = 1;
    }

    grid_width = columns * cell_width + (columns - 1) * WM_SWITCHER_GAP;
    grid_height = rows * cell_height + (rows - 1) * WM_SWITCHER_GAP;
    view.origin_x = area_x + (area_width - grid_width) / 2;
    view.origin_y = area_y + (area_height - grid_height) / 2;
    if (view.origin_x < 0) view.origin_x = 0;
    if (view.origin_y < 0) view.origin_y = 0;
}

/* --- drawing -------------------------------------------------------------- */

/* One cell: the picture, the name under it, and the frame that says whether it
   is the chosen one. A cell is drawn as a piece of the overlay in the
   overlay's own coordinates, so the same two numbers work for the drawing and
   for the hit test. */
static void draw_cell(WmCore *core, const WmSwitcher *switcher, int index) {
    int x;
    int y;
    int chosen = index == switcher->selected;
    int hovered = index == view.hover;
    XftFont *font = core->style.font;
    unsigned long border = chosen ? core->style.accent
                                  : core->style.panel_edge;

    if (index < 0 || index >= switcher->count || index >= view.count) {
        return;
    }
    cell_origin(index, &x, &y);

    /* The cell's body. The panel colour first, so a picture with room at the
       sides sits on the panel and not on the desktop behind it. */
    XSetForeground(core->display, core->gc, core->style.panel);
    XFillRectangle(core->display, view.window, core->gc, x, y,
                   (unsigned int)view.cell_width,
                   (unsigned int)view.cell_height);

    /* The picture, when there is one. A window with no picture — put away, so
       nothing of it is on the screen — is drawn as its own icon in the middle
       of the cell, which is the one thing about it that still exists. An icon
       that is not there either leaves the cell empty, which is a cell that is
       labelled and blank rather than a cell that is missing. */
    if (view.pictures[index] != None) {
        XCopyArea(core->display, view.pictures[index], view.window, core->gc,
                  0, 0, (unsigned int)view.cell_width,
                  (unsigned int)view.picture_height, x, y);
    } else {
        WmFrame *frame = wm_frame_find(core, switcher->cells[index].client);
        int drawn = 0;
        /* The icon is drawn at WM_ICON_SIZE and no other size: the pixmap
           wm_icon.c made is that many pixels square and a plain X pixmap
           cannot be scaled, so asking for another size would copy a rectangle
           that is not there. Where it goes may be anywhere; how big it is may
           not. */
        if (frame) {
            drawn = wm_frame_draw_icon(core, frame, view.window,
                                       x + (view.cell_width - WM_ICON_SIZE) / 2,
                                       y + (view.picture_height - WM_ICON_SIZE) / 2,
                                       WM_ICON_SIZE);
        }
        if (!drawn) {
            /* Nothing to show: the cell keeps the panel colour it was filled
               with, so the name under it is not floating on nothing. */
            XSetForeground(core->display, core->gc, core->style.field);
            XFillRectangle(core->display, view.window, core->gc,
                           x + 8, y + 8,
                           (unsigned int)(view.cell_width - 16),
                           (unsigned int)(view.picture_height - 16));
        }
    }

    /* A cell the pointer is over, and that is not the chosen one, has its
       caption bar lit. That is what "a click lands here" looks like, and it is
       deliberately not the two-line frame below: a hovered cell must not be
       mistakable for the chosen one, which is the whole reason the choice is
       drawn the way it is. */
    if (hovered && !chosen) {
        XSetForeground(core->display, core->gc, core->style.field);
        XFillRectangle(core->display, view.window, core->gc,
                       x + 1, y + view.picture_height,
                       (unsigned int)(view.cell_width - 2),
                       (unsigned int)WM_SWITCHER_CAPTION);
    }

    /* The caption: the window's own name, centred under the picture and cut to
       the room a cell has. It is drawn in the accent when the cell is the
       chosen one, which is the second half of what "chosen" looks like — the
       frame is the first, and either alone is easy to miss on a busy cell. */
    if (font) {
        char label[WM_SWITCHER_NAME_LENGTH + 4];
        int baseline = y + view.picture_height
                     + (WM_SWITCHER_CAPTION + font->ascent - font->descent) / 2;
        fit_text(core->display, font, switcher->cells[index].name,
                 view.cell_width - 12, label, sizeof(label));
        int text_width = wm_style_text_width(core->display, font, label);
        wm_style_text(core->display, core->screen, view.window, font,
                      x + (view.cell_width - text_width) / 2, baseline, label,
                      chosen ? core->style.accent : core->style.text);
    }

    /* The frame, last so it sits over the picture. Two pixels for the chosen
       cell and one for the rest: a highlight that is only a colour is easy to
       lose against a picture, and a width is not. */
    XSetForeground(core->display, core->gc, border);
    XDrawRectangle(core->display, view.window, core->gc, x, y,
                   (unsigned int)(view.cell_width - 1),
                   (unsigned int)(view.cell_height - 1));
    if (chosen) {
        /* A second line inside the first. A highlight that is only a colour is
           easy to lose against a picture, and a width is not — which is what
           makes the choice readable on a cell full of colour. */
        XDrawRectangle(core->display, view.window, core->gc, x + 1, y + 1,
                       (unsigned int)(view.cell_width - 3),
                       (unsigned int)(view.cell_height - 3));
    }
}

void wm_switcher_view_show(WmCore *core, WmSwitcher *switcher) {
    int grid_width;
    int grid_height;

    if (!view.open || view.window == None) {
        return;
    }

    /* Everything behind the cells. It is the desktop's own background colour
       rather than a picture of the screen, and that is what makes the chosen
       cell stand out: a switcher drawn over a faded screenshot is a switcher
       whose cells have to compete with what is behind them. */
    XSetForeground(core->display, core->gc, core->style.background);
    XFillRectangle(core->display, view.window, core->gc, 0, 0,
                   (unsigned int)core->width, (unsigned int)core->height);

    grid_width = view.columns * view.cell_width +
                 (view.columns - 1) * WM_SWITCHER_GAP;
    grid_height = view.rows * view.cell_height +
                  (view.rows - 1) * WM_SWITCHER_GAP;

    /* The panel the grid sits on, so the cells read as one arrangement rather
       than as pictures dropped on the desktop. */
    XSetForeground(core->display, core->gc, core->style.panel);
    XFillRectangle(core->display, view.window, core->gc,
                   view.origin_x - WM_SWITCHER_GAP,
                   view.origin_y - WM_SWITCHER_GAP,
                   (unsigned int)(grid_width + 2 * WM_SWITCHER_GAP),
                   (unsigned int)(grid_height + 2 * WM_SWITCHER_GAP));

    for (int i = 0; i < switcher->count && i < view.count; i++) {
        draw_cell(core, switcher, i);
    }
    XFlush(core->display);
}

void wm_switcher_view_motion(WmCore *core, WmSwitcher *switcher,
                             int root_x, int root_y) {
    int was = view.hover;
    int hovered = cell_index_at(root_x, root_y);

    if (hovered == was) {
        return;
    }
    view.hover = hovered;

    /* Only the highlight moves, so only the two cells that changed are drawn
       again: the one that lost it and the one that gained it. Redrawing the
       whole grid for a pointer crossing a gap would repaint every picture on
       every step of a slow movement.
     *
     * The cell that lost the highlight is read from `was` and not from
     * view.hover, because view.hover has already been moved on: reading it
     * back would draw the cell that gained the highlight twice and leave the
     * one that lost it holding a rectangle that is no longer true. */
    for (int i = 0; i < switcher->count && i < view.count; i++) {
        if (i == was || i == hovered) {
            draw_cell(core, switcher, i);
        }
    }
    XFlush(core->display);
}

int wm_switcher_view_cell_at(WmCore *core, int root_x, int root_y) {
    (void)core;
    return cell_index_at(root_x, root_y);
}

Window wm_switcher_overlay_window(void) {
    return view.open ? view.window : None;
}

void wm_switcher_overlay_raise(void) {
    if (!view.open || view.window == None || !view.display) {
        return;
    }
    XRaiseWindow(view.display, view.window);
    XFlush(view.display);
}

/* --- opening and closing --------------------------------------------------- */

int wm_switcher_view_open(WmCore *core, WmSwitcher *switcher) {
    XSetWindowAttributes attributes;

    if (!core || !switcher) {
        return -1;
    }
    memset(&view, 0, sizeof(view));
    view.display = core->display;
    view.hover = -1;

    layout_compute(core, switcher);

    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.background;
    attributes.border_pixel = 0;
    /* The overlay takes no input of its own: the keyboard and the pointer are
       held explicitly for the length of the gesture, and an event mask here
       would only mean the same clicks arriving twice. */
    attributes.event_mask = ExposureMask;

    view.window = XCreateWindow(core->display, core->root, 0, 0,
                                (unsigned int)core->width,
                                (unsigned int)core->height, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWBackPixel |
                                CWBorderPixel | CWEventMask, &attributes);
    if (view.window == None) {
        fprintf(stderr, "gnuchanwm: switcher: cannot make the overlay\n");
        return -1;
    }
    view.open = 1;

    /* Built before the map, and this order is the whole reason the pictures
       are of anything: the overlay covers the screen, so a read taken after it
       went up would find the overlay over every window it is picturing — a
       grid of pictures of the grid. */
    pictures_build(core, switcher);

    XMapRaised(core->display, view.window);
    XSync(core->display, False);
    wm_switcher_view_show(core, switcher);
    return 0;
}

void wm_switcher_view_close(WmCore *core) {
    if (!view.open) {
        return;
    }
    view.open = 0;
    if (core && core->display) {
        pictures_free(core);
        if (view.window != None) {
            XUnmapWindow(core->display, view.window);
            XDestroyWindow(core->display, view.window);
        }
        XFlush(core->display);
    }
    view.display = NULL;
    view.window = None;
    view.count = 0;
    view.hover = -1;
}
