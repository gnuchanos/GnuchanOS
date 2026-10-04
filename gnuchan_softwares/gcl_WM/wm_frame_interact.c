/*
 * wm_frame_interact.c — the working half of a frame: what a click, a drag, a
 * resize and an exposure do to it.
 *
 * A frame that can only be drawn cannot be used. This is the module that turns
 * the bar and the border into things a hand can act on:
 *
 *   a click on the bar          a drag to move the window, or a second click
 *                               to fill the screen
 *   a click on a button         the close, maximise and minimise glyphs
 *   Alt+left on a window        a drag from anywhere in it
 *   Alt+right on a window       a resize, with the edges taken from the place
 *                               the press landed
 *
 * It was the event half of wm_frame.c and it is its own file because it is the
 * only part of the frame that reads X events. Keeping it apart from the
 * table and the state changes means a change to the way a window is dragged
 * cannot change the way one is created, and the file is written the way the
 * screen is: press by press, with each branch saying what a hand meant.
 *
 * Getting this wrong is expensive in a way that is hard to see, and the two
 * faults both have their own note below because both were real: a synchronous
 * grab left unanswered (the mouse freezes over a Wine window) and a motion
 * backlog applied step by step (a resize that trails the hand).
 */
#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_frame.h"
#include "wm_frame_geometry.h"
#include "wm_frame_internal.h"

/* --- the double click ------------------------------------------------------
 *
 * Two presses close together in time and place are a double click, and X has
 * no event for one: it reports every press the same way. Nor is the interval
 * the desktop is configured with readable from Xlib — it lives in XSETTINGS —
 * so it is a constant here, at the value every desktop uses by default. The
 * window must not have moved between the presses either: a press that dragged
 * the window is not the first half of a double click. */
#define WM_DOUBLE_CLICK_MS 400
#define WM_DOUBLE_CLICK_SLOP 6

static Window click_client = None;
static Time click_time = 0;
static int click_x = 0;
static int click_y = 0;
static int click_frame_x = 0;
static int click_frame_y = 0;

static void click_forget(void) {
    click_client = None;
    click_time = 0;
}

static void click_remember(WmFrame *frame, XButtonEvent *press) {
    click_client = frame->client;
    click_time = press->time;
    click_x = press->x;
    click_y = press->y;
    click_frame_x = frame->x;
    click_frame_y = frame->y;
}

static int frame_is_double_click(WmFrame *frame, XButtonEvent *press) {
    if (click_client != frame->client || click_time == 0) {
        return 0;
    }
    if (press->time < click_time ||
        press->time - click_time > WM_DOUBLE_CLICK_MS) {
        return 0;
    }
    if (frame->x != click_frame_x || frame->y != click_frame_y) {
        return 0;
    }
    int dx = press->x - click_x;
    int dy = press->y - click_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= WM_DOUBLE_CLICK_SLOP && dy <= WM_DOUBLE_CLICK_SLOP;
}

/* --- the module: the bar's clicks, drags and exposures -------------------- */

/* The frame a press or motion names.
 *
 * It can name two different windows. A grab this module took on the frame
 * reports the frame; a grab it took on the client — the Alt+move and Alt+resize
 * ones — reports the client, because that is the window the grab is on. Both
 * are the same window from the user's side, so both are resolved here and
 * nothing below has to know which grab answered. */
/* The managed client that owns a window, when the window is that client or one
 * of its descendants.
 *
 * A press that lands on a client's OWN CHILD WINDOW — every Wine window has
 * them, and so does every toolkit with a real widget tree — is reported on
 * that child, not on the client the manager holds in its frame table. A lookup
 * by the event's window alone therefore finds no frame, and the press is then
 * treated as one that belongs to nobody: the synchronous passive grab it
 * activated (see wm_frame_create: XGrabButton ... GrabModeSync) is never
 * answered with XAllowEvents, so the server keeps the pointer frozen. From the
 * user's side that is the mouse DISAPPEARING the moment it is clicked inside
 * such a window — which is exactly what happens in winecfg.
 *
 * Walking up from the event's window to the first window the table knows turns
 * that press back into the client's. The walk is bounded and reads the tree
 * with XQueryTree, so it costs one round trip per click and never loops.
 *
 * None is returned when the window is not under any managed client — the bar,
 * the menu, the root and every override-redirect window take that path, and
 * the callers treat None as "not ours". */
