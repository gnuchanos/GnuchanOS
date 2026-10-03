/*
 * wm_idle.c — the desk that goes quiet.
 *
 * A black screen that will not come back is the one fault a session can have
 * that its own user cannot diagnose: the machine looks off, and the only thing
 * that would have said otherwise — a screen saver, a lock screen, anything at
 * all on the panel — never appears. That is what this module exists to stop.
 *
 * --- what was wrong ---
 *
 * The X server keeps two timers of its own, and GnuChanWM used to leave both of
 * them running. One is screen blanking: after a while with no input the server
 * paints the screen black by itself. The other is DPMS: after a longer while it
 * tells the monitor to power down. Neither is a screen saver and neither can be
 * woken by a program — they simply turn the panel off, and on a machine whose
 * session did nothing about it that is exactly the "the laptop goes black after
 * a while" a person reports. The session's OWN screen saver (GnuChanSS) was
 * never started at idle at all: nothing watched the idle clock.
 *
 * --- what this does ---
 *
 * At start-up it turns both of the server's timers OFF (`xset s off`, then
 * `xset -dpms`, as two commands — see idle_disable_server_timers() for why they
 * cannot be one line), so the only thing that ever darkens this desk is this
 * module and the programs it runs. Then it watches the idle clock itself,
 * through the X ScreenSaver extension — the same measurement the server uses,
 * and the same one GnuChanSS reads on its own — and when the keyboard and
 * pointer have been still for Idle.Seconds it runs the session's screen saver,
 * and its lock screen too when the settings ask for it.
 *
 * --- why a tick and not an event ---
 *
 * Being idle is not an X event; nothing is sent when the desk simply stays
 * quiet. So this is a polled module, woken on the interval below, and it costs
 * one cheap extension query each time. The interval is a second: fine enough
 * that the saver comes up within a second of the configured moment, coarse
 * enough that a quiet session is not spinning.
 *
 * --- why xset, and not the extension called directly ---
 *
 * The screen-blanking timeout is set through XSetScreenSaver and DPMS through
 * the DPMS extension, and reaching either one directly would add a library to
 * the window manager for two calls made once. `xset` is the tool every X
 * session already has (x11-xserver-utils), and the lid module already draws its
 * DPMS through it for the same reason. A machine without xset is a machine
 * where the server keeps its own timers, which is the behaviour that was there
 * before this module — degraded, but not worse.
 *
 * --- what it does NOT do ---
 *
 * It does not blank the screen itself and it does not draw: the picture is
 * GnuChanSS's job and the password is GnuChanSL's. This module only decides
 * WHEN, and hands off to the programs the settings name. That is the same
 * split the lid module has, and the two share the command lines: a machine
 * that named its own screen saver for a wake-up gets the same one here.
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

#include "wm_core.h"
#include "wm_spawn.h"

/* The programs to run when the settings name none. They are the GnuchanOS
 * programs, and they are named with the flags that make them show at once:
 *
 *   GnuChanSS --once   shows immediately rather than waiting out ITS OWN idle
 *                      timer. The session has already waited; asking the saver
 *                      to wait the same length again would mean the desk sits
 *                      dark for one more IdleSeconds on top of the one just
 *                      spent — which is the black screen this is meant to
 *                      remove.
 *   GnuChanSL          the lock screen, which has no timer of its own.
 *
 * A machine that has neither simply runs nothing, which is the same as having
 * asked for nothing. */
#define WM_IDLE_DEFAULT_SCREENSAVER "GnuChanSS --once"
#define WM_IDLE_DEFAULT_LOCKSCREEN  "GnuChanSL"

/* Whether the extension is here at all. Probed once; a server without it (very
 * old, or a deliberately minimal one) cannot answer the idle question and the
 * module does nothing rather than guessing. */
static int s_have_scrnsaver = 0;

/* The XScreenSaverInfo the extension fills in. Allocated once and reused: the
 * question is asked once a second for the life of the session, and a malloc per
 * second is a malloc nobody needs. */
static XScreenSaverInfo *s_info = NULL;

/* Whether the saver/lock have already been run for THIS quiet spell, and the
 * idle reading that says the spell is over.
 *
 * It is the whole reason a latch is needed: once the saver is up, nothing the
 * saver does touches the keyboard, so the server's idle clock keeps counting
 * and the next tick would see the same "too idle" and start a second saver over
 * the first. `s_fired` is set when they are started and cleared the moment the
 * idle clock drops back below the threshold, which happens when the person
 * comes back and the saver ends. One quiet spell, one saver. */
static int s_fired = 0;

/* Turn the server's own blanking and DPMS off, once, at start-up.
 *
 * `xset s off` stops the screen-blanking timer; `xset -dpms` stops the monitor
 * power timer. They are run as TWO separate commands and not as the one line
 * `xset s off -dpms`, and that is not a style choice: `xset` refuses the WHOLE
 * line when any one of its options is unsupported, so on a server without the
 * DPMS extension the `-dpms` makes xset exit before it ever applies `s off` —
 * the blanking timer stays on and the screen still goes dark, which is exactly
 * the fault this module exists to remove. Measured on Xvfb: a single
 * `xset s off -dpms` left `timeout: 600` in place; the two commands run one
 * after the other leave the blanking genuinely off.
 *
 * `s off` is always tried first, because it is the one that matters — the
 * screen going blank is the complaint — and a machine without the DPMS
 * extension still gets it. Both are idempotent, so running them again after a
 * reload does no harm. A machine with no xset at all is reported once and ends
 * up with the server's own timers, exactly as before this module. */
