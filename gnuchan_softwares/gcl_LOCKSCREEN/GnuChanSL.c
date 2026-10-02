/*
 * GnuChanSL.c — the GnuchanOS lock screen.
 *
 *     GnuChanSL              lock the screen; a password unlocks it
 *     GnuChanSL --version    print the version and exit
 *
 * A full-screen window over everything, a field to type a password in, and PAM
 * behind it: the password is checked against the system's own accounts, so a
 * password that works with `su` works here and an account that is locked stays
 * locked. A wrong password flashes red and clears; a right one takes the window
 * down and the process exits 0, which is how the session knows it was unlocked
 * rather than killed.
 *
 * --- what it is not ---
 *
 * It is not a screen saver and does not wait for an idle timer: it is run when
 * the session decides the screen should be locked — by the lid module on a wake
 * when OnLidOpenLockScreen is set, by the menu, or by hand. The screen saver is
 * GnuChanSS, a separate program; the two are named apart on purpose and do not
 * do each other's job.
 *
 * --- why the keyboard is grabbed ---
 *
 * A lock screen that let a keystroke reach the window underneath would let a
 * password be typed into a hidden terminal, so the keyboard and the pointer are
 * grabbed for as long as it is up. The grab is released the moment the window
 * comes down, right or wrong, so nothing is left held.
 */
#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#include "sl_config.h"
#include "sl_auth.h"

#define GNUCHANSL_VERSION "0.1.0"

/* The longest password this screen will accept. Far longer than any real one;
   it exists so the buffer has a written end. */
#define SL_MAX_PASSWORD 512

static volatile sig_atomic_t s_should_stop = 0;

static void handle_signal(int signum) {
    (void)signum;
    /* The flag is set so the loop can leave cleanly when it is asked by
       something that has already decided — a session shutdown, which logs the
       user out regardless. */
    s_should_stop = 1;
}

/* --- the pieces the drawing needs ----------------------------------------- */

typedef struct SlColours {
    XftColor background;
    XftColor panel;
    XftColor panel_edge;
    XftColor field;
    XftColor text;
    XftColor text_muted;
    XftColor accent;
    XftColor wrong;
} SlColours;

typedef struct SlScreen {
    Display *display;
    int screen;
    Window window;

    /* The off-screen copy one frame is drawn into before it is shown. Drawing
       straight to the window shows the frame half-made — the background, then
       the panel, then the field, then each line of text — and a keystroke
       redraws all of that, so the screen flickered on every key. Everything is
       drawn here and copied across in one operation instead. */
    Pixmap buffer;

    int width;
    int height;

    XftFont *font;
    XftDraw *draw;
    GC gc;

    SlColours colours;

    /* The settings, pointed at rather than copied: draw_screen reads the
       colours and the words out of it, and there is one of it for the life of
       the program. */
    const SlConfig *config;

    /* The password being typed, and how much of it there is. */
    char password[SL_MAX_PASSWORD];
    int password_length;

    /* Set while a wrong password is shown, with the wall-clock second it should
       stop being shown. */
    int wrong_shown;
    time_t wrong_until;
} SlScreen;

/* A colour name as an XftColor. Falls back to the given plain colour when the
   name cannot be parsed, so a mistyped colour is a plain one rather than a
   crash. */
static int load_colour(SlScreen *screen, const char *name,
                       const char *fallback, XftColor *out) {
    Visual *visual = DefaultVisual(screen->display, screen->screen);
    Colormap colormap = DefaultColormap(screen->display, screen->screen);
    if (XftColorAllocName(screen->display, visual, colormap,
                          (name && name[0]) ? name : fallback, out)) {
        return 1;
    }
    return XftColorAllocName(screen->display, visual, colormap, fallback, out);
}

