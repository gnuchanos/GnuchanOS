/*
 * wm_frame_internal.h — the frame helpers the frame's own files share.
 *
 * wm_frame.c was one file doing four jobs and is now several with one each:
 * wm_frame_geometry.c decides the arithmetic (there is no window in it),
 * wm_frame.c holds the table, the lifecycle and the state changes,
 * wm_frame_draw.c draws the bar, wm_frame_fullscreen.c holds the two states a
 * client can ask a frame to take, and wm_frame_interact.c answers the clicks.
 *
 * The split is not free. Those files need the helper that is the whole of "put
 * the frame's numbers onto the windows" (frame_apply), the one that tells the
 * client its size (frame_notify_configure), the ones that decide what size a
 * client may be given (the size hints and the clamps) and the one that answers
 * a fullscreen request in the client's own property. They are NOT part of the
 * manager's public interface — nothing outside the frame's own files may call
 * them, and a module that moved a window itself would have skipped the
 * bookkeeping in frame_apply() that keeps the frame, the client and the bar in
 * step — so they are declared here rather than in wm_frame.h, which every
 * other module includes.
 */
#ifndef GNUCHANWM_FRAME_INTERNAL_H
#define GNUCHANWM_FRAME_INTERNAL_H

#include "wm_frame.h"

/* The steps a client asks to be resized in, read from WM_NORMAL_HINTS: a base
   size and an increment, so a terminal is only ever resized by whole cells. */
typedef struct WmSizeHints {
    int base_width;
    int base_height;
    int width_inc;
    int height_inc;
    int min_width;
    int min_height;
} WmSizeHints;

/* Put the frame's own numbers onto the frame window and the client inside it,
   and draw the bar. Every change that moves or resizes a frame ends in this,
   so the frame, the client and the bar cannot disagree about where they are. */
void frame_apply(WmCore *core, WmFrame *frame);

/* Tell the client what it actually got. A reparenting window manager is
   required to send this, and a toolkit that watches for it draws nothing until
   it hears. Nothing is sent to a scaled window, which was never resized. */
void frame_notify_configure(WmCore *core, WmFrame *frame);

/* Read the client's resize steps, and the size nearest to `value` they allow
   (`base` is where the increments count from, `minimum` is the floor). */
void frame_read_size_hints(WmCore *core, WmFrame *frame, WmSizeHints *hints);
int frame_hint_size(int value, int base, int increment, int minimum);

/* Hold a frame's client to the screen, or to the workarea less the chrome, so
   a window never has an edge past the place X keeps pixels for. A MAXIMISED
   window and a fullscreen-like one take the workarea; an ordinary window takes
   the whole screen. */
void frame_clamp_size_to_screen(WmCore *core, WmFrame *frame);
void frame_clamp_size_to_workarea(WmCore *core, WmFrame *frame);

/* Pull a frame's title bar back inside the workarea so it can be grabbed and
   dragged again. The SIZE is not touched — see the note on it in wm_frame.c. */
void frame_clamp_to_workarea(WmCore *core, WmFrame *frame);

/* Write or clear the client's own _NET_WM_STATE fullscreen member. The state
   lives in the CLIENT's property, so a window being destroyed or handed back
   to the root has to have it dropped, and that drop happens in wm_frame.c. */
void frame_publish_fullscreen_state(WmCore *core, WmFrame *frame, int on);

#endif /* GNUCHANWM_FRAME_INTERNAL_H */
