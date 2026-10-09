/*
 * wifi_ui.c — the window, the list, the buttons, the keys, and what each does.
 *
 * This is where the manager meets the display. Everything it is made of has
 * already been read by the time this runs: the config, the palette, the
 * interface's name. What is left is a window, a loop, and the actions.
 *
 * --- this is an ORDINARY window ---
 *
 * GnuChanWM owns it: it frames it, moves it, focuses it and closes it. The
 * program names the window, asks for WM_DELETE_WINDOW so the close button sends
 * a message rather than killing the connection, maps it, and answers the events
 * that reach it. It is NOT override-redirect and it does NOT grab the keyboard.
 *
 * --- two ways to act, one function each ---
 *
 * The buttons at the foot and the letters do the same things through the same
 * functions: a button resolves to a WifiAction and dispatch_action() runs it, a
 * letter resolves to the same action and calls the same dispatch. So there is
 * one place where "rescan" is defined and the mouse and the keyboard cannot
 * drift apart.
 *
 * One action is deliberate and so is asked twice: RESTART reloads the wireless
 * driver, which drops the network for a moment. That is the same thing the
 * dotfiles' Reboot_Wifi.py does by hand — rfkill unblock all, modprobe -r, then
 * modprobe — and it is the fix for a card that has latched itself "radio off".
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "wifi_draw.h"
#include "wifi_ui.h"

#define WIFI_EVENT_MASK (KeyPressMask | ButtonPressMask | ButtonReleaseMask | \
                         PointerMotionMask | StructureNotifyMask | \
                         ExposureMask)

/* The height the window needs for the screen it is showing. The list is the
   status bar, an optional warning band, the rows, and the three foot bands —
   message, buttons, help. The sub-screens are a fixed few rows plus the same
   foot. Worked out before anything is drawn so the window opens at its size. */
static void work_out_size(WifiUi *ui) {
    ui->width = ui->config.width;
    if (ui->width > ui->screen_width) {
        ui->width = ui->screen_width;
    }

    int row = ui->style.row_height;
    int banner = (ui->block.hard || ui->block.soft) ? row : 0;
    int foot = 3 * row;   /* message, buttons, help */

    if (ui->mode == WIFI_MODE_LIST) {
        ui->height = row + ui->style.padding + banner +
                     ui->config.rows * row + foot;
    } else {
        ui->height = row + ui->style.padding + banner + 3 * row + foot;
    }

    if (ui->height > ui->screen_height) {
        ui->height = ui->screen_height;
    }
}

/* Resize the window to what the current mode wants, after the mode changed or
   the warning band appeared. */
static void resize_for_mode(WifiUi *ui) {
    work_out_size(ui);
    XResizeWindow(ui->display, ui->window,
                  (unsigned int)ui->width, (unsigned int)ui->height);
}

/* Read everything the window shows: the radio, the kernel's blocks, the
   interface (again, because a restart can change it), the saved profiles, what
   is joined, and the networks. The saved list is read BEFORE the scan, because
   the scan marks each network as saved by looking its SSID up in it. */
static void refresh_all(WifiUi *ui) {
    snprintf(ui->status, sizeof(ui->status), "Scanning…");
    wifi_draw(ui);

    ui->radio_on = wifi_radio_on();
    wifi_rfkill_state(&ui->block);
    wifi_saved_load(&ui->saved);

    char names[WIFI_MAX_SAVED][WIFI_TEXT];
    int name_count = wifi_saved_names(&ui->saved, names, WIFI_MAX_SAVED);

    /* The interface, and its driver's name for the restart action. A config
       that named a module wins only when the kernel gave none. */
    wifi_nm_device(ui->device, sizeof(ui->device));
    if (ui->device[0]) {
        wifi_rfkill_driver(ui->device, ui->driver_module,
                           sizeof(ui->driver_module));
    }
    if (!ui->driver_module[0] && ui->config.restart_module[0]) {
        snprintf(ui->driver_module, sizeof(ui->driver_module), "%s",
                 ui->config.restart_module);
    }

    if (ui->radio_on == 0) {
        /* With the radio off nothing is in range and everything would be stale;
           the list is cleared and the window says why. */
        ui->networks.count = 0;
        ui->active_ssid[0] = '\0';
        ui->active_address[0] = '\0';
        ui->selected = -1;
        ui->scroll = 0;
        ui->status[0] = '\0';
        resize_for_mode(ui);
        wifi_draw(ui);
        return;
    }

    wifi_nm_scan(&ui->networks, names, name_count);
    wifi_nm_active(ui->device, ui->active_ssid, sizeof(ui->active_ssid),
                   ui->active_address, sizeof(ui->active_address));

    if (ui->networks.count == 0) {
        ui->selected = -1;
    } else {
        if (ui->selected < 0) {
            ui->selected = 0;
        }
        if (ui->selected >= ui->networks.count) {
            ui->selected = ui->networks.count - 1;
        }
        for (int i = 0; i < ui->networks.count; i++) {
            if (ui->networks.items[i].in_use) {
                ui->selected = i;
                break;
            }
        }
    }
    ui->scroll = 0;
    ui->status[0] = '\0';
    resize_for_mode(ui);
    wifi_draw(ui);
}

