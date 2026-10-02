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
 *
 * ============================================================================
 * !! PERFORMANS UYARISI — BU DOSYADA BIR CPU DONGUSU TUZAGI VAR !!
 *
 * _NET_WM_STATE bir property'dir ve iki yonlu bir tuzak tasir. WM onu burada
 * OKUR (manage_property) ve wm_frame_set_fullscreen() uzerinden AYNI client'a
 * geri YAZAR (bkz. wm_frame.c: frame_publish_fullscreen_state). Client'a
 * PropertyChangeMask secili oldugu icin (bkz. wm_frame_create), WM'nin KENDI
 * yazmasi yeni bir PropertyNotify uretir ve manage_property() yeniden cagrilir.
 *
 * manage_property() icindeki "wanted != wm_frame_is_fullscreen(frame)" kontrolu
 * KALDIRILIRSA su sonsuz dongu olusur:
 *
 *      oku -> set_fullscreen -> YAZ -> PropertyNotify -> oku -> YAZ -> ...
 *
 * Bu dongu log'suz doner ve oyun/fullscreen bir pencere acikken GnuChanWM ile
 * Xorg'u %80-100 CPU'ya cikarir; masaustu bostayken %0 gorunur (teshis edilen
 * belirti tam buydu). Kontrolu kaldirma.
 * ============================================================================
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

/* The fullscreen state a client opened with.
 *
 * A client may set _NET_WM_STATE_FULLSCREEN on itself BEFORE it is mapped — a
 * game that knows it wants the whole screen does exactly this — and the
 * property is then already there when the frame is made. Reading it here is
 * what keeps that window from opening with a title bar for one frame and then
 * jumping to fullscreen when the ClientMessage arrives: the two would be
 * indistinguishable on screen from the flicker this whole change exists to
 * remove.
 *
 * The property is read through XA_ATOM, which is what the protocol says the
 * _NET_WM_STATE list is made of. */
static void manage_read_fullscreen_hint(WmCore *core, WmFrame *frame) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    int wanted = 0;

    if (XGetWindowProperty(core->display, frame->client, core->net_wm_state,
                           0, 32, False, XA_ATOM, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_type == XA_ATOM && actual_format == 32) {
            Atom *atoms = (Atom *)data;
            for (unsigned long i = 0; i < items; i++) {
                if (atoms[i] == core->net_wm_state_fullscreen) {
                    wanted = 1;
                    break;
                }
            }
        }
        if (data) {
            XFree(data);
        }
    }

    if (wanted) {
        wm_frame_set_fullscreen(core, frame, 1);
    }
}

