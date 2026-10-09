/*
 * wifi_draw.c — painting the window from the state.
 *
 * One function, one pass, the whole window each time. The window is a bar at
 * the top (the radio and, when there is one, the connection and its address), a
 * list of rows, a status line, and a help line naming the letters. Laying that
 * out is a few rectangles and a call to the text helper per string, and doing it
 * whole keeps the picture a plain function of the state rather than a thing that
 * has to be kept in step with the state by hand.
 *
 * A row is: a signal-strength bar on the left, the SSID, and marks on the right
 * — a lock for a network that needs a password, a dot for one that is saved,
 * and the word "connected" for the one joined. The selected row is filled with
 * the field colour and its SSID drawn in the accent.
 */
#include "wifi_draw.h"

#include <stdio.h>
#include <string.h>

/* A filled rectangle in a colour: the one primitive most of this is built
   from. */
static void fill(WifiUi *ui, unsigned long colour,
                 int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XFillRectangle(ui->display, ui->window, ui->gc,
                   x, y, (unsigned int)width, (unsigned int)height);
}

/* An outline with no fill, for the entry field. */
static void outline(WifiUi *ui, unsigned long colour,
                    int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XDrawRectangle(ui->display, ui->window, ui->gc,
                   x, y, (unsigned int)(width - 1), (unsigned int)(height - 1));
}

/* One line of text at a baseline. */
static void text(WifiUi *ui, int x, int baseline, const char *string,
                 unsigned long colour) {
    wifi_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                    x, baseline, string, colour);
}

/* The baseline for a line of text centred in a row starting at `row_y`. */
static int row_baseline(WifiUi *ui, int row_y) {
    int line = ui->style.font ? (ui->style.font->ascent +
                                 ui->style.font->descent) : 16;
    return row_y + (ui->style.row_height - line) / 2 +
           (ui->style.font ? ui->style.font->ascent : line);
}

/* --- the top bar ---------------------------------------------------------- */

/* The height of a bar: a row tall, so a line of text fits in it with room. */
static int bar_height(WifiUi *ui) {
    return ui->style.row_height;
}

/* How many network rows fit between the status bar at the top and the status
   and help lines at the foot. Public — see wifi_draw.h — because the input
   needs the same number to know where the selection stops and the list
   scrolls. */
int wifi_visible_rows(const WifiUi *ui) {
    int top = ui->style.row_height + ui->style.padding;
    int bottom = ui->height - 2 * ui->style.row_height;   /* status + help */
    int room = (bottom - top) / ui->style.row_height;
    if (room < 1) {
        room = 1;
    }
    return room;
}

/* The two bars the list screen has at its top: the status bar with the radio
   and the connection, and — under it — the column headings are not needed, so
   the list begins right under the status bar. */
static int list_top(WifiUi *ui) {
    return bar_height(ui) + ui->style.padding;
}

/* The radio, drawn as a small aerial: on, it is filled and carries a few
   expanding arcs; off, it is an empty box with a cross. Drawn by hand so no
   glyph is needed. */
static void draw_radio(WifiUi *ui, int x, int y) {
    int size = ui->style.row_height - 2 * ui->style.padding;
    if (size < 8) {
        size = 8;
    }
    int on = (ui->radio_on == 1);
    unsigned long colour = on ? ui->style.connected : ui->style.text_muted;

    /* The aerial's base: a small filled box. */
    fill(ui, colour, x, y + size - 4, size, 4);
    /* The mast and a line of waves, or a cross when it is off. */
    fill(ui, colour, x + size / 2 - 1, y, 3, size - 4);
    if (on) {
        fill(ui, colour, x + size - 4, y + 2, 3, 3);
        fill(ui, colour, x, y + 2, 3, 3);
    } else {
        /* A cross over the whole thing reads as "off" at a glance. */
        fill(ui, ui->style.background, x + size / 2 - 1, y, 3, size);
        fill(ui, colour, x, y + size / 2 - 1, size, 3);
    }
}

static void draw_status_bar(WifiUi *ui) {
    int height = bar_height(ui);
    fill(ui, ui->style.panel, 0, 0, ui->width, height);

    int x = ui->style.padding;
    draw_radio(ui, x, ui->style.padding);
    x += bar_height(ui);

    text(ui, x, row_baseline(ui, 0), ui->config.title, ui->style.accent);
    x += wifi_style_text_width(ui->display, ui->style.font, ui->config.title)
         + ui->style.padding * 2;

    /* The connection and its address, when there is one. This is the line that
       tells a person, without opening another tool, whether they are on and at
       what address. */
    if (ui->active_ssid[0]) {
        /* The SSID and the address are each up to WIFI_TEXT long, so the line
           has to hold both plus the "connected: " and the brackets: three texts
           rather than two, or a long name and a long address would not both
           fit. */
        char line[WIFI_TEXT * 3];
        if (ui->active_address[0]) {
            snprintf(line, sizeof(line), "connected: %s  (%s)",
                     ui->active_ssid, ui->active_address);
        } else {
            snprintf(line, sizeof(line), "connected: %s", ui->active_ssid);
        }
        text(ui, x, row_baseline(ui, 0), line, ui->style.connected);
    } else if (ui->radio_on == 0) {
        text(ui, x, row_baseline(ui, 0), "wifi is off", ui->style.text_muted);
    } else if (ui->radio_on == -1) {
        text(ui, x, row_baseline(ui, 0), "wifi state unknown",
             ui->style.text_muted);
    } else {
        text(ui, x, row_baseline(ui, 0), "not connected", ui->style.text_muted);
    }

    fill(ui, ui->style.panel_edge, 0, height - 2, ui->width, 2);
}

