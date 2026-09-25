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
 */
#include <stdio.h>

#include "wm_core.h"
#include "wm_frame.h"

/* The window that should actually take keyboard focus for one that was entered
   or clicked: the client, when the window is a frame. */
static Window focus_target(WmCore *core, Window window) {
    WmFrame *frame = wm_frame_find_by_frame(core, window);
    if (frame) {
        return frame->client;
    }
    return window;
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
