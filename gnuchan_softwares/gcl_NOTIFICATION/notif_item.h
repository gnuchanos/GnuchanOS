/*
 * notif_item.h — one live notification, and the stack of them.
 *
 * What arrives over D-Bus is a notification's parts: who sent it, a summary
 * line, a body, an urgency, a timeout, and whatever hints came with it. This is
 * where those parts are held once they are read, and where the stack of the
 * ones currently on screen lives.
 *
 * A notification is not a window. One X window holds the whole stack — the
 * bubbles are drawn into it one above another — because a stack of five
 * separate windows is five windows to place, raise and stack in the right
 * order, and every one of them an override-redirect the server will not manage.
 * Drawing the stack into one window makes the order the drawing order and the
 * expiry a single repaint.
 *
 * An item owns its picture, so every function that removes an item takes the
 * display the picture lives on: a pixmap freed on the wrong connection leaks,
 * and one freed twice is a fatal error.
 */
#ifndef GNUCHANNOTIFICATION_ITEM_H
#define GNUCHANNOTIFICATION_ITEM_H

#include <X11/Xlib.h>

#include "notif_config.h"
#include "notif_image.h"

typedef enum NotifUrgency {
    NOTIF_URGENCY_LOW = 0,
    NOTIF_URGENCY_NORMAL,
    NOTIF_URGENCY_CRITICAL,
} NotifUrgency;

typedef struct NotifItem {
    unsigned int id;
    char app_name[NOTIF_TEXT_LENGTH];
    char summary[NOTIF_TEXT_LENGTH];
    char body[NOTIF_MAX_BODY];
    NotifUrgency urgency;

    /* How long this one stays, in milliseconds. The client may ask for its own
       in the -1 / 0 / positive protocol: -1 means the server decides, 0 means
       never expire, a positive number is the client's own milliseconds.
       `never_expire` is the 0 case kept apart from a very large number, because
       "this stays until it is dismissed" is a different thing from "this stays
       for a day". */
    int timeout_ms;
    int never_expire;

    long long born_ms;

    NotifImage image;
    int has_image;

    /* Where the item's bubble is drawn and how tall it turned out, filled in by
       the renderer when it lays the stack out, so the hit test and the drawing
       agree on the same rectangle. */
    int x, y, width, height;
} NotifItem;

typedef struct NotifStack {
    NotifItem items[NOTIF_MAX_VISIBLE];
    int count;
    unsigned int next_id;
} NotifStack;

void notif_stack_init(NotifStack *stack);

/* Add a notification and return its new item, or NULL when the stack is full
   and every item on it is critical. Room is made by dropping the oldest
   non-critical bubble, whose picture is freed on `display`. */
NotifItem *notif_stack_add(NotifStack *stack, Display *display,
                           long long now_ms);

/* The item with this id for this app, or NULL. */
NotifItem *notif_stack_find(NotifStack *stack, const char *app_name,
                            unsigned int id);

/* Drop the item at `index`, freeing its picture on `display` and closing the
   gap. */
void notif_stack_drop(NotifStack *stack, Display *display, int index);

/* Drop every item whose time is up, freeing their pictures. Returns how many
   were dropped, so the caller knows whether it has to repaint. */
int notif_stack_expire(NotifStack *stack, Display *display, long long now_ms);

/* Which item a point in its own coordinates falls on, or -1. */
int notif_stack_index_at(const NotifStack *stack, int x, int y);

/* Free every item's picture and empty the stack. */
void notif_stack_clear(NotifStack *stack, Display *display);

#endif /* GNUCHANNOTIFICATION_ITEM_H */
