/*
 * wm_core.c — the X connection, the atom table, and the event loop.
 *
 * This file does no window management of its own. It claims the display,
 * resolves the atoms once, initialises every registered module, then reads
 * events from the X server and hands each one to every module in turn.
 * Everything a user would call "the window manager" lives in the modules.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_core.h"

Atom wm_atom(WmCore *core, const char *name) {
    return XInternAtom(core->display, name, False);
}

/* Xlib's default reaction to any X error — a BadWindow from a window destroyed
   between two events, a BadAccess from a key another client already owns — is
   to print one line and call exit(). A window manager meets those races
   constantly, so it must not: the error is reported and the WM keeps running.
   Without this, the first lost race kills the session. */
static int core_x_error(Display *display, XErrorEvent *error) {
    char text[256];
    text[0] = '\0';
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr,
            "gnuchanwm: X error: %s (request %d.%d, resource 0x%lx)\n",
            text, error->request_code, error->minor_code,
            (unsigned long)error->resourceid);
    return 0;
}

int wm_register(WmCore *core, const WmModule *module) {
    if (core->modules.count >= WM_MAX_MODULES) {
        fprintf(stderr, "gnuchanwm: too many modules (max %d)\n", WM_MAX_MODULES);
        return -1;
    }
    if (!module || !module->name) {
        return -1;
    }
    core->modules.items[core->modules.count++] = module;
    return 0;
}

/* Rebuild _NET_CLIENT_LIST on the root.
 *
 * The list holds the clients, not the desktop's own frames, and once a client
 * has been reparented it is no longer a child of the root — it lives inside a
 * frame. A walk of the query tree would therefore find nothing, so the frame
 * table is what is asked: it is the only thing that still knows which windows
 * are clients. A task list or a pager is what reads this property, and an
 * empty one is a task list that shows nothing. */
static void core_publish_client_list(WmCore *core) {
    Window clients[WM_MAX_FRAMES];
    int client_count = 0;
    for (int i = 0; i < core->frame_count && i < WM_MAX_FRAMES; i++) {
        clients[client_count++] = core->frames[i].client;
    }
    XChangeProperty(core->display, core->root, core->net_client_list,
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)clients, client_count);
}

static void core_publish_supported(WmCore *core) {
    Atom supported[] = {
        core->net_supported,
        core->net_supporting_wm_check,
        core->net_wm_name,
        core->net_active_window,
        core->net_client_list,
        core->net_wm_window_type,
    };
    XChangeProperty(core->display, core->root, core->net_supported,
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)supported,
                    (int)(sizeof(supported) / sizeof(supported[0])));
}

/* Advertise ourselves through _NET_SUPPORTING_WM_CHECK: a small window that
   names us, with the same property on the root pointing at it. This is how a
   desktop environment tells "a window manager is running" from "there is
   none". */
static Window core_create_check_window(WmCore *core) {
    Window check = XCreateSimpleWindow(core->display, core->root,
                                       0, 0, 1, 1, 0, 0, 0);
    const char *name = "GnuChanWM";
    XChangeProperty(core->display, check, core->net_supporting_wm_check,
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&check, 1);
    XChangeProperty(core->display, check, core->net_wm_name,
                    core->utf8_string, 8, PropModeReplace,
                    (const unsigned char *)name, (int)strlen(name));
    XChangeProperty(core->display, core->root, core->net_supporting_wm_check,
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&check, 1);
    return check;
}

