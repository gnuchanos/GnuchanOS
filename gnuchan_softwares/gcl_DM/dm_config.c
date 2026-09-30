/*
 * dm_config.c — read the settings script, and give the statements meaning.
 *
 * The parser next door turns the script into statements; this file is what
 * knows that `gcl_DM` is the login screen, and that a colour is written as
 * text that dm_style.c will resolve once there is a display to resolve it on.
 *
 * --- the script, in full ---
 *
 *     gcl_DM.call(
 *         Background="#1a0b2e",
 *         Panel="#32143f",
 *         Accent="#c77dff",
 *         Margin=16,
 *     )
 *
 * or the same things written as assignments:
 *
 *     gcl_DM.background = "#1a0b2e"
 *     gcl_DM.accent = "#c77dff"
 *     gcl_DM.margin = 16
 *
 * The two forms are read by the same code and are the same thing twice: the
 * call is what a file with a handful of settings reads like, and the
 * assignments are what a file being edited one line at a time reads like. A
 * script may use both. Either spelling of a name is accepted — UpperCamel in a
 * call and lower_case in an assignment — because the two forms are offered as
 * the same thing and a person should not have to remember which spelling
 * belongs to which.
 *
 * --- everything is optional, and that is the point ---
 *
 * A script that names three colours keeps the built-in other seven. This is
 * what makes the file a settings file rather than a whole theme: every field
 * starts empty, and empty means "the script did not name this one" to
 * dm_style.c, which is where the defaults live.
 *
 * --- a broken script is not applied at all ---
 *
 * The read is all-or-nothing. A file with a syntax error leaves the greeter
 * exactly as it was and puts the reason in config->notes; a file that parses
 * but names a measurement that is not a number says so the same way and keeps
 * the built-in for that one setting. The difference is deliberate: a syntax
 * error means the person is mid-edit and the whole thing is unreliable, while
 * one bad value in an otherwise good file is one bad value.
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dm_config.h"
#include "dm_config_parser.h"

/* The object a setting belongs to. It is written once here and matched against
   what the script says, so a typo in one place is a typo in one place rather
   than two spellings that disagree. */
#define CONFIG_OBJECT "gcl_DM"

/* --- the defaults --------------------------------------------------------- */

void dm_config_defaults(DmConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));

    /* Every field is cleared — by the memset above — and that IS the default:
       an empty string means "the script did not name this one", which is what
       tells dm_style.c to use its own value. The palette and the fonts that a
       machine with no script gets live in dm_style.c, in one place, where the
       drawing code that uses them can see them; writing them here as well
       would be the same numbers twice, and the two would drift.

       The five measurements are also zero, and zero means "not written" for
       the same reason: a login panel with no margin is not a thing anyone
       asks for, so the built-in value is kept. */

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
 *   $GCL_DM_CONFIG        one file named outright. It is first because a
 *                         person who sets it means it — a test, a second
 *                         theme — and nothing should get in front of it.
 *
 *   $XDG_CONFIG_HOME/...  the standard user location, which is where the
 *                         installer writes and where a login screen's own
 *                         settings belong.
 *
 *   ~/.config/...         the same location for a session that never set
 *                         XDG_CONFIG_HOME.
 *
 *   GnuChanDM_config/...  the copy the SOURCE TREE ships, relative to where
 *                         the greeter was started.
 *
 * That last entry is the whole reason it is relative. A greeter started by
 * systemd has `/` as its working directory, so the tree's copy is only found
 * when it is started BY HAND from the tree — which is exactly the case it
 * exists for. An installed machine never sees it, because the installer writes
 * the file under the user's own ~/.config and that is found first.
 *
 * When none of the places holds a file the standard user location is written,
 * and NOT an empty string: the caller then reports that path as missing, which
 * names the file a person is expected to write. An empty string would say only
 * that there is nowhere to look.
 *
 * Written into buffer, which is returned. `size` is how much room there is. */
