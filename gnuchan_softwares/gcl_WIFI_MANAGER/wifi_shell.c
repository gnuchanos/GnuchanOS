/*
 * wifi_shell.c — the one place nmcli is run from.
 *
 * See wifi_shell.h. What is here is the runner, the quoting and the terse-field
 * splitter, and every reason the comments carry is a reason a copy of this in a
 * second file would get wrong: the two streams are joined so a failure nmcli
 * narrated on stderr is read the same as its output, the single quote is the
 * only character the quoting has to think about, and the terse format escapes a
 * colon inside a value so a plain strtok would cut an SSID in half.
 */
#include "wifi_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static char s_program[WIFI_TEXT] = "nmcli";

void wifi_shell_set_program(const char *program) {
    if (program && program[0]) {
        snprintf(s_program, sizeof(s_program), "%s", program);
    }
}

const char *wifi_shell_program(void) {
    return s_program;
}

void wifi_shell_quote(const char *in, char *out, unsigned int size) {
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

int wifi_shell_run(const char *command, char *out, unsigned int size) {
    /* Room for the longest command a caller can build, plus the " 2>&1" added
       below, so the command that reaches the shell is the whole command. */
    char line[WIFI_TEXT * 12];
    snprintf(line, sizeof(line), "%s 2>&1", command);

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

const char *wifi_shell_split(const char *line, char *out, unsigned int size) {
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
