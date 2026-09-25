/*
 * wm_spawn.c — start a program, and find the terminal to start.
 *
 * Alt+Enter has to open *the* terminal, and which program that is differs
 * from machine to machine: a minimal Debian has one of a handful, a desktop
 * install another. This module answers "what is the user's terminal" by
 * checking the environment the session already set and then the usual
 * programs, and it is what the key module calls when Alt+Enter is pressed.
 *
 * Spawning is done by forking and re-executing, never by a shell string, so a
 * terminal named in $TERMINAL with a space in its path cannot turn into two
 * arguments.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_spawn.h"

/* The terminals this looks for, best first. The first one that exists is the
   one Alt+Enter opens. x-terminal-emulator is Debian's alternatives symlink —
   the distribution's own answer to "the terminal" — so it is tried first
   when it is present. */
static const char *TERMINAL_CANDIDATES[] = {
    "x-terminal-emulator",
    "gnome-terminal",
    "konsole",
    "xfce4-terminal",
    "alacritty",
    "kitty",
    "xterm",
    NULL,
};

/* Whether a program can be found on PATH. A name with a slash is used as it
   is, because that is what the caller meant; a bare name is searched for. */
static int program_exists(const char *name) {
    if (strchr(name, '/')) {
        return access(name, X_OK) == 0;
    }
    const char *path = getenv("PATH");
    if (!path) {
        return 0;
    }
    char buffer[4096];
    const char *start = path;
    size_t name_len = strlen(name);
    while (*start) {
        const char *end = strchr(start, ':');
        size_t dir_len = end ? (size_t)(end - start) : strlen(start);
        if (dir_len + name_len + 2 < sizeof(buffer)) {
            memcpy(buffer, start, dir_len);
            buffer[dir_len] = '/';
            memcpy(buffer + dir_len + 1, name, name_len + 1);
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

const char *wm_terminal_program(void) {
    /* The session's own choice wins. $TERMINAL is the convention every
       desktop and every terminal-aware tool agrees on, and a user who set it
       meant it. */
    const char *chosen = getenv("TERMINAL");
    if (chosen && chosen[0] && program_exists(chosen)) {
        return chosen;
    }
    for (int i = 0; TERMINAL_CANDIDATES[i]; i++) {
        if (program_exists(TERMINAL_CANDIDATES[i])) {
            return TERMINAL_CANDIDATES[i];
        }
    }
    return NULL;
}

int wm_spawn(const char *program, char *const argv[]) {
    if (!program) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("gnuchanwm: fork");
        return -1;
    }
    if (pid == 0) {
        /* In the child. A new session detaches the program from the WM's
           terminal — the WM has none — and from the WM's process group, so
           Ctrl+C in the spawned program cannot reach the WM. */
        setsid();
        execvp(program, argv);
        /* execvp only returns on failure. */
        perror("gnuchanwm: exec");
        _exit(127);
    }
    return 0;
}

int wm_spawn_terminal(void) {
    const char *terminal = wm_terminal_program();
    if (!terminal) {
        fprintf(stderr,
                "gnuchanwm: no terminal found. Install one (xterm, alacritty, "
                "gnome-terminal) or set $TERMINAL.\n");
        return -1;
    }
    char *argv[] = { (char *)terminal, NULL };
    return wm_spawn(terminal, argv);
}

int wm_spawn_terminal_displaying(const char *path) {
    const char *terminal = wm_terminal_program();
    if (!terminal || !path) {
        return -1;
    }
    /* Every terminal worth the name runs "-e <command>". The file is the
       command's own argument, never part of the shell text, so a path with a
       space in it is still one file. */
    char *argv[] = {
        (char *)terminal,
        "-e",
        "sh", "-c",
        "cat -- \"$1\"; printf '\\n[press Enter to close]\\n'; read _",
        "sh",
        (char *)path,
        NULL,
    };
    return wm_spawn(terminal, argv);
}
