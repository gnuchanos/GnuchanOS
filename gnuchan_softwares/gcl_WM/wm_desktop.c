/*
 * wm_desktop.c — the desktop: the backdrop, the bar, and the pointer.
 *
 * Two things are drawn here, and on different windows on purpose.
 *
 * The backdrop is painted on the root. The root is behind everything, which is
 * exactly what a backdrop is, and it needs no window of its own.
 *
 * The bar is not. A bar painted on the root is covered the moment any window
 * overlaps where it sits — a terminal opened full width hides it, and moving
 * that terminal away leaves whatever the server had underneath, because the
 * bar was never a surface of its own to be restored. So the bar is a window:
 * an override-redirect window along the edge the config asked for. Override-
 * redirect means the server maps it without asking the manager — which is us —
 * so it can never be framed or managed by accident, and raising it is a
 * restack rather than a repaint of something underneath.
 *
 * The widgets share the bar by weight: a widget with text asks for the room
 * that text takes, and the flexibles split whatever is left. That is what puts
 * a clock at the right edge whether the label beside it is two words or
 * twenty.
 *
 * One widget is not text at all. The group box is the list of the programs
 * that are open, drawn as their own icons, and a click on one of them goes to
 * that window — see bar_collect_icons() and the WM_WIDGET_GROUP_BOX branch of
 * the drawing loop. It is placed in the strip like every other widget, so a
 * config that wants the open windows in the middle of the bar writes the group
 * box in the middle of the list and gets them there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_image.h"
#include "wm_workspace.h"

/* The room a widget's text gets on either side of it. */
#define WM_BAR_PADDING 8

/* How often the tick runs, in milliseconds. It is fast enough that the clock
   keeps time to the second and slow enough that an idle session is not
   repainting anything worth measuring. */
#define WM_TICK_MS 500

/* The bar's window. Module state rather than core state: nothing but this file
   draws or moves the bar. */
static Window bar_window = None;

/* The bar's background picture, when the script named one through
   BackgroundImage. It is kept here, decoded once at the bar's height, so the
   once-a-second repaint copies a picture rather than reading a file; see
   wm_image.c. It is only ever used on the bar, so it lives beside the bar's
   window rather than on the core. */
static WmImage bar_image;

/* Declared ahead of the public entry points, which are written next to the
   raise they pair with rather than next to the drawing they call. */
static void desktop_configure_bar(WmCore *core);
static void desktop_bar(WmCore *core);

/* --- fonts ---------------------------------------------------------------- */

/* A bar's widget may name a font of its own, so the same bar can hold a big
   clock and small labels. Loading a font is not free and the bar is repainted
   every second, so what has been loaded is remembered here and looked up by
   the spec it was loaded from. */
typedef struct BarFont {
    char spec[WM_CONFIG_TEXT_LENGTH + 32];
    XftFont *font;
} BarFont;

static BarFont bar_fonts[WM_CONFIG_MAX_WIDGETS];
static int bar_font_count = 0;

static XftFont *bar_font_for(WmCore *core, const WmWidget *widget) {
    /* A widget that named no font uses the desktop's, which is the one font
       everything else on this desktop is drawn in. */
    if (!widget->font_family[0] || widget->font_size <= 0) {
        return core->style.font;
    }

    /* Xft names a font as a list of properties, not as an XLFD pattern: the
       family is the part before the first colon and everything after it is a
       property, so a family with a space in it needs no escaping and a size is
       written as pixelsize rather than as a field in a dash-separated string.
       pixelsize rather than size because the bar is measured in pixels. */
    char spec[WM_CONFIG_TEXT_LENGTH + 32];
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d",
             widget->font_family, widget->font_size);

    for (int i = 0; i < bar_font_count; i++) {
        if (strcmp(bar_fonts[i].spec, spec) == 0) {
            return bar_fonts[i].font;
        }
    }

    /* The size-specific name first, then the family alone, then the desktop's
       font: a machine that has the family but not at that size still draws a
       bar, and one with neither draws it in the desktop's font. */
    XftFont *font = wm_style_open_font(core->display, core->screen, spec);
    if (!font) {
        font = wm_style_open_font(core->display, core->screen,
                                  widget->font_family);
    }
    if (!font) {
        return core->style.font;
    }

    if (bar_font_count < WM_CONFIG_MAX_WIDGETS) {
        snprintf(bar_fonts[bar_font_count].spec,
                 sizeof(bar_fonts[bar_font_count].spec), "%s", spec);
        bar_fonts[bar_font_count].font = font;
        bar_font_count++;
    }
    return font;
}

static void bar_free_fonts(WmCore *core) {
    for (int i = 0; i < bar_font_count; i++) {
        if (bar_fonts[i].font) {
            XftFontClose(core->display, bar_fonts[i].font);
        }
    }
    bar_font_count = 0;
}

/* --- colours --------------------------------------------------------------- */

