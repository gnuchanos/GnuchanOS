/*
 * notif_dbus.c — the org.freedesktop.Notifications service.
 *
 * The four methods the protocol defines:
 *
 *   GetCapabilities()            -> as   what this server can do
 *   GetServerInformation()       -> ssss name, vendor, version, spec
 *   Notify(...)                  -> u    show one, answer its id
 *   CloseNotification(u)         ->      drop one by id
 *
 * A call that is not one of the four is answered with an error rather than left
 * to time out, because a client waiting for a reply is a client that has
 * stopped. The parsing is the fiddly part and it is confined here: a Notify
 * carries, in order, app_name, replaces_id, app_icon, summary, body, an array
 * of action pairs, a dictionary of hints, and the sender's timeout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>

#include "notif_dbus.h"

#define NOTIF_BUS_NAME        "org.freedesktop.Notifications"
#define NOTIF_OBJECT_PATH     "/org/freedesktop/Notifications"
#define NOTIF_INTERFACE       "org.freedesktop.Notifications"
#define NOTIF_SERVER_NAME     "GnuChanNotification"
#define NOTIF_SERVER_VENDOR   "GnuchanOS"
#define NOTIF_SERVER_VERSION  "1.0"
#define NOTIF_SPEC_VERSION    "1.2"

static void copy_string(char *out, unsigned int size, const char *in) {
    if (!out || size == 0) {
        return;
    }
    snprintf(out, size, "%s", in ? in : "");
}

static void take_string(DBusMessageIter *iter, char *out, unsigned int size) {
    const char *text = NULL;
    dbus_message_iter_get_basic(iter, &text);
    copy_string(out, size, text);
    dbus_message_iter_next(iter);
}

/* `image-data` is a struct: width, height, row stride, has-alpha, bits per
   sample, channels, then the bytes. The copy taken is the pixels as ARGB cards,
   one long per pixel, because the message is freed the moment this returns. */
static int read_image_data(DBusMessageIter *variant, NotifRequest *request) {
    DBusMessageIter structure;
    if (dbus_message_iter_get_arg_type(variant) != DBUS_TYPE_STRUCT) {
        return 0;
    }
    dbus_message_iter_recurse(variant, &structure);

    int width = 0;
    int height = 0;
    int row_stride = 0;
    int bits_per_sample = 0;
    int channels = 0;
    const unsigned char *data = NULL;
    int data_length = 0;

    for (int field = 0; field < 7; field++) {
        if (dbus_message_iter_get_arg_type(&structure) == DBUS_TYPE_INVALID) {
            return 0;
        }
        if (field == 0) {
            dbus_message_iter_get_basic(&structure, &width);
        } else if (field == 1) {
            dbus_message_iter_get_basic(&structure, &height);
        } else if (field == 2) {
            dbus_message_iter_get_basic(&structure, &row_stride);
        } else if (field == 4) {
            dbus_message_iter_get_basic(&structure, &bits_per_sample);
        } else if (field == 5) {
            dbus_message_iter_get_basic(&structure, &channels);
        } else if (field == 6) {
            DBusMessageIter array;
            dbus_message_iter_recurse(&structure, &array);
            dbus_message_iter_get_fixed_array(&array, &data, &data_length);
        }
        dbus_message_iter_next(&structure);
    }

    if (width <= 0 || height <= 0 || !data || data_length <= 0) {
        return 0;
    }
    int source_channels = channels > 0 ? channels : 4;
    if (source_channels < 3) {
        return 0;
    }
    if (row_stride <= 0) {
        row_stride = width * source_channels;
    }
    if (bits_per_sample <= 0) {
        bits_per_sample = 8;
    }

    long *cards = malloc(sizeof(long) * (size_t)width * (size_t)height);
    if (!cards) {
        return 0;
    }
    int step = bits_per_sample > 8 ? 2 : 1;
    for (int y = 0; y < height; y++) {
        const unsigned char *row = data + (size_t)y * (size_t)row_stride;
        for (int x = 0; x < width; x++) {
            const unsigned char *pixel =
                row + (size_t)x * source_channels * step;
            unsigned int red = pixel[0];
            unsigned int green = source_channels > 1 ? pixel[step] : red;
            unsigned int blue = source_channels > 2 ? pixel[2 * step] : red;
            unsigned int alpha = source_channels > 3 ? pixel[3 * step] : 255;
            cards[(size_t)y * width + x] =
                ((long)alpha << 24) | ((long)red << 16) |
                ((long)green << 8) | (long)blue;
        }
    }

    request->image_cards = cards;
    request->image_card_count = width * height;
    request->image_width = width;
    request->image_height = height;
    request->has_image_data = 1;
    return 1;
}

