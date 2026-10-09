/*
 * wifi_nm.c — the list of networks, joining one, and what is joined now.
 *
 * Every command is built and run through wifi_shell.c, which is the one place
 * the shell, the quoting and the terse parsing live. What is left here is the
 * shape of each question and what its answer means.
 */
#include "wifi_nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void wifi_nm_device(char *out, unsigned int size) {
    if (size) {
        out[0] = '\0';
    }

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s -t -f DEVICE,TYPE device",
             wifi_shell_program());

    static char output[WIFI_TEXT * 24];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        return;
    }

    char *save = output;
    while (save && *save) {
        char *newline = strchr(save, '\n');
        if (newline) {
            *newline = '\0';
        }

        char device[WIFI_TEXT];
        wifi_shell_split(save, device, sizeof(device));

        /* The two fields sit on one line "DEVICE:TYPE". device holds the first,
           and the type is the text after the first bare colon. */
        const char *rest = strchr(save, ':');
        if (rest) {
            char type[WIFI_TEXT];
            wifi_shell_split(rest + 1, type, sizeof(type));
            if (strcmp(type, "wifi") == 0 && device[0]) {
                snprintf(out, size, "%s", device);
                return;
            }
        }

        save = newline ? newline + 1 : NULL;
    }
}

/* Whether `ssid` is one of the saved profiles. The list is small and this is a
   plain scan of it; a hash table for a dozen names would be more code than the
   question is worth. */
static int is_saved(const char (*saved)[WIFI_TEXT], int saved_count,
                    const char *ssid) {
    for (int i = 0; i < saved_count; i++) {
        if (strcmp(saved[i], ssid) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Wait, so a scan that was just asked for has time to land. */
static void scan_wait(void) {
    struct timespec pause;
    pause.tv_sec = 2;
    pause.tv_nsec = 0;
    nanosleep(&pause, NULL);
}

int wifi_nm_scan(WifiList *list, const char *device,
                 const char (*saved_ssids)[WIFI_TEXT], int saved_count) {
    if (!list) {
        return -1;
    }
    list->count = 0;

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command),
             "%s -t -f SSID,SIGNAL,SECURITY,IN-USE device wifi list",
             wifi_shell_program());

    /* Ask for a fresh scan and WAIT for it before reading the list.
     *
     * This used to be one command — "device wifi list --rescan yes" — and that
     * is exactly what kept a network that had gone away on the list: that
     * command STARTS a scan and then prints the list NetworkManager already
     * had, because the scan it had just asked for has not finished. A phone's
     * hotspot switched off therefore stayed on the screen however often Rescan
     * was pressed, the list coming from the cache the new scan was about to
     * replace.
     *
     * So the two are split here: the scan is requested, the program waits long
     * enough for a real scan to sweep the air, and only then is the list read
     * — so what is read is the list the scan just produced, not the one it was
     * about to replace.
     *
     * The wait is a fixed one and not a poll of the list, and that is on
     * purpose: a poll cannot tell a scan that has finished from one still
     * running, because every read of the list carries a fresh signal-strength
     * number, so the answer "changes" on the very first read and a poll would
     * stop before the scan had swept anything at all. Two seconds is what an
     * ordinary scan takes; a slower one shows a list a moment behind, which is
     * not the fault this fixes — the fault was a list that never changed.
     */
    wifi_nm_rescan(device);
    scan_wait();

    static char output[WIFI_TEXT * WIFI_MAX_NETWORKS * 2];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
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

        const char *rest = wifi_shell_split(line, ssid, sizeof(ssid));
        rest = wifi_shell_split(rest, signal, sizeof(signal));
        rest = wifi_shell_split(rest, security, sizeof(security));
        wifi_shell_split(rest, in_use, sizeof(in_use));

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
            /* "--" is nmcli's way of saying open; anything else means a key, a
               password or an enterprise login is needed. */
            network->secured = (security[0] && strcmp(security, "--") != 0);
            network->in_use = (strchr(in_use, '*') != NULL);
            network->saved = is_saved(saved_ssids, saved_count, ssid);
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
    wifi_shell_quote(ssid, quoted_ssid, sizeof(quoted_ssid));

    char command[WIFI_TEXT * 8];
    if (password && password[0]) {
        char quoted_password[WIFI_TEXT * 2];
        wifi_shell_quote(password, quoted_password, sizeof(quoted_password));
        snprintf(command, sizeof(command),
                 "%s device wifi connect %s password %s",
                 wifi_shell_program(), quoted_ssid, quoted_password);
    } else {
        snprintf(command, sizeof(command),
                 "%s device wifi connect %s", wifi_shell_program(),
                 quoted_ssid);
    }

    static char output[WIFI_TEXT * 8];
    int status = wifi_shell_run(command, output, sizeof(output));
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
    wifi_shell_quote(device, quoted_device, sizeof(quoted_device));

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s device disconnect %s",
             wifi_shell_program(), quoted_device);

    static char output[WIFI_TEXT * 8];
    int status = wifi_shell_run(command, output, sizeof(output));
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
        wifi_shell_quote(device, quoted_device, sizeof(quoted_device));
        snprintf(command, sizeof(command), "%s device wifi rescan ifname %s",
                 wifi_shell_program(), quoted_device);
    } else {
        snprintf(command, sizeof(command), "%s device wifi rescan",
                 wifi_shell_program());
    }
    char output[WIFI_TEXT];
    wifi_shell_run(command, output, sizeof(output));
    /* The answer is not read: a rescan refused because one is already running
       is the expected state while a person presses "rescan" twice. */
}

