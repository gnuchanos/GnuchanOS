/*
 * fetch_config.c — the walk that gives the settings script its meaning.
 *
 * fetch_parser.c reads the file as a name and a value, or a name and a call.
 * This is the other half: it knows which call a fetch program understands and
 * what each of its arguments means.
 *
 * The one call it understands, whole:
 *
 *     gcl_Fetch.Main(
 *         Image="~/.config/GnuChanFetch/logo.png",   the picture, or ""
 *         ImageRows=16,                              how tall, in text rows
 *         Gap=3,                                     blank columns after it
 *         ImageBackground="#1c0532",                 behind a see-through logo
 *         Fields=["os", "kernel", "cpu"],            which lines, in order
 *         Colours=["#1c0532", "#32143f"],            the swatch under the title
 *     )
 *
 * The defaults are laid down first and the walk only ever changes what the file
 * names, so a script that names one field and not another gets the rest of the
 * program it expects. A field NAME that is not one this program has is reported
 * by fetch_draw.c when the line is drawn, not refused here: the reader's job is
 * to carry the names through, and only the drawer knows the full set.
 *
 * A call that cannot be READ — an unclosed bracket, a missing quote — is
 * refused by the parser, and this module turns that refusal into the message the
 * program prints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fetch_config.h"
#include "fetch_parser.h"

/* The fields shown by a machine that never wrote a script, in this order. They
   are the lines a fetch program is expected to print, and every one of them has
   a reader in fetch_info.c. */
static const char *const DEFAULT_FIELDS[] = {
    "model", "user", "host", "os", "kernel", "server", "uptime",
    "packages", "shell", "wm", "dm", "theme", "icons", "cursor",
    "terminal", "cpu", "gpu", "memory", "disk",
};
#define DEFAULT_FIELD_COUNT \
    ((int)(sizeof(DEFAULT_FIELDS) / sizeof(DEFAULT_FIELDS[0])))

/* The swatch a machine that never wrote a script gets: the desktop's purple
   ramp, the same colours the shipped script names. */
static const char *const DEFAULT_COLOURS[] = {
    "#1c0532", "#32143f", "#542080", "#6d28d9",
    "#9333ea", "#a855f7", "#c77dff", "#d8a4ff",
};
#define DEFAULT_COLOUR_COUNT \
    ((int)(sizeof(DEFAULT_COLOURS) / sizeof(DEFAULT_COLOURS[0])))

/* --- the program a machine with no settings file gets --------------------- */

void fetch_config_defaults(FetchConfig *config) {
    memset(config, 0, sizeof(*config));

    snprintf(config->image, sizeof(config->image),
             "~/.config/GnuChanFetch/logo.png");
    config->image_rows = 16;
    config->gap = 3;
    snprintf(config->image_background, sizeof(config->image_background),
             "#1c0532");

    for (int i = 0; i < DEFAULT_FIELD_COUNT && i < FETCH_MAX_FIELDS; i++) {
        snprintf(config->fields[config->field_count],
                 sizeof(config->fields[0]), "%s", DEFAULT_FIELDS[i]);
        config->field_count++;
    }
    for (int i = 0; i < DEFAULT_COLOUR_COUNT && i < FETCH_MAX_COLOURS; i++) {
        snprintf(config->colours[config->colour_count],
                 sizeof(config->colours[0]), "%s", DEFAULT_COLOURS[i]);
        config->colour_count++;
    }

    config->error[0] = '\0';
}

/* --- reading one call ----------------------------------------------------- */

/* If the call passed an argument called `name`, copy its text into out. Returns
   1 when it did. A caller that already has a value keeps it when this returns
   0, so a script that names one colour and not another gets the program it
   expects for the one it left out. */
static int argument_text(const FetchStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const FetchValue *value = fetch_argument(statement, name);
    if (!value) {
        return 0;
    }
    fetch_value_text(value, out, size);
    return 1;
}

/* A list of names — Fields=[...], Colours=[...] — copied into a fixed set of
   strings. The two are the same shape and the same ceiling, so one reader fills
   both: the list's items are flattened to text, in the order written, and a list
   longer than the ceiling is cut off there rather than overrunning the array. */
