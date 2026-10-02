/*
 * GnuChanSS.c — the GnuchanOS screen saver.
 *
 *     GnuChanSS              run: wait for the desk to go quiet, then show
 *     GnuChanSS --once       show at once, ignoring the idle timer
 *     GnuChanSS --effect X   force an effect ("pipe" or "3dwall")
 *     GnuChanSS --version    print the version and exit
 *
 * This is the entry point and the running loop. It owns the X connection, the
 * full-screen window, the timing, and the one decision that matters — "has
 * someone come back to the desk?" — and it owns nothing about what is drawn:
 * that is ss_effect.c, and which one runs is ss_config.c's answer.
 *
 * --- the two ways it starts ---
 *
 * The lid module of GnuChanWM runs it on a wake-up (OnLidOpenScreenSaver), and
 * a session may start it once at login to idle in the background. Both land
 * here: without --once it first waits until the keyboard and pointer have been
 * still for IdleSeconds, then shows; with --once it shows the moment it starts.
 *
 * --- the two ways it stops ---
 *
 * Any key, any click, or a pointer move ends the show and the screen returns to
 * what was under it. A SIGTERM or SIGINT (the session asking it to go, the lid
 * closing) ends it the same way. In every case the window is destroyed and the
 * process exits 0, so whoever ran it can tell a normal end from a fault.
 *
 * --- why it can be paused by a video ---
 *
 * While the org.freedesktop.ScreenSaver name is held and an inhibit is active,
 * nothing is drawn. That is what keeps a pipe show from appearing over a film;
 * see ss_dbus.c. The inhibit is consulted in BOTH places it matters: the idle
 * wait does not fire while one is held (a film is watched with nobody touching
 * the keyboard, so idle time alone would open the saver over it), and the show
 * does not draw while one appears mid-frame.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>

#include "ss_config.h"
#include "ss_effect.h"
#include "ss_dbus.h"

#define GNUCHANSS_VERSION "0.1.0"

/* The frame target: about thirty frames a second. A screen saver is a picture
   to glance at, not a game, and every frame is drawn on the processor — the X
   server tessellates each thick line itself — so thirty is plenty for a slow
   crawl and half the work of sixty. */
#define SS_FRAME_MICROSECONDS 33333

/* A pointer move only counts as "someone is here" when it moves more than a
   few pixels: a saver on a shaky desk must not blink out because a mouse
   twitched one pixel. */
#define SS_MOTION_THRESHOLD 4

static volatile sig_atomic_t s_should_stop = 0;

static void handle_signal(int signum) {
    (void)signum;
    s_should_stop = 1;
}

/* --- the idle clock ------------------------------------------------------- */

/* How long the keyboard and pointer have been still, in seconds, from the X
   server. The server keeps this number itself, so it is the same measurement it
   uses for its own blanking. A server without the extension answers a large
   value, so the saver starts rather than never on such a machine. */
static double idle_seconds(Display *display) {
    static XScreenSaverInfo *info = NULL;
    if (!info) {
        info = XScreenSaverAllocInfo();
        if (!info) {
            return 0.0;
        }
    }
    Window root = DefaultRootWindow(display);
    if (!XScreenSaverQueryInfo(display, root, info)) {
        return 0.0;
    }
    return (double)info->idle / 1000.0;
}

/* Block until the desk has been quiet for `seconds` AND nothing is holding the
   screen-saver inhibit. A signal that asks the process to stop is respected
   here too, so a session can end a still-waiting saver without waiting out the
   timer.
 *
 * The inhibit is the whole reason this is not a plain "sleep until idle": a
 * film plays with nobody touching the keyboard, so the idle clock reaches the
 * limit while the person is watching. Without the second condition the saver
 * would open over the middle of the film — the idle time says "away" and only
 * the inhibit knows why. While an inhibit is held the wait simply keeps
 * looping, so the moment the video stops the ordinary idle test can fire. The
 * bus is pumped here for the same reason it is pumped in the show: it is how
 * the inhibit count ever changes. */
static void wait_for_idle(Display *display, int seconds) {
    while (!s_should_stop) {
        ss_dbus_pump();
        if (ss_dbus_inhibit_count() == 0 &&
            idle_seconds(display) >= (double)seconds) {
            return;
        }
        usleep(500 * 1000);   /* half a second between looks */
    }
}

/* --- the window ----------------------------------------------------------- */

