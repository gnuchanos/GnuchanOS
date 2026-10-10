/*
 * net_ui.c — the window, the panels, the keys, and what each does.
 *
 * This is where the manager meets the display. Everything it is made of has
 * already been read by the time this runs: the config, the palette, the
 * interfaces, the resolver. What is left is a window, a loop, and the actions.
 *
 * --- the actions ---
 *
 * A button and a key run the same function: dispatch_action() is the one place
 * each is named, so the mouse and the keyboard cannot drift apart.
 *
 *   Up/Down   brings the chosen interface up or down with `ip link set`.
 *   Open Wi-Fi starts the wifi manager — the program that knows how to JOIN a
 *             wireless network. This program shows the adapter; that one joins
 *             the network. They are two programs because they are two jobs.
 *   DNS       shows the resolver panel.
 *   Refresh   reads the interfaces again.
 *   Apply     sets the resolver to what is typed.
 *   Revert    puts the resolver back to automatic.
 *
 * A click on a row only CHOOSES it; it never acts. Acting is the button or
 * Enter, a second and deliberate act, so browsing is not a series of changes.
 */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "net_draw.h"
#include "net_ui.h"

#define NET_EVENT_MASK (KeyPressMask | ButtonPressMask | ButtonReleaseMask | \
                        PointerMotionMask | StructureNotifyMask | \
                        ExposureMask)

/* --- running the wifi manager -------------------------------------------- */

/* Start a program by command line, in its own process, without waiting. The
   line is split on spaces — enough for "GnuChanWifi" and "GnuChanWifi --foo",
   which is all a config names here. The child is detached: the manager does not
   wait for a window it did not open. */
