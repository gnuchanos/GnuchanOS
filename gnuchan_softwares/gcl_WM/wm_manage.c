/*
 * wm_manage.c — takes over new windows and keeps the tree sane.
 *
 * When a client maps a top-level window, the X server sends us a MapRequest
 * instead of mapping it itself, and this module decides what happens to that
 * window. For now that decision is a border and then a map — but it is the one
 * place every future placement, tiling or decoration feature hangs off.
 */
#include <stdio.h>
#include <stdlib.h>

#include "wm_core.h"

/* The border drawn around a managed window. It is what tells a user which
   windows this WM is actually handling. A real frame with a title bar is a
   decoration module's job, not this one's. */
#define WM_BORDER_WIDTH 2

static unsigned long border_colour(WmCore *core, int focused) {
    Colormap cmap = DefaultColormap(core->display, core->screen);
    XColor colour;
    XColor exact;
    const char *name = focused ? "#c77dff" : "#32143f";
    if (XAllocNamedColor(core->display, cmap, name, &colour, &exact)) {
        return colour.pixel;
    }
    return focused ? BlackPixel(core->display, core->screen)
                   : WhitePixel(core->display, core->screen);
}

/* Whether a window asks not to be managed. A dock or a splash is the app
   telling us it is not a normal client window; treating one as normal is how
   a panel ends up with a border drawn around it. */
static int manage_is_unmanaged_type(WmCore *core, Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    Atom dock = wm_atom(core, "_NET_WM_WINDOW_TYPE_DOCK");
    Atom splash = wm_atom(core, "_NET_WM_WINDOW_TYPE_SPLASH");

    int unmanaged = 0;
    if (XGetWindowProperty(core->display, window, core->net_wm_window_type,
                           0, 32, False, XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &data) == Success) {
        if (data && actual_type == XA_ATOM && actual_format == 32) {
            Atom *atoms = (Atom *)data;
            for (unsigned long i = 0; i < nitems; i++) {
                if (atoms[i] == dock || atoms[i] == splash) {
                    unmanaged = 1;
                    break;
                }
            }
        }
        if (data) {
            XFree(data);
        }
    }
    return unmanaged;
}

/* Whether the window already has our border. A window mapped a second time —
   one that was withdrawn and shown again — must not get a second border. */
static int manage_has_border(WmCore *core, Window window) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, window, &attributes)) {
        return 0;
    }
    return attributes.border_width == WM_BORDER_WIDTH;
}

void wm_manage_frame(WmCore *core, Window window) {
    if (manage_is_unmanaged_type(core, window)) {
        return;
    }
    if (!manage_has_border(core, window)) {
        XSetWindowBorderWidth(core->display, window, WM_BORDER_WIDTH);
    }
    XSetWindowBorder(core->display, window,
                     border_colour(core, window == core->focused));
}

static void manage_map(WmCore *core, Window window) {
    if (manage_is_unmanaged_type(core, window)) {
        XMapWindow(core->display, window);
        return;
    }

    wm_manage_frame(core, window);

    /* Be told when the window's title or type changes, so a window that
       renames itself stays right in a task list. */
    XSelectInput(core->display, window,
                 PropertyChangeMask | StructureNotifyMask | EnterWindowMask |
                 LeaveWindowMask | FocusChangeMask);

    XMapWindow(core->display, window);

    /* Focus the new window: after a terminal or an editor starts, that is what
       a user expects. The focus module owns the policy. */
    wm_focus_set(core, window);
}

static void manage_configure(WmCore *core, XConfigureRequestEvent *request) {
    /* Honour what the client asks for. This is the simple case on purpose: a
       placement or tiling policy belongs to a later module, and this exists so
       a resize is not lost while there is no such module yet. */
    XWindowChanges changes;
    changes.x = request->x;
    changes.y = request->y;
    changes.width = request->width;
    changes.height = request->height;
    changes.border_width = request->border_width;
    changes.sibling = request->above;
    changes.stack_mode = request->detail;

    XConfigureWindow(core->display, request->window,
                     (unsigned int)request->value_mask, &changes);
    XSync(core->display, False);
    (void)core;
}

static int manage_init(WmCore *core) {
    /* Windows that existed before the WM started — ones the server mapped
       itself — are taken over here, because their MapRequest has already
       happened and will not come again. */
    Window root_return, parent_return;
    Window *children = NULL;
    unsigned int count = 0;
    if (XQueryTree(core->display, core->root, &root_return, &parent_return,
                   &children, &count)) {
        for (unsigned int i = 0; i < count; i++) {
            XWindowAttributes attributes;
            if (!XGetWindowAttributes(core->display, children[i], &attributes)) {
                continue;
            }
            if (attributes.override_redirect || attributes.map_state != IsViewable) {
                continue;
            }
            manage_map(core, children[i]);
        }
        if (children) {
            XFree(children);
        }
    }
    return 0;
}

static void manage_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case MapRequest:
        manage_map(core, event->xmaprequest.window);
        break;
    case ConfigureRequest:
        manage_configure(core, &event->xconfigurerequest);
        break;
    case DestroyNotify:
        if (core->focused == event->xdestroywindow.window) {
            core->focused = 0;
        }
        break;
    default:
        break;
    }
}

const WmModule wm_manage_module = {
    .name = "manage",
    .init = manage_init,
    .event = manage_event,
    .cleanup = NULL,
};