/* Resolved pixels, remembered by the name and the fallback they came from.
 *
 * A colour name costs an XAllocNamedColor round trip to the server, and the
 * bar is redrawn every second — once for the strip and twice for each widget.
 * Resolving the same six or seven names on every one of those redraws is work
 * that produces the same pixels every time, so the answers are kept here.
 *
 * The fallback is part of the key because it is what an unresolvable name
 * resolves to: a name that failed for one caller must not hand its fallback to
 * another that would have wanted a different one. */
typedef struct BarColour {
    char name[WM_CONFIG_TEXT_LENGTH];
    unsigned long fallback;
    unsigned long pixel;
} BarColour;

static BarColour bar_colours[2 * WM_CONFIG_MAX_WIDGETS + 8];
static int bar_colour_count = 0;

static unsigned long bar_colour(WmCore *core, const char *name,
                                unsigned long fallback) {
    if (!name || !name[0]) {
        return fallback;
    }
    for (int i = 0; i < bar_colour_count; i++) {
        if (bar_colours[i].fallback == fallback &&
            strcmp(bar_colours[i].name, name) == 0) {
            return bar_colours[i].pixel;
        }
    }

    unsigned long pixel = wm_style_colour(core->display, core->screen,
                                          name, fallback);
    if (bar_colour_count <
        (int)(sizeof(bar_colours) / sizeof(bar_colours[0]))) {
        snprintf(bar_colours[bar_colour_count].name,
                 sizeof(bar_colours[bar_colour_count].name), "%s", name);
        bar_colours[bar_colour_count].fallback = fallback;
        bar_colours[bar_colour_count].pixel = pixel;
        bar_colour_count++;
    }
    return pixel;
}

/* --- the buffer the bar is drawn into -------------------------------------- */

/* The bar is painted off-screen and copied up in one operation.
 *
 * Painted straight onto its window it is seen half-made: the strip is filled,
 * then each widget's background, then the text of each — and the clock redraws
 * the whole bar once a second, so that order repeats once a second. On a wide
 * bar that is a visible sweep of colour across the screen every second, which
 * is the spasm this buffer exists to remove. One copy per redraw is what makes
 * the bar appear complete or not at all.
 *
 * This is as close as a window manager comes to the thing the config calls it:
 * there is no frame to wait for here, so "not until it is finished" is
 * achieved by never letting the unfinished version reach the screen. */
static Pixmap bar_buffer = None;
static int bar_buffer_width = 0;
static int bar_buffer_height = 0;

/* Get a buffer of exactly this size, making one or throwing away the old one.
   A buffer of the wrong size is not stretched: a stretched bar is a blur. */
static Drawable bar_target(WmCore *core, int width, int height) {
    if (bar_buffer != None && bar_buffer_width == width &&
        bar_buffer_height == height) {
        return bar_buffer;
    }
    if (bar_buffer != None) {
        XFreePixmap(core->display, bar_buffer);
        bar_buffer = None;
    }
    if (width <= 0 || height <= 0) {
        return bar_window;
    }
    bar_buffer = XCreatePixmap(core->display, bar_window,
                               (unsigned int)width, (unsigned int)height,
                               (unsigned int)DefaultDepth(core->display,
                                                          core->screen));
    if (bar_buffer == None) {
        return bar_window;   /* no room for one: draw straight, flicker and all */
    }
    bar_buffer_width = width;
    bar_buffer_height = height;
    return bar_buffer;
}

/* --- the pointer ---------------------------------------------------------- */

/* The alien-violet arrow, one string per row: 'X' is a pixel, '.' is not.
 *
 * It is drawn here rather than asked for from the pointer theme because the
 * login screen draws the same shape the same way, and the two have to look
 * like one system: a session that logs in under one pointer and lands on
 * another is two desktops, not one. A theme cursor is also the theme's answer
 * to "what colour is the pointer", which would override the choice — and
 * XRecolorCursor does nothing to a modern theme's ARGB image anyway, so the
 * recolor below the old code tried was silently ignored. A shape built at this
 * level has no theme behind it to disagree with.
 *
 * This is the same table as GnuChanDM's dm_core.c, kept in step by hand; the
 * two binaries share no code, so the shape is the smallest thing that can be
 * duplicated to make the login screen and the desktop match. */
static const char *const DESKTOP_CURSOR_ARROW[15] = {
    "X..............",
    "XX.............",
    "X.X............",
    "X..X...........",
    "X...X..........",
    "X....X.........",
    "X.....X........",
    "X......X.......",
    "X.......X......",
    "X........X.....",
    "X.....XXXXX....",
    "X..X...X.......",
    "X.X.X...X......",
    "XX...X...X.....",
    "X.....X...X....",
};

#define DESKTOP_CURSOR_SIDE 15

/* One 1-bit pixmap holding the arrow. With `grow` set the shape is fattened
   by a pixel to make the mask: the mask is what the server draws the two
   colours through, so a mask larger than the source puts the background colour
   as an outline around every purple pixel — which is what keeps the pointer
   visible over a light patch as well as a dark one. */
