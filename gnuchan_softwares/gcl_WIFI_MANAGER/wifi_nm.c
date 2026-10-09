/*
 * wifi_nm.c — running nmcli and reading what it says.
 *
 * Every command here is built as a string and run through the shell, because
 * that is the one way to run a program and read its output that needs no
 * library at all. The cost of that choice is quoting, and quoting is therefore
 * the thing this file is most careful about: an SSID and a password are the
 * two pieces of text a person types, they may contain a space, a quote or a
 * shell character, and an unquoted one would be run as a command rather than
 * passed as a name. shell_quote() is the answer, and every piece of text that
 * came from a person goes through it.
 *
 * The output format is nmcli's terse one — `-t`, fields separated by colons —
 * rather than the aligned table meant for a terminal. The table pads and
 * wraps to a width, which makes it something to read rather than something to
 * parse; the terse form is exactly the fields, one record per line, with the
 * colons inside a value escaped. That escaping is why split_terse() below is
 * not a call to strtok.
 */
#include "wifi_nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* Which nmcli to run. "nmcli" by default, so PATH decides, and a config may
   name an absolute path instead. */
static char s_program[WIFI_TEXT] = "nmcli";

void wifi_nm_set_program(const char *program) {
    if (program && program[0]) {
        snprintf(s_program, sizeof(s_program), "%s", program);
    }
}

/* Quote one argument so the shell passes it through whole.
 *
 * The argument is wrapped in single quotes, and a single quote inside it is
 * closed, escaped and reopened — the `'\''` dance — which is the one sequence
 * single-quoting cannot express directly. Everything else, including spaces,
 * quotes and `$`, is literal between single quotes. The result is written into
 * `out`; a caller gives a buffer around twice the text plus a few.
 *
 * A NULL or empty argument becomes `''`, which the shell reads as an empty
 * string: an empty password has to reach nmcli as an argument, not vanish. */
static void shell_quote(const char *in, char *out, unsigned int size) {
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

/* Run a command through the shell and copy its output — stdout and stderr
   together — into `out`. Returns the exit status, or 127 when the command
   could not be run at all. Both streams are joined with `2>&1` so a failure
   that nmcli narrated on stderr is read back the same way as its output; a
   caller wants the words, not the stream they came on. */
static int run_capture(const char *command, char *out, unsigned int size) {
    /* Room for the longest command a caller can build, plus the " 2>&1" that
       is appended here, so the command that reaches the shell is the whole
       command and not a shortened one. */
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
    /* WIFEXITED and WEXITSTATUS live in <sys/wait.h>: without it the two are
       not declared, and a caller cannot tell a command that ran and failed
       from one that was never found. */
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/* Copy the next field of a terse record into `out`, and return a pointer to
   the character after the field's terminating colon, or NULL at the end of the
   line.
 *
 * This is where nmcli's escaping is undone. In terse output a colon or a
 * backslash INSIDE a value is written with a backslash before it, so `\:` is a
 * colon that belongs to the value and a bare `:` ends it. A value that is an
 * SSID like "Cafe: WiFi" therefore comes back whole, which is the point. */
static const char *split_terse(const char *line, char *out, unsigned int size) {
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
            /* The next character is literal, whatever it is. */
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

void wifi_nm_device(char *out, unsigned int size) {
    if (size) {
        out[0] = '\0';
    }

    /* The buffer is the program's own maximum plus the fixed text, with room
       to spare: a command that could be cut off is a command that runs wrong,
       and -Wformat-truncation cannot be told the fixed part is small. */
    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s -t -f DEVICE,TYPE device",
             s_program);

    /* Big enough for a device list; a machine with many interfaces still fits
       with room to spare. */
    static char output[WIFI_TEXT * 24];
    if (run_capture(command, output, sizeof(output)) != 0) {
        return;
    }

    char *save = output;
    while (save && *save) {
        char *newline = strchr(save, '\n');
        if (newline) {
            *newline = '\0';
        }

        char device[WIFI_TEXT];
        split_terse(save, device, sizeof(device));

        /* The two fields sit on one line "DEVICE:TYPE". device holds the
           first, and the type is the text after the first bare colon. */
        const char *rest = strchr(save, ':');
        if (rest) {
            char type[WIFI_TEXT];
            split_terse(rest + 1, type, sizeof(type));
            if (strcmp(type, "wifi") == 0 && device[0]) {
                snprintf(out, size, "%s", device);
                return;
            }
        }

        save = newline ? newline + 1 : NULL;
    }
}

int wifi_nm_scan(WifiList *list) {
    if (!list) {
        return -1;
    }
    list->count = 0;

    /* --rescan yes asks for a fresh scan first, so the list is what is in
       range now rather than what was cached at login. Fields, in order: the
       name a person reads, the strength, how it is secured, and whether it is
       the one joined. */
    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command),
             "%s -t -f SSID,SIGNAL,SECURITY,IN-USE device wifi list "
             "--rescan yes",
             s_program);

    /* One record per network, each maybe a couple of hundred bytes: room for
       the maximum the scan will hold, and then some. */
    static char output[WIFI_TEXT * WIFI_MAX_NETWORKS * 2];
    if (run_capture(command, output, sizeof(output)) != 0) {
        return -1;
    }

    char *line = output;
    while (line && *line && list->count < WIFI_MAX_NETWORKS) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        char ssid[WIFI_TEXT];
        char signal[WIFI_TEXT];
        char security[WIFI_TEXT];
        char in_use[WIFI_TEXT];

        const char *rest = split_terse(line, ssid, sizeof(ssid));
        rest = split_terse(rest, signal, sizeof(signal));
        rest = split_terse(rest, security, sizeof(security));
        split_terse(rest, in_use, sizeof(in_use));

        /* A hidden network has an empty SSID, which is not something a person
           can pick by name, so it is left out. */
        if (ssid[0]) {
            WifiNetwork *network = &list->items[list->count];
            memset(network, 0, sizeof(*network));
            snprintf(network->ssid, sizeof(network->ssid), "%s", ssid);
            network->signal = atoi(signal);
            if (network->signal < 0) {
                network->signal = 0;
            }
            if (network->signal > 100) {
                network->signal = 100;
            }
            /* "--" is nmcli's way of saying open; anything else means a key,
               a password or an enterprise login is needed. */
            network->secured = (security[0] && strcmp(security, "--") != 0);
            network->in_use = (strchr(in_use, '*') != NULL);
            list->count++;
        }

        line = newline ? newline + 1 : NULL;
    }

    return 0;
}

