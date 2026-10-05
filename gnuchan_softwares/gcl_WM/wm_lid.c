/*
 * wm_lid.c — the laptop lid, sleep, and what a wake-up does.
 *
 * A laptop that ignores its own lid is a laptop that cooks in a bag. Closing
 * the lid has to put the machine to sleep; opening it has to bring it back,
 * and — when the settings ask for it — put a screen saver and/or a lock screen
 * in front of whoever opened it. That is the whole of this module.
 *
 * --- why it lives here and not in the display manager ---
 *
 * gcl_DM/dm_power.c says it itself, in its own words: "The lid is a session
 * concern and it is up to the session to handle it." The display manager runs
 * as a system service, before and above any session; the lid is a thing the
 * logged-in user's session acts on. So the handler is a GnuChanWM module, and
 * it is configured through the session's own settings script.
 *
 * --- why the lid is POLLED and not woken by an event ---
 *
 * An X window manager is event-driven, and a lid state is not an X event.
 * The kernel reports it through ACPI — /proc/acpi/button/lid/<name>/state —
 * which is a file, not a signal. The "proper" wake-up is a logind D-Bus
 * signal, but that would put a D-Bus client (and its library) inside the
 * window manager for one boolean. A file read every couple of seconds is
 * nothing, and it works the same on a machine with no systemd at all; so the
 * module asks the tick loop to wake it, reads the file, and acts on a change.
 *
 * --- what it does on a change ---
 *
 *   closed -> (if LockBeforeSuspend) run the lock screen, then blank the
 *             screen, then (if OnLidCloseSuspend) suspend the machine.
 *   open   -> un-blank the screen, then (if OnLidOpenScreenSaver) run the
 *             screen saver and (if OnLidOpenLockScreen) run the lock screen.
 *
 * Every one of those is a setting read out of core->config on the tick it is
 * needed, so a reload of the settings script changes the behaviour live, the
 * same way every other setting does. A machine with no lid — a desktop — finds
 * no state file on the first tick and then does nothing, silently: there is
 * nothing to report and a desktop is not a broken laptop.
 *
 * --- the programs ---
 *
 * The screen saver and the lock screen are named in the settings script
 * (gcl_Power.Lid): ScreensaverCommand and LockScreenCommand, both written
 * command lines, so a machine whose screen saver is not the shipped one names
 * its own. Empty means the module looks GnuChanSS / GnuChanSL up on PATH,
 * which is what GnuchanOS ships.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_spawn.h"

/* The names the two programs have when the settings script names none. They
   are the GnuchanOS programs; a machine that has neither simply gets nothing
   run on the wake-up, which is the same as having asked for nothing.
 *
 * The screen saver is started with --once: a wake-up is a moment that wants
 * the saver NOW, not after IdleSeconds more of a desk that is only idle
 * because the machine was asleep. Without the flag the saver came up, took its
 * D-Bus name and then waited out the idle timer — which on a resume is a black
 * screen for the length of that timer, which is exactly what a wake-up must
 * not be. */
#define WM_LID_DEFAULT_SCREENSAVER "GnuChanSS --once"
#define WM_LID_DEFAULT_LOCKSCREEN "GnuChanSL"

/* The path the lid's state is read from, filled in once. /proc/acpi/button/lid
   holds one directory per lid (LID, LID0, ...) and each holds a `state` file
   whose text is `state: open` or `state: closed`. The path is discovered on
   the first tick and kept: a lid is not a thing that appears and disappears
   under a running session, and re-scanning a directory every two seconds only
   to find the same file is work for nothing.
 *
 * The buffer is sized for the worst case the format below can produce and not
 * for the names any real firmware uses: the root is 18 characters, a dirent
 * name can be up to 255 (NAME_MAX), and the "/state" and the terminator add
 * seven more, which is 280. A 256-byte buffer held every name seen in practice
 * and still drew a truncation warning, because the compiler reads the format,
 * not the firmware. */
static char lid_state_path[512] = "";
static int  lid_path_searched = 0;

/* The last state seen, and whether one has been seen at all.
 *
 * `seen` is what stops the FIRST tick from looking like a change: a session
 * that starts with the lid already closed must not immediately suspend, and a
 * session that starts with it open must not immediately run the screen saver.
 * The state is recorded on the first tick and acted on only from the second. */
static int lid_last_closed = 0;
static int lid_seen = 0;