static void spawn_command(const char *command) {
    if (!command || !command[0]) {
        return;
    }

    char line[NET_TEXT * 2];
    snprintf(line, sizeof(line), "%s", command);

    char *argv[32];
    int argc = 0;
    char *save = line;
    while (save && *save && argc < 31) {
        while (*save == ' ' || *save == '\t') {
            save++;
        }
        if (!*save) {
            break;
        }
        argv[argc++] = save;
        while (*save && *save != ' ' && *save != '\t') {
            save++;
        }
        if (*save) {
            *save++ = '\0';
        }
    }
    argv[argc] = NULL;
    if (argc == 0) {
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    /* The zombie is reaped here rather than waited for: the manager keeps
       running while the wifi window is open, so it cannot block on it. */
    if (pid > 0) {
        /* SIGCHLD default action is to reap children only when the parent
           waits; a non-blocking wait here catches the immediate failures and
           leaves the long-running window to be reaped when it exits. */
        int status = 0;
        waitpid(pid, &status, WNOHANG);
    }
}

/* --- sizing --------------------------------------------------------------- */

static void work_out_size(NetUi *ui) {
    ui->width = ui->config.width;
    if (ui->width > ui->screen_width) {
        ui->width = ui->screen_width;
    }

    int row = ui->style.row_height;
    int body_rows;

    if (ui->mode == NET_MODE_DNS) {
        /* The "Current DNS" header and one line of servers; the IPv4 heading
           and its two box rows; the IPv6 heading and its two box rows; and the
           hint at the foot — nine rows, plus air for the small gaps drawn
           between the groups. */
        body_rows = 10;
    } else {
        body_rows = ui->config.running_rows;
    }

    ui->height = row + ui->style.padding + body_rows * row + 3 * row;
    if (ui->height > ui->screen_height) {
        ui->height = ui->screen_height;
    }
}

static void resize_for_mode(NetUi *ui) {
    work_out_size(ui);
    XResizeWindow(ui->display, ui->window,
                  (unsigned int)ui->width, (unsigned int)ui->height);
}

/* --- reading the state ---------------------------------------------------- */

/* Read the interfaces, and the resolver for the chosen link (or the global one
   when nothing is chosen). Called at open and after an action. */
static void refresh_devices(NetUi *ui) {
    net_device_list(&ui->devices, &ui->config);

    if (ui->devices.count == 0) {
        ui->selected = -1;
    } else {
        if (ui->selected < 0) {
            ui->selected = 0;
        }
        if (ui->selected >= ui->devices.count) {
            ui->selected = ui->devices.count - 1;
        }
    }
    ui->scroll = 0;
    ui->hover = -1;
}

/* Whether an address is IPv6, told by the colon every IPv6 address has and no
   IPv4 one does. That is the whole test the four boxes need: the values on the
   left belong in the IPv4 group and the ones on the right in the IPv6 group. */
static int is_ipv6_address(const char *address) {
    return address && strchr(address, ':') != NULL;
}

/* Fill the DNS panel from what the system reports, sorted into the four boxes:
   the first IPv4 server into IPv4 Preferred, the second into IPv4 Alternate,
   and the IPv6 ones the same way. The link the servers are read for is the
   chosen interface's, so a per-link server set shows for the link it belongs
   to. */
static void load_dns(NetUi *ui) {
    const char *link = "";
    if (ui->selected >= 0 && ui->selected < ui->devices.count) {
        link = ui->devices.items[ui->selected].name;
    }
    net_dns_load(&ui->dns, &ui->config, link);

    ui->dns_v4_pref[0] = '\0';
    ui->dns_v4_alt[0] = '\0';
    ui->dns_v6_pref[0] = '\0';
    ui->dns_v6_alt[0] = '\0';

    int v4_seen = 0;
    int v6_seen = 0;
    for (int i = 0; i < ui->dns.count; i++) {
        const char *server = ui->dns.servers[i];
        if (is_ipv6_address(server)) {
            if (v6_seen == 0) {
                snprintf(ui->dns_v6_pref, sizeof(ui->dns_v6_pref), "%s",
                         server);
                v6_seen = 1;
            } else if (v6_seen == 1) {
                snprintf(ui->dns_v6_alt, sizeof(ui->dns_v6_alt), "%s", server);
                v6_seen = 2;
            }
        } else {
            if (v4_seen == 0) {
                snprintf(ui->dns_v4_pref, sizeof(ui->dns_v4_pref), "%s",
                         server);
                v4_seen = 1;
            } else if (v4_seen == 1) {
                snprintf(ui->dns_v4_alt, sizeof(ui->dns_v4_alt), "%s", server);
                v4_seen = 2;
            }
        }
    }
    ui->dns_focus = 0;
}

/* --- the four DNS boxes ---------------------------------------------------
 *
 * The shape a Windows adapter's own DNS dialog has: IPv4 and IPv6 entries,
 * each with a Preferred and an Alternate. Four boxes, because that is the four
 * values a person is handed when they look up "Google DNS" — 8.8.8.8, 8.8.4.4,
 * and the two IPv6 ones — and one family's pair of boxes cannot hold the
 * other's. `dns_focus` says which box the caret is in; Tab walks the four. */

static char *dns_field(NetUi *ui) {
    switch (ui->dns_focus) {
    case 0:  return ui->dns_v4_pref;
    case 1:  return ui->dns_v4_alt;
    case 2:  return ui->dns_v6_pref;
    default: return ui->dns_v6_alt;
    }
}

static int dns_field_append(NetUi *ui, const char *text) {
    char *field = dns_field(ui);
    size_t used = strlen(field);
    size_t length = strlen(text);
    if (used + length >= NET_TEXT) {
        return 0;
    }
    memcpy(field + used, text, length);
    field[used + length] = '\0';
    return 1;
}

static int dns_field_backspace(NetUi *ui) {
    char *field = dns_field(ui);
    size_t length = strlen(field);
    if (length == 0) {
        return 0;
    }
    field[length - 1] = '\0';
    return 1;
}

/* The servers to apply, in order: IPv4 Preferred, IPv4 Alternate, IPv6
   Preferred, IPv6 Alternate, skipping the empty ones. Nothing at all empty
   means "back to automatic". */
static int dns_collect(NetUi *ui, const char *list[NET_MAX_DNS]) {
    int count = 0;
    if (ui->dns_v4_pref[0]) {
        list[count++] = ui->dns_v4_pref;
    }
    if (ui->dns_v4_alt[0]) {
        list[count++] = ui->dns_v4_alt;
    }
    if (count < NET_MAX_DNS && ui->dns_v6_pref[0]) {
        list[count++] = ui->dns_v6_pref;
    }
    if (count < NET_MAX_DNS && ui->dns_v6_alt[0]) {
        list[count++] = ui->dns_v6_alt;
    }
    return count;
}

/* --- the actions ---------------------------------------------------------- */

static void do_toggle(NetUi *ui) {
    if (ui->selected < 0 || ui->selected >= ui->devices.count) {
        return;
    }
    NetDevice *device = &ui->devices.items[ui->selected];
    int up = !device->up;

    /* The name is copied out before the list is refreshed: refresh_devices()
       rewrites every item, so a pointer into it would name whatever the read
       put at that index, not the interface that was changed. */
    char name[NET_TEXT];
    snprintf(name, sizeof(name), "%s", device->name);

    snprintf(ui->status, sizeof(ui->status), "Bringing %s %s…",
             name, up ? "up" : "down");
    net_draw(ui);

    char error[NET_TEXT];
    if (net_device_set_up(&ui->config, name, up, error, sizeof(error)) == 0) {
        refresh_devices(ui);
        snprintf(ui->status, sizeof(ui->status), "%s is now %s",
                 name, up ? "up" : "down");
    } else {
        snprintf(ui->status, sizeof(ui->status), "%s", error);
    }
    net_draw(ui);
}

static void do_wifi(NetUi *ui) {
    char link[NET_TEXT];
    net_device_first_wifi(&ui->devices, link, sizeof(link));
    if (!link[0]) {
        snprintf(ui->status, sizeof(ui->status),
                 "No wireless adapter was found, so there is nothing for the "
                 "wifi manager to open");
        net_draw(ui);
        return;
    }
    spawn_command(ui->config.wifi_manager);
    snprintf(ui->status, sizeof(ui->status),
             "Opened the wifi manager for %s", link);
    net_draw(ui);
}

static void do_apply_dns(NetUi *ui) {
    const char *list[NET_MAX_DNS];
    int count = dns_collect(ui, list);

    const char *link = "";
    if (ui->selected >= 0 && ui->selected < ui->devices.count) {
        link = ui->devices.items[ui->selected].name;
    }

    snprintf(ui->status, sizeof(ui->status), "Applying…");
    net_draw(ui);

    char error[NET_TEXT];
    if (net_dns_set(&ui->config, link, list, count, error,
                    sizeof(error)) == 0) {
        load_dns(ui);
        if (count == 0) {
            snprintf(ui->status, sizeof(ui->status),
                     "The resolver is back to automatic");
        } else {
            snprintf(ui->status, sizeof(ui->status),
                     "The resolver was set");
        }
    } else {
        snprintf(ui->status, sizeof(ui->status), "%s", error);
    }
    net_draw(ui);
}

/* Run the action a button — or a letter — stands for. The one place they are
   named, so the mouse and the keyboard cannot drift apart. */
static void dispatch_action(NetUi *ui, NetAction action) {
    switch (action) {
    case NET_ACTION_TOGGLE:
        do_toggle(ui);
        break;
    case NET_ACTION_WIFI:
        do_wifi(ui);
        break;
    case NET_ACTION_DNS:
        ui->mode = NET_MODE_DNS;
        ui->status[0] = '\0';
        load_dns(ui);
        resize_for_mode(ui);
        net_draw(ui);
        break;
    case NET_ACTION_BACK:
        ui->mode = NET_MODE_DEVICES;
        ui->status[0] = '\0';
        resize_for_mode(ui);
        net_draw(ui);
        break;
    case NET_ACTION_REFRESH:
        ui->status[0] = '\0';
        refresh_devices(ui);
        net_draw(ui);
        break;
    case NET_ACTION_DNS_APPLY:
        do_apply_dns(ui);
        break;
    case NET_ACTION_DNS_REVERT:
        /* Empty all four boxes and apply: no servers is "back to automatic",
           which net_dns_set() turns into a revert. */
        ui->dns_v4_pref[0] = '\0';
        ui->dns_v4_alt[0] = '\0';
        ui->dns_v6_pref[0] = '\0';
        ui->dns_v6_alt[0] = '\0';
        do_apply_dns(ui);
        break;
    default:
        break;
    }
}

/* --- the keys ------------------------------------------------------------- */

static void move_selection(NetUi *ui, int step) {
    int rows = ui->devices.count;
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
    int room = net_visible_rows(ui);
    if (ui->selected < ui->scroll) {
        ui->scroll = ui->selected;
    }
    if (ui->selected >= ui->scroll + room) {
        ui->scroll = ui->selected - room + 1;
    }
}

static void handle_dns_key(NetUi *ui, KeySym symbol, XKeyEvent *key) {
    switch (symbol) {
    case XK_Escape:
        dispatch_action(ui, NET_ACTION_BACK);
        return;
    case XK_Return:
    case XK_KP_Enter:
        dispatch_action(ui, NET_ACTION_DNS_APPLY);
        return;
    case XK_Down:
    case XK_KP_Down:
        /* Step to the next box, wrapping: IPv4 Preferred, IPv4 Alternate, IPv6
           Preferred, IPv6 Alternate. Tab and the down arrow both do it, so a
           person filling in the four boxes never reaches for the mouse. */
        ui->dns_focus = (ui->dns_focus + 1) % 4;
        net_draw(ui);
        return;
    case XK_Up:
    case XK_KP_Up:
        ui->dns_focus = (ui->dns_focus + 3) % 4;
        net_draw(ui);
        return;
    case XK_Tab:
        ui->dns_focus = (ui->dns_focus + 1) % 4;
        net_draw(ui);
        return;
    case XK_BackSpace:
        if (dns_field_backspace(ui)) {
            net_draw(ui);
        }
        return;
    default:
        break;
    }

    char buffer[8];
    int length = XLookupString(key, buffer, sizeof(buffer) - 1, NULL, NULL);
    if (length <= 0) {
        return;
    }
    buffer[length] = '\0';
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)buffer[i];
        if (c < 0x20 || c == 0x7f) {
            return;
        }
    }
    if (dns_field_append(ui, buffer)) {
        ui->status[0] = '\0';
        net_draw(ui);
    }
}