char *dm_config_path(char *buffer, unsigned int size) {
    if (!buffer || size == 0) {
        return buffer;
    }
    buffer[0] = '\0';

    /* 1. The file named outright. */
    const char *named = getenv("GCL_DM_CONFIG");
    if (path_is_readable(named)) {
        snprintf(buffer, size, "%s", named);
        return buffer;
    }

    /* 2. $XDG_CONFIG_HOME when it is set and absolute, which is what the
       specification says it is for. A relative XDG_CONFIG_HOME is ignored on
       purpose — the spec says so, and honouring it would put the file wherever
       the greeter happened to be started from, which is a settings file nobody
       can find twice. */
    char candidate[DM_CONFIG_TEXT_LENGTH * 2];
    const char *base = getenv("XDG_CONFIG_HOME");
    if (base != NULL && base[0] == '/') {
        snprintf(candidate, sizeof(candidate), "%s/GnuChanDM/GnuChanDM.py",
                 base);
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
                 "%s/.config/GnuChanDM/GnuChanDM.py", home);
        if (path_is_readable(candidate)) {
            snprintf(buffer, size, "%s", candidate);
            return buffer;
        }
        snprintf(buffer, size, "%s", candidate);
    }

    /* 4. The tree's own copy, relative to where the greeter was started. The
       second spelling is the path from the repository root, so starting the
       greeter from the top of the tree finds it as well. */
    static const char *TREE_PATHS[] = {
        "GnuChanDM_config/GnuChanDM.py",
        "gnuchan_softwares/gcl_DM/GnuChanDM_config/GnuChanDM.py",
    };
    for (unsigned int i = 0;
         i < sizeof(TREE_PATHS) / sizeof(TREE_PATHS[0]); i++) {
        if (path_is_readable(TREE_PATHS[i])) {
            snprintf(buffer, size, "%s", TREE_PATHS[i]);
            return buffer;
        }
    }

    return buffer;
}

/* --- the names this file gives meaning to --------------------------------- */

/* What kind of thing a setting is, which is the only difference between one
   and another: a colour and a font are both read as text and differ only in
   how they are checked, and a measurement is read as a number. */
typedef enum SettingKind {
    SETTING_COLOR,       /* a written colour, kept as text   */
    SETTING_FONT,        /* an X font name, kept as text     */
    SETTING_MEASURE,     /* a number of pixels, kept as int  */
} SettingKind;

/* One setting: the name a call gives it, the name an assignment gives it, what
   kind it is, and where in DmConfig it lands.
 *
 * The offset is what keeps this table and the struct in step. A field added to
 * DmConfig without a row here is a setting no script can write; a row naming a
 * field that does not exist does not compile. Neither can drift silently, which
 * a table of string names and a separate switch would be free to do. */
typedef struct Setting {
    const char *call_name;
    const char *assign_name;
    SettingKind kind;
    size_t offset;
} Setting;

/* The colours, named for what they are drawn on.
 *
 * The X expands one row per colour, and the name in each row appears twice —
 * once as the UpperCamel a call writes and once as the lower_case an assignment
 * writes. Both spellings reach the same field, which is what lets the script
 * use either form for the same setting. */