/* The clock readings of the last tick, so a wake-up from the machine's OWN
 * sleep can be told from a tick that merely arrived late.
 *
 * The lid is not the only thing that suspends a laptop. An idle timer, the
 * power button and the machine's own firmware all put it to sleep with the lid
 * untouched — and because the lid never moved, the open-lid path never ran, so
 * nothing turned the screen back on and the machine came back to a black
 * screen. The tick loop is frozen while the machine is asleep and resumes with
 * it, so a clock jumping much further than the tick interval is the signal,
 * available without D-Bus, that says "the machine slept and has just woken".
 *
 * It has to be TWO clocks, and that is the whole of a bug this module used to
 * have. The wall clock alone cannot tell a real suspend from a tick that was
 * simply late: this process is a window manager, and a slow frame, a fork that
 * took a while, or a busy machine moves no clock but does delay the tick. When
 * that delay passed the old threshold the tick was read as a wake-up, the
 * wake-up path ran `GnuChanSS --once` — which skips the idle check and the
 * inhibit check by design, because a wake-up wants the saver NOW — and the
 * screen was covered under a hand that was still moving the mouse. That is
 * exactly the fault reported: "the screen saver opens even though I am moving
 * the mouse".
 *
 * CLOCK_MONOTONIC does not advance while the machine is suspended; the wall
 * clock does. So a real sleep shows up as the wall clock having advanced far
 * more than the monotonic clock since the last tick, and a merely late tick
 * shows up as the two having advanced by the same amount. Comparing the two
 * tells them apart with no D-Bus and no libsystemd, which is why it is done
 * this way rather than by subscribing to a logind signal. */
static time_t lid_last_wall = 0;
static struct timespec lid_last_monotonic;

/* How far the wall clock may run ahead of the monotonic clock between two
 * ticks before the gap is called a wake-up rather than a late tick. A real
 * suspend is seconds to hours long; a starved event loop is well under this.
 * Four seconds against a two-second tick leaves room for one missed tick and
 * no more. */
#define WM_LID_RESUME_GAP_SECONDS 4

/* Find the lid's state file, once. Sets lid_state_path and returns 1 when one
   was found and 0 when there is no lid (a desktop), in which case the search is
   not repeated. */
static int lid_find_state(void) {
    if (lid_path_searched) {
        return lid_state_path[0] != '\0';
    }
    lid_path_searched = 1;

    const char *root = "/proc/acpi/button/lid";
    DIR *directory = opendir(root);
    if (!directory) {
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        snprintf(lid_state_path, sizeof(lid_state_path), "%s/%s/state",
                 root, entry->d_name);
        if (access(lid_state_path, R_OK) == 0) {
            closedir(directory);
            return 1;
        }
    }
    closedir(directory);
    lid_state_path[0] = '\0';
    return 0;
}

/* Read the lid's state. Returns 1 with *closed set when the file was read, and
   0 when it could not be (the read is not an error worth reporting — the lid
   may have no state on some firmware, and a session must not fail over it). */
