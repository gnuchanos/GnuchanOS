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

/* Draw the bar again and keep it on top. Called when something outside this
   module changes what the bar should say. */
void wm_desktop_repaint(WmCore *core);

/* Put the bar back above the windows.
 *
 * Called by the frame module right after it raises a window, and that pairing
 * is the whole point: the bar and the windows are siblings on the root, so
 * raising a window puts it over the bar until the bar is raised again. Doing
 * it here, at the moment the window moves, is what keeps the bar on top
 * without a timer that restacks everything several times a second — which is
 * what a bar that re-raised itself on a tick used to do, and what made a
 * window appear to flicker while nothing was touching it. */
void wm_desktop_raise_bar(WmCore *core);

/* The bar's own window, or None when the session has no bar. The menu's logout
   walk needs it because the bar is a window this process owns, and killing a
   window this process owns closes its connection to the server. */
Window wm_desktop_bar_window(void);

#endif /* GNUCHANWM_DESKTOP_H */
