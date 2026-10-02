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
 * what this module does: it remembers the CRTC configuration AND the size of
 * the screen the session started with, and restores both the moment anything
 * changes them.
 *
 * BOTH are restored, and restoring only the CRTCs is the mistake this file was
 * corrected for. A game that changes the display mode changes the SIZE OF THE
 * SCREEN, not only the mode a CRTC drives: putting the CRTCs back alone leaves
 * the root window at the game's size, so the whole desktop stays in the corner
 * of a black screen — the exact "small screen in the left corner" a Wine game
 * leaves behind. The screen size is restored as well, and it is restored with
 * XRRSetScreenSize() and not by the CRTC write, because those are two separate
 * things in RandR: a CRTC can be set to a mode that fits a screen, and the
 * screen can still be a different size. So the screen is grown first (a CRTC
 * may not be set to a mode that does not fit the screen), the CRTCs are put
 * back, and the screen is then brought to exactly its own size.
 *
 * It is deliberately unconditional. A client changing the display mode is a
 * client changing the desktop out from under every other window on it, and
 * there is no case where a managed window is allowed to do that — the whole
 * point of the EWMH fullscreen state is that a window gets the screen WITHOUT
 * the screen changing size. A slow tick checks the same thing once a second as
 * a safety net for a change that produced no notice at all, which some drivers
 * and the older XRRSetScreenConfig path do: the check writes nothing when the
 * mode is already right, so a desktop that is behaving costs a few round trips
 * and nothing else.
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
#include "wm_frame.h"
#include "wm_randr.h"

/* The most CRTCs the desktop's mode is remembered for. A single-head machine
   has one; a laptop with an external display has two; the ceiling is high
   enough for a desk of monitors and low enough that the array is trivial. */
#define WM_MAX_CRTCS 16

/* How often the safety net runs. Once a second is short enough that a game
   which shrinks the screen and sends no notice is put back before the user has
   time to think the desktop is broken, and long enough that a session which is
   behaving spends almost nothing on it. */
#define WM_RANDR_TICK_MS 1000

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

/* The size of the screen the session started on. The CRTCs above describe
   which mode each output drives; this is the root window's own size, which a
   mode change moves as well and which XRRSetCrtcConfig() does NOT put back.
   The millimetre fields are the physical size, which does not change and is
   only carried so XRRSetScreenSize() is given a complete answer. */
static int saved_screen_width = 0;
static int saved_screen_height = 0;
static int saved_screen_width_mm = 0;
static int saved_screen_height_mm = 0;

static int randr_event_base = 0;
static int randr_ok = 0;

/* Set while a restore is being written, so the change-notify events the
   restore itself generates do not start another restore. The comparison in
   randr_restore() is what actually ends the loop — a second pass finds the
   mode already correct and writes nothing — but the flag keeps one call from
   re-entering itself. */
static int restoring = 0;

/* The root window's CURRENT size, read fresh from the server.
 *
 * DisplayWidth()/DisplayHeight() are NOT used, and that is on purpose: those
 * are read from the Screen structure Xlib filled in when the connection was
 * opened, and Xlib does not necessarily update them when a mode change arrives
 * — so a restore driven by them would compare the new size against a cached
 * copy of the old one and conclude there was nothing to do. XGetWindowAttributes
 * is a round trip, so what it returns is what the server has now, which is the
 * only thing worth comparing against. */
static void randr_current_size(WmCore *core, int *width, int *height) {
    XWindowAttributes attributes;
    if (XGetWindowAttributes(core->display, core->root, &attributes)) {
        *width = attributes.width;
        *height = attributes.height;
        return;
    }
    *width = DisplayWidth(core->display, core->screen);
    *height = DisplayHeight(core->display, core->screen);
}

/* The size of the mode a CRTC is driving RIGHT NOW — the resolution a client
   asked for when it changed the display mode. It is read before the mode is put
   back, because it is the one place a fullscreen client states how large it
   wants to be; wm_frame_set_fullscreen_size() is what turns it into the size of
   the container. Zero when there is no mode to read. */
static void randr_current_mode_size(WmCore *core, int *width, int *height) {
    *width = 0;
    *height = 0;

    XRRScreenResources *resources =
        XRRGetScreenResources(core->display, core->root);
    if (!resources) {
        return;
    }

    for (int i = 0; i < resources->ncrtc; i++) {
        XRRCrtcInfo *info = XRRGetCrtcInfo(core->display, resources,
                                           resources->crtcs[i]);
        if (!info) {
            continue;
        }

        if (info->mode != None) {
            for (int m = 0; m < resources->nmode; m++) {
                if (resources->modes[m].id == info->mode) {
                    if (resources->modes[m].width > 0 &&
                        resources->modes[m].height > 0) {
                        *width = (int)resources->modes[m].width;
                        *height = (int)resources->modes[m].height;
                    }
                    break;
                }
            }
        }
        XRRFreeCrtcInfo(info);
        if (*width > 0) {
            break;
        }
    }

    XRRFreeScreenResources(resources);
}

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

