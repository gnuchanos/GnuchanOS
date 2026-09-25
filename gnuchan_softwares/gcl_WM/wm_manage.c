/*
 * wm_manage.c — takes over new windows, and keeps the frame table true.
 *
 * The root redirects structure, so a client's MapRequest arrives here instead
 * of the server mapping it. What this module decides is whether the window is
 * one to manage at all (a dock or a splash asks not to be) and, when it is,
 * hands it to the frame module, which wraps it in a title bar.
 *
 * Everything after that is bookkeeping: a client that resized, renamed itself
 * or went away has to be reflected in the frame it lives in, or the bar and
 * the window under it stop agreeing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_core.h"
#include "wm_frame.h"

/* Whether a window asks not to be managed. A dock or a splash is the program
   telling us it is not a normal client window; a panel with a title bar drawn
   around it is what happens when this test is not made. */
static int manage_is_unmanaged_type(WmCore *core, Window window) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, window, &attributes)) {
        return 1;
    }
    if (attributes.override_redirect) {
        return 1;
    }
    /* A window that is not viewable is not one to decorate: it is either
       already gone or has never been shown. */
    if (attributes.class == InputOnly) {
        return 1;
    }

    Atom dock = wm_atom(core, "_NET_WM_WINDOW_TYPE_DOCK");
    Atom splash = wm_atom(core, "_NET_WM_WINDOW_TYPE_SPLASH");

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    int unmanaged = 0;

    if (XGetWindowProperty(core->display, window, core->net_wm_window_type,
                           0, 32, False, XA_ATOM, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_type == XA_ATOM && actual_format == 32) {
            Atom *atoms = (Atom *)data;
            for (unsigned long i = 0; i < items; i++) {
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

static void manage_map(WmCore *core, Window window) {
    if (manage_is_unmanaged_type(core, window)) {
        /* Mapped as the program asked, with no frame: it said it is not a
           window to decorate. */
        XMapWindow(core->display, window);
        return;
    }

    WmFrame *frame = wm_frame_create(core, window);
    if (!frame) {
        /* Already framed, or the table is full. Either way the window is
           mapped so it is not lost. */
        XMapWindow(core->display, window);
        return;
    }

    wm_focus_set(core, window);
}

static void manage_configure(WmCore *core, XConfigureRequestEvent *request) {
    WmFrame *frame = wm_frame_find(core, request->window);

    if (frame) {
        /* A managed window does not get to place itself: its position on the
           screen belongs to the frame, and a client that moved itself would
           slide out from under its own title bar. Only the size is taken. */
        if (request->value_mask & (CWWidth | CWHeight)) {
            int width = (request->value_mask & CWWidth)
                            ? request->width : frame->client_width;
            int height = (request->value_mask & CWHeight)
                             ? request->height : frame->client_height;
            wm_frame_resize(core, frame, width, height);
        }
        return;
    }

    /* Not managed: honour what the client asks for, so a window that is not
       ours to place still lands where it meant to. */
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
}

static void manage_unmap(WmCore *core, Window window) {
    /* A reparented client is unmapped by the server as part of being
       reparented. That unmap is ours, not the program hiding itself, and
       dropping the frame for it would destroy the title bar of a window that
       is still running. */
    if (core->ignore_unmaps > 0) {
        core->ignore_unmaps--;
        return;
    }

    /* The same unmap is reported twice — once through the root's substructure
       and once through the window's own structure — because both are selected.
       Only the first is the real one, and after reparenting the client is
       mapped again inside its frame, so a window that is still viewable here
       is the duplicate and is left alone. Without this the title bar would be
       destroyed the moment it was created. */
    XWindowAttributes attributes;
    if (XGetWindowAttributes(core->display, window, &attributes) &&
        attributes.map_state == IsViewable) {
        return;
    }

    WmFrame *frame = wm_frame_find(core, window);
    if (frame && !frame->dragging) {
        wm_frame_destroy(core, frame, 0);
    }
}

static void manage_destroy(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find(core, window);
    if (frame) {
        wm_frame_destroy(core, frame, 1);
    }
}

static void manage_property(WmCore *core, XPropertyEvent *event) {
    if (event->atom != core->net_wm_name) {
        return;
    }
    WmFrame *frame = wm_frame_find(core, event->window);
    if (frame) {
        wm_frame_update_name(core, frame);
    }
}

static int manage_init(WmCore *core) {
    /* Windows that existed before this manager started — ones the server
       mapped itself — are taken over here, because their MapRequest has
       already happened and will not come again. */
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
            if (attributes.override_redirect ||
                attributes.map_state != IsViewable) {
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
    case UnmapNotify:
        /* Synthetic events are ones we sent ourselves, and they are already
           accounted for. */
        if (!event->xunmap.send_event) {
            manage_unmap(core, event->xunmap.window);
        }
        break;
    case DestroyNotify:
        manage_destroy(core, event->xdestroywindow.window);
        break;
    case PropertyNotify:
        manage_property(core, &event->xproperty);
        break;
    case ConfigureNotify:
        /* A real notification about a managed client means it moved or resized
           itself. A synthetic one is the frame module telling the client what
           it got, and reacting to that would be a loop. */
        if (!event->xconfigure.send_event &&
            event->xconfigure.event == event->xconfigure.window) {
            WmFrame *frame = wm_frame_find(core, event->xconfigure.window);
            if (frame) {
                wm_frame_sync(core, frame);
            }
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

/* Kept for the focus module, which only wants a window redrawn once the focus
   it just set has moved. The drawing is the frame's. */
void wm_manage_frame(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find(core, window);
    if (frame) {
        wm_frame_draw(core, frame);
    }
}