/* Move the chosen row by `step`, wrapping round the ends. */
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
    int room = wifi_visible_rows(ui);
    if (ui->selected < ui->scroll) {
        ui->scroll = ui->selected;
    }
    if (ui->selected >= ui->scroll + room) {
        ui->scroll = ui->selected - room + 1;
    }
}

/* --- the actions ---------------------------------------------------------- */

/* Join the chosen network, with `password` when one was given. */
static void connect_current(WifiUi *ui, const char *password) {
    if (ui->selected < 0 || ui->selected >= ui->networks.count) {
        return;
    }
    char ssid[WIFI_TEXT];
    snprintf(ssid, sizeof(ssid), "%s", ui->networks.items[ui->selected].ssid);

    snprintf(ui->status, sizeof(ui->status), "Connecting to \"%s\"…", ssid);
    wifi_draw(ui);

    char error[WIFI_TEXT];
    if (wifi_nm_connect(ui->device, ssid, password, error, sizeof(error)) == 0) {
        ui->mode = WIFI_MODE_LIST;
        ui->password_length = 0;
        ui->password[0] = '\0';
        refresh_all(ui);
        snprintf(ui->status, sizeof(ui->status), "Connected to \"%s\"", ssid);
        wifi_draw(ui);
        return;
    }

    snprintf(ui->status, sizeof(ui->status), "%s", error);
    wifi_draw(ui);
}

/* Enter on the chosen row. A locked network that is not saved is one whose
   password is not known, so it asks for it; anything else connects with what
   the keyring already has. */
static void choose_selected(WifiUi *ui) {
    if (ui->selected < 0 || ui->selected >= ui->networks.count) {
        return;
    }
    WifiNetwork *network = &ui->networks.items[ui->selected];

    if (network->secured && !network->saved && !network->in_use) {
        snprintf(ui->pending_ssid, sizeof(ui->pending_ssid), "%s",
                 network->ssid);
        ui->password_length = 0;
        ui->password[0] = '\0';
        ui->status[0] = '\0';
        ui->mode = WIFI_MODE_PASSWORD;
        resize_for_mode(ui);
        wifi_draw(ui);
        return;
    }

    connect_current(ui, NULL);
}

/* Go back to the list from a sub-screen. */
static void leave_sub_screen(WifiUi *ui) {
    ui->mode = WIFI_MODE_LIST;
    ui->password_length = 0;
    ui->password[0] = '\0';
    ui->status[0] = '\0';
    resize_for_mode(ui);
    wifi_draw(ui);
}

/* The profile name for the chosen network, if it is saved, or an empty string.
   For a simple network the profile name and the SSID are the same. */
static const char *chosen_saved_name(WifiUi *ui) {
    if (ui->selected < 0 || ui->selected >= ui->networks.count) {
        return "";
    }
    const char *ssid = ui->networks.items[ui->selected].ssid;
    for (int i = 0; i < ui->saved.count; i++) {
        if (strcmp(ui->saved.names[i], ssid) == 0) {
            return ui->saved.names[i];
        }
    }
    return "";
}

static void ask_forget(WifiUi *ui) {
    const char *name = chosen_saved_name(ui);
    if (!name[0]) {
        snprintf(ui->status, sizeof(ui->status),
                 "That network is not saved, so there is nothing to forget");
        wifi_draw(ui);
        return;
    }
    snprintf(ui->pending_saved, sizeof(ui->pending_saved), "%s", name);
    ui->mode = WIFI_MODE_CONFIRM;
    ui->status[0] = '\0';
    resize_for_mode(ui);
    wifi_draw(ui);
}

static void confirm_forget(WifiUi *ui) {
    char error[WIFI_TEXT];
    if (wifi_saved_forget(ui->pending_saved, error, sizeof(error)) == 0) {
        ui->mode = WIFI_MODE_LIST;
        refresh_all(ui);
        snprintf(ui->status, sizeof(ui->status), "Forgot \"%s\"",
                 ui->pending_saved);
        wifi_draw(ui);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "%s", error);
    ui->mode = WIFI_MODE_LIST;
    resize_for_mode(ui);
    wifi_draw(ui);
}