/* Remember how every CRTC is set up right now, and how large the screen is.
   Called once, at start-up, before any client can have changed anything. */
static void randr_snapshot(WmCore *core) {
    randr_current_size(core, &saved_screen_width, &saved_screen_height);
    saved_screen_width_mm = DisplayWidthMM(core->display, core->screen);
    saved_screen_height_mm = DisplayHeightMM(core->display, core->screen);

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

/* Put the screen and every CRTC back to the size and mode they were remembered
   with, but only where they differ — so a restore that has nothing to do is a
   handful of reads and no writes, which is what stops the restore's own change
   events from producing more of them, and what makes the once-a-second check
   free when nothing is wrong. */
static void randr_restore(WmCore *core) {
    if (restoring || saved_count == 0) {
        return;
    }
    restoring = 1;

    int changed = 0;
    int width = 0;
    int height = 0;

    /* What resolution the client asked for, read BEFORE anything is put back.
       A mode change is the one place a fullscreen game states how large it
       wants to be — it publishes no size hint and makes its own window at
       whatever size Wine decides, which is the size of the screen Wine saw.
       That resolution becomes the size of the container (see
       wm_frame_set_fullscreen_size), so the game is shown at the size it chose
       and not stretched to the screen, which is what left it a small picture
       in a field of black. */
    {
        int want_width = 0;
        int want_height = 0;
        randr_current_mode_size(core, &want_width, &want_height);
        if (want_width > 1 && want_height > 1 &&
            (want_width != saved_screen_width ||
             want_height != saved_screen_height)) {
            wm_frame_set_fullscreen_size(core, want_width, want_height);
        }
    }

    /* The screen is grown FIRST, and that ordering is not cosmetic: a CRTC may
       not be set to a mode that does not fit inside the screen, so a CRTC
       cannot be put back to a large mode while the screen is still the small
       size a game left it at. The screen is taken to whichever of the two is
       larger, so there is room either way; the exact size is set once the
       CRTCs are back. */
    randr_current_size(core, &width, &height);
    if (width != saved_screen_width || height != saved_screen_height) {
        int room_width = width > saved_screen_width ? width : saved_screen_width;
        int room_height = height > saved_screen_height ? height : saved_screen_height;
        XRRSetScreenSize(core->display, core->root, room_width, room_height,
                         saved_screen_width_mm, saved_screen_height_mm);
        changed = 1;
    }

    XRRScreenResources *resources =
        XRRGetScreenResources(core->display, core->root);
    if (resources) {
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
    }

    /* And now the screen is brought to exactly its own size. A game may have
       made it larger as well as smaller, and an enlarged desktop is as wrong
       as a shrunken one; with the CRTCs back, shrinking it now cannot cut a
       CRTC off, because the CRTCs fit the size being asked for. */
    randr_current_size(core, &width, &height);
    if (width != saved_screen_width || height != saved_screen_height) {
        XRRSetScreenSize(core->display, core->root, saved_screen_width,
                         saved_screen_height, saved_screen_width_mm,
                         saved_screen_height_mm);
        changed = 1;
    }

    if (changed) {
        XSync(core->display, False);
        fprintf(stderr,
                "gnuchanwm: randr: a program changed the display mode; "
                "the desktop's own mode has been put back.\n");
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
       randr_restore() writes only what differs, so the notice the restore
       itself sends finds nothing left to do and stops. */
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

/* The safety net.
 *
 * The events above are the intended trigger and they are enough when they
 * arrive — but a driver that changes the mode without sending the notice, or a
 * client that used a path that did not ask for one, leaves the desktop shrunk
 * with nothing to react to. This runs the same restore once a second and does
 * nothing at all when the mode is already right, so its cost on a healthy
 * session is a few round trips and no writes. */
static void randr_tick(WmCore *core) {
    if (!randr_ok) {
        return;
    }
    randr_restore(core);
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
       is the trigger: the mode is not polled for as the FIRST line of defence,
       because the one moment it matters is the moment a client changes it —
       the tick above is only the net under that. */
    XRRSelectInput(core->display, core->root,
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);
    randr_snapshot(core);
    randr_ok = saved_count > 0;

    if (randr_ok) {
        fprintf(stderr,
                "gnuchanwm: randr: holding the desktop at its own display "
                "mode (%d CRTC%s, %dx%d)\n",
                saved_count, saved_count == 1 ? "" : "s",
                saved_screen_width, saved_screen_height);
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
    .tick = randr_tick,
    .interval_ms = WM_RANDR_TICK_MS,
    .cleanup = randr_cleanup,
};
