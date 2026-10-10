/*
 * dock_config.h — the settings GnuChanDock is built from.
 *
 * GnuChanDock is configured by a script, not by a file of key = value pairs,
 * the same way GnuChanWM, GnuChanSS, GnuChanTerm and GnuChanFetch are: the
 * same file a person would write to recolour the dock, resize its icons, or
 * point it at a different terminal. That script is Python, and this header is
 * the shape of what running it produces — the small, fixed set of things a
 * dock can be asked for.
 *
 * Everything here is plain data. The script is read by dock_parser.c, walked
 * by dock_config.c, and drawn by dock_draw.c. Nothing in this header knows
 * about X.
 */
#ifndef GNUCHANDOCK_CONFIG_H
#define GNUCHANDOCK_CONFIG_H

/* A written value: a colour, a path, a command, a label. Long enough for any
   of them and shared by the parser and the config so one length governs
   both. */
#define DOCK_TEXT_LENGTH 256

/* The most slots the dock may ever hold: the two fixed icons and the running
   windows together. A desktop with more open windows than this has a dock
   that is already wider than the screen. */
#define DOCK_MAX_ITEMS 48

typedef struct DockConfig {
    /* --- the palette, as written ------------------------------------------
     *
     * Text and not pixels: a colour name only becomes a pixel once there is a
     * display to allocate it on. The defaults are the desktop's own purple
     * ramp — the same colours GnuChanWM and GnuChanFetch use — so the dock
     * looks like the rest of the session out of the box. */
    char background[DOCK_TEXT_LENGTH];       /* the dock's body            */
    char background_edge[DOCK_TEXT_LENGTH];  /* the line around it         */
    char field[DOCK_TEXT_LENGTH];            /* a hovered icon's plate     */
    char text[DOCK_TEXT_LENGTH];             /* the label under an icon    */
    char accent[DOCK_TEXT_LENGTH];           /* the hovered icon's label   */
    char badge[DOCK_TEXT_LENGTH];            /* a group's open count       */

    /* --- the shape --------------------------------------------------------
     *
     * icon_size is the resting side of one icon in pixels; magnify is the
     * extra side the icon under the pointer grows by (see dock_draw.c). gap
     * is the space between two slots, padding the space inside the dock
     * before the first icon and after the last, and corner the radius of the
     * rounded corners. margin is how far the dock's near edge sits from the
     * screen edge.
     *
     * A value below its floor is refused by the walk and the default kept,
     * because the layout arithmetic divides by and adds these. */
    int icon_size;
    int gap;
    int padding;
    int margin;
    int corner;

    /* --- magnification ----------------------------------------------------
     *
     * Whether an icon grows as the pointer passes over it, which is the one
     * thing that makes a row of icons a dock rather than a strip. magnify is
     * the extra pixels the icon directly under the pointer gains; reach is
     * how many slots away the effect is felt, counted in icons. */
    int magnify;
    int magnify_reach;

    /* --- the label --------------------------------------------------------
     *
     * The font the labels are drawn in, and whether a label is drawn under an
     * icon at all. The label is the icon's own name: the terminal's is
     * `terminal_label`, a running window's is its title. */
    char font[DOCK_TEXT_LENGTH];
    int label_enabled;

    /* --- the two fixed icons ----------------------------------------------
     *
     * The settings icon is the first, always, and it opens GnuChanSettings
     * when it is clicked. The terminal icon is the second and it runs the
     * terminal. Both carry their own command, so either can be pointed at a
     * binary by full path on a session whose PATH does not reach them. */
    int settings_enabled;
    char settings_icon[DOCK_TEXT_LENGTH];
    char settings_label[DOCK_TEXT_LENGTH];
    /* The program the settings icon runs. It is named the same way the
       terminal is — a command on PATH, or a full path to one — so a session
       whose PATH does not carry the install directory can still point the
       icon at the binary rather than watching the click do nothing. */
    char settings_command[DOCK_TEXT_LENGTH];

    int terminal_enabled;
    char terminal_icon[DOCK_TEXT_LENGTH];
    char terminal_label[DOCK_TEXT_LENGTH];
    char terminal_command[DOCK_TEXT_LENGTH];

    /* --- the running programs ---------------------------------------------
     *
     * Whether the dock lists the windows that are open. The list is read from
     * the window manager's _NET_CLIENT_LIST, which is the extended hints' own
     * answer to "what is open" and the reason this dock does not have to be
     * part of the window manager to know. */
    int show_running;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[DOCK_TEXT_LENGTH * 2];
} DockConfig;

/* The dock a machine with no settings file gets: the purple palette, the
   shipped gear and logo, and the terminal the desktop names. */
void dock_config_defaults(DockConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which
   case config is left at its defaults and `error` says why. */
int dock_config_load(DockConfig *config, const char *path);

/* Read the settings from the standard place, or the defaults when there is no
   file. Always leaves a usable config. Returns 0 when a file was read. */
int dock_config_load_default(DockConfig *config);

/* Where the script is looked for: $XDG_CONFIG_HOME/GnuChanDock/GnuChanDock.py,
   or ~/.config/GnuChanDock/GnuChanDock.py. Written into buffer, returned. */
char *dock_config_path(char *buffer, unsigned int size);

/* A written colour resolved to a pixel on this display. `display_pointer` is a
   Display * passed as void so this header stays free of X. Returns `fallback`
   when the name is empty or the server has no such colour. */
unsigned long dock_config_colour(void *display_pointer, int screen,
                                 const char *name, unsigned long fallback);

#endif /* GNUCHANDOCK_CONFIG_H */