static void toggle_autoconnect(WifiUi *ui) {
    const char *name = chosen_saved_name(ui);
    if (!name[0]) {
        snprintf(ui->status, sizeof(ui->status),
                 "That network is not saved, so it has no such setting");
        wifi_draw(ui);
        return;
    }

    int current = 0;
    for (int i = 0; i < ui->saved.count; i++) {
        if (strcmp(ui->saved.names[i], name) == 0) {
            current = ui->saved.autoconnect[i];
            break;
        }
    }
    int wanted = !current;

    char error[WIFI_TEXT];
    if (wifi_saved_set_autoconnect(name, wanted, error, sizeof(error)) == 0) {
        refresh_all(ui);
        snprintf(ui->status, sizeof(ui->status),
                 "\"%s\" will %s join by itself", name,
                 wanted ? "now" : "no longer");
        wifi_draw(ui);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "%s", error);
    wifi_draw(ui);
}

static void toggle_radio(WifiUi *ui) {
    if (ui->radio_on < 0) {
        snprintf(ui->status, sizeof(ui->status),
                 "The wifi state could not be read, so it cannot be changed");
        wifi_draw(ui);
        return;
    }
    int wanted = !ui->radio_on;

    snprintf(ui->status, sizeof(ui->status), "Turning wifi %s…",
             wanted ? "on" : "off");
    wifi_draw(ui);

    char error[WIFI_TEXT];
    if (wifi_radio_set(wanted, error, sizeof(error)) == 0) {
        refresh_all(ui);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "%s", error);
    wifi_draw(ui);
}

static void do_disconnect(WifiUi *ui) {
    char error[WIFI_TEXT];
    snprintf(ui->status, sizeof(ui->status), "Disconnecting…");
    wifi_draw(ui);
    if (wifi_nm_disconnect(ui->device, error, sizeof(error)) == 0) {
        refresh_all(ui);
    } else {
        snprintf(ui->status, sizeof(ui->status), "%s", error);
        wifi_draw(ui);
    }
}

/* Ask before the restart: it drops the network for a moment. */
static void ask_restart(WifiUi *ui) {
    ui->mode = WIFI_MODE_RESTART;
    ui->status[0] = '\0';
    resize_for_mode(ui);
    wifi_draw(ui);
}

/* y in the restart screen: clear the blocks and reload the driver. This is the
   whole of Reboot_Wifi.py. The network drops while it happens and comes back
   with the rescan that follows. */
static void do_restart(WifiUi *ui) {
    ui->mode = WIFI_MODE_LIST;
    snprintf(ui->status, sizeof(ui->status),
             "Restarting the wifi driver…");
    resize_for_mode(ui);
    wifi_draw(ui);

    char error[WIFI_TEXT];
    if (wifi_rfkill_restart(ui->driver_module, error, sizeof(error)) == 0) {
        /* Give the kernel a moment to bring the interface back before reading
           it, so the scan is not run against a device that is still coming
           up. */
        refresh_all(ui);
        snprintf(ui->status, sizeof(ui->status),
                 "The wifi driver was restarted");
        wifi_draw(ui);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "%s", error);
    resize_for_mode(ui);
    wifi_draw(ui);
}

/* Run the action a button — or a letter, resolved to the same value — stands
   for. The one place the actions are named, so the mouse and the keyboard do
   not drift apart. */
