/*
 * runner_config.h — the settings GnuChanRunner is built from.
 *
 * GnuChanRunner is configured by a script, not by a file of key = value pairs,
 * the way GnuChanWM is: the same file a person would write to change a colour
 * or add a program of their own. That script is Python, and this header is the
 * shape of what running it produces — the small, fixed set of things a
 * launcher can be asked for.
 *
 * Everything here is plain data. The script is read by runner_parser.c, walked
 * by runner_config.c, and drawn by runner_draw.c. Nothing in this header knows
 * about X.
 */
#ifndef GNUCHANRUNNER_CONFIG_H
#define GNUCHANRUNNER_CONFIG_H

#include "runner_parser.h"

/* How many programs a script may add by hand, and how many .desktop files the
   scan will take. The ceiling on the scan is what keeps a machine with a
   thousand stale entries from making the search slow: the launcher is for
   starting the programs a person uses, and a list that long is not that. */
#define RUNNER_MAX_EXTRA_PROGRAMS 64

/* The .desktop entries the scan will hold. Read off /usr/share/applications
   and the user's own directory; far more than a desktop has room to show, so
   the ceiling is a bound on memory and not on the list a person sees. */
#define RUNNER_MAX_DESKTOP_ENTRIES 512

/* The directories the scan reads when the script names none. These are the
   two the freedesktop specification puts an installed program's entry in: one
   for the system's packages and one for what the user installed themselves. */
#define RUNNER_DEFAULT_SYSTEM_DIRS "/usr/share/applications"

/* The colour and font names a widget may carry. Long enough for a colour like
   "#c77dff" and a font like "monospace:pixelsize=12", which are the two kinds
   of text this file holds. */
typedef enum RunnerInputMode {
    RUNNER_MODE_DESKTOP,   /* the list of installed programs, as read */
    RUNNER_MODE_COMMAND,   /* whatever is typed, run as a command line */
} RunnerInputMode;

/* When the second mode is chosen. rofi's own "run" mode is entered by typing
   it; this is the same idea, kept as a setting because a machine that wants
   the command line always available should be able to say so. */
typedef enum RunnerCommandMode {
    RUNNER_COMMAND_OFF,      /* only installed programs are listed  */
    RUNNER_COMMAND_TYPED,    /* a command line is offered once nothing
                                matches what was typed               */
    RUNNER_COMMAND_ALWAYS,   /* a command line is offered on every query */
} RunnerCommandMode;

typedef struct RunnerConfig {
    /* The word shown before what is being typed. */
    char prompt[RUNNER_TEXT_LENGTH];

    /* The line shown when nothing has been typed yet, and the one shown when
       what was typed matches nothing. */
    char placeholder[RUNNER_TEXT_LENGTH];
    char empty_message[RUNNER_TEXT_LENGTH];

    /* The font the whole window is drawn in, and the size of it. A family
       without a size uses the launcher's own. */
    char font_family[RUNNER_TEXT_LENGTH];
    int font_size;

    /* The window's size. `width` is in pixels and is clamped to the screen;
       `rows` is how many matches are shown at once before the list is
       scrolled by the arrow keys. */
    int width;
    int rows;

    /* Where the window sits: "center", "top" or "bottom". Anything else is
       read as "center". */
    char position[RUNNER_TEXT_LENGTH];

    /* The palette. Each is a colour name resolved against the default when it
       is empty, exactly as the bar's colours are. */
    char background[RUNNER_TEXT_LENGTH];
    char panel[RUNNER_TEXT_LENGTH];
    char panel_edge[RUNNER_TEXT_LENGTH];
    char field[RUNNER_TEXT_LENGTH];
    char text[RUNNER_TEXT_LENGTH];
    char text_muted[RUNNER_TEXT_LENGTH];
    char accent[RUNNER_TEXT_LENGTH];

    /* Whether a capital letter makes a match, and whether "xte" finds
       "xterm" — the two things that decide what typing narrows the list to.
       Both default to the forgiving answer, because a launcher that misses a
       program because of a capital letter is worse than one that shows two
       extra lines. */
    int case_sensitive;
    int fuzzy;

    /* Whether the command line is offered, and how often. See
       RunnerCommandMode. */
    RunnerCommandMode command_mode;

    /* The directories the scan reads. Empty means the default above. */
    char desktop_dirs[RUNNER_MAX_LIST_ITEMS][RUNNER_TEXT_LENGTH];
    int desktop_dir_count;

    /* Programs the script adds itself, beyond the ones installed. Each is a
       name and the command to run, and they are searched with the rest. This
       is what a person writes for a script they keep in their own home, which
       has no .desktop file anywhere. */
    struct {
        char name[RUNNER_TEXT_LENGTH];
        char command[RUNNER_TEXT_LENGTH];
    } extra_programs[RUNNER_MAX_EXTRA_PROGRAMS];
    int extra_program_count;

    /* The .desktop keys whose entries are left out: NoDisplay=true and
       Hidden=true, by default, which are the two the specification uses for
       "installed but not to be shown". A script that wants them listed can
       clear this. */
    int show_no_display;
    int show_hidden;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[RUNNER_TEXT_LENGTH * 2];
} RunnerConfig;

/* The launcher a machine with no settings file gets: the prompt, the palette
   and the behaviour the shipped config.py describes, all in code, so a machine
   that never had the file still starts. */
void runner_config_defaults(RunnerConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which
   case config is left at its defaults and `error` says why. */
int runner_config_load(RunnerConfig *config, const char *path);

/* Where the script is looked for: $XDG_CONFIG_HOME/GnuChanRunner/config.py, or
   ~/.config/GnuChanRunner/config.py. Written into buffer, which is returned. */
char *runner_config_path(char *buffer, unsigned int size);

#endif /* GNUCHANRUNNER_CONFIG_H */
