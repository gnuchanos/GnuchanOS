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
 * Debian-focused on purpose: the session entry, the install and the
 * dependencies are all Debian's, and nothing here pretends otherwise.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "wm_core.h"

#define GNUCHANWM_VERSION "0.1.0"

/* Set by the signal handler and read by the loop. Only a flag of this exact
   type may be touched from a handler: anything else — a Window, a struct, a
   malloc — is undefined behaviour there, because the handler runs between
   two instructions of whatever the program was doing. */
static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signum) {
    (void)signum;
    stop_requested = 1;
}

/* The modules, in the order they initialise. Order matters where one module
   reads state another sets up: focus is initialised before manage, because
   taking over an existing window asks the focus module to move the focus.
   Key grabs come last so that a key that cannot be bound — another client
   already owns it — cannot stop the window manager itself from running. */
static void register_modules(WmCore *core) {
    wm_register(core, &wm_focus_module);
    wm_register(core, &wm_manage_module);
    wm_register(core, &wm_keys_module);
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

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    /* SA_RESTART is deliberately left off: the signal has to interrupt the
       blocking XNextEvent so the loop can see the flag. */
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    WmCore core;
    register_modules(&core);

    if (wm_core_init(&core) != 0) {
        /* wm_core_init has already said why. A non-zero exit is what a
           display manager reads to fall back to another session. */
        return 1;
    }

    /* The loop stops on a signal by clearing the flag it checks, so the core
       is given one last chance to close the display cleanly rather than the
       process dying with the connection open. */
    while (core.running && !stop_requested) {
        wm_core_step(&core);
    }
    core.running = 0;

    wm_core_shutdown(&core);
    return 0;
}
