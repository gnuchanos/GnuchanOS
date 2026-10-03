/* notif_config.c — the walk that gives the settings script its meaning. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "notif_config.h"
#include "notif_parser.h"

void notif_config_defaults(NotifConfig *config) {
    memset(config, 0, sizeof(*config));
    snprintf(config->background, sizeof(config->background), "#1a0b2e");
    snprintf(config->background_alt, sizeof(config->background_alt), "#241033");
    snprintf(config->frame, sizeof(config->frame), "#7b2cbf");
    snprintf(config->title, sizeof(config->title), "#e0c3fc");
    snprintf(config->body, sizeof(config->body), "#c9b6e4");
    snprintf(config->accent, sizeof(config->accent), "#c77dff");
    snprintf(config->urgent, sizeof(config->urgent), "#ff5c8a");
    snprintf(config->icon_background, sizeof(config->icon_background), "#1a0b2e");
    config->width = 360;
    config->padding = 14;
    config->margin = 14;
    config->gap = 10;
    config->corner = 14;
    config->icon_size = 48;
    snprintf(config->font, sizeof(config->font), "monospace:pixelsize=13");
    config->body_scale = 90;
    snprintf(config->position, sizeof(config->position), "top-right");
    config->timeout = 5000;
    config->max_visible = 5;
    config->show_icon = 1;
    config->show_body = 1;
    /* The desktop's top bar is this tall, and a bubble under it needs the room
       to clear it. */
    config->top_offset = 50;
    config->show_timer = 1;
    config->max_summary_lines = 3;
    config->max_body_lines = 12;
    config->error[0] = '\0';
}

static int argument_text(const NotifStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const NotifValue *value = notif_argument(statement, name);
    if (!value) {
        return 0;
    }
    notif_value_text(value, out, size);
    return 1;
}

static void argument_int_floor(const NotifStatement *statement,
                               const char *name, int floor, int *out) {
    const NotifValue *value = notif_argument(statement, name);
    if (!value) {
        return;
    }
    int number = notif_value_number(value, *out);
    if (number < floor) {
        return;
    }
    *out = number;
}

static void read_main(NotifConfig *config, const NotifStatement *statement) {
    char text[NOTIF_TEXT_LENGTH];

    if (argument_text(statement, "Background", text, sizeof(text))) {
        snprintf(config->background, sizeof(config->background), "%s", text);
    }
    if (argument_text(statement, "BackgroundAlt", text, sizeof(text)) ||
        argument_text(statement, "BackgroundAltColour", text, sizeof(text))) {
        snprintf(config->background_alt, sizeof(config->background_alt), "%s",
                 text);
    }
    if (argument_text(statement, "Frame", text, sizeof(text)) ||
        argument_text(statement, "Border", text, sizeof(text))) {
        snprintf(config->frame, sizeof(config->frame), "%s", text);
    }
    if (argument_text(statement, "Title", text, sizeof(text))) {
        snprintf(config->title, sizeof(config->title), "%s", text);
    }
    if (argument_text(statement, "Body", text, sizeof(text))) {
        snprintf(config->body, sizeof(config->body), "%s", text);
    }
    if (argument_text(statement, "Accent", text, sizeof(text)) ||
        argument_text(statement, "AccentColour", text, sizeof(text))) {
        snprintf(config->accent, sizeof(config->accent), "%s", text);
    }
    if (argument_text(statement, "Urgent", text, sizeof(text))) {
        snprintf(config->urgent, sizeof(config->urgent), "%s", text);
    }
    if (argument_text(statement, "IconBackground", text, sizeof(text)) ||
        argument_text(statement, "IconBackgroundColour", text, sizeof(text))) {
        snprintf(config->icon_background, sizeof(config->icon_background), "%s",
                 text);
    }

    argument_int_floor(statement, "Width", 120, &config->width);
    argument_int_floor(statement, "Padding", 0, &config->padding);
    argument_int_floor(statement, "Margin", 0, &config->margin);
    argument_int_floor(statement, "Gap", 0, &config->gap);
    argument_int_floor(statement, "Corner", 0, &config->corner);
    argument_int_floor(statement, "IconSize", 0, &config->icon_size);

    if (argument_text(statement, "Font", text, sizeof(text))) {
        snprintf(config->font, sizeof(config->font), "%s", text);
    }
    argument_int_floor(statement, "BodyScale", 10, &config->body_scale);
    if (config->body_scale > 400) {
        config->body_scale = 400;
    }

    if (argument_text(statement, "Position", text, sizeof(text))) {
        snprintf(config->position, sizeof(config->position), "%s", text);
    }
    argument_int_floor(statement, "Timeout", 0, &config->timeout);
    config->max_visible = notif_value_number(
        notif_argument(statement, "MaxVisible"), config->max_visible);
    if (config->max_visible < 1) {
        config->max_visible = 1;
    }
    if (config->max_visible > NOTIF_MAX_VISIBLE) {
        config->max_visible = NOTIF_MAX_VISIBLE;
    }

    config->show_icon = notif_value_bool(
        notif_argument(statement, "ShowIcon"), config->show_icon);
    config->show_body = notif_value_bool(
        notif_argument(statement, "ShowBody"), config->show_body);

    argument_int_floor(statement, "TopOffset", 0, &config->top_offset);
    config->show_timer = notif_value_bool(
        notif_argument(statement, "ShowTimer"), config->show_timer);
    argument_int_floor(statement, "MaxSummaryLines", 1,
                       &config->max_summary_lines);
    argument_int_floor(statement, "MaxBodyLines", 1, &config->max_body_lines);
    if (config->max_summary_lines > 8) {
        config->max_summary_lines = 8;
    }
    if (config->max_body_lines > 40) {
        config->max_body_lines = 40;
    }
}

char *notif_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size,
                 "%s/GnuChanNotification/GnuChanNotification.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size,
                 "%s/.config/GnuChanNotification/GnuChanNotification.py", home);
        return buffer;
    }
    buffer[0] = '\0';
    return buffer;
}

int notif_config_load(NotifConfig *config, const char *path) {
    notif_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    NotifStatement *statements = NULL;
    int count = 0;
    if (notif_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 notif_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == NOTIF_STMT_CALL &&
            strcmp(statements[i].target, "gcl_Notification.Main") == 0) {
            read_main(config, &statements[i]);
        }
    }
    notif_parse_free(statements, count);
    return 0;
}

int notif_config_load_default(NotifConfig *config) {
    char path[NOTIF_TEXT_LENGTH];
    notif_config_path(path, sizeof(path));
    if (!path[0]) {
        notif_config_defaults(config);
        return -1;
    }
    if (notif_config_load(config, path) != 0) {
        return -1;
    }
    return 0;
}

unsigned long notif_config_colour(void *display_pointer, int screen,
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
