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
 *
 * A command LINE — what RunProgram(command=...) carries — is a different
 * thing from a program name: "rofi -show run" is one line that names a
 * program and its arguments. wm_spawn_command() below splits such a line into
 * its words with a small tokeniser and hands the words to wm_spawn(), which is
 * still the fork + execvp path and still never a shell. The tokeniser is what
 * the script's own reader uses — quotes group, a backslash escapes — so a
 * command written in the settings file is read the way it was written.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_spawn.h"

/* The terminals this looks for, best first. The first one that exists is the
   one Alt+Enter opens.
 *
 * GNUCHANTERM IS FIRST, and it is the whole of why this list is not the same
 * one every desktop ships. This is GnuChanOS: the terminal this desktop is
 * themed around is its own, and a manager that opened a different one — xterm,
 * or whatever `x-terminal-emulator` happens to point at — would be opening a
 * terminal whose colours come from its own files and not from the settings
 * script, which is exactly the bug this fixes. A machine that has not installed
 * GnuChanTerm falls through to the rest, so the list still ends in something
 * every machine has.
 *
 * The rest is the usual order. x-terminal-emulator is Debian's alternatives
 * symlink — the distribution's own answer to "the terminal" — and the named
 * programs follow it for a machine that has one of them and not the symlink. */
static const char *TERMINAL_CANDIDATES[] = {
    "GnuChanTerm",
    "x-terminal-emulator",
    "gnome-terminal",
    "konsole",
    "xfce4-terminal",
    "alacritty",
    "kitty",
    "xterm",
    NULL,
};

/* The terminal the settings script named, if it named one this machine can
   run. It is a copy rather than a pointer: the config it came from may be
   replaced by a hot reload, and a pointer into that would dangle. */
static char preferred_terminal[256] = "";

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

void wm_spawn_set_terminal(const char *program) {
    preferred_terminal[0] = '\0';
    if (!program || !program[0]) {
        return;
    }
    if (!program_exists(program)) {
        fprintf(stderr,
                "gnuchanwm: config: terminal '%s' is not runnable; "
                "using the usual candidates\n", program);
        return;
    }
    snprintf(preferred_terminal, sizeof(preferred_terminal), "%s", program);
}

const char *wm_terminal_program(void) {
    /* The settings script's own answer first. A script asks for a terminal
       for a reason — it is the one installed on that machine, or the one its
       owner chose — so it is not overridden by a guess about what is usual. */
    if (preferred_terminal[0]) {
        return preferred_terminal;
    }
    /* Then the environment. $TERMINAL is the convention every desktop and
       every terminal-aware tool agrees on, and a user who set it meant it. */
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

/* A written command line, split into the words execvp wants. The words live
   back to back in `storage`; `argv` points into them and is NULL-terminated. */
typedef struct CommandLine {
    char *storage;
    char **argv;
    int argc;
} CommandLine;

static void command_line_release(CommandLine *line) {
    free(line->storage);
    free(line->argv);
    line->storage = NULL;
    line->argv = NULL;
    line->argc = 0;
}

/* Split a command line into words.
 *
 * This is the whole of what a command line needs and nothing more, and it is
 * a real little piece of lexing rather than a call to a shell. White space
 * separates words; a single or a double quote groups them, so a path with a
 * space in it stays one word; and a backslash escapes the character after it
 * — inside double quotes as well, and literally inside single ones, which is
 * what a shell does. The action a binding names is read by the same kind of
 * tokeniser that read the script, so a command written in the settings file
 * is read the way it was written.
 *
 * Returns 0 with `line` filled in, or -1 when the line is empty or memory
 * could not be had. */
static int command_line_split(const char *command, CommandLine *line) {
    memset(line, 0, sizeof(*line));
    if (!command || !command[0]) {
        return -1;
    }

    size_t length = strlen(command);
    /* A word can never be longer than the whole line, so the word block is the
       line; and there are at most half as many words as characters, plus one,
       which is room for the pointers. */
    line->storage = malloc(length + 1);
    line->argv = malloc(((length + 1) / 2 + 2) * sizeof(char *));
    if (!line->storage || !line->argv) {
        command_line_release(line);
        return -1;
    }

    char *write = line->storage;
    const char *c = command;
    while (*c) {
        while (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') {
            c++;
        }
        if (!*c) {
            break;
        }
        line->argv[line->argc++] = write;

        char quote = '\0';
        for (; *c; c++) {
            char ch = *c;
            if (quote == '\'') {
                if (ch == '\'') { quote = '\0'; continue; }
                *write++ = ch;
                continue;
            }
            if (quote == '"') {
                if (ch == '"') { quote = '\0'; continue; }
                if (ch == '\\' && c[1]) { c++; *write++ = *c; continue; }
                *write++ = ch;
                continue;
            }
            if (ch == '\'' || ch == '"') { quote = ch; continue; }
            if (ch == '\\' && c[1]) { c++; *write++ = *c; continue; }
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                break;
            }
            *write++ = ch;
        }
        if (quote != '\0') {
            fprintf(stderr,
                    "gnuchanwm: a command has a quote with no closing quote; "
                    "the rest of the line became one word\n");
        }
        *write++ = '\0';
    }
    line->argv[line->argc] = NULL;
    return 0;
}

int wm_spawn_command(const char *command) {
    CommandLine line;
    if (command_line_split(command, &line) != 0) {
        return -1;
    }
    if (line.argc == 0) {
        command_line_release(&line);
        return -1;
    }
    /* wm_spawn() forks, so the child has its own copy of the words and the
       release below happens in the parent alone. */
    int result = wm_spawn(line.argv[0], line.argv);
    command_line_release(&line);
    return result;
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
