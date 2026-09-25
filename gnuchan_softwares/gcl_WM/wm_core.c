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

/* Rebuild _NET_CLIENT_LIST on the root from the query tree. A child is a
   client when it carries WM_STATE — the property the ICCCM defines for
   exactly this test. */
static void core_publish_client_list(WmCore *core) {
    Window root_return, parent_return;
    Window *children = NULL;
    unsigned int count = 0;
    if (!XQueryTree(core->display, core->root, &root_return, &parent_return,
                    &children, &count)) {
        return;
    }
    if (children) {
        Window *clients = (Window *)malloc(sizeof(Window) * (count ? count : 1));
        unsigned int client_count = 0;
        if (clients) {
            for (unsigned int i = 0; i < count; i++) {
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char *data = NULL;
                if (XGetWindowProperty(core->display, children[i], core->wm_state,
                                       0, 0, False, AnyPropertyType,
                                       &actual_type, &actual_format, &nitems,
                                       &bytes_after, &data) == Success) {
                    if (data) {
                        XFree(data);
                    }
                    if (actual_type != None) {
                        clients[client_count++] = children[i];
                    }
                }
            }
            XChangeProperty(core->display, core->root, core->net_client_list,
                            XA_WINDOW, 32, PropModeReplace,
                            (unsigned char *)clients, (int)client_count);
            free(clients);
        }
        XFree(children);
    }
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
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
