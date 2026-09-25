/*
 * wm_module.h — the module interface every GnuChanWM module implements.
 *
 * A module is a piece of the window manager that owns something: the focus,
 * the keys, the desktop, the bar. The core knows how to run a list of them
 * and nothing about what any of them do, so the whole window manager, as a
 * feature set, is the list of modules GnuChanWM.c registers.
 *
 * Three of the four callbacks are events: init when the session starts,
 * event for every X event in turn, cleanup when it ends. The fourth is time.
 *
 * A window manager is event-driven, and for most of what it does that is
 * exactly right — nothing happens until something happens. Two things a
 * desktop needs are not events, though: a clock has to move, and a settings
 * file has to be noticed when it is saved. Both are "look again after a
 * while", and neither has an X event to hang on. So a module may ask to be
 * woken: set tick and give it the number of milliseconds to wait, and the
 * core will return to it between events even when the display is quiet.
 *
 * A module that wants no time just leaves both out, and the core's wait is
 * infinite when no module wants any — an idle session with no clock costs
 * nothing to keep open.
 */
#ifndef GNUCHANWM_MODULE_H
#define GNUCHANWM_MODULE_H

#include <X11/Xlib.h>

typedef struct WmCore WmCore;

#define WM_MAX_MODULES 16

/* A module that asks for more than this between ticks is a module that should
   not have asked; the core clamps rather than trusting it. */
#define WM_MAX_TICK_MS 60000

typedef struct WmModule {
    const char *name;
    int (*init)(WmCore *core);
    void (*event)(WmCore *core, XEvent *event);

    /* The idle callback, and how often it wants to run. Both or neither: a
       tick with no interval has no answer to "how long do I wait", and an
       interval with no tick is a request nobody serves. */
    void (*tick)(WmCore *core);
    int interval_ms;

    void (*cleanup)(WmCore *core);
} WmModule;

#endif /* GNUCHANWM_MODULE_H */
