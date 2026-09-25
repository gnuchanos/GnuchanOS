/*
 * GnuChanWM.c — the entry point.
 *
 * This is the only file that decides which modules make up the WM. The core
 * knows nothing about window management, focus or keys; it knows how to run a
 * list of modules. So the whole WM, as a feature set, is this list — and
 * adding a feature is adding a line here and a file beside it.
 *
 *     GnuChanWM            start (as a session's window manager)
 *     GnuChanWM --version  print the version and exit
 *
 * Everything the WM prints goes to a log file as well as to stderr, because a
 * window manager started from a display manager has no terminal to print to:
 * when something goes wrong the log is the only record, and the log is what a
 * terminal is opened to show.
 */
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_spawn.h"

#define GNUCHANWM_VERSION "0.1.0"

/* Where the WM writes what it has to say. /tmp is used rather than the home
   directory because it is the one place always writable, whatever user the
   session runs as, and it survives only until the next boot — which is the
   right lifetime for a start-up log. */
static const char *LOG_PATH = "/tmp/gnuchanwm.log";

/* Set by the signal handler and read by the loop. Only a flag of this exact
   type may be touched from a handler. */
static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signum) {
    (void)signum;
    stop_requested = 1;
}

/* The modules, in the order they start. Order matters where one module reads
   state another sets up; key grabs come last so a key that cannot be bound
   cannot stop the window manager itself from running. */
static void register_modules(WmCore *core) {
    /* Order matters where a module reads state another sets up: the desktop
       paints the background first, windows are managed on top of it, focus
       names the current one, and keys come last so a key that cannot be bound
       cannot stop the window manager itself from running. */
    wm_register(core, &wm_desktop_module);
    wm_register(core, &wm_manage_module);
    wm_register(core, &wm_frame_module);
    wm_register(core, &wm_focus_module);
    wm_register(core, &wm_keys_module);
    /* Last: it opens the first terminal, and by then the keys are already
       grabbed and the desktop is already painted, so the window it opens is
       managed by a session that is completely up rather than one still
       arranging itself. */
    wm_register(core, &wm_autostart_module);
}

/* Send everything the WM prints to the log as well as to stderr. Called
   before anything can print, so no message is ever missing from the log. */
static void open_log(void) {
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    dup2(fd, STDERR_FILENO);
    close(fd);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

/* When start-up fails after the display is open — a module could not run —
   the log is opened in a terminal so the failure is seen instead of leaving
   a bare screen. If no terminal can be opened, the messages have already
   gone to stderr and the log file, which is what a display manager keeps. */
static void show_failure(void) {
    fprintf(stderr, "gnuchanwm: start-up failed; opening a terminal with the log.\n");
    fflush(stderr);
    if (wm_spawn_terminal_displaying(LOG_PATH) != 0) {
        fprintf(stderr, "gnuchanwm: see %s for what went wrong.\n", LOG_PATH);
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanWM %s\n", GNUCHANWM_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanWM [--version] [--help]\n"
                   "\n"
                   "Starts the GnuchanOS window manager. Alt+Enter opens the\n"
                   "default terminal. It is meant to be started by a display\n"
                   "manager session, not from inside another session.\n");
            return 0;
        }
    }

    open_log();

    /* The first line, before anything can fail: it is what tells a log that
       the binary was started at all from a log that shows how far it got. */
    fprintf(stderr,
            "gnuchanwm: started (pid %ld, DISPLAY=%s)\n",
            (long)getpid(),
            getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    /* SA_RESTART is deliberately left off: the signal has to interrupt the
       blocking XNextEvent so the loop can see the flag. */
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    WmCore core;

    /* Claim the display first: a module's init needs the connection and the
       atom table that this opens, so it cannot be registered onto a core that
       does not have them yet. */
    if (wm_core_init(&core) != 0) {
        /* No display: a terminal cannot be opened either, because there is no
           X server to open it on. The message has gone to the log. */
        fprintf(stderr, "gnuchanwm: cannot start: no usable X display.\n");
        return 1;
    }

    register_modules(&core);

    if (wm_core_start(&core) != 0) {
        show_failure();
        wm_core_shutdown(&core);
        return 1;
    }

    /* The loop stops on a signal by clearing the flag it checks, so the core
       is given one last chance to close the display cleanly. */
    while (core.running && !stop_requested) {
        wm_core_step(&core);
    }
    core.running = 0;

    wm_core_shutdown(&core);
    return 0;
}
