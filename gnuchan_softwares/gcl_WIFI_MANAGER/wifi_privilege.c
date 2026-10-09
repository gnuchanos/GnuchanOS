/*
 * wifi_privilege.c — re-running through sudo, once.
 *
 * See wifi_privilege.h for why. What is here is the one check — "am I root?" —
 * and the one action — "if not, run sudo and never come back" — plus the guard
 * that stops the second copy from doing it again.
 *
 * The guard is an environment variable, GNUCHANWIFI_ELEVATED, set to "1" before
 * the sudo. The copy sudo starts inherits it, sees it, and does not try to
 * elevate again; without it, a machine where sudo somehow gives back a non-root
 * process would loop forever. It is the same guard the installers use.
 *
 * The environment is passed through by NAME rather than by value, and that is
 * not a style choice: sudo resets most of the environment for safety, and only
 * a variable sudo is told to keep survives. DISPLAY, XAUTHORITY and HOME are the
 * three that matter — the first two so the root copy can still open a window on
 * the user's X server, the third so it reads the config out of the user's home
 * and not root's, which is empty.
 */
#define _POSIX_C_SOURCE 200809L

#include "wifi_privilege.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef GNUCHANWIFI_ELEVATED_VARIABLE
#define GNUCHANWIFI_ELEVATED_VARIABLE "GNUCHANWIFI_ELEVATED"
#endif

static int is_root(void) {
    /* geteuid is the id that decides what the kernel will let this process do,
       which is the question being asked — not getuid, which is who logged in. */
    return geteuid() == 0;
}

int wifi_privilege_ensure(int argc, char **argv) {
    if (is_root()) {
        return 0;
    }

    /* The second copy sudo started carries this and must not try again. */
    if (getenv(GNUCHANWIFI_ELEVATED_VARIABLE) != NULL) {
        return 0;
    }

    /* The program's own path, so the sudo runs the same binary. argv[0] is
       used rather than a read of /proc/self/exe because it is what the caller
       already has and is right for every way this program is started. */
    const char *self = (argc > 0 && argv[0]) ? argv[0] : "GnuChanWifi";

    /* Build the sudo command line: sudo -E keeps the environment, and the
       three named variables are passed explicitly on top of that, because -E
       alone is not honoured on every sudo configuration. */
    char command[4096];
    int at = snprintf(command, sizeof(command), "sudo -E env %s=1",
                      GNUCHANWIFI_ELEVATED_VARIABLE);

    const char *keep[] = { "DISPLAY", "XAUTHORITY", "HOME", NULL };
    for (int i = 0; keep[i]; i++) {
        const char *value = getenv(keep[i]);
        if (value && value[0]) {
            at = snprintf(command + at, sizeof(command) - (size_t)at,
                          " %s=%s", keep[i], value);
        }
    }

    at += snprintf(command + at, sizeof(command) - (size_t)at,
                   " %s", self);

    /* The program's own arguments follow, so a --config or a --list survives
       the re-run just as the shell's quoting lets it. They are passed raw, as
       they were given; the shell cases this program accepts are the ones with
       no spaces, and a path with a space would need the caller to quote it,
       which is how it reached argv in the first place. */
    for (int i = 1; i < argc; i++) {
        at += snprintf(command + at, sizeof(command) - (size_t)at,
                       " %s", argv[i]);
    }

    fprintf(stderr,
            "gnuchanwifi: this manages the radio and the driver, so it needs "
            "root; asking for the password\n");

    /* system() runs the shell, the shell runs sudo, sudo prompts on the
       terminal it was started from and replaces the process with the root
       copy. When it returns here the window is already gone, so this only ever
       returns to exit. */
    int status = system(command);
    if (status == -1) {
        fprintf(stderr,
                "gnuchanwifi: sudo could not be run; the window will open, "
                "but the radio and the driver cannot be changed\n");
        return -1;
    }

    /* The sudo'd copy has finished (its window closed) or was refused. Either
       way this process is done: it was only ever a launcher for the root one. */
    exit(WIFEXITED(status) ? WEXITSTATUS(status) : 0);
}