static Pixmap desktop_cursor_shape(WmCore *core, int grow) {
    Pixmap pixmap = XCreatePixmap(core->display, core->root,
                                  DESKTOP_CURSOR_SIDE, DESKTOP_CURSOR_SIDE, 1);
    if (pixmap == None) {
        return None;
    }
    GC gc = XCreateGC(core->display, pixmap, 0, NULL);
    XSetForeground(core->display, gc, 0);
    XFillRectangle(core->display, pixmap, gc, 0, 0,
                   DESKTOP_CURSOR_SIDE, DESKTOP_CURSOR_SIDE);
    XSetForeground(core->display, gc, 1);

    for (int y = 0; y < DESKTOP_CURSOR_SIDE; y++) {
        for (int x = 0; x < DESKTOP_CURSOR_SIDE; x++) {
            int set = DESKTOP_CURSOR_ARROW[y][x] == 'X';
            if (!set && grow) {
                for (int dy = -1; dy <= 1 && !set; dy++) {
                    for (int dx = -1; dx <= 1 && !set; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny >= 0 && ny < DESKTOP_CURSOR_SIDE &&
                            nx >= 0 && nx < DESKTOP_CURSOR_SIDE &&
                            DESKTOP_CURSOR_ARROW[ny][nx] == 'X') {
                            set = 1;
                        }
                    }
                }
            }
            if (set) {
                XDrawPoint(core->display, pixmap, gc, x, y);
            }
        }
    }

    XFreeGC(core->display, gc);
    return pixmap;
}

/* The pointer, asked for from the cursor theme the settings script named.
 *
 * The root cursor used to be the drawn arrow below and nothing else, on the
 * reasoning that the login screen drew the same shape so the two matched. That
 * left the theme cursor invisible on the one surface the user sees it on most:
 * gcl_themes.Theme_cursor(...) set XCURSOR_THEME and every program honoured
 * it, but the desktop kept drawing its own arrow, so a cursor theme was chosen
 * and the pointer over the desktop never changed.
 *
 * XcursorLibraryLoadCursor is what loads a theme's cursor by name — plain X
 * cannot, because the core protocol's cursors are the server's own shapes and
 * know nothing about a theme. It reads XCURSOR_THEME and XCURSOR_SIZE from the
 * environment, which wm_theme_apply() has already set, so asking for the
 * ordinary arrow here is asking for the theme's arrow. None is returned when
 * the theme or the shape is not there, and the caller then falls back to the
 * drawn arrow, so a machine with no cursor theme still gets a visible pointer. */
static Cursor desktop_theme_cursor(WmCore *core) {
    /* "default" is the name a theme is expected to answer with its ordinary
       arrow: it is the name every toolkit asks for when it is told nothing
       else, and the one a cursor theme is required to provide. "left_ptr" is
       the freedesktop spelling of the same thing and is asked for second. */
    Cursor cursor = XcursorLibraryLoadCursor(core->display, "default");
    if (cursor == None) {
        cursor = XcursorLibraryLoadCursor(core->display, "left_ptr");
    }
    return cursor;
}

/* The pointer the desktop draws when no theme answers. XCreatePixmapCursor
   needs the two colours allocated; a display that refuses them (one with no
   colour at all) falls back to the font cursor, because a pointer nobody can
   see is worse than an ugly one. */
static Cursor desktop_draw_cursor(WmCore *core) {
    Pixmap source = desktop_cursor_shape(core, 0);
    Pixmap mask = desktop_cursor_shape(core, 1);
    if (source == None || mask == None) {
        if (source != None) XFreePixmap(core->display, source);
        if (mask != None) XFreePixmap(core->display, mask);
        return XCreateFontCursor(core->display, XC_left_ptr);
    }

    XColor foreground;   /* the accent purple #c77dff, as the login screen uses */
    XColor background;   /* the desktop background #1a0b2e                     */
    Colormap cmap = DefaultColormap(core->display, core->screen);
    if (!XParseColor(core->display, cmap, "#c77dff", &foreground) ||
        !XParseColor(core->display, cmap, "#1a0b2e", &background) ||
        !XAllocColor(core->display, cmap, &foreground) ||
        !XAllocColor(core->display, cmap, &background)) {
        XFreePixmap(core->display, source);
        XFreePixmap(core->display, mask);
        return XCreateFontCursor(core->display, XC_left_ptr);
    }

    /* The hotspot is the tip of the arrow, so the point the user aims at is
       the point that lands. */
    Cursor cursor = XCreatePixmapCursor(core->display, source, mask,
                                        &foreground, &background, 0, 0);
    XFreePixmap(core->display, source);
    XFreePixmap(core->display, mask);
    return cursor;
}

/* The pointer set on the root: the theme's if it has one, and the drawn arrow
   if it does not. The theme is asked for first because a cursor theme the
   settings script named is the theme the user asked for, and a drawn arrow
   over everything else would be the one place it did not apply. */
