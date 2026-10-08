/*
 * dock_menu.c — the list a category opens, its own small window.
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#include "dock_core.h"
#include "dock_items.h"
#include "dock_menu.h"
#include "dock_shape.h"

/* The gap between the dock's top edge and the list's bottom edge. */
#define DOCK_MENU_GAP 6
/* The room inside the list, left and right of a row's text, and above the first
   and below the last. */
#define DOCK_MENU_PADDING 10
/* A menu row is the window's title with a marker appended — "title (not
   focus)" — so it is the title's own length plus room for the marker. Sizing a
   row buffer to DOCK_TEXT_LENGTH alone would let a full-length title be cut,
   which is the truncation the compiler warns about. */
#define DOCK_MENU_ROW_LENGTH (DOCK_TEXT_LENGTH + 16)

static int row_height(const DockCore *core) {
    if (core->font) {
        return core->font->ascent + core->font->descent + 12;
    }
    return 24;
}

/* The width the longest title needs, so the window is as wide as the list is
   and no wider, up to a ceiling beyond which a title is trimmed instead. */
static int menu_width(const DockCore *core, const char *const *titles,
                      int count) {
    int widest = 0;
    for (int i = 0; i < count; i++) {
        if (!core->font) {
            continue;
        }
        XGlyphInfo extent;
        XftTextExtentsUtf8(core->display, core->font,
                           (const FcChar8 *)titles[i],
                           (int)strlen(titles[i]), &extent);
        if ((int)extent.xOff > widest) {
            widest = (int)extent.xOff;
        }
    }
    int width = widest + DOCK_MENU_PADDING * 2;
    int screen_width = DisplayWidth(core->display, core->screen);
    if (width > screen_width / 2) {
        width = screen_width / 2;
    }
    if (width < core->config.icon_size + DOCK_MENU_PADDING * 2) {
        width = core->config.icon_size + DOCK_MENU_PADDING * 2;
    }
    return width;
}

/* Trim a title to a width. Bytes are kept whole so a cut never splits a UTF-8
   character. */
static void fit_title(const DockCore *core, const char *title, int max_width,
                      char *out, unsigned int size) {
    /* The caller may hand the same buffer in and out (it passes a row's own
       title back into itself), so the working copy is local and nothing is
       written to `out` until it is final. Both copies are the full row's size,
       because the string handed in is a whole row and not just a title. */
    char working[DOCK_MENU_ROW_LENGTH];
    snprintf(working, sizeof(working), "%s", title);
    if (!core->font || max_width <= 0) {
        snprintf(out, size, "%s", working);
        return;
    }
    XGlyphInfo extent;
    XftTextExtentsUtf8(core->display, core->font, (const FcChar8 *)working,
                       (int)strlen(working), &extent);
    if ((int)extent.xOff <= max_width) {
        snprintf(out, size, "%s", working);
        return;
    }
    char trial[DOCK_MENU_ROW_LENGTH];
    int length = (int)strlen(working);
    for (int cut = length; cut > 0; cut--) {
        if (((unsigned char)working[cut] & 0xc0) == 0x80) {
            continue;
        }
        snprintf(trial, sizeof(trial), "%.*s...", cut, working);
        XftTextExtentsUtf8(core->display, core->font, (const FcChar8 *)trial,
                           (int)strlen(trial), &extent);
        if ((int)extent.xOff <= max_width) {
            snprintf(out, size, "%s", trial);
            return;
        }
    }
    out[0] = '\0';
}