static void handle_devices_key(NetUi *ui, KeySym symbol) {
    switch (symbol) {
    case XK_Escape:
        ui->running = 0;
        return;
    case XK_Return:
    case XK_KP_Enter:
        dispatch_action(ui, NET_ACTION_TOGGLE);
        return;
    case XK_w:
        dispatch_action(ui, NET_ACTION_WIFI);
        return;
    case XK_d:
        dispatch_action(ui, NET_ACTION_DNS);
        return;
    case XK_r:
        dispatch_action(ui, NET_ACTION_REFRESH);
        return;
    case XK_Up:
    case XK_KP_Up:
        move_selection(ui, -1);
        net_draw(ui);
        return;
    case XK_Down:
    case XK_KP_Down:
        move_selection(ui, 1);
        net_draw(ui);
        return;
    case XK_Page_Up:
        move_selection(ui, -net_visible_rows(ui));
        net_draw(ui);
        return;
    case XK_Page_Down:
        move_selection(ui, net_visible_rows(ui));
        net_draw(ui);
        return;
    case XK_Home:
        ui->selected = 0;
        move_selection(ui, 0);
        net_draw(ui);
        return;
    case XK_End:
        ui->selected = ui->devices.count - 1;
        move_selection(ui, 0);
        net_draw(ui);
        return;
    default:
        break;
    }
}

