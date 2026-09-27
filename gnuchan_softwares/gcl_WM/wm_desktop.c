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

/* The bars' windows: one per bar the config asked for, in the config's own
   order. Module state rather than core state: nothing but this file draws or
   moves a bar. `bar_window` is not one of these — it is the window currently
   being drawn, set for the length of one bar's paint so the drawing and
   measuring helpers below can name "the bar I am working on" without every one
   of them taking a bar index as well as a window. */
static Window bar_windows[WM_CONFIG_MAX_BARS];
static int bar_window_count = 0;
static Window bar_window = None;

/* The wallpaper, when the script named one through `gcl_Window.BackgroundImage`
   and wm_image.c has drawn it. It is made the root window's background below,
   so the server paints it into every part of the root it has to paint; the
   picture this holds is kept alive for as long as that is true, and replaced
   and freed when the script names a different one. */
static WmImage desktop_image;

/* Declared ahead of the public entry points, which are written next to the
   raise they pair with rather than next to the drawing they call. */
static void desktop_configure_bar(WmCore *core);
static void desktop_bar(WmCore *core);
static void desktop_paint_background(WmCore *core);

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
    /* Which bar the box was drawn on. A press is delivered to one bar's own
       window, and each bar has its boxes in its own coordinates — so the
       window is part of the match, or a press on the top bar would be tested
       against the bottom bar's icons at the same x. */
    Window bar;
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
    /* The walk counts up to the widget's own end_layout, so it is only as
       bounded as that number is. The reader clamps the range, and this is the
       second bound that keeps the loop's own promise true on its own terms:
       one cell per workspace there can be, and no more, whatever the range
       says. A start above the end is a range with nothing in it rather than a
       loop that never runs its body. */
    if (widget->start_layout > widget->end_layout) {
        return 0;
    }
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
            /* Written with an explicit field width: a symbol is the config's
               own string and may be far longer than one cell is, and a cell
               that overran its buffer would write over the next. Truncating a
               symbol longer than a cell is the right answer — there is no room
               to draw the rest of it anyway. */
            snprintf(cells[written], WM_LAYOUT_CELL_LENGTH, "%.*s",
                     WM_LAYOUT_CELL_LENGTH - 1, widget->symbol);
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

/* Where a bar goes on the screen, and how big it is.
 *
 * Three things are asked of a bar and they can disagree, so the order is
 * fixed: Position says which edge the bar hugs and makes it as long as that
 * edge, then X and Y — when the script wrote them — place the bar's own
 * corner, and last the empty spaces pull the two ends of the bar inwards.
 * Doing it in that order is what makes a bar written as Position="top", X=0,
 * Y=10, Left_EmptySpace=5 sit ten pixels down from the top with five pixels of
 * screen showing at each end, instead of one setting silently cancelling
 * another.
 *
 * "Along" is the way the bar runs and "across" is its thickness. A horizontal
 * bar is as long as the screen is wide and as thick as Size; a vertical one is
 * the other way round. The two ends are the left and right of a horizontal bar
 * and the top and bottom of a vertical one, which is why there are two pairs
 * of empty spaces — Left/Right for a horizontal bar and Up/Down for a vertical
 * one — and why each pair is applied to the along axis of its own pose. */
/* The bar's rectangle on the screen. The rule itself lives in
 * wm_config_bar_rect() — the SAME function the workarea subtracts — so the
 * space the bar covers and the space a window is kept out of are one
 * computation and cannot drift apart. They used to be two: the workarea
 * assumed "an edge bar starts at the screen edge and is Size thick", which is
 * wrong the moment a bar names an X, a Y or an empty space, and a bar written
 * Position="top", Y=10, Left_EmptySpace=5 then sat over the top of the window
 * it was supposed to be above. */
static void bar_geometry(WmCore *core, const WmBar *bar,
                         int *x, int *y, int *width, int *height) {
    wm_config_bar_rect(bar, core->width, core->height, x, y, width, height);
}