static void manage_map(WmCore *core, Window window) {
    if (manage_is_unmanaged_type(core, window)) {
        /* Mapped as the program asked, with no frame: it said it is not a
           window to decorate. */
        XWindowAttributes attributes;
        if (XGetWindowAttributes(core->display, window, &attributes)) {
            fprintf(stderr,
                    "gnuchanwm: manage: window 0x%lx NOT framed "
                    "(override_redirect=%d, %dx%d)\n",
                    (unsigned long)window, attributes.override_redirect,
                    attributes.width, attributes.height);
        }
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

    fprintf(stderr,
            "gnuchanwm: manage: window 0x%lx framed (client %dx%d, "
            "fullscreen-hint read next)\n",
            (unsigned long)window, frame->client_width, frame->client_height);

    /* A game that opened already asking for the whole screen is given it now,
       before the window is on screen, rather than one frame later. */
    manage_read_fullscreen_hint(core, frame);

    wm_focus_set(core, window);
}

/* The EWMH fullscreen request, as a client sends it.
 *
 * The message arrives on the ROOT and names the client in its window field; it
 * is a ClientMessage with message_type _NET_WM_STATE, the action in data.l[0],
 * the property being changed in data.l[1] and a second property in data.l[2]
 * (which this manager does not use — it is for a client that wants two states
 * changed at once, and there is only one state here).
 *
 * The action is a small number and not an atom, which is the one part of EWMH
 * that is easy to get wrong:
 *     0 remove, 1 add, 2 toggle.
 * A toggle is answered against the state the window is in NOW, so two toggles
 * never both turn it on.
 *
 * A message about a window this manager does not hold is ignored rather than
 * answered: it is a client that is not managed, and there is nothing to change
 * about it. */
static void manage_state_message(WmCore *core, XClientMessageEvent *message) {
    if (message->message_type != core->net_wm_state) {
        return;
    }

    WmFrame *frame = wm_frame_find(core, message->window);
    if (!frame) {
        return;
    }

    /* Only a request that NAMES fullscreen is acted on. A client may ask for
       another state in the same message — above, below, sticky — and those are
       not ones this desktop has an answer for, so the request is dropped
       rather than half-honoured. */
    Atom wanted = (Atom)message->data.l[1];
    if (wanted != core->net_wm_state_fullscreen) {
        return;
    }

    long action = message->data.l[0];
    int on;
    if (action == 0) {
        on = 0;                                     /* remove  */
    } else if (action == 1) {
        on = 1;                                     /* add     */
    } else {
        on = !wm_frame_is_fullscreen(frame);        /* toggle  */
    }
    wm_frame_set_fullscreen(core, frame, on);
}

static void manage_configure(WmCore *core, XConfigureRequestEvent *request) {
    WmFrame *frame = wm_frame_find(core, request->window);

    if (frame) {
        /* A FULLSCREEN window's geometry is refused outright, and this is what
           stops a game shaking its own title bar.

           The frame redirects structure (see the SubstructureRedirectMask in
           wm_frame_create), so a client's own XMoveWindow/XResizeWindow arrives
           here as a ConfigureRequest instead of being applied by the server. A
           Wine fullscreen game re-asserts its own geometry every drawn frame —
           it moves itself to the screen corner and sizes itself to the screen,
           believing it is an ordinary window on the root. Applying that would
           put the client over the frame's title bar and border and stretch the
           container to the screen; refusing it leaves the client where the
           manager put it, so the chrome stays visible and nothing flickers.
           The size a fullscreen window has is the resolution the game chose by
           changing the display mode (wm_frame_set_fullscreen_size), and it is
           not changed here. */
        if (wm_frame_is_fullscreen(frame)) {
            return;
        }

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
    WmFrame *frame = wm_frame_find(core, event->window);
    if (!frame) {
        return;
    }

    if (event->atom == core->net_wm_name) {
        wm_frame_update_name(core, frame);
        return;
    }

    /* A client may set _NET_WM_STATE itself instead of sending the message,
       and the protocol allows both. Reading the property again is what keeps a
       client that took that path from being held at the wrong size, and it is
       read rather than assumed because the client may be CLEARING the state as
       well as setting it — a game that closed its own menu and wants its title
       bar back sets the property to the empty list, and there is no message
       coming. */
    if (event->atom == core->net_wm_state) {
        Atom actual_type = None;
        int actual_format = 0;
        unsigned long items = 0;
        unsigned long after = 0;
        unsigned char *data = NULL;
        int wanted = 0;

        if (XGetWindowProperty(core->display, frame->client, core->net_wm_state,
                               0, 32, False, XA_ATOM, &actual_type,
                               &actual_format, &items, &after,
                               &data) == Success) {
            if (data && actual_type == XA_ATOM && actual_format == 32) {
                Atom *atoms = (Atom *)data;
                for (unsigned long i = 0; i < items; i++) {
                    if (atoms[i] == core->net_wm_state_fullscreen) {
                        wanted = 1;
                        break;
                    }
                }
            }
            if (data) {
                XFree(data);
            }
        }

        /* ONLY WHEN IT DIFFERS from the state the frame already holds.
         *
         * This test is the whole of the fix for a busy loop that pegged a core
         * and the X server for as long as any window was fullscreen — the
         * "the game does not flow any more" failure. The manager ANSWERS a
         * fullscreen request by writing this same _NET_WM_STATE property back
         * onto the client (see frame_publish_fullscreen_state), which is what
         * a toolkit actually waits for. Its own write is a PropertyNotify like
         * any other, though, and this window has PropertyChangeMask selected
         * (wm_frame_create), so the property change arrives right back here.
         * The old code then called wm_frame_set_fullscreen() again, which
         * wrote the property again, which notified again — a value written,
         * re-read, rewritten, with nothing in between to break the cycle. It
         * ran at the speed of the X server and never printed a line, which is
         * why it looked like a mystery. When the property already says what
         * the frame already is, there is nothing to change and nothing to
         * write, and the cycle stops after the single answer the client was
         * waiting for. */
        if (wanted != wm_frame_is_fullscreen(frame)) {
            wm_frame_set_fullscreen(core, frame, wanted);
        }
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
    case ClientMessage:
        manage_state_message(core, &event->xclient);
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
