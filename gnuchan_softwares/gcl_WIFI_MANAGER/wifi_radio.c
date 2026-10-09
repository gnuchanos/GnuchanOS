/*
 * wifi_radio.c — reading and setting the wifi switch.
 *
 * nmcli answers the radio's state as one word, "enabled" or "disabled", and
 * sets it with `radio wifi on` or `radio wifi off`. Both go through
 * wifi_shell.c, so the quoting and the running are not repeated here.
 */
#include "wifi_radio.h"

#include <stdio.h>
#include <string.h>

int wifi_radio_on(void) {
    char command[WIFI_TEXT * 4];
    snprintf(command, sizeof(command), "%s -t radio wifi",
             wifi_shell_program());

    static char output[WIFI_TEXT];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        /* nmcli could not be run, so the state is not known. -1 and not 0: a
           caller that guessed "off" would draw a switch the radio is not in,
           and pressing it would then turn the radio ON when the person meant
           to turn it off. */
        return -1;
    }

    /* The one word, without its newline. */
    output[strcspn(output, "\r\n")] = '\0';
    if (strcmp(output, "enabled") == 0) {
        return 1;
    }
    if (strcmp(output, "disabled") == 0) {
        return 0;
    }
    return -1;
}

int wifi_radio_set(int on, char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    char command[WIFI_TEXT * 4];
    snprintf(command, sizeof(command), "%s radio wifi %s",
             wifi_shell_program(), on ? "on" : "off");

    static char output[WIFI_TEXT * 4];
    if (wifi_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not change the radio");
        }
        return -1;
    }
    return 0;
}
