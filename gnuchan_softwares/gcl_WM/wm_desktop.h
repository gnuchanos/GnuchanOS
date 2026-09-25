/*
 * wm_desktop.h — repainting the desktop from outside the module.
 *
 * The desktop module owns the backdrop and the bar, and almost nothing else
 * has any business drawing them. The one exception is a change that alters
 * what the bar shows without the desktop having asked for it — switching
 * workspace changes the layout the bar marks as current — so that change is
 * given one entry point rather than reaching into the module's state.
 */
#ifndef GNUCHANWM_DESKTOP_H
#define GNUCHANWM_DESKTOP_H

/* WmCore is pointed at, never inspected here; its struct lives in wm_core.h,
   which includes this file's. */
typedef struct WmCore WmCore;

/* Draw the bar again and keep it on top. Called when something outside this
   module changes what the bar should say. */
void wm_desktop_repaint(WmCore *core);

#endif /* GNUCHANWM_DESKTOP_H */
