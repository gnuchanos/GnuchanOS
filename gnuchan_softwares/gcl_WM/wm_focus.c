/*
 * wm_focus.c — input focus follows the pointer and the click.
 *
 * The WM is the only thing allowed to move the input focus: a client that
 * calls XSetInputFocus itself is ignored by the server while a WM is running,
 * so if this module did nothing the keyboard would go nowhere. It keeps
 * core->focused in step with what the X server is told, because every other
 * module reads that field to know which window is "the current one".
 */
#include <stdio.h>

#include "wm_core.h"

/* Move the focus to a window, if there is one, and record it. Focus moves are
   all-or-nothing: a window that has just been unmapped cannot take focus, and
   asking the server to focus a gone window is an error round-trip. */
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

    core->focused = window;
    XSetInputFocus(core->display, window, RevertToPointerRoot, CurrentTime);

    /* Tell the window and everyone watching which window is active. This is
       what a task list highlights and what makes a window title bold. */
    XChangeProperty(core->display, core->root, core->net_active_window,
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&window, 1);

    /* Re-draw the border of the window that gained focus, so the visual focus
       indicator matches the input focus. The drawing belongs to the manage
       module — this only says the focused window changed. */
    wm_manage_frame(core, window);
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
    case EnterNotify:
        /* Pointer-follows focus. A crossing that merely passed through a
           child window is not a deliberate move, so it does not steal the
           focus; a click always does, which is what ButtonPress is for. */
        if (event->xcrossing.mode == NotifyNormal &&
            event->xcrossing.detail != NotifyInferior) {
            wm_focus_set(core, event->xcrossing.window);
        }
        break;
    case ButtonPress:
        wm_focus_click(core, event->xbutton.window);
        break;
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