static Cursor desktop_make_cursor(WmCore *core) {
    Cursor from_theme = desktop_theme_cursor(core);
    if (from_theme != None) {
        return from_theme;
    }
    return desktop_draw_cursor(core);
}

/* --- what a widget says --------------------------------------------------- */

/* Room for one workspace cell. It holds either a number or the symbol the
   widget names, so it is as wide as the wider of the two things a cell can
   be — a widget's symbol is the config's own string and can be anything. */
#define WM_LAYOUT_CELL_LENGTH 16

/* The gap between two window icons in the group box. */
#define WM_ICON_GAP 2

/* Where each open window's icon was last drawn on the bar, so a click on the
   bar is turned back into the window it landed on. Filled by desktop_bar()
   from the same numbers the drawing uses, so a click always lands on what was
   drawn — the same rule the desktop menu follows. */
typedef struct BarIconBox {
    int x, y, width, height;
    Window client;
} BarIconBox;

static BarIconBox bar_icon_boxes[WM_MAX_FRAMES];
static int bar_icon_count = 0;

/* The windows the group box lists: every open window on the current
   workspace, in the frame table's order. A minimised window is not on the
   screen, so it is not listed — the switcher key is what reaches it. Filled
   once per bar repaint, so the measuring pass and the drawing pass walk the
   same list and cannot disagree about how many icons there are. */
static WmFrame *bar_icon_frames[WM_MAX_FRAMES];
static int bar_icon_total = 0;

static void bar_collect_icons(WmCore *core) {
    bar_icon_total = 0;
    for (int i = 0; i < core->frame_count && i < WM_MAX_FRAMES; i++) {
        WmFrame *frame = &core->frames[i];
        if (frame->minimized) {
            continue;
        }
        if (frame->workspace != core->current_workspace) {
            continue;
        }
        bar_icon_frames[bar_icon_total++] = frame;
    }
}

/* The cells of a layout widget, one per workspace, and the number each cell
   stands for.
 *
 * A layout widget is not one label like the others: it is a row of numbers,
 * and the one that is current has to look different from the rest. A single
 * string cannot be given two colours, so the widget is broken into one label
 * per workspace here and each is drawn in its own cell — see the drawing loop
 * for what the current one is drawn in.
 *
 * The range is the widget's own start..end, less anything past the number of
 * workspaces the session actually has, which is read from the script. A
 * script drawing 0..5 on a session with six workspaces gets six cells; the
 * same script on a session that named four gets four, and the bar does not
 * offer a workspace the key cannot reach. */
static int bar_layout_cells(WmCore *core, const WmWidget *widget,
                            char cells[][WM_LAYOUT_CELL_LENGTH],
                            int *numbers, int max) {
    int available = wm_workspace_count(core);
    int written = 0;
    for (int number = widget->start_layout;
         number <= widget->end_layout && written < max; number++) {
        if (number < 0 || number >= available) {
            continue;
        }
        /* What a cell shows: the widget's symbol when it named one, and the
           workspace number when it did not. A symbol is what a desktop with
           its own furniture wants — dots, squares, anything that reads as a
           row of places rather than a row of numbers — and an empty symbol
           means "number them", which is what every other bar does.

           A cell is not always one character: a symbol like " [●] " is a
           whole cell with its own spacing, which is why this is a string. */
        if (widget->symbol[0]) {
            snprintf(cells[written], WM_LAYOUT_CELL_LENGTH, "%s",
                     widget->symbol);
        } else {
            snprintf(cells[written], WM_LAYOUT_CELL_LENGTH, "%d", number);
        }
        numbers[written] = number;
        written++;
    }
    return written;
}

/* The room between two workspace cells: the widget's own Gap when it named
   one, and the bar's default spacing when it did not. A script that writes Gap
   gets the spacing it asked for; one that never mentions it gets the value
   every other widget is laid out with, so the row of numbers looks the same as
   it always did. */
static int bar_layout_gap(const WmWidget *widget) {
    return widget->gap > 0 ? widget->gap : WM_BAR_PADDING;
}

/* The width a group box's separator takes, or 0 when the widget named none.
   The mark is text in the widget's own font, so it is measured the same way
   every other label on the bar is — which is what lets the measuring pass and
   the drawing pass agree about how wide the box is. */
static int bar_separator_width(WmCore *core, const WmWidget *widget,
                               XftFont *font) {
    if (!widget->separator[0] || !font) {
        return 0;
    }
    return wm_style_text_width(core->display, font, widget->separator);
}

/* The room between two icons of a group box: the plain gap when the widget
   drew no separator, and the gap, the mark and the gap again when it did. The
   mark is never flush against an icon — a "|" touching an icon reads as part
   of it — so it is given the bar's own spacing on either side. */
