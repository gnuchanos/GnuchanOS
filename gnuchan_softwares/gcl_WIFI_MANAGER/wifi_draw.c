/*
 * wifi_draw.c — painting the window from the state.
 *
 * One function, one pass, the whole window each time. The window is, from the
 * top down: a status bar (the radio, the title, and the connection with its
 * address), a warning band when the kernel has the radio blocked, the list of
 * networks, a line for the last message, a row of buttons, and a line naming the
 * letters. Laying that out is a few rectangles and a call to the text helper per
 * string, and doing it whole keeps the picture a plain function of the state
 * rather than a thing that has to be kept in step with the state by hand.
 *
 * A row is: a signal-strength bar on the left, the SSID, and marks on the right
 * — a lock for a network that needs a password, a dot for one that is saved,
 * and the word "connected" for the one joined. The selected row is filled with
 * the field colour and its SSID drawn in the accent.
 *
 * The vertical geometry is fixed and shared: wifi_visible_rows() measures with
 * the same numbers the drawing uses, so the scroll the input computes and the
 * rows the drawing paints are the same count. Those numbers are the four bands
 * below the list — message, buttons, help — plus the two above it.
 */
#include "wifi_draw.h"

#include <stdio.h>
#include <string.h>

/* --- geometry, shared by the drawing and the input ------------------------ */

/* The height of a band: a row tall, so a line of text fits in it with room. */
static int row_height(const WifiUi *ui) {
    return ui->style.row_height;
}

/* The warning band is drawn only when the kernel has the radio blocked, and
   takes a row when it is. */
static int has_banner(const WifiUi *ui) {
    return (ui->block.hard || ui->block.soft) ? 1 : 0;
}

/* The top of the network list: below the status bar, and below the warning band
   when there is one. */
static int list_top_y(const WifiUi *ui) {
    return row_height(ui) + ui->style.padding +
           has_banner(ui) * row_height(ui);
}

/* The bottom of the network list: above the message line, the button row and
   the help line, each a row tall, with a little air. */
static int list_bottom_y(const WifiUi *ui) {
    return ui->height - 3 * row_height(ui) - ui->style.padding;
}

int wifi_visible_rows(const WifiUi *ui) {
    int room = (list_bottom_y(ui) - list_top_y(ui)) / row_height(ui);
    if (room < 1) {
        room = 1;
    }
    return room;
}

/* The three bands at the foot: the message line, the buttons, the help. */
static int status_y(const WifiUi *ui) {
    return ui->height - 3 * row_height(ui);
}
static int buttons_y(const WifiUi *ui) {
    return ui->height - 2 * row_height(ui);
}
static int help_y(const WifiUi *ui) {
    return ui->height - row_height(ui);
}

/* --- the primitives ------------------------------------------------------- */

static void fill(WifiUi *ui, unsigned long colour,
                 int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XFillRectangle(ui->display, ui->window, ui->gc,
                   x, y, (unsigned int)width, (unsigned int)height);
}

static void outline(WifiUi *ui, unsigned long colour,
                    int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XDrawRectangle(ui->display, ui->window, ui->gc,
                   x, y, (unsigned int)(width - 1), (unsigned int)(height - 1));
}

static void text(WifiUi *ui, int x, int baseline, const char *string,
                 unsigned long colour) {
    wifi_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                    x, baseline, string, colour);
}

/* The baseline that centres a line of text in a band starting at `band_y`. */
static int band_baseline(const WifiUi *ui, int band_y) {
    int line = ui->style.font ? (ui->style.font->ascent +
                                 ui->style.font->descent) : 16;
    return band_y + (row_height(ui) - line) / 2 +
           (ui->style.font ? ui->style.font->ascent : line);
}

/* --- the buttons ---------------------------------------------------------- */

/* The buttons, in the order they are drawn. One table so a button's label and
   its action are written together and cannot drift apart. Connect is first
   because it is what a person does most and it acts on the chosen row. */
static const struct {
    const char *label;
    WifiAction action;
} kButtons[] = {
    { "Connect", WIFI_ACTION_CONNECT },
    { "Rescan",  WIFI_ACTION_RESCAN },
    { "Disconnect", WIFI_ACTION_DISCONNECT },
    { "Forget",  WIFI_ACTION_FORGET },
    { "Autojoin", WIFI_ACTION_AUTOCONNECT },
    { "Restart", WIFI_ACTION_RESTART },
    { "Quit",    WIFI_ACTION_QUIT },
};
#define BUTTON_COUNT ((int)(sizeof(kButtons) / sizeof(kButtons[0])))

