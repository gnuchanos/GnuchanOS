/*
 * wm_desktop.h — repainting and restacking the desktop from outside the module.
 *
 * The desktop module owns the backdrop and the bar, and almost nothing else
 * has any business drawing or moving them. The exceptions are the two changes
 * that happen outside it: switching workspace changes what the bar should say,
 * and raising a window can put that window above the bar.
 *
 * Both are given one entry point each rather than the bar's state being made
 * public, so the module still decides what the bar is and only the moment of
 * the change is described from outside.
 */
#ifndef GNUCHANWM_DESKTOP_H
#define GNUCHANWM_DESKTOP_H

/* Window is named in the declarations below, so the type has to be known:
   Xlib defines it as an unsigned long, and a caller that has the header and
   not the definition would be a caller with an implicit declaration. */
#include <X11/Xlib.h>

/* WmCore is pointed at, never inspected here; its struct lives in wm_core.h,
   which includes this file's. */
typedef struct WmCore WmCore;

/* Draw the bar again. Called when something outside this module changes what
   the bar should say. The bar stays below the windows; see wm_desktop_lower_bar
   for the stacking rule. */
void wm_desktop_repaint(WmCore *core);

/* Put the bar at the bottom of the stack, under every window.
 *
 * The bar and the windows are siblings on the root, and the bar belongs under
 * them: a window moved onto the bar then covers it, and the bar is seen again
 * as soon as the window moves off. The bar is still visible in its own strip
 * because windows are opened inside the workarea, which is the screen less the
 * bar.
 *
 * This is the opposite of the old wm_desktop_raise_bar, which kept the bar on
 * top and trapped any window dragged onto it. Called when the bar is configured
 * and after a repaint. */
void wm_desktop_lower_bar(WmCore *core);

/* The bar's own window, or None when the session has no bar. The menu's logout
   walk needs it because the bar is a window this process owns, and killing a
   window this process owns closes its connection to the server. */
int wm_desktop_is_bar_window(Window window);

#endif /* GNUCHANWM_DESKTOP_H */