static int bar_icon_gap(WmCore *core, const WmWidget *widget, XftFont *font) {
    int separator = bar_separator_width(core, widget, font);
    return separator > 0 ? WM_ICON_GAP + separator + WM_ICON_GAP : WM_ICON_GAP;
}

/* What a widget shows, in a buffer. This is the one place a widget's kind
   becomes words, so the layout below never has to know which kind it is looking
   at — it measures what this produced and draws it. The group box produces
   nothing here: it is icons, not words, and is measured and drawn as icons. */
static void bar_widget_label(const WmWidget *widget, char *out,
                             unsigned int size) {
    out[0] = '\0';
    switch (widget->kind) {
    case WM_WIDGET_CURRENT_LAYOUT:
        /* Drawn cell by cell rather than from this buffer, so that the current
           workspace can be drawn unlike the rest. See bar_layout_cells(). */
        break;
    case WM_WIDGET_GROUP_BOX:
        /* Words are not what this widget is for; it is the open windows' own
           icons. See the WM_WIDGET_GROUP_BOX branch of the drawing loop. */
        break;
    case WM_WIDGET_TEXT_BOX:
        snprintf(out, size, "%s", widget->text);
        break;
    case WM_WIDGET_CLOCK: {
        const char *format = widget->format[0] ? widget->format : "%H:%M";
        time_t now = time(NULL);
        struct tm *local = localtime(&now);
        if (local) {
            strftime(out, size, format, local);
        }
        break;
    }
    case WM_WIDGET_EMPTY_SPACE:
        break;
    }
}

/* --- the bar window ------------------------------------------------------- */

/* Put the bar window where the config says it goes, making it the first time.
   Called at start, on a screen resize and after a reload — so the strip follows
   the config rather than being fixed at whatever the session started with. */
static void desktop_configure_bar(WmCore *core) {
    const WmBar *bar = &core->config.bar;

    if (!bar->present || bar->size <= 0) {
        if (bar_window != None) {
            XUnmapWindow(core->display, bar_window);
        }
        return;
    }

    int height = bar->size;
    int y = strcmp(bar->position, "bottom") == 0 ? core->height - height : 0;

    if (bar_window == None) {
        XSetWindowAttributes attributes;
        memset(&attributes, 0, sizeof(attributes));
        /* Override-redirect: the server maps this window without asking the
           manager, so the bar can never be framed, focused or managed. */
        attributes.override_redirect = True;
        attributes.event_mask = ExposureMask | ButtonPressMask;
        attributes.background_pixel = BlackPixel(core->display, core->screen);
        attributes.border_pixel = 0;

        bar_window = XCreateWindow(core->display, core->root,
                                   0, y,
                                   (unsigned int)core->width,
                                   (unsigned int)height,
                                   0, CopyFromParent, InputOutput,
                                   CopyFromParent,
                                   CWOverrideRedirect | CWEventMask |
                                   CWBackPixel | CWBorderPixel,
                                   &attributes);
        if (bar_window == None) {
            fprintf(stderr, "gnuchanwm: cannot make the bar window\n");
            return;
        }
    } else {
        XMoveResizeWindow(core->display, bar_window, 0, y,
                          (unsigned int)core->width, (unsigned int)height);
    }
    XMapRaised(core->display, bar_window);
    XFlush(core->display);
}

/* Put the bar above everything.
 *
 * Called at the moments a window can have got above it — a window was raised,
 * or one was just mapped — and not on a timer. A bar that re-raised itself
 * several times a second was restacking the whole desktop every half second,
 * which is invisible while nothing is happening and is not while a window is
 * being dragged: every restack makes the window underneath repaint, and a
 * repaint on a timer looks like a flicker that nothing is causing. */
void wm_desktop_raise_bar(WmCore *core) {
    if (bar_window != None) {
        XRaiseWindow(core->display, bar_window);
        XFlush(core->display);
    }
}

/* Draw the bar again. See the header for why this is not simply bar(). */
void wm_desktop_repaint(WmCore *core) {
    desktop_configure_bar(core);
    desktop_bar(core);
    wm_desktop_raise_bar(core);
}

/* The bar: the strip, then its widgets, left to right. See the note at the top
   of the file for why the width is shared rather than each widget placed. */
