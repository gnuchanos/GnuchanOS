/*
 * dm_login.c — the layout and the drawing of the login screen.
 *
 * This is the only module that decides where anything is. It computes the
 * rectangles into the core, and both draws them and answers hit-tests from
 * the same numbers — so a click always lands on what the user saw.
 *
 * The screen is three bands:
 *
 *     the host name, centred
 *     the login panel: user, password, sign in
 *     the bottom bar: the session selector on the left, the power buttons on
 *                     the right
 *
 * The bottom bar is the one part that is not inside the panel, because it does
 * not belong to the act of logging in: which session to start and whether to
 * power the machine off are choices about the machine, and a user makes them
 * before typing a password or without ever typing one.
 *
 * The session list drops up from its button, because the button is at the
 * bottom of the screen and a list that dropped down would be off it.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dm_core.h"

/* The width of the session selector. Fixed rather than matched to the longest
   name: a machine with a session called "Xfce Session (development build)"
   must not push the power buttons off the screen. */
#define SESSION_BOX_WIDTH 220
#define SESSION_ROW_HEIGHT 26

static void screen_title(char *out, size_t size) {
    char host[128];
    if (gethostname(host, sizeof(host)) != 0) host[0] = '\0';
    host[sizeof(host) - 1] = '\0';
    snprintf(out, size, "%s", host[0] ? host : "GnuChanOS");
}

static int signin_ready(const DmCore *core) {
    return core->username_length > 0 && core->password_length > 0;
}

/* --- the layout ----------------------------------------------------------- */

static void layout_panel(DmCore *core) {
    const DmStyle *s = &core->style;
    int panel_width = s->panel_width;
    if (panel_width > core->width - 2 * s->margin) {
        panel_width = core->width - 2 * s->margin;
    }

    int field_width = panel_width - 2 * s->margin;
    int panel_height = s->margin + s->field_height + s->gap
                     + s->field_height + s->gap
                     + s->button_height + s->gap
                     + s->field_height + s->margin;

    /* The panel sits a little above centre, so the bottom bar has room of its
       own and the two do not look crowded together. */
    int panel_x = (core->width - panel_width) / 2;
    int panel_y = (core->height - panel_height) / 2 - s->button_height;
    if (panel_y < s->gap) {
        panel_y = s->gap;
    }

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

    core->panel_box.x = panel_x;
    core->panel_box.y = panel_y;
    core->panel_box.width = panel_width;
    core->panel_box.height = panel_height;
}

/* The bottom bar: the session selector on the left, the power buttons on the
   right, both on the same row so they read as one bar rather than two floating
   buttons. */
static void layout_bottom_bar(DmCore *core) {
    const DmStyle *s = &core->style;
    int height = s->button_height;

    int bar_y = core->height - height - s->margin;
    int panel_bottom = core->panel_box.y + core->panel_box.height + s->gap;
    if (bar_y < panel_bottom) {
        bar_y = panel_bottom;
    }

    int session_width = SESSION_BOX_WIDTH;
    if (session_width > core->width / 2) {
        session_width = core->width / 2 - s->margin;
    }

    core->session_box.x = s->margin;
    core->session_box.y = bar_y;
    core->session_box.width = session_width;
    core->session_box.height = height;

    int power_width = 150;
    int power_gap = s->gap;
    int pair_width = power_width * 2 + power_gap;
    int pair_x = core->width - s->margin - pair_width;
    if (pair_x < core->session_box.x + core->session_box.width + s->gap) {
        pair_x = core->session_box.x + core->session_box.width + s->gap;
    }

    core->power_box[0].x = pair_x;
    core->power_box[0].y = bar_y;
    core->power_box[0].width = power_width;
    core->power_box[0].height = height;
    core->power_box[1].x = pair_x + power_width + power_gap;
    core->power_box[1].y = bar_y;
    core->power_box[1].width = power_width;
    core->power_box[1].height = height;
}

/* The dropped-up list: one row per session, its bottom edge on the top of the
   selector so the two touch and read as one control. */
static void layout_session_list(DmCore *core) {
    core->session_list_box = core->session_box;

    if (core->session_count <= 0) {
        core->session_list_box.height = 0;
        return;
    }

    int height = core->session_count * SESSION_ROW_HEIGHT;
    int list_y = core->session_box.y - height;
    if (list_y < 0) {
        /* More sessions than there is room above the bar: the list is moved
           down and allowed to cover the bar, which is better than starting it
           off the screen. */
        list_y = 0;
        height = core->session_box.y;
        if (height < SESSION_ROW_HEIGHT) {
            height = SESSION_ROW_HEIGHT;
        }
    }

    core->session_list_box.x = core->session_box.x;
    core->session_list_box.y = list_y;
    core->session_list_box.width = core->session_box.width;
    core->session_list_box.height = height;
}

static void layout(DmCore *core) {
    layout_panel(core);
    layout_bottom_bar(core);
    layout_session_list(core);
    /* The rows follow from the list box, so they are placed here rather than
       at draw time: a click is hit-tested against them before anything is
       drawn, and both have to come from the same numbers. */
    dm_login_layout_sessions(core);
}