static void menu_paint(DockMenu *menu) {
    DockCore *core = menu->core;
    Display *display = core->display;
    GC gc = core->gc;

    XSetForeground(display, gc, core->background);
    dock_shape_fill(display, menu->window, gc, 0, 0, menu->width,
                    menu->height, core->config.corner);
    XSetForeground(display, gc, core->edge);
    dock_shape_outline(display, menu->window, gc, 0, 0, menu->width - 1,
                       menu->height - 1, core->config.corner, 2);

    XftColor text_colour;
    XftColor accent_colour;
    int have_text = XftColorAllocName(display, core->visual, core->colormap,
                                      core->config.text, &text_colour);
    int have_accent = XftColorAllocName(display, core->visual, core->colormap,
                                        core->config.accent, &accent_colour);

    XftDraw *draw = XftDrawCreate(display, menu->window, core->visual,
                                  core->colormap);

    int height = row_height(core);
    for (int i = 0; i < menu->item_count; i++) {
        int y = DOCK_MENU_PADDING + i * height;

        if (i == menu->hover_index) {
            XSetForeground(display, gc, core->field);
            dock_shape_fill(display, menu->window, gc, 2, y, menu->width - 4,
                            height, core->config.corner / 2);
        }

        char title[DOCK_TEXT_LENGTH];
        dock_window_title(core, menu->items[i], title, sizeof(title));
        if (!title[0]) {
            snprintf(title, sizeof(title), "window");
        }

        /* Whether this row is the window with the focus, read from the same
           _NET_ACTIVE_WINDOW the dock was built from. The marker is part of
           the text rather than a separate glyph so the row is one string to
           trim and one to draw — and the two rows a person compares are then
           "path (focus)" against "path (not focus)", which says the answer in
           words. */
        /* The row is the title with a marker appended, so it needs the title's
           full length PLUS the marker. Sizing it to DOCK_TEXT_LENGTH alone made
           the compiler warn that a full-length title could be cut: the row is
           trimmed to the width below anyway, but the buffer still has to hold
           the untrimmed string first. */
        int focused = (menu->items[i] == core->active_window);
        char row[DOCK_MENU_ROW_LENGTH];
        snprintf(row, sizeof(row), "%s (%s)", title,
                 focused ? "focus" : "not focus");

        int max_title = menu->width - DOCK_MENU_PADDING * 2;
        fit_title(core, row, max_title, row, sizeof(row));

        int baseline = y + (height +
                            (core->font ? core->font->ascent -
                                          core->font->descent : 0)) / 2;
        /* The focused row is drawn in the accent whenever it is not hovered,
           so the row the user is in is the one that stands out without the
           pointer having to find it. The hover still wins where the two meet:
           a hovered row is the one about to be clicked. */
        XftColor *colour;
        if (i == menu->hover_index && have_accent) {
            colour = &accent_colour;
        } else if (focused && have_accent) {
            colour = &accent_colour;
        } else {
            colour = have_text ? &text_colour : NULL;
        }
        if (draw && colour && core->font && row[0]) {
            XftDrawStringUtf8(draw, colour, core->font, DOCK_MENU_PADDING,
                              baseline, (const FcChar8 *)row,
                              (int)strlen(row));
        }
    }

    if (draw) {
        XftDrawDestroy(draw);
    }
    if (have_text) {
        XftColorFree(display, core->visual, core->colormap, &text_colour);
    }
    if (have_accent) {
        XftColorFree(display, core->visual, core->colormap, &accent_colour);
    }
    XFlush(display);
}

void dock_menu_close(DockMenu *menu) {
    if (!menu || menu->window == None) {
        return;
    }
    if (menu->core && menu->core->display) {
        XUnmapWindow(menu->core->display, menu->window);
        XDestroyWindow(menu->core->display, menu->window);
        XFlush(menu->core->display);
    }
    menu->window = None;
    menu->item_count = 0;
    menu->hover_index = -1;
    menu->owner_index = -1;
}

/* Whether the list is open and it was the slot at `index` that opened it. The
   dock asks this before it opens a category: the same slot clicked a second
   time is the list's own close, not a fresh opening. */
int dock_menu_owned_by(const DockMenu *menu, int index) {
    return menu && menu->window != None && menu->owner_index == index;
}

int dock_menu_is_open(const DockMenu *menu) {
    return menu && menu->window != None;
}

