/*
 * wifi_draw.c — painting the window from the state.
 *
 * One function, one pass, the whole window each time. The window is a title
 * bar, a list of rows, and a status line; laying that out is a few rectangles
 * and a call to the text helper per string, and doing it whole keeps the
 * picture a plain function of the state rather than a thing that has to be
 * kept in step with the state by hand.
 *
 * A row is: a signal-strength bar on the left, the SSID, and a mark on the
 * right — a lock for a network that needs a password, "connected" for the one
 * joined. The selected row is filled with the field colour and its SSID drawn
 * in the accent, which is what makes the keyboard's position visible.
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

/* An outline with no fill, for the panel's edge and the entry field. */
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

/* The height of the title bar. The title sits in it and the list begins under
   it. */
static int title_height(WifiUi *ui) {
    return ui->style.row_height + 2 * ui->style.padding;
}

/* --- the list screen ------------------------------------------------------ */

/* The signal strength of a network as a short bar: a full-width block for a
   strong signal down to a thin one for a weak. Drawn as five cells with as
   many filled as the strength earns, which reads at a glance and needs no
   font. */
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

static void draw_list(WifiUi *ui) {
    int top = title_height(ui);
    int bottom = ui->height - ui->style.row_height;   /* the status line */
    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int room = (bottom - top) / ui->style.row_height;
    if (room < 1) {
        room = 1;
    }

    /* The panel the rows sit on. */
    fill(ui, ui->style.panel, left, top, right - left, bottom - top);

    if (ui->device[0] == '\0') {
        text(ui, left + ui->style.padding,
             top + ui->style.row_height,
             "No wireless interface was found", ui->style.text_muted);
        return;
    }
    if (ui->networks.count == 0) {
        text(ui, left + ui->style.padding,
             top + ui->style.row_height,
             "No networks found; press R to scan again",
             ui->style.text_muted);
        return;
    }

    /* Keep the selection inside the visible window. */
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

        /* The SSID, in the accent when chosen and the plain colour otherwise;
           the joined network is drawn in the connected colour when it is not
           the chosen row, so it is marked even before it is selected. */
        unsigned long name_colour = ui->style.text;
        if (is_selected) {
            name_colour = ui->style.accent;
        } else if (network->in_use) {
            name_colour = ui->style.connected;
        }
        text(ui, x, row_baseline(ui, row_y), network->ssid, name_colour);

        /* The mark on the right: a lock when a password is needed, and the
           word "connected" for the one joined. */
        int mark_right = right - ui->style.padding;
        if (network->in_use) {
            const char *label = "connected";
            int label_w = wifi_style_text_width(ui->display, ui->style.font,
                                                label);
            text(ui, mark_right - label_w, row_baseline(ui, row_y), label,
                 ui->style.connected);
        } else if (network->secured) {
            /* A lock drawn as a small body with a shackle above it: a filled
               square with a thinner bar, which reads as a padlock at this
               size without needing a glyph the font may not have. */
            int lx = mark_right - 12;
            int ly = row_y + (ui->style.row_height - 14) / 2;
            fill(ui, ui->style.secured, lx, ly + 6, 12, 8);
            fill(ui, ui->style.secured, lx + 3, ly, 6, 5);
            fill(ui, ui->style.panel, lx + 4, ly + 2, 4, 3);
        }
    }
}

/* --- the password screen -------------------------------------------------- */

static void draw_password(WifiUi *ui) {
    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int y = title_height(ui) + ui->style.padding;

    char prompt[WIFI_TEXT * 2];
    snprintf(prompt, sizeof(prompt), "Password for \"%s\":", ui->pending_ssid);
    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         prompt, ui->style.text);
    y += ui->style.row_height;

    /* The entry field: an outlined box with the password in it, drawn as dots
       rather than letters — a wifi password typed in a public place is not
       something to put on a screen — and a caret after the last one. */
    int field_h = ui->style.row_height;
    fill(ui, ui->style.field, left, y, right - left, field_h);
    outline(ui, ui->style.panel_edge, left, y, right - left, field_h);

    int dots = ui->password_length;
    int dot_r = 3;
    int dot_gap = 14;
    int x = left + ui->style.padding + dot_r;
    int cy = y + field_h / 2;
    XSetForeground(ui->display, ui->gc, ui->style.text);
    for (int i = 0; i < dots; i++) {
        XFillArc(ui->display, ui->window, ui->gc,
                 x + i * dot_gap - dot_r, cy - dot_r,
                 (unsigned int)(2 * dot_r), (unsigned int)(2 * dot_r),
                 0, 360 * 64);
    }
    /* The caret: a thin bar right after the last dot, so an empty password
       still shows where the next character will land. */
    fill(ui, ui->style.accent, x + dots * dot_gap, y + ui->style.padding,
         2, field_h - 2 * ui->style.padding);

    y += field_h + ui->style.padding;
    text(ui, left + ui->style.padding, y + ui->style.font->ascent,
         "Enter connects, Escape goes back", ui->style.text_muted);
}

/* --- the frame ------------------------------------------------------------ */

void wifi_draw(WifiUi *ui) {
    if (!ui->display || ui->window == None) {
        return;
    }

    /* The background, then everything on top of it. */
    fill(ui, ui->style.background, 0, 0, ui->width, ui->height);

    /* The title bar, across the top, with the window's name in it. */
    int bar = title_height(ui);
    fill(ui, ui->style.panel, 0, 0, ui->width, bar);
    fill(ui, ui->style.panel_edge, 0, bar - 2, ui->width, 2);
    text(ui, ui->style.padding + 2, bar / 2 + ui->style.font->ascent / 2,
         ui->config.title, ui->style.accent);

    if (ui->mode == WIFI_MODE_PASSWORD) {
        draw_password(ui);
    } else {
        draw_list(ui);
    }

    /* The status line at the bottom, when there is something to say. */
    if (ui->status[0]) {
        text(ui, ui->style.padding,
             ui->height - ui->style.padding - ui->style.font->descent,
             ui->status, ui->style.text_muted);
    }

    XFlush(ui->display);
}
