/*
 * wifi_manager.c — the entry point.
 *
 * The manager is one window and one purpose: see the wireless state, pick a
 * network, join it, and manage the ones already saved. Everything it is made of
 * is in the files beside this one — the settings by wifi_config.c, the shell by
 * wifi_shell.c, the networks by wifi_nm.c, the radio by wifi_radio.c, the
 * profiles by wifi_saved.c, the window by wifi_ui.c — and what is left for this
 * file is the order to call them in and what to do with the answer.
 *
 *     GnuChanWifi            open the manager
 *     GnuChanWifi --config F read the settings at F
 *     GnuChanWifi --list     print the wireless state and the networks, exit
 *     GnuChanWifi --version  print the version and exit
 *
 * --list is the one thing to run without a window, and it is there on purpose:
 * a manager that will not open a window on a machine with a broken driver is one
 * whose diagnosis is a guess. --list prints the radio, the interface, what is
 * connected and the networks in range, which is the same picture the window
 * shows, as text.
 */
#include <stdio.h>
#include <string.h>

#include "wifi_config.h"
#include "wifi_nm.h"
#include "wifi_privilege.h"
#include "wifi_radio.h"
#include "wifi_rfkill.h"
#include "wifi_saved.h"
#include "wifi_ui.h"

#define GNUCHANWIFI_VERSION "0.1.0"

/* Print the wireless state and every network in range, as text. */
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
    wifi_shell_set_program(config.nmcli);

    int radio = wifi_radio_on();
    printf("radio: %s\n",
           radio == 1 ? "on" : (radio == 0 ? "off" : "unknown"));

    /* The kernel's own blocks, which are the reason an "on" radio can still
       find nothing: the report says so in words rather than leaving a person to
       wonder. */
    WifiBlock block;
    if (wifi_rfkill_state(&block) == 0) {
        if (block.hard) {
            printf("blocked: hardware switch (flip it, then restart)\n");
        } else if (block.soft) {
            printf("blocked: soft (the Restart action clears it)\n");
        } else {
            printf("blocked: no\n");
        }
    }

    char device[WIFI_TEXT];
    wifi_nm_device(device, sizeof(device));
    printf("interface: %s\n", device[0] ? device : "(none)");
    if (device[0] == '\0') {
        return 0;
    }

    char active[WIFI_TEXT];
    char address[WIFI_TEXT];
    if (wifi_nm_active(device, active, sizeof(active),
                       address, sizeof(address)) == 0) {
        printf("connected: %s%s%s\n", active,
               address[0] ? "  " : "", address);
    } else {
        printf("connected: (nothing)\n");
    }

    WifiSavedList saved;
    wifi_saved_load(&saved);
    printf("saved profiles: %d\n", saved.count);

    if (radio == 0) {
        return 0;
    }

    char names[WIFI_MAX_SAVED][WIFI_TEXT];
    int name_count = wifi_saved_names(&saved, names, WIFI_MAX_SAVED);

    WifiList list;
    if (wifi_nm_scan(&list, device, names, name_count) != 0) {
        fprintf(stderr, "gnuchanwifi: nmcli could not be run\n");
        return 1;
    }

    for (int i = 0; i < list.count; i++) {
        WifiNetwork *network = &list.items[i];
        printf("%-32s  %3d%%  %-7s%s%s\n",
               network->ssid,
               network->signal,
               network->secured ? "secured" : "open",
               network->saved ? "  [saved]" : "",
               network->in_use ? "  [connected]" : "");
    }
    fprintf(stderr, "gnuchanwifi: %d network(s)\n", list.count);
    return 0;
}

static void print_help(void) {
    printf("usage: GnuChanWifi [--config FILE] [--list] [--version] [--help]\n"
           "\n"
           "Opens the wifi manager: the wireless state at the top, the\n"
           "networks in range below. Click one to choose it, then press\n"
           "Connect (or Enter) to join it.\n"
           "\n"
           "  Up / Down     move through the networks\n"
           "  Enter         join the chosen network\n"
           "  r             scan again\n"
           "  Escape        close\n"
           "\n"
           "The buttons do the same: Connect, Forget, Rescan, Restart.\n"
           "Forget drops a saved network's profile; Restart unblocks the\n"
           "radio and reloads the wireless driver (the Vostro fix).\n"
           "\n"
           "  --config FILE  read the settings at FILE instead of the one\n"
           "                 under ~/.config/GnuChanWifi/\n"
           "  --list         print the state and the networks and exit\n"
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

    /* The settings are read here — before the password window — because that
       window is drawn in them, so the one thing a person sees before the
       manager opens looks like the manager. */
    WifiConfig config;
    char found[WIFI_TEXT * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = wifi_config_path(found, sizeof(found));
    }
    if (wifi_config_load(&config, path) != 0) {
        fprintf(stderr, "gnuchanwifi: %s; using the defaults\n", config.error);
    }

    /* Opening the window is what needs root: the radio, the driver reload and
       the kill switches are all root's to change, and a window that half-works
       because each action is refused is worse than asking for the password
       once. wifi_privilege_ensure() shows the password window, re-runs this
       program through sudo, and exits with the copy; a machine with no display
       or no sudo carries on here and the privileged actions simply report that
       they failed. */
    if (wifi_privilege_ensure(argc, argv, &config) != 0) {
        fprintf(stderr,
                "gnuchanwifi: carrying on without root; the radio and the "
                "driver cannot be changed\n");
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
