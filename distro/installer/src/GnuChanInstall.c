/*
 * GnuChanInstall.c - the GnuchanOS installer.
 *
 *     GnuChanInstall            the installer window
 *     GnuChanInstall --version  print the version and exit
 *
 * A window that asks four things - which disk, the login name, the password
 * (twice) and the computer's name - and then installs: it runs the shell script
 * /usr/local/sbin/gnuchanos-install with those answers and shows every line the
 * script prints as it comes. The window itself never touches a disk; the script
 * does, and says what it is doing in its own words. See inst.h.
 *
 * --- one thread, and why ----------------------------------------------------
 *
 * Xlib is not safe to call from two threads, so the whole program is one: the
 * loop waits with select() on TWO things at once - the X connection and the
 * installer's output pipe - and answers whichever is ready. That is how the
 * script's output is shown WHILE it runs and the window still redraws, without
 * a second thread to corrupt Xlib's state.
 *
 * License: GPL3.
 */

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/wait.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#include "inst.h"

#define GNUCHANOS_INSTALL_VERSION "0.1.0"

/* The window's starting size. Fixed, because this is a form with a fixed number
   of rows and a text pane; resizing it would only move the pane's edges. */
#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 660

/* --- the fields, in the order the window shows them ------------------------ */

static void build_fields(InstApp *app) {
    static const struct {
        const char *label;
        InstFieldKind kind;
        const char *initial;
    } spec[] = {
        { "Disk",             INST_FIELD_PLAIN,  ""           },
        { "Login name",       INST_FIELD_PLAIN,  "gnuchanos"  },
        { "Full name",        INST_FIELD_PLAIN,  ""           },
        { "Password",         INST_FIELD_SECRET, ""           },
        { "Password again",   INST_FIELD_SECRET, ""           },
        { "Computer name",    INST_FIELD_PLAIN,  "gnuchan"    },
    };
    app->field_count = (int)(sizeof(spec) / sizeof(spec[0]));
    for (int i = 0; i < app->field_count; i++) {
        InstField *field = &app->fields[i];
        memset(field, 0, sizeof(*field));
        field->label = spec[i].label;
        field->kind = spec[i].kind;
        snprintf(field->text, sizeof(field->text), "%s", spec[i].initial);
        field->length = (int)strlen(field->text);
    }
    app->field_active = 1;     /* the login name, the first thing to type */
}

/* --- the keyboard ---------------------------------------------------------- */

/* A key typed into the focused field. Only the printable characters, Backspace
   and Return do anything; everything else, and any key with Control, Alt or
   Super held, is left alone - a shortcut is not part of an answer. */
