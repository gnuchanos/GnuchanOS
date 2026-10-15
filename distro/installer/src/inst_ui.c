/*
 * inst_ui.c - the installer's window.
 *
 * Xlib and Xft, drawn the way every other Gnuchan program draws itself (see
 * GnuChanSL, GnuChanDM): one frame is built on an off-screen pixmap and copied
 * to the window whole, so a redraw never shows a half-drawn screen. The pieces
 * are a title, a short line of explanation, one row per answer, a button, and -
 * filling the lower half - the installer's output, which the engine feeds line
 * by line.
 *
 * Nothing here touches a disk. A click chooses a disk, focuses an answer, or
 * presses the button; the button starts the engine (inst_engine.c), which runs
 * the script. See inst.h for the split.
 *
 * License: GPL3.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "inst.h"

/* The window's own padding, in pixels, on every edge. */
#define PAD 24

/* --- colours --------------------------------------------------------------- */

int inst_load_colour(InstApp *app, const char *name, const char *fallback,
                     XftColor *out) {
    Visual *visual = DefaultVisual(app->display, app->screen);
    Colormap colormap = DefaultColormap(app->display, app->screen);
    if (name != NULL && name[0] != '\0' &&
        XftColorAllocName(app->display, visual, colormap, name, out)) {
        return 1;
    }
    return XftColorAllocName(app->display, visual, colormap, fallback, out);
}

void inst_free_colours(InstApp *app) {
    Visual *visual = DefaultVisual(app->display, app->screen);
    Colormap colormap = DefaultColormap(app->display, app->screen);
    XftColor *all[] = {
        &app->colours.background, &app->colours.panel,
        &app->colours.panel_edge, &app->colours.field,
        &app->colours.field_active, &app->colours.text,
        &app->colours.text_muted, &app->colours.accent,
        &app->colours.danger, &app->colours.good,
    };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        XftColorFree(app->display, visual, colormap, all[i]);
    }
}

/* --- small drawing helpers ------------------------------------------------- */

static void text_at(InstApp *app, XftColor *colour, XftFont *font,
                    int x, int baseline, const char *text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }
    XftDrawStringUtf8(app->draw, colour, font, x, baseline,
                      (const FcChar8 *)text, (int)strlen(text));
}

static int text_width(InstApp *app, XftFont *font, const char *text) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(app->display, font, (const FcChar8 *)text,
                       (int)strlen(text), &extents);
    return extents.xOff;
}

/* A rectangle outline two pixels thick, drawn by hand so its weight does not
   depend on the server's line width. */
static void box_outline(InstApp *app, unsigned long pixel,
                        int x, int y, int w, int h) {
    XSetForeground(app->display, app->gc, pixel);
    for (int i = 0; i < 2; i++) {
        XDrawRectangle(app->display, app->buffer, app->gc, x + i, y + i,
                       (unsigned int)(w - 2 * i - 1),
                       (unsigned int)(h - 2 * i - 1));
    }
}

/* --- the layout ------------------------------------------------------------ */

void inst_layout(InstApp *app) {
    int row_y = 96;                 /* below the title and the sub-line         */
    int field_h = app->font->ascent + app->font->descent + 14;
    int dropdown_rows = app->disk_count < INST_DISK_DROPDOWN_ROWS
        ? app->disk_count : INST_DISK_DROPDOWN_ROWS;

    for (int i = 0; i < app->field_count; i++) {
        InstField *field = &app->fields[i];
        field->box_x = PAD + 150;
        field->box_y = row_y;
        field->box_w = app->width - field->box_x - PAD;
        field->box_h = field_h;
        row_y += field_h + 12;
        if (i == 0 && app->disk_dropdown_open) {
            row_y += dropdown_rows * field_h + 8;
        }
    }

    app->button_x = PAD;
    app->button_y = row_y + 8;
    app->button_w = app->width - 2 * PAD;
    app->button_h = app->font->ascent + app->font->descent + 20;
    const char *erase_prompt = "Type ERASE in this box, then press Enter:";
    app->erase_box_x = PAD + text_width(app, app->font, erase_prompt) + 12;
    app->erase_box_y = app->button_y + app->button_h + 8;
    app->erase_box_w = 190;
    app->erase_box_h = field_h;
}

