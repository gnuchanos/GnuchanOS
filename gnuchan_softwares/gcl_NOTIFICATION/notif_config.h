/*
 * notif_config.h — the settings GnuChanNotification is built from.
 *
 * GnuChanNotification is configured by a script, not by a file of key = value
 * pairs, the same way GnuChanWM, GnuChanDock, GnuChanSS and GnuChanFetch are:
 * the same file a person would write to recolour the bubbles, move them to
 * another corner, change how long they stay, or turn their icons off. That
 * script is Python, and this header is the shape of what running it produces —
 * the small, fixed set of things a notification daemon can be asked for.
 *
 * Everything here is plain data. The script is read by notif_parser.c, walked
 * by notif_config.c, and drawn by notif_render.c. Nothing in this header knows
 * about X or about D-Bus.
 */
#ifndef GNUCHANNOTIFICATION_CONFIG_H
#define GNUCHANNOTIFICATION_CONFIG_H

/* A written value: a colour, a path, a command, a font. Long enough for any of
   them and shared by the parser and the config so one length governs both. */
#define NOTIF_TEXT_LENGTH 256

/* The most bubbles that may be on screen at once. Beyond this the oldest is
   dropped: a stack taller than this is a stack nobody reads, and the number
   also sizes the arrays that hold the live notifications. */
#define NOTIF_MAX_VISIBLE 32

/* The most a single notification's body text may hold before it is trimmed for
   display. The protocol allows far more; a bubble shows far less. */
#define NOTIF_MAX_BODY 4096

typedef struct NotifConfig {
    /* --- the palette, as written ------------------------------------------
     *
     * Text and not pixels: a colour name only becomes a pixel once there is a
     * display to allocate it on. The defaults are the desktop's own purple
     * ramp — the same colours GnuChanWM, GnuChanDock and GnuChanFetch use — so
     * a notification looks like the rest of the session out of the box. */
    char background[NOTIF_TEXT_LENGTH];      /* a bubble's body             */
    char background_alt[NOTIF_TEXT_LENGTH];  /* a bubble's second body band */
    char frame[NOTIF_TEXT_LENGTH];           /* the line around a bubble    */
    char title[NOTIF_TEXT_LENGTH];           /* the summary line            */
    char body[NOTIF_TEXT_LENGTH];            /* the body text               */
    char accent[NOTIF_TEXT_LENGTH];          /* links and the hover edge    */
    char urgent[NOTIF_TEXT_LENGTH];          /* the frame of an urgent one  */
    char icon_background[NOTIF_TEXT_LENGTH]; /* behind a see-through icon   */

    /* --- the shape --------------------------------------------------------
     *
     * width is the bubble's fixed width in pixels; the height follows from the
     * text it holds. padding is the room inside the bubble before the text,
     * margin how far the stack sits from the screen edge, gap the space
     * between two bubbles, corner the radius of the rounded corners, and
     * icon_size the side a picture is drawn at. */
    int width;
    int padding;
    int margin;
    int gap;
    int corner;
    int icon_size;

    /* --- the label --------------------------------------------------------
     *
     * The font the text is drawn in, and the two sizes the two lines share it
     * at: the summary is drawn at `font`, the body a size smaller. A body as
     * large as its title is a body nobody can tell from a title. */
    char font[NOTIF_TEXT_LENGTH];
    int body_scale;   /* percent of the font's size for the body line */

    /* --- the behaviour ----------------------------------------------------
     *
     * how long a notification stays, in milliseconds, when the program that
     * sent it asked for no particular time (a program may ask for its own).
     * max_visible is the most bubbles shown at once. position is where the
     * stack sits: "top-right", "top-left", "bottom-right", "bottom-left",
     * "top-center", "bottom-center". show_icon and show_body turn the two
     * lines of a bubble on and off. */
    char position[NOTIF_TEXT_LENGTH];
    int timeout;
    int max_visible;
    int show_icon;
    int show_body;

    /* How far below the top edge the stack begins when it sits at a top
       corner. The desktop's own bar is along the top, and a bubble placed at
       the very top would be half-hidden under it; this is the bar's own height
       plus the air under it, and the settings default it to 50. */
    int top_offset;

    /* Whether a thin countdown bar is drawn along a bubble's bottom edge,
       shrinking as its time runs out. A bubble that will never expire has no
       bar to draw, because there is nothing counting down. */
    int show_timer;

    /* The tallest the summary and the body are allowed to grow before they are
       cut off. The bubble is sized to its text, so these are only a ceiling
       against a runaway body making a bubble taller than the screen. */
    int max_summary_lines;
    int max_body_lines;

    /* The reason the last read refused, in the words to show a person. Empty
       when the file parsed. */
    char error[NOTIF_TEXT_LENGTH * 2];
} NotifConfig;

/* The daemon a machine with no settings file gets: the purple palette, a stack
   in the top-right corner, and icons on. */
void notif_config_defaults(NotifConfig *config);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed — in which
   case config is left at its defaults and `error` says why. */
int notif_config_load(NotifConfig *config, const char *path);

/* Read the settings from the standard place, or the defaults when there is no
   file. Always leaves a usable config. Returns 0 when a file was read. */
int notif_config_load_default(NotifConfig *config);

/* Where the script is looked for:
   $XDG_CONFIG_HOME/GnuChanNotification/GnuChanNotification.py, or
   ~/.config/GnuChanNotification/GnuChanNotification.py. Written into buffer,
   returned. */
char *notif_config_path(char *buffer, unsigned int size);

/* A written colour resolved to a pixel on this display. `display_pointer` is a
   Display * passed as void so this header stays free of X. Returns `fallback`
   when the name is empty or the server has no such colour. */
unsigned long notif_config_colour(void *display_pointer, int screen,
                                  const char *name, unsigned long fallback);

#endif /* GNUCHANNOTIFICATION_CONFIG_H */