void wifi_layout_buttons(WifiUi *ui) {
    int count = BUTTON_COUNT;
    if (count > WIFI_MAX_BUTTONS) {
        count = WIFI_MAX_BUTTONS;
    }

    int pad = ui->style.padding / 2;
    if (pad < 4) {
        pad = 4;
    }
    int gap = ui->style.padding / 2;
    if (gap < 4) {
        gap = 4;
    }

    /* Measure, and shrink the air until the row fits the window. A button that
       runs off the edge would be one a person cannot click, so the padding
       gives way before the words do. */
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += wifi_style_text_width(ui->display, ui->style.font,
                                       kButtons[i].label) + 2 * pad;
    }
    total += gap * (count - 1);

    int available = ui->width - 2 * ui->style.padding;
    while (total > available && (pad > 2 || gap > 2)) {
        if (pad > 2) {
            pad--;
            total -= 2 * count;
        } else if (gap > 2) {
            gap--;
            total -= (count - 1);
        }
    }

    int band = buttons_y(ui);
    int height = row_height(ui);
    int x = (ui->width - total) / 2;
    if (x < ui->style.padding) {
        x = ui->style.padding;
    }

    for (int i = 0; i < count; i++) {
        int width = wifi_style_text_width(ui->display, ui->style.font,
                                          kButtons[i].label) + 2 * pad;
        ui->buttons[i].x = x;
        ui->buttons[i].y = band;
        ui->buttons[i].width = width;
        ui->buttons[i].height = height;
        ui->buttons[i].action = kButtons[i].action;
        x += width + gap;
    }
    ui->button_count = count;
}

static void draw_buttons(WifiUi *ui) {
    wifi_layout_buttons(ui);
    for (int i = 0; i < ui->button_count; i++) {
        WifiButton *button = &ui->buttons[i];
        fill(ui, ui->style.field, button->x, button->y,
             button->width, button->height);
        outline(ui, ui->style.panel_edge, button->x, button->y,
                button->width, button->height);
        const char *label = kButtons[i].label;
        int label_w = wifi_style_text_width(ui->display, ui->style.font, label);
        int text_x = button->x + (button->width - label_w) / 2;
        text(ui, text_x, band_baseline(ui, button->y), label,
             ui->style.text);
    }
}

/* --- the top bar ---------------------------------------------------------- */

/* The radio, drawn as a small aerial: on, it carries two little waves; off, it
   is crossed out. Drawn by hand so no glyph is needed. */
static void draw_radio(WifiUi *ui, int x, int y) {
    int size = row_height(ui) - 2 * ui->style.padding;
    if (size < 8) {
        size = 8;
    }
    int on = (ui->radio_on == 1);
    unsigned long colour = on ? ui->style.connected : ui->style.text_muted;

    fill(ui, colour, x, y + size - 4, size, 4);
    fill(ui, colour, x + size / 2 - 1, y, 3, size - 4);
    if (on) {
        fill(ui, colour, x + size - 4, y + 2, 3, 3);
        fill(ui, colour, x, y + 2, 3, 3);
    } else {
        fill(ui, ui->style.background, x + size / 2 - 1, y, 3, size);
        fill(ui, colour, x, y + size / 2 - 1, size, 3);
    }
}

static void draw_status_bar(WifiUi *ui) {
    int height = row_height(ui);
    fill(ui, ui->style.panel, 0, 0, ui->width, height);

    int x = ui->style.padding;
    draw_radio(ui, x, ui->style.padding);
    x += height;

    text(ui, x, band_baseline(ui, 0), ui->config.title, ui->style.accent);
    x += wifi_style_text_width(ui->display, ui->style.font, ui->config.title)
         + ui->style.padding * 2;

    if (ui->active_ssid[0]) {
        /* The SSID and the address are each up to WIFI_TEXT long, so the line
           holds both plus the words around them: three texts, not two. */
        char line[WIFI_TEXT * 3];
        if (ui->active_address[0]) {
            snprintf(line, sizeof(line), "connected: %s  (%s)",
                     ui->active_ssid, ui->active_address);
        } else {
            snprintf(line, sizeof(line), "connected: %s", ui->active_ssid);
        }
        text(ui, x, band_baseline(ui, 0), line, ui->style.connected);
    } else if (ui->radio_on == 0) {
        text(ui, x, band_baseline(ui, 0), "wifi is off", ui->style.text_muted);
    } else if (ui->radio_on == -1) {
        text(ui, x, band_baseline(ui, 0), "wifi state unknown",
             ui->style.text_muted);
    } else {
        text(ui, x, band_baseline(ui, 0), "not connected", ui->style.text_muted);
    }

    fill(ui, ui->style.panel_edge, 0, height - 2, ui->width, 2);
}

/* The warning band, shown when the kernel has the radio blocked. It says which
   kind, because the two are different things to a person: one is a switch on
   the machine, the other the window can clear itself. */
