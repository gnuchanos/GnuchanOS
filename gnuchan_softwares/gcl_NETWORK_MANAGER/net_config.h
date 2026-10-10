/*
 * net_config.h — the settings the network manager is built from.
 *
 * The file is a plain list of `Name = value` lines, one per line, with `#`
 * starting a comment — the same shape the wifi manager and the launcher read,
 * and the same reason: a manager with a dozen settings and no calls should read
 * as a list of settings rather than a grammar.
 *
 * Everything here is plain data — colours as NAMES, because a name is what a
 * file can hold without a display. Only code with a connection to the server
 * turns a name into a pixel, and that is net_style.c.
 *
 *     ~/.config/GnuChanNetworkManager/config.py
 *
 *     NmcliPath    = "nmcli"
 *     IpPath       = "ip"
 *     ResolvectlPath = "resolvectl"
 *     WifiManager  = "GnuChanWifi"     # the program the wifi button opens
 *     DpiInitPath  = "/opt/zapret/init.d/sysv/zapret"  # zapret's own script
 *     FontFamily   = "monospace"
 *     FontSize     = 14
 *     Width        = 640
 *     RunningRows  = 6
 *     Title        = "Network"
 *     Background   = "#1a0b2e"
 *     Panel        = "#32143f"
 *     PanelEdge    = "#7b2cbf"
 *     Field        = "#241033"
 *     Text         = "#e0c3fc"
 *     TextMuted    = "#9d7bba"
 *     Accent       = "#c77dff"
 *     Connected    = "#9ece6a"
 *     Disabled     = "#ff9e64"
 *     Selection    = "#5a2a8f"
 */
#ifndef GNUCHANNET_CONFIG_H
#define GNUCHANNET_CONFIG_H

#include "net_shell.h"   /* NET_TEXT — the ceiling every field here uses */

typedef struct NetConfig {
    /* Which tools to run. Empty means the bare name on PATH. */
    char nmcli[NET_TEXT];
    char ip[NET_TEXT];
    char resolvectl[NET_TEXT];

    /* Where zapret's own init script lives — the script that starts and stops
       the DPI bypass (see net_dpi.c). zapret's installer puts it under
       /opt/zapret, and that is the default; it is named here because a machine
       that unpacked zapret somewhere else can point at it. */
    char dpi_init[NET_TEXT];

    /* The program the "Open Wi-Fi" button runs. The wifi manager is the one
       program that knows how to join a wireless network; a general network
       manager that reimplemented that would be a second program with its own
       bugs. It is named here so a machine with a different one can point at it,
       and it is started the way every other program is: a command line. */
    char wifi_manager[NET_TEXT];

    /* The title drawn at the top of the window. */
    char title[NET_TEXT];

    /* The font the whole window is drawn in, and its size in pixels. */
    char font_family[NET_TEXT];
    int font_size;

    /* The window's width in pixels (clamped to the screen) and how many
       running-connection rows are shown before that list scrolls. */
    int width;
    int running_rows;

    /* The palette, as colour names. See net_style.c for what each paints. */
    char background[NET_TEXT];
    char panel[NET_TEXT];
    char panel_edge[NET_TEXT];
    char field[NET_TEXT];
    char text[NET_TEXT];
    char text_muted[NET_TEXT];
    char accent[NET_TEXT];
    char connected[NET_TEXT];     /* a device that is up                  */
    char disabled[NET_TEXT];      /* a device that is down or blocked     */
    char selection[NET_TEXT];     /* the fill of the chosen row           */

    /* The reason the last read refused, in words to show a person. Empty when
       the file parsed. */
    char error[NET_TEXT];
} NetConfig;

/* The settings a machine with no file gets: the values above, in code, so a
   machine that never had the file still starts. */
void net_config_defaults(NetConfig *config);

/* Read the settings at `path` over the defaults. Returns 0 when the file was
   read, -1 when it could not be opened — in which case config is left at its
   defaults. A line the reader does not understand is skipped, not refused. */
int net_config_load(NetConfig *config, const char *path);

/* Where the file is looked for:
   $XDG_CONFIG_HOME/GnuChanNetworkManager/config.py, or
   ~/.config/GnuChanNetworkManager/config.py. Written into buffer, returned. */
char *net_config_path(char *buffer, unsigned int size);

#endif /* GNUCHANNET_CONFIG_H */
