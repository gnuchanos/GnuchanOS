/*
 * GnuChanDM.c — the entry point.
 *
 * This is the only file that decides which modules make up the greeter and what
 * happens when the user asks for something. The core knows nothing about
 * logging in; it runs a list of modules and hands them events. So the whole of
 * "what a login screen does" is: two modules, and a loop that acts on what they
 * asked for.
 *
 *     GnuChanDM            start the greeter
 *     GnuChanDM --version  print the version and exit
 *
 * It has to be started as root: checking a password and becoming a user are
 * root's work.
 */
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dm_core.h"

#define GNUCHANDM_VERSION "0.1.0"

static const char *LOG_PATH = "/tmp/gnuchandm.log";

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signum) {
    (void)signum;
    stop_requested = 1;
}

/* Login first: it lays the screen out, and the input module's hit-testing reads
   the rectangles that layout put down. */
static void register_modules(DmCore *core) {
    dm_register(core, &dm_login_module);
    dm_register(core, &dm_input_module);
}

static void open_log(void) {
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    dup2(fd, STDERR_FILENO);
    close(fd);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

/* Check the password, then — only then — start a session. */
static void attempt_login(DmCore *core) {
    if (dm_auth_check(core->username, core->password)) {
        dm_form_clear_password(core);
        if (dm_session_start(core, core->username) != 0) {
            dm_form_set_message(core, "could not start the session", 1);
        }
        /* Reached when the session has ended — the user logged out. The user
           name is kept, so the keyboard goes straight to the password. */
        core->focus = DM_FOCUS_PASSWORD;
        dm_form_set_message(core, NULL, 0);
        dm_core_redraw(core);
        return;
    }

    dm_form_clear_password(core);
    dm_form_set_message(core, "wrong user name or password", 1);
    core->focus = DM_FOCUS_PASSWORD;
}

/* Act on what the input module asked for, at the top of the loop — never inside
   the event that set it. */
static void act(DmCore *core) {
    DmAction action = core->action;
    core->action = DM_ACTION_NONE;

    switch (action) {
    case DM_ACTION_LOGIN:    attempt_login(core); break;
    case DM_ACTION_REBOOT:   dm_power_reboot();   break;
    case DM_ACTION_SHUTDOWN: dm_power_shutdown(); break;
    case DM_ACTION_NONE:                          break;
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanDM %s\n", GNUCHANDM_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanDM [--version] [--help]\n"
                   "\n"
                   "Starts the GnuchanOS login screen. It must run as root.\n");
            return 0;
        }
    }

    open_log();
    fprintf(stderr, "gnuchandm: started (pid %ld, DISPLAY=%s)\n",
            (long)getpid(), getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");

    if (geteuid() != 0) {
        fprintf(stderr, "gnuchandm: must run as root\n");
        return 1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    DmCore core;
    if (dm_core_init(&core) != 0) {
        fprintf(stderr, "gnuchandm: cannot start: no usable X display\n");
        return 1;
    }

    register_modules(&core);

    if (dm_core_start(&core) != 0) {
        dm_core_shutdown(&core);
        return 1;
    }

    fprintf(stderr, "gnuchandm: ready\n");

    while (core.running && !stop_requested) {
        dm_core_step(&core);
        if (core.action != DM_ACTION_NONE) act(&core);
    }

    dm_core_shutdown(&core);
    return 0;
}
