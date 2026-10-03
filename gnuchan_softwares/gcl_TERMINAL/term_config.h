/*
 * term_config.h — the settings the terminal is built from.
 *
 * GnuChanTerm is configured by a script and not by a file of key = value
 * pairs, the same way GnuChanWM is. The script is Python — or rather the part
 * of Python a person writes to describe a terminal — and this header is the
 * shape of what reading it produces: the small, fixed set of things a terminal
 * can be asked for.
 *
 * Everything here is plain data and nothing here knows about X. That is
 * deliberate and it is what makes the file order work: the config is read
 * BEFORE there is a display, because the font it names decides the cell size
 * and the cell size decides how big the window has to be. A config that could
 * only be read with a display open could not set the font.
 *
 *     read the script          ->  a TermConfig, all plain data
 *     open the display
 *     build the style from it  ->  the font, the palette, the bar
 *     size the window from the style
 *
 * --- what is configurable, and what is not ---
 *
 * What is here is what the code can actually honour today: the font and every
 * colour the terminal draws with. What is not here is written down in the
 * struct itself, with the reason, so that a person reading this file knows
 * what the script cannot yet say rather than finding out by writing it and
 * watching nothing happen.
 */
#ifndef GNUCHANTERM_CONFIG_H
#define GNUCHANTERM_CONFIG_H

#include <stdint.h>

/* A written value: a colour, a font name, a command. */
#define TERM_CONFIG_TEXT_LENGTH 256

/* The number of colours a SETTINGS FILE can name: the sixteen a program names
   by number, plus the theme's own default text and background.
 *
 * It is NOT the size of the palette. The palette a program reaches into is 256
 * entries — the sixteen, the 6x6x6 cube, the greyscale ramp — and there are two
 * more past those for the theme's own colours; a settings file has no business
 * naming the cube, because `38;5;196` means a specific red whatever the theme
 * says. So this is the length of the list the script writes, and it is mapped
 * onto the full palette in term_config_apply_style(). */
#define TERM_CONFIG_PALETTE_SIZE 18

/* Which of those eighteen is the theme's own text and background. The last two,
 * because a person writing a Colors list writes the sixteen a program names and
 * then, if they want, the two the terminal itself draws in — which is where
 * they have always been and where a list copied from another terminal expects
 * them. They do NOT correspond to indices 16 and 17 in the palette; see
 * TERM_COLOR_INDEX_FG in term_grid.h for why the palette puts them past 255. */
#define TERM_CONFIG_INDEX_TEXT 16
#define TERM_CONFIG_INDEX_BG   17

/* One colour of the palette, as written: -1 means "the script did not name
   this one" and the built-in value is kept. It is an int and not a uint32_t
   because "unset" has to be expressible, and every 24-bit colour is a legal
   value — including black, which is zero and which no sentinel could be told
   from. */
   
typedef struct TermConfigColor {
    int set;
    uint32_t rgb;
} TermConfigColor;

