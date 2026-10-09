/*
 * wifi_privilege.c — re-running through sudo, once.
 *
 * See wifi_privilege.h for why. What is here is the one check — "am I root?" —
 * and the one action — "if not, run sudo and do not come back" — plus the guard
 * that stops the second copy from doing it again.
 *
 * --- why execvp and not system() ---
 *
 * The first cut of this built one long string — "sudo -E env NAME=value ..." —
 * and handed it to system(), which runs it through /bin/sh. That is wrong on
 * three counts, and the third is what a person running it saw:
 *
 *   1. A shell re-splits the string, so a value with a space or a quote in it
 *      (a HOME under a directory with a space, a display name) becomes several
 *      arguments and the command means something else.
 *   2. The program's own path was whatever argv[0] held, which for a program
 *      started from the window manager is a bare name; `env` then had to find
 *      it on PATH, and the PATH under sudo is not the one it was launched with.
 *   3. There was no check that the assembled string fit its buffer, so on the
 *      edge it could be cut and the tail — variable names and all — read as
 *      arguments to `env`, which then reported the fragment as a missing file.
 *
 * So the command is now built as an ARRAY of arguments and passed to execvp,
 * with no shell in the way: nothing is re-split, every value is one argument
 * exactly as getenv() gave it, and there is no string to overrun. The program's
 * own path is read from /proc/self/exe, so `env` is handed an absolute path and
 * never has to search for it.
 *
 * The guard is still the environment variable GNUCHANWIFI_ELEVATED: the copy
 * sudo starts carries it, sees it, and does not try to elevate again, so a
 * machine where sudo somehow returns a non-root process does not loop.
 */
#define _POSIX_C_SOURCE 200809L

#include "wifi_privilege.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef GNUCHANWIFI_ELEVATED_VARIABLE
#define GNUCHANWIFI_ELEVATED_VARIABLE "GNUCHANWIFI_ELEVATED"
#endif

/* The variables carried into the root copy, in order. DISPLAY and XAUTHORITY so
   the copy can still reach the user's X server; HOME so it reads the settings
   out of the user's own home and not root's, which is empty. The list ends with
   NULL and its length is what the arrays below are sized from. */
static const char *const kKeep[] = { "DISPLAY", "XAUTHORITY", "HOME", NULL };
#define KEEP_COUNT ((int)(sizeof(kKeep) / sizeof(kKeep[0])) - 1)

/* Where this program really is. /proc/self/exe is the one answer that does not
   depend on how it was started, which is exactly the problem with argv[0] under
   a window manager. Falls back to argv[0] on a system without procfs. */
static void self_path(int argc, char **argv, char *out, unsigned int size) {
    ssize_t got = readlink("/proc/self/exe", out, size - 1);
    if (got > 0) {
        out[got] = '\0';
        return;
    }
    snprintf(out, size, "%s",
             (argc > 0 && argv[0]) ? argv[0] : "GnuChanWifi");
}

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

    char self[PATH_MAX];
    self_path(argc, argv, self, sizeof(self));

    /* The NAME=value strings for the kept variables, each in its own buffer so
       the pointers stay valid for the whole execvp. */
    char kept[KEEP_COUNT][PATH_MAX + 64];
    const char *kept_pointers[KEEP_COUNT + 1];
    int kept_count = 0;
    for (int i = 0; i < KEEP_COUNT; i++) {
        const char *value = getenv(kKeep[i]);
        if (value && value[0]) {
            snprintf(kept[kept_count], sizeof(kept[0]), "%s=%s",
                     kKeep[i], value);
            kept_pointers[kept_count] = kept[kept_count];
            kept_count++;
        }
    }
    kept_pointers[kept_count] = NULL;

    char elevated[sizeof(GNUCHANWIFI_ELEVATED_VARIABLE) + 2];
    snprintf(elevated, sizeof(elevated), "%s=1",
             GNUCHANWIFI_ELEVATED_VARIABLE);

    /* sudo -E env ELEVATED=1 <kept...> /abs/self <args...>
     *
     * The count: sudo, -E, env, the elevated pair, the kept pairs, the program
     * path, the program's own arguments, and the terminating NULL. */
    int slots = 4 + kept_count + 1 + (argc > 1 ? argc - 1 : 0) + 1;
    char **command = calloc((size_t)slots, sizeof(char *));
    if (!command) {
        return -1;
    }

    int at = 0;
    command[at++] = (char *)"sudo";
    command[at++] = (char *)"-E";
    command[at++] = (char *)"env";
    command[at++] = elevated;
    for (int i = 0; i < kept_count; i++) {
        command[at++] = (char *)kept_pointers[i];
    }
    command[at++] = self;
    for (int i = 1; i < argc; i++) {
        command[at++] = argv[i];
    }
    command[at] = NULL;

    fprintf(stderr,
            "gnuchanwifi: this manages the radio and the driver, so it needs "
            "root; asking for the password\n");
    fflush(stderr);

    /* execvp searches PATH for "sudo" and replaces this process with it: when
       it succeeds nothing below runs, and the elevated copy is the one that
       opens the window. When it returns, sudo was not found. */
    execvp("sudo", command);

    fprintf(stderr,
            "gnuchanwifi: sudo could not be run; the window will open, but "
            "the radio and the driver cannot be changed\n");
    free(command);
    return -1;
}