static int read_list(const FetchValue *value, char (*out)[FETCH_TEXT_LENGTH],
                     int *count, int maximum) {
    if (!value || value->kind != FETCH_VALUE_LIST) {
        return 0;
    }
    *count = 0;
    for (int i = 0; i < value->item_count && *count < maximum; i++) {
        char text[FETCH_TEXT_LENGTH];
        fetch_value_text(&value->items[i], text, sizeof(text));
        if (!text[0]) {
            continue;
        }
        snprintf(out[*count], FETCH_TEXT_LENGTH, "%s", text);
        (*count)++;
    }
    return 1;
}

/* gcl_Fetch.Main(...). The only call this program has. */
static void read_main(FetchConfig *config, const FetchStatement *statement) {
    char text[FETCH_TEXT_LENGTH];

    /* The picture is read as written, empty included: an empty Image is how a
       person asks for the facts on their own, so an empty value is kept rather
       than treated as "not written". */
    const FetchValue *image = fetch_argument(statement, "Image");
    if (image) {
        fetch_value_text(image, text, sizeof(text));
        snprintf(config->image, sizeof(config->image), "%s", text);
    }

    /* ImageRows has a floor of one: a picture zero rows tall is a picture
       nobody asked for, and a negative one cannot be drawn. */
    config->image_rows = fetch_value_number(
        fetch_argument(statement, "ImageRows"), config->image_rows);
    if (config->image_rows < 1) {
        config->image_rows = 1;
    }

    /* The gap is held at zero or more: negative columns between the picture and
       the text would run them into each other. */
    config->gap = fetch_value_number(
        fetch_argument(statement, "Gap"), config->gap);
    if (config->gap < 0) {
        config->gap = 0;
    }

    /* The picture's background, under its own name and the spelling with a
       "u" — a person writing a style file spells it one way or the other. */
    if (argument_text(statement, "ImageBackground", text, sizeof(text)) ||
        argument_text(statement, "ImageBackgroundColour", text, sizeof(text))) {
        snprintf(config->image_background, sizeof(config->image_background),
                 "%s", text);
    }

    /* A Fields list REPLACES the defaults whole: a script that names the lines
       it wants is naming all of them, and adding them to the built-in twelve
       would print lines nobody asked for. The same rule the window manager's
       bars follow. */
    read_list(fetch_argument(statement, "Fields"), config->fields,
              &config->field_count, FETCH_MAX_FIELDS);

    read_list(fetch_argument(statement, "Colours"), config->colours,
              &config->colour_count, FETCH_MAX_COLOURS);
    /* The American spelling too, so a script written either way is read. If
       both are given, the second read wins, which is what "last one written"
       means everywhere else in these files. */
    if (fetch_argument(statement, "Colors")) {
        read_list(fetch_argument(statement, "Colors"), config->colours,
                  &config->colour_count, FETCH_MAX_COLOURS);
    }
}

/* --- the public entry points ---------------------------------------------- */

char *fetch_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanFetch/GnuChanFetch.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanFetch/GnuChanFetch.py", home);
        return buffer;
    }
    /* No home and no XDG directory: there is nowhere to look, and the caller
       reads that as "no script" rather than as an error. */
    buffer[0] = '\0';
    return buffer;
}

int fetch_config_load(FetchConfig *config, const char *path) {
    /* The defaults are laid down first, and the walk only ever changes what
       the file names. That is what makes a settings file able to name one field
       and get the rest of the program it expects. */
    fetch_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    FetchStatement *statements = NULL;
    int count = 0;
    if (fetch_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 fetch_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == FETCH_STMT_CALL &&
            strcmp(statements[i].target, "gcl_Fetch.Main") == 0) {
            read_main(config, &statements[i]);
        }
        /* Every other statement is a name or a call this program does not act
           on. It is left alone: a script is lived in, and not every name in it
           is meant for here. */
    }
    fetch_parse_free(statements, count);
    return 0;
}

int fetch_config_load_default(FetchConfig *config) {
    char path[FETCH_TEXT_LENGTH];
    fetch_config_path(path, sizeof(path));
    if (!path[0]) {
        fetch_config_defaults(config);
        return -1;
    }
    if (fetch_config_load(config, path) != 0) {
        /* No file, or a file that could not be read: the defaults are what the
           caller wanted anyway, and the reason is in config->error. */
        return -1;
    }
    return 0;
}
