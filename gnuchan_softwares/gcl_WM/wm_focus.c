/*
 * wm_focus.c — input focus follows the pointer and the click.
 *
 * The manager is the only thing allowed to move the input focus: a client that
 * calls XSetInputFocus itself is ignored by the server while one is running, so
 * if this module did nothing the keyboard would go nowhere. It keeps
 * core->focused in step with what the server is told, because every other
 * module reads that field to know which window is "the current one".
 *
 * What arrives here is a window the pointer or the click landed on, and that is
 * often a frame — the title bar the manager owns — rather than the client
 * inside it. Focusing a frame would send the keyboard to a window with no
 * input, so every window is resolved to the client it holds before anything is
 * done with it. That resolution is the whole reason this module knows about
 * frames at all.
 *
 * The module also keeps the order the windows were last used in. That order is
 * not the frame table's: the table is creation order, and a user does not work
 * through their windows in the order they were opened. It is kept here because
 * this is the one place that knows a focus change has happened, and it is what
 * the switcher key reads to mean "the window before this one".
 */
#include <stdio.h>

#include "wm_core.h"
#include "wm_frame.h"

/* Note that a window is now the most recent one, moving it to the front of the
   order if it was already in it. The order holds each window once: a window
   seen twice would take two presses of the key to pass. */
static void focus_history_push(WmCore *core, Window window) {
    if (window == None) {
        return;
    }

    int found = -1;
    for (int i = 0; i < core->focus_history_count; i++) {
        if (core->focus_history[i] == window) {
            found = i;
            break;
        }
    }
    if (found == 0) {
        return;   /* already the most recent */
    }

    int count = core->focus_history_count;
    if (found > 0) {
        /* Move it up, closing the gap it leaves behind. */
        for (int i = found; i > 0; i--) {
            core->focus_history[i] = core->focus_history[i - 1];
        }
    } else {
        /* A window not in the order goes on the front, and the oldest falls
           off the end when the order is full. The one dropped is the least
           recently used, which is the one no longer worth a key press. */
        if (count == WM_MAX_FRAMES) {
            count--;
        }
        for (int i = count; i > 0; i--) {
            core->focus_history[i] = core->focus_history[i - 1];
        }
        core->focus_history_count = count + 1;
    }
    core->focus_history[0] = window;
}

void wm_focus_forget(WmCore *core, Window window) {
    if (window == None) {
        return;
    }
    for (int i = 0; i < core->focus_history_count; i++) {
        if (core->focus_history[i] != window) {
            continue;
        }
        for (int j = i; j + 1 < core->focus_history_count; j++) {
            core->focus_history[j] = core->focus_history[j + 1];
        }
        core->focus_history_count--;
        return;
    }
}

WmFrame *wm_focus_previous(WmCore *core) {
    /* The front of the order is the window that is focused now, so the
       previous one is the first entry after it that still exists. Entries are
       checked against the frame table because a window can be named here after
       it is gone; this must never offer a window the manager no longer has. */
    for (int i = 0; i < core->focus_history_count; i++) {
        Window window = core->focus_history[i];
        if (window == core->focused) {
            continue;
        }
        WmFrame *frame = wm_frame_find(core, window);
        if (frame) {
            return frame;
        }
    }
    return NULL;
}

/* The window that should actually take keyboard focus for one that was entered
   or clicked, or None when the window has no keyboard to take.
 *
 * Three things arrive here and only two of them are windows the keyboard
 * belongs on:
 *
 *   a client        itself                  the keyboard goes there
 *   a frame         the client inside it    the title bar is the manager's own
 *                                           window and has no input of its own
 *   the root        itself                  no window is focused
 *   anything else   None                    the bar, the menu, a dock
 *
 * The last case is the one that matters. The bar and the menu are the
 * manager's own override-redirect windows and they sit above the windows, so
 * moving the pointer onto the bar and off it again is a stream of EnterNotify
 * events naming a window that is not a client. Focusing one would send the
 * keyboard to a window with nothing to type into — and because every focus
 * change redraws the frame that lost it, each pass over the bar repainted
 * whatever window was underneath, which is what made a window that nobody was
 * touching flicker. Returning None drops those events instead: they are the
 * manager's own chrome and the focus simply does not move. */
static Window focus_target(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find_by_frame(core, window);
    if (frame) {
        return frame->client;
    }
    if (wm_frame_find(core, window)) {
        return window;
    }
    if (window == core->root) {
        return window;
    }
    return None;
}

void wm_focus_set(WmCore *core, Window window) {
    if (window == None) {
        return;
    }

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, window, &attributes)) {
        return;
    }
    if (attributes.map_state != IsViewable) {
        return;
    }

    if (core->focused == window) {
        return;
    }

    Window previous = core->focused;
    core->focused = window;
    XSetInputFocus(core->display, window, RevertToPointerRoot, CurrentTime);

    /* The window just focused is now the most recently used one, so it goes to
       the front of the order before anything reads it. */
    focus_history_push(core, window);

    /* Tell the window and everyone watching which window is active. This is
       what a task list highlights and what makes a window title bold. */
    XChangeProperty(core->display, core->root, core->net_active_window,
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&window, 1);

    /* Redraw both the window that lost the focus and the one that gained it:
       the bar and the outline of each are drawn in the colour that says which
       is which, so a change of focus is a change to two windows. */
    wm_manage_frame(core, window);
    if (previous != None) {
        wm_manage_frame(core, previous);
    }
}

void wm_focus_click(WmCore *core, Window window) {
    wm_focus_set(core, window);
}

static int focus_init(WmCore *core) {
    /* Focus the root on start: with no window focused, key events go to the
       root and the key module's grabs still reach us. */
    XSetInputFocus(core->display, core->root, RevertToPointerRoot, CurrentTime);
    core->focused = 0;
    return 0;
}

static void focus_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case EnterNotify: {
        /* Pointer-follows focus. A crossing that merely passed through a child
           window is not a deliberate move, so it does not steal the focus; a
           click always does, which is what ButtonPress is for. */
        if (event->xcrossing.mode == NotifyNormal &&
            event->xcrossing.detail != NotifyInferior) {
            wm_focus_set(core, focus_target(core, event->xcrossing.window));
        }
        break;
    }
    case ButtonPress: {
        Window client = focus_target(core, event->xbutton.window);
        wm_focus_click(core, client);
        /* Clicking a window brings it forward, which is what makes a stack of
           windows usable: without this the one that was opened last is on top
           for ever. */
        WmFrame *frame = wm_frame_find(core, client);
        if (frame) {
            wm_frame_raise(core, frame);
        }
        break;
    }
    default:
        break;
    }
}

const WmModule wm_focus_module = {
    .name = "focus",
    .init = focus_init,
    .event = focus_event,
    .cleanup = NULL,
};
