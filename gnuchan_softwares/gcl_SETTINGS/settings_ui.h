/*
 * settings_ui.h — the panel's window, its state, and its layout.
 *
 * This header is the agreement between the two halves of GnuChanSettings: the
 * code that handles what a person does (settings_ui.c) and the code that draws
 * the result (settings_draw.c). Both walk the SAME layout numbers — the
 * sidebar's width, a row's height, where the fields sit — so a click lands on
 * the field that was drawn under it. The numbers live here, once.
 *
 * The state is one struct. It holds the window and its drawing surfaces, the
 * style, the current program's values as the person has them on screen (which
 * are NOT yet written to disk — that is what Save does), which field has the
 * keyboard, and what the last save had to say.
 */
#ifndef GNUCHANSETTINGS_UI_H
#define GNUCHANSETTINGS_UI_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "settings_store.h"
#include "settings_style.h"
#include "settings_types.h"

/* --- the layout -----------------------------------------------------------
 *
 * The window is a fixed size and the panel scrolls inside it rather than the
 * window growing: a settings window that resized itself as a person changed
 * category would move the very controls they are aiming at. The numbers are
 * here because the drawing and the hit-testing must agree to the pixel. */
#define UI_WINDOW_WIDTH   840
#define UI_WINDOW_HEIGHT  580
#define UI_SIDEBAR_WIDTH  220
#define UI_HEADER_HEIGHT  86
#define UI_ROW_HEIGHT     58
#define UI_FOOTER_HEIGHT  64
#define UI_MARGIN         18
#define UI_CATEGORY_HEIGHT 44

/* The width of the field column on the right of a row's label: where the text
   box, the switch or the swatch is drawn. */
#define UI_FIELD_WIDTH   320
#define UI_SWITCH_WIDTH  64

/* The height of one name in an open dropdown. Shorter than a row, because a
   list of names is read as a list and not as a column of fields. */
#define UI_CHOICE_HEIGHT 30

/* Which row holds the keyboard. -1 means none, which is the state the panel
   opens in — nothing is focused until a person clicks a field. */
#define UI_FOCUS_NONE (-1)

/* One program's settings as the window holds them: the values on screen, which
   are written to the file only when Save is pressed. */
typedef struct UiPage {
    const AppDef     *app;
    SettingValue      values[SETTINGS_MAX_ROWS];
    int               loaded;      /* the file was read (0 = defaults shown) */
} UiPage;

typedef struct SettingsUi {
    Display *display;
    int      screen;
    Window   root;
    Window   window;
    GC       gc;
    Visual  *visual;
    Colormap colormap;
    int      depth;
    XftDraw *draw;                 /* the BACK BUFFER as an Xft surface */
    Pixmap   buffer;               /* the whole panel is painted here    */
    Drawable target;               /* where a paint goes: the back buffer */

    SettingsStyle style;

    const AppDef *apps;
    int           app_count;
    int           app_index;       /* which category is open            */
    UiPage        page;

    int           focus_row;       /* the row holding the keyboard, -1 = none */
    int           caret;           /* the caret's byte offset in that field   */
    int           scroll;          /* the first row shown, in rows            */

    int           hover_row;       /* the row under the pointer, -1 = none    */
    int           hover_category;  /* a category under the pointer, -1 = none */
    int           dropdown_row;    /* the choice row whose list is open, -1   */

    /* What the last save said, shown in the footer. `status_failed` colours it
       — a save that failed must not read like one that worked. */
    char status[SETTINGS_TEXT_LENGTH];
    int  status_failed;

    int running;
} SettingsUi;

/* Open the window: the display, the style, the first category's values. Returns
 * 0 on success, -1 when there is no display or the window cannot be made. */
int settings_ui_open(SettingsUi *ui);

/* The event loop, until the window is closed. */
void settings_ui_run(SettingsUi *ui);

/* Close the display and free everything settings_ui_open() made. */
void settings_ui_close(SettingsUi *ui);

/* Load the open category's values from its file into ui->page. Called when the
 * panel opens and when a person switches category. */
void settings_ui_load_page(SettingsUi *ui);

/* --- the geometry, shared by the drawing and the hit-testing ---------------
 *
 * Every one of these answers a question both halves ask, so they are computed
 * in one place. They are pure: no X, no state changed. */

/* The y of a category's row in the sidebar, and the category a y falls in (or
   -1). */
int settings_ui_category_y(int index);
int settings_ui_category_at(int y);

/* The rectangle of a setting row's field on screen, counting the scroll. */
void settings_ui_row_field_rect(const SettingsUi *ui, int row,
                                int *x, int *y, int *width, int *height);

/* The rectangle of one name in an open dropdown — the list hangs under the
   field of `row` — and which name a point falls in, or -1. */
void settings_ui_choice_rect(const SettingsUi *ui, int row, int index,
                             int *x, int *y, int *width, int *height);
int settings_ui_choice_at(const SettingsUi *ui, int x, int y);

/* The row a point falls in, counting the scroll; -1 when it is above the first
   shown row or in the footer. */
int settings_ui_row_at(const SettingsUi *ui, int x, int y);

/* How many rows fit in the panel's body at the current size. */
int settings_ui_visible_rows(void);

/* The rectangle of the Save button. */
void settings_ui_save_rect(const SettingsUi *ui, int *x, int *y,
                           int *width, int *height);

/* The rectangle of the Revert button. */
void settings_ui_revert_rect(const SettingsUi *ui, int *x, int *y,
                             int *width, int *height);

#endif /* GNUCHANSETTINGS_UI_H */
