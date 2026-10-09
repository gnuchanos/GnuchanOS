/*
 * wifi_config.c — the Name = value reader.
 *
 * One setting per line: a name, an `=`, and a value. The file is `config.py`
 * under ~/.config/GnuChanWifi/, because that is the convention the rest of the
 * desktop follows — GnuChanWM and GnuChanRunner both read a `.py` file that is
 * not Python but a plain settings list — and a wifi window that named its file
 * differently would be the one program whose settings are somewhere else.
 *
 * The syntax is the whole of this reader, and it is small: a value is text,
 * and if it is wrapped in quotes those are stripped. So both of these say the
 * same thing and a person writes whichever they prefer:
 *
 *     Title = Wi-Fi
 *     Title = "Wi-Fi"
 *
 * A `#` begins a comment and runs to the end of the line. A line that is blank,
 * a comment, or a name this manager does not have, is skipped without a word:
 * a settings file is lived in, and a name kept for someone's own note is not a
 * mistake worth refusing the whole file for, which is why the Python calls the
 * other programs' files may carry are simply not read rather than refused.
 *
 * There is no call syntax and no list syntax and no booleans, because this
 * manager has nothing to call, one list it fills from nmcli rather than from
 * the file, and no flag that is not already a yes/no colour. That is the whole
 * reason this reader is a hundred lines and not a grammar.
 */
#include "wifi_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- the settings a machine with no file gets ----------------------------- */

void wifi_config_defaults(WifiConfig *config) {
    memset(config, 0, sizeof(*config));

    snprintf(config->nmcli, sizeof(config->nmcli), "nmcli");
    snprintf(config->title, sizeof(config->title), "Wi-Fi");

    snprintf(config->font_family, sizeof(config->font_family), "monospace");
    config->font_size = 14;

    config->width = 520;
    config->rows = 10;

    /* The same purple the window manager, the launcher and the greeter are
       drawn in, so the wifi window is not the one surface with a different
       idea of the desktop's colour. The two extra colours mark a locked
       network and the row of the one already joined. */
    snprintf(config->background, sizeof(config->background), "#1a0b2e");
    snprintf(config->panel, sizeof(config->panel), "#32143f");
    snprintf(config->panel_edge, sizeof(config->panel_edge), "#7b2cbf");
    snprintf(config->field, sizeof(config->field), "#241033");
    snprintf(config->text, sizeof(config->text), "#e0c3fc");
    snprintf(config->text_muted, sizeof(config->text_muted), "#9d7bba");
    snprintf(config->accent, sizeof(config->accent), "#c77dff");
    snprintf(config->secured, sizeof(config->secured), "#ff9e64");
    snprintf(config->connected, sizeof(config->connected), "#9ece6a");

    config->error[0] = '\0';
}

/* --- reading a value ------------------------------------------------------ */

/* Trim leading and trailing whitespace in place, and return the start. A name
   and a value are both read this way, which is what makes "  Title = x  " the
   same as "Title = x". */
