/*
 * ss_dbus.c — the org.freedesktop.ScreenSaver name, low level.
 *
 * The D-Bus is spoken through libdbus directly rather than through a binding
 * layer, because what is needed is one name, five methods and a counter — and a
 * binding that could carry a whole toolkit is a large thing to put inside a
 * screen saver for that.
 *
 * Everything here is behind HAVE_DBUS. When the header is not there the build
 * defines the flag away, every function becomes a stub, and the program still
 * shows its animation: it simply cannot be inhibited by a video. That is the
 * honest fallback — a machine with no D-Bus is a machine where "do not run
 * during a film" is not a thing anyone asked for.
 */
#include <stdio.h>
#include <string.h>

#include "ss_dbus.h"

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

static DBusConnection *s_bus = NULL;
static int s_inhibit_count = 0;
static int s_claimed = 0;

/* The name a video player looks for, and the object path that goes with it. */
#define SS_BUS_NAME "org.freedesktop.ScreenSaver"
#define SS_OBJECT_PATH "/org/freedesktop/ScreenSaver"
#define SS_INTERFACE "org.freedesktop.ScreenSaver"

/* Answer a method call with a single value. Kept in one place because every
   method below is "read the call, send one thing back". */
static void reply_uint(DBusConnection *bus, DBusMessage *call,
                       dbus_uint32_t value) {
    DBusMessage *reply = dbus_message_new_method_return(call);
    if (!reply) {
        return;
    }
    dbus_message_append_args(reply, DBUS_TYPE_UINT32, &value,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(bus, reply, NULL);
    dbus_message_unref(reply);
}

static void reply_bool(DBusConnection *bus, DBusMessage *call, dbus_bool_t value) {
    DBusMessage *reply = dbus_message_new_method_return(call);
    if (!reply) {
        return;
    }
    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &value,
                             DBUS_TYPE_INVALID);
    dbus_connection_send(bus, reply, NULL);
    dbus_message_unref(reply);
}

static void reply_empty(DBusConnection *bus, DBusMessage *call) {
    DBusMessage *reply = dbus_message_new_method_return(call);
    if (!reply) {
        return;
    }
    dbus_connection_send(bus, reply, NULL);
    dbus_message_unref(reply);
}

/* Handle one method call. The method is matched by name; a name this saver does
   not know is answered as an error, which is what D-Bus expects rather than a
   silent drop. */
static void handle_call(DBusConnection *bus, DBusMessage *call) {
    const char *method = dbus_message_get_member(call);
    if (!method) {
        return;
    }

    if (strcmp(method, "Inhibit") == 0) {
        /* Two strings: the application's name and its reason. Both are ignored
           beyond being counted — the saver only needs to know that somebody,
           somewhere, does not want it to run. */
        const char *application = NULL;
        const char *reason = NULL;
        dbus_message_get_args(call, NULL,
                              DBUS_TYPE_STRING, &application,
                              DBUS_TYPE_STRING, &reason,
                              DBUS_TYPE_INVALID);
        s_inhibit_count++;
        reply_uint(bus, call, (dbus_uint32_t)s_inhibit_count);
        return;
    }
    if (strcmp(method, "UnInhibit") == 0) {
        if (s_inhibit_count > 0) {
            s_inhibit_count--;
        }
        reply_empty(bus, call);
        return;
    }
    if (strcmp(method, "Throttle") == 0) {
        /* Throttle asks the saver not to use the display's power saving, which
           this saver does not do at all, so it is accepted and dropped. */
        reply_uint(bus, call, 0);
        return;
    }
    if (strcmp(method, "UnThrottle") == 0) {
        reply_empty(bus, call);
        return;
    }
    if (strcmp(method, "Lock") == 0) {
        /* Locking is the lock screen's job, not the saver's. Answered as a
           no-op so a caller that asks is not surprised. */
        reply_empty(bus, call);
        return;
    }
    if (strcmp(method, "GetActive") == 0) {
        reply_bool(bus, call, 1);
        return;
    }
    if (strcmp(method, "GetActiveTime") == 0 ||
        strcmp(method, "GetSessionIdleTime") == 0) {
        reply_uint(bus, call, 0);
        return;
    }
    if (strcmp(method, "SetActive") == 0) {
        reply_bool(bus, call, 1);
        return;
    }

    /* A method this saver does not have. A proper error, not silence. */
    DBusMessage *reply = dbus_message_new_error(
        call, DBUS_ERROR_UNKNOWN_METHOD,
        "GnuChanSS does not have that method");
    if (reply) {
        dbus_connection_send(bus, reply, NULL);
        dbus_message_unref(reply);
    }
}

int ss_dbus_claim(void) {
    if (s_claimed) {
        return 1;
    }

    DBusError error;
    dbus_error_init(&error);
    s_bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (!s_bus) {
        dbus_error_free(&error);
        return 0;
    }

    /* Do not exit the process when the bus goes away; a saver that dies because
       a session bus restarted would take the screen with it. */
    dbus_connection_set_exit_on_disconnect(s_bus, 0);

    /* Ask for the name and do NOT queue: if another saver holds it, this one
       stays up but cannot be inhibited, which is better than waiting for a name
       that may never be given up. */
    int result = dbus_bus_request_name(s_bus, SS_BUS_NAME,
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    if (dbus_error_is_set(&error)) {
        dbus_error_free(&error);
        return 0;
    }
    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return 0;
    }

    s_claimed = 1;
    s_inhibit_count = 0;
    return 1;
}

int ss_dbus_inhibit_count(void) {
    return s_inhibit_count;
}

void ss_dbus_pump(void) {
    if (!s_bus) {
        return;
    }
    /* Deliver at most a handful per frame: more than that in one frame would
       stall the animation, and a call that waits is answered on the next. */
    for (int i = 0; i < 8; i++) {
        if (!dbus_connection_read_write(s_bus, 0)) {
            return;   /* the bus is gone */
        }
        DBusMessage *message = dbus_connection_pop_message(s_bus);
        if (!message) {
            return;
        }
        /* Only method calls addressed to this saver's own interface are
           answered; anything else on the bus is not ours. The member is checked
           by name inside handle_call(), which knows the methods. The type is
           asked first, and the interface compared by string, because
           dbus_message_is_method_call() with a NULL member trips an assertion
           inside libdbus — see the note in this file's history. */
        if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
            const char *interface = dbus_message_get_interface(message);
            if (!interface || strcmp(interface, SS_INTERFACE) == 0) {
                handle_call(s_bus, message);
            }
        }
        dbus_message_unref(message);
    }
}

void ss_dbus_release(void) {
    if (s_bus && s_claimed) {
        dbus_bus_release_name(s_bus, SS_BUS_NAME, NULL);
    }
    s_claimed = 0;
    s_inhibit_count = 0;
}

#else  /* !HAVE_DBUS */

/* No D-Bus in this build: every function is a stub and the saver cannot be
   inhibited. See the file comment for why that is the honest fallback. */

int ss_dbus_claim(void) {
    return 0;
}

int ss_dbus_inhibit_count(void) {
    return 0;
}

void ss_dbus_pump(void) {
}

void ss_dbus_release(void) {
}

#endif /* HAVE_DBUS */
