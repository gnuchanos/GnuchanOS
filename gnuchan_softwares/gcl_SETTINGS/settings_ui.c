 /*
 * settings_ui.c — the window's behaviour: categories, fields, and Save.
 *
 * This is the half of GnuChanSettings that answers what a person does. It owns
 * the window, reads the events, and keeps the values a person has typed in
 * memory until Save writes them. Nothing here draws: every pixel is put down
 * by settings_draw.c, which reads the state this file keeps. The two walk the
 * SAME geometry (settings_ui.h), so a click lands on the field drawn under it.
 *
 * The whole promise of the program is in two keys: typing changes a value on
 * screen and nothing else, and Save is what writes the file. A person can
 * therefore open the panel, change six things, dislike all six, close it, and
 * have changed nothing on disk.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "settings_draw.h"
#include "settings_ui.h"

/* --- the geometry, computed in one place ----------------------------------- */

int settings_ui_category_y(int index) {
    return UI_HEADER_HEIGHT + index * UI_CATEGORY_HEIGHT;
}

int settings_ui_category_at(int y) {
    if (y < UI_HEADER_HEIGHT) {
        return -1;
    }
    return (y - UI_HEADER_HEIGHT) / UI_CATEGORY_HEIGHT;
}

int settings_ui_visible_rows(void) {
    int body = UI_WINDOW_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT;
    return body / UI_ROW_HEIGHT;
}

void settings_ui_row_field_rect(const SettingsUi *ui, int row,
                                int *x, int *y, int *width, int *height) {
    /* `row` is the absolute row in the list; the scroll decides which line of
       the body it is drawn on. This is the same subtraction draw_row() makes,
       and it has to be the same one or a click would land a row or two away
       from the field it was aimed at once the list has been scrolled. */
    int shown = row - ui->scroll;
    int row_y = UI_HEADER_HEIGHT + shown * UI_ROW_HEIGHT;
    *x = UI_WINDOW_WIDTH - UI_MARGIN - UI_FIELD_WIDTH;
    *y = row_y + 8;
    *width = UI_FIELD_WIDTH;
    *height = UI_ROW_HEIGHT - 16;
}

int settings_ui_row_at(const SettingsUi *ui, int x, int y) {
    if (x < UI_SIDEBAR_WIDTH) {
        return -1;                        /* the sidebar, not the body */
    }
    int body_top = UI_HEADER_HEIGHT;
    int body_bottom = UI_WINDOW_HEIGHT - UI_FOOTER_HEIGHT;
    if (y < body_top || y >= body_bottom) {
        return -1;
    }
    int row = (y - body_top) / UI_ROW_HEIGHT;
    return ui->scroll + row;
}

void settings_ui_save_rect(const SettingsUi *ui, int *x, int *y,
                           int *width, int *height) {
    (void)ui;
    *width = 130;
    *height = UI_FOOTER_HEIGHT - 24;
    *x = UI_WINDOW_WIDTH - UI_MARGIN - *width;
    *y = UI_WINDOW_HEIGHT - UI_FOOTER_HEIGHT + 12;
}

void settings_ui_revert_rect(const SettingsUi *ui, int *x, int *y,
                             int *width, int *height) {
    settings_ui_save_rect(ui, x, y, width, height);
    *x -= (*width + 12);
}

/* The dropdown hangs directly under the field it belongs to, one name per
   UI_CHOICE_HEIGHT. Both the drawing and the hit-testing come through here, so
   the name a click lands on is the name that was drawn. */
void settings_ui_choice_rect(const SettingsUi *ui, int row, int index,
                             int *x, int *y, int *width, int *height) {
    int fx, fy, fw, fh;
    settings_ui_row_field_rect(ui, row, &fx, &fy, &fw, &fh);
    *x = fx;
    *y = fy + fh + index * UI_CHOICE_HEIGHT;
    *width = fw;
    *height = UI_CHOICE_HEIGHT;
}

int settings_ui_choice_at(const SettingsUi *ui, int x, int y) {
    if (ui->dropdown_row < 0 || !ui->page.app ||
        ui->dropdown_row >= ui->page.app->setting_count) {
        return -1;
    }
    int count =
        settings_choice_count(&ui->page.app->settings[ui->dropdown_row]);
    if (count <= 0) {
        return -1;
    }
    int fx, fy, fw, fh;
    settings_ui_row_field_rect(ui, ui->dropdown_row, &fx, &fy, &fw, &fh);
    int list_top = fy + fh;
    if (x < fx || x >= fx + fw || y < list_top) {
        return -1;
    }
    int index = (y - list_top) / UI_CHOICE_HEIGHT;
    if (index < 0 || index >= count) {
        return -1;
    }
    return index;
}