void dock_menu_open(DockMenu *menu, DockCore *core, DockItem *item,
                    int owner_index, int slot_x) {
    if (!menu || !core || !item || item->window_count == 0) {
        return;
    }
    dock_menu_close(menu);
    menu->core = core;
    menu->owner_index = owner_index;

    int count = item->window_count;
    if (count > DOCK_MAX_ITEMS) {
        count = DOCK_MAX_ITEMS;
    }

    /* The titles, so the width can be the widest of them. */
    char titles[DOCK_MAX_ITEMS][DOCK_TEXT_LENGTH];
    const char *pointers[DOCK_MAX_ITEMS];
    for (int i = 0; i < count; i++) {
        dock_window_title(core, item->windows[i], titles[i],
                          sizeof(titles[i]));
        if (!titles[i][0]) {
            snprintf(titles[i], sizeof(titles[i]), "window");
        }
        pointers[i] = titles[i];
    }

    int width = menu_width(core, pointers, count);
    int height = DOCK_MENU_PADDING * 2 + count * row_height(core);

    int screen_width = DisplayWidth(core->display, core->screen);
    int x = slot_x;
    if (x + width > screen_width) {
        x = screen_width - width;
    }
    if (x < 0) {
        x = 0;
    }
    int y = core->y - height - DOCK_MENU_GAP;
    if (y < 0) {
        y = 0;
    }

    for (int i = 0; i < count; i++) {
        menu->items[i] = item->windows[i];
    }
    menu->item_count = count;
    menu->hover_index = -1;
    menu->width = width;
    menu->height = height;
    menu->x = x;
    menu->y = y;

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->background;
    attributes.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                            PointerMotionMask | LeaveWindowMask | KeyPressMask;

    menu->window = XCreateWindow(core->display, core->root, x, y,
                                 (unsigned int)width, (unsigned int)height, 0,
                                 core->depth, InputOutput, core->visual,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask,
                                 &attributes);
    if (menu->window == None) {
        menu->item_count = 0;
        return;
    }

    /* The list's own square corners are cut off the same way the dock's are:
       the drawn body is round, and the window behind it must be too, or the
       corners would show as sharp triangles around the round body. */
    dock_shape_round(core->display, menu->window, width, height,
                     core->config.corner);

    /* Above the dock, whatever the dock's own stacking is, and keen to take
       the keyboard so Escape reaches it. The list is transient: it deals with
       the key itself and the dock keeps the pointer's ordinary handling. */
    XRaiseWindow(core->display, menu->window);
    XMapWindow(core->display, menu->window);
    XSetInputFocus(core->display, menu->window, RevertToPointerRoot,
                   CurrentTime);
    XFlush(core->display);
    menu_paint(menu);
}

/* Which row a y in the menu's own coordinates falls on, or -1. */
static int row_at(const DockMenu *menu, int y) {
    int height = row_height(menu->core);
    if (height <= 0) {
        return -1;
    }
    int index = (y - DOCK_MENU_PADDING) / height;
    if (index < 0 || index >= menu->item_count) {
        return -1;
    }
    /* The padding band under the last row belongs to no row. */
    int row_top = DOCK_MENU_PADDING + index * height;
    if (y >= row_top + height) {
        return -1;
    }
    return index;
}

int dock_menu_event(DockMenu *menu, XEvent *event) {
    if (!menu || !menu->core || menu->window == None) {
        return 0;
    }
    if (event->xany.window != menu->window) {
        return 0;
    }

    switch (event->type) {
    case Expose:
        if (event->xexpose.count == 0) {
            menu_paint(menu);
        }
        return 1;
    case MotionNotify: {
        int index = row_at(menu, event->xmotion.y);
        if (index != menu->hover_index) {
            menu->hover_index = index;
            menu_paint(menu);
        }
        return 1;
    }
    case LeaveNotify:
        if (menu->hover_index != -1) {
            menu->hover_index = -1;
            menu_paint(menu);
        }
        return 1;
    case ButtonPress:
        if (event->xbutton.button == Button1) {
            int index = row_at(menu, event->xbutton.y);
            if (index >= 0) {
                dock_core_activate(menu->core, menu->items[index]);
            }
        }
        /* Any click on the list puts it away, whether it chose a row or not:
           the list is a thing that is open while a question is being asked, and
           a click is an answer to it. */
        dock_menu_close(menu);
        return 1;
    case KeyPress:
        /* Escape puts the list away, the way it dismisses a menu everywhere
           else. The list took the keyboard when it opened, so this key comes
           here and not to the window underneath. */
        if (event->xkey.keycode ==
            XKeysymToKeycode(menu->core->display, XK_Escape)) {
            dock_menu_close(menu);
            return 1;
        }
        return 1;
    default:
        return 1;
    }
}
