/*
 * wifi_ui.c — the window, the list, the keys, and the connecting.
 *
 * This is where the manager meets the display. Everything it is made of has
 * already been read and resolved by the time this runs: the config, the palette,
 * the font, the interface's name. What is left is a window, a loop, and the
 * connecting.
 *
 * --- this is an ORDINARY window, and that is the point ---
 *
 * GnuChanWM, the window manager on this desktop, OWNS this window: it frames it,
 * moves it, focuses it, puts it in the taskbar and lets its title bar's close
 * button end it. The program does the three things a normal X client does and no
 * more — it names the window (WM_NAME and WM_CLASS, so the manager has a title
 * to draw and a class to match rules against), it asks for WM_DELETE_WINDOW (so
 * the close button sends a polite message instead of killing the connection),
 * and it maps the window and answers the events that reach it.
 *
 * It is NOT an override-redirect window and it does NOT grab the keyboard. A
 * launcher grabs the keyboard and covers the screen because it appears, is
 * answered, and goes away; a wifi window is an application a person leaves open
 * while they read a manual or copy a password from somewhere else. Grabbing the
 * keyboard would take every key from the whole session for as long as it was up,
 * which is exactly wrong for an application.
 *
 * --- the connecting lives here ---
 *
 * There is no second consumer of "which network was chosen", so the joining
 * happens in this file rather than being handed to a caller. A locked network
 * that is not already saved opens the password screen instead of connecting at
 * once; everything else connects directly. Which is which is read from the
 * network's own flags, and the password screen names the network so there is no
 * doubt what is being joined.
 */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "wifi_draw.h"
#include "wifi_ui.h"

/* The event mask the window subscribes to. StructureNotify is what a resize and
   the manager's close arrive through; ExposureMask is what asks for a repaint;
   the key, button and motion masks are the input the list is answered with.
   FocusChange is not asked for: this window does not change how it draws when
   it loses the focus — it is always drawn the same — so the notice would be
   read and thrown away. */
#define WIFI_EVENT_MASK (KeyPressMask | ButtonPressMask | ButtonReleaseMask | \
                         PointerMotionMask | StructureNotifyMask | \
                         ExposureMask)

/* The size the window needs for the screen it is showing: the title bar, the
   rows (or the password prompt), and the status line. Kept apart from any move
   so it can be worked out before the window exists — the window is created at
   the right size rather than resized after, which shows as an app that opens
   small and jumps. */
static void work_out_size(WifiUi *ui) {
    ui->width = ui->config.width;
    if (ui->width > ui->screen_width) {
        ui->width = ui->screen_width;
    }

    int title = ui->style.row_height + 2 * ui->style.padding;
    int status = ui->style.row_height;

    if (ui->mode == WIFI_MODE_PASSWORD) {
        /* The password screen is three lines tall and does not grow with the
           list; the window shrinks to it so the field is not lost in empty
           space. */
        ui->height = title + 3 * ui->style.row_height + 2 * ui->style.padding;
    } else {
        ui->height = title + ui->config.rows * ui->style.row_height + status;
    }

    if (ui->height > ui->screen_height) {
        ui->height = ui->screen_height;
    }
}

/* Take a fresh scan and put the list back at a sane place. The chosen row is
   kept only when it still points at something; a scan that shrank the list
   must not leave the selection past its end. */
static void refresh_scan(WifiUi *ui) {
    snprintf(ui->status, sizeof(ui->status), "Scanning…");
    wifi_draw(ui);

    wifi_nm_scan(&ui->networks);

    if (ui->networks.count == 0) {
        ui->selected = -1;
    } else {
        if (ui->selected < 0) {
            ui->selected = 0;
        }
        if (ui->selected >= ui->networks.count) {
            ui->selected = ui->networks.count - 1;
        }
        /* Put the chosen row on the joined network the first time, so Enter
           with nothing touched means "reconnect to where I am" rather than
           "join the first access point" — which is what a person opening a
           wifi window usually has in mind. */
        for (int i = 0; i < ui->networks.count; i++) {
            if (ui->networks.items[i].in_use) {
                ui->selected = i;
                break;
            }
        }
    }
    ui->scroll = 0;
    ui->status[0] = '\0';
    wifi_draw(ui);
}

/* How many rows fit in the window, which is what the selection is kept inside. */
static int visible_rows(const WifiUi *ui) {
    int title = ui->style.row_height + 2 * ui->style.padding;
    int status = ui->style.row_height;
    int room = (ui->height - title - status) / ui->style.row_height;
    if (room < 1) {
        room = 1;
    }
    return room;
}