static void read_hints(DBusMessageIter *hints, NotifRequest *request) {
    DBusMessageIter entry;
    if (dbus_message_iter_get_arg_type(hints) != DBUS_TYPE_ARRAY) {
        return;
    }
    dbus_message_iter_recurse(hints, &entry);
    while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter pair;
        dbus_message_iter_recurse(&entry, &pair);

        const char *key = NULL;
        dbus_message_iter_get_basic(&pair, &key);
        dbus_message_iter_next(&pair);

        if (key && dbus_message_iter_get_arg_type(&pair) == DBUS_TYPE_VARIANT) {
            DBusMessageIter variant;
            dbus_message_iter_recurse(&pair, &variant);
            int type = dbus_message_iter_get_arg_type(&variant);

            if (strcmp(key, "urgency") == 0 && type == DBUS_TYPE_BYTE) {
                unsigned char urgency = 1;
                dbus_message_iter_get_basic(&variant, &urgency);
                if (urgency == 0) {
                    request->urgency = NOTIF_URGENCY_LOW;
                } else if (urgency >= 2) {
                    request->urgency = NOTIF_URGENCY_CRITICAL;
                } else {
                    request->urgency = NOTIF_URGENCY_NORMAL;
                }
            } else if ((strcmp(key, "image-path") == 0 ||
                        strcmp(key, "image_path") == 0) &&
                       type == DBUS_TYPE_STRING) {
                const char *path = NULL;
                dbus_message_iter_get_basic(&variant, &path);
                copy_string(request->image_path,
                            sizeof(request->image_path), path);
            } else if ((strcmp(key, "image-data") == 0 ||
                        strcmp(key, "image_data") == 0) &&
                       type == DBUS_TYPE_STRUCT) {
                read_image_data(&variant, request);
            }
        }
        dbus_message_iter_next(&entry);
    }
}

static void send_empty_or_error(NotifDBus *bus, DBusMessage *message,
                                int known) {
    DBusMessage *reply;
    if (known) {
        reply = dbus_message_new_method_return(message);
    } else {
        reply = dbus_message_new_error(message, DBUS_ERROR_UNKNOWN_METHOD,
                                       "no such method on this interface");
    }
    if (reply) {
        dbus_connection_send(bus->connection, reply, NULL);
        dbus_message_unref(reply);
    }
}

static void send_string_list(NotifDBus *bus, DBusMessage *message,
                             const char *const *items, int count) {
    DBusMessage *reply = dbus_message_new_method_return(message);
    if (!reply) {
        return;
    }
    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    DBusMessageIter array;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &array);
    for (int i = 0; i < count; i++) {
        dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &items[i]);
    }
    dbus_message_iter_close_container(&iter, &array);
    dbus_connection_send(bus->connection, reply, NULL);
    dbus_message_unref(reply);
}

static void handle_get_capabilities(NotifDBus *bus, DBusMessage *message) {
    static const char *const capabilities[] = { "body", "actions",
                                                "body-markup" };
    send_string_list(bus, message, capabilities,
                     (int)(sizeof(capabilities) / sizeof(capabilities[0])));
}

static void handle_get_server_information(NotifDBus *bus,
                                          DBusMessage *message) {
    DBusMessage *reply = dbus_message_new_method_return(message);
    if (!reply) {
        return;
    }
    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    const char *name = NOTIF_SERVER_NAME;
    const char *vendor = NOTIF_SERVER_VENDOR;
    const char *version = NOTIF_SERVER_VERSION;
    const char *spec = NOTIF_SPEC_VERSION;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &name);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &vendor);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &version);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &spec);
    dbus_connection_send(bus->connection, reply, NULL);
    dbus_message_unref(reply);
}

static void handle_notify(NotifDBus *bus, DBusMessage *message) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter)) {
        send_empty_or_error(bus, message, 0);
        return;
    }

    NotifRequest request;
    memset(&request, 0, sizeof(request));
    request.urgency = NOTIF_URGENCY_NORMAL;
    request.expire_timeout = -1;

    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        take_string(&iter, request.app_name, sizeof(request.app_name));
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&iter, &request.replaces_id);
        dbus_message_iter_next(&iter);
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        take_string(&iter, request.app_icon, sizeof(request.app_icon));
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        take_string(&iter, request.summary, sizeof(request.summary));
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        take_string(&iter, request.body, sizeof(request.body));
    }
    /* The actions array, of (key, label) pairs, is stepped over: this server
       draws no buttons. */
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&iter);
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
        read_hints(&iter, &request);
        dbus_message_iter_next(&iter);
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
        dbus_message_iter_get_basic(&iter, &request.expire_timeout);
        dbus_message_iter_next(&iter);
    }

    /* The picture may be in the hints or in the app_icon field; the hint wins,
       because a sender that published pixels meant them. */
    if (!request.image_path[0] && request.app_icon[0]) {
        copy_string(request.image_path, sizeof(request.image_path),
                    request.app_icon);
    }

    /* The handler shows the bubble and answers with the id the daemon gave it.
       That id is what the client is told — it is the number the client must
       use to close this very notification — and a client that asked to replace
       one (replaces_id) is answered with the id of the bubble that took its
       place. Answering with the request's own replaces_id, as happens when the
       handler is not asked at all, tells every client its notification is id
       zero, and a client that later tries to close id zero closes nothing. */
    unsigned int id = 0;
    if (bus->on_notify) {
        id = bus->on_notify(&request, bus->userdata);
    }

    DBusMessage *reply = dbus_message_new_method_return(message);
    if (reply) {
        DBusMessageIter out;
        dbus_message_iter_init_append(reply, &out);
        dbus_message_iter_append_basic(&out, DBUS_TYPE_UINT32, &id);
        dbus_connection_send(bus->connection, reply, NULL);
        dbus_message_unref(reply);
    }
    notif_request_free(&request);
}

