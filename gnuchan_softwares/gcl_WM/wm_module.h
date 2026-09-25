/*
 * wm_module.h — the module interface every GnuChanWM module implements.
 */
#ifndef GNUCHANWM_MODULE_H
#define GNUCHANWM_MODULE_H

#include <X11/Xlib.h>

typedef struct WmCore WmCore;

#define WM_MAX_MODULES 16

typedef struct WmModule {
    const char *name;
    int (*init)(WmCore *core);
    void (*event)(WmCore *core, XEvent *event);
    void (*cleanup)(WmCore *core);
} WmModule;

#endif /* GNUCHANWM_MODULE_H */
