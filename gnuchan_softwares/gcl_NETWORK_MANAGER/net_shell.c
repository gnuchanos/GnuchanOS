/*
 * net_shell.c — the one place a network command is run from.
 *
 * See net_shell.h. What is here is the runner, the quoting and the terse-field
 * splitter, each the same shape the wifi manager proved: the two streams are
 * joined so a failure narrated on stderr is read the same as output, the single
 * quote is the only character the quoting has to think about, and the terse
 * format escapes a colon inside a value so a plain split would cut a name in
 * half.
 */
#include "net_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void net_shell_quote(const char *in, char *out, unsigned int size) {
    unsigned int at = 0;
    if (size == 0) {
        return;
    }
    out[at++] = '\'';
    for (const char *p = in ? in : ""; *p && at + 4 < size; p++) {
        if (*p == '\'') {
            /* close, escaped quote, reopen: '\'' */
            out[at++] = '\'';
            out[at++] = '\\';
            out[at++] = '\'';
            out[at++] = '\'';
        } else {
            out[at++] = *p;
        }
    }
    out[at++] = '\'';
    out[at] = '\0';
}

int net_shell_run(const char *command, char *out, unsigned int size) {
    /* Room for the longest command a caller can build, plus the wrapper and
       the " 2>&1" added below, so the command that reaches the shell is
       whole. */
    char line[NET_TEXT * 12];

    /* Every command is given a hard ceiling, and this is not belt-and-braces:
       it is what keeps the window from freezing. These run on the UI thread,
       and a few of them talk to a service rather than doing a quick job —
       resolvectl asks systemd-resolved over D-Bus, nmcli asks NetworkManager —
       and a service that is starting, wedged, or simply slow keeps the caller
       blocked for as long as D-Bus's own timeout allows (tens of seconds). A
       window frozen during an "Apply" is indistinguishable from a crash. With
       `timeout` each command is given a few seconds and then killed, the
       action reports that it did not answer, and the window stays alive.
       timeout is coreutils and is on every Debian. */
    const char *prefix = net_shell_have("timeout") ? "timeout 8 " : "";
    snprintf(line, sizeof(line), "%s%s 2>&1", prefix, command);

    FILE *pipe = popen(line, "r");
    if (!pipe) {
        if (size) {
            out[0] = '\0';
        }
        return 127;
    }

    unsigned int at = 0;
    int c;
    while ((c = fgetc(pipe)) != EOF) {
        if (at + 1 < size) {
            out[at++] = (char)c;
        }
        /* Past the buffer the rest is read and dropped: draining the pipe is
           what keeps pclose from waiting on a full pipe. */
    }
    out[at] = '\0';

    int status = pclose(pipe);
    if (status == -1) {
        return 127;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

const char *net_shell_split(const char *line, char *out, unsigned int size) {
    unsigned int at = 0;
    if (size == 0) {
        return NULL;
    }
    if (line == NULL) {
        out[0] = '\0';
        return NULL;
    }
    while (*line && *line != '\n') {
        if (*line == '\\' && line[1]) {
            if (at + 1 < size) {
                out[at++] = line[1];
            }
            line += 2;
            continue;
        }
        if (*line == ':') {
            out[at] = '\0';
            return line + 1;
        }
        if (at + 1 < size) {
            out[at++] = *line;
        }
        line++;
    }
    out[at] = '\0';
    return NULL;
}

/* Whether a program can be found on PATH. Walked by hand rather than with
   access() on the bare name, because a bare name is looked up by the shell and
   not by this process: what is wanted is the search the shell would do, so the
   PATH is split and each directory tried. */
int net_shell_have(const char *program) {
    if (!program || !program[0]) {
        return 0;
    }
    if (strchr(program, '/')) {
        return access(program, X_OK) == 0;
    }
    const char *path = getenv("PATH");
    if (!path) {
        return 0;
    }
    char *copy = strdup(path);
    if (!copy) {
        return 0;
    }
    int found = 0;
    for (char *dir = strtok(copy, ":"); dir; dir = strtok(NULL, ":")) {
        char candidate[NET_TEXT * 2];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, program);
        if (access(candidate, X_OK) == 0) {
            found = 1;
            break;
        }
    }
    free(copy);
    return found;
}
