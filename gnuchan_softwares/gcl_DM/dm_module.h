/*
 * dm_module.h — the module interface every GnuChanDM module implements.
 *
 * A module is one concern: drawing the login form, routing input, and so on.
 * The core knows nothing about any of them — it opens the display, runs the
 * event loop, and hands every event to each module in turn. Adding a feature
 * is adding a file beside this one and a line in GnuChanDM.c.
 */
#ifndef GNUCHANDM_MODULE_H
#define GNUCHANDM_MODULE_H

#include <X11/Xlib.h>

typedef struct DmCore DmCore;

#define DM_MAX_MODULES 16

typedef struct DmModule {
    const char *name;

    /* Called once, after the display and the window exist. Returns 0 on
       success; a non-zero return stops the greeter from starting. */
    int (*init)(DmCore *core);

    /* Called for every event the core reads. */
    void (*event)(DmCore *core, XEvent *event);

    /* Called when the screen has to be redrawn. A module that draws nothing
       leaves this NULL. */
    void (*draw)(DmCore *core);

    /* Called once, in reverse order, when the greeter is shutting down. */
    void (*cleanup)(DmCore *core);
} DmModule;

#endif /* GNUCHANDM_MODULE_H */