/* Move the chosen row by `step`, wrapping round the ends: the list is short and
   the two ends are next to each other in a person's mind. */
static void move_selection(WifiUi *ui, int step) {
    int rows = ui->networks.count;
    if (rows <= 0) {
        ui->selected = -1;
        return;
    }
    if (ui->selected < 0) {
        ui->selected = 0;
    } else {
        ui->selected += step;
        while (ui->selected < 0) {
            ui->selected += rows;
        }
        while (ui->selected >= rows) {
            ui->selected -= rows;
        }
    }
    int room = visible_rows(ui);
    if (ui->selected < ui->scroll) {
        ui->scroll = ui->selected;
    }
    if (ui->selected >= ui->scroll + room) {
        ui->scroll = ui->selected - room + 1;
    }
}

/* --- connecting ----------------------------------------------------------- */

/* Join the chosen network, with `password` when one was given. On success the
   list is taken again so the joined network shows as connected; on failure the
   message nmcli gave back is put on the status line and the password screen —
   when that is where we are — is left up so the password can be corrected
   rather than retyped from the start. */
static void connect_current(WifiUi *ui, const char *password) {
    if (ui->selected < 0 || ui->selected >= ui->networks.count) {
        return;
    }
    const char *ssid = ui->networks.items[ui->selected].ssid;

    snprintf(ui->status, sizeof(ui->status), "Connecting to \"%s\"…", ssid);
    wifi_draw(ui);

    char error[WIFI_TEXT];
    if (wifi_nm_connect(ui->device, ssid, password, error, sizeof(error)) == 0) {
        ui->mode = WIFI_MODE_LIST;
        refresh_scan(ui);
        snprintf(ui->status, sizeof(ui->status), "Connected to \"%s\"", ssid);
        wifi_draw(ui);
        return;
    }

    snprintf(ui->status, sizeof(ui->status), "%s", error);
    wifi_draw(ui);
}

/* Enter on the chosen row. A locked network that is not joined is one whose
   password is not saved, so it asks for the password; an open one, or one
   already joined, connects directly — the key is in the keyring and asking for
   it again would be asking for something the machine already has. */
static void choose_selected(WifiUi *ui) {
    if (ui->selected < 0 || ui->selected >= ui->networks.count) {
        return;
    }
    WifiNetwork *network = &ui->networks.items[ui->selected];

    if (network->secured && !network->in_use) {
        snprintf(ui->pending_ssid, sizeof(ui->pending_ssid), "%s",
                 network->ssid);
        ui->password_length = 0;
        ui->password[0] = '\0';
        ui->status[0] = '\0';
        ui->mode = WIFI_MODE_PASSWORD;
        work_out_size(ui);
        XResizeWindow(ui->display, ui->window,
                      (unsigned int)ui->width, (unsigned int)ui->height);
        wifi_draw(ui);
        return;
    }

    connect_current(ui, NULL);
}

/* Go back to the list from the password screen, and shrink the window to it. */
static void leave_password_screen(WifiUi *ui) {
    ui->mode = WIFI_MODE_LIST;
    ui->password_length = 0;
    ui->password[0] = '\0';
    ui->status[0] = '\0';
    work_out_size(ui);
    XResizeWindow(ui->display, ui->window,
                  (unsigned int)ui->width, (unsigned int)ui->height);
    wifi_draw(ui);
}

/* --- the password line ---------------------------------------------------- */

static int password_append(WifiUi *ui, const char *text) {
    if (!text || !text[0]) {
        return 0;
    }
    size_t length = strlen(text);
    if (ui->password_length + (int)length >= WIFI_MAX_PASSWORD) {
        return 0;
    }
    memcpy(ui->password + ui->password_length, text, length);
    ui->password_length += (int)length;
    ui->password[ui->password_length] = '\0';
    return 1;
}

static int password_backspace(WifiUi *ui) {
    if (ui->password_length == 0) {
        return 0;
    }
    ui->password_length--;
    ui->password[ui->password_length] = '\0';
    return 1;
}

/* --- the keys ------------------------------------------------------------- */