static char *trim(char *text) {
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

/* The value of a setting, with a surrounding pair of quotes stripped. The
   quotes are only removed when they match at both ends, so a value that merely
   contains one — a title like "it's" — is left exactly as written. */
static void unquote(char *text) {
    size_t length = strlen(text);
    if (length >= 2 &&
        ((text[0] == '"' && text[length - 1] == '"') ||
         (text[0] == '\'' && text[length - 1] == '\''))) {
        memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
}

/* --- the walk ------------------------------------------------------------- */

/* Apply one `Name = value` line. The value has already been trimmed and
   unquoted. A name this manager does not have is left alone. */
static void apply_setting(WifiConfig *config, const char *name,
                          const char *value) {
    /* Every string setting is copied the same way, with snprintf doing the
       truncation, so a value longer than its buffer is shortened rather than
       overrunning it. */
    #define SET_TEXT(field) \
        snprintf(config->field, sizeof(config->field), "%s", value)

    if (strcmp(name, "NmcliPath") == 0 || strcmp(name, "Nmcli") == 0) {
        SET_TEXT(nmcli);
    } else if (strcmp(name, "Title") == 0) {
        SET_TEXT(title);
    } else if (strcmp(name, "FontFamily") == 0) {
        SET_TEXT(font_family);
    } else if (strcmp(name, "FontSize") == 0) {
        int size = atoi(value);
        if (size > 0) {
            config->font_size = size;
        }
    } else if (strcmp(name, "Width") == 0) {
        int width = atoi(value);
        if (width > 0) {
            config->width = width;
        }
    } else if (strcmp(name, "Rows") == 0) {
        int rows = atoi(value);
        if (rows > 0) {
            config->rows = rows;
        }
    } else if (strcmp(name, "Background") == 0) {
        SET_TEXT(background);
    } else if (strcmp(name, "Panel") == 0) {
        SET_TEXT(panel);
    } else if (strcmp(name, "PanelEdge") == 0) {
        SET_TEXT(panel_edge);
    } else if (strcmp(name, "Field") == 0) {
        SET_TEXT(field);
    } else if (strcmp(name, "Text") == 0) {
        SET_TEXT(text);
    } else if (strcmp(name, "TextMuted") == 0) {
        SET_TEXT(text_muted);
    } else if (strcmp(name, "Accent") == 0) {
        SET_TEXT(accent);
    } else if (strcmp(name, "Secured") == 0) {
        SET_TEXT(secured);
    } else if (strcmp(name, "Connected") == 0) {
        SET_TEXT(connected);
    }
    /* Anything else is a name this manager does not have; it is left alone. */

    #undef SET_TEXT
}

/* --- the public entry points ---------------------------------------------- */

char *wifi_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanWifi/config.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanWifi/config.py", home);
        return buffer;
    }
    /* No home and no XDG directory: there is nowhere to look, and the caller
       reads that as "no file" rather than as an error. */
    buffer[0] = '\0';
    return buffer;
}

int wifi_config_load(WifiConfig *config, const char *path) {
    /* The defaults go down first, and the walk only ever changes what the file
       names. That is what lets a file name one colour and keep the rest of the
       desktop it expects. */
    wifi_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        snprintf(config->error, sizeof(config->error),
                 "could not open %s", path);
        return -1;
    }

    char line[WIFI_TEXT * 2];
    while (fgets(line, sizeof(line), file)) {
        /* A comment runs to the end of the line, and there is no quoting to
           protect a `#` inside a value: a colour or a font name has no `#`
           but its own leading one, which is the one case this must not treat
           as a comment. So a `#` only starts a comment when it is at the start
           of the line or after whitespace — a `#` that begins a value (a
           colour) is kept. */
        char *hash = strchr(line, '#');
        if (hash) {
            if (hash == line || isspace((unsigned char)hash[-1])) {
                *hash = '\0';
            }
        }

        char *content = trim(line);
        if (*content == '\0') {
            continue;
        }

        char *equals = strchr(content, '=');
        if (!equals) {
            /* A line with no `=`: a Python call, or a note. Not a setting, so
               it is skipped rather than refused — the file may carry calls
               meant for another reader. */
            continue;
        }
        *equals = '\0';
        char *name = trim(content);
        char *value = trim(equals + 1);
        unquote(value);

        if (name[0]) {
            apply_setting(config, name, value);
        }
    }
    fclose(file);

    /* Two settings with a sensible floor, clamped here rather than at every
       use: a window narrower than a word, a font of no size, or a scan that
       shows no rows, is a window nobody can use, and the file may say so by
       mistake. */
    if (config->width < 200) {
        config->width = 200;
    }
    if (config->rows < 1) {
        config->rows = 1;
    }
    if (config->rows > 40) {
        config->rows = 40;
    }
    if (config->font_size <= 0) {
        config->font_size = 14;
    }
    if (config->font_family[0] == '\0') {
        snprintf(config->font_family, sizeof(config->font_family), "monospace");
    }
    if (config->title[0] == '\0') {
        snprintf(config->title, sizeof(config->title), "Wi-Fi");
    }

    return 0;
}