static void desktop_bar(WmCore *core) {
    if (bar_window == None) {
        return;
    }
    const WmBar *bar = &core->config.bar;
    if (!bar->present || bar->size <= 0) {
        return;
    }
    int height = bar->size;
    int bar_width = core->width;

    /* Which windows the group box lists, worked out once so the measuring
       pass and the drawing pass below agree about how many there are. */
    bar_collect_icons(core);
    bar_icon_count = 0;

    /* Everything is drawn into the buffer and put on the window in one copy at
       the end, when the config asked for it. See bar_target() for why. */
    Drawable canvas = bar->vsync ? bar_target(core, bar_width, height)
                                 : bar_window;

    unsigned long strip_colour = bar_colour(core, bar->background,
                                            core->style.panel);
    XSetForeground(core->display, core->gc, strip_colour);
    XFillRectangle(core->display, canvas, core->gc,
                   0, 0, (unsigned int)bar_width, (unsigned int)height);

    /* The strip's picture, when the script named one through BackgroundImage.
       It is drawn over the flat colour rather than instead of it, so the
       colour underneath is what shows wherever the picture has a transparent
       pixel — and the colour is the fallback when the file is missing or does
       not decode, which is what the script's comment promises ("if this place
       is empty, it will use BackgroundColor").

       The picture is prepared once at the bar's height and repeated across the
       width; see wm_image.c for why a picture is tiled rather than stretched.
       Loading is cheap here because a name at a height already loaded is kept,
       so the once-a-second repaint only ever copies. */
    if (bar->background_image[0]) {
        if (wm_image_load(core, &bar_image, bar->background_image,
                          height) == 0) {
            wm_image_draw(core, &bar_image, canvas, 0, 0, bar_width);
        }
    }

    if (bar->widget_count == 0) {
        if (canvas != bar_window) {
            XCopyArea(core->display, canvas, bar_window, core->gc, 0, 0,
                      (unsigned int)bar_width, (unsigned int)height, 0, 0);
        }
        XFlush(core->display);
        return;
    }

    /* Measure once, then place. A widget's label is produced here and kept, so
       the measuring and the drawing agree about what is being drawn.
     *
     * A layout widget is the exception: its label is empty because it is not
       one string but a row of numbers, and the current one is drawn unlike the
       rest. Its cells are worked out here instead, and its width is the room
       they take — so the measuring still happens in one place, and the drawing
       loop below only has to draw what was measured. */
    static char labels[WM_CONFIG_MAX_WIDGETS][WM_CONFIG_TEXT_LENGTH];
    static char layout_cells[WM_CONFIG_MAX_WIDGETS][WM_WORKSPACE_MAX]
                            [WM_LAYOUT_CELL_LENGTH];
    static int layout_numbers[WM_CONFIG_MAX_WIDGETS][WM_WORKSPACE_MAX];
    int layout_cell_count[WM_CONFIG_MAX_WIDGETS];
    XftFont *fonts[WM_CONFIG_MAX_WIDGETS];
    int widths[WM_CONFIG_MAX_WIDGETS];
    int flexible_count = 0;
    int needed = 0;

    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        layout_cell_count[i] = 0;
        bar_widget_label(widget, labels[i], sizeof(labels[i]));
        fonts[i] = bar_font_for(core, widget);
        if (widget->kind == WM_WIDGET_EMPTY_SPACE) {
            /* Two kinds of space, decided by Expanding. An expanding one takes
               a share of what is left over and is counted here so the draw
               loop can divide by it; a fixed one asks for its own width in
               pixels and is measured like a label, which is what makes
               Horizontal a real width rather than a hint. */
            if (widget->expanding) {
                widths[i] = 0;
                flexible_count++;
            } else {
                widths[i] = widget->horizontal;
                needed += widths[i];
            }
        } else if (widget->kind == WM_WIDGET_GROUP_BOX) {
            /* The open windows' icons, laid out one after another with a gap
               between them, and the widget's separator drawn in each gap when
               it named one. The box is exactly as wide as they take and no
               wider, so an empty desktop leaves it at nothing rather than as
               a stub of bar with no meaning. The separator's room is measured
               in — one between each pair, and none after the last — so the box
               is as wide as it draws. */
            if (bar_icon_total > 0) {
                int gap = bar_icon_gap(core, widget, fonts[i]);
                widths[i] = bar_icon_total * WM_ICON_SIZE +
                            (bar_icon_total - 1) * gap;
            } else {
                widths[i] = 0;
            }
            needed += widths[i];
        } else if (widget->kind == WM_WIDGET_CURRENT_LAYOUT) {
            layout_cell_count[i] = bar_layout_cells(
                core, widget, layout_cells[i], layout_numbers[i],
                WM_WORKSPACE_MAX);
            /* The widest cell, so every number gets the same room and the row
               does not shift when the current one changes from "9" to "10". */
            int cell = 0;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int width = wm_style_text_width(core->display, fonts[i],
                                                layout_cells[i][c]);
                if (width > cell) {
                    cell = width;
                }
            }
            int gap = bar_layout_gap(widget);
            widths[i] = cell == 0
                            ? 0
                            : layout_cell_count[i] * (cell + gap);
            needed += widths[i];
        } else {
            widths[i] = wm_style_text_width(core->display, fonts[i],
                                            labels[i]) +
                        2 * WM_BAR_PADDING;
            needed += widths[i];
        }
    }

    int leftover = core->width - needed;
    if (leftover < 0) {
        leftover = 0;
    }
    int share = flexible_count > 0 ? leftover / flexible_count : 0;

    int x = 0;
    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        /* Only an expanding space takes the shared room. A fixed one was
           measured like a label and keeps the width it asked for. */
        int width = (widget->kind == WM_WIDGET_EMPTY_SPACE && widget->expanding)
                        ? share : widths[i];
        if (width < 0) {
            width = 0;
        }

        unsigned long background = bar_colour(core, widget->background,
                                              strip_colour);
        XSetForeground(core->display, core->gc, background);
        XFillRectangle(core->display, canvas, core->gc,
                       x, 0, (unsigned int)width, (unsigned int)height);

        if (widget->kind == WM_WIDGET_CURRENT_LAYOUT && fonts[i]) {
            /* One number per cell, and the current workspace drawn in the
               accent so the bar says where the user is. Everything else on the
               bar is drawn in the widget's own foreground; the current cell is
               the one exception, which is the whole reason layout is drawn
               here rather than as one label with the others. */
            unsigned long foreground = bar_colour(core, widget->foreground,
                                                  core->style.text);
            unsigned long active =
                bar_colour(core, core->config.active_border, core->style.accent);
            int cell = 0;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int room = wm_style_text_width(core->display, fonts[i],
                                               layout_cells[i][c]);
                if (room > cell) {
                    cell = room;
                }
            }
            int baseline =
                (height + fonts[i]->ascent - fonts[i]->descent) / 2;
            int gap = bar_layout_gap(widget);
            int cursor = x;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int current = layout_numbers[i][c] == core->current_workspace;
                wm_style_text(core->display, core->screen, canvas, fonts[i],
                              cursor + gap / 2, baseline,
                              layout_cells[i][c],
                              current ? active : foreground);
                cursor += cell + gap;
            }
        } else if (widget->kind == WM_WIDGET_GROUP_BOX) {
            /* The open windows, as their own icons: one per program, centred
               in the strip, each recorded in bar_icon_boxes so a click on it
               can be turned back into that window. A window with no icon of
               its own — a bare X program such as xterm usually has none — is
               left as a gap rather than a placeholder, because a row of
               identical placeholders would say less than nothing. */
            int icon_y = (height - WM_ICON_SIZE) / 2;
            int gap = bar_icon_gap(core, widget, fonts[i]);
            int separator = bar_separator_width(core, widget, fonts[i]);
            unsigned long sep_colour =
                bar_colour(core, widget->separator_color,
                           bar_colour(core, widget->foreground,
                                      core->style.text));
            int sep_baseline =
                (height + (fonts[i] ? fonts[i]->ascent : 8) -
                 (fonts[i] ? fonts[i]->descent : 2)) / 2;
            int cursor = x + WM_ICON_GAP / 2;
            for (int k = 0; k < bar_icon_total && k < WM_MAX_FRAMES; k++) {
                WmFrame *frame = bar_icon_frames[k];
                wm_frame_draw_icon(core, frame, canvas, cursor, icon_y,
                                   WM_ICON_SIZE);
                if (bar_icon_count < WM_MAX_FRAMES) {
                    bar_icon_boxes[bar_icon_count].x = cursor;
                    bar_icon_boxes[bar_icon_count].y = icon_y;
                    bar_icon_boxes[bar_icon_count].width = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].height = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].client = frame->client;
                    bar_icon_count++;
                }
                cursor += WM_ICON_SIZE;
                /* The mark between this icon and the next, not after the last
                   one: a trailing "|" would read as a separator with nothing
                   on its right. The advance is the same gap the measuring pass
                   used, so the icons stay where they were measured. */
                if (k + 1 < bar_icon_total) {
                    if (separator > 0 && fonts[i]) {
                        wm_style_text(core->display, core->screen, canvas,
                                      fonts[i], cursor + WM_ICON_GAP,
                                      sep_baseline, widget->separator,
                                      sep_colour);
                    }
                    cursor += gap;
                }
            }
        } else if (labels[i][0] && fonts[i]) {
            unsigned long foreground = bar_colour(core, widget->foreground,
                                                  core->style.text);
            int baseline = (height + fonts[i]->ascent - fonts[i]->descent) / 2;
            wm_style_text(core->display, core->screen, canvas, fonts[i],
                          x + WM_BAR_PADDING, baseline, labels[i], foreground);
        }
        x += width;
    }

    /* The whole bar, finished, onto the window in one operation. */
    if (canvas != bar_window) {
        XCopyArea(core->display, canvas, bar_window, core->gc, 0, 0,
                  (unsigned int)bar_width, (unsigned int)height, 0, 0);
    }
    XFlush(core->display);
}

