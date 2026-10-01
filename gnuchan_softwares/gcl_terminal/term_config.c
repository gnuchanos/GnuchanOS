/*
 * term_config.c — read the settings script, and push it onto the style.
 *
 * The parser next door turns the script into statements; this file gives those
 * statements meaning. It is the only place that knows what `gcl_Terminal`
 * is, and the only place that knows a colour written "#09030d" is three bytes
 * and not seven characters.
 *
 * --- the script, in full ---
 *
 *     gcl_Terminal.call(
 *         Font="monospace-11",
 *         Colors=["#170a20", "#c084fc", ...],
 *         BarBackground="#09030d",
 *         BarForeground="#ead7ff",
 *     )
 *
 * or the same things written as assignments:
 *
 *     gcl_Terminal.font = "monospace-11"
 *     gcl_Terminal.colors = ["#170a20", ...]
 *     gcl_Terminal.bar_background = "#09030d"
 *     gcl_Terminal.bar_foreground = "#ead7ff"
 *
 * The two forms are read by the same code and are the same thing twice: the
 * call is what a file with a handful of settings reads like, and the
 * assignments are what a file being edited one line at a time reads like. A
 * script may use both.
 *
 * --- everything is optional, and that is the point ---
 *
 * A script that names three colours keeps the built-in other fifteen. This is
 * what makes the file a settings file rather than a whole theme, and it is why
 * every colour is a TermConfigColor with a `set` flag and not a bare uint32_t:
 * "black" and "not written" have to be distinguishable, and zero is black.
 *
 * --- a broken script is not applied at all ---
 *
 * The read is all-or-nothing. A file with a syntax error leaves the terminal
 * exactly as it was and puts the reason in config->notes; a file that parses
 * but names a colour that is not a colour says so the same way and keeps the
 * built-in for that one colour. The difference is deliberate: a syntax error
 * means the person is mid-edit and the whole thing is unreliable, while one
 * bad colour in an otherwise good file is one bad colour.
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "term_config.h"
#include "term_config_parser.h"
#include "term_style.h"

/* The names this file gives meaning to. They are written once here and matched
   against what the script says, so a typo in one place is a typo in one place
   rather than two spellings that disagree. */
#define CONFIG_OBJECT   "gcl_Terminal"
#define KEY_FONT        "Font"
#define KEY_PROMPT      "Prompt"
#define KEY_COLORS      "Colors"
#define KEY_BAR_BG      "BarBackground"
#define KEY_BAR_FG      "BarForeground"
#define KEY_CURSOR      "Cursor"

/* --- the defaults --------------------------------------------------------- */

void term_config_defaults(TermConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));

    /* The font is left empty, and empty means "the built-in" to whoever opens
       it. Naming "monospace-11" here would be the same string written twice —
       once in this file and once in term_style.c — and the two would drift.
       What this file knows is whether the script named a font; what a font
       NAME means is the style's business. */
    config->font[0] = '\0';

    /* No prompt named, which leaves the shell's own alone. This is NOT a
       built-in PS1 written here: a terminal that shipped one would be a
       terminal that decided how every shell on the machine should look, and
       the whole point of the prompt being in the script is that it is written
       where the rest of the terminal is. See TermConfig.prompt. */
    config->prompt[0] = '\0';

    /* Every colour starts unset. Nothing is filled in from gcl_palette.h here,
       because the palette belongs to the style and this struct only carries
       what the SCRIPT said — see term_config_apply_style(), which layers the
       script's colours over the built-in ones. */
    for (int i = 0; i < TERM_CONFIG_PALETTE_SIZE; i++) {
        config->palette[i].set = 0;
        config->palette[i].rgb = 0;
    }
    config->bar_background.set = 0;
    config->bar_foreground.set = 0;
    config->cursor.set = 0;

    config->notes[0] = '\0';
}

/* --- where the script is -------------------------------------------------- */

/* Whether a path names something that can be read. It is how the search below
   decides a candidate is THE file: a place with nothing at it is not an
   answer, and walking past it is the whole point of the search. */
static int path_is_readable(const char *path) {
    return path != NULL && path[0] != '\0' && access(path, R_OK) == 0;
}