/* Make one bar's window, or move the one that is already there.
 *
 * The window is created once and only moved afterwards — on a screen resize,
 * on a reload, on a change of position — because a bar that is destroyed and
 * made again loses what is drawn on it and is seen to blink. Only a window
 * this manager no longer wants, one past the number of bars the config now
 * asks for, is unmapped; it is left alive so a reload that brings the bar
 * back does not have to make it again. */
static void desktop_configure_one_bar(WmCore *core, int index,
                                      const WmBar *bar) {
    if (index < 0 || index >= WM_CONFIG_MAX_BARS) {
        return;
    }
    if (!bar->present || bar->size <= 0) {
        if (bar_windows[index] != None) {
            XUnmapWindow(core->display, bar_windows[index]);
        }
        return;
    }

    int x, y, width, height;
    bar_geometry(core, bar, &x, &y, &width, &height);

    if (bar_windows[index] == None) {
        XSetWindowAttributes attributes;
        memset(&attributes, 0, sizeof(attributes));
        /* Override-redirect: the server maps this window without asking the
           manager, so a bar can never be framed, focused or managed. */
        attributes.override_redirect = True;
        attributes.event_mask = ExposureMask | ButtonPressMask;
        attributes.background_pixel = BlackPixel(core->display, core->screen);
        attributes.border_pixel = 0;

        bar_windows[index] = XCreateWindow(core->display, core->root,
                                           x, y,
                                           (unsigned int)width,
                                           (unsigned int)height,
                                           0, CopyFromParent, InputOutput,
                                           CopyFromParent,
                                           CWOverrideRedirect | CWEventMask |
                                           CWBackPixel | CWBorderPixel,
                                           &attributes);
        if (bar_windows[index] == None) {
            fprintf(stderr, "gnuchanwm: cannot make bar %d's window\n", index);
            return;
        }
    } else {
        XMoveResizeWindow(core->display, bar_windows[index], x, y,
                          (unsigned int)width, (unsigned int)height);
    }
    /* Mapped, then put at the bottom of the stack: the bar belongs under the
       windows, so a window dragged onto it covers it rather than being trapped
       behind it. XMapWindow rather than XMapRaised because raising it here
       would be undone a line later anyway. */
    XMapWindow(core->display, bar_windows[index]);
    XLowerWindow(core->display, bar_windows[index]);
    XFlush(core->display);
}

/* Put every bar where the config says it goes. Called at start, on a screen
   resize and after a reload, so the bars follow the config rather than being
   fixed at whatever the session started with. */
static void desktop_configure_bar(WmCore *core) {
    int count = core->config.bar_count;
    if (count > WM_CONFIG_MAX_BARS) {
        count = WM_CONFIG_MAX_BARS;
    }
    if (count < 0) {
        count = 0;
    }

    for (int i = 0; i < count; i++) {
        desktop_configure_one_bar(core, i, &core->config.bars[i]);
    }
    /* The windows of bars the config no longer asks for are put away, not
       destroyed: the reload that dropped a bar may be followed by one that
       brings it back at the same index, and a bar that was never destroyed is
       one it can simply be shown at again. */
    for (int i = count; i < bar_window_count && i < WM_CONFIG_MAX_BARS; i++) {
        if (bar_windows[i] != None) {
            XUnmapWindow(core->display, bar_windows[i]);
        }
    }
    bar_window_count = count;
    XFlush(core->display);
}

/* Put the bar below every window.
 *
 * The bar sits at the bottom of the stack, so a window moved onto it covers it
 * and is never trapped behind it. This replaces the old bar-on-top behaviour,
 * which raised the bar again the moment a window was raised — so a window
 * dragged up to the bar disappeared underneath it and could not be brought
 * back out. The bar is still seen in its own strip, because windows are placed
 * in the workarea and do not open over it.
 *
 * Called when the bar is (re)configured so a freshly mapped bar starts at the
 * bottom, and after a repaint. It is a restack and not a repaint, so a window
 * on top of the bar is not disturbed. */