static void draw_banner(WifiUi *ui) {
    if (!has_banner(ui)) {
        return;
    }
    int y = row_height(ui);
    fill(ui, ui->style.field, 0, y, ui->width, row_height(ui));
    fill(ui, ui->style.secured, 0, y, 4, row_height(ui));

    const char *message;
    if (ui->block.hard) {
        message = "The radio is blocked by a hardware switch (fn+F11 on a "
                  "Vostro); flip it, then press Restart";
    } else {
        message = "The radio is soft-blocked by the kernel; press Restart to "
                  "clear it";
    }
    text(ui, ui->style.padding * 2, band_baseline(ui, y), message,
         ui->style.secured);
}

/* --- the list screen ------------------------------------------------------ */

static void draw_signal(WifiUi *ui, int x, int y, int strength) {
    int cells = 5;
    int filled = (strength * cells) / 100;
    if (filled < 1 && strength > 0) {
        filled = 1;
    }
    int cell_w = 4;
    int gap = 2;
    int bar_h = row_height(ui) - 2 * ui->style.padding;
    if (bar_h < 4) {
        bar_h = 4;
    }
    for (int i = 0; i < cells; i++) {
        int cell_h = bar_h * (i + 1) / cells;
        int cx = x + i * (cell_w + gap);
        int cy = y + ui->style.padding + (bar_h - cell_h);
        unsigned long colour = (i < filled) ? ui->style.accent
                                            : ui->style.field;
        fill(ui, colour, cx, cy, cell_w, cell_h);
    }
}

static void draw_lock(WifiUi *ui, int x, int y) {
    int ly = y + (row_height(ui) - 14) / 2;
    fill(ui, ui->style.secured, x, ly + 6, 12, 8);
    fill(ui, ui->style.secured, x + 3, ly, 6, 5);
    fill(ui, ui->style.panel, x + 4, ly + 2, 4, 3);
}

static void draw_list(WifiUi *ui) {
    int top = list_top_y(ui);
    draw_status_bar(ui);
    draw_banner(ui);

    if (ui->device[0] == '\0') {
        text(ui, ui->style.padding, band_baseline(ui, top),
             "No wireless interface was found", ui->style.text_muted);
        return;
    }
    if (ui->networks.count == 0) {
        const char *message = "No networks found; press Rescan";
        if (ui->block.hard || ui->block.soft) {
            message = "No networks: the radio is blocked (see above)";
        } else if (ui->radio_on == 0) {
            message = "No networks; press w to turn the radio on";
        }
        text(ui, ui->style.padding, band_baseline(ui, top), message,
             ui->style.text_muted);
        return;
    }

    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int room = wifi_visible_rows(ui);

    if (ui->selected < ui->scroll) {
        ui->scroll = ui->selected;
    }
    if (ui->selected >= ui->scroll + room) {
        ui->scroll = ui->selected - room + 1;
    }
    if (ui->scroll < 0) {
        ui->scroll = 0;
    }

    for (int row = 0; row < room; row++) {
        int index = ui->scroll + row;
        if (index >= ui->networks.count) {
            break;
        }
        WifiNetwork *network = &ui->networks.items[index];
        int row_y = top + row * row_height(ui);

        int is_selected = (index == ui->selected);
        if (is_selected) {
            fill(ui, ui->style.field, left, row_y, right - left,
                 row_height(ui));
        }

        int x = left + ui->style.padding;
        draw_signal(ui, x, row_y, network->signal);
        x += 5 * 6 + ui->style.padding;

        unsigned long name_colour = ui->style.text;
        if (is_selected) {
            name_colour = ui->style.accent;
        } else if (network->in_use) {
            name_colour = ui->style.connected;
        }
        text(ui, x, band_baseline(ui, row_y), network->ssid, name_colour);

        int mark_right = right - ui->style.padding;
        if (network->in_use) {
            const char *label = "connected";
            int label_w = wifi_style_text_width(ui->display, ui->style.font,
                                                label);
            mark_right -= label_w;
            text(ui, mark_right, band_baseline(ui, row_y), label,
                 ui->style.connected);
            mark_right -= ui->style.padding;
        }
        if (network->saved) {
            int ly = row_y + row_height(ui) / 2;
            mark_right -= 10;
            fill(ui, ui->style.accent, mark_right, ly - 3, 6, 6);
            mark_right -= ui->style.padding;
        }
        if (network->secured) {
            mark_right -= 12;
            draw_lock(ui, mark_right, row_y);
        }
    }
}

/* --- the password screen -------------------------------------------------- */