static void dispatch_action(WifiUi *ui, WifiAction action) {
    switch (action) {
    case WIFI_ACTION_CONNECT:
        choose_selected(ui);
        break;
    case WIFI_ACTION_RESCAN:
        refresh_all(ui);
        break;
    case WIFI_ACTION_DISCONNECT:
        do_disconnect(ui);
        break;
    case WIFI_ACTION_FORGET:
        ask_forget(ui);
        break;
    case WIFI_ACTION_AUTOCONNECT:
        toggle_autoconnect(ui);
        break;
    case WIFI_ACTION_RESTART:
        ask_restart(ui);
        break;
    case WIFI_ACTION_QUIT:
        ui->running = 0;
        break;
    default:
        break;
    }
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

static void handle_password_key(WifiUi *ui, KeySym symbol, XKeyEvent *key) {
    switch (symbol) {
    case XK_Escape:
        leave_sub_screen(ui);
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
}

static void handle_confirm_key(WifiUi *ui, KeySym symbol) {
    if (symbol == XK_Escape || symbol == XK_n || symbol == XK_N) {
        leave_sub_screen(ui);
        return;
    }
    if (symbol == XK_y || symbol == XK_Y) {
        confirm_forget(ui);
    }
}

static void handle_restart_key(WifiUi *ui, KeySym symbol) {
    if (symbol == XK_Escape || symbol == XK_n || symbol == XK_N) {
        leave_sub_screen(ui);
        return;
    }
    if (symbol == XK_y || symbol == XK_Y) {
        do_restart(ui);
    }
}

static void handle_list_key(WifiUi *ui, KeySym symbol) {
    switch (symbol) {
    case XK_Escape:
    case XK_q:
    case XK_Q:
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
        move_selection(ui, -wifi_visible_rows(ui));
        wifi_draw(ui);
        return;
    case XK_Page_Down:
        move_selection(ui, wifi_visible_rows(ui));
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
    case XK_r:
        dispatch_action(ui, WIFI_ACTION_RESCAN);
        return;
    case XK_w:
        toggle_radio(ui);
        return;
    case XK_d:
        dispatch_action(ui, WIFI_ACTION_DISCONNECT);
        return;
    case XK_f:
        dispatch_action(ui, WIFI_ACTION_FORGET);
        return;
    case XK_a:
        dispatch_action(ui, WIFI_ACTION_AUTOCONNECT);
        return;
    case XK_R:
        dispatch_action(ui, WIFI_ACTION_RESTART);
        return;
    default:
        break;
    }
}

static void handle_key(WifiUi *ui, XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);
    if (symbol == NoSymbol) {
        return;
    }

    if (ui->mode == WIFI_MODE_PASSWORD) {
        handle_password_key(ui, symbol, key);
    } else if (ui->mode == WIFI_MODE_CONFIRM) {
        handle_confirm_key(ui, symbol);
    } else if (ui->mode == WIFI_MODE_RESTART) {
        handle_restart_key(ui, symbol);
    } else {
        handle_list_key(ui, symbol);
    }
}

/* A click: first the buttons, then a row of the list. The buttons come first
   because they sit below the list and a click on one must not also move the
   selection. */
static void handle_click(WifiUi *ui, XButtonEvent *button) {
    if (button->button != Button1 || ui->mode != WIFI_MODE_LIST) {
        return;
    }

    for (int i = 0; i < ui->button_count; i++) {
        WifiButton *candidate = &ui->buttons[i];
        if (button->x >= candidate->x &&
            button->x < candidate->x + candidate->width &&
            button->y >= candidate->y &&
            button->y < candidate->y + candidate->height) {
            dispatch_action(ui, candidate->action);
            return;
        }
    }

    int top = ui->style.row_height + ui->style.padding +
              ((ui->block.hard || ui->block.soft) ? ui->style.row_height : 0);
    if (button->y < top) {
        return;
    }
    int row = (button->y - top) / ui->style.row_height;
    int index = ui->scroll + row;
    if (index < 0 || index >= ui->networks.count) {
        return;
    }
    /* A click only CHOOSES the row — it moves the highlight and nothing else.
       Joining is a second, deliberate act: the Connect button or Enter. A click
       that connected as well would make browsing the list a series of
       disconnects and reconnects, which is how this first behaved and what
       "why can I not just pick one" means. */
    ui->selected = index;
    wifi_draw(ui);
}

/* --- the window ----------------------------------------------------------- */

int wifi_ui_open(WifiUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;
    ui->mode = WIFI_MODE_LIST;
    ui->radio_on = -1;

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

    char found_path[WIFI_TEXT * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = wifi_config_path(found_path, sizeof(found_path));
    }
    if (wifi_config_load(&ui->config, path) != 0) {
        fprintf(stderr, "gnuchanwifi: %s; using the defaults\n",
                ui->config.error);
    }

    wifi_shell_set_program(ui->config.nmcli);
    wifi_style_load(&ui->style, ui->display, ui->screen, &ui->config);
    wifi_nm_device(ui->device, sizeof(ui->device));
    wifi_rfkill_state(&ui->block);

    ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
    if (!ui->gc) {
        fprintf(stderr, "gnuchanwifi: cannot make a graphics context\n");
        return -1;
    }

    work_out_size(ui);

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

    XStoreName(ui->display, ui->window, ui->config.title);

    XClassHint class_hint;
    class_hint.res_name = (char *)"gnuchanwifi";
    class_hint.res_class = (char *)"GnuChanWifi";
    XSetClassHint(ui->display, ui->window, &class_hint);

    Atom wm_delete = XInternAtom(ui->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ui->display, ui->window, &wm_delete, 1);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);

    refresh_all(ui);

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
            if ((Atom)event.xclient.data.l[0] == wm_delete) {
                ui->running = 0;
            }
            break;
        case DestroyNotify:
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
