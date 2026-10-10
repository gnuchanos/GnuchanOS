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

/* The same two names for the settings panel. An open GnuChanSettings is not a
   program of its own on the dock: the gear is already there as a fixed slot,
   so the panel's window belongs in that slot and not as a second gear beside
   it. This is what the terminal slot does for terminals, applied to the panel
   the gear opens. */
#define DOCK_SETTINGS_CLASS "GnuChanSettings"
#define DOCK_SETTINGS_INSTANCE "gcl_settings"

/* How many programs' icons the dock remembers at once. One per class seen, so
   it is the number of distinct programs a session runs rather than the number
   of windows it has open. */
#define DOCK_ICON_CACHE_MAX 64

/* Icons remembered by the class they were loaded for.
 *
 * dock_items_build() runs whenever anything the dock draws changes — a window
 * opening or closing above all — and it used to free every icon it held and
 * ask the server for all of them again: an XGetWindowProperty per window, the
 * ARGB list decoded and scaled per window, for windows that had not changed at
 * all. One window closing made every other window's icon be rebuilt from
 * scratch, which is the CPU wave this cache removes.
 *
 * An icon belongs to a PROGRAM and not to a window — the dock groups by
 * WM_CLASS on that same reasoning — so what was loaded for a class is kept
 * here and handed back the next time the class is seen. Only a class never
 * seen before costs a load. A window that published no class gets a key of its
 * own, its id, so two anonymous windows do not end up wearing each other's
 * icon.
 *
 * The cache is emptied when the icon size or the background behind the icons
 * may have changed — a restyle — since an icon is scaled and composited for
 * the dock it is in; see dock_items_free_icons(). */
typedef struct DockIconCacheEntry {
    char key[DOCK_TEXT_LENGTH];
    DockIcon icon;
} DockIconCacheEntry;

static DockIconCacheEntry dock_icon_cache[DOCK_ICON_CACHE_MAX];
static int dock_icon_cache_count = 0;

/* Release every icon in the cache. Called when the dock restyles — the size an
   icon is scaled to and the background it is composited over are the dock's and
   can change there — and at shutdown. */
void dock_items_free_icons(DockCore *core);

static DockIconCacheEntry *icon_cache_find(const char *key) {
    for (int i = 0; i < dock_icon_cache_count; i++) {
        if (strcmp(dock_icon_cache[i].key, key) == 0) {
            return &dock_icon_cache[i];
        }
    }
    return NULL;
}

/* The icon for a window's class, borrowed from the cache and loaded only the
   first time that class is seen. The returned icon belongs to the cache and
   must not be freed by the caller — see the comment on the cache above. */
static const DockIcon *icon_for_window(DockCore *core, Window client,
                                       const char *key) {
    DockIconCacheEntry *entry = icon_cache_find(key);
    if (entry) {
        return &entry->icon;
    }

    /* The table is bounded and small: a session with more than this many
       distinct programs drops what it remembers and starts again, which is a
       load for icons still on screen but a load that cannot happen twice in a
       row. */
    if (dock_icon_cache_count >= DOCK_ICON_CACHE_MAX) {
        dock_items_free_icons(core);
    }

    entry = &dock_icon_cache[dock_icon_cache_count];
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->icon.pixmap = None;
    entry->icon.side = 0;
    entry->icon.ok = 0;
    dock_icon_load_window(core->display, core->root, core->visual, core->depth,
                          &entry->icon, client, core->config.icon_size,
                          core->background);
    dock_icon_cache_count++;
    return &entry->icon;
}

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

static int is_settings_window(const char *wm_class, const char *instance) {
    return (wm_class[0] && strcasecmp(wm_class, DOCK_SETTINGS_CLASS) == 0) ||
           (instance[0] &&
            strcasecmp(instance, DOCK_SETTINGS_INSTANCE) == 0);
}

/* Which workspace a window is on, read from its _NET_WM_DESKTOP. Returns -1
   when the window published none — a window the manager has not tagged, or a
   session with no EWMH manager — and the caller then shows it rather than
   hiding it: a window that cannot be placed is a window the user would be
   unable to find if the dock dropped it. */
static long window_desktop(DockCore *core, Window client) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    long desktop = -1;

    if (XGetWindowProperty(core->display, client, core->net_wm_desktop, 0, 1,
                           False, XA_CARDINAL, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_format == 32 && items > 0) {
            desktop = (long)(*(unsigned long *)data);
        }
        if (data) {
            XFree(data);
        }
    }
    return desktop;
}

