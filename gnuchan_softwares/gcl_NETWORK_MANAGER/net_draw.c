/*
 * net_draw.c — painting the window from the state.
 *
 * One function, one pass, the whole window each time. The window is, from the
 * top down: a status bar (the title and a one-line summary of the machine), the
 * panel body, then three foot bands — the last message, the buttons, the help
 * line. Laying that out is a few rectangles and a call to the text helper per
 * string, and doing it whole keeps the picture a plain function of the state.
 *
 * The device panel's rows are: a kind mark on the left (a plug for a wired
 * link, a small aerial for a wireless one, a ring for a loopback), the
 * interface name, its state, and its address on the right. The chosen row is
 * filled with the selection colour and its name drawn in the accent.
 *
 * The DNS panel is: the servers as they are and where they came from, then the
 * four boxes Windows asks the same question with — IPv4 and IPv6, each with a
 * Preferred and an Alternate, drawn side by side under its family's heading.
 * Each box is a plain field with a caret when it has the focus, the same shape
 * the wifi manager's password line has.
 *
 * The vertical geometry is fixed and shared: net_visible_rows() measures with
 * the same numbers the drawing uses, so the scroll the input computes and the
 * rows the drawing paints are the same count.
 */
#include "net_draw.h"

#include <stdio.h>
#include <string.h>

/* --- geometry, shared by the drawing and the input ------------------------ */

static int row_height(const NetUi *ui) {
    return ui->style.row_height;
}

/* The top of the panel body: below the status bar. */
static int body_top_y(const NetUi *ui) {
    return row_height(ui) + ui->style.padding;
}

/* The bottom of the panel body: above the message line, the button row and the
   help line, each a row tall. */
static int body_bottom_y(const NetUi *ui) {
    return ui->height - 3 * row_height(ui) - ui->style.padding;
}

int net_visible_rows(const NetUi *ui) {
    int room = (body_bottom_y(ui) - body_top_y(ui)) / row_height(ui);
    if (room < 1) {
        room = 1;
    }
    return room;
}

static int status_y(const NetUi *ui) {
    return ui->height - 3 * row_height(ui);
}
static int buttons_y(const NetUi *ui) {
    return ui->height - 2 * row_height(ui);
}
static int help_y(const NetUi *ui) {
    return ui->height - row_height(ui);
}

/* --- the primitives ------------------------------------------------------- */

static Drawable target(const NetUi *ui) {
    return ui->buffer ? ui->buffer : ui->window;
}

static void fill(NetUi *ui, unsigned long colour,
                 int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XFillRectangle(ui->display, target(ui), ui->gc,
                   x, y, (unsigned int)width, (unsigned int)height);
}

static void outline(NetUi *ui, unsigned long colour,
                    int x, int y, int width, int height) {
    XSetForeground(ui->display, ui->gc, colour);
    XDrawRectangle(ui->display, target(ui), ui->gc,
                   x, y, (unsigned int)(width - 1), (unsigned int)(height - 1));
}

static void text(NetUi *ui, int x, int baseline, const char *string,
                 unsigned long colour) {
    net_style_text(ui->display, ui->screen, target(ui), ui->style.font,
                   x, baseline, string, colour);
}

/* The baseline that centres a line of text in a band starting at `band_y`. */
static int band_baseline(const NetUi *ui, int band_y) {
    int line = ui->style.font ? (ui->style.font->ascent +
                                 ui->style.font->descent) : 16;
    return band_y + (row_height(ui) - line) / 2 +
           (ui->style.font ? ui->style.font->ascent : line);
}

/* --- the buttons ---------------------------------------------------------- */

/* One button in a table: what it says and what it does. A NAMED type, because
   the two tables below and the function that lays them out must agree on one
   type — two anonymous structs that merely look alike are different types in
   C, and passing one where the other is expected does not compile. */
typedef struct NetButtonDef {
    const char *label;
    NetAction action;
} NetButtonDef;

/* The buttons, per mode. One table so a button's label and its action are
   written together and cannot drift apart. */
static const NetButtonDef kDeviceButtons[] = {
    { "Up/Down",    NET_ACTION_TOGGLE  },
    { "Open Wi-Fi", NET_ACTION_WIFI    },
    { "DNS",        NET_ACTION_DNS     },
    { "DPI: Open",  NET_ACTION_DPI     },
    { "Refresh",    NET_ACTION_REFRESH },
};
#define DEVICE_BUTTON_COUNT \
    ((int)(sizeof(kDeviceButtons) / sizeof(kDeviceButtons[0])))

