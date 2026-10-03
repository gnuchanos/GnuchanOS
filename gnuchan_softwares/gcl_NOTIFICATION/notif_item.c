/*
 * notif_item.c — one live notification, and the stack of them.
 */
#include <string.h>

#include "notif_item.h"

void notif_stack_init(NotifStack *stack) {
    memset(stack, 0, sizeof(*stack));
    stack->next_id = 1;
}

/* Release an item's picture and mark it picture-less, so a second release is a
   no-op and the display is never asked to free a pixmap twice. */
static void release_image(Display *display, NotifItem *item) {
    if (item->has_image) {
        notif_image_free(display, &item->image);
        item->has_image = 0;
    }
}

/* Move every item from `from` down one, closing the gap left by an item that
   was just removed. */
static void close_gap(NotifStack *stack, int from) {
    for (int i = from; i + 1 < stack->count; i++) {
        stack->items[i] = stack->items[i + 1];
    }
    stack->count--;
}

void notif_stack_clear(NotifStack *stack, Display *display) {
    if (!stack) {
        return;
    }
    for (int i = 0; i < stack->count; i++) {
        release_image(display, &stack->items[i]);
    }
    stack->count = 0;
}

void notif_stack_drop(NotifStack *stack, Display *display, int index) {
    if (!stack || index < 0 || index >= stack->count) {
        return;
    }
    release_image(display, &stack->items[index]);
    close_gap(stack, index);
}

NotifItem *notif_stack_add(NotifStack *stack, Display *display,
                           long long now_ms) {
    if (!stack) {
        return NULL;
    }

    if (stack->count >= NOTIF_MAX_VISIBLE) {
        /* Make room by dropping the oldest bubble that is not critical: a
           critical one is the one the user must not miss. The array is in
           arrival order, so the first non-critical item is the oldest. */
        int victim = -1;
        for (int i = 0; i < stack->count; i++) {
            if (stack->items[i].urgency != NOTIF_URGENCY_CRITICAL) {
                victim = i;
                break;
            }
        }
        if (victim < 0) {
            return NULL;
        }
        release_image(display, &stack->items[victim]);
        close_gap(stack, victim);
    }

    NotifItem *item = &stack->items[stack->count];
    memset(item, 0, sizeof(*item));
    item->id = stack->next_id++;
    item->born_ms = now_ms;
    stack->count++;
    return item;
}

NotifItem *notif_stack_find(NotifStack *stack, const char *app_name,
                            unsigned int id) {
    if (!stack) {
        return NULL;
    }
    for (int i = 0; i < stack->count; i++) {
        if (stack->items[i].id == id &&
            strcmp(stack->items[i].app_name, app_name ? app_name : "") == 0) {
            return &stack->items[i];
        }
    }
    return NULL;
}

int notif_stack_expire(NotifStack *stack, Display *display, long long now_ms) {
    if (!stack) {
        return 0;
    }
    int dropped = 0;
    for (int i = 0; i < stack->count;) {
        NotifItem *item = &stack->items[i];
        if (item->never_expire ||
            now_ms - item->born_ms < (long long)item->timeout_ms) {
            i++;
            continue;
        }
        release_image(display, item);
        close_gap(stack, i);
        dropped++;
    }
    return dropped;
}

int notif_stack_index_at(const NotifStack *stack, int x, int y) {
    if (!stack) {
        return -1;
    }
    for (int i = 0; i < stack->count; i++) {
        const NotifItem *item = &stack->items[i];
        if (x >= item->x && x < item->x + item->width &&
            y >= item->y && y < item->y + item->height) {
            return i;
        }
    }
    return -1;
}
