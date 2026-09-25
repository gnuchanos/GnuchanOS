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
#include <X11/Xatom.h>
#include "wm_module.h"
#include "wm_style.h"
#include "wm_frame.h"
#include "wm_config.h"

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
    Atom wm_state;
    Atom wm_protocols;
    Atom wm_delete_window;
    Atom utf8_string;

    int running;

    /* The window that currently has input focus, 0 when none. */
    Window focused;

    /* Every window that has had the focus, most recent first, with no window
       appearing twice. The switcher key walks this rather than the frame
       table: the table is creation order, and creation order has nothing to
       do with what the user was doing. Alt+Tab has to go back to the window
       the user was last in — that is the only order in which one press of the
       key undoes one move. */
    Window focus_history[WM_MAX_FRAMES];
    int focus_history_count;

    /* The picture the whole desktop shares: the palette, the font and the
       one graphics context everything draws with. Modules never create their
       own — a second palette is how one desktop starts looking like two. */
    WmStyle style;
    GC gc;
    WmConfig config;                        /* the parsed settings file */

    /* The pointer set on the root. Kept so the module that made it can free
       it; a cursor allocated and forgotten is a leak per session. */
    Cursor root_cursor;

    /* Every managed window, one frame each. The table lives on the core rather
       than in a module because three modules need it: manage fills it, focus
       asks it which client a frame holds, and the title bar's clicks are
       answered by it. */
    WmFrame frames[WM_MAX_FRAMES];
    int frame_count;

    /* Unmap events to accept without treating them as the user hiding a
       window. Reparenting a mapped client makes the server unmap it, and that
       one unmap is ours, not the program's. */
    int ignore_unmaps;

    int width;                /* the desktop's size; the wallpaper         */
    int height;               /* restarts from these when the screen is    */
                              /* resized                                   */

    WmModuleList modules;
};

int  wm_core_init(WmCore *core);

/* Run every registered module's init, in registration order. Separate from
   wm_core_init() because a module is registered on the core before the core
   has a display, and a module's init needs that display to exist. */
int  wm_core_start(WmCore *core);

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

/* The most recently used window other than the focused one, or NULL when
   there is none. This is what Alt+Tab goes to: the window the user was in
   before, not the next one in the frame table. */
WmFrame *wm_focus_previous(WmCore *core);

/* Forget a window that no longer exists, so the switcher cannot activate a
   window that has been closed. Called when a window is destroyed. */
void wm_focus_forget(WmCore *core, Window window);

/* wm_manage.c — re-draws a window's border in the focused/unfocused colour. */
void wm_manage_frame(WmCore *core, Window window);

/* --- the module entry points, one per file -------------------------------- */
extern const WmModule wm_manage_module;   /* wm_manage.c  */
extern const WmModule wm_focus_module;    /* wm_focus.c   */
extern const WmModule wm_keys_module;     /* wm_keys.c    */
extern const WmModule wm_desktop_module;  /* wm_desktop.c */
extern const WmModule wm_frame_module;    /* wm_frame.c   */
extern const WmModule wm_autostart_module;/* wm_autostart.c */
extern const WmModule wm_menu_module;     /* wm_menu.c    */
extern const WmModule wm_theme_module;    /* wm_theme.c   */
extern const WmModule wm_config_module;   /* wm_config_file.c */

#endif /* GNUCHANWM_CORE_H */