/* --- the output ring ------------------------------------------------------- */

void inst_add_line(InstApp *app, const char *text) {
    /* Keep a copy; a NULL or empty line is kept as an empty string so blank
       lines the script prints stay blank rather than vanish. */
    char *copy = strdup(text != NULL ? text : "");
    if (copy == NULL) {
        return;
    }
    if (app->line_count < INST_OUTPUT_LINES) {
        app->lines[(app->line_head + app->line_count) % INST_OUTPUT_LINES] =
            copy;
        app->line_count++;
    } else {
        free(app->lines[app->line_head]);
        app->lines[app->line_head] = copy;
        app->line_head = (app->line_head + 1) % INST_OUTPUT_LINES;
    }
}

static const char *line_at(InstApp *app, int index) {
    return app->lines[(app->line_head + index) % INST_OUTPUT_LINES];
}

void inst_set_note(InstApp *app, int is_error, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->note, sizeof(app->note), fmt, args);
    va_end(args);
    app->note_is_error = is_error;
}

/* --- drawing --------------------------------------------------------------- */

void inst_draw(InstApp *app) {
    int line_h = app->font->ascent + app->font->descent + 4;

    /* background */
    XSetForeground(app->display, app->gc, app->colours.background.pixel);
    XFillRectangle(app->display, app->buffer, app->gc, 0, 0,
                   (unsigned int)app->width, (unsigned int)app->height);

    /* title and sub-line */
    text_at(app, &app->colours.accent, app->font_bold, PAD,
            PAD + app->font_bold->ascent,
            "Install GnuChanOS to a disk");
    text_at(app, &app->colours.text_muted, app->font, PAD,
            PAD + app->font_bold->ascent + app->font->ascent + 26,
            "The live desktop you are using is copied to the disk as it is.");

    /* the field rows */
    for (int i = 0; i < app->field_count; i++) {
        InstField *field = &app->fields[i];
        int label_baseline = field->box_y +
            (field->box_h + app->font->ascent - app->font->descent) / 2;
        text_at(app, &app->colours.text, app->font, PAD, label_baseline,
                field->label);

        /* the box */
        int active = ((i == app->field_active) || (i == 0 && app->disk_dropdown_open))
                     && !app->running;
        XSetForeground(app->display, app->gc,
                       active ? app->colours.field_active.pixel
                              : app->colours.field.pixel);
        XFillRectangle(app->display, app->buffer, app->gc, field->box_x,
                       field->box_y, (unsigned int)field->box_w,
                       (unsigned int)field->box_h);
        box_outline(app, active ? app->colours.accent.pixel
                                : app->colours.panel_edge.pixel,
                    field->box_x, field->box_y, field->box_w, field->box_h);

        /* the text in it: dots when the field is a secret, the text otherwise */
        char shown[INST_FIELD_TEXT];
        if (field->kind == INST_FIELD_SECRET) {
            int n = 0;
            for (int k = 0; k < field->length &&
                            n < (int)sizeof(shown) - 1; k++) {
                if (((unsigned char)field->text[k] & 0xc0) != 0x80) {
                    shown[n++] = '*';
                }
            }
            shown[n] = '\0';
        } else {
            snprintf(shown, sizeof(shown), "%s", field->text);
        }
        int text_baseline = field->box_y +
            (field->box_h + app->font->ascent - app->font->descent) / 2;
        text_at(app, &app->colours.text, app->font,
                field->box_x + 12, text_baseline, shown);

        if (i == 0) {
            int arrow_x = field->box_x + field->box_w - 20;
            int arrow_y = field->box_y + field->box_h / 2;
            unsigned long arrow_colour = active
                ? app->colours.accent.pixel : app->colours.text_muted.pixel;
            XSetForeground(app->display, app->gc, arrow_colour);
            XDrawLine(app->display, app->buffer, app->gc,
                      arrow_x - 4, arrow_y - 2, arrow_x, arrow_y + 2);
            XDrawLine(app->display, app->buffer, app->gc,
                      arrow_x, arrow_y + 2, arrow_x + 4, arrow_y - 2);
        }
    }

    if (app->disk_dropdown_open && app->disk_count > 1) {
        InstField *field = &app->fields[0];
        int row_h = field->box_h;
        int visible = app->disk_count < INST_DISK_DROPDOWN_ROWS
            ? app->disk_count : INST_DISK_DROPDOWN_ROWS;
        int menu_y = field->box_y + field->box_h;
        int menu_h = visible * row_h;
        XSetForeground(app->display, app->gc, app->colours.field.pixel);
        XFillRectangle(app->display, app->buffer, app->gc, field->box_x,
                       menu_y, (unsigned int)field->box_w,
                       (unsigned int)menu_h);
        box_outline(app, app->colours.accent.pixel, field->box_x, menu_y,
                    field->box_w, menu_h);

        for (int row = 0; row < visible; row++) {
            int choice = app->disk_dropdown_scroll + row;
            int item_y = menu_y + row * row_h;
            if (choice == app->disk_dropdown_cursor) {
                XSetForeground(app->display, app->gc,
                               app->colours.field_active.pixel);
                XFillRectangle(app->display, app->buffer, app->gc,
                               field->box_x + 2, item_y + 1,
                               (unsigned int)(field->box_w - 4),
                               (unsigned int)(row_h - 2));
            }
            int baseline = item_y +
                (row_h + app->font->ascent - app->font->descent) / 2;
            text_at(app, &app->colours.text, app->font,
                    field->box_x + 12, baseline,
                    app->disks[choice].description);
            if (row + 1 < visible) {
                XSetForeground(app->display, app->gc,
                               app->colours.panel_edge.pixel);
                XDrawLine(app->display, app->buffer, app->gc,
                          field->box_x + 2, item_y + row_h - 1,
                          field->box_x + field->box_w - 3,
                          item_y + row_h - 1);
            }
        }
    }

    /* the button */
    int button_hot = app->button_hot && !app->running;
    XSetForeground(app->display, app->gc,
                   app->running ? app->colours.panel_edge.pixel
                                : (button_hot ? app->colours.accent.pixel
                                              : app->colours.panel.pixel));
    XFillRectangle(app->display, app->buffer, app->gc, app->button_x,
                   app->button_y, (unsigned int)app->button_w,
                   (unsigned int)app->button_h);
    box_outline(app, app->colours.accent.pixel, app->button_x, app->button_y,
                app->button_w, app->button_h);

    const char *button_text;
    if (app->running) {
        button_text = "Installing...";
    } else if (app->confirming_erase) {
        button_text = "Confirm disk erase below";
    } else if (app->finished && app->exit_code == 0) {
        button_text = "Done - close this window";
    } else {
        button_text = "Install GnuChanOS";
    }
    int bw = text_width(app, app->font_bold, button_text);
    int button_baseline = app->button_y +
        (app->button_h + app->font_bold->ascent - app->font_bold->descent) / 2;
    text_at(app, &app->colours.text, app->font_bold,
            app->button_x + (app->button_w - bw) / 2, button_baseline,
            button_text);

    /* the note under the button */
    if (app->confirming_erase) {
        int baseline = app->button_y + app->button_h + app->font->ascent + 12;
        text_at(app, &app->colours.danger, app->font, PAD, baseline,
                "Type ERASE in this box, then press Enter:");

        XSetForeground(app->display, app->gc,
                       app->colours.field_active.pixel);
        XFillRectangle(app->display, app->buffer, app->gc,
                       app->erase_box_x, app->erase_box_y,
                       (unsigned int)app->erase_box_w,
                       (unsigned int)app->erase_box_h);
        box_outline(app, app->colours.accent.pixel, app->erase_box_x,
                    app->erase_box_y, app->erase_box_w, app->erase_box_h);
        int input_baseline = app->erase_box_y +
            (app->erase_box_h + app->font->ascent - app->font->descent) / 2;
        text_at(app, &app->colours.text, app->font,
                app->erase_box_x + 10, input_baseline,
                app->erase_confirmation);
    } else if (app->note[0] != '\0') {
        text_at(app, app->note_is_error ? &app->colours.danger
                                        : &app->colours.text_muted,
                app->font, PAD,
                app->button_y + app->button_h + app->font->ascent + 12,
                app->note);
    }

    /* the output pane fills what is left, down to the bottom padding */
    int out_y = app->button_y + app->button_h + app->font->ascent +
                app->font->descent + 24;
    int out_h = app->height - out_y - PAD;
    if (out_h > 40) {
        XSetForeground(app->display, app->gc, app->colours.field.pixel);
        XFillRectangle(app->display, app->buffer, app->gc, PAD, out_y,
                       (unsigned int)(app->width - 2 * PAD),
                       (unsigned int)out_h);
        box_outline(app, app->colours.panel_edge.pixel, PAD, out_y,
                    app->width - 2 * PAD, out_h);

        /* Show the tail of the output: as many of the last lines as fit. */
        int rows = (out_h - 16) / line_h;
        if (rows < 1) {
            rows = 1;
        }
        int first = app->line_count - rows;
        if (first < 0) {
            first = 0;
        }
        int y = out_y + 12 + app->font->ascent;
        for (int i = first; i < app->line_count; i++) {
            text_at(app, &app->colours.text, app->font, PAD + 10, y,
                    line_at(app, i));
            y += line_h;
        }
    }

    /* the finished frame, in one copy */
    XCopyArea(app->display, app->buffer, app->window, app->gc, 0, 0,
              (unsigned int)app->width, (unsigned int)app->height, 0, 0);
    XFlush(app->display);
}