static void handle_key(NetUi *ui, XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);
    if (symbol == NoSymbol) {
        return;
    }
    if (ui->mode == NET_MODE_DNS) {
        handle_dns_key(ui, symbol, key);
    } else {
        handle_devices_key(ui, symbol);
    }
}

/* --- the pointer ---------------------------------------------------------- */

static void handle_motion(NetUi *ui, XMotionEvent *motion) {
    if (ui->mode != NET_MODE_DEVICES) {
        if (ui->hover != -1) {
            ui->hover = -1;
            net_draw(ui);
        }
        return;
    }

    int top = ui->style.row_height + ui->style.padding;
    int row = -1;
    if (motion->y >= top) {
        int candidate = ui->scroll + (motion->y - top) / ui->style.row_height;
        if (candidate >= 0 && candidate < ui->devices.count) {
            row = candidate;
        }
    }

    if (row != ui->hover) {
        ui->hover = row;
        net_draw(ui);
    }
}

static void handle_click(NetUi *ui, XButtonEvent *button) {
    if (button->button != Button1) {
        return;
    }

    for (int i = 0; i < ui->button_count; i++) {
        NetButton *candidate = &ui->buttons[i];
        if (button->x >= candidate->x &&
            button->x < candidate->x + candidate->width &&
            button->y >= candidate->y &&
            button->y < candidate->y + candidate->height) {
            dispatch_action(ui, candidate->action);
            return;
        }
    }

    if (ui->mode != NET_MODE_DEVICES) {
        return;
    }

    int top = ui->style.row_height + ui->style.padding;
    if (button->y < top) {
        return;
    }
    int row = (button->y - top) / ui->style.row_height;
    int index = ui->scroll + row;
    if (index < 0 || index >= ui->devices.count) {
        return;
    }
    /* A click only CHOOSES the row. Acting is a second, deliberate act: the
       button or Enter. */
    ui->selected = index;
    net_draw(ui);
}

