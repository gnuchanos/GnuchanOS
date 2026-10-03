/*
 * dock_items.h — the slots the dock holds, in the order it holds them.
 *
 * A slot is one of four things, and the four are what the two fixed icons plus
 * the open windows come to once the windows are gathered by what they are:
 *
 *   SETTINGS  the gear, always first, and it does nothing yet.
 *   TERMINAL  the logo, always second. It runs the terminal, and it also holds
 *             every open GnuChanTerm, so a row of terminals is one slot.
 *   WINDOW    one open window nothing else shares — a single Firefox, say.
 *   GROUP     two or more open windows of the same program, gathered into one
 *             slot that wears the program's icon and a count of how many.
 *
 * The gathering is by WM_CLASS, the same name the icon lookup keys on: two
 * windows of one program are two of a kind, and a dock that showed them as two
 * identical icons would be showing the same picture twice with no way to tell
 * which is which. A singleton stays itself; the moment a second arrives the
 * slot becomes a group, and it stays a group for as long as both are open.
 *
 * A slot always carries the windows it stands for, even the singleton that
 * stands for one: that is what a click reads, so the click code says the same
 * thing whether the slot has one window or five.
 */
#ifndef GNUCHANDOCK_ITEMS_H
#define GNUCHANDOCK_ITEMS_H

#include <X11/Xlib.h>

#include "dock_config.h"
#include "dock_icon.h"

typedef struct DockCore DockCore;

typedef enum DockItemKind {
    DOCK_ITEM_SETTINGS = 0,
    DOCK_ITEM_TERMINAL,
    DOCK_ITEM_WINDOW,
    DOCK_ITEM_GROUP,
} DockItemKind;

typedef struct DockItem {
    DockItemKind kind;
    char label[DOCK_TEXT_LENGTH];
    char command[DOCK_TEXT_LENGTH];
    char wm_class[DOCK_TEXT_LENGTH];
    DockIcon icon;

    /* The windows this slot stands for. A singleton has one; a group has its
       whole membership; the terminal has every open GnuChanTerm; the gear has
       none. */
    Window windows[DOCK_MAX_ITEMS];
    int window_count;
} DockItem;

void dock_items_build(DockCore *core);
void dock_items_clear(DockCore *core);
int dock_items_index_at(const DockCore *core, int x, int y);

/* What a window is called: its _NET_WM_NAME as UTF-8, or its older WM_NAME
   when it published none. Empty when it is named by neither. The menu reads
   every row's name this way, which is the same answer the items were built
   from, so a row and its slot never disagree. */
void dock_window_title(DockCore *core, Window client, char *out,
                       unsigned int size);

/* Whether a slot is a category — two of a kind, or any open terminal — and so
   draws a count and opens the list when it is clicked. */
int dock_item_is_category(const DockItem *item);

#endif /* GNUCHANDOCK_ITEMS_H */