/* THE FIRST PLACE THAT HOLDS A FILE WINS, and the order is the whole of this
 * function:
 *
 *   $GCL_TERMINAL_CONFIG    one file named outright. It is first because a
 *                           person who sets it means it — a second theme, a
 *                           test, a session with its own colours — and
 *                           nothing should be able to get in front of it.
 *
 *   $XDG_CONFIG_HOME/...    the standard user location, which is where the
 *                           installer writes and where a desktop's own
 *                           settings belong.
 *
 *   ~/.config/...           the same location for a session that never set
 *                           XDG_CONFIG_HOME, which is most of them.
 *
 *   <exe>/GnuChanTerm_config/...  the copy the SOURCE TREE ships, found from
 *                           the running BINARY and not from the working
 *                           directory. That distinction is the whole of this
 *                           step: a terminal started by the window manager, by
 *                           a desktop entry or by a key binding has a working
 *                           directory of $HOME or whatever the session was,
 *                           and a path relative to that finds nothing. The
 *                           binary, wherever it was built, always knows where
 *                           it is, so this is the copy that is actually beside
 *                           it. It is next to last because it is the
 *                           developer's file — a machine with settings of its
 *                           own must not have the tree's one step in front of
 *                           it — and it is what makes a build run from the
 *                           tree, with no install, read the settings the tree
 *                           ships.
 *
 *   GnuChanTerm_config/...  the same copy, relative to the working directory.
 *                           It is kept and tried after the one above because it
 *                           is what a person running the binary by hand, from
 *                           the source directory, has always had; the check
 *                           above answers the same case from the other side,
 *                           so this one is a backstop rather than the answer.
 *
 * That last entry is the whole reason the settings file appeared to do
 * nothing. The terminal only ever looked in the two user locations, so a
 * build run from the tree — which is what `run` is, and what a person testing
 * a change does — had no settings file at all: the colours stayed the built-in
 * palette of gcl_palette.h and the prompt stayed whatever the shell's own
 * startup files set, because none of gcl_Terminal was ever read.
 *
 * When none of the places holds a file the standard user location is written,
 * and NOT an empty string: the caller then reports that path as missing, which
 * names the file a person is expected to write. An empty string would say only
 * that there is nowhere to look.
 *
 * Written into buffer, which is returned. `size` is how much room there is. */
char *term_config_path(char *buffer, unsigned int size) {
    if (!buffer || size == 0) {
        return buffer;
    }
    buffer[0] = '\0';

    /* 1. The file named outright. */
    const char *named = getenv("GCL_TERMINAL_CONFIG");
    if (path_is_readable(named)) {
        snprintf(buffer, size, "%s", named);
        return buffer;
    }

    /* 2. $XDG_CONFIG_HOME when it is set and absolute, which is what the
       specification says it is for. A relative XDG_CONFIG_HOME is ignored on
       purpose — the spec says so, and honouring it would put the file wherever
       the user happened to start the terminal from, which is a settings file
       nobody can find twice. */
    char candidate[TERM_CONFIG_TEXT_LENGTH * 2];
    const char *base = getenv("XDG_CONFIG_HOME");
    if (base != NULL && base[0] == '/') {
        snprintf(candidate, sizeof(candidate),
                 "%s/GnuChanTerm/GnuChanTerm.py", base);
        if (path_is_readable(candidate)) {
            snprintf(buffer, size, "%s", candidate);
            return buffer;
        }
    }

    /* 3. ~/.config, the same place for a session that never set
       XDG_CONFIG_HOME. The path is written even when there is nothing at it —
       it is the fallback the caller reports — and the search carries on,
       because the file being looked for may be down at the tree's copy. */
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        snprintf(candidate, sizeof(candidate),
                 "%s/.config/GnuChanTerm/GnuChanTerm.py", home);
        if (path_is_readable(candidate)) {
            snprintf(buffer, size, "%s", candidate);
            return buffer;
        }
        snprintf(buffer, size, "%s", candidate);
    }

    /* 4. The tree's own copy, found from the BINARY rather than from the
       working directory — see the note above for why that is the difference
       that matters. /proc/self/exe is the kernel's own answer to "where is the
       program I am running", and it is a symlink to the real file wherever it
       was started from. The readlink gives the directory the binary is in, and
       the config is looked for in the tree beside it.
     *
     * A machine without /proc — not Linux — simply finds nothing here and
       falls to the step below, so this cannot be the reason a terminal fails
       to start. */
    char exe[TERM_CONFIG_TEXT_LENGTH];
    ssize_t exe_length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (exe_length > 0) {
        exe[exe_length] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash != NULL) {
            *slash = '\0';   /* the directory the binary is in */

            /* WALK UP from the binary, one directory at a time, and look for
               the tree's copy at each level. A fixed number of ".." would be
               wrong, and that is the whole reason for the walk: where the copy
               sits relative to the binary depends on how it was built. A build
               into a `build/` directory under the source has it three levels
               up; a build into `_temp/<name>-build/` beside the repository
               root — which is what makefile.py does — has it four. Encoding
               one of those would work on one machine and not the other.
             *
               TWO SHAPES are looked for at every level, because the tree
               itself can be entered from two places: the config directly under
               the level (a working directory that IS the source), and the copy
               under the repository's own gnuchan_softwares/gcl_terminal (a
               level that is the repository root). One of the two is what a
               given level is, and trying both is what makes the walk
               layout-blind.
             *
               The walk stops at the root — the slash that has no directory
               before it — so a binary outside any tree simply finds nothing
               and falls to the step below. */
            char level[TERM_CONFIG_TEXT_LENGTH];
            snprintf(level, sizeof(level), "%s", exe);
            for (int depth = 0; depth < 6; depth++) {
                snprintf(candidate, sizeof(candidate),
                         "%s/GnuChanTerm_config/GnuChanTerm.py", level);
                if (path_is_readable(candidate)) {
                    snprintf(buffer, size, "%s", candidate);
                    return buffer;
                }
                snprintf(candidate, sizeof(candidate),
                         "%s/gnuchan_softwares/gcl_terminal/"
                         "GnuChanTerm_config/GnuChanTerm.py", level);
                if (path_is_readable(candidate)) {
                    snprintf(buffer, size, "%s", candidate);
                    return buffer;
                }
                char *up = strrchr(level, '/');
                if (up == NULL || up == level) {
                    break;    /* the root: nothing above it to try */
                }
                *up = '\0';
            }
        }
    }

    /* 5. The same copy, beside the working directory. A backstop for the case
       above — a session with no /proc, or a binary moved away from its tree
       while the working directory still holds one. */
    if (path_is_readable("GnuChanTerm_config/GnuChanTerm.py")) {
        snprintf(buffer, size, "%s", "GnuChanTerm_config/GnuChanTerm.py");
        return buffer;
    }

    return buffer;
}

