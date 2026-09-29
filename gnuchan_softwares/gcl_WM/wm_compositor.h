/*
 * wm_compositor.h — scale a window's content to fit the window it is in.
 *
 * --- the problem -----------------------------------------------------------
 *
 * A window manager can make a window smaller. It tells the program the new
 * size, and a program that listens rearranges itself: that is how every
 * toolkit window behaves and nothing here is needed for it.
 *
 * A program that does NOT listen keeps drawing at the size it chose. The
 * window is 800x600 and the program still draws its interface across
 * 1600x900, so the right-hand side and the bottom of it are outside the window
 * and are simply not there. The buttons are gone, the map is gone, the menu is
 * gone. Shrinking the window did not shrink the game; it hid most of it.
 *
 * --- what this does --------------------------------------------------------
 *
 * The window's PIXELS are taken away from the server and drawn scaled instead,
 * so the whole of the content - the whole 1600x900 of it - is drawn inside the
 * 800x600 window. Nothing is hidden and the program is not asked to change:
 * it goes on drawing whatever size it likes, and what it drew is fitted into
 * the window for it.
 *
 * The three X calls that do it, and each one is about ONE WINDOW:
 *
 *   XCompositeRedirectWindow(window)      the window is no longer drawn to
 *                                         the screen by the server; what the
 *                                         program draws goes to a pixmap of
 *                                         its own that only this manager can
 *                                         see. Nobody else's window changes.
 *
 *   XCompositeNameWindowPixmap(window)    that pixmap, so it can be drawn.
 *
 *   XRenderComposite(...)                 the pixmap drawn into the window's
 *                                         place, at the size the window is,
 *                                         with a filter — which is the scal-
 *                                         ing, and the whole point.
 *
 * The screen and the rest of the desktop are not involved at all. The root
 * window is not redirected, the bar is not redirected, and a window that does
 * not need this is not redirected either: only the windows this module is
 * asked to scale.
 *
 * --- what it costs ---------------------------------------------------------
 *
 * A redirected window is drawn by this manager and by nobody else. If this
 * manager stops drawing it — a bug, a crash, a window it forgot — the window
 * shows whatever was last drawn, or nothing. That is why every path here is
 * written to fail by UNREDIRECTING: a window that cannot be scaled is put back
 * under the server and looks exactly as it did before, rather than being lost.
 *
 * It also needs the server to have the Composite, Damage and Render extensions.
 * A server without them cannot do this at all, and the module says so once and
 * leaves every window alone.
 *
 * --- what is not done here -------------------------------------------------
 *
 * OpenGL windows (a game drawn with GL — Stardew Valley is one) do not put
 * their pixels in the window the ordinary way; they draw into a GL buffer that
 * is copied to the window by the driver. Redirecting such a window gives a
 * stale first frame. Getting those needs GLX_EXT_texture_from_pixmap, which is
 * a fourth extension and a second code path, and it is deliberately left for
 * its own change: it is named in wm_compositor.c where it would go.
 */
#ifndef GNUCHANWM_COMPOSITOR_H
#define GNUCHANWM_COMPOSITOR_H

#include <X11/Xlib.h>

#include "wm_module.h"

/* WmCore is pointed at, never inspected here. */
typedef struct WmCore WmCore;

/* Whether scaling is available and wanted. False when the server lacks the
   extensions, when the settings script turned it off, or when the module was
   never registered — and every caller checks it, so a desktop that cannot do
   this behaves exactly as it did before it existed. */
int wm_compositor_available(void);

/* Take a window to be scaled, or give it back.
 *
 * `scale_size` is NOT passed here: what a window is scaled to is the size of
 * the window itself, which the frame already knows and changes as the user
 * drags the edges. So this only says "this one is scaled from now on", and the
 * drawing follows the frame's own geometry.
 *
 * A window this cannot be done to — a server without the extensions, or a
 * request the server refuses — is left exactly as it was and the call returns
 * -1. The caller treats that as "an ordinary window" and nothing is worse than
 * before.
 */
int  wm_compositor_scale(WmCore *core, Window window);
void wm_compositor_unscale(WmCore *core, Window window);

/* Whether a window is currently being scaled. The frame asks before it draws
   its client area: an ordinary window is the server's to draw and the frame
   leaves its rectangle alone, while a scaled one is drawn by this module and
   nothing else will appear there. */
int  wm_compositor_is_scaled(Window window);

/* Draw a scaled window's content into its frame.
 *
 * `target` is the frame's off-screen buffer, and `x`, `y`, `width` and
 * `height` are where the client sits inside it — the same numbers
 * frame_apply() moves the client to. The window's own size, which is what it
 * is drawing at, is read here rather than passed: it is the one thing that
 * changes without the frame being told, because the program is what changes
 * it.
 *
 * Returns 1 when something was drawn, 0 when there was nothing to draw — the
 * window is not ready, or its pixmap is stale — in which case the caller
 * leaves the frame's border colour showing rather than drawing nothing over
 * it. */
int  wm_compositor_draw(WmCore *core, Window client, Drawable target,
                        int x, int y, int width, int height);

/* Turn a point in the frame's client area into the point inside the window it
   was drawn from.
 *
 * This is what makes a scaled window clickable, and it is not optional: the
 * window is drawn SMALLER than it is, so a click lands where the user pointed
 * in the small picture while the program is waiting for a coordinate in its
 * own — four times as far right and down in a window drawn at half. Without
 * this every click is somewhere else and the window looks alive but answers
 * nothing.
 *
 * `target_width` and `target_height` are the client area's size — the
 * frame's client_width and client_height, which is how big the picture is
 * drawn — and `x` and `y` are in that area. The answer goes into `client_x`
 * and `client_y` in the window's OWN coordinates, which is what the program
 * is sent.
 *
 * A window that is not scaled is passed through unchanged, so a caller does
 * not have to ask whether this applies before calling it. */
void wm_compositor_unscale_point(Window client, int target_width,
                                 int target_height, int x, int y,
                                 int *client_x, int *client_y);

/* Draw a window again because its content changed. Called from the module's
   event callback for the Damage events the server sends, and by whoever needs
   a scaled window repainted at once — a frame moving, a window raised. */
void wm_compositor_repaint(WmCore *core, Window client);

/* Put every scaled window back under the server for a moment, and take them
   again.
 *
 * This is for the switcher, which reads the ROOT window's pixels to make its
 * little pictures: a redirected window is not in those pixels, so the pictures
 * would have a hole exactly where each scaled window is. Between the two calls
 * the windows are the server's again and are drawn unscaled.
 *
 * Both are safe to call when nothing is scaled or when the server cannot do
 * this at all, which is what lets the switcher call them unconditionally. */
void wm_compositor_suspend(WmCore *core);
void wm_compositor_resume(WmCore *core);

/* The module: it redirects the windows, watches them for damage, and draws
   them. Registered after the frame module, because it is the frame that
   decides which windows are drawn scaled. */
extern const WmModule wm_compositor_module;

#endif /* GNUCHANWM_COMPOSITOR_H */
