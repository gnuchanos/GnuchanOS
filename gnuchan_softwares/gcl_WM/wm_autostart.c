/*
 * wm_autostart.c — the terminal that is open when the session starts.
 *
 * A window manager draws nothing by itself: it paints the desktop and then
 * waits for a program to open a window. A session that started no program is
 * therefore a themed, empty screen with no way to do anything on it, which
 * from the user's side is the same as a session that failed to start. The
 * first terminal is what makes the difference — it is the way to reach every
 * other program, and on a machine whose whole desktop is a window manager it
 * is also the only starting point there is.
 *
 * This is deliberately its own module rather than a line in the entry point:
 * what starts with a session is a policy, and a policy is the kind of thing
 * that changes. A machine that wants an editor, a browser or a panel opens one
 * by adding a file beside this one and a line to the module list, and touching
 * nothing else.
 *
 * It runs at init, before the event loop reads anything. That is safe because
 * spawning only forks: the child connects to the X server on its own, and the
 * window it opens reaches this manager later, as a MapRequest, through the
 * loop that is by then running.
 */
#include <stdio.h>

#include "wm_config.h"
#include "wm_core.h"
#include "wm_spawn.h"

/* Start every program the settings script named through gcl_autostart.all.
   Each entry is a command line, so it goes through the same tokeniser a
   RunProgram action does — a name with arguments in it is read the way it was
   written. A command that cannot be found fails in its own child and is logged
   there; it does not stop the ones after it, because a missing dock must not
   cost a session its notification server. */
static void start_autostart(const WmCore *core) {
    if (!core) {
        return;
    }
    for (int i = 0; i < core->config.autostart_count; i++) {
        const char *command = core->config.autostart[i];
        if (!command[0]) {
            continue;
        }
        if (wm_spawn_command(command) != 0) {
            fprintf(stderr,
                    "gnuchanwm: autostart: could not start '%s'\n", command);
        } else {
            fprintf(stderr, "gnuchanwm: autostart: started '%s'\n", command);
        }
    }
}

static int autostart_init(WmCore *core) {
    if (wm_spawn_terminal() != 0) {
        /* Not fatal. A machine with no terminal installed, or one whose
           $TERMINAL points at something missing, still has a working window
           manager and still has Alt+Enter — which will report the same thing
           when it is pressed. Failing start-up over it would take the display
           away from a user who could have fixed it from a terminal. */
        fprintf(stderr,
                "gnuchanwm: no terminal to open at start-up; "
                "install one or set $TERMINAL\n");
        /* Shown as well as logged: an empty desktop with nothing on it is
           exactly what a missing terminal looks like, and the log is not
           somewhere a running session can be read. */
        wm_config_show_message(core,
                               "GnuChanWM: no terminal was found",
                               "install one (xterm, alacritty) or set $TERMINAL",
                               "");
    }

    /* Then whatever the script named: the session's daemons, the notification
       server above all, so it is running before the first program that might
       notify does. */
    start_autostart(core);
    return 0;
}

const WmModule wm_autostart_module = {
    .name = "autostart",
    .init = autostart_init,
    .event = NULL,
    .cleanup = NULL,
};
