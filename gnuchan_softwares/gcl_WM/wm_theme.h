/*
 * wm_theme.h — the desktop's answer to "which theme is this session using".
 *
 * A window manager is not the thing that draws a GTK button. It is, however,
 * the first thing running in a session and the only thing that talks to the X
 * server on the session's behalf — so it is where the answer to "which theme"
 * has to live, or every program guesses at start-up and they guess differently.
 *
 * The names are published two ways, because the two ways cover different
 * programs. XSETTINGS is what every GTK application reads, and a program that
 * has already started re-reads it when it changes, so a theme chosen here
 * reaches windows that are already open. The resource manager is what X itself
 * and a program without a toolkit read, and it is where the cursor's name and
 * size are written: the cursor is set by the X server the first time a client
 * asks for one, so a session that never says has to make do with whatever the
 * pointer theme was left at.
 *
 * The names come from the environment first, so a user or a session entry can
 * name a theme without touching this file, and fall back to the themes this
 * system ships. A name is only published when the theme is installed: naming a
 * theme that is not there draws every window in the toolkit's built-in grey,
 * which looks less like a theme was chosen and more like one was lost.
 */
#ifndef GNUCHANWM_THEME_H
#define GNUCHANWM_THEME_H

#include "wm_module.h"

/* The module publishes the session's theme and cursor. */
extern const WmModule wm_theme_module;

#endif /* GNUCHANWM_THEME_H */
