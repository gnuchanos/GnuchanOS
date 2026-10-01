/*
 * wm_randr.c — hold the desktop's display mode and put it back when a client
 * changes it.
 *
 * The bug this exists for: a Wine game — GTA Vice City is the one it was
 * written against — asks to be fullscreen, and a manager that does not answer
 * the EWMH fullscreen state leaves it with nowhere to go, so it reaches for
 * the DISPLAY instead. It changes the X server's screen mode: the whole
 * desktop is resized, every window on it is squashed into the smaller
 * rectangle, and the user's screen has visibly shrunk. The window manager is
 * still running and still thinks the desktop is its original size — see the
 * trusted geometry in wm_core.h — but the server underneath it is not.
 *
 * Ignoring the root's ConfigureNotify (which the core already does) is not
 * enough, because that only stops the manager from FOLLOWING the change. The
 * change has still been made. The mode itself has to be put back, and that is
 * what this module does: it remembers the CRTC configuration the session
 * started on and restores it the moment anything changes it.
 *
 * It is deliberately unconditional. A client changing the display mode is a
 * client changing the desktop out from under every other window on it, and
 * there is no case where a managed window is allowed to do that — the whole
 * point of the EWMH fullscreen state is that a window gets the screen WITHOUT
 * the screen changing size.
 *
 * A server without RandR, or with a RandR too old to have CRTCs, is not an
 * error: the module says so once and does nothing, and the session behaves
 * exactly as it did before the module existed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "wm_core.h"
#include "wm_randr.h"

/* The most CRTCs the desktop's mode is remembered for. A single-head machine
   has one; a laptop with an external display has two; the ceiling is high
   enough for a desk of monitors and low enough that the array is trivial. */
#define WM_MAX_CRTCS 16

/* One CRTC's configuration as it was found at start-up: the mode it was
   driving, where it put it, and which outputs it was connected to. That is
   everything XRRSetCrtcConfig() needs to put it back. */
typedef struct SavedCrtc {
    RRCrtc crtc;
    RRMode mode;
    int x;
    int y;
    Rotation rotation;
    int output_count;
    RROutput *outputs;      /* owned; freed in the cleanup */
} SavedCrtc;

static SavedCrtc saved[WM_MAX_CRTCS];
static int saved_count = 0;

static int randr_event_base = 0;
static int randr_ok = 0;

/* Set while a restore is being written, so the change-notify events the
   restore itself generates do not start another restore. The comparison in
   randr_restore() is what actually ends the loop — a second pass finds the
   mode already correct and writes nothing — but the flag keeps one call from
   re-entering itself. */
static int restoring = 0;

/* Forget the saved outputs. Called once, at shutdown. */
static void randr_free_saved(void) {
    for (int i = 0; i < saved_count; i++) {
        if (saved[i].outputs) {
            free(saved[i].outputs);
            saved[i].outputs = NULL;
        }
    }
    saved_count = 0;
}

/* Remember how every CRTC is set up right now. Called once, at start-up,
   before any client can have changed anything. */
static void randr_snapshot(WmCore *core) {
    XRRScreenResources *resources =
        XRRGetScreenResources(core->display, core->root);
    if (!resources) {
        return;
    }

    saved_count = 0;
    for (int i = 0; i < resources->ncrtc && saved_count < WM_MAX_CRTCS; i++) {
        XRRCrtcInfo *info = XRRGetCrtcInfo(core->display, resources,
                                           resources->crtcs[i]);
        if (!info) {
            continue;
        }

        SavedCrtc *entry = &saved[saved_count];
        entry->crtc = resources->crtcs[i];
        entry->mode = info->mode;
        entry->x = info->x;
        entry->y = info->y;
        entry->rotation = info->rotation;
        entry->output_count = info->noutput;
        entry->outputs = NULL;

        if (info->noutput > 0) {
            entry->outputs = malloc(sizeof(RROutput) *
                                    (size_t)info->noutput);
            if (entry->outputs) {
                memcpy(entry->outputs, info->outputs,
                       sizeof(RROutput) * (size_t)info->noutput);
            } else {
                entry->output_count = 0;
            }
        }

        saved_count++;
        XRRFreeCrtcInfo(info);
    }

    XRRFreeScreenResources(resources);
}

