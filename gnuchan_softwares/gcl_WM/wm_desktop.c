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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/cursorfont.h>

#include "wm_core.h"
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
    XFontStruct *font;
} BarFont;

static BarFont bar_fonts[WM_CONFIG_MAX_WIDGETS];
static int bar_font_count = 0;

static XFontStruct *bar_font_for(WmCore *core, const WmWidget *widget) {
    /* A widget that named no font uses the desktop's, which is the one font
       everything else on this desktop is drawn in. */
    if (!widget->font_family[0] || widget->font_size <= 0) {
        return core->style.font;
    }

    char spec[WM_CONFIG_TEXT_LENGTH + 32];
    snprintf(spec, sizeof(spec), "-*-%s-*-*-*-*-%d-*-*-*-*-*-*-*",
             widget->font_family, widget->font_size);

    for (int i = 0; i < bar_font_count; i++) {
        if (strcmp(bar_fonts[i].spec, spec) == 0) {
            return bar_fonts[i].font;
        }
    }

    /* The size-specific spec first, then the family alone, then the desktop's
       font: a machine that has only one of the three still draws a bar. */
    XFontStruct *font = XLoadQueryFont(core->display, spec);
    if (!font) {
        font = XLoadQueryFont(core->display, widget->font_family);
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
            XFreeFont(core->display, bar_fonts[i].font);
        }
    }
    bar_font_count = 0;
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

/* The pointer the desktop draws. It is drawn on the root whatever the config
   says about a cursor theme: the root is the one surface the pointer sits on
   with nothing under it, and it is where the login screen was a moment ago, so
   the two have to agree. A theme cursor here would be the theme's arrow with
   the theme's colours, which is what the login screen deliberately does not
   use. Programs are unaffected — they take their cursor from XCURSOR_THEME,
   which wm_config_apply() still sets.

   XCreatePixmapCursor needs the two colours allocated; a display that refuses
   them (one with no colour at all) falls back to the font cursor, because a
   pointer nobody can see is worse than an ugly one. */
static Cursor desktop_make_cursor(WmCore *core) {
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

/* --- what a widget says --------------------------------------------------- */

/* Room for one workspace number as text. It is wide enough for any int the
   format could be given rather than only for the twelve workspaces a session
   may ask for, so the compiler has no reason to warn about a truncation the
   code cannot actually reach. */
#define WM_LAYOUT_CELL_LENGTH 16

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
        snprintf(cells[written], WM_LAYOUT_CELL_LENGTH, "%d", number);
        numbers[written] = number;
        written++;
    }
    return written;
}

/* What a widget shows, in a buffer. This is the one place a widget's kind
   becomes words, so the layout below never has to know which kind it is looking
   at — it measures what this produced and draws it. */