/* --- the answers, checked before the disk is touched ----------------------- */

int inst_validate(InstApp *app) {
    if (app->disk_choice < 0 || app->disk_choice >= app->disk_count) {
        inst_set_note(app, 1, "No disk to install onto was found.");
        return 0;
    }
    const char *user = app->fields[1].text;
    size_t user_length = strlen(user);
    if (user_length == 0) {
        inst_set_note(app, 1, "Enter a login name.");
        return 0;
    }
    if (user_length > 32 ||
        !(islower((unsigned char)user[0]) || user[0] == '_')) {
        inst_set_note(app, 1, "Login names must start with a lowercase letter "
                              "or _ and contain at most 32 characters.");
        return 0;
    }
    for (const char *p = user + 1; *p; p++) {
        if (!(islower((unsigned char)*p) || isdigit((unsigned char)*p) ||
              *p == '_' || *p == '-')) {
            inst_set_note(app, 1, "The login name may contain only lowercase "
                                  "letters, digits, _ and -.");
            return 0;
        }
    }
    if (strcmp(user, "root") == 0 || strcmp(user, "daemon") == 0 ||
        strcmp(user, "bin") == 0 || strcmp(user, "sys") == 0 ||
        strcmp(user, "nobody") == 0) {
        inst_set_note(app, 1, "Choose a regular, non-system login name.");
        return 0;
    }
    if (app->fields[3].text[0] == '\0') {
        inst_set_note(app, 1, "Enter a password.");
        return 0;
    }
    if (strlen(app->fields[3].text) > 255) {
        inst_set_note(app, 1, "The password may contain at most 255 characters.");
        return 0;
    }
    if (strcmp(app->fields[3].text, app->fields[4].text) != 0) {
        inst_set_note(app, 1, "The passwords do not match.");
        return 0;
    }
    const char *hostname = app->fields[5].text;
    size_t hostname_length = strlen(hostname);
    if (hostname_length == 0 || hostname_length > 63 ||
        !(isalpha((unsigned char)hostname[0]) ||
          isdigit((unsigned char)hostname[0]))) {
        inst_set_note(app, 1, "Enter a hostname of 1-63 letters, "
                              "digits or hyphens.");
        return 0;
    }
    for (const char *p = hostname; *p; p++) {
        if (!(isalpha((unsigned char)*p) || isdigit((unsigned char)*p) ||
              *p == '-')) {
            inst_set_note(app, 1, "The hostname may contain only letters, "
                                  "digits and hyphens.");
            return 0;
        }
    }
    if (hostname[hostname_length - 1] == '-') {
        inst_set_note(app, 1, "A hostname cannot end with a hyphen.");
        return 0;
    }
    return 1;
}

