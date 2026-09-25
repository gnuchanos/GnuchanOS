/*
 * dm_login.c — the layout and the drawing of the login screen.
 *
 * This is the only module that decides where anything is. It computes the
 * rectangles into the core, and both draws them and answers hit-tests from the
 * same numbers — so a click always lands on what the user saw.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dm_core.h"

static void screen_title(char *out, size_t size) {
    char host[128];
    if (gethostname(host, sizeof(host)) != 0) host[0] = '\0';
    host[sizeof(host) - 1] = '\0';
    snprintf(out, size, "%s", host[0] ? host : "GnuChanOS");
}

static void layout(DmCore *core) {
    const DmStyle *s = &core->style;
    int panel_width = s->panel_width;
    if (panel_width > core->width - 2 * s->margin) panel_width = core->width - 2 * s->margin;

    int field_width = panel_width - 2 * s->margin;
    int panel_height = s->margin + s->field_height + s->gap
                     + s->field_height + s->gap
                     + s->button_height + s->gap
                     + s->field_height + s->margin;

    int panel_x = (core->width - panel_width) / 2;
    int panel_y = (core->height - panel_height) / 2;
    if (panel_y < 0) panel_y = 0;

    int inner_x = panel_x + s->margin;
    int y = panel_y + s->margin;

    core->field_box[DM_FIELD_USERNAME].x = inner_x;
    core->field_box[DM_FIELD_USERNAME].y = y;
    core->field_box[DM_FIELD_USERNAME].width = field_width;
    core->field_box[DM_FIELD_USERNAME].height = s->field_height;
    y += s->field_height + s->gap;

    core->field_box[DM_FIELD_PASSWORD].x = inner_x;
    core->field_box[DM_FIELD_PASSWORD].y = y;
    core->field_box[DM_FIELD_PASSWORD].width = field_width;
    core->field_box[DM_FIELD_PASSWORD].height = s->field_height;
    y += s->field_height + s->gap;

    core->button_box.x = inner_x;
    core->button_box.y = y;
    core->button_box.width = field_width;
    core->button_box.height = s->button_height;

    int bar_y = core->height - s->button_height - s->margin;
    if (bar_y < panel_y + panel_height + s->gap) bar_y = panel_y + panel_height + s->gap;
    core->power_box[0].x = s->margin;
    core->power_box[0].y = bar_y;
    core->power_box[0].width = 140;
    core->power_box[0].height = s->button_height;
    core->power_box[1].x = core->width - s->margin - 140;
    core->power_box[1].y = bar_y;
    core->power_box[1].width = 140;
    core->power_box[1].height = s->button_height;

    core->panel_box.x = panel_x;
    core->panel_box.y = panel_y;
    core->panel_box.width = panel_width;
    core->panel_box.height = panel_height;
}

static void box_text_centred(DmCore *core, int x, int y, int width, int height,
                             const char *text, XFontStruct *font,
                             unsigned long colour) {
    int text_width = dm_draw_text_width(font, text, (int)strlen(text));
    int ascent = font ? font->ascent : 8;
    int descent = font ? font->descent : 2;
    int baseline = y + (height + ascent - descent) / 2;
    dm_draw_text(core, x + (width - text_width) / 2, baseline, text, font, colour);
}

static void draw_field(DmCore *core, DmField field, const char *label,
                       const char *value, int length, int secret) {
    const DmStyle *s = &core->style;
    int x = core->field_box[field].x;
    int y = core->field_box[field].y;
    int w = core->field_box[field].width;
    int h = core->field_box[field].height;

    unsigned long fill = (core->focused == field) ? s->field_focus : s->field;
    unsigned long edge = (core->focused == field) ? s->accent : s->panel_edge;
    dm_draw_box(core, x, y, w, h, fill, edge, 1);

    int padding = 10;
    int ascent = s->font_field ? s->font_field->ascent : 8;
    int descent = s->font_field ? s->font_field->descent : 2;
    int baseline = y + (h + ascent - descent) / 2;
    dm_draw_text(core, x + padding, baseline, label, s->font_label, s->text_muted);

    int value_x = x + padding
                + dm_draw_text_width(s->font_label, label, (int)strlen(label))
                + padding;

    if (length == 0) {
        dm_draw_text(core, value_x, baseline, "...", s->font_field, s->text_muted);
        return;
    }
    if (secret) {
        char masked[DM_MAX_PASSWORD];
        int count = length;
        if (count > (int)sizeof(masked) - 1) count = (int)sizeof(masked) - 1;
        memset(masked, '*', (size_t)count);
        masked[count] = '\0';
        dm_draw_text(core, value_x, baseline, masked, s->font_field, s->text);
        return;
    }
    dm_draw_text(core, value_x, baseline, value, s->font_field, s->text);
}

static void draw_button(DmCore *core, int x, int y, int w, int h,
                        const char *label, unsigned long fill) {
    const DmStyle *s = &core->style;
    dm_draw_box(core, x, y, w, h, fill, s->panel_edge, 1);
    box_text_centred(core, x, y, w, h, label, s->font_label, s->text);
}

static void login_draw(DmCore *core) {
    const DmStyle *s = &core->style;
    layout(core);
    dm_draw_clear(core, s->background);

    char title[160];
    screen_title(title, sizeof(title));
    int title_width = dm_draw_text_width(s->font_title, title, (int)strlen(title));
    dm_draw_text(core, (core->width - title_width) / 2,
                 core->panel_box.y - s->gap, title, s->font_title, s->text);

    dm_draw_box(core, core->panel_box.x, core->panel_box.y,
                core->panel_box.width, core->panel_box.height,
                s->panel, s->panel_edge, 1);

    draw_field(core, DM_FIELD_USERNAME, "user", core->username, core->username_length, 0);
    draw_field(core, DM_FIELD_PASSWORD, "pass", core->password, core->password_length, 1);

    unsigned long signin =
        (core->username_length > 0 && core->password_length > 0) ? s->accent : s->accent_dim;
    draw_button(core, core->button_box.x, core->button_box.y,
                core->button_box.width, core->button_box.height, "sign in", signin);

    int message_y = core->button_box.y + core->button_box.height + s->gap;
    if (core->message[0]) {
        int w = dm_draw_text_width(s->font_label, core->message, (int)strlen(core->message));
        int ascent = s->font_label ? s->font_label->ascent : 8;
        dm_draw_text(core, (core->width - w) / 2, message_y + ascent,
                     core->message, s->font_label,
                     core->message_is_error ? s->danger : s->text_muted);
    }

    draw_button(core, core->power_box[0].x, core->power_box[0].y,
                core->power_box[0].width, core->power_box[0].height, "reboot", s->panel);
    draw_button(core, core->power_box[1].x, core->power_box[1].y,
                core->power_box[1].width, core->power_box[1].height, "shut down", s->panel);
}

static int inside(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

int dm_login_field_at(DmCore *core, int x, int y) {
    layout(core);
    for (int i = 0; i < DM_FIELD_COUNT; i++) {
        if (inside(x, y, core->field_box[i].x, core->field_box[i].y,
                   core->field_box[i].width, core->field_box[i].height)) return i;
    }
    return -1;
}

DmButton dm_login_button_at(DmCore *core, int x, int y) {
    layout(core);
    if (inside(x, y, core->button_box.x, core->button_box.y,
               core->button_box.width, core->button_box.height)) return DM_BUTTON_SIGNIN;
    if (inside(x, y, core->power_box[0].x, core->power_box[0].y,
               core->power_box[0].width, core->power_box[0].height)) return DM_BUTTON_REBOOT;
    if (inside(x, y, core->power_box[1].x, core->power_box[1].y,
               core->power_box[1].width, core->power_box[1].height)) return DM_BUTTON_SHUTDOWN;
    return DM_BUTTON_NONE;
}

const DmModule dm_login_module = {
    .name = "login", .init = NULL, .event = NULL, .draw = login_draw, .cleanup = NULL,
};
