/*
 * wifi_saved.c — reading, forgetting, and re-pointing the saved profiles.
 *
 * `nmcli -t -f NAME,TYPE,AUTOCONNECT connection show` gives every profile in
 * one line each; only the wifi ones are kept, because a wired profile is not
 * something this window has any business listing. Forget and autoconnect are
 * two more nmcli calls against the profile's NAME.
 *
 * A profile's NAME is used as the network's identity throughout, and for a
 * simple wifi network that name IS the SSID. A machine with two profiles for
 * one SSID — a rare, deliberate thing — would show them here by their names,
 * which is the honest answer: they are two different profiles.
 */
#include "wifi_saved.h"

#include <stdio.h>
#include <string.h>

int wifi_saved_load(WifiSavedList *list) {
    if (!list) {
        return -1;
    }
    list->count = 0;

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command),
             "%s -t -f NAME,TYPE,AUTOCONNECT connection show",
             wifi_shell_program());

    static char output[WIFI_TEXT * WIFI_MAX_SAVED * 2];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        return -1;
    }

    char *line = output;
    while (line && *line && list->count < WIFI_MAX_SAVED) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        char name[WIFI_TEXT];
        char type[WIFI_TEXT];
        char autoconnect[WIFI_TEXT];

        const char *rest = wifi_shell_split(line, name, sizeof(name));
        rest = wifi_shell_split(rest, type, sizeof(type));
        wifi_shell_split(rest, autoconnect, sizeof(autoconnect));

        if (name[0] && strcmp(type, "802-11-wireless") == 0) {
            int index = list->count;
            snprintf(list->names[index], sizeof(list->names[0]), "%s", name);
            list->autoconnect[index] = (strcmp(autoconnect, "yes") == 0);
            list->count++;
        }

        line = newline ? newline + 1 : NULL;
    }

    return 0;
}

int wifi_saved_names(const WifiSavedList *list, char (*out)[WIFI_TEXT],
                     int max) {
    if (!list || !out || max <= 0) {
        return 0;
    }
    int count = list->count < max ? list->count : max;
    for (int i = 0; i < count; i++) {
        snprintf(out[i], WIFI_TEXT, "%s", list->names[i]);
    }
    return count;
}

int wifi_saved_forget(const char *name, char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }
    if (!name || !name[0]) {
        if (error && size) {
            snprintf(error, size, "no saved network to forget");
        }
        return -1;
    }

    char quoted[WIFI_TEXT * 2];
    wifi_shell_quote(name, quoted, sizeof(quoted));

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s connection delete %s",
             wifi_shell_program(), quoted);

    static char output[WIFI_TEXT * 4];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not forget the network");
        }
        return -1;
    }
    return 0;
}

int wifi_saved_set_autoconnect(const char *name, int on, char *error,
                               unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }
    if (!name || !name[0]) {
        if (error && size) {
            snprintf(error, size, "no saved network to change");
        }
        return -1;
    }

    char quoted[WIFI_TEXT * 2];
    wifi_shell_quote(name, quoted, sizeof(quoted));

    char command[WIFI_TEXT * 8];
    snprintf(command, sizeof(command), "%s connection modify %s "
             "connection.autoconnect %s",
             wifi_shell_program(), quoted, on ? "yes" : "no");

    static char output[WIFI_TEXT * 4];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not change the setting");
        }
        return -1;
    }
    return 0;
}
