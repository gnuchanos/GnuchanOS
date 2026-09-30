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
        /* The socket is closed, and XCloseDisplay is deliberately NOT called.
           The two are not the same thing, and the difference is the whole of
           this function.

           XCloseDisplay frees the display structure — its screens, its
           extensions, its buffers — and a forked child holds a copy of that
           structure, not one of its own. Calling it here means the child
           walking and freeing memory the parent is still using, and it is
           exactly the sequence the Xlib manual warns against after a fork:
           the child can fault inside Xlib and never reach the exec below.
           That is a launcher that closes and starts nothing, which is what
           this did.

           close(ConnectionNumber()) is the documented way to drop the
           connection in a child: it releases the socket and touches nothing
           else. The descriptor is the child's own copy — closing it cannot
           close the parent's — and no request is written, so the parent's
           stream stays exactly as the parent left it. */
        close(ConnectionNumber(display));
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
        /* A copy of stderr is kept for one message, and that copy is the whole
           reason this line exists: child_detach() points stderr at /dev/null,
           so a program that cannot be run — a command that is not installed, a
           path that is wrong — fails with nothing said anywhere. That is a
           launcher that "does nothing when you press Enter", which is a fault
           nobody can act on. The copy is closed as soon as the message is
           written, and nothing else is ever sent to it.

           The message is built and written by hand rather than with printf,
           because this is between fork and exec: the child shares the parent's
           stdio buffers and must not touch them. */
        int report = dup(STDERR_FILENO);

        child_detach(display);

        execvp(argv[0], argv);

        /* execvp only returns on failure. */
        if (report >= 0) {
            char message[512];
            int length = snprintf(message, sizeof(message),
                                  "gnuchanrunner: cannot run '%s': %s\n",
                                  argv[0], strerror(errno));
            if (length > 0) {
                size_t written = (size_t)length < sizeof(message)
                               ? (size_t)length : sizeof(message) - 1;
                ssize_t ignored = write(report, message, written);
                (void)ignored;
            }
            close(report);
        }

        /* _exit rather than exit, because the child shares the parent's
           buffers and exit would flush them a second time. */
        _exit(127);
    }
    return pid;
}

/* Whether a program can be found on PATH, or at the path it was written with.
   A bare name is searched for, which is what a terminal in a .desktop file is;
   a name with a slash is used as it is. */
static int program_exists(const char *name) {
    if (!name || !name[0]) {
        return 0;
    }
    if (strchr(name, '/')) {
        return access(name, X_OK) == 0;
    }
    const char *path = getenv("PATH");
    if (!path) {
        return 0;
    }
    char buffer[4096];
    const char *start = path;
    size_t name_length = strlen(name);
    while (*start) {
        const char *end = strchr(start, ':');
        size_t dir_length = end ? (size_t)(end - start) : strlen(start);
        if (dir_length + name_length + 2 < sizeof(buffer)) {
            memcpy(buffer, start, dir_length);
            buffer[dir_length] = '/';
            memcpy(buffer + dir_length + 1, name, name_length + 1);
            if (access(buffer, X_OK) == 0) {
                return 1;
            }
        }
        if (!end) {
            break;
        }
        start = end + 1;
    }
    return 0;
}

const char *runner_launch_terminal(void) {
    const char *from_environment = getenv("TERMINAL");
    /* $TERMINAL is answered only when it names something that can actually be
       run: a value left over from another session and since uninstalled must
       fall through to the candidates below rather than leave a program with no
       terminal to start in. */
    if (from_environment && from_environment[0] &&
        program_exists(from_environment)) {
        return from_environment;
    }

    /* The candidates are tried in order and the first one this machine really
       has is the answer. Returning candidates[0] without looking — which is
       what this did — names a terminal that may not be installed at all; the
       program is then handed to a terminal that does not exist and nothing
       happens, which is the same "nothing happened" as a bad command and much
       harder to explain. */
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
        if (program_exists(candidates[i])) {
            return candidates[i];
        }
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