/* The DNS buttons. The third is a toggle — "Secure DNS: on" / "Secure DNS: off"
   — so the label here is the off shape and button_label() below supplies the
   live one from the state. The table carries it so the count, the order and the
   action live in one place. */
static const NetButtonDef kDnsButtons[] = {
    { "Apply",           NET_ACTION_DNS_APPLY  },
    { "Revert",          NET_ACTION_DNS_REVERT },
    { "Secure DNS: off", NET_ACTION_DNS_SECURE },
    { "Back",            NET_ACTION_BACK       },
};
#define DNS_BUTTON_COUNT \
    ((int)(sizeof(kDnsButtons) / sizeof(kDnsButtons[0])))

/* The label a button actually shows. Every button uses its table label except
   the DNS panel's Secure DNS toggle, whose word follows the state — so the
   layout and the drawing must ask here rather than read the table directly, or
   the box would be sized for "off" while drawn as "on" and the text would
   overrun it. */
static const char *button_label(const NetUi *ui, const NetButtonDef *table,
                                int index) {
    if (table == kDnsButtons && index == 2) {
        return ui->dns_secure ? "Secure DNS: on" : "Secure DNS: off";
    }
    /* The device panel's DPI button is a toggle too: it reads "DPI: Open" when
       the bypass is off and "DPI: Close" when it is on, so the word says what
       pressing it will do. The live label is produced here for the same reason
       the Secure DNS one is — the box is sized from this, so a box sized for
       "Open" and drawn as "Close" would overrun its own text. */
    if (table == kDeviceButtons && index == 3) {
        return ui->dpi_active ? "DPI: Close" : "DPI: Open";
    }
    return table[index].label;
}

/* Lay out one row of buttons, centred, shrinking the air until it fits the
   window. `table` and `count` are one of the two above. */
static void layout_row(NetUi *ui, const NetButtonDef *table, int count) {
    if (count > NET_MAX_BUTTONS) {
        count = NET_MAX_BUTTONS;
    }

    int pad = ui->style.padding / 2;
    if (pad < 4) {
        pad = 4;
    }
    int gap = ui->style.padding / 2;
    if (gap < 4) {
        gap = 4;
    }

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += net_style_text_width(ui->display, ui->style.font,
                                      button_label(ui, table, i)) + 2 * pad;
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
        int width = net_style_text_width(ui->display, ui->style.font,
                                         button_label(ui, table, i)) + 2 * pad;
        ui->buttons[i].x = x;
        ui->buttons[i].y = band;
        ui->buttons[i].width = width;
        ui->buttons[i].height = height;
        ui->buttons[i].action = table[i].action;
        x += width + gap;
    }
    ui->button_count = count;
}

void net_layout_buttons(NetUi *ui) {
    if (ui->mode == NET_MODE_DNS) {
        layout_row(ui, kDnsButtons, DNS_BUTTON_COUNT);
    } else {
        layout_row(ui, kDeviceButtons, DEVICE_BUTTON_COUNT);
    }
}

static void draw_buttons(NetUi *ui) {
    net_layout_buttons(ui);
    const NetButtonDef *table =
        (ui->mode == NET_MODE_DNS) ? kDnsButtons : kDeviceButtons;

    for (int i = 0; i < ui->button_count; i++) {
        NetButton *button = &ui->buttons[i];
        fill(ui, ui->style.field, button->x, button->y,
             button->width, button->height);
        outline(ui, ui->style.panel_edge, button->x, button->y,
                button->width, button->height);
        const char *label = button_label(ui, table, i);
        int label_w = net_style_text_width(ui->display, ui->style.font, label);
        int text_x = button->x + (button->width - label_w) / 2;
        /* A Secure DNS button that is ON — and the device panel's DPI button
           when the bypass is running — is drawn in the connected colour, so the
           state is readable at a glance and not only by reading the word. */
        unsigned long colour =
            ((table == kDnsButtons && i == 2 && ui->dns_secure) ||
             (table == kDeviceButtons && i == 3 && ui->dpi_active))
                ? ui->style.connected
                : ui->style.text;
        text(ui, text_x, band_baseline(ui, button->y), label, colour);
    }
}

/* --- the status bar ------------------------------------------------------- */