/* --- painting ------------------------------------------------------------- */

/* The backdrop. Called at start and on every Expose of the root, so a session
   that logs in over an old one does not inherit its picture. */
static void desktop_paint_background(WmCore *core) {
    XSetForeground(core->display, core->gc, core->style.background);
    XFillRectangle(core->display, core->root, core->gc,
                   0, 0,
                   (unsigned int)core->width, (unsigned int)core->height);
    XFlush(core->display);
}

/* --- the module ----------------------------------------------------------- */

static int desktop_init(WmCore *core) {
    XSetWindowBackground(core->display, core->root, core->style.background);
    XClearWindow(core->display, core->root);

    /* The pointer is drawn before anything else, so the first thing the user
       sees on an empty desktop is the same arrow the login screen showed. */
    {
        Cursor cursor = desktop_make_cursor(core);
        if (cursor != None) {
            XDefineCursor(core->display, core->root, cursor);
            core->root_cursor = cursor;
        }
    }

    desktop_paint_background(core);
    desktop_configure_bar(core);
    desktop_bar(core);
    return 0;
}

/* A press on the bar. The only thing on it that is a target is a window icon,
   so a press is turned back into the window it landed on and that window is
   activated; a press on any other part of the bar does nothing, because the
   rest of the bar is not a control. */
