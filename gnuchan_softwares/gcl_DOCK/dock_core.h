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
