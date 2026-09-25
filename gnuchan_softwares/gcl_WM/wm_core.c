/*
 * wm_core.c — the X connection, the atom table, and the event loop.
 *
 * This file does no window management of its own. It claims the display,
 * resolves the atoms once, initialises every registered module, then reads
 * events from the X server and hands each one to every module in turn.
 * Everything a user would call "the window manager" lives in the modules.
 *
 * The loop is not a plain XNextEvent, because two things a desktop needs are
 * not events: a clock has to move, and a settings file has to be noticed when
 * it is saved. So the core waits on the X connection with a timeout — the
 * shortest one any module asked for — and calls the modules' tick callbacks
 * whenever it wakes, whether that was an event or the timeout. A session with
 * no clock and no config still waits forever, which costs nothing.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "wm_core.h"

Atom wm_atom(WmCore *core, const char *name) {
    return XInternAtom(core->display, name, False);
}

/* Set while the substructure is being claimed. Xlib reports an X error
   asynchronously, from inside the request that caused it, and there is no
   return value to test — so the only way to learn that another window manager
   already owns the display is to notice the error as it arrives. The flag is
   read once, around the one request that can fail this way. */
static volatile sig_atomic_t claim_refused = 0;

/* Xlib's default reaction to any X error — a BadWindow from a window destroyed
   between two events, a BadAccess from a key another client already owns — is
   to print one line and call exit(). A window manager meets those races
   constantly, so it must not: the error is reported and the WM keeps running.
   Without this, the first lost race kills the session.
 *
 * The one error that is not survivable is BadAccess on the substructure claim:
 * it means a window manager is already running on this display, and two
 * managers fight over every map, configure and unmap until neither works. It
 * is recorded rather than acted on here, because a handler must not end the
 * process; wm_core_init() reads the flag and stops. */
static int core_x_error(Display *display, XErrorEvent *error) {
    char text[256];
    text[0] = '\0';
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr,
            "gnuchanwm: X error: %s (request %d.%d, resource 0x%lx)\n",
            text, error->request_code, error->minor_code,
            (unsigned long)error->resourceid);
    if (error->error_code == BadAccess &&
        error->request_code == X_ChangeWindowAttributes) {
        claim_refused = 1;
    }
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
       configure and unmap of a top-level window through us.
     *
     * Selecting it is also how we tell that another WM is not already running.
     * The server answers with BadAccess and redirects nothing, and because the
     * reply is asynchronous there is no return value to test: the error
     * handler above records it in claim_refused as it arrives, and XSync()
     * is what forces that answer to have come back before the flag is read.
     * Without this test a second WM would start, find no windows to manage,
     * and fight the first one for every event — which is worse than not
     * starting, because it looks like a working session that drops windows. */
    claim_refused = 0;
    XSelectInput(core->display, core->root, WM_EVENT_MASK);
    XSync(core->display, False);

    if (claim_refused) {
        fprintf(stderr,
                "gnuchanwm: another window manager already owns this display; "
                "not starting a second one.\n");
        return -1;
    }

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

/* The shortest interval any module asked for, in milliseconds, or -1 when no
   module wants time. The caller waits that long before giving up on the
   display, which is what lets a clock move on an idle session. */
static int core_tick_interval(WmCore *core) {
    int shortest = -1;
    for (int i = 0; i < core->modules.count; i++) {
        const WmModule *module = core->modules.items[i];
        if (!module->tick || module->interval_ms <= 0) {
            continue;
        }
        int interval = module->interval_ms;
        if (interval > WM_MAX_TICK_MS) {
            interval = WM_MAX_TICK_MS;
        }
        if (shortest < 0 || interval < shortest) {
            shortest = interval;
        }
    }
    return shortest;
}

static void core_call_ticks(WmCore *core) {
    for (int i = 0; i < core->modules.count; i++) {
        const WmModule *module = core->modules.items[i];
        if (module->tick) {
            module->tick(core);
        }
    }
}

static void core_dispatch(WmCore *core, XEvent *event) {
    /* A window that was just mapped, unmapped or destroyed changes the client
       list, so it is republished before the modules see it. */
    if (event->type == MapNotify || event->type == UnmapNotify ||
        event->type == DestroyNotify) {
        core_publish_client_list(core);
    }

    for (int i = 0; i < core->modules.count; i++) {
        const WmModule *module = core->modules.items[i];
        if (module->event) {
            module->event(core, event);
        }
    }
}

/* Read and dispatch exactly one event, after waiting for the modules that
   asked for time.
 *
 * The wait is a select() on the X connection rather than a blocking
 * XNextEvent, and the difference is the whole reason this is not one line: a
 * clock that only moved when a window was clicked would not be a clock. The
 * callbacks run after the wait, not before, so an event that arrives while
 * the display is quiet is dispatched as soon as it does rather than after the
 * next tick.
 *
 * A signal interrupts the wait — that is how the loop is asked to stop — and
 * the interrupted wait is treated as a plain timeout: the flag the handler
 * set is read by the caller, and ticks get one more chance to run cleanly. */
void wm_core_step(WmCore *core) {
    int interval = core_tick_interval(core);

    int fd = ConnectionNumber(core->display);
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(fd, &readable);

    struct timeval timeout;
    struct timeval *wait = NULL;
    if (interval >= 0) {
        timeout.tv_sec = interval / 1000;
        timeout.tv_usec = (interval % 1000) * 1000;
        wait = &timeout;
    }

    while (XPending(core->display) == 0) {
        int ready = select(fd + 1, &readable, NULL, NULL, wait);
        if (ready > 0) {
            break;
        }
        if (ready < 0 && errno == EINTR) {
            break;
        }
        if (ready == 0) {
            /* The display was quiet for as long as the shortest interval
               allowed: this is the tick. */
            core_call_ticks(core);
            if (!core->running) {
                return;
            }
        }
    }

    if (XPending(core->display) == 0) {
        return;
    }

    XEvent event;
    XNextEvent(core->display, &event);
    core_dispatch(core, &event);
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
