/*
 * dm_config.h — the settings the login screen is built from.
 *
 * GnuChanDM is configured by a script and not by a file of key = value pairs,
 * the same way GnuChanWM and GnuChanTerm are. The script is Python — or rather
 * the part of Python a person writes to describe a login screen — and this
 * header is the shape of what reading it produces: the small, fixed set of
 * things a greeter can be asked for.
 *
 * --- the order, and why it is the whole point ---
 *
 *     dm_config_load_default()   the script, read into plain data
 *     dm_style_load(..., config) the palette, the fonts and the sizes from it
 *     dm_core_init()             the window, sized and coloured by that style
 *
 * The config is read BEFORE the style is built, and the style before the
 * window. A setting read after the window existed could not set the font the
 * window's text is drawn in, and a login screen whose font only changed on the
 * next redraw would be one that resized its own controls under the pointer.
 *
 * Everything here is plain data and nothing here knows about X. That is what
 * lets the read happen before there is a display: the file is parsed into this
 * struct, and the X names are resolved later by dm_style.c, which is the only
 * thing that can.
 *
 * --- what is configurable, and what is not ---
 *
 * What is here is what the drawing code can actually honour today: every
 * colour the greeter draws with, its three fonts, and its five measurements.
 * What is not here is written down at the end of the struct, with the reason,
 * so that a person reading this file knows what the script cannot yet say
 * rather than finding out by writing it and watching nothing happen.
 */
#ifndef GNUCHANDM_CONFIG_H
#define GNUCHANDM_CONFIG_H

/* A written value: a colour, a font name. */
#define DM_CONFIG_TEXT_LENGTH 256

/* The greeter's settings, as written.
 *
 * A colour is the TEXT of a colour — "#c77dff" — and not a pixel, because a
 * colour name only becomes a pixel once there is a display to allocate it on.
 * The same is true of a font. Empty means "the script did not name this one",
 * which is what keeps every setting optional: a script that names three
 * colours keeps the built-in other seven, and one that says nothing keeps the
 * login screen exactly as it shipped.
 *
 * That is the difference between a settings file and a whole theme, and it is
 * why every field here is a string that may be empty rather than a value with
 * a default written into it. The defaults live in dm_style.c, in one place,
 * where the drawing code that uses them can see them.
 */
typedef struct DmConfig {
    /* --- the palette ------------------------------------------------------
     *
     * The ten colours the greeter draws with, named for what they are rather
     * than for a number, because a login screen has no "ANSI colour 3": every
     * one of these is chosen for one surface and is read in one place.
     *
     * They are named as they are drawn. `background` is the whole screen
     * behind the panel, `panel` the login box itself, `panel_edge` the line
     * around it, `field` an input box and `field_focus` the one being typed
     * in, `text` what the user reads and `text_muted` the labels and hints,
     * `accent` the sign-in button and the focus ring, `accent_dim` the button
     * when it is idle, and `danger` an error message. */
    char background[DM_CONFIG_TEXT_LENGTH];
    char panel[DM_CONFIG_TEXT_LENGTH];
    char panel_edge[DM_CONFIG_TEXT_LENGTH];
    char field[DM_CONFIG_TEXT_LENGTH];
    char field_focus[DM_CONFIG_TEXT_LENGTH];
    char text[DM_CONFIG_TEXT_LENGTH];
    char text_muted[DM_CONFIG_TEXT_LENGTH];
    char accent[DM_CONFIG_TEXT_LENGTH];
    char accent_dim[DM_CONFIG_TEXT_LENGTH];
    char danger[DM_CONFIG_TEXT_LENGTH];

    /* --- the fonts --------------------------------------------------------
     *
     * An X core font name, which is what this greeter draws with: the fields
     * are XFontStruct and the drawing is XDrawString, so the name is the
     * server's own pattern and not an Xft description.
     *
     * A name the server cannot open is not fatal. dm_style.c tries the one
     * written here FIRST and then the built-in list, so a machine whose
     * configured font is missing still draws its login screen in a font it
     * has — a greeter that refused to start over a font would be a machine
     * nobody can log into. */
    char font_label[DM_CONFIG_TEXT_LENGTH];   /* labels, buttons   */
    char font_field[DM_CONFIG_TEXT_LENGTH];   /* what is typed in  */
    char font_title[DM_CONFIG_TEXT_LENGTH];   /* the host name     */

    /* --- the measurements -------------------------------------------------
     *
     * Pixels, and zero means "not written" — a login panel with no margin is
     * not a thing anyone asks for, so zero is free to mean unset and the
     * built-in value is kept. A script that gives one of these a number has
     * to give it a usable one; the reader refuses a negative rather than
     * drawing a box inside out. */
    int margin;        /* space inside the panel and the window */
    int gap;           /* space between two stacked controls    */
    int panel_width;   /* the login panel's width               */
    int field_height;  /* the height of one input field         */
    int button_height; /* the height of one button              */

    /* --- what is NOT here, and why ----------------------------------------
     *
     * The session to start, the host name above the panel, and whether the
     * power buttons are shown at all are three things a person might
     * reasonably want to set, and none of them is in this struct.
     *
     * They are not in it because the code behind them is not there yet: the
     * session is whatever the user picks from the list the greeter scans out
     * of /usr/share/xsessions, the host name is read from the machine, and
     * the two power buttons are always drawn. A setting that is read and then
     * quietly ignored is worse than a setting that is not offered — the first
     * is a lie the person who wrote the script has no way to find out about —
     * so they go in when the code behind them does.
     */

    /* --- something that could not be given --------------------------------
     *
     * In the words to show the person who wrote it: a colour that is not a
     * colour, a measurement that makes no sense, an unknown setting. It is
     * carried here rather than printed where it is found because the read may
     * happen before there is anywhere to print it, and because a greeter's
     * output goes to a log nobody reads until something else has gone wrong.
     *
     * Empty when there is nothing to say, which is the usual case. */
    char notes[DM_CONFIG_TEXT_LENGTH * 2];
} DmConfig;

/* The login screen the code had before it was configurable: the palette in
   dm_style.c, its fonts and its measurements. A machine whose config is
   missing or broken gets exactly this. */
void dm_config_defaults(DmConfig *config);

/* Where the script is looked for, FIRST PLACE THAT HAS ONE WINNING:

     $GCL_DM_CONFIG                         a file named outright
     $XDG_CONFIG_HOME/GnuChanDM/GnuChanDM.py
     ~/.config/GnuChanDM/GnuChanDM.py
     GnuChanDM_config/GnuChanDM.py          the copy the source tree ships

   The user's own ~/.config comes first of the fixed places, which is where the
   installer writes and where a login screen's settings belong. The tree's copy
   is relative to the working directory and is what lets a build run straight
   from the tree read the settings the tree ships, instead of falling back to
   the built-in palette. It is LAST so a machine with settings of its own never
   has the tree's file in front of it.

   Written into buffer, which is returned. `size` is how much room there is. */
char *dm_config_path(char *buffer, unsigned int size);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed far enough to
   trust — a half-read config is never applied.
 *
 * A missing file is NOT an error: it returns 0 and leaves the defaults, which
 * is what a machine that has never been configured must get. A broken file is
 * an error, and the reason is put in config->notes. */
int dm_config_load(DmConfig *config, const char *path);

/* Read the script from the usual place, on top of the built-in defaults.
   Returns 0 when a script was read and -1 when none was found — which is not
   a failure and is what a freshly installed machine gets. */
int dm_config_load_default(DmConfig *config);

#endif /* GNUCHANDM_CONFIG_H */