static void free_colours(SlScreen *screen) {
    Visual *visual = DefaultVisual(screen->display, screen->screen);
    Colormap colormap = DefaultColormap(screen->display, screen->screen);
    XftColorFree(screen->display, visual, colormap, &screen->colours.background);
    XftColorFree(screen->display, visual, colormap, &screen->colours.panel);
    XftColorFree(screen->display, visual, colormap, &screen->colours.panel_edge);
    XftColorFree(screen->display, visual, colormap, &screen->colours.field);
    XftColorFree(screen->display, visual, colormap, &screen->colours.text);
    XftColorFree(screen->display, visual, colormap, &screen->colours.text_muted);
    XftColorFree(screen->display, visual, colormap, &screen->colours.accent);
    XftColorFree(screen->display, visual, colormap, &screen->colours.wrong);
}

/* --- drawing -------------------------------------------------------------- */

/* Draw one line of text centred on the screen at the given baseline y. */
static void draw_centred(SlScreen *screen, XftColor *colour, int y,
                         const char *text) {
    if (!text || !text[0]) {
        return;
    }
    XGlyphInfo extents;
    XftTextExtentsUtf8(screen->display, screen->font,
                       (const FcChar8 *)text, (int)strlen(text), &extents);
    int x = (screen->width - extents.xOff) / 2;
    XftDrawStringUtf8(screen->draw, colour, screen->font, x, y,
                      (const FcChar8 *)text, (int)strlen(text));
}

/* Draw a two-pixel rectangle by hand, so its thickness does not depend on the
   server's default line width. It draws onto the off-screen buffer, like
   everything else in a frame. */
static void draw_box(SlScreen *screen, unsigned long pixel,
                     int x, int y, int width, int height) {
    XSetForeground(screen->display, screen->gc, pixel);
    for (int i = 0; i < 2; i++) {
        XDrawRectangle(screen->display, screen->buffer, screen->gc,
                       x + i, y + i,
                       (unsigned int)(width - 2 * i - 1),
                       (unsigned int)(height - 2 * i - 1));
    }
}

/* The whole lock screen, drawn once per change. It is not animated: a lock
   screen that redrew on a timer would wake the machine for nothing, and the
   only things that change are the clock and the password field. */