int wifi_nm_connect(const char *device, const char *ssid,
                    const char *password, char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }
    if (!device || !device[0] || !ssid || !ssid[0]) {
        if (error && size) {
            snprintf(error, size, "no wireless interface or network chosen");
        }
        return -1;
    }

    char quoted_ssid[WIFI_TEXT * 2];
    shell_quote(ssid, quoted_ssid, sizeof(quoted_ssid));

    /* One command line with the two forms: with a password, and without. A
       password that was given is passed on the command line, which is how
       nmcli is told one at all; a saved network connects with the first form
       and needs no password. */
    char command[WIFI_TEXT * 8];
    if (password && password[0]) {
        char quoted_password[WIFI_TEXT * 2];
        shell_quote(password, quoted_password, sizeof(quoted_password));
        snprintf(command, sizeof(command),
                 "%s device wifi connect %s password %s",
                 s_program, quoted_ssid, quoted_password);
    } else {
        snprintf(command, sizeof(command),
                 "%s device wifi connect %s", s_program, quoted_ssid);
    }

    static char output[WIFI_TEXT * 8];
    int status = run_capture(command, output, sizeof(output));
    if (status != 0) {
        if (error && size) {
            /* nmcli's own words, trimmed to one line, are more useful than a
               generic message: "Secrets were required" tells a person what to
               do and "failed" does not. */
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not connect");
        }
        return -1;
    }
    return 0;
}

int wifi_nm_disconnect(const char *device, char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }
    if (!device || !device[0]) {
        if (error && size) {
            snprintf(error, size, "no wireless interface to disconnect");
        }
        return -1;
    }

    char quoted_device[WIFI_TEXT * 2];
    shell_quote(device, quoted_device, sizeof(quoted_device));

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s device disconnect %s",
             s_program, quoted_device);

    static char output[WIFI_TEXT * 8];
    int status = run_capture(command, output, sizeof(output));
    if (status != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not disconnect");
        }
        return -1;
    }
    return 0;
}

void wifi_nm_rescan(const char *device) {
    char command[WIFI_TEXT * 8];
    if (device && device[0]) {
        char quoted_device[WIFI_TEXT * 2];
        shell_quote(device, quoted_device, sizeof(quoted_device));
        snprintf(command, sizeof(command), "%s device wifi rescan ifname %s",
                 s_program, quoted_device);
    } else {
        snprintf(command, sizeof(command), "%s device wifi rescan", s_program);
    }
    char output[WIFI_TEXT];
    run_capture(command, output, sizeof(output));
    /* The answer is not read: a rescan refused because one is already running
       is the expected state while a person presses "rescan" twice, and it is
       not something to put on the screen. */
}