/* A colour name from the settings, as a pixel. Falls back to black when the
   name cannot be parsed, so a mistyped colour is black rather than a crash. */
static unsigned long pixel_of(Display *display, int screen, const char *name,
                              unsigned long fallback) {
    Colormap colormap = DefaultColormap(display, screen);
    XColor colour;
    XColor exact;
    if (!name || !name[0]) {
        return fallback;
    }
    if (XAllocNamedColor(display, colormap, name, &colour, &exact)) {
        return colour.pixel;
    }
    return fallback;
}

/* --- the show ------------------------------------------------------------- */

/* Run the show once: open the window, draw frames until someone comes back.
   Returns 0 on a normal end. */
static int run_show(Display *display, int screen, const SsConfig *config,
                    unsigned long primary, unsigned long background) {
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    Window root = RootWindow(display, screen);
    unsigned long black = BlackPixel(display, screen);

    /* Take the keyboard and the pointer on the ROOT window, before anything is
       drawn, and not on the saver's own window as it used to be. Two faults
       come from doing it here and both of them happened:

         - The root is always viewable, so the grab cannot fail the way a
           just-mapped window's can (GrabNotViewable). The saver is then
           guaranteed to receive every key, which is what makes any key end the
           show.
         - It is a single-instance lock. A second saver — the saver key pressed
           twice — cannot take the keyboard, finds that out, and leaves without
           covering the screen. Before this, the second saver drew itself on top
           of the first but could not take the keyboard, so the key that should
           have ended the show went to the first saver behind it and the screen
           was left covered with nothing able to end it. */
    /* The grab is retried for a short while before giving up. When this
       program is started by the window manager's own key, that key is still
       held down as this runs, and the manager's grab on it can make the very
       first attempt fail — which used to end the program instantly, so the
       saver never appeared and the key's auto-repeat started another copy on
       every repeat. Retrying lets the key be released and the grab succeed.
       A failure that outlasts the retries means another screen saver really is
       up, and then this one leaves without covering the screen. */
    Status keyboard_grab = GrabNotViewable;
    Status pointer_grab = GrabNotViewable;
    for (int attempt = 0; attempt < 15 && !s_should_stop; attempt++) {
        keyboard_grab = XGrabKeyboard(display, root, False, GrabModeAsync,
                                      GrabModeAsync, CurrentTime);
        if (keyboard_grab != GrabSuccess) {
            struct timespec pause = {0, 100 * 1000 * 1000};   /* 100 ms */
            nanosleep(&pause, NULL);
            continue;
        }
        pointer_grab = XGrabPointer(display, root, False,
                                    ButtonPressMask | PointerMotionMask,
                                    GrabModeAsync, GrabModeAsync, None, None,
                                    CurrentTime);
        if (pointer_grab == GrabSuccess) {
            break;
        }
        XUngrabKeyboard(display, CurrentTime);
        struct timespec pause = {0, 100 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
    if (keyboard_grab != GrabSuccess || pointer_grab != GrabSuccess) {
        fprintf(stderr,
                "gnuchanss: could not take the keyboard and pointer "
                "(keyboard %d, pointer %d); another screen saver is probably "
                "up\n", (int)keyboard_grab, (int)pointer_grab);
        return 0;
    }

    /* An override-redirect window is not decorated or placed by any window
       manager: the saver covers the whole screen with no frame. The keyboard
       and the pointer arrive through the grab on the root, taken above. */
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = black;
    attributes.event_mask = ExposureMask | StructureNotifyMask;

    Window window = XCreateWindow(
        display, root, 0, 0, (unsigned int)width, (unsigned int)height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);

    XMapRaised(display, window);

    GC gc = XCreateGC(display, window, 0, NULL);

    /* Every frame is built on this off-screen copy and copied to the window in
       one operation. Drawing straight to the window shows the picture being
       made — the background cleared, then the shapes laid on it — and at speed
       that half-built state is what the eye reads as a flicker. A pixmap of the
       screen's own depth means the copy across is a plain blit. */
    Pixmap buffer = XCreatePixmap(display, window,
                                  (unsigned int)width, (unsigned int)height,
                                  (unsigned int)DefaultDepth(display, screen));

    SsEffect effect;
    ss_effect_init(&effect, config->effect, width, height, primary, background);

    /* The pipe draws over what is there, so the screen starts the background
       colour; the wall clears itself every frame. Doing it for both is one
       line and makes the first frame right either way. */
    XSetForeground(display, gc, background);
    XFillRectangle(display, buffer, gc, 0, 0,
                   (unsigned int)width, (unsigned int)height);
    XFlush(display);

    int last_x = -1;
    int last_y = -1;
    int running = 1;

    while (running && !s_should_stop) {
        /* Everything the user did arrives as events; any of them ends the
           show. A pointer move has to be a real one (see SS_MOTION_THRESHOLD). */
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            switch (event.type) {
            case KeyPress:
            case ButtonPress:
                running = 0;
                break;
            case MotionNotify: {
                int dx = event.xmotion.x - last_x;
                int dy = event.xmotion.y - last_y;
                if (last_x >= 0 &&
                    (abs(dx) > SS_MOTION_THRESHOLD ||
                     abs(dy) > SS_MOTION_THRESHOLD)) {
                    running = 0;
                }
                last_x = event.xmotion.x;
                last_y = event.xmotion.y;
                break;
            }
            default:
                break;
            }
        }
        if (!running) {
            break;
        }

        /* A video playing holds an inhibit: do not draw, but keep answering the
           bus so this state can change. See ss_dbus.c. */
        ss_dbus_pump();
        if (ss_dbus_inhibit_count() > 0) {
            struct timespec pause = {0, 250 * 1000 * 1000};
            nanosleep(&pause, NULL);
            continue;
        }

        /* The frame is built on the off-screen copy, then copied over in one
           operation, so the window is only ever shown whole. */
        effect.draw(&effect, display, buffer, gc);
        XCopyArea(display, buffer, window, gc, 0, 0,
                  (unsigned int)width, (unsigned int)height, 0, 0);
        XFlush(display);

        struct timespec frame = {0, SS_FRAME_MICROSECONDS * 1000};
        nanosleep(&frame, NULL);
    }

    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    ss_effect_free(&effect);
    XFreePixmap(display, buffer);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XFlush(display);
    return 0;
}

/* --- the program ---------------------------------------------------------- */

int main(int argc, char **argv) {
    int run_once = 0;
    const char *forced_effect = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) {
            run_once = 1;
        } else if (strcmp(argv[i], "--effect") == 0 && i + 1 < argc) {
            forced_effect = argv[++i];
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanSS %s\n", GNUCHANSS_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanSS [--once] [--effect pipe|3dwall] "
                   "[--version]\n"
                   "\n"
                   "Shows a full-screen screen saver after the keyboard and\n"
                   "pointer have been idle for IdleSeconds (see\n"
                   "~/.config/GnuChanSS/GnuChanSS.py). Any key, click or\n"
                   "pointer move ends it. A program playing a video can hold\n"
                   "it off through the freedesktop ScreenSaver name.\n");
            return 0;
        }
    }

    SsConfig config;
    if (ss_config_load_default(&config) != 0 && config.error[0]) {
        /* A file that was there and could not be read is worth saying; a
           missing file is not, because the defaults are what was wanted. */
        fprintf(stderr, "gnuchanss: %s\n", config.error);
    }

    /* A forced effect overrides the file, for a person trying one out from a
       shell. */
    if (forced_effect) {
        config.effect = ss_effect_of(forced_effect);
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "gnuchanss: cannot open the display; DISPLAY is '%s'\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
        return 1;
    }
    int screen = DefaultScreen(display);

    /* A signal is a clean way in: the session may ask the saver to stop when
       the user logs out or the lid closes. */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    /* SA_RESTART is left off so a signal interrupts the frame's sleep and the
       loop can see the flag. */
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    /* Take the ScreenSaver name before the idle wait, so a video that starts
       while the saver is still waiting can inhibit it. A failure here is not
       fatal: the saver still shows, it just cannot be paused. */
    if (config.inhibit && ss_dbus_claim() != 1) {
        fprintf(stderr,
                "gnuchanss: could not take the ScreenSaver name; a video "
                "will not be able to pause the show\n");
    }

    if (!run_once) {
        wait_for_idle(display, config.idle_seconds);
    }

    if (!s_should_stop) {
        unsigned long primary = pixel_of(display, screen, config.primary,
                                         WhitePixel(display, screen));
        unsigned long background = pixel_of(display, screen, config.background,
                                            BlackPixel(display, screen));
        run_show(display, screen, &config, primary, background);
    }

    ss_dbus_release();
    XCloseDisplay(display);
    return 0;
}