static void draw_status_bar(NetUi *ui) {
    int height = row_height(ui);
    fill(ui, ui->style.panel, 0, 0, ui->width, height);

    int x = ui->style.padding;
    text(ui, x, band_baseline(ui, 0), ui->config.title, ui->style.accent);
    x += net_style_text_width(ui->display, ui->style.font, ui->config.title)
         + ui->style.padding * 2;

    /* A one-line summary of the machine, so the state is readable without
       reading every row. */
    int connected = 0;
    for (int i = 0; i < ui->devices.count; i++) {
        if (ui->devices.items[i].state == 1) {
            connected++;
        }
    }
    char summary[NET_TEXT * 2];
    if (ui->devices.count == 0) {
        snprintf(summary, sizeof(summary), "no interfaces were read");
    } else if (connected == 0) {
        snprintf(summary, sizeof(summary), "%d interface(s), none connected",
                 ui->devices.count);
    } else {
        snprintf(summary, sizeof(summary), "%d interface(s), %d connected",
                 ui->devices.count, connected);
    }
    text(ui, x, band_baseline(ui, 0), summary, ui->style.text_muted);

    fill(ui, ui->style.panel_edge, 0, height - 2, ui->width, 2);
}

/* --- the device panel ----------------------------------------------------- */

/* A small mark for the interface kind, drawn by hand so no glyph is needed:
   a plug for wired, an aerial for wireless, a ring for loopback, a dot
   otherwise. */
static void draw_kind(NetUi *ui, NetDeviceKind kind, int x, int y,
                      int size) {
    unsigned long colour = (kind == NET_DEVICE_WIFI) ? ui->style.accent
                                                     : ui->style.text_muted;
    int cy = y + size / 2;

    switch (kind) {
    case NET_DEVICE_ETHERNET:
        /* A plug: a body with a stub. */
        fill(ui, colour, x, cy - 3, size - 4, 6);
        fill(ui, colour, x + size - 4, cy - 2, 4, 4);
        break;
    case NET_DEVICE_WIFI:
        /* An aerial: a mast with two waves. */
        fill(ui, colour, x + size / 2 - 1, cy - size / 2, 3, size);
        fill(ui, colour, x, cy - size / 2 + 2, 3, 3);
        fill(ui, colour, x + size - 3, cy - size / 2 + 2, 3, 3);
        break;
    case NET_DEVICE_LOOPBACK:
        /* A ring. */
        XSetForeground(ui->display, ui->gc, colour);
        XDrawArc(ui->display, target(ui), ui->gc, x, cy - size / 2 + 2,
                 (unsigned)(size - 4), (unsigned)(size - 4), 0, 360 * 64);
        break;
    default:
        fill(ui, colour, x + size / 2 - 2, cy - 2, 4, 4);
        break;
    }
}

/* The state word for an interface: "connected", "down", "up", or nothing when
   it could not be read. */
static const char *state_word(const NetDevice *device) {
    if (device->state == 1) {
        return "connected";
    }
    if (device->state == 0 && device->up) {
        return "up";
    }
    if (device->state == 0) {
        return "disconnected";
    }
    return device->up ? "up" : "";
}

static void draw_devices(NetUi *ui) {
    int top = body_top_y(ui);

    if (ui->devices.count == 0) {
        text(ui, ui->style.padding, band_baseline(ui, top),
             "No interfaces were found (nmcli and ip both gave nothing)",
             ui->style.text_muted);
        return;
    }

    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int room = net_visible_rows(ui);

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
        if (index >= ui->devices.count) {
            break;
        }
        NetDevice *device = &ui->devices.items[index];
        int row_y = top + row * row_height(ui);

        int is_selected = (index == ui->selected);
        if (is_selected) {
            fill(ui, ui->style.selection, left, row_y, right - left,
                 row_height(ui));
            fill(ui, ui->style.accent, left, row_y, 4, row_height(ui));
        } else if (index == ui->hover) {
            fill(ui, ui->style.field, left, row_y, right - left,
                 row_height(ui));
        }

        int x = left + ui->style.padding + 4;
        int mark = row_height(ui) - 2 * ui->style.padding;
        if (mark < 10) {
            mark = 10;
        }
        draw_kind(ui, device->kind, x, row_y + ui->style.padding, mark);
        x += mark + ui->style.padding;

        unsigned long name_colour = ui->style.text;
        if (is_selected) {
            name_colour = ui->style.accent;
        }
        text(ui, x, band_baseline(ui, row_y), device->name, name_colour);
        x += net_style_text_width(ui->display, ui->style.font, device->name)
             + ui->style.padding * 2;

        /* The state, right after the name, in the state colour. */
        const char *state = state_word(device);
        unsigned long state_colour = (device->state == 1) ? ui->style.connected
                                                          : ui->style.text_muted;
        text(ui, x, band_baseline(ui, row_y), state, state_colour);

        /* The address on the right; the connection name before it when the
           interface is managed and has one. */
        int mark_right = right - ui->style.padding;
        if (device->address[0]) {
            int address_w = net_style_text_width(ui->display, ui->style.font,
                                                 device->address);
            mark_right -= address_w;
            text(ui, mark_right, band_baseline(ui, row_y), device->address,
                 ui->style.text);
            mark_right -= ui->style.padding * 2;
        }

        /* The connection name, clipped to the room between the state word and
           the address so it never overruns either. */
        if (device->connection[0]) {
            int from = x + net_style_text_width(ui->display, ui->style.font,
                                                state) + ui->style.padding;
            int room_for = mark_right - from;
            if (room_for > 0) {
                char fitted[NET_TEXT];
                net_style_fit(ui->display, ui->style.font, device->connection,
                              room_for, fitted, sizeof(fitted));
                text(ui, from, band_baseline(ui, row_y), fitted,
                     ui->style.text_muted);
            }
        }
    }
}