int wm_core_init(WmCore *core) {
    memset(core, 0, sizeof(*core));

    core->display = XOpenDisplay(NULL);
    if (!core->display) {
        fprintf(stderr, "gnuchanwm: cannot open the X display. Is DISPLAY set?\n");
        return -1;
    }

    core->screen = DefaultScreen(core->display);
    core->root = RootWindow(core->display, core->screen);
    core->running = 1;
    core->width = DisplayWidth(core->display, core->screen);
    core->height = DisplayHeight(core->display, core->screen);

    /* The palette and the font are the desktop's whole appearance, and the
       one graphics context is what every module draws with. They are made
       here, once, before any module can ask for them. */
    if (wm_style_load(&core->style, core->display, core->screen) != 0) {
        fprintf(stderr, "gnuchanwm: cannot resolve the desktop style\n");
        return -1;
    }
    core->gc = XCreateGC(core->display, core->root, 0, NULL);
    if (core->gc == NULL) {
        fprintf(stderr, "gnuchanwm: cannot create a graphics context\n");
        return -1;
    }

    /* Installed before any request that can fail: from here on an X error is
       a line in the log and nothing more. */
    XSetErrorHandler(core_x_error);

    core->net_supported = wm_atom(core, "_NET_SUPPORTED");
    core->net_supporting_wm_check = wm_atom(core, "_NET_SUPPORTING_WM_CHECK");
    core->net_wm_name = wm_atom(core, "_NET_WM_NAME");
    core->net_active_window = wm_atom(core, "_NET_ACTIVE_WINDOW");
    core->net_client_list = wm_atom(core, "_NET_CLIENT_LIST");
    core->net_wm_window_type = wm_atom(core, "_NET_WM_WINDOW_TYPE");
    core->net_wm_window_type_dock = wm_atom(core, "_NET_WM_WINDOW_TYPE_DOCK");
    core->wm_state = wm_atom(core, "WM_STATE");
    core->wm_protocols = wm_atom(core, "WM_PROTOCOLS");
    core->wm_delete_window = wm_atom(core, "WM_DELETE_WINDOW");
    core->utf8_string = wm_atom(core, "UTF8_STRING");

    /* Claim the substructure: from now on the X server routes every map,
       configure and unmap of a top-level window through us. Selecting it is
       also how we tell that another WM is not already running — the call
       fails with BadAccess, which is fatal here because two WMs on one
       display fight over every window. */
    XSelectInput(core->display, core->root, WM_EVENT_MASK);
    XSync(core->display, False);

    core_create_check_window(core);
    core_publish_supported(core);
    core_publish_client_list(core);
    XSync(core->display, False);

    return 0;
}

/* Run every module's init, in the order they were registered. This is a step
   of its own because the core has to exist before a module can be registered
   on it, and a module's init needs the display that wm_core_init() opened —
   so the two cannot be the same call. */
int wm_core_start(WmCore *core) {
    for (int i = 0; i < core->modules.count; i++) {
        const WmModule *module = core->modules.items[i];
        if (module->init && module->init(core) != 0) {
            fprintf(stderr, "gnuchanwm: module '%s' failed to start\n", module->name);
            return -1;
        }
    }
    fprintf(stderr, "gnuchanwm: running with %d module(s)\n", core->modules.count);
    return 0;
}

/* Read and dispatch exactly one event. The loop is built on this rather than
   containing the blocking call itself, because the caller has to be able to
   stop between events — a signal interrupts XNextEvent, and the flag it sets
   is only visible once this function returns. */
void wm_core_step(WmCore *core) {
    XEvent event;
    XNextEvent(core->display, &event);

    /* A window that was just mapped, unmapped or destroyed changes the client
       list, so it is republished before the modules see it. */
    if (event.type == MapNotify || event.type == UnmapNotify ||
        event.type == DestroyNotify) {
        core_publish_client_list(core);
    }

    for (int i = 0; i < core->modules.count; i++) {
        const WmModule *module = core->modules.items[i];
        if (module->event) {
            module->event(core, &event);
        }
    }
}

void wm_core_run(WmCore *core) {
    while (core->running) {
        wm_core_step(core);
    }
}

void wm_core_shutdown(WmCore *core) {
    for (int i = core->modules.count - 1; i >= 0; i--) {
        const WmModule *module = core->modules.items[i];
        if (module->cleanup) {
            module->cleanup(core);
        }
    }
    if (core->display) {
        if (core->gc != NULL) {
            XFreeGC(core->display, core->gc);
            core->gc = NULL;
        }
        wm_style_free(&core->style, core->display);
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
