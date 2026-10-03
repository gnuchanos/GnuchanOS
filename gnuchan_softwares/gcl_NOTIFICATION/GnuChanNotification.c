/*
 * GnuChanNotification.c — the entry point.
 *
 * GnuChanNotification is a notification daemon in the shape of dunst: it holds
 * the name `org.freedesktop.Notifications` on the session bus and draws the
 * notifications other programs send it. It is a C program, and it is the same
 * program in this desktop's own colours — the purple ramp GnuChanWM, GnuChanDock
 * and GnuChanFetch are drawn in — with a picture when the sender carries one.
 *
 * This file is deliberately thin. All it does is decide what to read, in what
 * order, and hand the result to the modules that do the work: notif_config reads
 * the settings, notif_dbus speaks the protocol, notif_render draws the stack,
 * notif_item holds it. The loop at the bottom is the whole of the daemon:
 *
 *     read the bus for a little while    -> a notification arrives
 *     lay the stack out                  -> the bubbles find their places
 *     paint it                           -> the screen shows them
 *     tick                               -> what is past its time goes
 *
 *     GnuChanNotification            run the daemon
 *     GnuChanNotification --help     a short usage note
 *     GnuChanNotification --version  the version line
 *     GnuChanNotification --test     show one notification and exit
 *
 * An argument that is not one of those is refused rather than ignored, because
 * a person who typed something meant something by it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include "notif_config.h"
#include "notif_dbus.h"
#include "notif_item.h"
#include "notif_render.h"

#define PROGRAM_NAME    "GnuChanNotification"
#define PROGRAM_VERSION "1.0"

/* How long the loop waits for the bus before it looks at the clock. Short
   enough that a bubble expires when it should and long enough that a quiet
   daemon is not a busy one. */
#define NOTIF_TICK_MS 250

/* How long a critical notification is held when the sender asked for the
   server's own time. Critical means "do not let me miss this". */
#define NOTIF_CRITICAL_FACTOR 3

/* --- the clock ------------------------------------------------------------ */

/* Milliseconds since some fixed point, monotonic: a notification's life must
   not jump when the system clock is set. */
static long long now_milliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

/* --- the daemon ----------------------------------------------------------- */

typedef struct NotifDaemon {
    Display *display;
    int screen;
    NotifConfig config;
    NotifRender render;
    NotifDBus bus;
    NotifStack stack;
} NotifDaemon;

/* Resolve the sender's timeout against the daemon's own, as the protocol asks:
   -1 means "server decides", 0 means "never", a positive number is the sender's
   own milliseconds. A critical notification is held longer when the sender left
   the time to the server, because a program that marks something critical is
   telling the server it matters. */
static void resolve_timeout(const NotifDaemon *daemon, NotifItem *item,
                            int sent_timeout) {
    if (sent_timeout == 0) {
        item->never_expire = 1;
        item->timeout_ms = 0;
        return;
    }
    int timeout = sent_timeout > 0 ? sent_timeout : daemon->config.timeout;
    if (item->urgency == NOTIF_URGENCY_CRITICAL && sent_timeout <= 0) {
        timeout = daemon->config.timeout * NOTIF_CRITICAL_FACTOR;
    }
    item->timeout_ms = timeout;
    item->never_expire = 0;
}

/* A notification arrived. Make it an item, give it its picture, and let the
   stack place it. */