/* --- the list screen ------------------------------------------------------ */

/* The signal strength of a network as a short bar: as many cells filled as the
   strength earns. Reads at a glance and needs no font. */
static void draw_signal(WifiUi *ui, int x, int y, int strength) {
    int cells = 5;
    int filled = (strength * cells) / 100;
    if (filled < 1 && strength > 0) {
        filled = 1;
    }
    int cell_w = 4;
    int gap = 2;
    int bar_h = ui->style.row_height - 2 * ui->style.padding;
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

/* A small padlock, drawn by hand so no glyph is needed. */
static void draw_lock(WifiUi *ui, int x, int y) {
    int ly = y + (ui->style.row_height - 14) / 2;
    fill(ui, ui->style.secured, x, ly + 6, 12, 8);
    fill(ui, ui->style.secured, x + 3, ly, 6, 5);
    fill(ui, ui->style.panel, x + 4, ly + 2, 4, 3);
}

static void draw_list(WifiUi *ui) {
    int top = list_top(ui);
    draw_status_bar(ui);

    if (ui->device[0] == '\0') {
        text(ui, ui->style.padding, top + ui->style.row_height,
             "No wireless interface was found", ui->style.text_muted);
        return;
    }
    if (ui->radio_on == 0) {
        text(ui, ui->style.padding, top + ui->style.row_height,
             "The wifi radio is off — press w to turn it on",
             ui->style.text_muted);
        return;
    }
    if (ui->networks.count == 0) {
        text(ui, ui->style.padding, top + ui->style.row_height,
             "No networks found; press r to scan again",
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
        int row_y = top + row * ui->style.row_height;

        int is_selected = (index == ui->selected);
        if (is_selected) {
            fill(ui, ui->style.field, left, row_y, right - left,
                 ui->style.row_height);
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
        text(ui, x, row_baseline(ui, row_y), network->ssid, name_colour);

        /* The marks on the right, from the right edge inward: "connected", then
           a dot for saved, then a lock for secured. */
        int mark_right = right - ui->style.padding;
        if (network->in_use) {
            const char *label = "connected";
            int label_w = wifi_style_text_width(ui->display, ui->style.font,
                                                label);
            mark_right -= label_w;
            text(ui, mark_right, row_baseline(ui, row_y), label,
                 ui->style.connected);
            mark_right -= ui->style.padding;
        }
        if (network->saved) {
            int ly = row_y + ui->style.row_height / 2;
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
    int top = list_top(ui);
    draw_status_bar(ui);

    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int y = top + ui->style.padding;

    char prompt[WIFI_TEXT * 2];
    snprintf(prompt, sizeof(prompt), "Password for \"%s\":", ui->pending_ssid);
    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         prompt, ui->style.text);
    y += ui->style.row_height;

    int field_h = ui->style.row_height;
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
    /* The caret, so an empty password still shows where the next character
       lands. */
    fill(ui, ui->style.accent, x + ui->password_length * dot_gap,
         y + ui->style.padding, 2, field_h - 2 * ui->style.padding);

    y += field_h + ui->style.padding;
    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         "Enter connects, Escape goes back", ui->style.text_muted);
}

/* --- the confirm screen --------------------------------------------------- */

static void draw_confirm(WifiUi *ui) {
    int top = list_top(ui);
    draw_status_bar(ui);

    int left = ui->style.padding;
    int y = top + ui->style.padding;

    char question[WIFI_TEXT * 2];
    snprintf(question, sizeof(question),
             "Forget the saved network \"%s\"?", ui->pending_saved);
    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         question, ui->style.text);
    y += ui->style.row_height;

    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         "Its password will be forgotten too. y to confirm, n or Escape to keep it.",
         ui->style.text_muted);
}

/* --- the help line -------------------------------------------------------- */

/* Every letter the manager answers to, at the foot of the window, so the
   commands are on the screen rather than in a manual. */
static void draw_help(WifiUi *ui) {
    const char *help = "Up/Down move   Enter join   r rescan   w radio   "
                       "d disconnect   f forget   a autoconnect   q quit";
    text(ui, ui->style.padding,
         ui->height - ui->style.padding - ui->style.font->descent,
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
        break;
    case WIFI_MODE_CONFIRM:
        draw_confirm(ui);
        break;
    case WIFI_MODE_LIST:
    default:
        draw_list(ui);
        break;
    }

    /* The status message and the help line share the bottom. The message — a
       failure, "Connecting…" — is drawn above the help when there is one. */
    if (ui->status[0]) {
        text(ui, ui->style.padding,
             ui->height - ui->style.row_height - ui->style.padding,
             ui->status, ui->style.text);
    }
    draw_help(ui);

    XFlush(ui->display);
}