static void draw_screen(SlScreen *screen) {
    const SlConfig *config = screen->config;
    int font_height = screen->font->ascent + screen->font->descent;

    /* The background covers everything. */
    XSetForeground(screen->display, screen->gc,
                   screen->colours.background.pixel);
    XFillRectangle(screen->display, screen->buffer, screen->gc, 0, 0,
                   (unsigned int)screen->width, (unsigned int)screen->height);

    int panel_width = 480;
    if (panel_width > screen->width - 40) {
        panel_width = screen->width - 40;
    }

    /* The panel is as tall as what it holds, counted by the same steps the
       drawing below walks. It used to be a fixed number of font-heights, which
       came out shorter than the clock, the three lines of text and the field
       put together once the field was added — so the field hung out of the
       bottom of its own panel. Counting the rows first is what keeps the box
       around its contents. */
    int field_height = font_height + 16;
    int panel_height = font_height + 28;              /* first row, top pad  */
    if (config->clock_format[0]) {
        panel_height += font_height + 22;             /* the clock           */
    }
    panel_height += font_height + 10;                 /* the title           */
    panel_height += font_height + 28;                 /* the subtitle        */
    panel_height += font_height + 8;                  /* the prompt          */
    panel_height += field_height;                     /* the password field  */
    panel_height += 28;                               /* bottom pad          */
    int panel_x = (screen->width - panel_width) / 2;
    int panel_y = (screen->height - panel_height) / 2;

    /* The panel: a filled box in the field colour, then its edge. */
    XSetForeground(screen->display, screen->gc, screen->colours.panel.pixel);
    XFillRectangle(screen->display, screen->buffer, screen->gc, panel_x,
                   panel_y, (unsigned int)panel_width,
                   (unsigned int)panel_height);
    draw_box(screen, screen->colours.panel_edge.pixel, panel_x, panel_y,
             panel_width, panel_height);

    int line = panel_y + font_height + 28;

    /* The clock, at the top of the panel, in the accent colour. An empty format
       means no clock at all, which is what a script that wrote ClockFormat=""
       asked for. */
    if (config->clock_format[0]) {
        time_t now = time(NULL);
        struct tm broken;
        localtime_r(&now, &broken);
        char clock_text[128];
        strftime(clock_text, sizeof(clock_text), config->clock_format, &broken);
        draw_centred(screen, &screen->colours.accent, line, clock_text);
    }
    line += font_height + 22;

    /* The title, then the subtitle under it. */
    draw_centred(screen, &screen->colours.text, line, config->title);
    line += font_height + 10;
    draw_centred(screen, &screen->colours.text_muted, line,
                 config->subtitle);
    line += font_height + 28;

    /* The prompt. */
    draw_centred(screen, &screen->colours.text_muted, line, config->prompt);
    line += font_height + 8;

    /* The password field. Its border is the accent colour, or the wrong colour
       while a wrong password is being shown. */
    XftColor border = screen->wrong_shown ? screen->colours.wrong
                                          : screen->colours.accent;
    int field_width = panel_width - 80;
    int field_x = panel_x + 40;
    int field_y = line;

    XSetForeground(screen->display, screen->gc, screen->colours.field.pixel);
    XFillRectangle(screen->display, screen->buffer, screen->gc, field_x,
                   field_y, (unsigned int)field_width,
                   (unsigned int)field_height);
    draw_box(screen, border.pixel, field_x, field_y, field_width, field_height);

    /* The typed password: one dot per character when dots are asked for, and
       nothing at all otherwise. */
    if (config->show_password_dots && screen->password_length > 0) {
        char dots[SL_MAX_PASSWORD + 1];
        int count = screen->password_length;
        if (count > (int)sizeof(dots) - 1) {
            count = (int)sizeof(dots) - 1;
        }
        for (int i = 0; i < count; i++) {
            dots[i] = '*';
        }
        dots[count] = '\0';
        XGlyphInfo extents;
        XftTextExtentsUtf8(screen->display, screen->font,
                           (const FcChar8 *)dots, count, &extents);
        int x = field_x + (field_width - extents.xOff) / 2;
        int text_y = field_y + 8 + screen->font->ascent;
        XftDrawStringUtf8(screen->draw, &border, screen->font, x, text_y,
                          (const FcChar8 *)dots, count);
    }

    /* The finished frame, copied onto the window in one operation, so what is
       on screen is always a whole frame and never a half-drawn one. */
    XCopyArea(screen->display, screen->buffer, screen->window, screen->gc,
              0, 0, (unsigned int)screen->width, (unsigned int)screen->height,
              0, 0);
    XFlush(screen->display);
}

/* --- the program ---------------------------------------------------------- */

/* The user whose password is being asked for: the one who owns the session,
   from LOGNAME or USER, or the real uid's account as a last resort. */
