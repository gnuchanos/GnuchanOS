/*
 * dm_core.c — the display, the window, the event loop, and the module list.
 *
 * The greeter draws every frame into an off-screen pixmap and then puts that
 * pixmap on the window in one XCopyArea. Drawing straight to the window would
 * clear it and rebuild it in full view of the user, which is what turns a held
 * and repeating Tab key into a flicker.
 *
 * There is one greeter per machine, so the list of modules is a file-static
 * here rather than a field of the core: a second core would share it, and a
 * second core is not a thing that can exist.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/cursorfont.h>

#include "dm_core.h"

static DmModuleList g_modules;

static int core_x_error(Display *display, XErrorEvent *error) {
    char text[256];
    text[0] = '\0';
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr, "gnuchandm: X error: %s\n", text);
    return 0;
}

/* The pointer over the login screen: a hand in the accent purple on the panel
   colour, so it is visible on a machine whose only pointer is this one and
   which mouse theme has never been chosen. */
static Cursor core_make_cursor(DmCore *core) {
    Cursor cursor = XCreateFontCursor(core->display, XC_hand2);
    if (cursor == None) {
        return None;
    }
    XColor foreground;   /* the accent purple */
    XColor background;   /* the dark panel    */
    Colormap cmap = DefaultColormap(core->display, core->screen);

    if (!XParseColor(core->display, cmap, "#c77dff", &foreground) ||
        !XParseColor(core->display, cmap, "#32143f", &background) ||
        !XAllocColor(core->display, cmap, &foreground) ||
        !XAllocColor(core->display, cmap, &background)) {
        return cursor;   /* the server's own colours are better than none */
    }

    XRecolorCursor(core->display, cursor, &foreground, &background);
    return cursor;
}

static int core_create_buffer(DmCore *core) {
    core->buffer = XCreatePixmap(core->display, core->window,
                                 (unsigned int)core->width,
                                 (unsigned int)core->height,
                                 (unsigned int)DefaultDepth(core->display, core->screen));
    return core->buffer != None ? 0 : -1;
}

int dm_register(DmCore *core, const DmModule *module) {
    (void)core;
    if (!module || !module->name) return -1;
    if (g_modules.count >= DM_MAX_MODULES) return -1;
    g_modules.items[g_modules.count++] = module;
    return 0;
}

int dm_core_init(DmCore *core) {
    memset(core, 0, sizeof(*core));

    core->display = XOpenDisplay(NULL);
    if (!core->display) {
        fprintf(stderr, "gnuchandm: cannot open the X display\n");
        return -1;
    }

    XSetErrorHandler(core_x_error);

    core->screen = DefaultScreen(core->display);
    core->root = RootWindow(core->display, core->screen);
    core->width = DisplayWidth(core->display, core->screen);
    core->height = DisplayHeight(core->display, core->screen);
    core->running = 1;
    core->focus = DM_FOCUS_USERNAME;

    if (dm_style_load(&core->style, core->display, core->screen) != 0) return -1;

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.background;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            StructureNotifyMask | FocusChangeMask;

    core->cursor = core_make_cursor(core);
    if (core->cursor != None) {
        attributes.cursor = core->cursor;
    }

    core->window = XCreateWindow(
        core->display, core->root,
        0, 0, (unsigned int)core->width, (unsigned int)core->height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask | CWCursor, &attributes);
    if (core->window == None) return -1;

    core->gc = XCreateGC(core->display, core->window, 0, NULL);
    if (core->gc == NULL) return -1;

    if (core_create_buffer(core) != 0) return -1;

    XSetInputFocus(core->display, core->window, RevertToPointerRoot, CurrentTime);
    XMapRaised(core->display, core->window);
    XSync(core->display, False);
    return 0;
}

int dm_core_start(DmCore *core) {
    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->init && module->init(core) != 0) {
            fprintf(stderr, "gnuchandm: module '%s' failed\n", module->name);
            return -1;
        }
    }
    dm_core_redraw(core);
    return 0;
}

void dm_core_redraw(DmCore *core) {
    if (!core->display || core->window == None || core->buffer == None) return;
    /* While a session is being started the greeter is handing the screen over
       and must not draw over it. */
    if (core->starting_session) return;

    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->draw) module->draw(core);
    }

    /* The frame is complete in the pixmap: show it in one operation. */
    XCopyArea(core->display, core->buffer, core->window, core->gc,
              0, 0, (unsigned int)core->width, (unsigned int)core->height, 0, 0);
    XFlush(core->display);
}

void dm_core_step(DmCore *core) {
    XEvent event;
    XNextEvent(core->display, &event);

    if (event.type == ConfigureNotify) {
        core->width = event.xconfigure.width;
        core->height = event.xconfigure.height;
        if (core->buffer != None) XFreePixmap(core->display, core->buffer);
        if (core_create_buffer(core) != 0) return;
    }

    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->event) module->event(core, &event);
    }

    switch (event.type) {
    case Expose:
    case ConfigureNotify:
    case KeyPress:
    case ButtonPress:
    case FocusIn:
        dm_core_redraw(core);
        break;
    default:
        break;
    }
}

void dm_core_shutdown(DmCore *core) {
    for (int i = g_modules.count - 1; i >= 0; i--) {
        const DmModule *module = g_modules.items[i];
        if (module->cleanup) module->cleanup(core);
    }
    g_modules.count = 0;
    dm_form_clear_password(core);

    if (core->buffer != None) {
        XFreePixmap(core->display, core->buffer);
        core->buffer = None;
    }
    if (core->cursor != None) {
        XFreeCursor(core->display, core->cursor);
        core->cursor = None;
    }
    if (core->gc != NULL) {
        XFreeGC(core->display, core->gc);
        core->gc = NULL;
    }
    if (core->window != None) {
        XDestroyWindow(core->display, core->window);
        core->window = None;
    }
    if (core->display) {
        dm_style_free(&core->style, core->display);
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