static unsigned int on_notify(const NotifRequest *request, void *userdata) {
    NotifDaemon *daemon = (NotifDaemon *)userdata;
    if (!daemon || !request) {
        return 0;
    }

    /* A client that replaces one of its own notifications hands back the id of
       the one to replace. That bubble is dropped, and its client is told it is
       gone, so the new one takes its place rather than standing beside it. */
    if (request->replaces_id != 0) {
        for (int i = 0; i < daemon->stack.count; i++) {
            if (daemon->stack.items[i].id == request->replaces_id) {
                notif_dbus_emit_closed(&daemon->bus, request->replaces_id, 3);
                notif_stack_drop(&daemon->stack, daemon->display, i);
                break;
            }
        }
    }

    NotifItem *item = notif_stack_add(&daemon->stack, daemon->display,
                                      now_milliseconds());
    if (!item) {
        /* Every bubble on the screen is critical and there is no room for one
           more. Nothing is drawn; the sender is still answered by the bus
           module, which is the protocol's business, not this one. */
        return 0;
    }

    snprintf(item->app_name, sizeof(item->app_name), "%s", request->app_name);
    snprintf(item->summary, sizeof(item->summary), "%s", request->summary);
    snprintf(item->body, sizeof(item->body), "%s", request->body);
    item->urgency = request->urgency;
    resolve_timeout(daemon, item, request->expire_timeout);

    /* The picture is loaded once and owned by the item. A hint of pixels wins
       over a path, because a sender that published pixels meant them; a path is
       otherwise read from disk. Either one failing is not a failure of the
       notification: the bubble is drawn with its text alone. */
    if (daemon->config.show_icon) {
        int loaded = 0;
        if (request->has_image_data && request->image_cards) {
            loaded = notif_image_load_argb(
                         daemon->display, daemon->render.root,
                         daemon->render.visual, daemon->render.depth,
                         &item->image, request->image_cards,
                         request->image_card_count, request->image_width,
                         request->image_height, daemon->config.icon_size,
                         daemon->render.icon_background) == 0;
        }
        if (!loaded && request->image_path[0]) {
            loaded = notif_image_load_file(
                         daemon->display, daemon->render.root,
                         daemon->render.visual, daemon->render.depth,
                         &item->image, request->image_path,
                         daemon->config.icon_size,
                         daemon->render.icon_background) == 0;
        }
        item->has_image = loaded;
    }

    daemon->render.now_ms = now_milliseconds();
    notif_render_layout(&daemon->render, &daemon->stack);
    notif_render_draw(&daemon->render, &daemon->stack);
    return item->id;
}

/* A client closed a notification by id. */
static void on_close(unsigned int id, void *userdata) {
    NotifDaemon *daemon = (NotifDaemon *)userdata;
    if (!daemon) {
        return;
    }
    for (int i = 0; i < daemon->stack.count; i++) {
        if (daemon->stack.items[i].id == id) {
            notif_stack_drop(&daemon->stack, daemon->display, i);
            notif_render_layout(&daemon->render, &daemon->stack);
            notif_render_draw(&daemon->render, &daemon->stack);
            return;
        }
    }
}

/* --- one test bubble ------------------------------------------------------ */

/* Show one notification without a client, so a person can see the daemon's own
   work — the colours, the shape, the picture — without writing a program to
   send it. It goes through the same path a real one does, all but the bus. */
static void show_test_notification(NotifDaemon *daemon) {
    NotifRequest request;
    memset(&request, 0, sizeof(request));
    snprintf(request.app_name, sizeof(request.app_name), PROGRAM_NAME);
    snprintf(request.summary, sizeof(request.summary), PROGRAM_NAME);
    snprintf(request.body, sizeof(request.body),
             "This is a test notification. It is drawn in the desktop's own "
             "purple, with a picture when one is named.");
    request.urgency = NOTIF_URGENCY_NORMAL;
    request.expire_timeout = 4000;
    on_notify(&request, daemon);
    notif_request_free(&request);
}

/* --- the usage note ------------------------------------------------------- */

static void print_help(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("\n");
    printf("A notification daemon in the shape of dunst: it holds the name\n");
    printf("org.freedesktop.Notifications on the session bus and draws the\n");
    printf("notifications other programs send it, in GnuchanOS's purple.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s              run the daemon\n", PROGRAM_NAME);
    printf("  %s --test       show one notification and exit\n", PROGRAM_NAME);
    printf("  %s --help       this note\n", PROGRAM_NAME);
    printf("  %s --version    the version\n", PROGRAM_NAME);
    printf("\n");
    printf("The settings are read from\n");
    printf("  ~/.config/GnuChanNotification/GnuChanNotification.py\n");
    printf("and every value has a default, so the daemon runs whether or not\n");
    printf("that file is there.\n");
}

/* --- the entry point ------------------------------------------------------ */