/* --- the DNS panel -------------------------------------------------------- */

/* The rectangle of DNS box `index`, for both the drawing and the click. The
   panel is: the "Current DNS" header and one line of servers, then the IPv4
   heading and its two boxes, then the IPv6 heading and its two boxes. A box is
   two rows — its label, then the field — so the field's y is the label's row
   plus one. Written once and used by both, so a box that is drawn and a box
   that is clicked are the same rectangle and cannot drift apart. */
int net_dns_box_rect(const NetUi *ui, int index,
                     int *x, int *y, int *width, int *height) {
    if (index < 0 || index > 3) {
        return 0;
    }
    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int row = row_height(ui);
    int column_gap = ui->style.padding * 2;
    int half = (right - left - column_gap) / 2;

    /* The panel, top down: the "Current DNS" header, the servers line, a gap,
       then four boxes in two rows of two — IPv4's pair, a gap, IPv6's pair.
       Each box is two rows (its label, then its field), and the family is named
       IN the label ("IPv4 Preferred"), so there is no separate heading row to
       drift out of step. `label_top` is the first box's label row; a box's
       field is one row below its label. */
    int label_top = body_top_y(ui) + 2 * row + ui->style.padding;
    int group = index / 2;                 /* 0 = IPv4, 1 = IPv6 */
    int label_y = label_top + group * (2 * row + ui->style.padding);
    int field_y = label_y + row;

    int column = index % 2;                /* 0 = Preferred, 1 = Alternate */
    *x = left + column * (half + column_gap);
    *y = field_y;
    *width = half;
    *height = row;
    return 1;
}

/* One labelled box, at a given span and field row: the label on the row above
   the field, the field itself, the text, and a caret when the box has the
   focus. `box_y` is the FIELD row; the label goes one row above it, which is
   the same geometry net_dns_box_rect() reports. */
static void draw_dns_box(NetUi *ui, int x, int width, int box_y,
                         const char *label, const char *value, int focused) {
    int ascent = ui->style.font ? ui->style.font->ascent : 16;
    int field_h = row_height(ui);

    text(ui, x, box_y - row_height(ui) + ascent, label, ui->style.text_muted);

    fill(ui, ui->style.field, x, box_y, width, field_h);
    outline(ui, focused ? ui->style.accent : ui->style.panel_edge,
            x, box_y, width, field_h);

    char shown[NET_TEXT];
    net_style_fit(ui->display, ui->style.font, value,
                  width - 2 * ui->style.padding, shown, sizeof(shown));
    text(ui, x + ui->style.padding, box_y + ascent, shown, ui->style.text);

    if (focused) {
        fill(ui, ui->style.accent,
             x + ui->style.padding +
                 net_style_text_width(ui->display, ui->style.font, shown),
             box_y + ui->style.padding / 2, 2, field_h - ui->style.padding);
    }
}