void wm_desktop_lower_bar(WmCore *core) {
    for (int i = 0; i < bar_window_count && i < WM_CONFIG_MAX_BARS; i++) {
        if (bar_windows[i] != None) {
            XLowerWindow(core->display, bar_windows[i]);
        }
    }
    XFlush(core->display);
}

/* Whether a window is one of the desktop's own bars. The menu asks this before
   it decides whether a window on screen is an open program or the desktop's
   furniture: a bar is the latter, and must never be listed or closed as if it
   were something the user opened. */
int wm_desktop_is_bar_window(Window window) {
    if (window == None) {
        return 0;
    }
    for (int i = 0; i < bar_window_count && i < WM_CONFIG_MAX_BARS; i++) {
        if (bar_windows[i] == window) {
            return 1;
        }
    }
    return 0;
}

/* Draw the bar again. See the header for why this is not simply bar(). */
void wm_desktop_repaint(WmCore *core) {
    /* The backdrop is put back as well as the bar: a reload can change the
       desktop colour, and it is applied here rather than on the next login. */
    desktop_paint_background(core);
    desktop_configure_bar(core);
    desktop_bar(core);
    wm_desktop_lower_bar(core);
}

/* One bar: the strip, then its widgets, laid out the way its pose runs.
 *
 * This is the horizontal case and the one the desktop has always drawn: a
 * strip as tall as Size and as wide as the bar, its widgets placed left to
 * right and sharing the width. A widget with text asks for the room that text
 * takes, an expanding space takes a share of what is left, and the clock is
 * pushed to whichever end the space was written at. */
