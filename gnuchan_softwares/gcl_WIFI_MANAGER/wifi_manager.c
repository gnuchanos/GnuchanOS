/*
 * wifi_manager.c — the entry point.
 *
 * The manager is one window and one purpose: pick a wireless network, give it a
 * password if it needs one, and join it. Everything it is made of is in the
 * files beside this one — the settings are read by wifi_config.c, the networks
 * by wifi_nm.c, the palette by wifi_style.c, the window by wifi_ui.c — and what
 * is left for this file is the order to call them in and what to do with the
 * answer.
 *
 *     GnuChanWifi            open the manager
 *     GnuChanWifi --config F read the settings at F
 *     GnuChanWifi --list     print the networks in range and exit
 *     GnuChanWifi --version  print the version and exit
 *
 * It is started the way any program is: from a key binding in GnuChanWM's own
 * settings script, or from a terminal. It knows nothing about the window manager
 * and does not need one: the display is the only thing it asks for.
 *
 * --list is the one thing to run without a window, and it is there on purpose:
 * a wifi manager that will not open a window on a machine with a broken driver
 * is one whose diagnosis is a guess. --list prints what nmcli reported, which
 * is the same list the window would show, and turns "no networks" from a
 * mystery into a line of output.
 */
#include <stdio.h>
#include <string.h>

#include "wifi_config.h"
#include "wifi_nm.h"
#include "wifi_ui.h"

#define GNUCHANWIFI_VERSION "0.1.0"

/* Print every network in range, one per line, in the form
   "SSID -- signal% (secured|open) [connected]". This is what --list is for: the
   one way to see what the manager will offer without opening a window. */
static int list_networks(const char *config_path) {
    WifiConfig config;
    char found[WIFI_TEXT * 2];
    const char *path = config_path;

    if (!path || !path[0]) {
        path = wifi_config_path(found, sizeof(found));
    }
    if (wifi_config_load(&config, path) != 0) {
        fprintf(stderr, "gnuchanwifi: %s; using the defaults\n", config.error);
    }
    wifi_nm_set_program(config.nmcli);

    char device[WIFI_TEXT];
    wifi_nm_device(device, sizeof(device));
    printf("interface: %s\n", device[0] ? device : "(none)");
    if (device[0] == '\0') {
        return 0;
    }

    WifiList list;
    if (wifi_nm_scan(&list) != 0) {
        fprintf(stderr, "gnuchanwifi: nmcli could not be run\n");
        return 1;
    }

    for (int i = 0; i < list.count; i++) {
        WifiNetwork *network = &list.items[i];
        printf("%-32s  %3d%%  %-7s%s\n",
               network->ssid,
               network->signal,
               network->secured ? "secured" : "open",
               network->in_use ? "  [connected]" : "");
    }
    fprintf(stderr, "gnuchanwifi: %d network(s)\n", list.count);
    return 0;
}

static void print_help(void) {
    printf("usage: GnuChanWifi [--config FILE] [--list] [--version] [--help]\n"
           "\n"
           "Opens the wifi manager: choose a network with the arrow keys,\n"
           "press Enter to join it, type the password when one is asked for.\n"
           "\n"
           "  Up / Down     move through the networks\n"
           "  Enter         join the chosen network\n"
           "  r             scan again\n"
           "  d             disconnect\n"
           "  q / Escape    close\n"
           "\n"
           "  --config FILE  read the settings at FILE instead of the one\n"
           "                 under ~/.config/GnuChanWifi/\n"
           "  --list         print the networks in range and exit\n"
           "  --version      print the version and exit\n");
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    int list_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanWifi %s\n", GNUCHANWIFI_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--list") == 0) {
            list_only = 1;
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "gnuchanwifi: --config needs a file name\n");
                return 2;
            }
            config_path = argv[++i];
            continue;
        }
        fprintf(stderr, "gnuchanwifi: unknown option '%s'\n", argv[i]);
        print_help();
        return 2;
    }

    if (list_only) {
        return list_networks(config_path);
    }

    WifiUi ui;
    if (wifi_ui_open(&ui, config_path) != 0) {
        wifi_ui_close(&ui);
        return 1;
    }
    wifi_ui_run(&ui);
    wifi_ui_close(&ui);
    return 0;
}