/* --- a colour written as text -------------------------------------------- */

/* "#rrggbb", "rrggbb", "#rgb" or "rgb" into a packed 0xRRGGBB. Returns 0 on
   success and -1 when the text is not a colour.
 *
 * The short form is expanded by doubling each digit — "#abc" is "#aabbcc" —
 * which is what every other program on the system does with it, so a colour
 * copied from one file into another is the same colour. */
static int parse_color(const char *text, uint32_t *out) {
    if (!text || !out) {
        return -1;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (*text == '#') {
        text++;
    }

    char digits[8];
    unsigned int count = 0;
    for (const char *p = text; *p != '\0' && count < sizeof(digits) - 1; p++) {
        if (*p == ' ' || *p == '\t') {
            break;
        }
        if (!isxdigit((unsigned char)*p)) {
            return -1;
        }
        digits[count++] = *p;
    }
    digits[count] = '\0';

    unsigned long value = strtoul(digits, NULL, 16);
    if (count == 3) {
        /* #abc becomes #aabbcc: each digit is repeated, which is what makes
           the short form the same colour as the long one and not a different
           colour that happens to be three digits. */
        unsigned long r = (value >> 8) & 0xF;
        unsigned long g = (value >> 4) & 0xF;
        unsigned long b = value & 0xF;
        *out = (uint32_t)((r * 17) << 16 | (g * 17) << 8 | (b * 17));
        return 0;
    }
    if (count == 6) {
        *out = (uint32_t)value;
        return 0;
    }
    return -1;
}

/* --- reading a value into the config ------------------------------------- */

/* The name of a palette entry, in the order the sixteen are named everywhere,
   so a note about one can say WHICH one rather than "a colour". The last two
   are the theme's own and are named for what they are. */
static const char *palette_name(int index) {
    static const char *NAMES[TERM_CONFIG_PALETTE_SIZE] = {
        "black",          "red",            "green",         "yellow",
        "blue",           "magenta",        "cyan",          "white",
        "bright black",   "bright red",     "bright green",  "bright yellow",
        "bright blue",    "bright magenta", "bright cyan",   "bright white",
        "text",           "background",
    };
    if (index < 0 || index >= TERM_CONFIG_PALETTE_SIZE) {
        return "colour";
    }
    return NAMES[index];
}

/* Add a line to config->notes, and say so on stderr as well.
 *
 * Both, and not one: the note is what a settings script's author is shown when
 * the terminal starts from a desktop entry and there is no terminal to print
 * on, and stderr is what they see when they run it from a shell to find out
 * what is wrong. A message that only went to a place the reader is not is a
 * message that was not delivered. */
static void config_note(TermConfig *config, const char *message) {
    if (config->notes[0] != '\0') {
        unsigned int used = (unsigned int)strlen(config->notes);
        if (used + 2 < sizeof(config->notes)) {
            snprintf(config->notes + used, sizeof(config->notes) - used,
                     "; %s", message);
        }
    } else {
        snprintf(config->notes, sizeof(config->notes), "%s", message);
    }
    fprintf(stderr, "gnuchanterm: config: %s\n", message);
}

/* One colour value into a slot. `what` names it for the message when it is not
   a colour, so the person is told which one rather than that one of them is
   wrong. */
static void set_one_color(TermConfig *config, TermConfigColor *slot,
                          const TermValue *value, const char *what) {
    char text[TERM_CONFIG_TEXT_LENGTH];
    term_config_value_text(value, text, sizeof(text));
    if (text[0] == '\0') {
        char message[TERM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s is not a colour; the built-in one is kept", what);
        config_note(config, message);
        return;
    }
    uint32_t rgb = 0;
    if (parse_color(text, &rgb) != 0) {
        /* The text is shown back but CUT at forty characters, and the cut is
           not tidiness: `text` holds a whole written value and `what` names
           it, and the two together can be longer than this buffer. Left
           alone, gcc says so — -Wformat-truncation — and it is right: the
           message would be silently clipped, and the person would be shown
           half of what they wrote with nothing to say there was more. Forty
           characters is a colour and then some, which is enough to recognise
           their own mistake in. */
        char message[TERM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s is not a colour (\"%.40s\"); the built-in one is kept",
                 what, text);
        config_note(config, message);
        return;
    }
    slot->set = 1;
    slot->rgb = rgb;
}

/* The Colors list: up to TERM_CONFIG_PALETTE_SIZE entries, in the order every
   terminal has used since the eighties. A shorter list sets the first entries
   and leaves the rest — so a script that names only the sixteen a program can
   use does not have to repeat the theme's own two. */
static void read_colors(TermConfig *config, const TermValue *value) {
    if (value->kind != TERM_VALUE_LIST) {
        config_note(config, "Colors is not a list; the built-in palette kept");
        return;
    }
    int count = value->item_count;
    if (count > TERM_CONFIG_PALETTE_SIZE) {
        /* The extra entries are dropped rather than refused: the first
           eighteen are the ones that mean something, and a script that wrote
           more was working from a longer palette than this terminal has. */
        count = TERM_CONFIG_PALETTE_SIZE;
    }
    for (int i = 0; i < count; i++) {
        char what[64];
        snprintf(what, sizeof(what), "Colors[%d] (%s)", i, palette_name(i));
        if (value->items[i].kind == TERM_VALUE_CALL ||
            value->items[i].kind == TERM_VALUE_LIST) {
            continue;
        }
        set_one_color(config, &config->palette[i], &value->items[i], what);
    }
}

/* One `Name=value` of the call, or one `object.field = value` assignment. The
   name is the same string in both forms, which is why this takes it rather
   than reading it — the two callers spell it differently and mean the same. */
static void read_setting(TermConfig *config, const char *name,
                         const TermValue *value) {
    if (strcmp(name, KEY_FONT) == 0) {
        char text[TERM_CONFIG_TEXT_LENGTH];
        term_config_value_text(value, text, sizeof(text));
        if (text[0] == '\0') {
            config_note(config, "Font is not a name; the built-in font kept");
            return;
        }
        snprintf(config->font, sizeof(config->font), "%s", text);
        return;
    }
    if (strcmp(name, KEY_PROMPT) == 0) {
        char text[TERM_CONFIG_TEXT_LENGTH];
        term_config_value_text(value, text, sizeof(text));
        if (text[0] == '\0') {
            config_note(config, "Prompt is not a string; the shell's own is kept");
            return;
        }
        /* Copied whole and NOT interpreted. The \u, \h and \w in it are the
           shell's escapes and the shell expands them; this only carries the
           text to the environment the child is given. Anything that looked
           inside would be this file deciding what a prompt means, which is
           the shell's business. */
        snprintf(config->prompt, sizeof(config->prompt), "%s", text);
        return;
    }
    if (strcmp(name, KEY_COLORS) == 0) {
        read_colors(config, value);
        return;
    }
    if (strcmp(name, KEY_BAR_BG) == 0) {
        set_one_color(config, &config->bar_background, value, "BarBackground");
        return;
    }
    if (strcmp(name, KEY_BAR_FG) == 0) {
        set_one_color(config, &config->bar_foreground, value, "BarForeground");
        return;
    }
    if (strcmp(name, KEY_CURSOR) == 0) {
        set_one_color(config, &config->cursor, value, "Cursor");
        return;
    }

    /* A name this terminal does not know. It is said out loud and not silently
       dropped: `BarColour` and `BarBackground` look alike, and a person who
       typed the first would otherwise watch their bar stay the same colour
       with nothing anywhere to explain it. */
    char message[TERM_CONFIG_TEXT_LENGTH];
    snprintf(message, sizeof(message),
             "\"%s\" is not a setting this terminal has; it is ignored", name);
    config_note(config, message);
}

/* One statement of the script. Everything that is not a setting of
   gcl_Terminal is left alone — a script may hold a note to itself, and a name
   given a value that this terminal has no use for is not a mistake. */
static void read_statement(TermConfig *config, const TermStatement *statement) {
    /* gcl_Terminal.call(Name=value, ...) */
    if (statement->kind == TERM_STMT_CALL) {
        if (strcmp(statement->target, CONFIG_OBJECT ".call") != 0) {
            return;
        }
        for (int i = 0; i < statement->arg_count; i++) {
            if (statement->args[i].name[0] == '\0') {
                /* A positional argument. The call takes named ones only, and
                   an unnamed one cannot be placed — so it is named in the
                   note rather than guessed at. */
                config_note(config,
                            "gcl_Terminal.call() takes named arguments only");
                continue;
            }
            read_setting(config, statement->args[i].name,
                         &statement->args[i].value);
        }
        return;
    }

    /* gcl_Terminal.field = value */
    const char *dot = strrchr(statement->target, '.');
    if (dot == NULL) {
        return;
    }
    size_t object_length = (size_t)(dot - statement->target);
    if (object_length != strlen(CONFIG_OBJECT) ||
        strncmp(statement->target, CONFIG_OBJECT, object_length) != 0) {
        return;
    }

    /* The field as written is lower_case by the convention assignments follow,
       and the argument names are UpperCamel by the convention Python calls
       follow. Both are accepted for the same thing, because the two forms are
       offered as the same thing twice and a person should not have to
       remember which spelling belongs to which. */
    const char *field = dot + 1;
    char name[TERM_CONFIG_TEXT_LENGTH];
    snprintf(name, sizeof(name), "%s", field);
    if (strcmp(name, "font") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_FONT);
    } else if (strcmp(name, "prompt") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_PROMPT);
    } else if (strcmp(name, "colors") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_COLORS);
    } else if (strcmp(name, "bar_background") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_BAR_BG);
    } else if (strcmp(name, "bar_foreground") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_BAR_FG);
    } else if (strcmp(name, "cursor") == 0) {
        snprintf(name, sizeof(name), "%s", KEY_CURSOR);
    }
    read_setting(config, name, &statement->value);
}

