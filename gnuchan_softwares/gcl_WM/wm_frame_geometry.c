/*
 * wm_frame_geometry.c — where a frame's chrome is, and how large it is.
 *
 * See wm_frame_geometry.h for why this is its own file. Every function here is
 * arithmetic over a WmFrame and nothing else: no display is opened, no window
 * is touched, and nothing is drawn. That is what makes it the safe part of the
 * frame code to keep apart from the drawing and the event handling.
 */
#include "wm_core.h"
#include "wm_frame_geometry.h"

/* --- geometry -------------------------------------------------------------
 * All of it is computed from the client's size, so there is one place that
 * decides what a frame looks like and every other function asks it. */

/* The chrome drawn around the client: the border on the left, right and
   bottom, and the title bar above.
 *
 * The chrome is kept for EVERY window, a fullscreen-like one included. That is
 * the whole point of this desktop: a client never gets to strip the frame off
 * itself.

   The reasoning, because it is not the obvious choice. EWMH says a fullscreen
   window has no decorations, and a game that asks for it means "give me the
   whole screen". A manager that answers by drawing the window borderless at
   (0,0) the size of the screen has handed the game the display — and the game
   is then a surface with no title bar and no buttons, which the user cannot
   move, cannot close and cannot get out of if the game hangs. That is the trap
   this file exists to avoid: the window is a CONTAINER that happens to be as
   large as the screen, not the screen itself.

   So a fullscreen-like window here means the client fills the visible screen
   area and the frame is drawn around it, exactly as on any other window. The
   client is inset by (border, title) and the frame is that much larger, so the
   title bar sits at the top of the screen and the buttons stay reachable. */
int frame_border_of(const WmFrame *frame) {
    return frame->border;
}

int frame_title_of(const WmFrame *frame) {
    return WM_TITLE_HEIGHT;
}

int frame_width(const WmFrame *frame) {
    return frame->client_width + 2 * frame_border_of(frame);
}

int frame_height(const WmFrame *frame) {
    return frame->client_height + frame_title_of(frame) + frame_border_of(frame);
}

/* The border width the desktop's style asks for, held inside what may be
   drawn. It is the one place the clamping happens, so a frame made now and a
   frame adjusted after a reload get the same answer. */
int style_border_width(const WmCore *core) {
    int width = core->style.border_width;
    if (width < 1) {
        width = WM_FRAME_BORDER;
    }
    if (width > WM_FRAME_BORDER_MAX) {
        width = WM_FRAME_BORDER_MAX;
    }
    return width;
}

/* Where a button sits inside the bar. They are laid out from the right edge
   inwards in the order of WmFrameButton, so close is rightmost — the corner a
   hand already expects — and every consumer reads the same rule. */
int button_x(const WmFrame *frame, WmFrameButton button) {
    int step = WM_BUTTON_SIZE + WM_BUTTON_GAP;
    return frame_width(frame) - WM_BUTTON_SIZE - WM_BUTTON_GAP
           - (int)button * step;
}

int button_y(void) {
    return (WM_TITLE_HEIGHT - WM_BUTTON_SIZE) / 2;
}

/* Which button a point on the bar is inside, or WM_BUTTON_COUNT for none. */
WmFrameButton button_at(const WmFrame *frame, int x, int y) {
    for (int i = 0; i < WM_BUTTON_COUNT; i++) {
        int bx = button_x(frame, (WmFrameButton)i);
        int by = button_y();
        if (x >= bx && x < bx + WM_BUTTON_SIZE &&
            y >= by && y < by + WM_BUTTON_SIZE) {
            return (WmFrameButton)i;
        }
    }
    return WM_BUTTON_COUNT;
}
