/*
 * wm_workspace.c — the desktops a window can be on.
 *
 * A workspace is not a window and not a screen: it is a number a window is
 * tagged with, and switching is mapping the frames that carry the new number
 * and unmapping the rest. That is the whole mechanism, and it is why a
 * workspace costs nothing until it has a window on it.
 *
 * There are two reasons a frame can be off screen, and they are not the same:
 *
 *   - minimised: the user put this one window away
 *   - on another workspace: the whole screen changed out from under it
 *
 * Both are "not mapped", so both the switcher key and a workspace switch have
 * to look at both before mapping a frame. That is what wm_workspace_apply is
 * for: it is the one place that decides whether a frame should be on screen,
 * and both callers go through it rather than each deciding for themselves.
 *
 * How many there are comes from the script — see wm_workspace_count below —
 * and not from a constant here, so the number of keys the switcher grabs and
 * the number of labels the bar draws are decided in the one place a person
 * writes them down.
 */
#include <stdio.h>

#include <X11/Xlib.h>

#include "wm_core.h"
#include "wm_desktop.h"
#include "wm_frame.h"
#include "wm_workspace.h"

int wm_workspace_count(const WmCore *core) {
    if (!core) {
        return 1;
    }
    int count = core->config.workspace_count;
    if (count < 1) {
        count = 1;
    }
    if (count > WM_WORKSPACE_MAX) {
        count = WM_WORKSPACE_MAX;
    }
    return count;
}

void wm_workspace_apply(WmCore *core) {
    for (int i = 0; i < core->frame_count; i++) {
        WmFrame *frame = &core->frames[i];

        /* A frame is on screen only when it is on the current workspace and
           the user has not put it away. Anything else is unmapped, which is
           also what the server does with a frame that is already down, so no
           case needs a test of what the frame's map state already is. */
        int wanted = (frame->workspace == core->current_workspace) &&
                     !frame->minimized;

        XWindowAttributes attributes;
        int shown = XGetWindowAttributes(core->display, frame->frame,
                                         &attributes) &&
                    attributes.map_state == IsViewable;

        if (wanted && !shown) {
            XMapRaised(core->display, frame->frame);
            wm_frame_draw(core, frame);
        } else if (!wanted && shown) {
            XUnmapWindow(core->display, frame->frame);
        }
    }
    XFlush(core->display);
}

void wm_workspace_switch(WmCore *core, int workspace) {
    if (workspace < 0 || workspace >= wm_workspace_count(core)) {
        return;
    }
    if (workspace == core->current_workspace) {
        return;
    }

    core->current_workspace = workspace;

    /* The keyboard may be on a window that is about to go away. It is moved
       first, while every window is still where it was, because finding the
       window to move it to means looking through the ones on the new
       workspace. */
    if (core->focused != None) {
        WmFrame *focused = wm_frame_find(core, core->focused);
        if (!focused || focused->workspace != workspace) {
            Window next = None;
            for (int i = 0; i < core->focus_history_count; i++) {
                WmFrame *candidate =
                    wm_frame_find(core, core->focus_history[i]);
                if (candidate && candidate->workspace == workspace &&
                    !candidate->minimized) {
                    next = candidate->client;
                    break;
                }
            }
            if (next != None) {
                core->focused = 0;
                wm_focus_set(core, next);
            } else {
                core->focused = 0;
                XSetInputFocus(core->display, core->root,
                               RevertToPointerRoot, CurrentTime);
            }
        }
    }

    wm_workspace_apply(core);

    /* The layout widget draws the workspaces and says which one is current, so
       a switch makes it wrong until it is drawn again. It is drawn here rather
       than on a timer because this is the one moment the answer changes. */
    wm_desktop_repaint(core);

    fprintf(stderr, "gnuchanwm: workspace %d of %d\n",
            workspace, wm_workspace_count(core));
}

void wm_workspace_place(WmCore *core, struct WmFrame *frame) {
    if (!frame) {
        return;
    }
    frame->workspace = core->current_workspace;
}

static int workspace_init(WmCore *core) {
    core->current_workspace = 0;
    return 0;
}

const WmModule wm_workspace_module = {
    .name = "workspace",
    .init = workspace_init,
    .event = NULL,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};