/* --- loading a category ---------------------------------------------------- */

void settings_ui_load_page(SettingsUi *ui) {
    ui->page.app = &ui->apps[ui->app_index];
    ui->page.loaded = settings_load(ui->page.app, ui->page.values);
    ui->focus_row = UI_FOCUS_NONE;
    ui->hover_row = -1;
    ui->dropdown_row = -1;
    ui->scroll = 0;
    ui->status[0] = '\0';
    ui->status_failed = 0;

    if (ui->page.loaded == 1) {
        snprintf(ui->status, sizeof(ui->status),
                 "%s ayarlari okundu.", ui->page.app->name);
    } else {
        snprintf(ui->status, sizeof(ui->status),
                 "%s icin ayar dosyasi yok; varsayilanlar gosteriliyor ve "
                 "kaydedince olusturulacak.", ui->page.app->name);
    }
    ui->status_failed = 0;
}

/* --- acting on the page ---------------------------------------------------- */

static void switch_category(SettingsUi *ui, int index) {
    if (index < 0 || index >= ui->app_count) {
        return;
    }
    ui->app_index = index;
    settings_ui_load_page(ui);
}

static void save_page(SettingsUi *ui) {
    char reason[SETTINGS_TEXT_LENGTH];
    if (settings_save(ui->page.app, ui->page.values, reason) == 0) {
        snprintf(ui->status, sizeof(ui->status), "%s kaydedildi.",
                 ui->page.app->name);
        ui->status_failed = 0;
    } else {
        snprintf(ui->status, sizeof(ui->status), "Kaydedilemedi: %.200s",
                 reason);
        ui->status_failed = 1;
    }
}

/* --- the keyboard ---------------------------------------------------------- */

/* Put a character into the focused field, if that field takes text. A number
   field takes only digits and a minus or a dot; a switch takes no typing at
   all — it is toggled, not typed. */
static int field_accepts_char(const SettingDef *def, char c) {
    if (def->type == SETTING_INT || def->type == SETTING_REAL) {
        if (isdigit((unsigned char)c)) {
            return 1;
        }
        return c == '-' || (def->type == SETTING_REAL && c == '.');
    }
    return 1;      /* a colour and a text take anything the person types */
}

static void field_insert(SettingsUi *ui, const char *text) {
    if (ui->focus_row < 0 || ui->focus_row >= ui->page.app->setting_count) {
        return;
    }
    const SettingDef *def = &ui->page.app->settings[ui->focus_row];
    /* A switch is toggled and a choice is chosen from its list; neither is
       typed into, so a keystroke must not land in either. */
    if (def->type == SETTING_BOOL || def->type == SETTING_CHOICE) {
        return;
    }
    SettingValue *value = &ui->page.values[ui->focus_row];

    size_t length = strlen(value->text);
    for (const char *p = text; *p; p++) {
        if (!field_accepts_char(def, *p)) {
            continue;
        }
        if (length + 1 >= sizeof(value->text)) {
            break;
        }
        value->text[length++] = *p;
        value->text[length] = '\0';
    }
    ui->caret = (int)length;
}

static void field_backspace(SettingsUi *ui) {
    if (ui->focus_row < 0) {
        return;
    }
    SettingValue *value = &ui->page.values[ui->focus_row];
    size_t length = strlen(value->text);
    if (length == 0) {
        return;
    }
    value->text[length - 1] = '\0';
    ui->caret = (int)(length - 1);
}

static void toggle_bool(SettingsUi *ui, int row) {
    if (row < 0 || row >= ui->page.app->setting_count) {
        return;
    }
    if (ui->page.app->settings[row].type != SETTING_BOOL) {
        return;
    }
    SettingValue *value = &ui->page.values[row];
    int flag = setting_value_bool(value) ? 0 : 1;
    setting_value_set_bool(value, flag, ui->page.app->capital_bools);
}