int wifi_nm_active(const char *device, char *ssid, unsigned int ssid_size,
                   char *address, unsigned int address_size) {
    if (ssid && ssid_size) {
        ssid[0] = '\0';
    }
    if (address && address_size) {
        address[0] = '\0';
    }
    if (!device || !device[0]) {
        return -1;
    }

    /* The interface's own state, in one line: DEVICE:TYPE:STATE:CONNECTION. The
       state "connected" and a connection name is what says something is up; the
       name is the profile, which is the SSID for a simple network. */
    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command),
             "%s -t -f DEVICE,STATE,CONNECTION device status", 
             wifi_shell_program());

    static char output[WIFI_TEXT * 16];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        return -1;
    }

    int connected = 0;
    char *line = output;
    while (line && *line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        char line_device[WIFI_TEXT];
        char state[WIFI_TEXT];
        char connection[WIFI_TEXT];
        const char *rest = wifi_shell_split(line, line_device,
                                            sizeof(line_device));
        rest = wifi_shell_split(rest, state, sizeof(state));
        wifi_shell_split(rest, connection, sizeof(connection));

        if (strcmp(line_device, device) == 0 &&
            strcmp(state, "connected") == 0) {
            connected = 1;
            if (ssid && ssid_size) {
                snprintf(ssid, ssid_size, "%s", connection);
            }
            break;
        }

        line = newline ? newline + 1 : NULL;
    }

    if (!connected) {
        return -1;
    }

    /* The IPv4 address, asked of the same interface. It is a second command
       because the address is what a person wants to see and the connection name
       does not carry it. */
    snprintf(command, sizeof(command),
             "%s -t -f IP4.ADDRESS device show %s", wifi_shell_program(),
             device);

    static char address_output[WIFI_TEXT * 8];
    if (wifi_shell_run(command, address_output, sizeof(address_output)) == 0 &&
        address && address_size) {
        const char *value = strchr(address_output, ':');
        if (value) {
            value++;
            char line_value[WIFI_TEXT];
            wifi_shell_split(value, line_value, sizeof(line_value));
            /* nmcli reports an address as "192.168.1.5/24"; the slash and the
               prefix length are not part of the address a person reads. */
            char *slash = strchr(line_value, '/');
            if (slash) {
                *slash = '\0';
            }
            snprintf(address, address_size, "%s", line_value);
        }
    }

    return 0;
}