static void bar_draw_horizontal(WmCore *core, const WmBar *bar, Window window,
                                int bar_width, int height) {
    /* Everything is drawn into the buffer and put on the window in one copy at
       the end, when the config asked for it. See bar_target() for why. */
    Drawable canvas = bar->vsync ? bar_target(core, bar_width, height)
                                 : window;

    unsigned long strip_colour = bar_colour(core, bar->background,
                                            core->style.panel);
    XSetForeground(core->display, core->gc, strip_colour);
    XFillRectangle(core->display, canvas, core->gc,
                   0, 0, (unsigned int)bar_width, (unsigned int)height);

    if (bar->widget_count == 0) {
        if (canvas != window) {
            XCopyArea(core->display, canvas, window, core->gc, 0, 0,
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

    int leftover = bar_width - needed;
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
               can be turned back into that window. A window that published no
               icon — which is most bare X programs, xterm among them — is
               drawn as the first letter of its title instead, in the widget's
               own colour: a row of gaps would say nothing at all, and a letter
               at least names the window. */
            int icon_y = (height - WM_ICON_SIZE) / 2;
            int gap = bar_icon_gap(core, widget, fonts[i]);
            int separator = bar_separator_width(core, widget, fonts[i]);
            unsigned long icon_colour = bar_colour(core, widget->foreground,
                                                   core->style.text);
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
                if (!wm_frame_draw_icon(core, frame, canvas, cursor, icon_y,
                                        WM_ICON_SIZE) && fonts[i]) {
                    char letter[2] = { '?', '\0' };
                    unsigned char first = (unsigned char)frame->name[0];
                    /* A printable first character only: a title beginning with
                       a control byte would draw as nothing, so it falls back to
                       the question mark above. */
                    if (frame->has_name && first >= 0x20 && first != 0x7f) {
                        letter[0] = (char)first;
                    }
                    int letter_width = wm_style_text_width(core->display,
                                                           fonts[i], letter);
                    wm_style_text(core->display, core->screen, canvas,
                                  fonts[i],
                                  cursor + (WM_ICON_SIZE - letter_width) / 2,
                                  sep_baseline, letter, icon_colour);
                }
                if (bar_icon_count < WM_MAX_FRAMES) {
                    bar_icon_boxes[bar_icon_count].x = cursor;
                    bar_icon_boxes[bar_icon_count].y = icon_y;
                    bar_icon_boxes[bar_icon_count].width = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].height = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].client = frame->client;
                    bar_icon_boxes[bar_icon_count].bar = window;
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
    if (canvas != window) {
        XCopyArea(core->display, canvas, window, core->gc, 0, 0,
                  (unsigned int)bar_width, (unsigned int)height, 0, 0);
    }
    XFlush(core->display);
}

/* One bar, written vertically: the strip as wide as Size and as tall as the
 * bar, its widgets stacked top to bottom and sharing the height.
 *
 * It is the same idea as the horizontal case with the two axes exchanged: a
 * widget asks for the along-axis room its content takes — the height of a line
 * of text, the height of the icons, the height of the row of workspace cells —
 * an expanding space takes a share of what is left, and each is placed under
 * the one before. Text is drawn the same way up as everywhere else rather than
 * rotated: a vertical bar is a list of labels down the side of the screen, not
 * a bar turned on its side, and a rotated word is harder to read than a short
 * one.
 *
 * The empty spaces follow the same rule the geometry gives them: on a vertical
 * bar they are Up_EmptySpace at the top and Down_EmptySpace at the bottom,
 * which is why the along axis here is the height. */
static void bar_draw_vertical(WmCore *core, const WmBar *bar, Window window,
                              int width, int bar_height) {
    Drawable canvas = bar->vsync ? bar_target(core, width, bar_height)
                                 : window;

    unsigned long strip_colour = bar_colour(core, bar->background,
                                            core->style.panel);
    XSetForeground(core->display, core->gc, strip_colour);
    XFillRectangle(core->display, canvas, core->gc,
                   0, 0, (unsigned int)width, (unsigned int)bar_height);

    if (bar->widget_count == 0) {
        if (canvas != window) {
            XCopyArea(core->display, canvas, window, core->gc, 0, 0,
                      (unsigned int)width, (unsigned int)bar_height, 0, 0);
        }
        XFlush(core->display);
        return;
    }

    static char labels[WM_CONFIG_MAX_WIDGETS][WM_CONFIG_TEXT_LENGTH];
    static char layout_cells[WM_CONFIG_MAX_WIDGETS][WM_WORKSPACE_MAX]
                            [WM_LAYOUT_CELL_LENGTH];
    static int layout_numbers[WM_CONFIG_MAX_WIDGETS][WM_WORKSPACE_MAX];
    int layout_cell_count[WM_CONFIG_MAX_WIDGETS];
    XftFont *fonts[WM_CONFIG_MAX_WIDGETS];
    int extents[WM_CONFIG_MAX_WIDGETS];
    int flexible_count = 0;
    int needed = 0;

    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        layout_cell_count[i] = 0;
        bar_widget_label(widget, labels[i], sizeof(labels[i]));
        fonts[i] = bar_font_for(core, widget);
        int line = fonts[i] ? (fonts[i]->ascent + fonts[i]->descent) : 16;

        if (widget->kind == WM_WIDGET_EMPTY_SPACE) {
            if (widget->expanding) {
                extents[i] = 0;
                flexible_count++;
            } else {
                extents[i] = widget->horizontal;
                needed += extents[i];
            }
        } else if (widget->kind == WM_WIDGET_GROUP_BOX) {
            if (bar_icon_total > 0) {
                int gap = bar_icon_gap(core, widget, fonts[i]);
                extents[i] = bar_icon_total * WM_ICON_SIZE +
                             (bar_icon_total - 1) * gap;
            } else {
                extents[i] = 0;
            }
            needed += extents[i];
        } else if (widget->kind == WM_WIDGET_CURRENT_LAYOUT) {
            layout_cell_count[i] = bar_layout_cells(
                core, widget, layout_cells[i], layout_numbers[i],
                WM_WORKSPACE_MAX);
            int gap = bar_layout_gap(widget);
            extents[i] = layout_cell_count[i] * (line + gap);
            needed += extents[i];
        } else {
            extents[i] = line + 2 * WM_BAR_PADDING;
            needed += extents[i];
        }
    }

    int leftover = bar_height - needed;
    if (leftover < 0) {
        leftover = 0;
    }
    int share = flexible_count > 0 ? leftover / flexible_count : 0;

    int y = 0;
    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        int extent = (widget->kind == WM_WIDGET_EMPTY_SPACE && widget->expanding)
                         ? share : extents[i];
        if (extent < 0) {
            extent = 0;
        }

        unsigned long background = bar_colour(core, widget->background,
                                              strip_colour);
        XSetForeground(core->display, core->gc, background);
        XFillRectangle(core->display, canvas, core->gc,
                       0, y, (unsigned int)width, (unsigned int)extent);

        if (widget->kind == WM_WIDGET_CURRENT_LAYOUT && fonts[i]) {
            unsigned long foreground = bar_colour(core, widget->foreground,
                                                  core->style.text);
            unsigned long active =
                bar_colour(core, core->config.active_border, core->style.accent);
            int gap = bar_layout_gap(widget);
            int line = fonts[i]->ascent + fonts[i]->descent;
            int cursor = y;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int room = wm_style_text_width(core->display, fonts[i],
                                               layout_cells[i][c]);
                int current = layout_numbers[i][c] == core->current_workspace;
                wm_style_text(core->display, core->screen, canvas, fonts[i],
                              (width - room) / 2,
                              cursor + gap / 2 + fonts[i]->ascent,
                              layout_cells[i][c],
                              current ? active : foreground);
                cursor += line + gap;
            }
        } else if (widget->kind == WM_WIDGET_GROUP_BOX) {
            int gap = bar_icon_gap(core, widget, fonts[i]);
            int separator = bar_separator_width(core, widget, fonts[i]);
            unsigned long icon_colour = bar_colour(core, widget->foreground,
                                                   core->style.text);
            unsigned long sep_colour =
                bar_colour(core, widget->separator_color,
                           bar_colour(core, widget->foreground,
                                      core->style.text));
            int cursor = y + WM_ICON_GAP / 2;
            for (int k = 0; k < bar_icon_total && k < WM_MAX_FRAMES; k++) {
                WmFrame *frame = bar_icon_frames[k];
                int icon_x = (width - WM_ICON_SIZE) / 2;
                if (!wm_frame_draw_icon(core, frame, canvas, icon_x, cursor,
                                        WM_ICON_SIZE) && fonts[i]) {
                    char letter[2] = { '?', '\0' };
                    unsigned char first = (unsigned char)frame->name[0];
                    if (frame->has_name && first >= 0x20 && first != 0x7f) {
                        letter[0] = (char)first;
                    }
                    int letter_width = wm_style_text_width(core->display,
                                                           fonts[i], letter);
                    int line = fonts[i]->ascent + fonts[i]->descent;
                    wm_style_text(core->display, core->screen, canvas,
                                  fonts[i],
                                  (width - letter_width) / 2,
                                  cursor + (WM_ICON_SIZE - line) / 2 +
                                      fonts[i]->ascent,
                                  letter, icon_colour);
                }
                if (bar_icon_count < WM_MAX_FRAMES) {
                    bar_icon_boxes[bar_icon_count].x = icon_x;
                    bar_icon_boxes[bar_icon_count].y = cursor;
                    bar_icon_boxes[bar_icon_count].width = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].height = WM_ICON_SIZE;
                    bar_icon_boxes[bar_icon_count].client = frame->client;
                    bar_icon_boxes[bar_icon_count].bar = window;
                    bar_icon_count++;
                }
                cursor += WM_ICON_SIZE;
                if (k + 1 < bar_icon_total) {
                    if (separator > 0 && fonts[i]) {
                        int sep_width = wm_style_text_width(core->display,
                                                            fonts[i],
                                                            widget->separator);
                        wm_style_text(core->display, core->screen, canvas,
                                      fonts[i], (width - sep_width) / 2,
                                      cursor + WM_ICON_GAP +
                                          fonts[i]->ascent,
                                      widget->separator, sep_colour);
                    }
                    cursor += gap;
                }
            }
        } else if (labels[i][0] && fonts[i]) {
            unsigned long foreground = bar_colour(core, widget->foreground,
                                                  core->style.text);
            int text_width = wm_style_text_width(core->display, fonts[i],
                                                 labels[i]);
            wm_style_text(core->display, core->screen, canvas, fonts[i],
                          (width - text_width) / 2,
                          y + WM_BAR_PADDING + fonts[i]->ascent,
                          labels[i], foreground);
        }
        y += extent;
    }

    if (canvas != window) {
        XCopyArea(core->display, canvas, window, core->gc, 0, 0,
                  (unsigned int)width, (unsigned int)bar_height, 0, 0);
    }
    XFlush(core->display);
}

