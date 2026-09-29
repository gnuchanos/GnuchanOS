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
 * Both are "not mapped", so both have to be looked at before a frame is
 * mapped. That is what wm_workspace_apply is for: it is the one place that
 * decides whether a frame should be on screen, and every caller goes through
 * it rather than deciding for itself.
 *
 * There are two ways a window's workspace changes, and they are the mirror
 * image of each other:
 *
 *   wm_workspace_switch   the USER moves: one desk is shown and the others
 *                         are hidden, and the windows do not move at all
 *   wm_workspace_move     a WINDOW moves: the focused one is tagged with
 *                         another desk and the user stays where they are
 *
 * They differ only in which windows change and agree in everything after
 * that, which is why both end at the same place — see workspace_settle().
 *
 * How many there are comes from the script — see wm_workspace_count below —
 * and not from a constant here, so the number of keys the switcher grabs and
 * the number of labels the bar draws are decided in the one place a person
 * writes them down. Both sets of keys are bound from that same number, so a
 * session with four desks has four ways to reach one and four ways to send a
 * window to one.
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

/* Move the keyboard off a window that is no longer on `workspace`.
 *
 * Both of the things that change which workspace a window is on need this: a
 * switch moves the USER away from the window, and a move sends the WINDOW away
 * from the user. Either way the window the keyboard is on is no longer on the
 * screen, and the keyboard has to land on one that is.
 *
 * The window it lands on is the most recently used one that is still here,
 * which is the order the switcher walks and for the same reason: the window
 * the user was in before this one is the one they are most likely to want
 * next, and that order is already kept for exactly this.
 *
 * It runs BEFORE the map and unmap, while every window is still where it was.
 * The search asks whether each candidate is viewable, and that is a question
 * the server answers about the window as it is at that moment — a window that
 * was unmapped a moment ago is reported as not viewable until the server has
 * processed the change, so a focus decided after the unmap would be a focus on
 * nothing. */
static void workspace_focus_stay(WmCore *core, int workspace) {
    Window next = None;

    if (core->focused == None) {
        return;
    }

    WmFrame *focused = wm_frame_find(core, core->focused);
    if (focused && focused->workspace == workspace && !focused->minimized) {
        return;     /* the keyboard is already somewhere it can stay */
    }

    for (int i = 0; i < core->focus_history_count; i++) {
        WmFrame *candidate = wm_frame_find(core, core->focus_history[i]);
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
        /* Nothing here to focus: the keyboard goes to the root, which is where
           it starts and where the key grabs still reach this manager. */
        core->focused = 0;
        XSetInputFocus(core->display, core->root, RevertToPointerRoot,
                       CurrentTime);
    }
}

/* Show and hide so that exactly `workspace`'s windows are on screen, with the
   keyboard looked after and the bar told that the answer changed.
 *
 * A switch and a move differ in WHICH windows they change and agree in
 * everything after that, so the part they agree on is written once here. */
static void workspace_settle(WmCore *core, int workspace) {
    /* The keyboard first, while every window is still where it was — see
       workspace_focus_stay(). */
    workspace_focus_stay(core, workspace);

    wm_workspace_apply(core);

    /* The layout widget draws the workspaces and says which one is current, so
       both a switch and a move make it wrong until it is drawn again. It is
       drawn here rather than on a timer because this is the one moment the
       answer changes — and a move changes it too: the window that left the
       screen is one of the windows the bar was drawn from. */
    wm_desktop_repaint(core);
}

void wm_workspace_switch(WmCore *core, int workspace) {
    if (workspace < 0 || workspace >= wm_workspace_count(core)) {
        return;
    }
    if (workspace == core->current_workspace) {
        return;
    }

    core->current_workspace = workspace;
    workspace_settle(core, workspace);

    fprintf(stderr, "gnuchanwm: workspace %d of %d\n",
            workspace, wm_workspace_count(core));
}

void wm_workspace_move(WmCore *core, int workspace) {
    WmFrame *frame;

    if (workspace < 0 || workspace >= wm_workspace_count(core)) {
        return;
    }
    if (core->focused == None) {
        return;
    }

    frame = wm_frame_find(core, core->focused);
    if (!frame) {
        return;
    }
    /* Sending a window to the desk it is already on is not a move, and doing
       the work anyway would unmap and remap the window the user is typing in
       — seen from the outside as that window blinking for no reason. */
    if (frame->workspace == workspace) {
        return;
    }

    /* The window's workspace changes and the USER's does not: that is the
       whole difference between this and a switch. core->current_workspace is
       deliberately left alone, so the window leaves this screen rather than
       the user following it. */
    frame->workspace = workspace;

    /* The focused window has just left, and the frame may have to be hidden if
       it was on another desk to begin with. workspace_settle() does both, and
       the workspace it is given is the one the user is still looking at — so
       the keyboard lands on something that is still on screen. */
    workspace_settle(core, core->current_workspace);

    fprintf(stderr, "gnuchanwm: window '%s' moved to workspace %d of %d\n",
            frame->has_name && frame->name[0] ? frame->name : "window",
            workspace, wm_workspace_count(core));
}

void wm_workspace_step(WmCore *core, int steps) {
    int count = wm_workspace_count(core);
    if (count <= 1) {
        return;
    }

    /* Wrapping by modulo, with the count added first so a negative step is
       still a walk forward round the ring: stepping back one from the first
       workspace has to land on the last one, not on a number below zero. */
    int next = (core->current_workspace + steps) % count;
    if (next < 0) {
        next += count;
    }
    wm_workspace_switch(core, next);
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