/* --- small drawing helpers ------------------------------------------------ */

static void box_text_centred(DmCore *core, int x, int y, int width, int height,
                             const char *text, XFontStruct *font,
                             unsigned long colour) {
    int text_width = dm_draw_text_width(font, text, (int)strlen(text));
    int ascent = font ? font->ascent : 8;
    int descent = font ? font->descent : 2;
    int baseline = y + (height + ascent - descent) / 2;
    dm_draw_text(core, x + (width - text_width) / 2, baseline, text, font, colour);
}

/* Text shortened to fit, ending in an ellipsis. A session name is written by
   whoever installed the session and can be any length; a fixed box means the
   name has to give, not the box. */
static void text_clipped(DmCore *core, int x, int baseline, const char *text,
                         XFontStruct *font, unsigned long colour, int room) {
    if (!font || !text || room <= 0) {
        return;
    }
    if (dm_draw_text_width(font, text, (int)strlen(text)) <= room) {
        dm_draw_text(core, x, baseline, text, font, colour);
        return;
    }
    size_t length = strlen(text);
    while (length > 0) {
        length--;
        char attempt[192];
        snprintf(attempt, sizeof(attempt), "%.*s...", (int)length, text);
        if (dm_draw_text_width(font, attempt, (int)strlen(attempt)) <= room) {
            dm_draw_text(core, x, baseline, attempt, font, colour);
            return;
        }
    }
    dm_draw_text(core, x, baseline, "...", font, colour);
}