/* Whether a window belongs on the dock right now: it is on the workspace that
   is showing. With no current desktop published by the manager (have_desktop
   is 0) every window belongs, which is the safe answer for a plain X session —
   the alternative hides windows on a screen that has told the dock nothing. */
static int window_on_current_desktop(DockCore *core, Window client) {
    if (!core->have_desktop) {
        return 1;
    }
    long desktop = window_desktop(core, client);
    if (desktop < 0) {
        return 1;
    }
    return desktop == core->current_desktop;
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
           window of a program is that window and not the program. Its icon
           comes from the cache, keyed on the class: a program's icon is the
           same for every window of it, so the second window of a program that
           was closed and reopened does not pay for a second decode. A window
           that published no class is keyed by id, so two anonymous windows do
           not share one icon. */
        found = item_add(core, DOCK_ITEM_WINDOW, title, NULL);
        if (!found) {
            return;
        }
        snprintf(found->wm_class, sizeof(found->wm_class), "%s", wm_class);
        char key[DOCK_TEXT_LENGTH];
        if (wm_class[0]) {
            snprintf(key, sizeof(key), "class:%s", wm_class);
        } else {
            snprintf(key, sizeof(key), "win:%lu", (unsigned long)client);
        }
        /* A COPY of the cache's icon, not a second load: the pixmap inside is
           shared and owned by the cache. dock_items_clear() therefore does not
           free it. */
        found->icon = *icon_for_window(core, client, key);
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
    /* The slot with the focused window in it is the one the user is in, and it
       is marked so the dock can draw it apart from the rest. */
    if (client == core->active_window) {
        found->has_focus = 1;
    }
}

static void items_add_windows(DockCore *core, DockItem *terminal,
                              DockItem *settings) {
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
        /* Windows on another workspace are not on this screen, and a dock that
           listed them would be offering to raise windows the user cannot see.
           They are left out until the workspace they are on is showing. */
        if (!window_on_current_desktop(core, client)) {
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
                if (client == core->active_window) {
                    terminal->has_focus = 1;
                }
            }
            continue;
        }

        if (is_settings_window(wm_class, instance)) {
            /* The settings panel, into the gear's own slot — the same thing
               done for terminals above. Without this the panel is an ordinary
               window and gets a slot of its own, so the dock shows a SECOND
               settings icon while the panel is open; the fixed gear is already
               there, so the panel's window belongs inside it. */
            if (settings && settings->window_count < DOCK_MAX_ITEMS) {
                settings->windows[settings->window_count++] = client;
                if (client == core->active_window) {
                    settings->has_focus = 1;
                }
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
    /* The icons are NOT freed here. A slot's icon is a copy of one the cache
       owns — see icon_for_window() — and freeing it would leave the cache
       pointing at a destroyed pixmap. The cache is emptied as a whole, by
       dock_items_free_icons(), when the dock restyles or ends. */
    core->item_count = 0;
}

void dock_items_free_icons(DockCore *core) {
    for (int i = 0; i < dock_icon_cache_count; i++) {
        dock_icon_free(core->display, &dock_icon_cache[i].icon);
        dock_icon_cache[i].key[0] = '\0';
    }
    dock_icon_cache_count = 0;
}

void dock_items_build(DockCore *core) {
    dock_items_clear(core);

    DockItem *settings = NULL;
    if (core->config.settings_enabled) {
        /* The settings slot carries its own command, exactly as the terminal
           slot does: the dock runs what the settings file names rather than a
           command hard-coded here, so a session whose PATH does not reach the
           install directory can point it at the binary by full path. */
        settings = item_add(core, DOCK_ITEM_SETTINGS,
                            core->config.settings_label,
                            core->config.settings_command);
    }

    DockItem *terminal = NULL;
    if (core->config.terminal_enabled) {
        terminal = item_add(core, DOCK_ITEM_TERMINAL,
                            core->config.terminal_label,
                            core->config.terminal_command);
    }

    if (core->config.show_running) {
        items_add_windows(core, terminal, settings);
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
       the count and lists the terminals, exactly as any other group does. The
       settings gear is deliberately NOT one: it holds at most the panel it
       opened, and a click on it is meant to open the panel or raise the one
       already open — one act, no list in between. dock_core.c's click handler
       does exactly that. */
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
