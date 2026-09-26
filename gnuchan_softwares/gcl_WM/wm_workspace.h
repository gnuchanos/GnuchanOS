/*
 * wm_workspace.h — the desktops a window can be on.
 *
 * A workspace is a screen the user can switch to; the windows on it are the
 * ones that are mapped while it is current. There is no window per workspace:
 * a window belongs to one, and switching is mapping the set that belongs to the
 * new one and unmapping the rest.
 *
 * How many workspaces there are is the script's number, not this file's: the
 * script draws them with a layout widget, so the range it drew is the answer,
 * and a line of its own may name a different one. WM_WORKSPACE_MAX below is
 * only the ceiling — the most a session may ask for — because the frame table
 * has to have a bound and a number written by hand can be anything.
 */
#ifndef GNUCHANWM_WORKSPACE_H
#define GNUCHANWM_WORKSPACE_H

#include "wm_module.h"

/* WmCore and WmFrame are pointed at, never inspected here; their structs live
   in wm_core.h and wm_frame.h, which include this file's. */
typedef struct WmCore WmCore;
struct WmFrame;

/* The most workspaces a session may ask for. A script that draws more than
   this has the extra ones ignored, which is the price of the frame table
   being an array rather than a list. */
#define WM_WORKSPACE_MAX 12

/* How many workspaces this session actually has, from the script. Falls back
   to one when the script named none, because a desktop with no workspaces
   cannot place a window on one. */
int wm_workspace_count(const WmCore *core);

/* Make one workspace the current one: show the windows that belong to it, hide
   the rest, and move the keyboard off a window that is no longer on screen. */
void wm_workspace_switch(WmCore *core, int workspace);

/* Put a frame on whichever workspace is current when it is opened. A new window
   belongs to the screen the user is looking at, not to the one they happened to
   be on when the program started. */
void wm_workspace_place(WmCore *core, struct WmFrame *frame);

/* Show or hide every frame so that exactly the current workspace's windows are
   on screen. Called after a window is put away or brought back, so the two
   things that map and unmap — minimise and workspace switching — agree. */
void wm_workspace_apply(WmCore *core);

extern const WmModule wm_workspace_module;

#endif /* GNUCHANWM_WORKSPACE_H */
