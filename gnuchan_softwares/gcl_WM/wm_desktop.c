/*
 * wm_desktop.c — the desktop itself: the background, the bar, and the pointer.
 *
 * This is the whole desktop a fresh session gets. It paints the backdrop,
 * draws the bar along the edge the config asked for, and gives the root a
 * pointer that can actually be seen.
 *
 * The bar is drawn here rather than in a module of its own because it is made
 * of the same thing the background is: paint on the root window. A dock window
 * would have to be raised, kept above clients, and kept out of the client
 * list; the root is already behind everything and is already the surface this
 * module paints. One painter, one surface, no stacking to manage.
 *
 * The widgets on the bar share it by weight: a widget with text asks for the
 * room that text takes, and the flexible ones split whatever is left. That is
 * what puts a clock at the right edge whether the label beside it is two
 * words or twenty.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/cursorfont.h>

#include "wm_core.h"

/* The room a widget's text gets on either side of it. */
#define WM_BAR_PADDING 8

/* How often the tick runs, in milliseconds. It is fast enough that the clock
   keeps time to the second and slow enough that an idle session is not
   repainting anything worth measuring. */
#define WM_TICK_MS 500

/* A small cursor so the pointer is visible even on a machine whose pointer
   theme was never chosen. The colours are the desktop's own accent on its own
   panel colour. */
static Cursor desktop_cursor(WmCore *core) {
    Cursor cursor = XCreateFontCursor(core->display, XC_left_ptr);
    if (cursor == None) {
        return None;
    }
    XColor foreground;
    XColor background;
    Colormap cmap = DefaultColormap(core->display, core->screen);
    if (!XAllocNamedColor(core->display, cmap, "#c77dff", &foreground, &foreground) ||
        !XAllocNamedColor(core->display, cmap, "#32143f", &background, &background)) {
        return cursor;
    }
    XRecolorCursor(core->display, cursor, &foreground, &background);
    return cursor;
}

/* What a widget shows, in a buffer. This is the one place a widget's kind
   becomes words, so the layout below never has to know which kind it is
   looking at — it measures what this produced and draws it. */
static void bar_widget_label(const WmWidget *widget, char *out,
                             unsigned int size) {
    out[0] = '\0';
    switch (widget->kind) {
    case WM_WIDGET_CURRENT_LAYOUT: {
        /* The layouts as "0 1 2 3 4 5". The desktop has no workspace model
           yet, so the range is drawn as the labels the user wrote. */
        unsigned int used = 0;
        for (int number = widget->start_layout;
             number <= widget->end_layout; number++) {
            int written = snprintf(out + used, size - used, "%s%d",
                                   used ? " " : "", number);
            if (written < 0 || (unsigned int)written >= size - used) {
                break;
            }
            used += (unsigned int)written;
        }
        break;
    }
    case WM_WIDGET_GROUP_BOX:
        snprintf(out, size, "%s", widget->symbol);
        break;
    case WM_WIDGET_TEXT_BOX:
        snprintf(out, size, "%s", widget->text);
        break;
    case WM_WIDGET_CLOCK: {
        /* A clock with no format still tells the time: an empty format is a
           widget that was asked for and not configured, not one that should
           draw nothing. */
        const char *format = widget->format[0] ? widget->format : "%H:%M";
        time_t now = time(NULL);
        struct tm *local = localtime(&now);
        if (local) {
            strftime(out, size, format, local);
        }
        break;
    }
    case WM_WIDGET_EMPTY_SPACE:
        /* Space has no text; its room is the point. */
        break;
    }
}

/* The bar: the strip, then its widgets, left to right. See the note at the
   top of the file for why the width is shared rather than each widget placed. */