static void handle_key(WifiUi *ui, XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);
    if (symbol == NoSymbol) {
        return;
    }

    if (ui->mode == WIFI_MODE_PASSWORD) {
        switch (symbol) {
        case XK_Escape:
            /* Back to the list with nothing joined — the choice is abandoned,
               not the program. */
            leave_password_screen(ui);
            return;
        case XK_Return:
        case XK_KP_Enter:
            connect_current(ui, ui->password);
            return;
        case XK_BackSpace:
            if (password_backspace(ui)) {
                wifi_draw(ui);
            }
            return;
        default:
            break;
        }

        char text[8];
        int length = XLookupString(key, text, sizeof(text) - 1, NULL, NULL);
        if (length <= 0) {
            return;
        }
        text[length] = '\0';
        /* Only printable characters: a control character has no place in a
           password and would be an invisible byte to delete later. */
        for (int i = 0; i < length; i++) {
            unsigned char c = (unsigned char)text[i];
            if (c < 0x20 || c == 0x7f) {
                return;
            }
        }
        if (password_append(ui, text)) {
            ui->status[0] = '\0';
            wifi_draw(ui);
        }
        return;
    }

    /* --- the list --- */
    switch (symbol) {
    case XK_Escape:
        ui->running = 0;
        return;
    case XK_Return:
    case XK_KP_Enter:
        choose_selected(ui);
        return;
    case XK_Up:
    case XK_KP_Up:
        move_selection(ui, -1);
        wifi_draw(ui);
        return;
    case XK_Down:
    case XK_KP_Down:
        move_selection(ui, 1);
        wifi_draw(ui);
        return;
    case XK_Page_Up:
        move_selection(ui, -visible_rows(ui));
        wifi_draw(ui);
        return;
    case XK_Page_Down:
        move_selection(ui, visible_rows(ui));
        wifi_draw(ui);
        return;
    case XK_Home:
        ui->selected = 0;
        move_selection(ui, 0);
        wifi_draw(ui);
        return;
    case XK_End:
        ui->selected = ui->networks.count - 1;
        move_selection(ui, 0);
        wifi_draw(ui);
        return;
    default:
        break;
    }

    /* A letter: `r` scans again, `d` disconnects. The letters are the whole of
       the manager's commands, and they are letters rather than more key
       bindings because a window this small is answered with letters. */
    char text[8];
    int length = XLookupString(key, text, sizeof(text) - 1, NULL, NULL);
    if (length <= 0) {
        return;
    }
    unsigned char c = (unsigned char)text[0];
    if (c == 'r' || c == 'R') {
        refresh_scan(ui);
    } else if (c == 'd' || c == 'D') {
        char error[WIFI_TEXT];
        snprintf(ui->status, sizeof(ui->status), "Disconnecting…");
        wifi_draw(ui);
        if (wifi_nm_disconnect(ui->device, error, sizeof(error)) == 0) {
            refresh_scan(ui);
        } else {
            snprintf(ui->status, sizeof(ui->status), "%s", error);
            wifi_draw(ui);
        }
    }
}

/* Choose the row a click landed on. A click on the title bar or the status line
   chooses nothing; everything else is a row. */
static void handle_click(WifiUi *ui, XButtonEvent *button) {
    if (button->button != Button1) {
        return;
    }
    if (ui->mode != WIFI_MODE_LIST) {
        return;   /* the password screen is typed into, not clicked */
    }

    int title = ui->style.row_height + 2 * ui->style.padding;
    if (button->y < title) {
        return;
    }
    int status = ui->style.row_height;
    if (button->y >= ui->height - status) {
        return;
    }
    int row = (button->y - title) / ui->style.row_height;
    int index = ui->scroll + row;
    if (index < 0 || index >= ui->networks.count) {
        return;
    }
    ui->selected = index;
    choose_selected(ui);
}

/* --- the window ----------------------------------------------------------- */