/* --- the read ------------------------------------------------------------- */

int term_config_load(TermConfig *config, const char *path) {
    if (!config || !path || path[0] == '\0') {
        return -1;
    }

    /* A missing file is not a failure. It is the ordinary state of a machine
       that has never been configured, and the defaults it leaves behind are
       the terminal as it shipped. */
    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return 0;
    }
    fclose(probe);

    TermStatement *statements = NULL;
    int count = 0;
    if (term_config_parse_file(path, &statements, &count) != 0) {
        const char *why = term_config_last_error();
        char message[TERM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s could not be read (%s); the defaults are used",
                 path, (why && why[0]) ? why : "it is not understood");
        config_note(config, message);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        read_statement(config, &statements[i]);
    }
    term_config_statements_free(statements, count);
    return 0;
}

int term_config_load_default(TermConfig *config) {
    term_config_defaults(config);

    char path[TERM_CONFIG_TEXT_LENGTH * 2];
    term_config_path(path, sizeof(path));
    if (path[0] == '\0') {
        /* Nowhere to look. Not an error and not worth a note: a machine with
           no HOME is a machine started from something that had no business
           starting a terminal, and the defaults are a working terminal. */
        return -1;
    }

    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return -1;
    }
    fclose(probe);

    return term_config_load(config, path) == 0 ? 0 : -1;
}