static void desktop_bar(WmCore *core) {
    const WmBar *bar = &core->config.bar;
    if (!bar->present || bar->size <= 0) {
        return;
    }
    int height = bar->size;
    int strip_y = strcmp(bar->position, "bottom") == 0
                      ? core->height - height
                      : 0;

    unsigned long strip_colour = wm_style_colour(core->display, core->screen,
                                                 bar->background,
                                                 core->style.panel);
    XSetForeground(core->display, core->gc, strip_colour);
    XFillRectangle(core->display, core->root, core->gc,
                   0, strip_y, (unsigned int)core->width,
                   (unsigned int)height);

    if (bar->widget_count == 0) {
        return;
    }

    /* Measure once, then place. A widget's label is produced here and kept,
       so the measuring and the drawing agree about what is being drawn. */
    static char labels[WM_CONFIG_MAX_WIDGETS][WM_CONFIG_TEXT_LENGTH];
    int widths[WM_CONFIG_MAX_WIDGETS];
    int flexible_count = 0;
    int needed = 0;

    for (int i = 0; i < bar->widget_count && i < WM_CONFIG_MAX_WIDGETS; i++) {
        const WmWidget *widget = &bar->widgets[i];
        bar_widget_label(widget, labels[i], sizeof(labels[i]));
        if (widget->kind == WM_WIDGET_EMPTY_SPACE) {
            widths[i] = 0;
            flexible_count++;
        } else {
            widths[i] = wm_style_text_width(core->style.font, labels[i]) +
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
        XFillRectangle(core->display, core->root, core->gc,
                       x, strip_y, (unsigned int)width, (unsigned int)height);

        if (labels[i][0] && core->style.font) {
            unsigned long foreground =
                wm_style_colour(core->display, core->screen,
                                widget->foreground, core->style.text);
            int baseline = strip_y +
                           (height + core->style.font->ascent -
                            core->style.font->descent) / 2;
            wm_style_text(core->display, core->root, core->gc, core->style.font,
                          x + WM_BAR_PADDING, baseline, labels[i], foreground);
        }
        x += width;
    }
}

/* Paint the background and the bar. Called at start, on every Expose of the
   root, and on the tick that moves the clock. */
static void desktop_paint(WmCore *core) {
    XSetForeground(core->display, core->gc, core->style.background);
    XFillRectangle(core->display, core->root, core->gc,
                   0, 0,
                   (unsigned int)core->width, (unsigned int)core->height);

    desktop_bar(core);

    XFlush(core->display);
}

static int desktop_init(WmCore *core) {
    XSetWindowBackground(core->display, core->root, core->style.background);
    XClearWindow(core->display, core->root);

    Cursor cursor = desktop_cursor(core);
    if (cursor != None) {
        XDefineCursor(core->display, core->root, cursor);
        /* The cursor is part of the desktop, so it is freed with it. */
        core->root_cursor = cursor;
    }

    desktop_paint(core);
    return 0;
}

static void desktop_event(WmCore *core, XEvent *event) {
    /* The root gets exposed when a window above it is unmapped — the greeter
       going away, a full-screen program closing. Repainting then is what
       keeps the desktop from showing whatever was underneath. */
    if (event->type == Expose && event->xexpose.window == core->root) {
        desktop_paint(core);
    } else if (event->type == ConfigureNotify &&
               event->xconfigure.window == core->root) {
        core->width = event->xconfigure.width;
        core->height = event->xconfigure.height;
        desktop_paint(core);
    }
}

/* The idle work of the desktop: keep the clock moving and re-read the config
   script when it is saved. This is what makes the script live — there is no
   key to press and no session to restart. The bar is repainted whenever
   anything it shows could have changed, which is every tick that reloaded. */
static void desktop_tick(WmCore *core) {
    static long long last_stamp = -1;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long stamp = (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;

    int reloaded = wm_config_reload(core);
    if (reloaded) {
        desktop_paint(core);
        last_stamp = stamp;
        return;
    }

    /* The clock only needs a repaint when the second it shows has changed; a
       tick that redraws an unchanged clock is a wake-up per half second that
       buys nothing. */
    if (last_stamp < 0) {
        last_stamp = stamp;
        return;
    }
    if (stamp - last_stamp >= 1000) {
        last_stamp = stamp;
        desktop_paint(core);
    }
}

static void desktop_cleanup(WmCore *core) {
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