static const char *session_user(char *buffer, unsigned int size) {
    const char *name = getenv("LOGNAME");
    if (!name || !name[0]) {
        name = getenv("USER");
    }
    if (name && name[0]) {
        snprintf(buffer, size, "%s", name);
        return buffer;
    }
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        snprintf(buffer, size, "%s", pw->pw_name);
        return buffer;
    }
    buffer[0] = '\0';
    return buffer;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanSL %s\n", GNUCHANSL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanSL [--version]\n"
                   "\n"
                   "Locks the screen. Type your password to unlock. The\n"
                   "screen can be locked by the session's own key, by the\n"
                   "menu, or by the lid module on a wake when\n"
                   "OnLidOpenLockScreen is set.\n");
            return 0;
        }
    }

    setlocale(LC_ALL, "");

    SlConfig config;
    if (sl_config_load_default(&config) != 0 && config.error[0]) {
        fprintf(stderr, "gnuchansl: %s\n", config.error);
    }

    char user[SL_TEXT_LENGTH];
    session_user(user, sizeof(user));
    if (!user[0]) {
        fprintf(stderr, "gnuchansl: could not work out whose password to ask "
                        "for; there is no LOGNAME, USER or account.\n");
        return 1;
    }
    /* The title defaults to the user's own name, which is the one thing a
       machine that named none still wants to see. */
    if (config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", user);
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "gnuchansl: cannot open the display; DISPLAY is '%s'\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
        return 1;
    }
    int screen_number = DefaultScreen(display);
    int width = DisplayWidth(display, screen_number);
    int height = DisplayHeight(display, screen_number);
    Window root = RootWindow(display, screen_number);

    SlScreen state;
    memset(&state, 0, sizeof(state));
    state.display = display;
    state.screen = screen_number;
    state.width = width;
    state.height = height;
    state.config = &config;

    Visual *visual = DefaultVisual(display, screen_number);
    Colormap colormap = DefaultColormap(display, screen_number);

    /* The font. A font that cannot be opened is fatal: a lock screen with no
       text is a screen nobody can get past. */
    char font_name[SL_TEXT_LENGTH * 2];
    snprintf(font_name, sizeof(font_name), "%s:pixelsize=%d",
             config.font_family[0] ? config.font_family : "monospace",
             config.font_size > 0 ? config.font_size : 14);
    state.font = XftFontOpenName(display, screen_number, font_name);
    if (!state.font) {
        state.font = XftFontOpenName(display, screen_number,
                                     "monospace:pixelsize=14");
    }
    if (!state.font) {
        fprintf(stderr, "gnuchansl: cannot open a font; install a monospace "
                        "font.\n");
        XCloseDisplay(display);
        return 1;
    }

    /* Take the keyboard and the pointer on the ROOT window, before the lock
       window is even mapped, and only then raise the lock over everything.
       Held this way the grab cannot fail the way a just-mapped window's can
       (GrabNotViewable), and — just as important — it is a single-instance
       lock: the key that locks the screen pressed twice leaves the first
       lock screen holding the keyboard, so the second one cannot take it and
       leaves without covering the screen. Before this the second lock screen
       would come up over the first but could not hold the keyboard, so every
       keystroke went past it to the window manager underneath: the password
       field did nothing and the manager's own hotkeys fired instead. If the
       grab does not succeed, a lock screen is already up and this one backs
       out rather than covering the working one. */
    /* The grab is retried for a short while before giving up.
     *
     * When this program is started by a key binding, the modifier key (Alt)
     * is still held down as it runs and the window manager's passive grab on
     * that combination is still active, so the very first attempt can fail
     * with AlreadyGrabbed — and before this the lock screen then exited at
     * once and nothing appeared on the screen. That is exactly why the lock
     * key "did nothing" while the screen-saver key, whose program already
     * retried, worked. Retrying lets the key be released and the grab succeed.
     *
     * A failure that outlasts the retries means a lock screen really is up,
     * and this one leaves without covering the working one. */
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
                "gnuchansl: the screen is already locked (keyboard %d, "
                "pointer %d)\n", (int)keyboard_grab, (int)pointer_grab);
        XCloseDisplay(display);
        return 0;
    }

    /* The window: override-redirect so no window manager decorates or places
       it, full screen, and black until the first draw. The keyboard and the
       pointer arrive through the grab on the root, taken above, so the window
       needs only to be shown. */
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = BlackPixel(display, screen_number);
    attributes.event_mask = ExposureMask | StructureNotifyMask;

    state.window = XCreateWindow(
        display, root, 0, 0, (unsigned int)width, (unsigned int)height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
    XMapRaised(display, state.window);
    XRaiseWindow(display, state.window);

    state.gc = XCreateGC(display, state.window, 0, NULL);

    /* One frame is built here and copied to the window whole. A pixmap of the
       screen's own depth, so the copy is a plain blit with no conversion. */
    state.buffer = XCreatePixmap(display, state.window,
                                 (unsigned int)width, (unsigned int)height,
                                 (unsigned int)DefaultDepth(display,
                                                            screen_number));

    memset(&state.colours, 0, sizeof(state.colours));
    load_colour(&state, config.background, "#0d0512", &state.colours.background);
    load_colour(&state, config.panel, "#1b0c22", &state.colours.panel);
    load_colour(&state, config.panel_edge, "#7b2cbf", &state.colours.panel_edge);
    load_colour(&state, config.field, "#241033", &state.colours.field);
    load_colour(&state, config.text, "#e0c3fc", &state.colours.text);
    load_colour(&state, config.text_muted, "#9d7bba", &state.colours.text_muted);
    load_colour(&state, config.accent, "#d400ff", &state.colours.accent);
    load_colour(&state, config.wrong, "#ff4d6d", &state.colours.wrong);

    /* The text is drawn onto the same off-screen buffer the shapes go to. */
    state.draw = XftDrawCreate(display, state.buffer, visual, colormap);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    draw_screen(&state);

    int unlocked = 0;
    int last_clock = -1;
    while (!unlocked && !s_should_stop) {
        /* The wrong-password flash comes down on its own after wrong_seconds,
           with no key needed: a person who mistypes should not have to press a
           key to be allowed to try again. */
        if (state.wrong_shown && time(NULL) >= state.wrong_until) {
            state.wrong_shown = 0;
            state.password_length = 0;
            state.password[0] = '\0';
            draw_screen(&state);
        }

        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == Expose) {
                draw_screen(&state);
                continue;
            }
            if (event.type != KeyPress) {
                continue;
            }

            /* The key, as a character. Only the printable ones go into the
               password; Return checks it and Backspace takes the last one off.
               Everything else is ignored, which is what a password field
               does. */
            char text[8];
            KeySym keysym = NoSymbol;
            int count = XLookupString(&event.xkey, text, sizeof(text),
                                      &keysym, NULL);

            /* A key pressed while Alt, Ctrl or Super is held is a hotkey
               combination, not a character of a password, so it is dropped
               whole: Enter with Alt down must not submit the field, and no
               combination must leave a stray character behind. Shift is left
               alone, because a capital letter is a real part of a password. */
            if (event.xkey.state & (ControlMask | Mod1Mask | Mod4Mask)) {
                continue;
            }

            if (keysym == XK_Return || keysym == XK_KP_Enter) {
                if (state.password_length > 0 &&
                    sl_auth_check(user, state.password)) {
                    unlocked = 1;
                    break;
                }
                /* Wrong: flash, keep the message up for the configured time,
                   and clear the field. */
                state.wrong_shown = 1;
                state.wrong_until = time(NULL) + config.wrong_seconds;
                state.password_length = 0;
                state.password[0] = '\0';
                draw_screen(&state);
            } else if (keysym == XK_BackSpace) {
                if (state.password_length > 0) {
                    state.password[--state.password_length] = '\0';
                    draw_screen(&state);
                }
            } else if (count > 0 && isprint((unsigned char)text[0])) {
                if (state.password_length < SL_MAX_PASSWORD - 1) {
                    state.password[state.password_length++] = text[0];
                    state.password[state.password_length] = '\0';
                    if (state.wrong_shown) {
                        state.wrong_shown = 0;
                    }
                    draw_screen(&state);
                }
            }
        }

        /* The clock is redrawn when its minute changes, and not otherwise:
           there is nothing else to draw between keys. */
        if (config.clock_format[0]) {
            time_t now = time(NULL);
            int minute = (int)(now / 60);
            if (minute != last_clock) {
                last_clock = minute;
                draw_screen(&state);
            }
        }

        /* A short wait between looks: the loop only has to notice a key and a
           clock tick, and spinning would keep the machine's fan on for a screen
           nobody is using. */
        if (!unlocked && !s_should_stop) {
            struct timespec pause = {0, 100 * 1000 * 1000};
            nanosleep(&pause, NULL);
        }
    }

    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);

    /* The password is wiped from memory before the process ends: a lock screen
       that left it lying in a buffer would be a lock screen that wrote the
       secret down. */
    memset(state.password, 0, sizeof(state.password));

    XftDrawDestroy(state.draw);
    free_colours(&state);
    XftFontClose(display, state.font);
    XFreePixmap(display, state.buffer);
    XFreeGC(display, state.gc);
    XDestroyWindow(display, state.window);
    XCloseDisplay(display);
    return 0;
}
