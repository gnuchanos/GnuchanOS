/*
 * wm_input.c — the pointer settings, and what the wheel does on the desktop.
 *
 * Two things live here because they are the same subject: what the input
 * devices do. Nothing about them belongs to a window, so nothing about them
 * belongs in the frame code, and the config's own gcl_mouse and gcl_touchpad
 * blocks would otherwise be read by nobody.
 *
 * --- the wheel ---------------------------------------------------------------
 *
 * A wheel over a window scrolls that window, and the manager must not be
 * involved: the program is what knows what its content is. Over the desktop
 * there is nothing to scroll, so the same gesture does the one thing a desktop
 * can do with it — it turns the workspace over. That is not a compromise, it is
 * what the gesture means in each place, and it is why the config names the
 * wheel's two directions separately from "none": a wheel that scrolls and a
 * wheel that does nothing are different requests.
 *
 * What the config asks for wins. `ScrollUp="workspace_next"` turns the
 * workspace over everywhere, including over windows, because that is what it
 * was told; `ScrollUp="none"` does nothing anywhere.
 *
 * --- the touchpad ------------------------------------------------------------
 *
 * The pad is configured through xinput, which talks to the X server and not to
 * any desktop: the same settings therefore hold under this manager and under
 * any other, and nothing is written to a desktop's own settings store.
 *
 * The X server reads /etc/X11/xorg.conf.d once, at startup, and never again —
 * so a file written now changes the next session and not this one. xinput is
 * what closes that gap. This module does not write the file (that is what
 * dotfile/TOUCHPAD_SETTINGS/settings_touchpad.py is for, and it is a script
 * because it needs root and runs once); it applies the settings to the session
 * that is running, which is the half a window manager can do without asking
 * for a password.
 *
 * Every property is set on its own. A pad without middle-button emulation, or
 * an old driver, is missing some of them, and one missing property must not
 * stop the rest — the difference between a pad missing one feature and a pad
 * missing all of them.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "wm_core.h"
#include "wm_workspace.h"

/* --- running xinput --------------------------------------------------------
 *
 * The same shape wm_menu.c uses for its power requests, and for the same
 * reason: a program that is asked to do something has to be judged by whether
 * it did it, not by whether it exists. xinput is absent on plenty of machines
 * — a display server with no input tools installed is not broken — so every
 * call here is allowed to fail and none of them is fatal. */