static void handle_key(SettingsUi *ui, XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);
    if (symbol == NoSymbol) {
        return;
    }

    switch (symbol) {
    case XK_Escape:
        /* Escape puts an open list away first, and only closes the panel when
           there is no list to close. */
        if (ui->dropdown_row >= 0) {
            ui->dropdown_row = -1;
            return;
        }
        ui->running = 0;
        return;
    case XK_BackSpace:
        field_backspace(ui);
        return;
    case XK_Tab:
        if (ui->page.app->setting_count > 0) {
            ui->focus_row = (ui->focus_row + 1) % ui->page.app->setting_count;
            ui->caret = (int)strlen(ui->page.values[ui->focus_row].text);
        }
        return;
    default:
        break;
    }

    char text[8];
    int length = XLookupString(key, text, sizeof(text) - 1, NULL, NULL);
    if (length <= 0) {
        return;
    }
    text[length] = '\0';
    for (int i = 0; i < length; i++) {
        if ((unsigned char)text[i] < 0x20) {
            return;
        }
    }
    field_insert(ui, text);
}

/* --- the pointer ----------------------------------------------------------- */

static void handle_click(SettingsUi *ui, XButtonEvent *press) {
    if (press->button != Button1) {
        return;
    }
    int x = press->x;
    int y = press->y;

    int bx, by, bw, bh;
    settings_ui_save_rect(ui, &bx, &by, &bw, &bh);
    if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
        save_page(ui);
        return;
    }
    settings_ui_revert_rect(ui, &bx, &by, &bw, &bh);
    if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
        settings_ui_load_page(ui);
        return;
    }

    if (x < UI_SIDEBAR_WIDTH) {
        int category = settings_ui_category_at(y);
        if (category >= 0 && category < ui->app_count) {
            switch_category(ui, category);
        }
        return;
    }

    /* An open dropdown answers the click first: a name in it is chosen, and a
       click anywhere else puts the list away before the click is read again. */
    if (ui->dropdown_row >= 0) {
        int choice = settings_ui_choice_at(ui, x, y);
        if (choice >= 0) {
            const SettingDef *def = &ui->page.app->settings[ui->dropdown_row];
            const char *const *list = settings_choices(def);
            if (list && list[choice]) {
                snprintf(ui->page.values[ui->dropdown_row].text,
                         sizeof(ui->page.values[ui->dropdown_row].text),
                         "%s", list[choice]);
            }
            ui->dropdown_row = -1;
            return;
        }
        ui->dropdown_row = -1;
        /* Fall through: the click may still be on a field. */
    }

    /* The body: a row. A click on a switch toggles it, a click on a choice
       opens its list, and a click on any other field puts the keyboard in it. */
    int row = settings_ui_row_at(ui, x, y);
    if (row < 0 || row >= ui->page.app->setting_count) {
        ui->focus_row = UI_FOCUS_NONE;
        return;
    }
    const SettingDef *def = &ui->page.app->settings[row];
    ui->focus_row = row;
    ui->caret = (int)strlen(ui->page.values[row].text);
    if (def->type == SETTING_BOOL) {
        toggle_bool(ui, row);
    } else if (def->type == SETTING_CHOICE) {
        ui->dropdown_row = row;
    }
}

static void handle_motion(SettingsUi *ui, XMotionEvent *motion) {
    int row = settings_ui_row_at(ui, motion->x, motion->y);
    int category = motion->x < UI_SIDEBAR_WIDTH
                       ? settings_ui_category_at(motion->y)
                       : -1;
    if (row != ui->hover_row || category != ui->hover_category) {
        ui->hover_row = row;
        ui->hover_category = category;
        settings_draw(ui);
    }
}

/* The wheel over the body scrolls the rows, kept inside the list so a person
   cannot scroll past the last setting into empty space. */
static void handle_wheel(SettingsUi *ui, XButtonEvent *press) {
    int rows = ui->page.app->setting_count;
    int visible = settings_ui_visible_rows();
    int maximum = rows - visible;
    if (maximum < 0) {
        maximum = 0;
    }
    if (press->button == Button4) {
        ui->scroll--;
    } else if (press->button == Button5) {
        ui->scroll++;
    } else {
        return;
    }
    if (ui->scroll < 0) {
        ui->scroll = 0;
    }
    if (ui->scroll > maximum) {
        ui->scroll = maximum;
    }
    settings_draw(ui);
}

/* --- opening and closing --------------------------------------------------- */