static void handle_key(InstApp *app, XKeyEvent *event) {
    if (app->running) {
        return;
    }
    if (app->disk_dropdown_open) {
        char buffer[32];
        KeySym keysym = NoSymbol;
        XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);
        if (keysym == XK_Escape) {
            app->disk_dropdown_open = 0;
            inst_layout(app);
        } else if (keysym == XK_Up || keysym == XK_Down) {
            int step = keysym == XK_Up ? -1 : 1;
            app->disk_dropdown_cursor += step;
            if (app->disk_dropdown_cursor < 0) {
                app->disk_dropdown_cursor = app->disk_count - 1;
            } else if (app->disk_dropdown_cursor >= app->disk_count) {
                app->disk_dropdown_cursor = 0;
            }
            if (app->disk_dropdown_cursor < app->disk_dropdown_scroll) {
                app->disk_dropdown_scroll = app->disk_dropdown_cursor;
            } else if (app->disk_dropdown_cursor >=
                       app->disk_dropdown_scroll + INST_DISK_DROPDOWN_ROWS) {
                app->disk_dropdown_scroll =
                    app->disk_dropdown_cursor - INST_DISK_DROPDOWN_ROWS + 1;
            }
        } else if (keysym == XK_Return || keysym == XK_KP_Enter) {
            app->disk_choice = app->disk_dropdown_cursor;
            snprintf(app->fields[0].text, sizeof(app->fields[0].text), "%s",
                     app->disks[app->disk_choice].description);
            app->fields[0].length = (int)strlen(app->fields[0].text);
            app->disk_dropdown_open = 0;
            app->field_active = 0;
            inst_layout(app);
        }
        inst_draw(app);
        return;
    }
    if (app->confirming_erase) {
        if (event->state & (ControlMask | Mod1Mask | Mod4Mask)) {
            return;
        }

        char buffer[32];
        KeySym keysym = NoSymbol;
        int count = XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);
        size_t length = strlen(app->erase_confirmation);

        if (keysym == XK_Escape) {
            app->confirming_erase = 0;
            app->erase_confirmation[0] = '\0';
            inst_set_note(app, 0, "Erase confirmation cancelled.");
        } else if (keysym == XK_BackSpace) {
            if (length > 0) {
                app->erase_confirmation[--length] = '\0';
            }
            inst_set_note(app, 1, "Type ERASE exactly, then press Enter.");
        } else if (keysym == XK_Return || keysym == XK_KP_Enter) {
            if (strcmp(app->erase_confirmation, "ERASE") == 0) {
                app->confirming_erase = 0;
                app->erase_confirmed = 1;
                inst_engine_start(app);
            } else {
                inst_set_note(app, 1, "Type ERASE exactly, then press Enter.");
            }
        } else if (count == 1 && buffer[0] >= 0x20 && buffer[0] < 0x7f &&
                   length < sizeof(app->erase_confirmation) - 1) {
            app->erase_confirmation[length] = buffer[0];
            app->erase_confirmation[length + 1] = '\0';
            inst_set_note(app, 1, "Confirmation: %s (type ERASE, then Enter)",
                          app->erase_confirmation);
        }
        inst_draw(app);
        return;
    }

    int index = app->field_active;
    if (index < 1 || index >= app->field_count) {
        return;      /* the disk row and "no field" take no typing */
    }
    InstField *field = &app->fields[index];

    char buffer[32];
    KeySym keysym = NoSymbol;
    int count = XLookupString(event, buffer, sizeof(buffer), &keysym, NULL);

    if (event->state & (ControlMask | Mod1Mask | Mod4Mask)) {
        return;
    }

    if (keysym == XK_Return || keysym == XK_KP_Enter ||
        keysym == XK_Tab) {
        /* move to the next answer, so a person can type without the mouse */
        app->field_active = (index + 1 < app->field_count) ? index + 1 : 0;
        if (app->field_active == 0) {
            app->field_active = 1;
        }
        inst_draw(app);
        return;
    }
    if (keysym == XK_BackSpace) {
        if (field->length > 0) {
            do {
                field->length--;
            } while (field->length > 0 &&
                     ((unsigned char)field->text[field->length] & 0xc0) == 0x80);
            field->text[field->length] = '\0';
            inst_draw(app);
        }
        return;
    }
    if (count > 0) {
        int allow_utf8 = index == 2 || index == 3 || index == 4;
        if (count > INST_FIELD_TEXT - 1 - field->length) {
            return;
        }
        for (int i = 0; i < count; i++) {
            unsigned char byte = (unsigned char)buffer[i];
            if (!((byte >= 0x20 && byte < 0x7f) ||
                  (allow_utf8 && byte >= 0x80))) {
                return;
            }
        }
        for (int i = 0; i < count; i++) {
            field->text[field->length++] = buffer[i];
        }
        field->text[field->length] = '\0';
        inst_draw(app);
    }
}

/* --- the window ------------------------------------------------------------ */

