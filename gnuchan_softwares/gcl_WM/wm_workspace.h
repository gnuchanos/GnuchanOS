/*
 * wm_workspace.h — the desktops a window can be on.
 *
 * A workspace is a screen the user can switch to; the windows on it are the
 * ones that are mapped while it is current. There is no window per workspace:
 * a window belongs to one, and switching is mapping the set that belongs to the
 * new one and unmapping the rest.
 *
 * The count is fixed by the layout widget the script draws: it shows the
 * layouts 0..5, so there are six. A config that drew a different range would
 * want a different count, which is why the number lives here as one constant
 * rather than being spelled out wherever it is used.
 */
#ifndef GNUCHANWM_WORKSPACE_H
#define GNUCHANWM_WORKSPACE_H

#include "wm_module.h"

/* WmCore and WmFrame are pointed at, never inspected here; their structs live
   in wm_core.h and wm_frame.h, which include this file's. */
typedef struct WmCore WmCore;
struct WmFrame;

/* The number of workspaces the desktop has. */
#define WM_WORKSPACE_COUNT 6

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