static int lid_read(int *closed) {
    if (!lid_find_state()) {
        return 0;
    }
    FILE *file = fopen(lid_state_path, "r");
    if (!file) {
        return 0;
    }
    char text[64];
    if (!fgets(text, sizeof(text), file)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    /* `state: closed` / `state: open`. The word after the colon is what
       matters; anything that is not "open" is taken as closed, which is the
       safe reading — a lid we cannot call open is one to sleep under. */
    const char *colon = strchr(text, ':');
    const char *value = colon ? colon + 1 : text;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    *closed = strncmp(value, "open", 4) != 0;
    return 1;
}

/* Blank or un-blank the screen through the server's DPMS.
 *
 * `xset` is the tool every X session has (x11-xserver-utils), and it is used
 * rather than the DPMS extension directly so this module needs no library the
 * window manager does not already link. A machine without xset simply does not
 * blink: the suspend on the closed lid turns the panel off on its own, and the
 * resume turns it back on, so the screen ends up dark and lit either way. */
static void lid_screen(int on) {
    wm_spawn_command(on ? "xset dpms force on" : "xset dpms force off");
}

/* Run a written command, or the named program when the command is empty. The
   command is a LINE and not a program name, so it goes through the same
   tokeniser a RunProgram action does. */
static void lid_run(const char *command, const char *fallback) {
    if (command && command[0]) {
        wm_spawn_command(command);
    } else if (fallback && fallback[0]) {
        wm_spawn_command(fallback);
    }
}

/* Put the machine to sleep.
 *
 * systemctl is the answer on anything running systemd, which is every
 * GnuchanOS install; the direct write to /sys/power/state is the fallback for
 * a machine without it. The write is done here rather than through a spawn
 * because there is no program to run for it — it is one line into a kernel
 * file. Nothing is reported when neither works: the person whose laptop did
 * not sleep already knows. */
static void lid_suspend(void) {
    if (access("/usr/bin/systemctl", X_OK) == 0 ||
        access("/bin/systemctl", X_OK) == 0) {
        wm_spawn_command("systemctl suspend");
        return;
    }
    FILE *file = fopen("/sys/power/state", "w");
    if (file) {
        fputs("mem", file);
        fclose(file);
    }
}

/* What the settings say, read fresh on the tick they are used — so a reload
   changes the behaviour with no restart. */
static void lid_on_close(WmCore *core) {
    if (core->config.lock_before_suspend) {
        lid_run(core->config.lockscreen_command, WM_LID_DEFAULT_LOCKSCREEN);
    }
    if (core->config.lid_suspend_on_close) {
        lid_screen(0);
        lid_suspend();
    }
}

/* What a wake-up does, whatever woke the machine: turn the screen back on, and
 * then put the screen saver and/or the lock screen up if the settings ask for
 * them. A lid that was opened and a machine that woke from its own sleep are
 * the same event from the user's side — the desk is back and wants covering —
 * so both go through here. */
static void lid_on_wake(WmCore *core) {
    lid_screen(1);
    if (core->config.lid_open_screensaver) {
        lid_run(core->config.screensaver_command, WM_LID_DEFAULT_SCREENSAVER);
    }
    if (core->config.lid_open_lockscreen) {
        lid_run(core->config.lockscreen_command, WM_LID_DEFAULT_LOCKSCREEN);
    }
}

/* The tick: read the lid, and act when it changed — or when the machine woke. */
static void lid_tick(WmCore *core) {
    /* Whether this tick came after a sleep. Read before anything can return,
       because the clocks are advanced on every tick and a wake-up has to be
       recognised on the very tick it is seen.
     *
     * The test is the wall clock having advanced far more than the monotonic
     * clock since the last tick — see the note on lid_last_wall. The wall clock
     * advancing on its own, by the same amount as the monotonic clock, is a
     * late tick and not a wake-up; only a real suspend leaves the monotonic
     * clock behind. */
    time_t now_wall = time(NULL);
    struct timespec now_monotonic;
    clock_gettime(CLOCK_MONOTONIC, &now_monotonic);
    int resumed = 0;
    if (lid_last_wall != 0) {
        long long wall_gap = (long long)(now_wall - lid_last_wall);
        long long monotonic_gap =
            (long long)(now_monotonic.tv_sec - lid_last_monotonic.tv_sec);
        resumed = (wall_gap - monotonic_gap) > WM_LID_RESUME_GAP_SECONDS;
    }
    lid_last_wall = now_wall;
    lid_last_monotonic = now_monotonic;

    int closed = 0;
    int have_lid = lid_read(&closed);

    if (have_lid) {
        if (!lid_seen) {
            /* The first look only records where the lid is, so a session that
               starts already closed does not immediately suspend. */
            lid_seen = 1;
            lid_last_closed = closed;
            return;
        }
        if (closed != lid_last_closed) {
            /* The lid moved: it was opened or closed while the session ran. */
            lid_last_closed = closed;
            if (closed) {
                lid_on_close(core);
            } else {
                lid_on_wake(core);
            }
            return;
        }
        /* The lid did not move, but the machine may still have been asleep and
           woken — an idle suspend, the power button, the machine's own timer.
           With the lid open the desk is back and is covered exactly as an
           opened lid would cover it; with the lid shut there is nobody to show
           anything to, so nothing is done. */
        if (resumed && !closed) {
            lid_on_wake(core);
        }
        return;
    }

    /* No lid at all (a desktop), or a lid whose state will not read. The
       wake-up handling is still wanted — a desktop sleeps on its idle timer
       too, and it comes back to the same black screen the lid path was fixed
       for. Nothing here depends on the lid, only on the clock. */
    if (resumed) {
        lid_on_wake(core);
    }
}

static int lid_init(WmCore *core) {
    (void)core;
    /* Nothing runs here: the lid state is read on the first tick, once the
       loop is turning, so a machine with no lid never even opens a file. */
    return 0;
}

const WmModule wm_lid_module = {
    .name = "lid",
    .init = lid_init,
    .event = NULL,
    .tick = lid_tick,
    /* Two seconds. A lid is opened and closed by hand, and hand motions are not
       faster than that; a shorter interval would be waking the loop to read the
       same value. */
    .interval_ms = 2000,
    .cleanup = NULL,
};