int wifi_ui_open(WifiUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;
    ui->mode = WIFI_MODE_LIST;

    /* The locale first, so XLookupString returns the characters the keyboard
       actually types rather than assuming Latin-1. */
    setlocale(LC_ALL, "");

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "gnuchanwifi: cannot open the X display\n");
        return -1;
    }
    ui->screen = DefaultScreen(ui->display);
    ui->root = RootWindow(ui->display, ui->screen);
    ui->screen_width = DisplayWidth(ui->display, ui->screen);
    ui->screen_height = DisplayHeight(ui->display, ui->screen);

    /* The settings, then the palette, then the interface. In that order: the
       palette is built from the config's colour names, and the interface name
       is asked for once and kept. */
    char found_path[WIFI_TEXT * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = wifi_config_path(found_path, sizeof(found_path));
    }
    if (wifi_config_load(&ui->config, path) != 0) {
        /* A settings file that could not be read is not fatal: the defaults
           are already in place — wifi_config_load lays them down first — and
           the manager starts with them rather than not starting. */
        fprintf(stderr, "gnuchanwifi: %s; using the defaults\n",
                ui->config.error);
    }

    wifi_nm_set_program(ui->config.nmcli);
    wifi_style_load(&ui->style, ui->display, ui->screen, &ui->config);
    wifi_nm_device(ui->device, sizeof(ui->device));

    ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
    if (!ui->gc) {
        fprintf(stderr, "gnuchanwifi: cannot make a graphics context\n");
        return -1;
    }

    /* The size is worked out before the window is made, so the window opens at
       the size it will be rather than opening small and resizing. */
    work_out_size(ui);

    /* THE WINDOW KEEPS ITS OWN PIXELS.
     *
     * A plain window that gets covered keeps nothing: when the covering window
     * goes away, the server has no memory of what was under it and fills the
     * newly exposed part with the window's background until the program draws
     * it again. BackingStore Always makes the server keep the window's pixels
     * off-screen, so an uncover restores what was there; and the background is
     * the theme's own colour rather than black, so even the instant before the
     * first frame is the window's colour and not a hole. This is the same pair
     * of choices the terminal makes, for the same reason. */
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.background_pixel = ui->style.background;
    attributes.backing_store = Always;
    attributes.border_pixel = BlackPixel(ui->display, ui->screen);
    attributes.event_mask = WIFI_EVENT_MASK;

    ui->window = XCreateWindow(ui->display, ui->root, 0, 0,
                               (unsigned int)ui->width,
                               (unsigned int)ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWBackingStore | CWBorderPixel |
                               CWEventMask, &attributes);
    if (ui->window == None) {
        fprintf(stderr, "gnuchanwifi: cannot make the window\n");
        return -1;
    }

    /* The window's X name, which becomes WM_NAME — the title bar text and what
       a task list or dock shows. */
    XStoreName(ui->display, ui->window, ui->config.title);

    /* The class, so the window manager can match its rules against this program
       rather than guessing from the size: res_name is the program, res_class
       the application. This is the same pair the terminal sets. */
    XClassHint class_hint;
    class_hint.res_name = (char *)"gnuchanwifi";
    class_hint.res_class = (char *)"GnuChanWifi";
    XSetClassHint(ui->display, ui->window, &class_hint);

    /* Ask for WM_DELETE_WINDOW: the title bar's close button then sends this
       window a message asking it to quit, which the loop answers, rather than
       killing the X connection out from under the program. */
    Atom wm_delete = XInternAtom(ui->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ui->display, ui->window, &wm_delete, 1);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);

    /* The first scan is taken after the window is up, so "Scanning…" is what
       the window shows while nmcli answers, rather than a blank window that
       looks broken for the second or two a scan takes. */
    ui->selected = -1;
    refresh_scan(ui);

    ui->running = 1;
    wifi_draw(ui);
    return 0;
}

int wifi_ui_run(WifiUi *ui) {
    if (!ui->display || ui->window == None) {
        return -1;
    }

    Atom wm_delete = XInternAtom(ui->display, "WM_DELETE_WINDOW", False);

    while (ui->running) {
        XEvent event;
        XNextEvent(ui->display, &event);

        switch (event.type) {
        case KeyPress:
            handle_key(ui, &event.xkey);
            break;

        case ButtonPress:
            handle_click(ui, &event.xbutton);
            break;

        case Expose:
            if (event.xexpose.window == ui->window &&
                event.xexpose.count == 0) {
                wifi_draw(ui);
            }
            break;

        case ConfigureNotify:
            /* The window manager resized or moved the window — a maximise, a
               drag, the manager placing it. The new size is taken and drawn
               into; the size the grid is built for follows it. */
            if (event.xconfigure.window == ui->window) {
                if (event.xconfigure.width != ui->width ||
                    event.xconfigure.height != ui->height) {
                    ui->width = event.xconfigure.width;
                    ui->height = event.xconfigure.height;
                    wifi_draw(ui);
                }
            }
            break;

        case ClientMessage:
            /* The title bar's close button, or the session asking this program
               to quit. Both arrive as the WM_DELETE_WINDOW message this window
               asked for; the loop ends and the program closes cleanly. */
            if ((Atom)event.xclient.data.l[0] == wm_delete) {
                ui->running = 0;
            }
            break;

        case DestroyNotify:
            /* The window was destroyed by something else — the manager going
               away — and there is nothing left to draw into. */
            if (event.xdestroywindow.window == ui->window) {
                ui->running = 0;
            }
            break;

        default:
            break;
        }
    }

    XFlush(ui->display);
    return 0;
}

void wifi_ui_close(WifiUi *ui) {
    if (ui->display) {
        if (ui->gc) {
            XFreeGC(ui->display, ui->gc);
            ui->gc = NULL;
        }
        if (ui->window != None) {
            XDestroyWindow(ui->display, ui->window);
            ui->window = None;
        }
        wifi_style_free(&ui->style, ui->display);
        XCloseDisplay(ui->display);
        ui->display = NULL;
    }
}
