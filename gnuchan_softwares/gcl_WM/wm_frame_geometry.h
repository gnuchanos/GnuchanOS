/*
 * wm_frame_geometry.h — where a frame's chrome is, and how large it is.
 *
 * The shape of a frame is decided here and nowhere else: the border, the
 * title bar above it, the buttons in that bar, and the frame's whole size,
 * which is the client plus both. Every other part of the frame code asks these
 * functions rather than working the numbers out again, so a bar that is a
 * different height in one place than another is not a thing this window
 * manager can draw.
 *
 * They live in their own file because they are the one part of wm_frame.c that
 * touches no window, no display and no drawing: they are arithmetic over a
 * WmFrame, and wm_frame_draw.c, wm_frame.c and the compositor all read the
 * SAME arithmetic rather than each keeping its own copy of "border + 22".
 *
 * The chrome is the same for EVERY window, a fullscreen-like one included —
 * see frame_border_of() in the .c for why that is not a bug. Nothing in this
 * file reads WmFrame.fullscreen.
 */
#ifndef GNUCHANWM_FRAME_GEOMETRY_H
#define GNUCHANWM_FRAME_GEOMETRY_H

#include "wm_frame.h"

/* The struct WmCore lives in wm_core.h, which includes wm_frame.h, so the one
   function here that needs the core points at it through a forward
   declaration rather than the two headers including each other. Its caller
   includes wm_core.h for the definition. */
typedef struct WmCore WmCore;

/* The border on the left, right and bottom of the client. */
int frame_border_of(const WmFrame *frame);

/* The title bar above the client. */
int frame_title_of(const WmFrame *frame);

/* The whole frame: the client plus its chrome. */
int frame_width(const WmFrame *frame);
int frame_height(const WmFrame *frame);

/* The border width the desktop's style asks for, clamped to what may be drawn.
   The frame keeps its own copy in WmFrame.border; this is what that copy is
   taken from. */
int style_border_width(const WmCore *core);

/* Where one of the three buttons sits in the bar, and which one a point on the
   bar is inside — WM_BUTTON_COUNT for none. The layout and the hit test read
   the same two functions, so the button a click lands on is the button it is
   drawn over. */
int button_x(const WmFrame *frame, WmFrameButton button);
int button_y(void);
WmFrameButton button_at(const WmFrame *frame, int x, int y);

#endif /* GNUCHANWM_FRAME_GEOMETRY_H */
