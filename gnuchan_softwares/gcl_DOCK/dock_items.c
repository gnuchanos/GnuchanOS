/*
 * dock_items.c — build the slots from the settings and the screen.
 *
 * The fixed two come first, in their fixed places, and then the open windows
 * are gathered: every GnuchanOS terminal into the terminal slot, and everything
 * else by its class, so two of a kind become one slot with a count rather than
 * two identical icons side by side. The order the rest appear in is the order
 * the window manager lists them, which is stable and needs no sorting of its
 * own — a dock that reshuffled itself on every refresh would be a dock that
 * moved a window out from under the pointer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "dock_items.h"
#include "dock_core.h"

/* The class every GnuchanOS terminal carries, and the instance name it is
   started under. A window matching either is a terminal, however many are
   open, and all of them belong to the terminal slot. */
#define DOCK_TERMINAL_CLASS "GnuChanTerm"
#define DOCK_TERMINAL_INSTANCE "gcl_terminal"

void dock_window_title(DockCore *core, Window client, char *out,
                       unsigned int size) {
    out[0] = '\0';

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(core->display, client, core->net_wm_name, 0, 1024,
                           False, core->utf8_string, &actual_type,
                           &actual_format, &items, &after, &data) == Success) {
        if (data && actual_format == 8 && items > 0) {
            size_t copy = items < size - 1 ? items : size - 1;
            memcpy(out, data, copy);
            out[copy] = '\0';
        }
        if (data) {
            XFree(data);
        }
    }
    if (out[0]) {
        return;
    }

    char *legacy = NULL;
    if (XFetchName(core->display, client, &legacy) && legacy) {
        snprintf(out, size, "%s", legacy);
        XFree(legacy);
    }
}

/* A window's class and instance — the two halves of WM_CLASS. Either may come
   back empty, which is what a window that set no hint looks like. */
static void window_class(DockCore *core, Window client, char *class_out,
                         unsigned int class_size, char *instance_out,
                         unsigned int instance_size) {
    class_out[0] = '\0';
    instance_out[0] = '\0';

    XClassHint hint;
    memset(&hint, 0, sizeof(hint));
    if (!XGetClassHint(core->display, client, &hint)) {
        return;
    }
    if (hint.res_class) {
        snprintf(class_out, class_size, "%s", hint.res_class);
        XFree(hint.res_class);
    }
    if (hint.res_name) {
        snprintf(instance_out, instance_size, "%s", hint.res_name);
        XFree(hint.res_name);
    }
}

static int is_terminal_window(const char *wm_class, const char *instance) {
    return (wm_class[0] && strcasecmp(wm_class, DOCK_TERMINAL_CLASS) == 0) ||
           (instance[0] &&
            strcasecmp(instance, DOCK_TERMINAL_INSTANCE) == 0);
}

static int window_is_skippable(DockCore *core, Window client) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(core->display, client, core->net_wm_window_type, 0,
                           32, False, XA_ATOM, &actual_type, &actual_format,
                           &items, &after, &data) != Success) {
        return 0;
    }
    int skip = 0;
    if (data && actual_format == 32) {
        Atom *atoms = (Atom *)data;
        for (unsigned long i = 0; i < items; i++) {
            if (atoms[i] == core->net_wm_window_type_dock ||
                atoms[i] == core->net_wm_window_type_desktop) {
                skip = 1;
                break;
            }
        }
    }
    if (data) {
        XFree(data);
    }
    return skip;
}

static DockItem *item_add(DockCore *core, DockItemKind kind, const char *label,
                          const char *command) {
    if (core->item_count >= DOCK_MAX_ITEMS) {
        return NULL;
    }
    DockItem *item = &core->items[core->item_count];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    if (label) {
        snprintf(item->label, sizeof(item->label), "%s", label);
    }
    if (command) {
        snprintf(item->command, sizeof(item->command), "%s", command);
    }
    core->item_count++;
    return item;
}

/* The slot for a window that is not a terminal: the one already standing for
   its class, or a new one. The moment a second window lands in a singleton the
   slot becomes a group — that is the whole of "two of a kind are a category". */
