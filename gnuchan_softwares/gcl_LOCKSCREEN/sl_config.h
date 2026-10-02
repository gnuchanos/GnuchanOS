/*
 * sl_config.h — the settings GnuChanSL is built from.
 *
 * GnuChanSL is configured by a script, not by a file of key = value pairs, the
 * same way GnuChanWM and GnuChanSS are: the same file a person would write to
 * change a colour or the message on the screen. That script is Python, and this
 * header is the shape of what running it produces — the small, fixed set of
 * things a lock screen can be asked for.
 *
 * Everything here is plain data. The script is read by sl_parser.c, walked by
 * sl_config.c, and drawn by GnuChanSL.c. Nothing in this header knows about X.
 */
#ifndef GNUCHANSL_CONFIG_H
#define GNUCHANSL_CONFIG_H

/* A written value: a colour, a font, a message. Long enough for any of them and
   shared by the parser and the config so one length governs both. */
#define SL_TEXT_LENGTH 256

typedef struct SlConfig {
    /* The palette, as written. Text and not pixels because a colour name only
       becomes a pixel once there is a display to allocate it on. */
    char background[SL_TEXT_LENGTH];
    char panel[SL_TEXT_LENGTH];
    char panel_edge[SL_TEXT_LENGTH];
    char field[SL_TEXT_LENGTH];
    char text[SL_TEXT_LENGTH];
    char text_muted[SL_TEXT_LENGTH];
    char accent[SL_TEXT_LENGTH];
    /* The colour a wrong password flashes, and the one a right one does. */
    char wrong[SL_TEXT_LENGTH];

    /* The font the whole screen is drawn in, and the size of it. A family
       without a size uses the lock screen's own. */
    char font_family[SL_TEXT_LENGTH];
    int font_size;

    /* The heading and the line under it. Defaults name the user and ask for the
       password; a machine that wants its own words sets them here. */
    char title[SL_TEXT_LENGTH];
    char subtitle[SL_TEXT_LENGTH];
    char prompt[SL_TEXT_LENGTH];

    /* The strftime format the clock is drawn in, or empty for no clock. */
    char clock_format[SL_TEXT_LENGTH];

    /* Whether the password field draws one dot per typed character. On by
       default; a machine away from shoulders can turn it off. */
    int show_password_dots;

    /* How long a wrong password's message and colour stay up, in seconds,
       before the field is cleared ready for the next try. */
    int wrong_seconds;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[SL_TEXT_LENGTH * 2];
} SlConfig;

/* The lock screen a machine with no settings file gets: the desktop's purple,
   the user's name, and a clock. */
void sl_config_defaults(SlConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which case
   config is left at its defaults and `error` says why. */
int sl_config_load(SlConfig *config, const char *path);

/* Read the settings from the standard place, or the defaults when there is no
   file. Always leaves a usable config. Returns 0 when a file was read. */
int sl_config_load_default(SlConfig *config);

/* Where the script is looked for: $XDG_CONFIG_HOME/GnuChanSL/GnuChanSL.py, or
   ~/.config/GnuChanSL/GnuChanSL.py. Written into buffer, which is returned. */
char *sl_config_path(char *buffer, unsigned int size);

#endif /* GNUCHANSL_CONFIG_H */
