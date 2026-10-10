/*
 * net_privilege.c — asking in a window, running through sudo, not coming back.
 *
 * See net_privilege.h for why. What is here is the order of the three steps:
 *
 *   1. If already root, do nothing.
 *   2. If not, show the password window (net_login.c) and get the key.
 *   3. Run sudo with the key on its standard input — sudo -S reads it from
 *      there — and wait for that copy to finish. This process was only ever a
 *      launcher for the root one, so when the copy exits, this exits with it.
 *
 * --- why sudo -S and a pipe ---
 *
 * sudo normally reads the password from a terminal (/dev/tty). A manager started
 * from the launcher has no terminal: there is no /dev/tty to read from, sudo
 * cannot prompt, and the manager appears to open nothing at all. sudo -S reads
 * the password from standard input instead, so the window that asked for it is
 * the only prompt, and this hands the answer over a pipe.
 *
 * The password is written to the pipe and the write end is closed at once, so
 * sudo sees the answer and then EOF. It is never written to a file and never put
 * on the command line (where /proc would show it), and it is overwritten before
 * this process leaves.
 */
#define _POSIX_C_SOURCE 200809L

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "net_login.h"
#include "net_privilege.h"

#ifndef GNUCHANNET_ELEVATED_VARIABLE
#define GNUCHANNET_ELEVATED_VARIABLE "GNUCHANNET_ELEVATED"
#endif

/* The variables carried into the root copy. DISPLAY and XAUTHORITY so it can
   reach the user's X server; HOME so it reads the settings out of the user's own
   home and not root's, which is empty. */
static const char *const kKeep[] = { "DISPLAY", "XAUTHORITY", "HOME", NULL };
#define KEEP_COUNT ((int)(sizeof(kKeep) / sizeof(kKeep[0])) - 1)

static int is_root(void) {
    return geteuid() == 0;
}

/* Where this program really is: /proc/self/exe, so the copy is run by absolute
   path and never searched for on a PATH that sudo has changed. */
static void self_path(int argc, char **argv, char *out, unsigned int size) {
    ssize_t got = readlink("/proc/self/exe", out, size - 1);
    if (got > 0) {
        out[got] = '\0';
        return;
    }
    snprintf(out, size, "%s",
             (argc > 0 && argv[0]) ? argv[0] : "GnuChanNetworkManager");
}

/* Overwrite a password buffer once it is no longer needed. Not protection
   against a determined reader — this process is small and short-lived — but the
   cost is a few stores. */
static void wipe(char *password) {
    if (password) {
        memset(password, 0, strlen(password));
    }
}

/* Build the sudo argument vector and run it with the password on standard
   input. Does not return on success: it waits for the copy and exits with its
   status. Returns -1 only when sudo could not be started at all. */
static int run_sudo(int argc, char **argv, const char *password) {
    char self[PATH_MAX];
    self_path(argc, argv, self, sizeof(self));

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

    char elevated[sizeof(GNUCHANNET_ELEVATED_VARIABLE) + 2];
    snprintf(elevated, sizeof(elevated), "%s=1",
             GNUCHANNET_ELEVATED_VARIABLE);

    /* sudo -S -E env ELEVATED=1 <kept...> /abs/self <args...>
     *
     * -S is the one that matters: it is what makes sudo read the password from
     * standard input rather than a terminal. */
    int slots = 5 + kept_count + 1 + (argc > 1 ? argc - 1 : 0) + 1;
    char **command = calloc((size_t)slots, sizeof(char *));
    if (!command) {
        return -1;
    }

    int at = 0;
    command[at++] = (char *)"sudo";
    command[at++] = (char *)"-S";
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

    int pipe_to_stdin[2];
    if (pipe(pipe_to_stdin) != 0) {
        free(command);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_to_stdin[0]);
        close(pipe_to_stdin[1]);
        free(command);
        return -1;
    }

    if (pid == 0) {
        /* The child: its standard input IS the read end of the pipe, so what
           the parent writes below arrives as sudo's password. */
        close(pipe_to_stdin[1]);
        dup2(pipe_to_stdin[0], STDIN_FILENO);
        if (pipe_to_stdin[0] != STDIN_FILENO) {
            close(pipe_to_stdin[0]);
        }
        execvp("sudo", command);
        _exit(127);        /* execvp only returns on failure */
    }

    /* The parent: hand over the password and close the write end, so sudo reads
       the answer and then an end of input. */
    close(pipe_to_stdin[0]);
    if (password && password[0]) {
        size_t length = strlen(password);
        ssize_t ignored = write(pipe_to_stdin[1], password, length);
        (void)ignored;
    }
    ssize_t newline = write(pipe_to_stdin[1], "\n", 1);
    (void)newline;
    close(pipe_to_stdin[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    free(command);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

int net_privilege_ensure(int argc, char **argv, const NetConfig *config) {
    if (is_root()) {
        return 0;
    }

    /* A copy that is somehow still not root must not try again: this is set on
       the sudo command line, so the copy carries it. */
    if (getenv(GNUCHANNET_ELEVATED_VARIABLE) != NULL) {
        return 0;
    }

    /* The one thing the person sees before the manager opens. */
    char password[NET_LOGIN_MAX];
    if (net_login_prompt(config, password, sizeof(password)) != 0) {
        fprintf(stderr,
                "gnuchannetworkmanager: no password was given, so it will "
                "open without root; the DNS and the interfaces cannot be "
                "changed\n");
        return -1;
    }

    int result = run_sudo(argc, argv, password);
    wipe(password);

    if (result < 0) {
        fprintf(stderr,
                "gnuchannetworkmanager: sudo could not be run; the window "
                "will open, but the DNS and the interfaces cannot be changed\n");
        /* Carry on and let the window open without root rather than failing to
           open at all: seeing the interfaces is still worth something. */
        return -1;
    }

    /* The root copy has finished (its window was closed) or sudo refused the
       password. Either way this launcher is done. */
    exit(result);
}