/* Every bar the config asks for, one after another.
 *
 * A script may write as many gcl_BAR.call(...) as it likes — a bar along the
 * top, another along the bottom, a small one down the side — and each is drawn
 * here on its own window at its own place, the way bar_geometry() worked out.
 * `bar_window` is set to the window being drawn for the length of one bar, so
 * the helpers above and the click bookkeeping can name "the bar I am working
 * on" without being handed an index as well. */
static void desktop_bar(WmCore *core) {
    int count = core->config.bar_count;
    if (count > WM_CONFIG_MAX_BARS) {
        count = WM_CONFIG_MAX_BARS;
    }
    if (count < 0) {
        count = 0;
    }

    /* Which windows the group box lists, worked out once before any bar is
       drawn so every bar's measuring and drawing passes agree about how many
       icons there are. The boxes a click is matched against are collected from
       all bars at once, and the window each was drawn on is part of the record
       so a press on one bar is never read as a press on another. */
    bar_collect_icons(core);
    bar_icon_count = 0;

    for (int i = 0; i < count; i++) {
        const WmBar *bar = &core->config.bars[i];
        Window window = bar_windows[i];
        if (window == None || !bar->present || bar->size <= 0) {
            continue;
        }
        int x, y, width, height;
        bar_geometry(core, bar, &x, &y, &width, &height);

        bar_window = window;
        if (strcmp(bar->pose, "vertical") == 0) {
            bar_draw_vertical(core, bar, window, width, height);
        } else {
            bar_draw_horizontal(core, bar, window, width, height);
        }
        bar_window = None;
    }
}

