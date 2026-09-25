/*
 * dm_core.c — the display, the window, the event loop, and the module list.
 *
 * There is one greeter per machine, so the list of modules is a file-static
 * here rather than a field of the core: a second core would share it, and a
 * second core is not a thing that can exist.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dm_core.h"

static DmModuleList g_modules;

static int core_x_error(Display *display, XErrorEvent *error) {
    char text[256];
    text[0] = '\0';
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr, "gnuchandm: X error: %s\n", text);
    return 0;
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
    core->focused = DM_FIELD_USERNAME;

    if (dm_style_load(&core->style, core->display, core->screen) != 0) return -1;

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.background;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            StructureNotifyMask | FocusChangeMask;

    core->window = XCreateWindow(
        core->display, core->root,
        0, 0, (unsigned int)core->width, (unsigned int)core->height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
    if (core->window == None) return -1;

    core->gc = XCreateGC(core->display, core->window, 0, NULL);
    if (core->gc == NULL) return -1;

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
    if (!core->display || core->window == None) return;
    /* While a session is being started the greeter is handing the screen over
       and must not draw over it. */
    if (core->starting_session) return;

    for (int i = 0; i < g_modules.count; i++) {
        const DmModule *module = g_modules.items[i];
        if (module->draw) module->draw(core);
    }
    XFlush(core->display);
}

void dm_core_step(DmCore *core) {
    XEvent event;
    XNextEvent(core->display, &event);

    if (event.type == ConfigureNotify) {
        core->width = event.xconfigure.width;
        core->height = event.xconfigure.height;
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