static void desktop_bar_press(WmCore *core, XButtonEvent *press) {
    for (int i = 0; i < bar_icon_count; i++) {
        BarIconBox *box = &bar_icon_boxes[i];
        if (press->x >= box->x && press->x < box->x + box->width &&
            press->y >= box->y && press->y < box->y + box->height) {
            WmFrame *frame = wm_frame_find(core, box->client);
            if (frame) {
                wm_frame_activate(core, frame);
            }
            return;
        }
    }
}

static void desktop_event(WmCore *core, XEvent *event) {
    if (bar_window != None && event->xany.window == bar_window) {
        if (event->type == Expose) {
            desktop_bar(core);
            return;
        }
        if (event->type == ButtonPress) {
            desktop_bar_press(core, &event->xbutton);
            return;
        }
    }

    switch (event->type) {
    case Expose:
        if (event->xexpose.window == core->root) {
            desktop_paint_background(core);
        }
        break;
    case ConfigureNotify:
        if (event->xconfigure.window == core->root) {
            core->width = event->xconfigure.width;
            core->height = event->xconfigure.height;
            desktop_paint_background(core);
            desktop_configure_bar(core);
            desktop_bar(core);
        }
        break;
    case MapNotify:
        /* A window has just been placed on top of everything. The bar belongs
           above it, so it is raised again — this is the one event that tells
           the bar it has been covered. It is also the moment a new program's
           icon has to appear, so the bar is drawn again. */
        if (event->xmap.window != bar_window) {
            desktop_bar(core);
            wm_desktop_raise_bar(core);
        }
        break;
    default:
        break;
    }
}

/* The idle work of the desktop: keep the clock moving.
 *
 * The config is deliberately not looked at here. Reloading is the reload key's
 * job and only its job — a person presses Ctrl+Alt+R when they want the script
 * read again, and the desktop does not watch the file behind their back. The
 * two things a watch would have to get right, a change noticed the instant it
 * is saved and a mistake never applied half-read, are what the key gives
 * instead: one press reads the file, applies it whole or not at all, and puts
 * a mistake in a window rather than in a log nobody is reading. A tick that
 * re-read the file several times a second bought nothing but a stat() every
 * half second for a file that almost never changes. */
static void desktop_tick(WmCore *core) {
    static long long last_stamp = -1;

    /* The clock only needs a repaint when the second it shows has changed; a
       tick that redraws an unchanged clock is a wake-up per half second that
       buys nothing. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long stamp = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
    if (last_stamp < 0) {
        last_stamp = stamp;
    } else if (stamp - last_stamp >= 1000) {
        last_stamp = stamp;
        desktop_bar(core);
    }

    /* Nothing is raised here on purpose. The bar only has to go back on top
       when something has been put above it, and that is a moment — a window
       raised or mapped — not an interval. Raising it here as well would
       restack the desktop every half second for no reason, and each restack
       is a repaint of whatever is under the bar. */
}

static void desktop_cleanup(WmCore *core) {
    /* The buffer is a pixmap of the bar window and would be freed with it, but
       it is freed here so the size it recorded cannot survive a restart as a
       stale answer to "is a buffer of this size already there". */
    if (bar_buffer != None) {
        XFreePixmap(core->display, bar_buffer);
        bar_buffer = None;
        bar_buffer_width = 0;
        bar_buffer_height = 0;
    }
    wm_image_free(core, &bar_image);
    bar_colour_count = 0;
    bar_icon_total = 0;
    bar_icon_count = 0;

    if (bar_window != None) {
        XDestroyWindow(core->display, bar_window);
        bar_window = None;
    }
    bar_free_fonts(core);

    if (core->root_cursor != None) {
        XUndefineCursor(core->display, core->root);
        XFreeCursor(core->display, core->root_cursor);
        core->root_cursor = None;
    }
}

const WmModule wm_desktop_module = {
    .name = "desktop",
    .init = desktop_init,
    .event = desktop_event,
    .tick = desktop_tick,
    .interval_ms = WM_TICK_MS,
    .cleanup = desktop_cleanup,
};