/* --- painting ------------------------------------------------------------- */

/* Drop the wallpaper and everything that publishes it.
 *
 * Called when the desktop stops showing a picture — the script stopped naming
 * one, or the one it names cannot be read — so the pixmap this module made is
 * freed and the two properties that name it are removed. Doing only half of
 * that is the bug this function exists to prevent: a freed pixmap still named
 * by _XROOTPMAP_ID is a stale id other programs read, and a pixmap left alive
 * but no longer shown is a full-screen buffer held on the server for the life
 * of the session. The properties are deleted rather than set to None, because
 * a program reading the root background treats "the property is not there" as
 * "there is no picture" and falls back to the colour, which is what the
 * desktop just did. */
static void desktop_forget_image(WmCore *core) {
    if (desktop_image.pixmap != None) {
        Atom root_pixmap = XInternAtom(core->display, "_XROOTPMAP_ID", False);
        Atom esetroot = XInternAtom(core->display, "ESETROOT_PMAP_ID", False);
        if (root_pixmap != None) {
            XDeleteProperty(core->display, core->root, root_pixmap);
        }
        if (esetroot != None) {
            XDeleteProperty(core->display, core->root, esetroot);
        }
    }
    wm_image_free(core, &desktop_image);
}

/* The backdrop: the wallpaper when there is one, and the flat desktop colour
 * when there is not.
 *
 * Whatever comes out is set as the root window's *background* rather than
 * painted onto its pixels, and that is the point. The root window has a
 * background of its own, and the X server paints that background into every
 * part of the root it has to paint, without asking anybody: move a window
 * away, close one, or clear the root, and the server fills the hole itself.
 * Painting pixels directly would leave those holes — the server clears the
 * uncovered part to whatever it was told the background is, and nothing puts a
 * painted picture back, because losing those pixels is not an event this
 * manager is told about. A background set on the window is the server's to
 * repaint, so nothing has to be redrawn at all. feh does exactly this, and
 * additionally publishes the pixmap as _XROOTPMAP_ID so other programs that
 * read the root background — desktop widgets, compositors — find the same
 * picture rather than a black or stale root. */