static void idle_disable_server_timers(void) {
    if (wm_spawn_command("xset s off") != 0) {
        fprintf(stderr,
                "gnuchanwm: idle: xset was not found, so the server's own "
                "blanking stays on; the screen saver below still runs at the "
                "configured idle time, but the server may darken the panel "
                "first\n");
        /* Without xset there is no point trying the DPMS name either. */
        return;
    }
    /* The monitor power timer. A server without the DPMS extension answers
     * "server does not have extension for -dpms option" and does nothing,
     * which is harmless — and, crucially, cannot undo the `s off` above. */
    wm_spawn_command("xset -dpms");
}

/* Run a written command, or the named program when the command is empty. The
 * command is a LINE and not a program name, so it goes through the same
 * tokeniser a RunProgram action does — see wm_spawn_command(). */
static void idle_run(const char *command, const char *fallback) {
    if (command && command[0]) {
        wm_spawn_command(command);
    } else if (fallback && fallback[0]) {
        wm_spawn_command(fallback);
    }
}

/* How long the keyboard and pointer have been still, in seconds, or -1 when the
 * extension cannot answer. */
static double idle_seconds(Display *display) {
    if (!s_have_scrnsaver || !s_info) {
        return -1.0;
    }
    Window root = DefaultRootWindow(display);
    if (!XScreenSaverQueryInfo(display, root, s_info)) {
        return -1.0;
    }
    return (double)s_info->idle / 1000.0;
}

/* What to do when the desk has been quiet long enough: the settings' own
 * screen saver and/or lock screen, read fresh from the config so a reload
 * changes them with no restart. The order is the lock LAST — the same order the
 * lid uses on a close — because the lock screen takes the keyboard, and a
 * screen saver started after it would grab underneath a lock that is already
 * holding everything. With the lock up the saver is behind it and unseen, which
 * is what "lock" means. */
static void idle_fire(WmCore *core) {
    if (core->config.idle_screensaver) {
        idle_run(core->config.screensaver_command,
                 WM_IDLE_DEFAULT_SCREENSAVER);
    }
    if (core->config.idle_lockscreen) {
        idle_run(core->config.lockscreen_command, WM_IDLE_DEFAULT_LOCKSCREEN);
    }
}

/* The tick: read the idle clock and act when it has been quiet long enough. */
static void idle_tick(WmCore *core) {
    if (!core->config.idle_enabled) {
        /* Turned off in the settings. The server's own timers were still
         * disabled at start-up — that is the default and the whole point — but
         * this session runs no saver of its own. */
        return;
    }

    double idle = idle_seconds(core->display);
    if (idle < 0.0) {
        /* No extension to ask: nothing to do, and nothing to report either —
         * the start-up already said so once. */
        return;
    }

    int seconds = core->config.idle_seconds;
    if (seconds < 1) {
        seconds = 1;
    }

    if (idle >= (double)seconds) {
        if (!s_fired) {
            /* Silence the moment it is acted on, before the spawn, so a slow
             * spawn cannot be started twice by the tick that follows. */
            s_fired = 1;
            fprintf(stderr,
                    "gnuchanwm: idle: the desk has been quiet for %.0f "
                    "second(s); running the screen saver%s\n",
                    idle, core->config.idle_lockscreen ? " and lock" : "");
            idle_fire(core);
        }
        return;
    }

    /* The desk is in use again. Clearing the latch is what lets the NEXT quiet
     * spell start the saver again: the person came back, the saver ended on its
     * own when they did, and this tick is the first one to see the clock reset.
     */
    s_fired = 0;
}

static int idle_init(WmCore *core) {
    (void)core;

    /* Turn the server's own two timers off first, before this module ever
     * looks at the idle clock. It is the fix for the black screen, and it is
     * done whether or not the settings later ask for a saver of their own: a
     * machine whose settings say "no saver" still must not be blanked by the
     * server behind its back. */
    idle_disable_server_timers();

    /* Ask whether the extension is here, once. A server without it cannot
     * answer "how idle", and the module then leaves the clock alone. */
    int event_base = 0;
    int error_base = 0;
    Display *display = core->display;
    s_have_scrnsaver = XScreenSaverQueryExtension(display, &event_base,
                                                  &error_base);
    if (s_have_scrnsaver) {
        s_info = XScreenSaverAllocInfo();
        if (!s_info) {
            s_have_scrnsaver = 0;
        }
    }
    if (!s_have_scrnsaver) {
        fprintf(stderr,
                "gnuchanwm: idle: this X server has no ScreenSaver "
                "extension, so the idle clock cannot be read; the screen "
                "saver will not start on idle here\n");
    }
    s_fired = 0;
    return 0;
}

static void idle_cleanup(WmCore *core) {
    (void)core;
    if (s_info) {
        XFree(s_info);
        s_info = NULL;
    }
    s_have_scrnsaver = 0;
}

const WmModule wm_idle_module = {
    .name = "idle",
    .init = idle_init,
    .event = NULL,
    .tick = idle_tick,
    /* A second. The threshold is in whole seconds and a saver that comes up a
     * second late is not noticed; a shorter interval would wake the loop for
     * nothing. */
    .interval_ms = 1000,
    .cleanup = idle_cleanup,
};
