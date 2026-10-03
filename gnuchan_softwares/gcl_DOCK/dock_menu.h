/*
 * dock_menu.h — the little window a category opens.
 *
 * A slot that stands for more than one window — a group of a program's windows,
 * or the terminal with its terminals — is a question until it is clicked, and
 * this is the answer: a short list above the dock, one row per window the slot
 * holds, named the way the window names itself. Clicking a row brings that
 * window to the front; clicking the slot again puts the list away.
 *
 * It is a window of its own rather than something drawn onto the dock, because
 * a dock is as tall as its icons and the list is taller than that: drawn inside
 * the dock it would be clipped to the bar. So it is a second override-redirect
 * window, mapped above the dock and unmapped when it closes, and the dock's own
 * loop hands it any event that lands on it.
 *
 * Nothing here is cached. The list is built each time the slot is opened, from
 * the windows the slot holds at that moment, because the whole point of the
 * list is to say what is open NOW, and a list kept from the last opening would
 * be answering a question that has changed.
 */
#ifndef GNUCHANDOCK_MENU_H
#define GNUCHANDOCK_MENU_H

#include <X11/Xlib.h>

#include "dock_config.h"

typedef struct DockCore DockCore;
typedef struct DockItem DockItem;

/* The most rows the list will draw before it stops. The ceiling is the dock's
   own ceiling on slots: a group cannot hold more windows than the dock holds
   items. */
typedef struct DockMenu {
    DockCore *core;

    Window window;      /* None while the list is put away                */
    int width;
    int height;
    int x;
    int y;

    /* Which slot opened the list, counted the way the dock counts its items.
       The same slot clicked again is the list's own close: the click that
       raises it puts it back. -1 while nothing is open. */
    int owner_index;

    Window items[DOCK_MAX_ITEMS];
    int item_count;
    int hover_index;    /* the row under the pointer, or -1               */
} DockMenu;

/* Open the list for `item`, above the dock and with its left edge at the
   screen's own `slot_x` (the dock's position plus the slot's offset within
   it, so the list lands over the icon that opened it and not at the screen's
   left edge). `owner_index` is which slot opened it, remembered so the same
   slot clicked again closes it. A list already open is replaced; a slot that
   holds nothing opens nothing. */
void dock_menu_open(DockMenu *menu, DockCore *core, DockItem *item,
                    int owner_index, int slot_x);

/* Put the list away, if it is up. Safe to call when it is not. */
void dock_menu_close(DockMenu *menu);

/* Whether the list is open and it was the slot at `index` that opened it. The
   dock asks this before it reopens a category: the same slot clicked a second
   time is the list's own close, not a fresh opening. */
int dock_menu_owned_by(const DockMenu *menu, int index);

/* Whether the list is currently up. */
int dock_menu_is_open(const DockMenu *menu);

/* Hand one event to the list. Returns 1 when the event was the list's and has
   been dealt with, 0 when the dock's own loop should take it instead. */
int dock_menu_event(DockMenu *menu, XEvent *event);

#endif /* GNUCHANDOCK_MENU_H */