static void bar_widget_label(const WmWidget *widget, char *out,
                             unsigned int size) {
    out[0] = '\0';
    switch (widget->kind) {
    case WM_WIDGET_CURRENT_LAYOUT:
        /* Drawn cell by cell rather than from this buffer, so that the current
           workspace can be drawn unlike the rest. See bar_layout_cells(). */
        break;
    case WM_WIDGET_GROUP_BOX:
        snprintf(out, size, "%s", widget->symbol);
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

    unsigned long strip_colour = wm_style_colour(core->display, core->screen,
                                                 bar->background,
                                                 core->style.panel);
    XSetForeground(core->display, core->gc, strip_colour);
    XFillRectangle(core->display, bar_window, core->gc,
                   0, 0, (unsigned int)core->width, (unsigned int)height);

    if (bar->widget_count == 0) {
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
    XFontStruct *fonts[WM_CONFIG_MAX_WIDGETS];
    int widths[WM_CONFIG_MAX_WIDGETS];
    int flexible_count = 0;
    int needed = 0;

    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        layout_cell_count[i] = 0;
        bar_widget_label(widget, labels[i], sizeof(labels[i]));
        fonts[i] = bar_font_for(core, widget);
        if (widget->kind == WM_WIDGET_EMPTY_SPACE) {
            widths[i] = 0;
            flexible_count++;
        } else if (widget->kind == WM_WIDGET_CURRENT_LAYOUT) {
            layout_cell_count[i] = bar_layout_cells(
                core, widget, layout_cells[i], layout_numbers[i],
                WM_WORKSPACE_MAX);
            /* The widest cell, so every number gets the same room and the row
               does not shift when the current one changes from "9" to "10". */
            int cell = 0;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int width = wm_style_text_width(fonts[i], layout_cells[i][c]);
                if (width > cell) {
                    cell = width;
                }
            }
            widths[i] = cell == 0 ? 0
                                  : layout_cell_count[i] * (cell + WM_BAR_PADDING);
            needed += widths[i];
        } else {
            widths[i] = wm_style_text_width(fonts[i], labels[i]) +
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
        int width = (widget->kind == WM_WIDGET_EMPTY_SPACE) ? share : widths[i];
        if (width < 0) {
            width = 0;
        }

        unsigned long background = wm_style_colour(core->display, core->screen,
                                                   widget->background,
                                                   strip_colour);
        XSetForeground(core->display, core->gc, background);
        XFillRectangle(core->display, bar_window, core->gc,
                       x, 0, (unsigned int)width, (unsigned int)height);

        if (widget->kind == WM_WIDGET_CURRENT_LAYOUT && fonts[i]) {
            /* One number per cell, and the current workspace drawn in the
               accent so the bar says where the user is. Everything else on the
               bar is drawn in the widget's own foreground; the current cell is
               the one exception, which is the whole reason layout is drawn
               here rather than as one label with the others. */
            unsigned long foreground =
                wm_style_colour(core->display, core->screen,
                                widget->foreground, core->style.text);
            unsigned long active =
                wm_style_colour(core->display, core->screen,
                                core->config.active_border,
                                core->style.accent);
            int cell = 0;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int room = wm_style_text_width(fonts[i], layout_cells[i][c]);
                if (room > cell) {
                    cell = room;
                }
            }
            int baseline =
                (height + fonts[i]->ascent - fonts[i]->descent) / 2;
            int cursor = x;
            for (int c = 0; c < layout_cell_count[i]; c++) {
                int current = layout_numbers[i][c] == core->current_workspace;
                wm_style_text(core->display, bar_window, core->gc, fonts[i],
                              cursor + WM_BAR_PADDING / 2, baseline,
                              layout_cells[i][c],
                              current ? active : foreground);
                cursor += cell + WM_BAR_PADDING;
            }
        } else if (labels[i][0] && fonts[i]) {
            unsigned long foreground =
                wm_style_colour(core->display, core->screen,
                                widget->foreground, core->style.text);
            int baseline = (height + fonts[i]->ascent - fonts[i]->descent) / 2;
            wm_style_text(core->display, bar_window, core->gc, fonts[i],
                          x + WM_BAR_PADDING, baseline, labels[i], foreground);
        }
        x += width;
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

static void desktop_event(WmCore *core, XEvent *event) {
    if (bar_window != None && event->xany.window == bar_window &&
        event->type == Expose) {
        desktop_bar(core);
        return;
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
           the bar it has been covered. */
        if (event->xmap.window != bar_window) {
            wm_desktop_raise_bar(core);
        }
        break;
    default:
        break;
    }
}

/* The idle work of the desktop: keep the clock moving and re-read the config
   script when it is saved. This is what makes the script live — there is no key
   to press and no session to restart. */
static void desktop_tick(WmCore *core) {
    static long long last_stamp = -1;

    if (wm_config_reload(core)) {
        /* A reload can move the bar or resize it, so everything the config
           feeds is redone rather than only the paint. The pointer is not one
           of those things: it is built once at start and the run from the
           config only says which theme the programs should ask for. */
        wm_desktop_repaint(core);
    }

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
