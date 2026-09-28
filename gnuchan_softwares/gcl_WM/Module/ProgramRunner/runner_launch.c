/*
 * runner_launch.c — fork, detach, and exec.
 *
 * One child is started and the launcher does not wait for it. Everything here
 * is about making the child a program that happens to have been started rather
 * than a process tied to the launcher:
 *
 *   setsid()          the child leads a session of its own, so it survives the
 *                     launcher exiting and never answers the launcher's
 *                     terminal's signals
 *
 *   the display is    closed in the child before the exec, so the program does
 *   closed            not hold the launcher's connection to the server open,
 *                     and does not appear to the manager as the launcher
 *
 *   the standard      pointed at /dev/null, because a program started by a
 *   streams           launcher with no terminal has nowhere for its output to
 *                     go, and a child writing to a closed descriptor is a
 *                     child that dies on its first line of output
 *
 * A double fork is deliberately not done. It is the usual way to make a
 * daemon, and it is not wanted here: the program is a program, it should be a
 * child of the session like any other, and a manager that tracks it should see
 * it appear normally.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "runner_launch.h"

/* Close the display in a forked child, and point its standard streams at
   nothing. Both are one-way: after this the child has no connection to the
   server and no terminal, which is what makes it a program the launcher
   started rather than one it is holding. */
static void child_detach(Display *display) {
    if (display) {
        /* XCloseDisplay flushes any request still queued, which in a forked
           child is a request the parent also thinks it made: the child must
           not send anything, so the connection is closed with the buffer
           thrown away. XCloseDisplay on a display whose buffer is empty is
           exactly that, and the launcher flushes after every draw, so the
           buffer is empty here. */
        XCloseDisplay(display);
    }

    setsid();

    /* The three standard streams go to /dev/null. Opening it once and
       duplicating it is what a child with no terminal does: it may write all
       it likes and the writes go nowhere, which is better than a write to a
       closed descriptor killing it. */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) {
            close(devnull);
        }
    }
}

/* Run one command, given its words, after detaching. Returns the fork's
   answer: the child's pid on success, -1 when the fork failed. */
static pid_t launch_words(Display *display, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        child_detach(display);
        execvp(argv[0], argv);
        /* execvp only returns on failure. _exit rather than exit, because the
           child shares the parent's buffers and exit would flush them a second
           time. */
        _exit(127);
    }
    return pid;
}

const char *runner_launch_terminal(void) {
    const char *from_environment = getenv("TERMINAL");
    if (from_environment && from_environment[0]) {
        /* Answered only when it is a name that can be run: a $TERMINAL left
           over from another session and since uninstalled must fall through to
           the candidates below rather than leave a program unable to start. */
        if (strchr(from_environment, '/')) {
            if (access(from_environment, X_OK) == 0) {
                return from_environment;
            }
        } else {
            return from_environment;    /* execvp will find it or not */
        }
    }

    static const char *const candidates[] = {
        "x-terminal-emulator",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "alacritty",
        "kitty",
        "xterm",
        NULL,
    };
    for (int i = 0; candidates[i]; i++) {
        /* The candidates are all names without a slash, found through PATH,
           which is what a session's terminal is. Checking the path here would
           mean writing a PATH walk for a decision execvp makes anyway, so the
           first candidate is taken and a machine that has none gets the same
           "nothing happened" a bad command gives. */
        return candidates[i];
    }
    return NULL;
}

int runner_launch_program(Display *display, const RunnerProgram *program) {
    if (!program || program->count <= 0 || program->command[0][0] == '\0') {
        return -1;
    }

    if (!program->in_terminal) {
        char *argv[RUNNER_MAX_COMMAND_WORDS + 1];
        for (int i = 0; i < program->count; i++) {
            argv[i] = (char *)program->command[i];
        }
        argv[program->count] = NULL;
        return launch_words(display, argv) < 0 ? -1 : 0;
    }

    /* Terminal=true: the program is run inside a terminal window. The
       terminal is asked to run the program and its arguments, terminal by
       terminal: -e is the convention most of them share and is where a program
       that is not the terminal's own default goes. */
    const char *terminal = runner_launch_terminal();
    if (!terminal) {
        /* No terminal on this machine: the program is run with its output
           going nowhere, which is worse than a terminal window and better
           than not starting it. */
        char *argv[RUNNER_MAX_COMMAND_WORDS + 1];
        for (int i = 0; i < program->count; i++) {
            argv[i] = (char *)program->command[i];
        }
        argv[program->count] = NULL;
        return launch_words(display, argv) < 0 ? -1 : 0;
    }

    char *argv[RUNNER_MAX_COMMAND_WORDS + 3];
    int at = 0;
    argv[at++] = (char *)terminal;
    argv[at++] = "-e";
    for (int i = 0; i < program->count; i++) {
        argv[at++] = (char *)program->command[i];
    }
    argv[at] = NULL;

    return launch_words(display, argv) < 0 ? -1 : 0;
}

int runner_launch_command(Display *display, const char *command) {
    if (!command || !command[0]) {
        return -1;
    }

    /* The line is split by the same rule a .desktop file's Exec is, so a
       command typed with quotes works the same way one written in a file
       does. */
    RunnerProgram program;
    memset(&program, 0, sizeof(program));
    if (runner_apps_split_command(&program, command) != 0) {
        return -1;
    }

    char *argv[RUNNER_MAX_COMMAND_WORDS + 1];
    for (int i = 0; i < program.count; i++) {
        argv[i] = program.command[i];
    }
    argv[program.count] = NULL;

    return launch_words(display, argv) < 0 ? -1 : 0;
}
