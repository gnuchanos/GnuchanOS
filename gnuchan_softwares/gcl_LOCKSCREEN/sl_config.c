/*
 * sl_config.c — the walk that gives the settings script its meaning.
 *
 * sl_parser.c reads the file as a name and a value, or a name and a call. This
 * is the other half: it knows which call a lock screen understands and what
 * each of its arguments means.
 *
 * The one call it understands, whole:
 *
 *     gcl_SL.Lock(
 *         Background="#0d0512",     the screen behind everything
 *         Panel="#1b0c22",          the box the password sits in
 *         PanelEdge="#7b2cbf",
 *         Field="#241033",          the password field
 *         Text="#e0c3fc",
 *         TextMuted="#9d7bba",
 *         Accent="#d400ff",
 *         Wrong="#ff4d6d",          the flash when a password is wrong
 *         FontFamily="monospace",
 *         FontSize=14,
 *         Title="...",              the heading
 *         Subtitle="...",           the line under it
 *         Prompt="Password:",
 *         ClockFormat="%H:%M",      empty for no clock
 *         ShowPasswordDots=True,
 *         WrongSeconds=2,
 *     )
 *
 * A name that is not one of these is not a mistake worth refusing the file for:
 * a script is written while it is being lived with. That is different from a
 * statement that cannot be READ — an unclosed bracket, a missing quote — which
 * the parser refuses, and this module turns that refusal into the message the
 * program prints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sl_config.h"
#include "sl_parser.h"

/* --- the lock screen a machine with no settings file gets ----------------- */

void sl_config_defaults(SlConfig *config) {
    memset(config, 0, sizeof(*config));

    /* The desktop's own purple: the same names GnuChanWM and GnuChanSS use, so
       the lock screen looks like the session it belongs to out of the box. */
    snprintf(config->background, sizeof(config->background), "#0d0512");
    snprintf(config->panel, sizeof(config->panel), "#1b0c22");
    snprintf(config->panel_edge, sizeof(config->panel_edge), "#7b2cbf");
    snprintf(config->field, sizeof(config->field), "#241033");
    snprintf(config->text, sizeof(config->text), "#e0c3fc");
    snprintf(config->text_muted, sizeof(config->text_muted), "#9d7bba");
    snprintf(config->accent, sizeof(config->accent), "#d400ff");
    snprintf(config->wrong, sizeof(config->wrong), "#ff4d6d");

    snprintf(config->font_family, sizeof(config->font_family), "monospace");
    config->font_size = 14;

    /* The words name the user and ask for the password; a machine that wants
       its own sets them in the script. The title is filled in at start-up with
       the user's own name, so it is left empty here and defaulted there. */
    config->title[0] = '\0';
    snprintf(config->subtitle, sizeof(config->subtitle), "Enter your password");
    snprintf(config->prompt, sizeof(config->prompt), "Password:");

    /* "%s" with the format as the argument, not as the format itself: strftime
       reads "%H:%M", printf does not, and passing it as printf's own format is
       how a clock ends up drawing nothing. */
    snprintf(config->clock_format, sizeof(config->clock_format), "%s",
             "%H:%M");

    config->show_password_dots = 1;
    config->wrong_seconds = 2;
    config->error[0] = '\0';
}

/* --- reading one call ----------------------------------------------------- */

/* If the call passed an argument called `name`, copy its text into out. Returns
   1 when it did. A caller that already has a value keeps it when this returns
   0. */
static int argument_text(const SlStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const SlValue *value = sl_argument(statement, name);
    if (!value) {
        return 0;
    }
    sl_value_text(value, out, size);
    return out[0] != '\0';
}

/* One colour, read under whichever of its two spellings the script used. The
   colour words are the same for both spellings; only Color/Colour differs. */