/* --- onto the style ------------------------------------------------------- */

/* Where a slot in the script's Colors list lands in the PALETTE.
 *
 * The list is eighteen long and the palette is 258, and they are not the same
 * numbering past the sixteenth. The first sixteen are the same in both — a
 * program names 0 to 15 and the theme writes 0 to 15, and they are the same
 * entries. The script's last two are the theme's own text and background, and
 * in the palette those live PAST the 256 a program can name — see
 * TERM_COLOR_INDEX_FG. So the seventeenth and eighteenth entries of the list
 * are the 257th and 258th entries of the palette, and this is the one place
 * that mapping is written down.
 *
 * It returns -1 for a slot with no place in the palette, which cannot happen
 * for the eighteen the list holds and is answered rather than assumed. */
static int config_slot_to_palette(int slot) {
    if (slot < 0 || slot >= TERM_CONFIG_PALETTE_SIZE) {
        return -1;
    }
    if (slot == TERM_CONFIG_INDEX_TEXT) {
        return TERM_COLOR_INDEX_FG;
    }
    if (slot == TERM_CONFIG_INDEX_BG) {
        return TERM_COLOR_INDEX_BG;
    }
    return slot;
}

void term_config_apply_style(const TermConfig *config, struct TermStyle *style) {
    if (!config || !style) {
        return;
    }

    /* The WHOLE palette, with the script's colours layered over the top.
     *
     * The style's own values are the starting point and NOT the built-in
     * palette in gcl_palette.h, because those two can already differ: a
     * program may have set a palette entry for its own output through OSC 4,
     * and the style carries the themed palette term_style_init() built.
     * Reading the runtime palette is what keeps a config from undoing a
     * program's own colour, and what makes editing gcl_palette.h and writing a
     * config the same kind of change rather than two that fight.
     *
     * The copy is the FULL 256-colour palette and not the eighteen a script
     * can name, and that is the whole of this being correct: the cube and the
     * greyscale ramp are entries a program reaches by number and the script
     * has no business touching, so they are carried through untouched rather
     * than being cut off at eighteen. Writing back only eighteen — which is
     * what this did before the palette grew — would set the palette's count to
     * 18 and make every `38;5;N` above 17 fall back to the default, which is
     * the bug the 256-colour form had. */
    uint32_t palette[TERM_PALETTE_SIZE];
    for (int i = 0; i < TERM_PALETTE_SIZE; i++) {
        palette[i] = (i < style->palette.count)
                         ? style->palette.colors[i]
                         : term_style_default_bg(style);
    }

    /* The script's colours over the top. Each slot is placed where it belongs
       in the palette rather than at the same number, because the last two are
       not at 16 and 17 in the palette — see config_slot_to_palette(). */
    for (int slot = 0; slot < TERM_CONFIG_PALETTE_SIZE; slot++) {
        if (!config->palette[slot].set) {
            continue;
        }
        int index = config_slot_to_palette(slot);
        if (index >= 0 && index < TERM_PALETTE_SIZE) {
            palette[index] = config->palette[slot].rgb;
        }
    }

    term_style_set_palette(style, palette, TERM_PALETTE_SIZE);

    /* The bar's two, each falling back to what the style already had so a
       script that names one of them keeps the other. */
    uint32_t bar_bg = term_style_bar_bg(style);
    uint32_t bar_fg = term_style_bar_fg(style);
    if (config->bar_background.set) {
        bar_bg = config->bar_background.rgb;
    }
    if (config->bar_foreground.set) {
        bar_fg = config->bar_foreground.rgb;
    }
    term_style_set_bar(style, bar_bg, bar_fg);

    /* The cursor's colour, falling back to the style's own for the same reason
       the bar's two do: a script that names it gets it, and one that does not
       keeps what the theme shipped. */
    uint32_t cursor = term_style_cursor(style);
    if (config->cursor.set) {
        cursor = config->cursor.rgb;
    }
    term_style_set_cursor(style, cursor);
}