static void desktop_paint_background(WmCore *core) {
    if (core->width <= 0 || core->height <= 0) {
        return;
    }

    /* The flat colour the desktop is painted in when there is no picture: what
       gcl_Window.background_color named, or the palette's own background
       (wm_style.c) when it named none. Resolved here rather than kept on the
       style because it is the desktop's colour and not a widget's — the bar
       and the frames have their own, and one of those must not follow a change
       to this one. */
    unsigned long flat = core->config.desktop_background_color[0]
                             ? wm_style_colour(core->display, core->screen,
                                               core->config.desktop_background_color,
                                               core->style.background)
                             : core->style.background;

    /* A picture that is not named, is not there, or does not decode leaves the
       flat colour, which is what the desktop had before it could have a
       wallpaper — and what it falls back to when the picture is commented out
       with a '#'. */
    if (!core->config.desktop_background_image[0] ||
        wm_image_load(core, &desktop_image,
                      core->config.desktop_background_image,
                      core->width, core->height) != 0) {
        /* The picture that was there has to go, and this is the moment it
           stops being shown: the root is about to be given a colour instead,
           so the old pixmap is freed and the two properties that still name it
           are removed. See desktop_forget_image(). */
        desktop_forget_image(core);
        XSetWindowBackground(core->display, core->root, flat);
        XClearWindow(core->display, core->root);
        XFlush(core->display);
        return;
    }

    /* The picture is made the root's background pixmap, and published as
       _XROOTPMAP_ID and ESETROOT_PMAP_ID — the two properties feh, hsetroot,
       xsetroot and every wallpaper reader agree on. Setting the property is
       what makes the picture the one a program reading the root finds, so such
       a program draws the same wallpaper rather than a black or stale root. */
    XSetWindowBackgroundPixmap(core->display, core->root, desktop_image.pixmap);
    XClearWindow(core->display, core->root);

    Atom root_pixmap = XInternAtom(core->display, "_XROOTPMAP_ID", False);
    Atom esetroot = XInternAtom(core->display, "ESETROOT_PMAP_ID", False);
    if (root_pixmap != None) {
        XChangeProperty(core->display, core->root, root_pixmap, XA_PIXMAP, 32,
                        PropModeReplace,
                        (unsigned char *)&desktop_image.pixmap, 1);
    }
    if (esetroot != None) {
        XChangeProperty(core->display, core->root, esetroot, XA_PIXMAP, 32,
                        PropModeReplace,
                        (unsigned char *)&desktop_image.pixmap, 1);
    }
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
        /* The box's own bar is part of the match: a press is delivered to one
           bar's window, and two bars can have a box at the same x — the top
           bar's third icon and the bottom bar's third icon. Without the window
           in the test, a press on one would activate the other's window. */
        if (box->bar != press->window) {
            continue;
        }
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
    if (wm_desktop_is_bar_window(event->xany.window)) {
        if (event->type == Expose) {
            /* Every bar is drawn again, rather than working out which bar the
               expose belonged to: the cost is one strip per bar, which is the
               same work the clock's own tick does once a second. */
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
           BELOW it, so the bar is put back at the bottom — a new window is the
           one moment something can have been stacked under the bar. It is also
           the moment a new program's icon has to appear, so the bar is drawn
           again. */
        if (!wm_desktop_is_bar_window(event->xmap.window)) {
            desktop_bar(core);
            wm_desktop_lower_bar(core);
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
    wm_image_free(core, &desktop_image);
    bar_colour_count = 0;
    bar_icon_total = 0;
    bar_icon_count = 0;

    for (int i = 0; i < WM_CONFIG_MAX_BARS; i++) {
        if (bar_windows[i] != None) {
            XDestroyWindow(core->display, bar_windows[i]);
            bar_windows[i] = None;
        }
    }
    bar_window_count = 0;
    bar_window = None;
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