static Window frame_client_of(WmCore *core, Window window) {
    Window walk = window;

    for (int depth = 0; depth < 64 && walk != None && walk != core->root;
         depth++) {
        if (wm_frame_find(core, walk)) {
            return walk;
        }

        Window root_return = None;
        Window parent_return = None;
        Window *children = NULL;
        unsigned int count = 0;
        if (!XQueryTree(core->display, walk, &root_return, &parent_return,
                        &children, &count)) {
            break;
        }
        if (children) {
            XFree(children);
        }
        if (parent_return == None || parent_return == walk) {
            break;
        }
        walk = parent_return;
    }
    return None;
}

static WmFrame *frame_for_event(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find_by_frame(core, window);
    if (!frame) {
        frame = wm_frame_find(core, window);
    }
    if (!frame) {
        /* A press or motion on a client's own child window names the child,
           not the client — see frame_client_of(). */
        Window client = frame_client_of(core, window);
        if (client != None) {
            frame = wm_frame_find(core, client);
        }
    }
    return frame;
}

static void frame_begin_drag(WmCore *core, WmFrame *frame, XButtonEvent *press) {
    frame->dragging = 1;
    frame->drag_pointer_x = press->x_root;
    frame->drag_pointer_y = press->y_root;
    frame->drag_frame_x = frame->x;
    frame->drag_frame_y = frame->y;

    wm_focus_set(core, frame->client);
    wm_frame_raise(core, frame);

    /* The pointer is taken for the length of the drag. Without this a fast
       drag leaves the frame and the motion stops arriving, and the window
       stops following the hand. */
    XGrabPointer(core->display, frame->frame, False,
                 PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

static void frame_end_drag(WmCore *core, WmFrame *frame) {
    frame->dragging = 0;
    XUngrabPointer(core->display, CurrentTime);
    XFlush(core->display);
}

/* --- resizing --------------------------------------------------------------
 *
 * Alt+right-button resizes, and which sides move is decided by where the press
 * landed rather than by which key was held: a press near an edge takes that
 * edge, a press near a corner takes both, and a press in the body takes the
 * bottom and the right — the diagonal a hand pulls when it wants something
 * bigger. One mechanism, three ways of aiming it. */

/* Which sides a press at this point takes. The point is in the frame's own
   coordinates: the press may have arrived on the frame or on the client inside
   it, and the caller puts it into the same space either way, so the borders
   that can be grabbed are in one place. */
static int resize_edges_at(const WmFrame *frame, int x, int y) {
    int width = frame_width(frame);
    int height = frame_height(frame);
    int edges = 0;

    if (x <= WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_LEFT;
    } else if (x >= width - WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_RIGHT;
    }
    if (y <= WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_TOP;
    } else if (y >= height - WM_RESIZE_GRAB) {
        edges |= WM_RESIZE_BOTTOM;
    }

    if (edges == 0) {
        edges = WM_RESIZE_BOTTOM | WM_RESIZE_RIGHT;
    }
    return edges;
}

static void frame_begin_resize(WmCore *core, WmFrame *frame,
                               XButtonEvent *press) {
    frame->resizing = 1;
    frame->resize_edges = resize_edges_at(frame,
                                          press->x_root - frame->x,
                                          press->y_root - frame->y);
    frame->resize_pointer_x = press->x_root;
    frame->resize_pointer_y = press->y_root;
    frame->resize_x = frame->x;
    frame->resize_y = frame->y;
    frame->resize_width = frame->client_width;
    frame->resize_height = frame->client_height;

    /* A resize is measured from where it began, not from the last motion, so
       the numbers above are the ones every later motion is applied to. That is
       what makes a drag that is taken back undo itself exactly. */
    XGrabPointer(core->display, frame->frame, False,
                 PointerMotionMask | ButtonReleaseMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

/* One motion of a resize, from the pointer's absolute place. */
static void frame_resize_drag(WmCore *core, WmFrame *frame,
                              int pointer_x, int pointer_y) {
    WmSizeHints hints;
    int dx = pointer_x - frame->resize_pointer_x;
    int dy = pointer_y - frame->resize_pointer_y;

    int x = frame->resize_x;
    int y = frame->resize_y;
    int width = frame->resize_width;
    int height = frame->resize_height;
    int moving_left = (frame->resize_edges & WM_RESIZE_LEFT) != 0;
    int moving_top = (frame->resize_edges & WM_RESIZE_TOP) != 0;

    /* A side that is moving takes its share of the motion; the opposite side
       stays where it was. A left or top edge therefore moves the frame's
       origin as well as its size, which is the whole difference between
       growing a window and growing it outwards. */
    if (moving_left) {
        width = frame->resize_width - dx;
    } else if (frame->resize_edges & WM_RESIZE_RIGHT) {
        width = frame->resize_width + dx;
    }
    if (moving_top) {
        height = frame->resize_height - dy;
    } else if (frame->resize_edges & WM_RESIZE_BOTTOM) {
        height = frame->resize_height + dy;
    }

    /* The pointer's size is snapped to the grid the client asked for before it
       is applied, so the window is never a fraction of a cell — see the note
       above WmSizeHints. A client that named no increments has one pixel as
       its step and passes through unchanged. */
    frame_read_size_hints(core, frame, &hints);
    width = frame_hint_size(width, hints.base_width, hints.width_inc,
                            hints.min_width > WM_RESIZE_MIN_WIDTH
                                ? hints.min_width : WM_RESIZE_MIN_WIDTH);
    height = frame_hint_size(height, hints.base_height, hints.height_inc,
                             hints.min_height > WM_RESIZE_MIN_HEIGHT
                                 ? hints.min_height : WM_RESIZE_MIN_HEIGHT);

    /* The drag stops at the desktop, like every other size path. Without this
       a window dragged bigger than the screen has its right and bottom edges
       past the screen's own, where X keeps no pixels and raises no Expose when
       they are dragged back — the blank edge this limit exists to prevent,
       made here by the hand rather than by the program. The size is clamped
       BEFORE the origin is computed, so a left or top drag that hits the limit
       keeps its opposite edge still. */
    frame->client_width = width;
    frame->client_height = height;
    frame_clamp_size_to_screen(core, frame);
    width = frame->client_width;
    height = frame->client_height;

    /* The origin follows from the size for an edge that is moving: the
       OPPOSITE edge is the one that has to stay still. Done after the snapping
       so a left or top edge cannot drift by whatever the snapping rounded
       away. */
    if (moving_left) {
        x = frame->resize_x + (frame->resize_width - width);
    }
    if (moving_top) {
        y = frame->resize_y + (frame->resize_height - height);
    }

    frame->x = x;
    frame->y = y;
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

static void frame_end_resize(WmCore *core, WmFrame *frame) {
    frame->resizing = 0;
    XUngrabPointer(core->display, CurrentTime);
    XFlush(core->display);
}

/* Answer the synchronous grab a press may have arrived under.
 *
 * The passive grabs this module takes on a client are GrabModeSync: the
 * server holds the pointer until the manager answers with XAllowEvents. A
 * path that decides the press is not for it and returns without answering
 * leaves the device held, and the freeze is the server's rather than this
 * window's — so the whole display stops responding, which from the user's
 * side is a session that has gone dead.
 *
 * Only a window this module manages can have one of those grabs on it, and
 * the grab reports the client it was taken on, so that is the test. A press
 * that arrived on the frame itself, on the bar or on the root was not grabbed
 * and is left alone: sending an XAllowEvents for a grab that is not active is
 * a no-op at best and is what makes the difference between answering a grab
 * and guessing at one. */
static void frame_allow_sync_grab(WmCore *core, XButtonEvent *press) {
    /* The grab may have been taken on a client the press reached through one
       of that client's own CHILD windows, in which case the event names the
       child and not the client. Telling those apart from a press that belongs
       to nobody is the whole of this test: a press that is under SOME managed
       client activated that client's grab and has to be answered, or the
       pointer stays frozen (see frame_client_of). */
    if (wm_frame_find(core, press->window) ||
        frame_client_of(core, press->window) != None) {
        XAllowEvents(core->display, AsyncPointer, press->time);
    }
}

/* Put the pointer where a click on a scaled window actually is.
 *
 * A scaled window is drawn bigger than the frame showing it: the picture is
 * fitted to the frame, so the point the user aimed at is not the point inside
 * the window. The press arrives with the pointer where the hand left it, and
 * the program is about to be handed that coordinate — which is the wrong one,
 * by the same ratio the picture is scaled by. Half a window's width out at
 * half size.
 *
 * The pointer is therefore moved to the other end of the scale before the
 * press is replayed, so the program receives the coordinate it would have
 * received if it had been the size of the frame all along.
 *
 * Two things follow, and both are worth saying plainly rather than leaving to
 * be discovered: the pointer VISIBLY JUMPS, because it has to be somewhere and
 * the only place the program will look is its own coordinates; and the jump is
 * between clicks, so this is a feature for a game played with a pad or a
 * keyboard, not for one played by pointing. That is the honest cost of fitting
 * a window that does not want to be fitted, and it is why the key is a
 * deliberate press rather than something done to every window. */
static void frame_warp_click(WmCore *core, WmFrame *frame, XButtonEvent *press) {
    int client_x = press->x;
    int client_y = press->y;

    wm_compositor_unscale_point(frame->client,
                                frame->client_width, frame->client_height,
                                press->x, press->y, &client_x, &client_y);
    if (client_x == press->x && client_y == press->y) {
        return;     /* it is drawn at its own size; the point is the point */
    }

    /* Where that point is on the screen: the frame's corner, plus the frame's
       own chrome — the client sits at (border, WM_TITLE_HEIGHT) inside it —
       plus the point inside the window. */
    int root_x = frame->x + frame_border_of(frame) + client_x;
    int root_y = frame->y + frame_title_of(frame) + client_y;

    XWarpPointer(core->display, None, core->root, 0, 0, 0, 0, root_x, root_y);
    XSync(core->display, False);
}

static void frame_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case ButtonPress: {
        /* Alt+button is the manager's own, wherever it landed. The press may
           have arrived on the frame — the title bar and the border, which are
           the manager's own window — or on the client inside it, which the
           passive grabs route here. Both are resolved to the same frame so the
           two halves of a window behave the same way. */
        if ((event->xbutton.state & Mod1Mask) &&
            (event->xbutton.button == Button1 ||
             event->xbutton.button == Button3)) {
            int on_client = 0;
            WmFrame *frame = wm_frame_find(core, event->xbutton.window);
            if (frame) {
                on_client = 1;
            } else {
                frame = wm_frame_find_by_frame(core, event->xbutton.window);
            }
            if (!frame) {
                /* Neither a managed client nor one of this manager's frames,
                   but the grab was taken on a client and the press still has
                   to be answered or the pointer stays frozen. */
                frame_allow_sync_grab(core, &event->xbutton);
                break;
            }

            wm_frame_activate(core, frame);

            /* A frame that is maximised is put back before it is moved or
               resized: dragging a full-screen window is almost always meant as
               "take it out of full screen", and a resize of one has nothing to
               grab because its edges are the screen's. */
            if (frame->maximized) {
                wm_frame_maximize(core, frame);
            }

            /* The press on the client is held by a synchronous passive grab;
               releasing it without replaying is what makes the press the
               manager's rather than the program's. The frame's own window has
               no such grab, so nothing has to be released there. */
            if (on_client) {
                XAllowEvents(core->display, AsyncPointer, event->xbutton.time);
            }

            if (event->xbutton.button == Button1) {
                frame_begin_drag(core, frame, &event->xbutton);
            } else {
                frame_begin_resize(core, frame, &event->xbutton);
            }
            XFlush(core->display);
            break;
        }

        if (event->xbutton.button != Button1) {
            /* Not a button this manager acts on. A grab may still be holding
               the pointer, so it is released before the press is dropped. */
            frame_allow_sync_grab(core, &event->xbutton);
            break;
        }
        /* A press the passive grab took on a program's own window. The click
           was aimed at the program, so it is not answered here: the window is
           focused and raised, and the press is then replayed. Replaying is both
           what releases the synchronous grab and what hands the event on, so a
           click meant for the program still reaches it.

           The window named is resolved through the client's CHILD windows too
           (frame_client_of): a click inside a Wine window lands on one of the
           window's own child windows, and a lookup that only matched the client
           would find nothing — dropping the click, and leaving the synchronous
           grab unanswered so the pointer froze (the "mouse disappears in
           winecfg" fault). */
        Window client_window = event->xbutton.window;
        if (!wm_frame_find(core, client_window)) {
            Window owner = frame_client_of(core, client_window);
            if (owner != None) {
                client_window = owner;
            }
        }
        WmFrame *clicked = wm_frame_find(core, client_window);
        if (clicked) {
            wm_focus_set(core, clicked->client);
            wm_frame_raise(core, clicked);
            /* A scaled window is not where it looks, so the pointer is put
               where the program will look before the press is replayed. It is
               done here and not earlier because this is the only path that
               hands a press to the program: a click the manager answers itself
               never reaches the program and needs no correction. */
            if (clicked->scaled) {
                frame_warp_click(core, clicked, &event->xbutton);
            }
            XAllowEvents(core->display, ReplayPointer, event->xbutton.time);
            XFlush(core->display);
            break;
        }

        WmFrame *frame = wm_frame_find_by_frame(core, event->xbutton.window);
        if (!frame) {
            frame_allow_sync_grab(core, &event->xbutton);
            break;
        }
        WmFrameButton button = button_at(frame, event->xbutton.x,
                                         event->xbutton.y);
        if (button == WM_BUTTON_CLOSE) {
            wm_frame_close(core, frame);
        } else if (button == WM_BUTTON_MAXIMIZE) {
            wm_frame_maximize(core, frame);
        } else if (button == WM_BUTTON_MINIMIZE) {
            wm_frame_minimize(core, frame);
        } else if (event->xbutton.y < WM_TITLE_HEIGHT) {
            /* One click on the bar starts a drag; two fill the screen or put
               it back. Every desktop fills the screen on a double click, and
               the box is a small target to ask a hand to find. */
            if (frame_is_double_click(frame, &event->xbutton)) {
                click_forget();
                wm_frame_maximize(core, frame);
            } else {
                click_remember(frame, &event->xbutton);
                frame_begin_drag(core, frame, &event->xbutton);
            }
        }
        break;
    }
    case MotionNotify: {
        /* The window named here is whichever grab is reporting: the frame for a
           title-bar drag, the client for an Alt+move or Alt+resize. Both are
           resolved to the same frame, so the two paths are one. */
        WmFrame *frame = frame_for_event(core, event->xmotion.window);
        if (!frame) {
            break;
        }
        if (frame->dragging || frame->resizing) {
            /* COALESCE. The pointer reports every step it takes and the server
               queues them all, so one drag across the screen arrives as dozens
               of motion events, every one of them stale by the time it is read.
               Applying each one resizes the window, repaints the title bar, and
               — through the ConfigureNotify — makes the program inside redraw
               its whole content: dozens of times over for one visible movement,
               with the window trailing the hand by the whole backlog. That
               backlog is the lag of a resize drag. Only the newest position
               matters, so the queued ones are taken off the queue and dropped
               here and the work is done once.

               Matched by WINDOW, so another window's motion events are left
               alone: a drag owns the pointer, and the events it owns are the
               ones for the window its grab was taken on. */
            XEvent newer;
            while (XCheckTypedWindowEvent(core->display, event->xmotion.window,
                                          MotionNotify, &newer)) {
                *event = newer;
            }
        }
        if (frame->dragging) {
            int x = frame->drag_frame_x +
                    (event->xmotion.x_root - frame->drag_pointer_x);
            int y = frame->drag_frame_y +
                    (event->xmotion.y_root - frame->drag_pointer_y);
            /* The frame is kept clear of the bar at both edges, so a window
               can always be put somewhere it can be grabbed again. */
            wm_frame_move(core, frame, x, y);
        } else if (frame->resizing) {
            frame_resize_drag(core, frame,
                              event->xmotion.x_root, event->xmotion.y_root);
        }
        break;
    }
    case ButtonRelease: {
        WmFrame *frame = frame_for_event(core, event->xbutton.window);
        if (frame && frame->dragging) {
            frame_end_drag(core, frame);
        } else if (frame && frame->resizing) {
            frame_end_resize(core, frame);
        }
        break;
    }
    case Expose: {
        /* Exposes arrive in a RUN — a resize or a raise queues one per exposed
           rectangle — and `count` is how many more are already waiting for
           this window. Drawing for each of them draws the whole frame once per
           rectangle, and only the last one covers them all, so only that one
           draws. During a resize drag this is the difference between one
           repaint and a dozen per motion. */
        if (event->xexpose.count == 0) {
            WmFrame *frame =
                wm_frame_find_by_frame(core, event->xexpose.window);
            if (frame) {
                wm_frame_draw(core, frame);
            }
        }
        break;
    }
    case ConfigureNotify: {
        /* The screen changed size. A maximised window was fitted to the old
           size, so it is fitted again: leaving it is a window that no longer
           fills the screen it is on.

           A fullscreen client can try to change the display mode on the root
           instead of asking the WM for the fullscreen state. That changes the
           geometry under the manager, but it is not the desktop the user asked
           to use, so the WM's own size is kept fixed at the last trusted value
           rather than following the client-driven resize. */
        if (event->xconfigure.window == core->root) {
            int trusted_width = core->desktop_width > 1 ? core->desktop_width
                                                       : core->width;
            int trusted_height = core->desktop_height > 1 ? core->desktop_height
                                                          : core->height;

            if (event->xconfigure.width != trusted_width ||
                event->xconfigure.height != trusted_height) {
                /* A client changed the display mode out from under the manager.
                   The manager's own idea of the desktop is KEPT — and this is
                   what stops every window being squashed into the corner the
                   smaller mode left them in. The mode itself is put back by the
                   randr module (see wm_randr.c), which is the half that
                   actually undoes the change; this half only refuses to follow
                   it in the meantime.

                   NOTHING here is done to a fullscreen window, and that is a
                   correction. It used to be forced to the trusted size — the
                   whole screen, no chrome — on any stray root resize. That is
                   the very stretch-to-the-screen this desktop does not do: a
                   fullscreen window here is an ordinary container, and it is
                   resized by the same rules as any other (or not at all, when
                   the change was not the manager's doing). */
                core->width = trusted_width;
                core->height = trusted_height;
                core->desktop_width = trusted_width;
                core->desktop_height = trusted_height;
                for (int i = 0; i < core->frame_count; i++) {
                    WmFrame *frame = &core->frames[i];
                    if (frame->maximized && !frame->minimized) {
                        frame->maximized = 0;   /* so maximize() fits it again */
                        wm_frame_maximize(core, frame);
                    }
                }
                break;
            }

            core->width = event->xconfigure.width;
            core->height = event->xconfigure.height;
            core->desktop_width = core->width;
            core->desktop_height = core->height;
            for (int i = 0; i < core->frame_count; i++) {
                WmFrame *frame = &core->frames[i];
                if (frame->maximized && !frame->minimized) {
                    frame->maximized = 0;   /* so maximize() fits it again */
                    wm_frame_maximize(core, frame);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

const WmModule wm_frame_module = {
    .name = "frame",
    .init = NULL,
    .event = frame_event,
    .cleanup = NULL,
};