static void handle_close(NotifDBus *bus, DBusMessage *message) {
    DBusMessageIter iter;
    unsigned int id = 0;
    if (dbus_message_iter_init(message, &iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&iter, &id);
    }
    if (bus->on_close) {
        bus->on_close(id, bus->userdata);
    }
    send_empty_or_error(bus, message, 1);
}

int notif_dbus_init(NotifDBus *bus) {
    if (!bus) {
        return -1;
    }
    memset(bus, 0, sizeof(*bus));

    DBusError error;
    dbus_error_init(&error);
    bus->connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!bus->connection) {
        fprintf(stderr, "gnuchannotification: no session bus: %s\n",
                error.message ? error.message : "unknown error");
        dbus_error_free(&error);
        return -1;
    }
    dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);

    int result = dbus_bus_request_name(bus->connection, NOTIF_BUS_NAME,
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "gnuchannotification: could not ask for %s: %s\n",
                NOTIF_BUS_NAME, error.message);
        dbus_error_free(&error);
        dbus_connection_unref(bus->connection);
        bus->connection = NULL;
        return -1;
    }
    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fprintf(stderr, "gnuchannotification: another notification server "
                        "owns %s; not starting\n", NOTIF_BUS_NAME);
        dbus_connection_unref(bus->connection);
        bus->connection = NULL;
        return -1;
    }
    bus->owns_name = 1;
    return 0;
}

int notif_dbus_owns_name(NotifDBus *bus) {
    return bus && bus->connection && bus->owns_name;
}

void notif_dbus_set_handlers(NotifDBus *bus, NotifNotifyHandler on_notify,
                             NotifCloseHandler on_close,
                             NotifActionHandler on_action, void *userdata) {
    if (!bus) {
        return;
    }
    bus->on_notify = on_notify;
    bus->on_close = on_close;
    bus->on_action = on_action;
    bus->userdata = userdata;
}

int notif_dbus_dispatch(NotifDBus *bus, int timeout_ms) {
    if (!bus || !bus->connection) {
        return 0;
    }
    /* read_write() answers FALSE once the connection is closed — the session
       bus going away, which is how this daemon loses its name. Its answer has
       to be read, and this is the whole bug when it is not: a closed
       connection is not waited on at all, so the call returns at once, and the
       loop around it spins on a dead bus at full speed for ever. So the FALSE
       is passed up as -1 and the caller stops. */
    if (!dbus_connection_read_write(bus->connection, timeout_ms)) {
        return -1;
    }

    int handled = 0;
    for (;;) {
        DBusMessage *message = dbus_connection_pop_message(bus->connection);
        if (!message) {
            break;
        }
        if (dbus_message_is_method_call(message, NOTIF_INTERFACE,
                                        "GetCapabilities")) {
            handle_get_capabilities(bus, message);
        } else if (dbus_message_is_method_call(message, NOTIF_INTERFACE,
                                               "GetServerInformation")) {
            handle_get_server_information(bus, message);
        } else if (dbus_message_is_method_call(message, NOTIF_INTERFACE,
                                               "Notify")) {
            handle_notify(bus, message);
        } else if (dbus_message_is_method_call(message, NOTIF_INTERFACE,
                                               "CloseNotification")) {
            handle_close(bus, message);
        } else if (dbus_message_get_type(message) ==
                   DBUS_MESSAGE_TYPE_METHOD_CALL) {
            send_empty_or_error(bus, message, 0);
        }
        dbus_message_unref(message);
        handled++;
    }

    dbus_connection_flush(bus->connection);
    return handled;
}

void notif_dbus_emit_closed(NotifDBus *bus, unsigned int id,
                            unsigned int reason) {
    if (!bus || !bus->connection) {
        return;
    }
    DBusMessage *signal = dbus_message_new_signal(
        NOTIF_OBJECT_PATH, NOTIF_INTERFACE, "NotificationClosed");
    if (!signal) {
        return;
    }
    DBusMessageIter iter;
    dbus_message_iter_init_append(signal, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &id);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &reason);
    dbus_connection_send(bus->connection, signal, NULL);
    dbus_message_unref(signal);
}

void notif_dbus_free(NotifDBus *bus) {
    if (!bus || !bus->connection) {
        return;
    }
    dbus_connection_flush(bus->connection);
    dbus_connection_unref(bus->connection);
    bus->connection = NULL;
}

void notif_request_free(NotifRequest *request) {
    if (!request) {
        return;
    }
    free(request->image_cards);
    request->image_cards = NULL;
    request->image_card_count = 0;
}
