/*
 * ss_config.h — the settings GnuChanSS is built from.
 *
 * GnuChanSS is configured by a script, not by a file of key = value pairs, the
 * same way GnuChanWM and GnuChanTerm are: the same file a person would write to
 * pick the effect or recolour it. That script is Python, and this header is the
 * shape of what running it produces — the small, fixed set of things a screen
 * saver can be asked for.
 *
 * Everything here is plain data. The script is read by ss_parser.c, walked by
 * ss_config.c, and drawn by GnuChanSS.c. Nothing in this header knows about X.
 */
#ifndef GNUCHANSS_CONFIG_H
#define GNUCHANSS_CONFIG_H

/* A written value: a colour, an effect name, a font. Long enough for any of
   them and shared by the parser and the config so one length governs both. */
#define SS_TEXT_LENGTH 256

/* The effects a script may name. Adding an effect is adding one enumerator and
   one branch in the drawing code, exactly as a bar widget is added in the WM.
   The enum is SsEffectKind and not SsEffect, which is the struct an effect's
   running state lives in (ss_effect.h): an enum and a struct of the same name
   cannot both exist, and the two are different things — one is WHICH effect,
   the other is a running one. */
typedef enum SsEffectKind {
    SS_EFFECT_PIPE,      /* the classic pipe show                     */
    SS_EFFECT_3DWALL,    /* the turning wall of bricks                */
} SsEffectKind;

typedef struct SsConfig {
    /* Which effect runs. The script names it: Name="pipe" / Name="3dwall". A
       name that matches nothing is left as the default and said so on stderr,
       because a screen saver that comes up blank is worse than one that comes
       up as the pipe. */
    SsEffectKind effect;

    /* The effect's colours, as written. Text and not pixels because a colour
       name only becomes a pixel once there is a display to allocate it on. */
    char primary[SS_TEXT_LENGTH];
    char background[SS_TEXT_LENGTH];

    /* How long the keyboard and pointer have to be still, in seconds, before
       the saver starts. A value below one is held at one: a saver that runs
       the moment the desk is quiet is a saver nobody can use. */
    int idle_seconds;

    /* Whether the saver should take the org.freedesktop.ScreenSaver name, so
       a program playing a video can inhibit it. On by default; a machine that
       wants no D-Bus at all can turn it off. */
    int inhibit;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[SS_TEXT_LENGTH * 2];
} SsConfig;

/* The saver a machine with no settings file gets: the pipe, in the desktop's
   purple, after five minutes. */
void ss_config_defaults(SsConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which
   case config is left at its defaults and `error` says why. */
int ss_config_load(SsConfig *config, const char *path);

/* Read the settings from the standard place, or the defaults when there is no
   file. Always leaves a usable config. Returns 0 when a file was read. */
int ss_config_load_default(SsConfig *config);

/* Where the script is looked for: $XDG_CONFIG_HOME/GnuChanSS/GnuChanSS.py, or
   ~/.config/GnuChanSS/GnuChanSS.py. Written into buffer, which is returned. */
char *ss_config_path(char *buffer, unsigned int size);

/* The effect a written name stands for, and the name an effect is written as.
   Exposed because both the walker and anything that prints a name need them. */
SsEffectKind ss_effect_of(const char *written);
const char *ss_effect_name(SsEffectKind effect);

#endif /* GNUCHANSS_CONFIG_H */
