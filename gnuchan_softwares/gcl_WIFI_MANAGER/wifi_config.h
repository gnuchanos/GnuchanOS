/*
 * wifi_config.h — the settings the wifi manager is built from.
 *
 * The file is a plain list of `Name = value` lines, one per line, with `#`
 * starting a comment. It is deliberately NOT the small Python subset the
 * launcher and the window manager read: a wifi manager has a dozen settings and
 * no calls, and a person opening the file to change a colour should see a list
 * of settings rather than a grammar. The reader in wifi_config.c is therefore a
 * few lines long and a person can keep all of it in their head.
 *
 * Everything here is plain data — colours as NAMES, because a name is what a
 * file can hold without a display. Only code with a connection to the server
 * turns a name into a pixel, and that is wifi_style.c.
 *
 *     ~/.config/GnuChanWifi/config.py
 *
 *     NmcliPath   = "nmcli"
 *     FontFamily  = "monospace"
 *     FontSize    = 14
 *     Width       = 520
 *     Rows        = 10
 *     Title       = "Wi-Fi"
 *     Background  = "#1a0b2e"
 *     Panel       = "#32143f"
 *     PanelEdge   = "#7b2cbf"
 *     Field       = "#241033"
 *     Text        = "#e0c3fc"
 *     TextMuted   = "#9d7bba"
 *     Accent      = "#c77dff"
 *     Secured     = "#ff9e64"
 *     Connected   = "#9ece6a"
 */
#ifndef GNUCHANWIFI_CONFIG_H
#define GNUCHANWIFI_CONFIG_H

#include "wifi_shell.h"   /* WIFI_TEXT — the ceiling every field here uses */

typedef struct WifiConfig {
    /* Which nmcli to run. Empty means "nmcli" on PATH. */
    char nmcli[WIFI_TEXT];

    /* The wireless driver's module name, for the "restart wifi" action. Empty
       means "ask the kernel" — wifi_rfkill_driver() reads it off the interface
       — and a name here is only a fallback for a machine where that read gives
       nothing. The Vostro's card is "ath5k"; naming it is harmless anywhere
       else, because the read is tried first. */
    char restart_module[WIFI_TEXT];

    /* The title drawn at the top of the window. */
    char title[WIFI_TEXT];

    /* The font the whole window is drawn in, and its size in pixels. */
    char font_family[WIFI_TEXT];
    int font_size;

    /* The window's width in pixels (clamped to the screen) and how many
       networks are shown at once before the list scrolls. */
    int width;
    int rows;

    /* The palette, as colour names. See wifi_style.c for what each paints. */
    char background[WIFI_TEXT];
    char panel[WIFI_TEXT];
    char panel_edge[WIFI_TEXT];
    char field[WIFI_TEXT];
    char text[WIFI_TEXT];
    char text_muted[WIFI_TEXT];
    char accent[WIFI_TEXT];
    char secured[WIFI_TEXT];      /* the marker on a password-protected net */
    char connected[WIFI_TEXT];    /* the row of the network joined         */
    char selection[WIFI_TEXT];    /* the fill of the row the cursor is on  */

    /* The reason the last read refused, in words to show a person. Empty when
       the file parsed. */
    char error[WIFI_TEXT];
} WifiConfig;

/* The settings a machine with no file gets: the values above, in code, so a
   machine that never had the file still starts. */
void wifi_config_defaults(WifiConfig *config);

/* Read the settings at `path` over the defaults. Returns 0 when the file was
   read, -1 when it could not be opened — in which case config is left at its
   defaults. A line the reader does not understand is skipped, not refused: a
   file is lived in, and a name this manager does not use is a name it does not
   use. */
int wifi_config_load(WifiConfig *config, const char *path);

/* Where the file is looked for: $XDG_CONFIG_HOME/GnuChanWifi/config.py, or
   ~/.config/GnuChanWifi/config.py. Written into buffer, which is returned. */
char *wifi_config_path(char *buffer, unsigned int size);

#endif /* GNUCHANWIFI_CONFIG_H */