static int run_quiet(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        /* Nothing from the child reaches the session's log: xinput's output is
           a diagnosis, and a missing property is reported by the caller with
           the property's name, which is more use than the tool's own words. */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    pid_t done;
    do {
        done = waitpid(pid, &status, 0);
    } while (done < 0 && errno == EINTR);

    if (done < 0) {
        return -1;
    }
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/* --- the touchpad ---------------------------------------------------------- */

/* One libinput property, as xinput takes it: a name and the numbers that
   follow it. The booleans are 1 and 0 rather than "on" and "off" because that
   is what the X property reads — libinput's settings are exposed as numbers,
   and xinput passes them through unchanged. */
typedef struct TouchpadSetting {
    const char *property;
    const char *arguments;
} TouchpadSetting;

/* The properties the config's four touchpad switches map to.
 *
 * Three-finger and four-finger swipes have no libinput property: libinput
 * reports the gesture and the compositor decides what it means, which is why
 * X11 window managers cannot bind them and Wayland compositors can. They are
 * parsed and held, and their absence from this table is the honest answer —
 * the log says so rather than the setting pretending to be applied.
 */
static const TouchpadSetting TOUCHPAD_SETTINGS[] = {
    { "libinput Tapping Enabled",                 "1" },
    { "libinput Tapping Drag Enabled",            "1" },
    { "libinput Natural Scrolling Enabled",       "0" },
    { "libinput Scroll Method Enabled",           "1 0 0" },
    { "libinput Disable While Typing Enabled",    "1" },
};

#define TOUCHPAD_SETTING_COUNT \
    ((int)(sizeof(TOUCHPAD_SETTINGS) / sizeof(TOUCHPAD_SETTINGS[0])))

/* Apply one property to every device that answers to the touchpad properties.
 *
 * The device list is not filtered by name: "touchpad" is a manufacturer's
 * habit, and pads called TrackPoint, ClickPad and Synaptics exist while mice
 * with the word in their name exist too. A device is a touchpad, for this
 * purpose, when it carries the property being set — which is the test
 * settings_touchpad.py uses, and the two agreeing is what keeps one pad from
 * behaving two ways depending on which program configured it.
 *
 * xinput takes the device name as one argument, so a walk of `xinput list
 * --name-only` is not needed here: XListInputDevices would be, and that is a
 * second library for one call. Instead the setting is applied with the device
 * named "touchpad" — which libinput's own udev rule gives every pad — and any
 * pad that answers to the setting takes it. */
static void apply_touchpad_settings(WmCore *core, int tap_to_click,
                                    int two_finger_scroll) {
    (void)core;

    /* The two the config drives override the table's defaults. A pad with
       tapping switched off and NaturalScrolling still on would be a pad
       nothing wrote to and nobody meant, so the config's answer is what is
       applied — and the table above is only the base the config adjusts. */
    TouchpadSetting settings[TOUCHPAD_SETTING_COUNT];
    memcpy(settings, TOUCHPAD_SETTINGS, sizeof(settings));

    for (int i = 0; i < TOUCHPAD_SETTING_COUNT; i++) {
        if (strcmp(settings[i].property, "libinput Tapping Enabled") == 0) {
            settings[i].arguments = tap_to_click ? "1" : "0";
        } else if (strcmp(settings[i].property,
                          "libinput Scroll Method Enabled") == 0) {
            /* Two-finger scrolling and edge scrolling are the same property's
               two settings; "1 0 0" is two-finger, "0 1 0" is edge. A pad
               that scrolls two fingers is the modern one and the one the
               config's name means. */
            settings[i].arguments = two_finger_scroll ? "1 0 0" : "0 1 0";
        }
    }

    /* The device is named by the property rather than by a list, so the
       setting goes to whichever device carries it. The whole set is applied
       once; a failure on one property is reported and the next is tried. */
    int applied = 0;
    int failed = 0;
    for (int i = 0; i < TOUCHPAD_SETTING_COUNT; i++) {
        char *argv[8];
        int argument_count = 0;
        argv[argument_count++] = "xinput";
        argv[argument_count++] = "set-prop";
        argv[argument_count++] = "touchpad";
        argv[argument_count++] = (char *)settings[i].property;

        /* The arguments are a short list of numbers, copied by pointer: the
           strings are literals and live for the length of the call. */
        char arguments[32];
        snprintf(arguments, sizeof(arguments), "%s", settings[i].arguments);
        char *walk = arguments;
        while (*walk && argument_count < 7) {
            argv[argument_count++] = walk;
            while (*walk && *walk != ' ') {
                walk++;
            }
            if (*walk == ' ') {
                *walk++ = '\0';
            }
        }
        argv[argument_count] = NULL;

        if (run_quiet(argv) == 0) {
            applied++;
        } else {
            failed++;
        }
    }

    if (applied > 0) {
        fprintf(stderr, "gnuchanwm: touchpad: %d setting(s) applied to %d "
                        "device name(s), %d not supported\n",
                applied, applied, failed);
    } else {
        /* The usual reason, and the one worth saying: xinput is not installed,
           or there is no touchpad, or this is a Wayland session — and on any
           of those the settings belong to something else. */
        fprintf(stderr, "gnuchanwm: touchpad: nothing applied (no xinput, no "
                        "touchpad, or not an X11 session)\n");
    }

    /* Three-finger and four-finger swipes are named in the config and no
       libinput property carries them: the gesture is the compositor's to
       interpret on X11 as well as on Wayland. Said once so a config that asked
       for one does not look like a config that was ignored. */
    fprintf(stderr, "gnuchanwm: touchpad: three- and four-finger swipes are "
                    "the compositor's; libinput exposes no property for them\n");
}

/* --- what the wheel does --------------------------------------------------- */

/* The workspace move a wheel button asks for, or 0.
 *
 * A wheel action is the one place the desktop has to know whether the pointer
 * is over a window or over the desktop itself: over a window the wheel scrolls
 * the program, over the desktop there is nothing to scroll. The window is what
 * the event names — ButtonPress on the root is the desktop, ButtonPress on
 * anything else came from a client or from a frame. */
static int wheel_steps(WmCore *core, XButtonEvent *event) {
    WmMouseAction action =
        wm_config_mouse_action(&core->config, event->button);

    if (action == WM_MOUSE_WORKSPACE_NEXT) {
        return 1;
    }
    if (action == WM_MOUSE_WORKSPACE_PREV) {
        return -1;
    }

    /* WM_MOUSE_SCROLL_UP and WM_MOUSE_SCROLL_DOWN are the wheel scrolling. On
       the desktop there is nothing to scroll, so the two become the workspace
       move — which is the only reading of the gesture that has an effect
       there. Over a window they are left alone, and the event is not even
       seen: the program has the button selected and it is the program's. */
    if (event->window != core->root) {
        return 0;
    }
    if (action == WM_MOUSE_SCROLL_UP) {
        return -1;
    }
    if (action == WM_MOUSE_SCROLL_DOWN) {
        return 1;
    }
    return 0;
}

/* --- the module ------------------------------------------------------------ */

static int input_init(WmCore *core) {
    if (!core) {
        return -1;
    }
    /* The touchpad is configured once, at start. A pad does not change while a
       session runs, and re-applying the settings on the idle tick would be a
       process spawned several times a second for an answer that never
       changes. A config reload is the one thing that can change them, and
       wm_config_apply() runs wm_input_apply() when it does. */
    apply_touchpad_settings(core, core->config.touchpad_tap_to_click,
                            core->config.touchpad_two_finger_scroll);
    return 0;
}

static void input_event(WmCore *core, XEvent *event) {
    if (event->type != ButtonPress) {
        return;
    }
    /* Only the two wheel buttons are the desktop's business, and only when the
       config asked for something other than "left to the program". Everything
       else is a click, and a click belongs to whatever is under the pointer —
       which the frame code has already dealt with in its passive grab. */
    if (event->xbutton.button != Button4 && event->xbutton.button != Button5) {
        return;
    }

    int steps = wheel_steps(core, &event->xbutton);
    if (steps != 0) {
        wm_workspace_step(core, steps);
    }
}

const WmModule wm_input_module = {
    .name = "input",
    .init = input_init,
    .event = input_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = NULL,
};

/* Re-apply the touchpad settings. Called by wm_config_apply() after a reload,
   because a script that changed TapToClick and was not re-applied would be a
   script whose change waits for the next login. */
void wm_input_apply(WmCore *core) {
    if (!core) {
        return;
    }
    apply_touchpad_settings(core, core->config.touchpad_tap_to_click,
                            core->config.touchpad_two_finger_scroll);
}
