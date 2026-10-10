/*
 * net_config.c — the Name = value reader.
 *
 * One setting per line: a name, an `=`, and a value. The reader is the same
 * small one the wifi manager uses, and the reason is the same: this program has
 * no calls and no lists in its file, so a reader with those would be a grammar
 * nobody needed. A `#` begins a comment; a value may be wrapped in quotes and
 * they are stripped when they match at both ends.
 */
#include "net_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void net_config_defaults(NetConfig *config) {
    memset(config, 0, sizeof(*config));

    snprintf(config->nmcli, sizeof(config->nmcli), "nmcli");
    snprintf(config->ip, sizeof(config->ip), "ip");
    snprintf(config->resolvectl, sizeof(config->resolvectl), "resolvectl");
    snprintf(config->wifi_manager, sizeof(config->wifi_manager),
             "GnuChanWifi");

    snprintf(config->title, sizeof(config->title), "Network");

    snprintf(config->font_family, sizeof(config->font_family), "monospace");
    config->font_size = 14;

    config->width = 640;
    config->running_rows = 6;

    /* The same purple the rest of the desktop is drawn in, so this window is
       not the one surface with a different idea of the desktop's colour. The
       two state colours mark a device that is up and one that is down. */
    snprintf(config->background, sizeof(config->background), "#1a0b2e");
    snprintf(config->panel, sizeof(config->panel), "#32143f");
    snprintf(config->panel_edge, sizeof(config->panel_edge), "#7b2cbf");
    snprintf(config->field, sizeof(config->field), "#241033");
    snprintf(config->text, sizeof(config->text), "#e0c3fc");
    snprintf(config->text_muted, sizeof(config->text_muted), "#9d7bba");
    snprintf(config->accent, sizeof(config->accent), "#c77dff");
    snprintf(config->connected, sizeof(config->connected), "#9ece6a");
    snprintf(config->disabled, sizeof(config->disabled), "#ff9e64");
    snprintf(config->selection, sizeof(config->selection), "#5a2a8f");

    config->error[0] = '\0';
}

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

static void unquote(char *text) {
    size_t length = strlen(text);
    if (length >= 2 &&
        ((text[0] == '"' && text[length - 1] == '"') ||
         (text[0] == '\'' && text[length - 1] == '\''))) {
        memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
}

static void apply_setting(NetConfig *config, const char *name,
                          const char *value) {
    #define SET_TEXT(field) \
        snprintf(config->field, sizeof(config->field), "%s", value)

    if (strcmp(name, "NmcliPath") == 0 || strcmp(name, "Nmcli") == 0) {
        SET_TEXT(nmcli);
    } else if (strcmp(name, "IpPath") == 0 || strcmp(name, "Ip") == 0) {
        SET_TEXT(ip);
    } else if (strcmp(name, "ResolvectlPath") == 0 ||
               strcmp(name, "Resolvectl") == 0) {
        SET_TEXT(resolvectl);
    } else if (strcmp(name, "WifiManager") == 0 ||
               strcmp(name, "WifiProgram") == 0) {
        SET_TEXT(wifi_manager);
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
    } else if (strcmp(name, "RunningRows") == 0 ||
               strcmp(name, "Rows") == 0) {
        int rows = atoi(value);
        if (rows > 0) {
            config->running_rows = rows;
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
    } else if (strcmp(name, "Connected") == 0) {
        SET_TEXT(connected);
    } else if (strcmp(name, "Disabled") == 0) {
        SET_TEXT(disabled);
    } else if (strcmp(name, "Selection") == 0) {
        SET_TEXT(selection);
    }
    /* Anything else is a name this manager does not have; it is left alone. */

    #undef SET_TEXT
}

char *net_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanNetworkManager/config.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanNetworkManager/config.py",
                 home);
        return buffer;
    }
    buffer[0] = '\0';
    return buffer;
}

int net_config_load(NetConfig *config, const char *path) {
    net_config_defaults(config);

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

    char line[NET_TEXT * 2];
    while (fgets(line, sizeof(line), file)) {
        /* A `#` only starts a comment at the start of a line or after
           whitespace: a `#` that begins a value is a colour, and cutting there
           would throw the colour away. */
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

    /* The settings with a sensible floor, clamped here rather than at every
       use: a window narrower than a word, a font of no size, or a list that
       shows no rows, is a window nobody can use. */
    if (config->width < 320) {
        config->width = 320;
    }
    if (config->running_rows < 1) {
        config->running_rows = 1;
    }
    if (config->running_rows > 40) {
        config->running_rows = 40;
    }
    if (config->font_size <= 0) {
        config->font_size = 14;
    }
    if (config->font_family[0] == '\0') {
        snprintf(config->font_family, sizeof(config->font_family), "monospace");
    }
    if (config->title[0] == '\0') {
        snprintf(config->title, sizeof(config->title), "Network");
    }

    return 0;
}
