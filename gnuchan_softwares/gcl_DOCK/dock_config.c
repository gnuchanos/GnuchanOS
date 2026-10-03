/*
 * dock_config.c — settings walk. Reads gcl_Dock.Main(...) from the script.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "dock_config.h"
#include "dock_parser.h"

void dock_config_defaults(DockConfig *config) {
    memset(config, 0, sizeof(*config));

    snprintf(config->background, sizeof(config->background), "#1a0b2e");
    snprintf(config->background_edge, sizeof(config->background_edge),
             "#7b2cbf");
    snprintf(config->field, sizeof(config->field), "#241033");
    snprintf(config->text, sizeof(config->text), "#e0c3fc");
    snprintf(config->accent, sizeof(config->accent), "#c77dff");
    snprintf(config->badge, sizeof(config->badge), "#1a0b2e");

    config->icon_size = 48;
    config->gap = 30;
    config->padding = 10;
    config->margin = 6;
    config->corner = 16;
    config->magnify = 18;
    config->magnify_reach = 2;

    snprintf(config->font, sizeof(config->font), "monospace:pixelsize=15");
    config->label_enabled = 1;

    config->settings_enabled = 1;
    snprintf(config->settings_icon, sizeof(config->settings_icon),
             "~/.config/GnuChanDock/settings.png");
    snprintf(config->settings_label, sizeof(config->settings_label),
             "settings");

    config->terminal_enabled = 1;
    snprintf(config->terminal_icon, sizeof(config->terminal_icon),
             "~/.config/GnuChanDock/logo.png");
    snprintf(config->terminal_label, sizeof(config->terminal_label),
             "terminal");
    snprintf(config->terminal_command, sizeof(config->terminal_command),
             "GnuChanTerm");

    config->show_running = 1;
    config->error[0] = '\0';
}

static int argument_text(const DockStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const DockValue *value = dock_argument(statement, name);
    if (!value) {
        return 0;
    }
    dock_value_text(value, out, size);
    return 1;
}

static void argument_int_floor(const DockStatement *statement,
                               const char *name, int floor, int *out) {
    const DockValue *value = dock_argument(statement, name);
    if (!value) {
        return;
    }
    int number = dock_value_number(value, *out);
    if (number < floor) {
        return;
    }
    *out = number;
}

static void read_main(DockConfig *config, const DockStatement *statement) {
    char text[DOCK_TEXT_LENGTH];

    if (argument_text(statement, "Background", text, sizeof(text))) {
        snprintf(config->background, sizeof(config->background), "%s", text);
    }
    if (argument_text(statement, "BackgroundEdge", text, sizeof(text)) ||
        argument_text(statement, "BackgroundColour", text, sizeof(text))) {
        snprintf(config->background_edge, sizeof(config->background_edge),
                 "%s", text);
    }
    if (argument_text(statement, "Field", text, sizeof(text))) {
        snprintf(config->field, sizeof(config->field), "%s", text);
    }
    if (argument_text(statement, "Text", text, sizeof(text))) {
        snprintf(config->text, sizeof(config->text), "%s", text);
    }
    if (argument_text(statement, "Accent", text, sizeof(text)) ||
        argument_text(statement, "AccentColor", text, sizeof(text))) {
        snprintf(config->accent, sizeof(config->accent), "%s", text);
    }
    if (argument_text(statement, "Badge", text, sizeof(text))) {
        snprintf(config->badge, sizeof(config->badge), "%s", text);
    }

    argument_int_floor(statement, "IconSize", 8, &config->icon_size);
    argument_int_floor(statement, "Gap", 0, &config->gap);
    argument_int_floor(statement, "Padding", 0, &config->padding);
    argument_int_floor(statement, "Margin", 0, &config->margin);
    argument_int_floor(statement, "Corner", 0, &config->corner);
    argument_int_floor(statement, "Magnify", 0, &config->magnify);
    argument_int_floor(statement, "MagnifyReach", 0, &config->magnify_reach);

    if (argument_text(statement, "Font", text, sizeof(text))) {
        snprintf(config->font, sizeof(config->font), "%s", text);
    }
    config->label_enabled = dock_value_bool(
        dock_argument(statement, "LabelEnabled"), config->label_enabled);

    config->settings_enabled = dock_value_bool(
        dock_argument(statement, "SettingsEnabled"), config->settings_enabled);
    if (argument_text(statement, "SettingsIcon", text, sizeof(text))) {
        snprintf(config->settings_icon, sizeof(config->settings_icon), "%s",
                 text);
    }
    if (argument_text(statement, "SettingsLabel", text, sizeof(text))) {
        snprintf(config->settings_label, sizeof(config->settings_label), "%s",
                 text);
    }

    config->terminal_enabled = dock_value_bool(
        dock_argument(statement, "TerminalEnabled"), config->terminal_enabled);
    if (argument_text(statement, "TerminalIcon", text, sizeof(text))) {
        snprintf(config->terminal_icon, sizeof(config->terminal_icon), "%s",
                 text);
    }
    if (argument_text(statement, "TerminalLabel", text, sizeof(text))) {
        snprintf(config->terminal_label, sizeof(config->terminal_label), "%s",
                 text);
    }
    if (argument_text(statement, "TerminalCommand", text, sizeof(text))) {
        snprintf(config->terminal_command, sizeof(config->terminal_command),
                 "%s", text);
    }

    config->show_running = dock_value_bool(
        dock_argument(statement, "ShowRunning"), config->show_running);
}

char *dock_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanDock/GnuChanDock.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanDock/GnuChanDock.py", home);
        return buffer;
    }
    buffer[0] = '\0';
    return buffer;
}

int dock_config_load(DockConfig *config, const char *path) {
    dock_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    DockStatement *statements = NULL;
    int count = 0;
    if (dock_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 dock_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == DOCK_STMT_CALL &&
            strcmp(statements[i].target, "gcl_Dock.Main") == 0) {
            read_main(config, &statements[i]);
        }
    }
    dock_parse_free(statements, count);
    return 0;
}

int dock_config_load_default(DockConfig *config) {
    char path[DOCK_TEXT_LENGTH];
    dock_config_path(path, sizeof(path));
    if (!path[0]) {
        dock_config_defaults(config);
        return -1;
    }
    if (dock_config_load(config, path) != 0) {
        return -1;
    }
    return 0;
}

unsigned long dock_config_colour(void *display_pointer, int screen,
                                 const char *name, unsigned long fallback) {
    Display *display = (Display *)display_pointer;
    if (!display || !name || !name[0]) {
        return fallback;
    }
    Colormap colormap = DefaultColormap(display, screen);
    XColor colour;
    if (XAllocNamedColor(display, colormap, name, &colour, &colour)) {
        return colour.pixel;
    }
    return fallback;
}
