/*
 * wm_frame.h — the title bar and the frame around every managed window.
 *
 * A window manager that only draws a border cannot be used: a window with no
 * title bar cannot be moved, and a window that cannot be moved is stuck where
 * it opened. Making one movable is the whole of this file.
 *
 * It is done by reparenting, which is what every real window manager does. The
 * manager makes a frame window, puts the client inside it, and draws a title
 * bar of its own above it. The client keeps believing it is a top-level window
 * — the X server translates the coordinates — while what the user clicks and
 * drags is the frame the manager owns.
 *
 *     root
 *      └── frame          what is moved, dragged and drawn (override-redirect)
 *           └── client    the program's window, reparented inside it
 *
 * The frame is override-redirect on purpose. The root redirects structure, so
 * a normal window's map would come back to the manager as its own MapRequest
 * and the manager would be talking to itself for ever; an override-redirect
 * window is what the server maps without asking.
 *
 * Everything the frame draws goes into an off-screen copy first and is put on
 * the window in one operation. Drawing straight to the window means the title
 * bar is cleared, then partly redrawn, then finished — and a window being
 * dragged or focused is being redrawn many times a second, so that order is
 * seen as a flicker. See wm_frame.c for what fills the copy.
 */
#ifndef GNUCHANWM_FRAME_H
#define GNUCHANWM_FRAME_H

#include <X11/Xlib.h>

#include "wm_module.h"

/* The struct WmCore lives in wm_core.h, which includes this file, so a frame
   points back at the core through a forward declaration rather than by the two
   headers including each other. */
typedef struct WmCore WmCore;

/* The chrome every window shares. Constants, not per-window choices: a title
   bar that is a different height on every window is not one desktop. */
#define WM_TITLE_HEIGHT 22
#define WM_FRAME_BORDER 2
#define WM_BUTTON_SIZE 16
#define WM_BUTTON_GAP 6
#define WM_MAX_FRAMES 128

/* The three buttons, in the order they are drawn from the right edge inwards:
   close is rightmost, then maximise, then minimise. A count and an order, so
   the drawing loop, the hit test and the geometry all read the same table. */
typedef enum WmFrameButton {
    WM_BUTTON_CLOSE = 0,
    WM_BUTTON_MAXIMIZE,
    WM_BUTTON_MINIMIZE,
    WM_BUTTON_COUNT,
} WmFrameButton;

/* One managed window, as the desktop holds it. */
typedef struct WmFrame {
    Window frame;        /* the manager's window: moved, drawn, dragged      */
    Window client;       /* the program's window, reparented inside it       */

    int x, y;            /* where the frame sits on the screen               */

    int client_width;    /* the client's size, which the frame wraps         */
    int client_height;

    int dragging;        /* 1 while the title bar is held                    */
    int drag_pointer_x;  /* where the pointer was when the drag began        */
    int drag_pointer_y;
    int drag_frame_x;    /* where the frame was when the drag began          */
    int drag_frame_y;

    /* The two states the title bar's buttons put a window in. A minimised
       window is unmapped rather than iconified: there is no task list on this
       desktop to bring it back from, so the way back is the switcher key, and
       an unmapped frame is what that key restores. */
    int minimized;       /* 1 while the frame is put away                    */
    int maximized;       /* 1 while the client fills the screen              */

    /* Where the frame was before it was maximised, so the same button puts it
       back. Only meaningful while maximized is set. */
    int restore_x, restore_y;
    int restore_width, restore_height;

    /* The off-screen copy every drawing goes into, and the size it currently
       holds. Redrawing a window many times a second straight to the screen is
       what flickers; drawing here and copying once is what does not. */
    Pixmap buffer;
    int buffer_width, buffer_height;

    int has_name;        /* whether the client named itself                  */
    char name[160];
} WmFrame;

/* --- the table ------------------------------------------------------------ */

WmFrame *wm_frame_find(WmCore *core, Window client);
WmFrame *wm_frame_find_by_frame(WmCore *core, Window frame);

/* --- the frame ------------------------------------------------------------ */

/* Take a client into a frame: make the frame, reparent the client into it, map
   both and draw the bar. NULL when the client already has a frame, or is one
   that asks not to be managed. */
WmFrame *wm_frame_create(WmCore *core, Window client);

/* Put the frame at a new place on the screen. */
void wm_frame_move(WmCore *core, WmFrame *frame, int x, int y);

/* Resize the frame so it wraps a client of this size, and tell the client what
   it got. The client's position never changes: the bar is always above it. */
void wm_frame_resize(WmCore *core, WmFrame *frame, int width, int height);

/* Put the frame above the other windows. */
void wm_frame_raise(WmCore *core, WmFrame *frame);

/* Drop the frame. With client_gone the client is already destroyed and is not
   touched; otherwise it is put back on the root first, so a program that
   outlives the window manager is not left inside a window nobody owns. */
void wm_frame_destroy(WmCore *core, WmFrame *frame, int client_gone);

/* --- acting on a window --------------------------------------------------- */

/* Ask the client to close through WM_DELETE_WINDOW. A client that does not
   speak the protocol has no way to be asked and is killed, which is what every
   window manager does with one. */
void wm_frame_close(WmCore *core, WmFrame *frame);

/* Put the frame away, and bring it back. A minimised frame is unmapped; the
   switcher key restores it, because this desktop has no task list to click it
   in. */
void wm_frame_minimize(WmCore *core, WmFrame *frame);
void wm_frame_restore(WmCore *core, WmFrame *frame);

/* Fill the screen with the client, or put it back where it was. It is the
   client that is resized, not the frame: a maximised window keeps its title
   bar, so its buttons stay reachable. */
void wm_frame_maximize(WmCore *core, WmFrame *frame);

/* Bring a window to the front and give it the keyboard, restoring it first if
   it was minimised. The switcher key and the focus module both go through this,
   so it is the one place that says what "go to this window" means. */
void wm_frame_activate(WmCore *core, WmFrame *frame);

/* Read the client's name again and redraw the bar. */
void wm_frame_update_name(WmCore *core, WmFrame *frame);

/* Bring the frame back in step with a client that moved or resized itself.
   Called when the server reports the client changed. */
void wm_frame_sync(WmCore *core, WmFrame *frame);

/* --- drawing -------------------------------------------------------------- */

void wm_frame_draw(WmCore *core, WmFrame *frame);
void wm_frame_draw_all(WmCore *core);

/* --- the module ----------------------------------------------------------- */

/* The module answers the title bar's clicks, drags and exposures. It is what
   turns a drawn bar into one that can actually be used. */
extern const WmModule wm_frame_module;

#endif /* GNUCHANWM_FRAME_H */
