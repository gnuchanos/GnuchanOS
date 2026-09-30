/*
 * term_module.h — what a module of this terminal is.
 *
 * The same shape the window manager uses, and for the same reason: the core
 * knows how to run a list of modules and nothing about terminals, and every
 * feature is a file beside this one plus a line in the entry point. A module
 * is given the core and nothing else — it does not open its own display, it
 * does not read its own PTY, and it does not run its own loop.
 *
 *     init     once, before the loop, in registration order
 *     event    for every X event the core reads
 *     tick     when the display has been quiet for interval_ms
 *     cleanup  once, on the way out, in reverse order
 *
 * A module that wants nothing from an event leaves the pointer NULL; the core
 * checks. There is no `enabled` flag: a module that must not run is a module
 * that is not registered, which keeps the entry point the one place that says
 * what this terminal is.
 */
#ifndef GNUCHANTERM_MODULE_H
#define GNUCHANTERM_MODULE_H

#include <X11/Xlib.h>

/* The most modules the core holds. It is a ceiling on the array, not a plan:
   the session has ten, which leaves room to add a feature without touching
   this number, and a line here rather than a silent overflow if that ever
   stops being true. */
#define TERM_MAX_MODULES 16

/* The longest a module may ask to be woken. An interval longer than this is
   held here — a module that asks for an hour is a module with a bug, and one
   wake an hour would make the loop look hung when it was only waiting. */
#define TERM_MAX_TICK_MS 1000

typedef struct TermCore TermCore;

typedef struct TermModule {
    /* The name written to the log, and the only thing a failure names. */
    const char *name;

    int (*init)(TermCore *core);
    void (*event)(TermCore *core, XEvent *event);
    void (*tick)(TermCore *core);

    /* How often tick() is wanted, in milliseconds. Zero means never: the loop
       then blocks on the display outright, which is what a terminal with
       nothing to animate wants. */
    int interval_ms;

    void (*cleanup)(TermCore *core);
} TermModule;

#endif /* GNUCHANTERM_MODULE_H */