#define COLOR_ROW(upper, lower) \
    { #upper, #lower, SETTING_COLOR, offsetof(DmConfig, lower) },
#define FONT_ROW(upper, lower) \
    { #upper, #lower, SETTING_FONT, offsetof(DmConfig, lower) },
#define MEASURE_ROW(upper, lower) \
    { #upper, #lower, SETTING_MEASURE, offsetof(DmConfig, lower) },

static const Setting SETTINGS[] = {
    COLOR_ROW(Background, background)
    COLOR_ROW(Panel,      panel)
    COLOR_ROW(PanelEdge,  panel_edge)
    COLOR_ROW(Field,      field)
    COLOR_ROW(FieldFocus, field_focus)
    COLOR_ROW(Text,       text)
    COLOR_ROW(TextMuted,  text_muted)
    COLOR_ROW(Accent,     accent)
    COLOR_ROW(AccentDim,  accent_dim)
    COLOR_ROW(Danger,     danger)

    FONT_ROW(FontLabel,  font_label)
    FONT_ROW(FontField,  font_field)
    FONT_ROW(FontTitle,  font_title)

    MEASURE_ROW(Margin,       margin)
    MEASURE_ROW(Gap,          gap)
    MEASURE_ROW(PanelWidth,   panel_width)
    MEASURE_ROW(FieldHeight,  field_height)
    MEASURE_ROW(ButtonHeight, button_height)
};

#define SETTINGS_COUNT ((int)(sizeof(SETTINGS) / sizeof(SETTINGS[0])))

/* --- notes ---------------------------------------------------------------- */

/* Add a line to config->notes, and say so on stderr as well.
 *
 * Both, and not one: a greeter is started by systemd with its output in a log,
 * so the note is what a person finds when they go looking — and stderr is
 * where they find it. A message that only went to a place the reader is not is
 * a message that was not delivered. */
static void config_note(DmConfig *config, const char *message) {
    if (config->notes[0] != '\0') {
        unsigned int used = (unsigned int)strlen(config->notes);
        if (used + 2 < sizeof(config->notes)) {
            snprintf(config->notes + used, sizeof(config->notes) - used,
                     "; %s", message);
        }
    } else {
        snprintf(config->notes, sizeof(config->notes), "%s", message);
    }
    fprintf(stderr, "gnuchandm: config: %s\n", message);
}

/* --- one value into the config ------------------------------------------- */

/* A written colour, checked to LOOK like one — a leading '#' and hexadecimal
   digits — and not resolved.
 *
 * It is not resolved because resolving a colour is XAllocNamedColor and there
 * is no display yet when this runs: the config is read before the style, which
 * is before the window. The check exists so a person who writes
 * `Accent="#gggggg"` or `Accent=6` hears about it at start rather than finding
 * their sign-in button drawn in a fallback with no explanation anywhere. */
static int looks_like_a_colour(const char *text) {
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    const char *digits = text[0] == '#' ? text + 1 : text;
    int count = 0;
    for (const char *p = digits; *p != '\0'; p++) {
        if (!isxdigit((unsigned char)*p)) {
            return 0;
        }
        count++;
    }
    /* Three, six or eight digits: the short form, the usual one, and the one
       with an alpha byte that XAllocNamedColor also accepts. */
    return count == 3 || count == 6 || count == 8;
}

/* Read one value into the field its setting names.
 *
 * `field` is where text goes and `measure` where a number goes; exactly one of
 * them is non-NULL, chosen by the setting's kind. */
static void read_setting_value(DmConfig *config, const Setting *setting,
                               const DmValue *value, char *field,
                               int *measure) {
    if (setting->kind == SETTING_MEASURE) {
        /* A measurement has to have been written. Zero is the "not written"
           sentinel, and a negative would be a box drawn inside out, so both
           are refused with a note rather than applied. */
        if (value == NULL) {
            return;
        }
        int pixels = dm_config_value_number(value, 0);
        if (pixels < 0) {
            char message[DM_CONFIG_TEXT_LENGTH];
            snprintf(message, sizeof(message),
                     "%s is negative (%d); the built-in is kept",
                     setting->call_name, pixels);
            config_note(config, message);
            return;
        }
        if (pixels == 0) {
            return;
        }
        *measure = pixels;
        return;
    }

    char text[DM_CONFIG_TEXT_LENGTH];
    dm_config_value_text(value, text, sizeof(text));
    if (text[0] == '\0') {
        char message[DM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s is not a value; the built-in is kept",
                 setting->call_name);
        config_note(config, message);
        return;
    }

    if (setting->kind == SETTING_COLOR && !looks_like_a_colour(text)) {
        char message[DM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s is not a colour (\"%.40s\"); the built-in is kept",
                 setting->call_name, text);
        config_note(config, message);
        return;
    }

    snprintf(field, DM_CONFIG_TEXT_LENGTH, "%s", text);
}

/* One `Name=value` of the call, or one `object.field = value` assignment. The
   name is matched WHOLE: a substring match would make `Text` fill in
   `TextMuted`, which is a colour meant for one surface landing on another. */
static void read_named_setting(DmConfig *config, const char *name,
                               const DmValue *value) {
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        const Setting *setting = &SETTINGS[i];
        if (strcmp(name, setting->call_name) != 0 &&
            strcmp(name, setting->assign_name) != 0) {
            continue;
        }

        char *field = (char *)config + setting->offset;
        if (setting->kind == SETTING_MEASURE) {
            read_setting_value(config, setting, value, NULL, (int *)field);
        } else {
            read_setting_value(config, setting, value, field, NULL);
        }
        return;
    }

    /* A name this greeter does not know. It is said out loud and not silently
       dropped: `PanelEdge` and `Panel` look alike, and a person who typed the
       first would otherwise watch their panel stay the same colour with
       nothing anywhere to explain it. */
    char message[DM_CONFIG_TEXT_LENGTH];
    snprintf(message, sizeof(message),
             "\"%s\" is not a setting this login screen has; it is ignored",
             name);
    config_note(config, message);
}

/* One statement of the script. Everything that is not a setting of gcl_DM is
   left alone — a script may hold a note to itself, and a name given a value
   this greeter has no use for is not a mistake. */
static void read_statement(DmConfig *config, const DmStatement *statement) {
    /* gcl_DM.call(Name=value, ...) */
    if (statement->kind == DM_STMT_CALL) {
        if (strcmp(statement->target, CONFIG_OBJECT ".call") != 0) {
            return;
        }
        for (int i = 0; i < statement->arg_count; i++) {
            if (statement->args[i].name[0] == '\0') {
                /* A positional argument. The call takes named ones only, and
                   an unnamed one cannot be placed — so it is named in the note
                   rather than guessed at. */
                config_note(config,
                            "gcl_DM.call() takes named arguments only");
                continue;
            }
            read_named_setting(config, statement->args[i].name,
                               &statement->args[i].value);
        }
        return;
    }

    /* gcl_DM.field = value */
    const char *dot = strrchr(statement->target, '.');
    if (dot == NULL) {
        return;
    }
    size_t object_length = (size_t)(dot - statement->target);
    if (object_length != strlen(CONFIG_OBJECT) ||
        strncmp(statement->target, CONFIG_OBJECT, object_length) != 0) {
        return;
    }
    read_named_setting(config, dot + 1, &statement->value);
}

/* --- the read ------------------------------------------------------------- */

int dm_config_load(DmConfig *config, const char *path) {
    if (!config || !path || path[0] == '\0') {
        return -1;
    }

    /* A missing file is not a failure. It is the ordinary state of a machine
       that has never been configured, and the defaults it leaves behind are
       the login screen as it shipped. */
    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return 0;
    }
    fclose(probe);

    DmStatement *statements = NULL;
    int count = 0;
    if (dm_config_parse_file(path, &statements, &count) != 0) {
        const char *why = dm_config_last_error();
        char message[DM_CONFIG_TEXT_LENGTH];
        snprintf(message, sizeof(message),
                 "%s could not be read (%s); the defaults are used",
                 path, (why && why[0]) ? why : "it is not understood");
        config_note(config, message);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        read_statement(config, &statements[i]);
    }
    dm_config_statements_free(statements, count);
    return 0;
}

int dm_config_load_default(DmConfig *config) {
    dm_config_defaults(config);

    char path[DM_CONFIG_TEXT_LENGTH * 2];
    dm_config_path(path, sizeof(path));
    if (path[0] == '\0') {
        /* Nowhere to look. Not an error and not worth a note: a machine with
           no HOME is a machine started from something that had no business
           starting a login screen, and the defaults are a working one. */
        return -1;
    }

    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return -1;
    }
    fclose(probe);

    return dm_config_load(config, path) == 0 ? 0 : -1;
}
