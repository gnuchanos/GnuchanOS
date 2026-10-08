/*
 * dock_core.h — the X connection, the dock window, and the shared state.
 */
#ifndef GNUCHANDOCK_CORE_H
#define GNUCHANDOCK_CORE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "dock_config.h"
#include "dock_items.h"
#include "dock_menu.h"

typedef struct DockCore {
    Display *display;
    int screen;
    Window root;

    Window window;
    int width;
    int height;
    int x;
    int y;

    Visual *visual;
    int depth;
    Colormap colormap;

    GC gc;
    XftFont *font;

    Atom net_client_list;
    Atom net_wm_name;
    Atom utf8_string;
    Atom net_active_window;
    Atom net_wm_window_type;
    Atom net_wm_window_type_dock;
    Atom net_wm_window_type_desktop;
    /* The EWMH desktop atoms the dock reads to show one workspace at a time.
       _NET_CURRENT_DESKTOP says which workspace is on screen; _NET_WM_DESKTOP,
       on each client, says which one that window is on. A dock that did not
       read these would show every window on every workspace at once, and the
       row would fill with windows the user cannot see. */
    Atom net_current_desktop;
    Atom net_wm_desktop;
    /* The fullscreen state of the active window. _NET_WM_STATE is the list a
       client's states live in and _NET_WM_STATE_FULLSCREEN is the member that
       means "this window owns the whole screen". The dock reads both to tell a
       MAXIMISED window — which it may come forward over — from a FULLSCREEN
       one, which it must not. */
    Atom net_wm_state;
    Atom net_wm_state_fullscreen;
    /* The workspace currently on screen, and the window with the focus, read
       from the root at the start of a build. Zero when the manager publishes
       neither, in which case every window is shown — the safe answer for a
       manager that does not speak EWMH. */
    long current_desktop;
    Window active_window;
    /* 1 when the manager published a current desktop for this build, so a
       window whose own desktop cannot be read is shown rather than hidden. */
    int have_desktop;
    /* 1 when the active window is fullscreen, read at the start of every build
       beside the two above. A fullscreen window owns the whole screen, so the
       dock stays behind it and is not brought forward — a dock over a game or
       a film is in the way. A MAXIMISED window is not fullscreen and is not
       this, which is exactly the line: the dock may come forward over one and
       must not over the other. */
    int fullscreen_active;

    DockConfig config;
    unsigned long background;
    unsigned long edge;
    unsigned long field;
    unsigned long text;
    unsigned long accent;
    unsigned long badge;

    DockItem items[DOCK_MAX_ITEMS];
    int item_count;

    DockIcon settings_icon;
    DockIcon terminal_icon;

    /* The list a category opens. It lives with the core because the dock's own
       loop is what hands it events: it is a second window of the dock and not
       a program of its own. */
    DockMenu menu;

    int hover_index;
    int running;

    /* Auto-hide. The dock is put away below the screen and brought up when the
       pointer reaches the bottom edge, where a thin trigger strip waits for it.
       `revealed` is whether the dock is up; `hide_pending`/`hide_at_ms` are the
       delay that lets a hand cross the gap between the trigger and the dock
       without the dock dropping again on the way. */
    Window trigger;
    int revealed;
    int hide_pending;
    unsigned long hide_at_ms;

    /* Set when something the dock DRAWS has changed — a window opened or
       closed, a title changed — and cleared once the row has been rebuilt and
       painted. It is what turns the dock from a program that rebuilt and
       repainted its whole row five times a second for ever, whether or not
       anything had happened, into one that draws only when there is something
       new to draw. On a two-core laptop with no GPU acceleration that
       difference is a core left free — and the redraw storm was also what
       pushed the X server's 2D acceleration into a GPU hang. */
    int dirty;
} DockCore;

int dock_core_init(DockCore *core);
void dock_core_run(DockCore *core);
void dock_core_shutdown(DockCore *core);
void dock_core_restyle(DockCore *core);
void dock_core_relayout(DockCore *core);
void dock_core_refresh(DockCore *core);
void dock_draw(DockCore *core);
void dock_core_run_command(const char *command);
void dock_core_activate(DockCore *core, Window client);

#endif /* GNUCHANDOCK_CORE_H */