int settings_ui_open(SettingsUi *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->focus_row = UI_FOCUS_NONE;
    ui->hover_row = -1;
    ui->hover_category = -1;
    ui->app_index = 0;

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "gnuchansettings: cannot open the X display\n");
        return -1;
    }
    ui->screen = DefaultScreen(ui->display);
    ui->root = RootWindow(ui->display, ui->screen);
    ui->visual = DefaultVisual(ui->display, ui->screen);
    ui->colormap = DefaultColormap(ui->display, ui->screen);
    ui->depth = DefaultDepth(ui->display, ui->screen);

    if (settings_style_load(&ui->style, ui->display, ui->screen) != 0) {
        fprintf(stderr, "gnuchansettings: cannot load the fonts\n");
        XCloseDisplay(ui->display);
        ui->display = NULL;
        return -1;
    }

    ui->apps = settings_apps(&ui->app_count);
    ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.background_pixel = ui->style.background;
    attributes.border_pixel = ui->style.panel_edge;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            PointerMotionMask | StructureNotifyMask;
    ui->window = XCreateWindow(ui->display, ui->root, 0, 0,
                               UI_WINDOW_WIDTH, UI_WINDOW_HEIGHT, 0,
                               ui->depth, InputOutput, ui->visual,
                               CWBackPixel | CWBorderPixel | CWEventMask,
                               &attributes);
    if (ui->window == None) {
        fprintf(stderr, "gnuchansettings: cannot make the window\n");
        settings_style_free(&ui->style, ui->display);
        XCloseDisplay(ui->display);
        ui->display = NULL;
        return -1;
    }
    XStoreName(ui->display, ui->window, "GnuChanSettings");

    /* The window's class, which is how the dock knows what this program is.
       It reads WM_CLASS to tell one program's windows from another's and to
       gather them — see dock_items.c — and a window that set none is a window
       with no name to gather by. The instance is the name a single window is
       started under and the class the program's own name, exactly as the
       terminal sets them, so the dock can collect every GnuChanSettings window
       into its one settings slot instead of drawing each as another icon. */
    {
        XClassHint hint;
        hint.res_name = (char *)"gcl_settings";
        hint.res_class = (char *)"GnuChanSettings";
        XSetClassHint(ui->display, ui->window, &hint);
    }

    /* The whole panel is painted into this pixmap and copied to the window in
       ONE XCopyArea, rather than drawn straight onto the window a shape at a
       time. That is what keeps the redraw from being seen: a window painted
       piece by piece shows each piece as it lands — the flicker — while a
       window shown a finished picture never does. */
    ui->buffer = XCreatePixmap(ui->display, ui->window, UI_WINDOW_WIDTH,
                               UI_WINDOW_HEIGHT, (unsigned int)ui->depth);
    if (ui->buffer == None) {
        fprintf(stderr, "gnuchansettings: cannot make the back buffer\n");
        XDestroyWindow(ui->display, ui->window);
        XFreeGC(ui->display, ui->gc);
        settings_style_free(&ui->style, ui->display);
        XCloseDisplay(ui->display);
        ui->display = NULL;
        return -1;
    }
    ui->target = ui->buffer;
    ui->draw = XftDrawCreate(ui->display, ui->buffer, ui->visual,
                             ui->colormap);

    settings_ui_load_page(ui);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);
    return 0;
}

void settings_ui_run(SettingsUi *ui) {
    ui->running = 1;
    while (ui->running) {
        XEvent event;
        XNextEvent(ui->display, &event);
        int redraw = 0;

        switch (event.type) {
        case Expose:
            if (event.xexpose.count == 0) {
                redraw = 1;
            }
            break;
        case KeyPress:
            handle_key(ui, &event.xkey);
            redraw = 1;
            break;
        case ButtonPress:
            handle_wheel(ui, &event.xbutton);
            if (event.xbutton.button == Button1) {
                handle_click(ui, &event.xbutton);
            }
            redraw = 1;
            break;
        case MotionNotify:
            handle_motion(ui, &event.xmotion);
            break;
        case ConfigureNotify:
            redraw = 1;
            break;
        case DestroyNotify:
            ui->running = 0;
            break;
        default:
            break;
        }

        if (redraw) {
            settings_draw(ui);
        }
    }
}

void settings_ui_close(SettingsUi *ui) {
    if (!ui->display) {
        return;
    }
    if (ui->draw) {
        XftDrawDestroy(ui->draw);
        ui->draw = NULL;
    }
    if (ui->buffer != None) {
        XFreePixmap(ui->display, ui->buffer);
        ui->buffer = None;
    }
    if (ui->window != None) {
        XDestroyWindow(ui->display, ui->window);
        ui->window = None;
    }
    if (ui->gc) {
        XFreeGC(ui->display, ui->gc);
        ui->gc = NULL;
    }
    settings_style_free(&ui->style, ui->display);
    XCloseDisplay(ui->display);
    ui->display = NULL;
}