/* --- the window ----------------------------------------------------------- */

int net_ui_open(NetUi *ui, const char *config_path) {
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;
    ui->hover = -1;
    ui->mode = NET_MODE_DEVICES;

    setlocale(LC_ALL, "");

    ui->display = XOpenDisplay(NULL);
    if (!ui->display) {
        fprintf(stderr, "gnuchannetworkmanager: cannot open the X display\n");
        return -1;
    }
    ui->screen = DefaultScreen(ui->display);
    ui->root = RootWindow(ui->display, ui->screen);
    ui->screen_width = DisplayWidth(ui->display, ui->screen);
    ui->screen_height = DisplayHeight(ui->display, ui->screen);

    char found_path[NET_TEXT * 2];
    const char *path = config_path;
    if (!path || !path[0]) {
        path = net_config_path(found_path, sizeof(found_path));
    }
    if (net_config_load(&ui->config, path) != 0) {
        fprintf(stderr, "gnuchannetworkmanager: %s; using the defaults\n",
                ui->config.error);
    }

    net_style_load(&ui->style, ui->display, ui->screen, &ui->config);

    ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
    if (!ui->gc) {
        fprintf(stderr, "gnuchannetworkmanager: cannot make a graphics "
                "context\n");
        return -1;
    }

    refresh_devices(ui);
    load_dns(ui);
    work_out_size(ui);

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.background_pixel = ui->style.background;
    attributes.backing_store = Always;
    attributes.border_pixel = BlackPixel(ui->display, ui->screen);
    attributes.event_mask = NET_EVENT_MASK;

    ui->window = XCreateWindow(ui->display, ui->root, 0, 0,
                               (unsigned int)ui->width,
                               (unsigned int)ui->height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWBackPixel | CWBackingStore | CWBorderPixel |
                               CWEventMask, &attributes);
    if (ui->window == None) {
        fprintf(stderr, "gnuchannetworkmanager: cannot make the window\n");
        return -1;
    }

    XStoreName(ui->display, ui->window, ui->config.title);

    XClassHint class_hint;
    class_hint.res_name = (char *)"gnuchannetworkmanager";
    class_hint.res_class = (char *)"GnuChanNetworkManager";
    XSetClassHint(ui->display, ui->window, &class_hint);

    Atom wm_delete = XInternAtom(ui->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ui->display, ui->window, &wm_delete, 1);

    XMapWindow(ui->display, ui->window);
    XFlush(ui->display);

    ui->running = 1;
    net_draw(ui);
    return 0;
}

int net_ui_run(NetUi *ui) {
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
        case MotionNotify:
            handle_motion(ui, &event.xmotion);
            break;
        case Expose:
            if (event.xexpose.window == ui->window &&
                event.xexpose.count == 0) {
                net_draw(ui);
            }
            break;
        case ConfigureNotify:
            if (event.xconfigure.window == ui->window) {
                if (event.xconfigure.width != ui->width ||
                    event.xconfigure.height != ui->height) {
                    ui->width = event.xconfigure.width;
                    ui->height = event.xconfigure.height;
                    net_draw(ui);
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

void net_ui_close(NetUi *ui) {
    if (ui->display) {
        if (ui->buffer) {
            XFreePixmap(ui->display, ui->buffer);
            ui->buffer = None;
        }
        if (ui->gc) {
            XFreeGC(ui->display, ui->gc);
            ui->gc = NULL;
        }
        if (ui->window != None) {
            XDestroyWindow(ui->display, ui->window);
            ui->window = None;
        }
        net_style_free(&ui->style, ui->display);
        XCloseDisplay(ui->display);
        ui->display = NULL;
    }
}
