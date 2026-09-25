/*
 * wm_desktop.c — the desktop itself: the root window's background and the
 * pointer.
 *
 * This is the whole desktop a fresh session gets. It draws one thing — the
 * background colour — and gives the root a pointer that can actually be seen.
 * That is enough on purpose: a window manager that shows a usable black screen
 * and opens a terminal on a key is a working desktop, and everything else
 * (a panel, a menu, a wallpaper, icons) is a module added beside this one
 * rather than a change to it.
 *
 * Why the background at all: the X server paints the root window from
 * whatever a previous session left, or not at all. Setting it here is what
 * makes "the window manager started" visible as the themed colour instead of
 * a black screen that looks like nothing ran.
 */
#include <string.h>
#include <unistd.h>

#include <X11/cursorfont.h>

#include "wm_core.h"

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

/* Paint the background. Called at start and on every Expose of the root, so a
   session that logs in over an old one does not inherit its picture. */
static void desktop_paint(WmCore *core) {
    XSetForeground(core->display, core->gc, core->style.background);
    XFillRectangle(core->display, core->root, core->gc,
                   0, 0,
                   (unsigned int)core->width, (unsigned int)core->height);
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
    .cleanup = desktop_cleanup,
};