static void draw_field(DmCore *core, DmField field, const char *label,
                       const char *value, int length, int secret) {
    const DmStyle *s = &core->style;
    int focused = (core->focus == (DmFocus)field);
    int x = core->field_box[field].x;
    int y = core->field_box[field].y;
    int w = core->field_box[field].width;
    int h = core->field_box[field].height;

    unsigned long fill = focused ? s->field_focus : s->field;
    unsigned long edge = focused ? s->accent : s->panel_edge;
    dm_draw_box(core, x, y, w, h, fill, edge, focused ? 2 : 1);

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

/* A button's fill says two things at once: whether the keyboard is on it, and
   — for sign in — whether there is anything to submit. A focused button is
   always drawn bright, because a selected control the user cannot see is a
   control they cannot use. */
static void draw_button(DmCore *core, int x, int y, int w, int h,
                        const char *label, int focused, int enabled) {
    const DmStyle *s = &core->style;
    unsigned long fill = (focused || enabled) ? s->accent : s->accent_dim;
    unsigned long edge = focused ? s->text : s->panel_edge;

    dm_draw_box(core, x, y, w, h, fill, edge, focused ? 2 : 1);
    box_text_centred(core, x, y, w, h, label, s->font_label, s->text);
}

/* --- the bottom bar ------------------------------------------------------- */

static void draw_session_selector(DmCore *core) {
    const DmStyle *s = &core->style;
    int focused = (core->focus == DM_FOCUS_SESSION);
    int x = core->session_box.x;
    int y = core->session_box.y;
    int w = core->session_box.width;
    int h = core->session_box.height;

    unsigned long edge = focused ? s->text : s->panel_edge;
    dm_draw_box(core, x, y, w, h, s->panel, edge, focused ? 2 : 1);

    int padding = 10;
    int ascent = s->font_label ? s->font_label->ascent : 8;
    int descent = s->font_label ? s->font_label->descent : 2;
    int baseline = y + (h + ascent - descent) / 2;

    dm_draw_text(core, x + padding, baseline, "session", s->font_label,
                 s->text_muted);

    const char *name = "none";
    if (core->session_count > 0 &&
        core->session_selected >= 0 &&
        core->session_selected < core->session_count) {
        name = core->sessions[core->session_selected].name;
    }

    int name_x = x + padding
               + dm_draw_text_width(s->font_label, "session", 7)
               + padding;
    int arrow_room = 22;
    int room = x + w - arrow_room - name_x;
    text_clipped(core, name_x, baseline, name, s->font_label, s->text, room);

    /* The arrow: up when the list is closed, since it opens upward, and down
       when it is open, so the control says which way it will move. */
    int mar = w - 16;
    int may = y + h / 2;
    XSetForeground(core->display, core->gc, s->accent);
    if (core->session_open) {
        XDrawLine(core->display, core->buffer, core->gc, mar - 5, may - 3, mar, may + 2);
        XDrawLine(core->display, core->buffer, core->gc, mar, may + 2, mar + 5, may - 3);
    } else {
        XDrawLine(core->display, core->buffer, core->gc, mar - 5, may + 2, mar, may - 3);
        XDrawLine(core->display, core->buffer, core->gc, mar, may - 3, mar + 5, may + 2);
    }
}

static void draw_session_list(DmCore *core) {
    if (!core->session_open || core->session_count <= 0) {
        return;
    }
    const DmStyle *s = &core->style;

    dm_draw_box(core, core->session_list_box.x, core->session_list_box.y,
                core->session_list_box.width, core->session_list_box.height,
                s->panel, s->accent, 1);

    for (int i = 0; i < core->session_count; i++) {
        const struct DmRect *row = &core->session_row[i];
        int selected = (i == core->session_selected);
        int hovered = (i == core->session_hover);

        if (selected || hovered) {
            dm_draw_box(core, row->x + 1, row->y, row->width - 2, row->height,
                        selected ? s->field_focus : s->field, s->field, 0);
        }

        int ascent = s->font_label ? s->font_label->ascent : 8;
        int descent = s->font_label ? s->font_label->descent : 2;
        int baseline = row->y + (row->height + ascent - descent) / 2;
        int room = row->width - 24;
        text_clipped(core, row->x + 10, baseline, core->sessions[i].name,
                     s->font_label, selected ? s->text : s->text_muted, room);

        /* A tick beside the session that is in force. */
        if (selected) {
            int mx = row->x + row->width - 14;
            int my = row->y + row->height / 2;
            XSetForeground(core->display, core->gc, s->accent);
            XDrawLine(core->display, core->buffer, core->gc, mx - 5, my, mx - 1, my + 4);
            XDrawLine(core->display, core->buffer, core->gc, mx - 1, my + 4, mx + 5, my - 4);
        }
    }
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

    draw_button(core, core->button_box.x, core->button_box.y,
                core->button_box.width, core->button_box.height, "sign in",
                core->focus == DM_FOCUS_SIGNIN, signin_ready(core));

    int message_y = core->button_box.y + core->button_box.height + s->gap;
    if (core->message[0]) {
        int w = dm_draw_text_width(s->font_label, core->message, (int)strlen(core->message));
        int ascent = s->font_label ? s->font_label->ascent : 8;
        dm_draw_text(core, (core->width - w) / 2, message_y + ascent,
                     core->message, s->font_label,
                     core->message_is_error ? s->danger : s->text_muted);
    } else {
        const char *hint = "Tab moves, Enter selects";
        int w = dm_draw_text_width(s->font_label, hint, (int)strlen(hint));
        int ascent = s->font_label ? s->font_label->ascent : 8;
        dm_draw_text(core, (core->width - w) / 2, message_y + ascent,
                     hint, s->font_label, s->text_muted);
    }

    /* The bottom bar last, so a dropped-up list is drawn over the panel rather
       than under it: the list is the thing the user is looking at. */
    draw_session_selector(core);
    draw_button(core, core->power_box[0].x, core->power_box[0].y,
                core->power_box[0].width, core->power_box[0].height, "reboot",
                core->focus == DM_FOCUS_REBOOT, 0);
    draw_button(core, core->power_box[1].x, core->power_box[1].y,
                core->power_box[1].width, core->power_box[1].height, "shut down",
                core->focus == DM_FOCUS_SHUTDOWN, 0);

    draw_session_list(core);
}

/* --- hit-testing ---------------------------------------------------------- */

static int inside(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

int dm_login_field_at(DmCore *core, int x, int y) {
    layout(core);
    for (int i = 0; i < DM_FIELD_COUNT; i++) {
        if (inside(x, y, core->field_box[i].x, core->field_box[i].y,
                   core->field_box[i].width, core->field_box[i].height)) {
            return i;
        }
    }
    return -1;
}

/* Which session row a point is on, or -1. Only meaningful while the list is
   open; the rows are laid out from the same numbers they are drawn from. */
int dm_login_session_row_at(DmCore *core, int x, int y) {
    if (!core->session_open) {
        return -1;
    }
    layout(core);
    for (int i = 0; i < core->session_count; i++) {
        const struct DmRect *row = &core->session_row[i];
        if (inside(x, y, row->x, row->y, row->width, row->height)) {
            return i;
        }
    }
    return -1;
}

DmButton dm_login_button_at(DmCore *core, int x, int y) {
    layout(core);
    if (inside(x, y, core->button_box.x, core->button_box.y,
               core->button_box.width, core->button_box.height)) {
        return DM_BUTTON_SIGNIN;
    }
    if (inside(x, y, core->session_box.x, core->session_box.y,
               core->session_box.width, core->session_box.height)) {
        return DM_BUTTON_SESSION;
    }
    if (inside(x, y, core->power_box[0].x, core->power_box[0].y,
               core->power_box[0].width, core->power_box[0].height)) {
        return DM_BUTTON_REBOOT;
    }
    if (inside(x, y, core->power_box[1].x, core->power_box[1].y,
               core->power_box[1].width, core->power_box[1].height)) {
        return DM_BUTTON_SHUTDOWN;
    }
    return DM_BUTTON_NONE;
}

/* Fill the session rows, called by the layout so the drawing and the
   hit-testing cannot disagree about where a row is. */
void dm_login_layout_sessions(DmCore *core) {
    for (int i = 0; i < core->session_count; i++) {
        core->session_row[i].x = core->session_list_box.x + 1;
        core->session_row[i].y = core->session_list_box.y + 1
                               + i * SESSION_ROW_HEIGHT;
        core->session_row[i].width = core->session_list_box.width - 2;
        core->session_row[i].height = SESSION_ROW_HEIGHT;
    }
}

const DmModule dm_login_module = {
    .name = "login", .init = NULL, .event = NULL, .draw = login_draw, .cleanup = NULL,
};