static void set_up_window(InstApp *app) {
    app->display = XOpenDisplay(NULL);
    if (app->display == NULL) {
        fprintf(stderr, "gnuchanos-installer: cannot open the display; "
                        "DISPLAY is '%s'\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(unset)");
        exit(1);
    }
    app->screen = DefaultScreen(app->display);
    app->depth = DefaultDepth(app->display, app->screen);
    Window root = RootWindow(app->display, app->screen);

    app->width = WINDOW_WIDTH;
    app->height = WINDOW_HEIGHT;

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.background_pixel = BlackPixel(app->display, app->screen);
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            PointerMotionMask | StructureNotifyMask;
    app->window = XCreateWindow(
        app->display, root, 0, 0, (unsigned int)app->width,
        (unsigned int)app->height, 0, CopyFromParent, InputOutput,
        CopyFromParent, CWBackPixel | CWEventMask, &attributes);

    XStoreName(app->display, app->window, "Install GnuChanOS");

    /* Close the window through the window manager's own protocol, so the close
       button and the program agree; a terminal `kill` is handled the same way
       by the WM_DELETE_WINDOW message below. */
    Atom wm_delete = XInternAtom(app->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(app->display, app->window, &wm_delete, 1);

    /* Centre it on the screen: an installer that opens in a corner is one a
       person has to go looking for. */
    int screen_w = DisplayWidth(app->display, app->screen);
    int screen_h = DisplayHeight(app->display, app->screen);
    XMoveWindow(app->display, app->window,
                (screen_w - app->width) / 2, (screen_h - app->height) / 2);

    XMapWindow(app->display, app->window);

    app->gc = XCreateGC(app->display, app->window, 0, NULL);
    app->buffer = XCreatePixmap(app->display, app->window,
                                (unsigned int)app->width,
                                (unsigned int)app->height,
                                (unsigned int)app->depth);
}

static void set_up_fonts(InstApp *app) {
    const char *family = "sans";
    char name[128];
    snprintf(name, sizeof(name), "%s:pixelsize=14", family);
    app->font = XftFontOpenName(app->display, app->screen, name);
    if (app->font == NULL) {
        app->font = XftFontOpenName(app->display, app->screen,
                                    "monospace:pixelsize=14");
    }
    snprintf(name, sizeof(name), "%s:pixelsize=17:bold", family);
    app->font_bold = XftFontOpenName(app->display, app->screen, name);
    if (app->font_bold == NULL) {
        app->font_bold = app->font;
    }
    if (app->font == NULL) {
        fprintf(stderr, "gnuchanos-installer: cannot open a font; install a "
                        "sans or monospace font.\n");
        exit(1);
    }
    Visual *visual = DefaultVisual(app->display, app->screen);
    Colormap colormap = DefaultColormap(app->display, app->screen);
    app->draw = XftDrawCreate(app->display, app->buffer, visual, colormap);
}

static void load_colours(InstApp *app) {
    memset(&app->colours, 0, sizeof(app->colours));
    inst_load_colour(app, COL_BG,       COL_BG,       &app->colours.background);
    inst_load_colour(app, COL_PANEL,    COL_PANEL,    &app->colours.panel);
    inst_load_colour(app, COL_EDGE,     COL_EDGE,     &app->colours.panel_edge);
    inst_load_colour(app, COL_FIELD,    COL_FIELD,    &app->colours.field);
    inst_load_colour(app, COL_FIELD_ON, COL_FIELD_ON, &app->colours.field_active);
    inst_load_colour(app, COL_TEXT,     COL_TEXT,     &app->colours.text);
    inst_load_colour(app, COL_MUTED,    COL_MUTED,    &app->colours.text_muted);
    inst_load_colour(app, COL_ACCENT,   COL_ACCENT,   &app->colours.accent);
    inst_load_colour(app, COL_DANGER,   COL_DANGER,   &app->colours.danger);
    inst_load_colour(app, COL_GOOD,     COL_GOOD,     &app->colours.good);
}

/* --- the program ----------------------------------------------------------- */

static int run(InstApp *app) {
    int xfd = ConnectionNumber(app->display);

    while (!app->should_close) {
        /* Wait on the X connection and, while an install runs, on its output
           pipe as well. The timeout is short so the loop also notices the
           child exiting; it costs nothing when both are idle. */
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(xfd, &ready);
        int maxfd = xfd;
        int efd = inst_engine_fd(app);
        if (efd >= 0) {
            FD_SET(efd, &ready);
            if (efd > maxfd) {
                maxfd = efd;
            }
        }
        struct timeval timeout = {0, 100 * 1000};
        select(maxfd + 1, &ready, NULL, NULL, &timeout);

        /* the installer's output, if any arrived */
        if (app->running) {
            int before = app->line_count;
            int before_head = app->line_head;
            inst_engine_poll(app);
            if (app->line_count != before || app->line_head != before_head ||
                !app->running) {
                inst_draw(app);
            }
        }

        /* the window's own events */
        while (XPending(app->display) > 0) {
            XEvent event;
            XNextEvent(app->display, &event);
            switch (event.type) {
            case Expose:
                inst_draw(app);
                break;
            case KeyPress:
                handle_key(app, &event.xkey);
                break;
            case MotionNotify:
                inst_pointer_motion(app, event.xmotion.x, event.xmotion.y);
                break;
            case ButtonPress:
                if (app->disk_dropdown_open &&
                    (event.xbutton.button == Button4 ||
                     event.xbutton.button == Button5)) {
                    int max_scroll = app->disk_count - INST_DISK_DROPDOWN_ROWS;
                    if (max_scroll < 0) {
                        max_scroll = 0;
                    }
                    app->disk_dropdown_scroll +=
                        event.xbutton.button == Button4 ? -1 : 1;
                    if (app->disk_dropdown_scroll < 0) {
                        app->disk_dropdown_scroll = 0;
                    } else if (app->disk_dropdown_scroll > max_scroll) {
                        app->disk_dropdown_scroll = max_scroll;
                    }
                    if (app->disk_dropdown_cursor < app->disk_dropdown_scroll) {
                        app->disk_dropdown_cursor = app->disk_dropdown_scroll;
                    } else if (app->disk_dropdown_cursor >=
                               app->disk_dropdown_scroll + INST_DISK_DROPDOWN_ROWS) {
                        app->disk_dropdown_cursor =
                            app->disk_dropdown_scroll + INST_DISK_DROPDOWN_ROWS - 1;
                    }
                    inst_draw(app);
                } else if (event.xbutton.button == Button1) {
                    inst_button_press(app, event.xbutton.x, event.xbutton.y);
                }
                break;
            case ClientMessage:
                if (app->running) {
                    inst_set_note(app, 1,
                                  "Installation is running; wait for it to finish.");
                    inst_draw(app);
                } else {
                    app->should_close = 1;
                }
                break;
            default:
                break;
            }
        }
    }
    return app->exit_code;
}

static void tear_down(InstApp *app) {
    for (int i = 0; i < app->line_count; i++) {
        free(app->lines[(app->line_head + i) % INST_OUTPUT_LINES]);
    }
    inst_free_colours(app);
    XftDrawDestroy(app->draw);
    if (app->font_bold != app->font) {
        XftFontClose(app->display, app->font_bold);
    }
    XftFontClose(app->display, app->font);
    XFreePixmap(app->display, app->buffer);
    XFreeGC(app->display, app->gc);
    XDestroyWindow(app->display, app->window);
    XCloseDisplay(app->display);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanInstall %s\n", GNUCHANOS_INSTALL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: GnuChanInstall [--version]\n"
                   "\n"
                   "Installs the running live system onto a disk. Choose the\n"
                   "disk, the login name and the password; the install runs\n"
                   "and its output is shown in the window.\n");
            return 0;
        }
    }

    setlocale(LC_ALL, "");

    static InstApp app;
    memset(&app, 0, sizeof(app));
    app.engine_fd = -1;
    app.engine_pid = -1;
    app.disk_choice = -1;

    build_fields(&app);
    set_up_window(&app);
    set_up_fonts(&app);
    load_colours(&app);

    inst_list_disks(&app);
    if (app.disk_count > 0) {
        snprintf(app.fields[0].text, INST_FIELD_TEXT, "%s",
                 app.disks[app.disk_choice].description);
        app.fields[0].length = (int)strlen(app.fields[0].text);
    } else {
        inst_set_note(&app, 1, "No disk to install onto was found.");
    }

    inst_layout(&app);
    inst_draw(&app);

    int result = run(&app);
    tear_down(&app);
    return result;
}
