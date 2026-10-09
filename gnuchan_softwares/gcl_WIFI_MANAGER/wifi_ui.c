/*
 * wifi_ui.c — the window, the grab, the keys, and the connecting.
 *
 * This is where the manager meets the display. Everything it is made of has
 * already been read and resolved by the time this runs: the config, the palette,
 * the font, the interface's name. What is left is a window, a grab, and a loop
 * that turns a key into a choice, and a choice into a connection.
 *
 * The grab is the same one the launcher takes, and for the same reason. The
 * keyboard is held so the first letter typed reaches this window and not
 * whatever had the focus before it; the pointer is held so a click anywhere is
 * either on a row or means "go away". The window is override-redirect, so
 * GnuChanWM never frames it, and the input focus is set by hand because there is
 * no window manager going to set it.
 *
 * Connecting happens here rather than in a caller, because this program is the
 * caller: there is no second consumer of "which network was chosen". The shape
 * of it is the only interesting decision — a locked network that is not already
 * saved opens the password screen instead of connecting at once; everything else
 * connects directly. Which is which is read from the network's own flags, and
 * the password screen names the network so there is no doubt what is being
 * joined.
 */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "wifi_draw.h"
#include "wifi_ui.h"

/* Where the window sits, and how tall it is. Centred, the same as the
   launcher: a thing that appears in the middle of a desk is a thing to answer,
   and one that appears in a corner is easy to miss. */
static void work_out_geometry(WifiUi *ui) {
    ui->width = ui->config.width;
    if (ui->width > ui->screen_width) {
        ui->width = ui->screen_width;
    }

    int title = ui->style.row_height + 2 * ui->style.padding;
    int rows = ui->config.rows;
    int status = ui->style.row_height;   /* room for a message at the foot */

    if (ui->mode == WIFI_MODE_PASSWORD) {
        /* The password screen is three lines tall and does not grow with the
           list; the window shrinks to it so the field is not lost in empty
           space. */
        ui->height = title + 3 * ui->style.row_height + 2 * ui->style.padding;
    } else {
        ui->height = title + rows * ui->style.row_height + status +
                     ui->style.padding;
    }

    if (ui->height > ui->screen_height) {
        ui->height = ui->screen_height;
    }

    int x = (ui->screen_width - ui->width) / 2;
    int y = (ui->screen_height - ui->height) / 3;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    /* The size is worked out once BEFORE the window is made — the window is
       created at the right size rather than resized after — so this is called
       while ui->window is still None. XMoveResizeWindow on None is a protocol
       error, so the move is only made once there is a window to move. */
    if (ui->window != None) {
        XMoveResizeWindow(ui->display, ui->window, x, y,
                          (unsigned int)ui->width, (unsigned int)ui->height);
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
        work_out_geometry(ui);
        wifi_draw(ui);
        return;
    }

    connect_current(ui, NULL);
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
            ui->mode = WIFI_MODE_LIST;
            ui->password_length = 0;
            ui->password[0] = '\0';
            ui->status[0] = '\0';
            work_out_geometry(ui);
            wifi_draw(ui);
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

    /* A letter: `r` scans again, `d` disconnects, `q` quits. The letters are
       the whole of the manager's commands, and they are letters rather than
       more key bindings because a window this small is answered with letters. */
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
    } else if (c == 'q' || c == 'Q') {
        ui->running = 0;
    }
}

/* --- the window ----------------------------------------------------------- */

/* Take the keyboard and the pointer for as long as the window is up. The input
   focus is set first, because the window is override-redirect and no window
   manager will focus it; the grabs are retried, because a grab is refused with
   GrabNotViewable while the server is still finishing the map and with
   AlreadyGrabbed while another client holds one for a moment. */
static int grab_input(WifiUi *ui) {
    XSetInputFocus(ui->display, ui->window, RevertToPointerRoot, CurrentTime);
    XSync(ui->display, False);

    int keyboard = GrabNotViewable;
    for (int attempt = 0; attempt < 20; attempt++) {
        XGrabPointer(ui->display, ui->window, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        keyboard = XGrabKeyboard(ui->display, ui->window, False,
                                 GrabModeAsync, GrabModeAsync, CurrentTime);
        if (keyboard == GrabSuccess) {
            return 1;
        }
        usleep(10000);
    }
    fprintf(stderr,
            "gnuchanwifi: the keyboard could not be grabbed (%d); "
            "typing will go to whatever had the focus\n", keyboard);
    return 0;
}

static void ungrab_input(WifiUi *ui) {
    XUngrabKeyboard(ui->display, CurrentTime);
    XUngrabPointer(ui->display, CurrentTime);
}

/* Choose the row a click landed on. A click on the title bar, the status line
   or the padding chooses nothing; a click outside the window dismisses it. */
static void handle_click(WifiUi *ui, XButtonEvent *button) {
    if (button->button != Button1) {
        return;
    }
    if (button->x < 0 || button->x >= ui->width ||
        button->y < 0 || button->y >= ui->height) {
        ui->running = 0;
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

int wifi_ui_open(WifiUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;

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

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = ui->style.background;
    attributes.border_pixel = 0;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                            ButtonReleaseMask | PointerMotionMask;

    ui->mode = WIFI_MODE_LIST;
    work_out_geometry(ui);

    ui->window = XCreateWindow(ui->display, ui->root, 0, 0,
                               (unsigned int)ui->width,
                               (unsigned int)ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel |
                               CWBorderPixel | CWEventMask, &attributes);
    if (ui->window == None) {
        fprintf(stderr, "gnuchanwifi: cannot make the window\n");
        return -1;
    }

    XMapRaised(ui->display, ui->window);
    /* The window has to be on screen before the keyboard is grabbed: XFlush
       only sends the map request, and a grab on an unmapped window is refused
       with GrabNotViewable. XSync waits for the map to have happened. */
    XSync(ui->display, False);

    grab_input(ui);

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
            if (event.xexpose.window == ui->window) {
                wifi_draw(ui);
            }
            break;
        case ConfigureNotify:
            /* The window was resized by something outside — rare, and the
               answer is to take the new size and draw into it. */
            if (event.xconfigure.window == ui->window) {
                ui->width = event.xconfigure.width;
                ui->height = event.xconfigure.height;
                wifi_draw(ui);
            }
            break;
        default:
            break;
        }
    }

    ungrab_input(ui);
    XFlush(ui->display);
    return 0;
}

void wifi_ui_close(WifiUi *ui) {
    if (ui->display) {
        ungrab_input(ui);
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