static void draw_dns(NetUi *ui) {
    int top = body_top_y(ui);
    int left = ui->style.padding;
    int right = ui->width - ui->style.padding;
    int ascent = ui->style.font ? ui->style.font->ascent : 16;
    int column_gap = ui->style.padding * 2;
    int half = (right - left - column_gap) / 2;
    int y = top;

    /* The servers as they are now, and where they came from, so the current
       state is readable without opening another tool. */
    char header[NET_TEXT * 2];
    if (ui->dns.source[0]) {
        snprintf(header, sizeof(header), "Current DNS (from %s):",
                 ui->dns.source);
    } else {
        snprintf(header, sizeof(header), "Current DNS:");
    }
    text(ui, left, y + ascent, header, ui->style.accent);
    y += row_height(ui);

    if (ui->dns.count == 0) {
        text(ui, left + ui->style.padding, y + ascent,
             "(none set — the network's own are used)",
             ui->style.text_muted);
        y += row_height(ui);
    } else {
        char line[NET_TEXT * 4];
        const char *join = "";
        line[0] = '\0';
        for (int i = 0; i < ui->dns.count; i++) {
            size_t used = strlen(line);
            snprintf(line + used, sizeof(line) - used, "%s%s", join,
                     ui->dns.servers[i]);
            join = "   ";
        }
        text(ui, left + ui->style.padding, y + ascent, line, ui->style.text);
        y += row_height(ui);
    }

    y += ui->style.padding;

    /* Four boxes in two rows of two: IPv4's pair, then IPv6's pair. The family
       is named IN each label ("IPv4 Preferred") rather than on a heading row of
       its own, so the geometry net_dns_box_rect() reports and the geometry this
       draws are the same walk down from the top — no separate heading to get
       out of step with the click. */
    static const char *const labels[4] = {
        "IPv4 Preferred", "IPv4 Alternate",
        "IPv6 Preferred", "IPv6 Alternate",
    };
    const char *const values[4] = {
        ui->dns_v4_pref, ui->dns_v4_alt, ui->dns_v6_pref, ui->dns_v6_alt,
    };

    for (int i = 0; i < 4; i++) {
        int bx = 0, by = 0, bw = 0, bh = 0;
        net_dns_box_rect(ui, i, &bx, &by, &bw, &bh);
        draw_dns_box(ui, bx, bw, by, labels[i], values[i],
                     ui->dns_focus == i);
    }

    /* The hint sits under the last box's field row. */
    y = body_top_y(ui) + 2 * row_height(ui) + ui->style.padding +
        2 * (2 * row_height(ui) + ui->style.padding) + row_height(ui);
    (void)half;
    (void)column_gap;
    (void)ascent;
    text(ui, left, y + ascent,
         "Apply sets them. All empty = automatic (the network's own).",
         ui->style.text_muted);
}

/* --- the foot ------------------------------------------------------------- */

static void draw_status_line(NetUi *ui) {
    if (!ui->status[0]) {
        return;
    }
    char fitted[NET_TEXT * 4];
    net_style_fit(ui->display, ui->style.font, ui->status,
                  ui->width - 2 * ui->style.padding, fitted, sizeof(fitted));
    text(ui, ui->style.padding, band_baseline(ui, status_y(ui)), fitted,
         ui->style.text);
}

static void draw_help(NetUi *ui) {
    const char *help;
    if (ui->mode == NET_MODE_DNS) {
        help = "Tab switch field   Enter apply   Escape back";
    } else {
        help = "Up/Down move   Enter up/down   w wifi   d DNS   p DPI   "
               "r refresh   Escape close";
    }
    int descent = ui->style.font ? ui->style.font->descent : 4;
    text(ui, ui->style.padding, help_y(ui) + row_height(ui) - descent - 2,
         help, ui->style.text_muted);
}

/* --- the frame ------------------------------------------------------------ */

void net_draw(NetUi *ui) {
    if (!ui->display || ui->window == None) {
        return;
    }

    /* The off-screen buffer, made when the window opens and remade whenever
       the window is resized. Everything below is drawn into it, and the
       finished frame is copied to the window in one go at the end. */
    if (!ui->buffer || ui->buffer_width != ui->width ||
        ui->buffer_height != ui->height) {
        if (ui->buffer) {
            XFreePixmap(ui->display, ui->buffer);
        }
        ui->buffer = XCreatePixmap(ui->display, ui->root,
                                   (unsigned int)ui->width,
                                   (unsigned int)ui->height,
                                   (unsigned int)DefaultDepth(ui->display,
                                                              ui->screen));
        ui->buffer_width = ui->width;
        ui->buffer_height = ui->height;
    }

    fill(ui, ui->style.background, 0, 0, ui->width, ui->height);

    draw_status_bar(ui);

    if (ui->mode == NET_MODE_DNS) {
        draw_dns(ui);
    } else {
        draw_devices(ui);
    }

    draw_status_line(ui);
    draw_buttons(ui);
    draw_help(ui);

    XCopyArea(ui->display, target(ui), ui->window, ui->gc, 0, 0,
              (unsigned int)ui->width, (unsigned int)ui->height, 0, 0);
    XFlush(ui->display);
}