static void draw_password(WifiUi *ui) {
    int top = list_top_y(ui);
    draw_status_bar(ui);
    draw_banner(ui);

    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int y = top + ui->style.padding;

    char prompt[WIFI_TEXT * 2];
    snprintf(prompt, sizeof(prompt), "Password for \"%s\":", ui->pending_ssid);
    text(ui, left + ui->style.padding,
         y + (ui->style.font ? ui->style.font->ascent : 16),
         prompt, ui->style.text);
    y += row_height(ui);

    int field_h = row_height(ui);
    fill(ui, ui->style.field, left, y, right - left, field_h);
    outline(ui, ui->style.panel_edge, left, y, right - left, field_h);

    int dot_r = 3;
    int dot_gap = 14;
    int x = left + ui->style.padding + dot_r;
    int cy = y + field_h / 2;
    XSetForeground(ui->display, ui->gc, ui->style.text);
    for (int i = 0; i < ui->password_length; i++) {
        XFillArc(ui->display, ui->window, ui->gc,
                 x + i * dot_gap - dot_r, cy - dot_r,
                 (unsigned int)(2 * dot_r), (unsigned int)(2 * dot_r),
                 0, 360 * 64);
    }
    fill(ui, ui->style.accent, x + ui->password_length * dot_gap,
         y + ui->style.padding, 2, field_h - 2 * ui->style.padding);

    y += field_h + ui->style.padding;
    text(ui, left + ui->style.padding,
         y + (ui->style.font ? ui->style.font->ascent : 16),
         "Enter connects, Escape goes back", ui->style.text_muted);
}

/* --- the confirmation screens --------------------------------------------- */

static void draw_confirm(WifiUi *ui) {
    int top = list_top_y(ui);
    draw_status_bar(ui);
    draw_banner(ui);

    int left = ui->style.padding;
    int y = top + ui->style.padding;
    int ascent = ui->style.font ? ui->style.font->ascent : 16;

    char question[WIFI_TEXT * 2];
    snprintf(question, sizeof(question),
             "Forget the saved network \"%s\"?", ui->pending_saved);
    text(ui, left + ui->style.padding, y + ascent, question, ui->style.text);
    y += row_height(ui);

    text(ui, left + ui->style.padding, y + ascent,
         "Its password will be forgotten too. y to confirm, n or Escape to keep it.",
         ui->style.text_muted);
}

static void draw_restart(WifiUi *ui) {
    int top = list_top_y(ui);
    draw_status_bar(ui);
    draw_banner(ui);

    int left = ui->style.padding;
    int y = top + ui->style.padding;
    int ascent = ui->style.font ? ui->style.font->ascent : 16;

    text(ui, left + ui->style.padding, y + ascent,
         "Restart the wifi driver?", ui->style.text);
    y += row_height(ui);

    char line[WIFI_TEXT * 2];
    if (ui->driver_module[0]) {
        snprintf(line, sizeof(line),
                 "The driver (%s) is reloaded and the radio is unblocked. "
                 "The network drops for a moment.",
                 ui->driver_module);
    } else {
        snprintf(line, sizeof(line),
                 "The radio is unblocked. No driver could be named, so the "
                 "module is not reloaded.");
    }
    text(ui, left + ui->style.padding, y + ascent, line, ui->style.text_muted);
    y += row_height(ui);
    text(ui, left + ui->style.padding, y + ascent,
         "y to restart, n or Escape to cancel", ui->style.text_muted);
}

/* --- the foot ------------------------------------------------------------- */

/* The last message — a failure, "Connecting…" — above the buttons. */
static void draw_status_line(WifiUi *ui) {
    if (!ui->status[0]) {
        return;
    }
    text(ui, ui->style.padding, band_baseline(ui, status_y(ui)),
         ui->status, ui->style.text);
}

/* The letters, named at the foot so the keyboard is usable without the mouse.
   Shorter than the button bar on purpose: the buttons are the full list, this
   is the reminder for the ones with a letter. */
static void draw_help(WifiUi *ui) {
    const char *help = "Up/Down move   Enter join   r rescan   w wifi   "
                       "R restart   q quit";
    int descent = ui->style.font ? ui->style.font->descent : 4;
    text(ui, ui->style.padding, help_y(ui) + row_height(ui) - descent - 2,
         help, ui->style.text_muted);
}

/* --- the frame ------------------------------------------------------------ */

void wifi_draw(WifiUi *ui) {
    if (!ui->display || ui->window == None) {
        return;
    }

    fill(ui, ui->style.background, 0, 0, ui->width, ui->height);

    switch (ui->mode) {
    case WIFI_MODE_PASSWORD:
        draw_password(ui);
        draw_status_line(ui);
        break;
    case WIFI_MODE_CONFIRM:
        draw_confirm(ui);
        draw_status_line(ui);
        break;
    case WIFI_MODE_RESTART:
        draw_restart(ui);
        draw_status_line(ui);
        break;
    case WIFI_MODE_LIST:
    default:
        draw_list(ui);
        draw_status_line(ui);
        draw_buttons(ui);
        break;
    }

    draw_help(ui);
    XFlush(ui->display);
}
