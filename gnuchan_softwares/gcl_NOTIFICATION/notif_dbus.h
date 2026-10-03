/*
 * notif_dbus.h — the org.freedesktop.Notifications service.
 *
 * A notification daemon is not an X program at all, not first: it is a program
 * that holds the name `org.freedesktop.Notifications` on the session bus and
 * answers the four methods the protocol defines. Every program that shows a
 * notification — a mail client, a music player, a script with notify-send —
 * calls Notify on that name and is done; whether anyone draws anything is the
 * daemon's business.
 *
 * So this module is the daemon's ear. It takes the bus, asks to own the name,
 * and every message the loop asks it to read is either one of the four methods
 * or a nudge it can ignore. What it understands is turned into a NotifRequest —
 * the plain parts of a notification — and handed to the handler the caller set;
 * what it does not understand is answered with the protocol's own error, so a
 * caller is never left waiting.
 *
 * Whatever picture the notification carried comes as either a file path or a
 * block of ARGB pixels. Both are carried through to the handler and freed by
 * notif_request_free().
 */
#ifndef GNUCHANNOTIFICATION_DBUS_H
#define GNUCHANNOTIFICATION_DBUS_H

#include <dbus/dbus.h>

#include "notif_config.h"
#include "notif_item.h"

typedef struct NotifRequest {
    char app_name[NOTIF_TEXT_LENGTH];
    unsigned int replaces_id;
    char app_icon[NOTIF_TEXT_LENGTH];
    char summary[NOTIF_TEXT_LENGTH];
    char body[NOTIF_MAX_BODY];
    NotifUrgency urgency;

    /* The sender's own timeout, in the protocol's three cases: -1 (let the
       server decide), 0 (never expire), or a positive number of milliseconds. */
    int expire_timeout;

    /* A picture, if the sender carried one. `image_path` is a picture on disk,
       and the three `image_*` fields are the pixels of an `image-data` hint,
       RGBA in a flat array, width * height of them. */
    char image_path[NOTIF_TEXT_LENGTH];
    int has_image_data;
    int image_width;
    int image_height;
    long *image_cards;
    int image_card_count;
} NotifRequest;

/* A notification arrived. The handler returns the id the daemon gave the
   bubble, so Notify can answer the client with it — the protocol's own answer
   to the caller, and the number the caller must use to close it. It returns 0
   when the bubble could not be shown at all. */
typedef unsigned int (*NotifNotifyHandler)(const NotifRequest *request,
                                           void *userdata);
typedef void (*NotifCloseHandler)(unsigned int id, void *userdata);
typedef void (*NotifActionHandler)(unsigned int id, const char *key,
                                   void *userdata);

typedef struct NotifDBus {
    DBusConnection *connection;
    int owns_name;

    NotifNotifyHandler on_notify;
    NotifCloseHandler on_close;
    NotifActionHandler on_action;
    void *userdata;
} NotifDBus;

/* Connect to the session bus and ask to own `org.freedesktop.Notifications`.
   Returns 0 on success, -1 when there is no session bus, the connection could
   not be made, or another server already holds the name. */
int notif_dbus_init(NotifDBus *bus);

/* Whether the bus has granted the name. */
int notif_dbus_owns_name(NotifDBus *bus);

void notif_dbus_set_handlers(NotifDBus *bus, NotifNotifyHandler on_notify,
                             NotifCloseHandler on_close,
                             NotifActionHandler on_action, void *userdata);

/* Read and answer whatever is waiting on the bus, waiting no longer than
   `timeout_ms` for the first message (a negative waits for ever, which a
   tick-driven loop never wants). Returns the number of messages handled. */
int notif_dbus_dispatch(NotifDBus *bus, int timeout_ms);

/* Tell every client that a notification was closed and why, as the protocol
   requires: 1 expired, 2 dismissed by the user, 3 closed by a call. */
void notif_dbus_emit_closed(NotifDBus *bus, unsigned int id,
                            unsigned int reason);

/* Release the name request, the connection, and anything a request left
   allocated. Safe to call twice. */
void notif_dbus_free(NotifDBus *bus);

/* Free the pixels a request carried. */
void notif_request_free(NotifRequest *request);

#endif /* GNUCHANNOTIFICATION_DBUS_H */
