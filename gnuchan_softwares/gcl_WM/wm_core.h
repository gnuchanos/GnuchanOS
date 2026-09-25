/*
 * wm_core.h — the X connection, the event loop, and the shared state modules
 * are given.
 *
 * The core owns exactly one of each X-wide thing: the Display, the root
 * window, the event loop, and the atom table every module looks atoms up in.
 * Modules never open their own display and never run their own event loop.
 */
#ifndef GNUCHANWM_CORE_H
#define GNUCHANWM_CORE_H

#include <X11/Xlib.h>
#include "wm_module.h"

/* The window state the core selects on the root. */
#define WM_EVENT_MASK (SubstructureRedirectMask | SubstructureNotifyMask | \
                       StructureNotifyMask | PropertyChangeMask | \
                       EnterWindowMask | LeaveWindowMask | FocusChangeMask | \
                       ButtonPressMask)

typedef struct WmModuleList {
    const WmModule *items[WM_MAX_MODULES];
    int count;
} WmModuleList;

struct WmCore {
    Display *display;
    int screen;
    Window root;

    Atom net_supported;
    Atom net_supporting_wm_check;
    Atom net_wm_name;
    Atom net_active_window;
    Atom net_client_list;
    Atom net_wm_window_type;
    Atom net_wm_window_type_dock;
    Atom wm_protocols;
    Atom wm_delete_window;
    Atom utf8_string;

    int running;

    /* The window that currently has input focus, 0 when none. */
    Window focused;

    WmModuleList modules;
};

int  wm_core_init(WmCore *core);

/* Read and dispatch exactly one event. Split out of wm_core_run() so the
   caller can stop between events — see GnuChanWM.c. */
void wm_core_step(WmCore *core);
void wm_core_run(WmCore *core);
void wm_core_shutdown(WmCore *core);

Atom wm_atom(WmCore *core, const char *name);

/* Register a module. Call before wm_core_init(). Returns 0 on success. */
int  wm_register(WmCore *core, const WmModule *module);

/* focus is the one piece of state every other module reads and writes. */
void wm_focus_set(WmCore *core, Window window);
void wm_focus_click(WmCore *core, Window window);

/* wm_manage.c — re-draws a window's border in the focused/unfocused colour. */
void wm_manage_frame(WmCore *core, Window window);

/* --- the module entry points, one per file -------------------------------- */
extern const WmModule wm_manage_module;   /* wm_manage.c  */
extern const WmModule wm_focus_module;    /* wm_focus.c   */
extern const WmModule wm_keys_module;     /* wm_keys.c    */

#endif /* GNUCHANWM_CORE_H */