typedef struct TermConfig {
    /* --- the font ---------------------------------------------------------
     *
     * An Xft font description: a family, a size, and the hinting and
     * antialiasing settings, written the way every X program writes one.
     * `monospace-11` is the built-in default.
     *
     * It is the one setting that has to be read before there is a display,
     * because the cell size comes out of the font and the window is sized in
     * cells — see the file comment above. */
    char font[TERM_CONFIG_TEXT_LENGTH];

    /* --- the prompt -------------------------------------------------------
     *
     * The shell's PS1, written the way the shell itself writes it: the escapes
     * \\u, \\h, \\w and \\n are the shell's own and are passed through
     * untouched, because it is the shell that expands them and not this file.
     *
     * It is handed to the child in its environment and nothing of the user's
     * is read or written. Three names carry it, and the reason it takes three
     * is that putting the text in PS1 alone does NOT work: a shell reads the
     * environment first and its startup files after, and /etc/profile and
     * /etc/bash.bashrc both set PS1 on every Debian install. So PROMPT_COMMAND
     * carries a command that puts the prompt back before each one — see
     * GnuChanTerm.c's build_env(), which is where the three are built.
     *
     * Empty means "the script named no prompt", which leaves the shell's own
     * alone — the same contract the font and every colour have. */
    char prompt[TERM_CONFIG_TEXT_LENGTH];

    /* --- the colours ------------------------------------------------------
     *
     * The palette a program names by number, plus the two the theme owns. Each
     * one is separately optional: a script that names three of them keeps the
     * built-in values for the other fifteen, which is the difference between a
     * settings file and a whole theme. */
    TermConfigColor palette[TERM_CONFIG_PALETTE_SIZE];

    /* The bar along the bottom, which is the terminal's own furniture and not
       the program's output. */
    TermConfigColor bar_background;
    TermConfigColor bar_foreground;

    /* The block drawn over the cell the cursor sits on. It is the terminal's
       own and not a palette entry, because it is not a colour a program can
       ask for: it is what the terminal draws to say where the cursor is, and a
       program that wanted a different one would be asking for a different
       terminal. */
    TermConfigColor cursor;

    /* --- what is NOT here, and why ----------------------------------------
     *
     * The cursor's shape, the margin and height of the bar, the starting size
     * of the window, the shell to run, and the scrollback's DEPTH are all
     * things a person might reasonably want to set, and none of them is in
     * this struct. They are not in it because the code that would honour them
     * is not there yet: the frame's two measurements are compile-time constants
     * in term_core.h and term_render.c, the window has one starting size, and
     * the scrollback is a fixed four thousand lines — see TERM_SCROLL_MAX_LINES
     * in term_scroll.h. The scrollback itself EXISTS and is drawn and scrolled;
     * what this struct cannot say is how deep it should be.
     *
     * A setting that is read and then quietly ignored is worse than a setting
     * that is not offered: the first is a lie the person who wrote the script
     * has no way to find out about. They go in when the code behind them does.
     */

    /* --- something that could not be given --------------------------------
     *
     * In the words to show the person who wrote it: a colour that is not a
     * colour, a font that would not open, an unknown setting. It is carried
     * here rather than printed where it is found because the read happens
     * before there is a window to print on — see term_config_apply_style().
     *
     * Empty when there is nothing to say, which is the usual case. */
    char notes[TERM_CONFIG_TEXT_LENGTH * 2];
} TermConfig;

/* The terminal the code had before it was configurable: the palette in
   gcl_palette.h, the bar, the cursor, and the built-in font. A machine whose
   config is missing or broken gets exactly this. */
void term_config_defaults(TermConfig *config);

/* Where the script is looked for, FIRST PLACE THAT HAS ONE WINNING:

     $GCL_TERMINAL_CONFIG                    a file named outright
     $XDG_CONFIG_HOME/GnuChanTerm/GnuChanTerm.py
     ~/.config/GnuChanTerm/GnuChanTerm.py
     GnuChanTerm_config/GnuChanTerm.py       the copy the source tree ships

   The last one is relative to the working directory and is what lets a build
   run straight from the tree — `makefile.py run` — read the settings the tree
   ships instead of falling back to the built-in palette of gcl_palette.h. It
   is LAST so a machine with settings of its own never has the tree's file in
   front of it; see term_config_path() in term_config.c for why each place is
   where it is.

   Written into buffer, which is returned. `size` is how much room there is. */
char *term_config_path(char *buffer, unsigned int size);

/* Read the script at path into config. Returns 0 when the file was read and
   walked, -1 when it could not be opened or could not be parsed far enough to
   trust — a half-read config is never applied.
 *
 * A missing file is NOT an error: it returns 0 and leaves the defaults, which
 * is what a machine that has never been configured must get. A broken file is
 * an error, and the reason is put in config->notes. */
int term_config_load(TermConfig *config, const char *path);

/* Read the script from the usual place, and start from the built-in defaults.
   Returns 0 when a script was read, and -1 when none was found — which is not
   a failure and is what a freshly installed machine gets. The caller uses the
   return value only to decide whether to say so. */
int term_config_load_default(TermConfig *config);

/* Push the config onto the style: the palette, the bar's two colours, and the
   cursor's. The font is the one thing this does NOT do, because the font has
   to be opened before the display is sized and this is called after — see
   GnuChanTerm.c for the order. */
struct TermStyle;
void term_config_apply_style(const TermConfig *config, struct TermStyle *style);

#endif /* GNUCHANTERM_CONFIG_H */