/* --- the pointer ----------------------------------------------------------- */

void inst_pointer_motion(InstApp *app, int x, int y) {
    int hot = (x >= app->button_x && x < app->button_x + app->button_w &&
               y >= app->button_y && y < app->button_y + app->button_h);
    if (hot != app->button_hot) {
        app->button_hot = hot;
        inst_draw(app);
    }
}

void inst_button_press(InstApp *app, int x, int y) {
    if (app->running) {
        return;
    }
    if (app->disk_dropdown_open) {
        InstField *field = &app->fields[0];
        int visible = app->disk_count < INST_DISK_DROPDOWN_ROWS
            ? app->disk_count : INST_DISK_DROPDOWN_ROWS;
        int menu_y = field->box_y + field->box_h;
        int menu_bottom = menu_y + visible * field->box_h;
        if (x >= field->box_x && x < field->box_x + field->box_w &&
            y >= menu_y && y < menu_bottom) {
            int row = (y - menu_y) / field->box_h;
            app->disk_choice = app->disk_dropdown_scroll + row;
            app->disk_dropdown_cursor = app->disk_choice;
            snprintf(field->text, sizeof(field->text), "%s",
                     app->disks[app->disk_choice].description);
            field->length = (int)strlen(field->text);
            app->disk_dropdown_open = 0;
            app->field_active = 0;
            inst_layout(app);
        } else if (x >= field->box_x && x < field->box_x + field->box_w &&
                   y >= field->box_y && y < field->box_y + field->box_h) {
            app->disk_dropdown_open = 0;
            app->field_active = 0;
            inst_layout(app);
        } else {
            app->disk_dropdown_open = 0;
            inst_layout(app);
        }
        inst_draw(app);
        return;
    }
    if (app->confirming_erase) {
        if (x >= app->erase_box_x && x < app->erase_box_x + app->erase_box_w &&
            y >= app->erase_box_y && y < app->erase_box_y + app->erase_box_h) {
            return;
        }
        if (x >= app->button_x && x < app->button_x + app->button_w &&
            y >= app->button_y && y < app->button_y + app->button_h) {
            inst_set_note(app, 1, "Type ERASE in the box below, then press Enter.");
        } else {
            app->confirming_erase = 0;
            app->erase_confirmation[0] = '\0';
            inst_set_note(app, 0, "Erase confirmation cancelled.");
        }
        inst_draw(app);
        return;
    }

    /* the button? */
    if (x >= app->button_x && x < app->button_x + app->button_w &&
        y >= app->button_y && y < app->button_y + app->button_h) {
        if (app->finished && app->exit_code == 0) {
            app->should_close = 1;
            return;
        }
        if (inst_validate(app)) {
            app->confirming_erase = 1;
            app->erase_confirmation[0] = '\0';
            inst_set_note(app, 1,
                          "ALL DATA on %s will be erased. Type ERASE, then Enter.",
                          app->disks[app->disk_choice].device);
            inst_draw(app);
        }
        return;
    }

    /* a field? */
    for (int i = 0; i < app->field_count; i++) {
        InstField *field = &app->fields[i];
        if (x >= field->box_x && x < field->box_x + field->box_w &&
            y >= field->box_y && y < field->box_y + field->box_h) {
            if (i == 0) {
                if (app->disk_count > 1) {
                    app->disk_dropdown_open = 1;
                    app->disk_dropdown_cursor = app->disk_choice;
                    app->disk_dropdown_scroll = app->disk_choice;
                    if (app->disk_dropdown_scroll >= INST_DISK_DROPDOWN_ROWS) {
                        app->disk_dropdown_scroll =
                            app->disk_dropdown_cursor - INST_DISK_DROPDOWN_ROWS + 1;
                    }
                    app->field_active = 0;
                    inst_layout(app);
                }
            } else {
                app->field_active = i;
            }
            inst_draw(app);
            return;
        }
    }
}