static void group_window(DockCore *core, Window client, const char *wm_class,
                         const char *title) {
    DockItem *found = NULL;
    for (int i = 0; i < core->item_count; i++) {
        DockItem *item = &core->items[i];
        if ((item->kind == DOCK_ITEM_WINDOW || item->kind == DOCK_ITEM_GROUP) &&
            wm_class[0] && strcasecmp(item->wm_class, wm_class) == 0) {
            found = item;
            break;
        }
    }

    if (!found) {
        /* A new singleton. Its label is the window's own title, because one
           window of a program is that window and not the program. */
        found = item_add(core, DOCK_ITEM_WINDOW, title, NULL);
        if (!found) {
            return;
        }
        snprintf(found->wm_class, sizeof(found->wm_class), "%s", wm_class);
        dock_icon_load_window(core->display, core->root, core->visual,
                              core->depth, &found->icon, client,
                              core->config.icon_size, core->background);
    } else if (found->window_count >= 1) {
        /* A second window of the same program: it is now a category. The label
           becomes the program's own name, which is what the group is. */
        found->kind = DOCK_ITEM_GROUP;
        if (wm_class[0]) {
            snprintf(found->label, sizeof(found->label), "%s", wm_class);
        }
    }

    if (found->window_count < DOCK_MAX_ITEMS) {
        found->windows[found->window_count++] = client;
    }
}

static void items_add_windows(DockCore *core, DockItem *terminal) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(core->display, core->root, core->net_client_list, 0,
                           4096, False, XA_WINDOW, &actual_type,
                           &actual_format, &items, &after, &data) != Success) {
        return;
    }
    if (!data || actual_format != 32) {
        if (data) {
            XFree(data);
        }
        return;
    }

    Window *windows = (Window *)data;
    for (unsigned long i = 0; i < items && core->item_count < DOCK_MAX_ITEMS;
         i++) {
        Window client = windows[i];
        if (client == None || window_is_skippable(core, client)) {
            continue;
        }

        char wm_class[DOCK_TEXT_LENGTH];
        char instance[DOCK_TEXT_LENGTH];
        window_class(core, client, wm_class, sizeof(wm_class), instance,
                     sizeof(instance));

        if (is_terminal_window(wm_class, instance)) {
            /* Every terminal, into the one terminal slot. */
            if (terminal && terminal->window_count < DOCK_MAX_ITEMS) {
                terminal->windows[terminal->window_count++] = client;
            }
            continue;
        }

        char title[DOCK_TEXT_LENGTH];
        dock_window_title(core, client, title, sizeof(title));
        group_window(core, client, wm_class, title);
    }
    XFree(data);
}

void dock_items_clear(DockCore *core) {
    for (int i = 0; i < core->item_count; i++) {
        if (core->items[i].kind == DOCK_ITEM_WINDOW ||
            core->items[i].kind == DOCK_ITEM_GROUP) {
            dock_icon_free(core->display, &core->items[i].icon);
        }
    }
    core->item_count = 0;
}

void dock_items_build(DockCore *core) {
    dock_items_clear(core);

    if (core->config.settings_enabled) {
        item_add(core, DOCK_ITEM_SETTINGS, core->config.settings_label, NULL);
    }

    DockItem *terminal = NULL;
    if (core->config.terminal_enabled) {
        terminal = item_add(core, DOCK_ITEM_TERMINAL,
                            core->config.terminal_label,
                            core->config.terminal_command);
    }

    if (core->config.show_running) {
        items_add_windows(core, terminal);
    }
}

int dock_item_is_category(const DockItem *item) {
    if (!item) {
        return 0;
    }
    if (item->kind == DOCK_ITEM_GROUP) {
        return 1;
    }
    /* The terminal is a category once something is running in it: it then shows
       the count and lists the terminals, exactly as any other group does. */
    if (item->kind == DOCK_ITEM_TERMINAL) {
        return item->window_count >= 1;
    }
    return 0;
}

int dock_items_index_at(const DockCore *core, int x, int y) {
    (void)y;
    int pitch = core->config.icon_size + core->config.gap;
    if (pitch <= 0) {
        return -1;
    }
    /* The row starts half the magnify room in, exactly as dock_draw.c draws it,
       so the slot the pointer reads is the slot it sees. */
    int row_left = core->config.padding + core->config.magnify / 2;
    for (int i = 0; i < core->item_count; i++) {
        int slot_x = row_left + i * pitch;
        if (x >= slot_x && x < slot_x + core->config.icon_size) {
            return i;
        }
    }
    return -1;
}