int main(int argc, char **argv) {
    int test_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = 1;
            continue;
        }
        fprintf(stderr, "%s: unknown argument '%s'\n", PROGRAM_NAME, argv[i]);
        fprintf(stderr, "run '%s --help' for the usage\n", PROGRAM_NAME);
        return 2;
    }

    NotifDaemon daemon;
    memset(&daemon, 0, sizeof(daemon));
    notif_stack_init(&daemon.stack);

    if (notif_config_load_default(&daemon.config) != 0) {
        if (daemon.config.error[0] &&
            strcmp(daemon.config.error, "no settings file was found") != 0) {
            fprintf(stderr, "%s: %s\n", PROGRAM_NAME, daemon.config.error);
            fprintf(stderr, "%s: using the built-in defaults\n", PROGRAM_NAME);
        }
    }

    daemon.display = XOpenDisplay(NULL);
    if (!daemon.display) {
        fprintf(stderr, "%s: no display to draw on\n", PROGRAM_NAME);
        return 1;
    }
    daemon.screen = DefaultScreen(daemon.display);

    if (notif_render_init(&daemon.render, daemon.display, daemon.screen,
                          &daemon.config) != 0) {
        fprintf(stderr, "%s: could not set up the window\n", PROGRAM_NAME);
        XCloseDisplay(daemon.display);
        return 1;
    }

    if (test_mode) {
        show_test_notification(&daemon);
        /* Give the server a moment to put the window up and for the eye to
           catch it, then take it down. */
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 400000000L };
        nanosleep(&pause, NULL);
        XSync(daemon.display, False);
        notif_stack_clear(&daemon.stack, daemon.display);
        notif_render_free(&daemon.render);
        XCloseDisplay(daemon.display);
        return 0;
    }

    if (notif_dbus_init(&daemon.bus) != 0) {
        fprintf(stderr, "%s: could not take the notification name\n",
                PROGRAM_NAME);
        notif_render_free(&daemon.render);
        XCloseDisplay(daemon.display);
        return 1;
    }
    notif_dbus_set_handlers(&daemon.bus, on_notify, on_close, NULL, &daemon);

    fprintf(stderr, "%s %s is running\n", PROGRAM_NAME, PROGRAM_VERSION);
    for (;;) {
        notif_dbus_dispatch(&daemon.bus, NOTIF_TICK_MS);

        long long now = now_milliseconds();

        /* Every bubble keeps its own clock, so before any is dropped each one
           whose time is up is announced to its client: the protocol says a
           notification that goes away says so, and a client that sent one and
           wanted to know is waiting for that word. The reason is 1, expired. */
        for (int i = 0; i < daemon.stack.count; i++) {
            NotifItem *item = &daemon.stack.items[i];
            if (!item->never_expire &&
                now - item->born_ms >= (long long)item->timeout_ms) {
                notif_dbus_emit_closed(&daemon.bus, item->id, 1);
            }
        }

        int expired = notif_stack_expire(&daemon.stack, daemon.display, now);
        if (expired > 0) {
            notif_render_layout(&daemon.render, &daemon.stack);
        }

        /* The countdown bars shrink every tick, so the stack is repainted
           every tick while any bubble is counting down — which is the whole
           point of a timer. A stack with nothing counting down is left alone,
           so a quiet daemon does no painting. */
        int counting = 0;
        for (int i = 0; i < daemon.stack.count; i++) {
            if (!daemon.stack.items[i].never_expire) {
                counting = 1;
                break;
            }
        }
        if (daemon.stack.count > 0 && counting) {
            daemon.render.now_ms = now;
            notif_render_draw(&daemon.render, &daemon.stack);
        } else if (expired > 0) {
            daemon.render.now_ms = now;
            notif_render_draw(&daemon.render, &daemon.stack);
        }

        /* The X side. A bubble window that is exposed — mapped for the first
           time, uncovered by another window moving away — has just been filled
           with its own background colour by the server, so the paint that was
           in it is gone and it stands there as an empty panel. An Expose is
           the server saying exactly that, and the answer is to paint again. */
        while (XPending(daemon.display)) {
            XEvent event;
            XNextEvent(daemon.display, &event);
            if (event.type == Expose && event.xexpose.count == 0) {
                notif_render_draw(&daemon.render, &daemon.stack);
            }
        }
        XFlush(daemon.display);
    }

    notif_stack_clear(&daemon.stack, daemon.display);
    notif_dbus_free(&daemon.bus);
    notif_render_free(&daemon.render);
    XCloseDisplay(daemon.display);
    return 0;
}
