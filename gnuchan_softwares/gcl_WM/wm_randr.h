/*
 * wm_randr.h — keep a game from resizing the whole desktop.
 *
 * A Direct3D game that cannot get the fullscreen state the EWMH way reaches
 * for the DISPLAY instead: it changes the X server's mode, and the whole
 * desktop is resized under the window manager. From the user's side that is
 * the screen shrinking and everything on it being squashed — which is exactly
 * what a Wine game going fullscreen does on a manager that does not answer it.
 *
 * The module answers it by remembering the mode the desktop started on and
 * putting it back the moment something changes it. See wm_randr.c.
 */
#ifndef GNUCHANWM_RANDR_H
#define GNUCHANWM_RANDR_H

#include "wm_module.h"

extern const WmModule wm_randr_module;

#endif /* GNUCHANWM_RANDR_H */
