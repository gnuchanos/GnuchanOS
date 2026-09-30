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

char *term_config_path(char *buffer, unsigned int size) {
    if (!buffer || size == 0) {
        return buffer;
    }
    buffer[0] = '\0';

    /* $XDG_CONFIG_HOME when it is set and absolute, which is what the
       specification says it is for; ~/.config otherwise. A relative
       XDG_CONFIG_HOME is ignored on purpose — the spec says so, and honouring
       it would put the file wherever the user happened to start the terminal
       from, which is a settings file nobody can find twice. */
    const char *base = getenv("XDG_CONFIG_HOME");
    if (base != NULL && base[0] == '/') {
        snprintf(buffer, size, "%s/GnuChanTerm/GnuChanTerm.py", base);
        return buffer;
    }

    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        /* No HOME and no XDG_CONFIG_HOME: there is nowhere to look, and the
           caller is told by the empty string rather than by a path made of
           guesses. */
        return buffer;
    }
    snprintf(buffer, size, "%s/.config/GnuChanTerm/GnuChanTerm.py", home);
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

void term_config_apply_style(const TermConfig *config, struct TermStyle *style) {
    if (!config || !style) {
        return;
    }

    /* The palette the style ended up with, with the script's colours layered
       over the top.
     *
     * The style's own values are the starting point and NOT the built-in
     * palette in gcl_palette.h, because those two can already differ: a
     * program may have set a palette entry for its own output through OSC 4,
     * and the style carries the themed palette term_style_init() built.
     * Reading the runtime palette is what keeps a config from undoing a
     * program's own colour, and what makes editing gcl_palette.h and writing a
     * config the same kind of change rather than two that fight.
     *
     * For the sixteen a program names, palette entry i IS the answer. The last
     * two are the theme's own text and background and are read through their
     * own accessors, because that is what those indices mean — see
     * term_style.c. */
    uint32_t palette[TERM_CONFIG_PALETTE_SIZE];
    for (int i = 0; i < TERM_CONFIG_PALETTE_SIZE; i++) {
        if (i == TERM_COLOR_INDEX_FG) {
            palette[i] = term_style_default_fg(style);
        } else if (i == TERM_COLOR_INDEX_BG) {
            palette[i] = term_style_default_bg(style);
        } else if (i < style->palette.count) {
            palette[i] = style->palette.colors[i];
        } else {
            /* An entry the style never set. Nothing has a colour for it, so
               the background stands in — a cell drawn in it is visible and
               wrong rather than invisible, which is the same choice the
               renderer makes for a colour it cannot allocate. */
            palette[i] = term_style_default_bg(style);
        }
        if (config->palette[i].set) {
            palette[i] = config->palette[i].rgb;
        }
    }
    term_style_set_palette(style, palette, TERM_CONFIG_PALETTE_SIZE);

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