/* Put every CRTC back to the mode and place it was remembered with, but only
   where it differs — so a restore that has nothing to do is a handful of
   reads and no writes, which is what stops the restore's own change events
   from producing more of them. */
static void randr_restore(WmCore *core) {
    if (restoring || saved_count == 0) {
        return;
    }
    restoring = 1;

    XRRScreenResources *resources =
        XRRGetScreenResources(core->display, core->root);
    if (resources) {
        int changed = 0;
        for (int i = 0; i < saved_count; i++) {
            SavedCrtc *entry = &saved[i];
            XRRCrtcInfo *info = XRRGetCrtcInfo(core->display, resources,
                                               entry->crtc);
            if (!info) {
                continue;
            }
            if (info->mode != entry->mode || info->x != entry->x ||
                info->y != entry->y || info->rotation != entry->rotation) {
                XRRSetCrtcConfig(core->display, resources, entry->crtc,
                                 CurrentTime, entry->x, entry->y,
                                 entry->mode, entry->rotation,
                                 entry->outputs, entry->output_count);
                changed = 1;
            }
            XRRFreeCrtcInfo(info);
        }
        XRRFreeScreenResources(resources);

        if (changed) {
            XSync(core->display, False);
            fprintf(stderr,
                    "gnuchanwm: randr: a program changed the display mode; "
                    "the desktop's own mode has been put back.\n");
        }
    }

    restoring = 0;
}

static void randr_event(WmCore *core, XEvent *event) {
    if (!randr_ok) {
        return;
    }

    /* The RandR notices, which is the clean path: a mode change reaches this
       manager as RRScreenChangeNotify and a CRTC change as RRNotify. Both are
       restored from, and both are safe to restore from more than once —
       randr_restore() writes only the CRTCs that differ, so the notice the
       restore itself sends finds nothing left to do and stops. */
    int type = event->type - randr_event_base;
    if (type == RRScreenChangeNotify || type == RRNotify) {
        randr_restore(core);
    }

    /* And a second trigger, because not every client announces a mode change
       the same way. A game that changed the mode with the older
       XRRSetScreenConfig path, or through a tool that did not select for the
       RandR events on the root, still leaves the root at a different size —
       and the root's own ConfigureNotify always arrives. It is a cheap test
       and it means the guard holds even when the notice it was written for
       does not come. */
    if (event->type == ConfigureNotify &&
        event->xconfigure.window == core->root) {
        randr_restore(core);
    }
}

static int randr_init(WmCore *core) {
    int error_base = 0;
    int major = 0;
    int minor = 0;

    if (!XRRQueryExtension(core->display, &randr_event_base, &error_base)) {
        return 0;   /* no RandR: nothing to guard, nothing to say */
    }
    XRRQueryVersion(core->display, &major, &minor);
    if (!(major > 1 || (major == 1 && minor >= 2))) {
        /* Without CRTCs there is no mode to remember and restore. */
        return 0;
    }

    /* Ask to be told when the screen or a CRTC changes. It is the notice that
       is the trigger: the mode is not polled for, because the one moment it
       matters is the moment a client changes it. */
    XRRSelectInput(core->display, core->root,
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);
    randr_snapshot(core);
    randr_ok = saved_count > 0;

    if (randr_ok) {
        fprintf(stderr,
                "gnuchanwm: randr: holding the desktop at its own display "
                "mode (%d CRTC%s)\n",
                saved_count, saved_count == 1 ? "" : "s");
    }
    return 0;
}

static void randr_cleanup(WmCore *core) {
    (void)core;
    randr_free_saved();
    randr_ok = 0;
}

const WmModule wm_randr_module = {
    .name = "randr",
    .init = randr_init,
    .event = randr_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = randr_cleanup,
};