static void read_colour(const SlStatement *statement, const char *name,
                        char *destination, unsigned int size) {
    char buffer[SL_TEXT_LENGTH];
    char name_colour[SL_TEXT_LENGTH];
    snprintf(name_colour, sizeof(name_colour), "%sColour", name);
    if (argument_text(statement, name, buffer, sizeof(buffer)) ||
        argument_text(statement, name_colour, buffer, sizeof(buffer))) {
        snprintf(destination, size, "%s", buffer);
    }
}

/* gcl_SL.Lock(...). The only call this program has. */
static void read_lock(SlConfig *config, const SlStatement *statement) {
    read_colour(statement, "Background", config->background,
                sizeof(config->background));
    read_colour(statement, "Panel", config->panel, sizeof(config->panel));
    read_colour(statement, "PanelEdge", config->panel_edge,
                sizeof(config->panel_edge));
    read_colour(statement, "Field", config->field, sizeof(config->field));
    read_colour(statement, "Text", config->text, sizeof(config->text));
    read_colour(statement, "TextMuted", config->text_muted,
                sizeof(config->text_muted));
    read_colour(statement, "Accent", config->accent, sizeof(config->accent));
    read_colour(statement, "Wrong", config->wrong, sizeof(config->wrong));

    char text[SL_TEXT_LENGTH];
    if (argument_text(statement, "FontFamily", text, sizeof(text))) {
        snprintf(config->font_family, sizeof(config->font_family), "%s", text);
    }
    config->font_size = sl_value_number(sl_argument(statement, "FontSize"),
                                        config->font_size);

    if (argument_text(statement, "Title", text, sizeof(text))) {
        snprintf(config->title, sizeof(config->title), "%s", text);
    }
    if (argument_text(statement, "Subtitle", text, sizeof(text))) {
        snprintf(config->subtitle, sizeof(config->subtitle), "%s", text);
    }
    if (argument_text(statement, "Prompt", text, sizeof(text))) {
        snprintf(config->prompt, sizeof(config->prompt), "%s", text);
    }
    /* The clock format is read even when empty: a script that writes
       ClockFormat="" is asking for no clock, and that has to stick. */
    const SlValue *clock = sl_argument(statement, "ClockFormat");
    if (clock) {
        sl_value_text(clock, config->clock_format,
                      sizeof(config->clock_format));
    }

    config->show_password_dots = sl_value_bool(
        sl_argument(statement, "ShowPasswordDots"),
        config->show_password_dots);
    config->wrong_seconds = sl_value_number(
        sl_argument(statement, "WrongSeconds"), config->wrong_seconds);
}

/* --- the public entry points ---------------------------------------------- */

char *sl_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanSL/GnuChanSL.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanSL/GnuChanSL.py", home);
        return buffer;
    }
    buffer[0] = '\0';
    return buffer;
}

int sl_config_load(SlConfig *config, const char *path) {
    /* The defaults are laid down first, and the walk only ever changes what the
       file names. */
    sl_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    SlStatement *statements = NULL;
    int count = 0;
    if (sl_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 sl_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == SL_STMT_CALL &&
            strcmp(statements[i].target, "gcl_SL.Lock") == 0) {
            read_lock(config, &statements[i]);
        }
        /* Every other statement is a name or a call this program does not act
           on. It is left alone: a script is lived in. */
    }
    sl_parse_free(statements, count);

    /* Two settings with a sensible floor, clamped here rather than at the use:
       a font of no size or a message that stays up for no time is a screen
       nobody can read. */
    if (config->font_size <= 0) {
        config->font_size = 14;
    }
    if (config->font_family[0] == '\0') {
        snprintf(config->font_family, sizeof(config->font_family), "monospace");
    }
    if (config->wrong_seconds < 1) {
        config->wrong_seconds = 1;
    }
    return 0;
}

int sl_config_load_default(SlConfig *config) {
    char path[SL_TEXT_LENGTH];
    sl_config_path(path, sizeof(path));
    if (!path[0]) {
        sl_config_defaults(config);
        return -1;
    }
    if (sl_config_load(config, path) != 0) {
        return -1;
    }
    return 0;
}
