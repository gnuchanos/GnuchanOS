/*
 * wm_menu.c — the desktop's right-click menu.
 *
 * A window manager with no panel and no taskbar has exactly one place the user
 * can point at that is not a window: the desktop itself. This is what the
 * desktop's own button does — it opens a menu of the things that belong to the
 * session rather than to a window: ending it, one way to restart and one to
 * power off.
 *
 * There is one page, and it is the system's. A settings page used to sit
 * beside it as an empty placeholder, and it is gone: the desktop is configured
 * by the script alone. GnuChanWM.py is the one place a setting is changed, and
 * a second place that edited the same values would be a second source of truth
 * to keep in step. A page row is therefore only drawn when there is more than
 * one page to choose between; with one page the menu opens straight onto its
 * entries. The table is still data: adding a page is adding a row.
 *
 * It is drawn into its own override-redirect window so this manager does not
 * frame it — a menu with a title bar is not a menu — and the pointer is
 * grabbed while it is open, so the click that dismisses it is delivered to the
 * menu rather than to whatever is under it.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "wm_core.h"
#include "wm_desktop.h"
#include "wm_spawn.h"

/* The distances the drawing is built from, so nothing has to be restated in
   two places when one of them changes. */
#define MENU_PADDING 8
#define MENU_ROW_HEIGHT 26
#define MENU_TAB_HEIGHT 28
#define MENU_MIN_WIDTH 190
#define MENU_TAB_GAP 2
#define MENU_MAX_ENTRIES 8

typedef struct MenuEntry {
    const char *label;
    void (*action)(WmCore *core);
} MenuEntry;

typedef struct MenuTab {
    const char *name;
    const MenuEntry *entries;
    int count;
} MenuTab;

/* --- the menu's state ------------------------------------------------------ */

/* One menu exists, so its state lives here rather than on the core: a second
   open menu is not a thing that can exist, and a field on the core would
   suggest it could.
 *
 * It is declared before the entries because ending the session has to know
 * which window belongs to this process: the list it kills is everything on the
 * display, and this manager is on that list. */
static Window menu_window = None;
static int menu_open = 0;
static int menu_tab = 0;
static int menu_width = 0;
static int menu_height = 0;

/* What the pointer is on: a tab index, MENU_HOVER_ENTRY + a row, or -1. */
#define MENU_HOVER_ENTRY 100
static int menu_hover = -1;

/* Where each tab and each row of the active page sits, filled by menu_layout
   from the same numbers the drawing uses. */
static struct { int x, y, width, height; } tab_box[2];
static struct { int x, y, width, height; } entry_box[MENU_MAX_ENTRIES];

static int inside(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

/* --- running a program and judging it -------------------------------------- */

/* Run a program and wait for it, returning 0 only when it both ran and
   succeeded.
 *
 * The wait and the exit status are the point. A power request is a list of
 * ways to ask, tried in turn, and each has to be judged before the next is
 * made. A chain that only moved on when a program was missing would stop at
 * the first one that exists and refuses — and on a machine where systemctl
 * exists but has no session to let it through, every later way would be
 * skipped for the first one's sake.
 *
 * The child leads its own process group, so a program that decides to ask a
 * terminal a question cannot take the menu's own input away from it. */
static int run_program(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        setsid();
        execvp(argv[0], argv);
        _exit(127);   /* not installed, or cannot be run */
    }

    int status = 0;
    pid_t done;
    do {
        done = waitpid(pid, &status, 0);
    } while (done < 0 && errno == EINTR);

    if (done < 0) {
        return -1;
    }
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/* --- what the entries do --------------------------------------------------- */

/* Log out: end the session.
 *
 * There is no session manager to ask, so the session is ended the only other
 * way there is: every client on the display is killed, and then this manager
 * stops. Killing is abrupt, but the window manager is what the display manager
 * waits on — the moment this process exits, the greeter comes back. A polite
 * close request to a client that ignores it would leave the user on a dead
 * desktop with no way back to the login screen.
 *
 * The menu is this process's own window and is taken down first: the list is
 * of clients to kill, and the server would otherwise close this connection
 * partway through the loop, leaving the rest of the session running. */
/* Whether a window on the root belongs to this manager rather than to a
 * program.
 *
 * This test is what makes the walk below able to finish. XKillClient on a
 * window this client owns does not merely close that window: it closes this
 * client's connection to the server, which is what the protocol says killing a
 * resource's owner means. The root's children include the manager's frames,
 * its bar, its check window and its config-message window — so without this
 * test the very first child killed is the manager's own, the connection goes
 * down mid-loop, and every kill still queued behind it is discarded with it.
 * The session then does not end at all: the programs stay up and the menu is
 * gone, with nothing on the screen to say why.
 *
 * A frame is recognised by the table rather than by asking the server, because
 * the table is the manager's own record of what it made. The rest are the
 * single windows the manager keeps: the bar, the advertisement window and the
 * message window. */
static int window_belongs_to_manager(WmCore *core, Window window) {
    if (window == core->check_window ||
        window == wm_desktop_bar_window() ||
        window == wm_config_error_window()) {
        return 1;
    }
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].frame == window) {
            return 1;
        }
    }
    return 0;
}

static void action_logout(WmCore *core) {
    Window root_return = None;
    Window parent_return = None;
    Window *children = NULL;
    unsigned int count = 0;

    if (menu_window != None) {
        if (menu_open) {
            XUngrabPointer(core->display, CurrentTime);
            XUngrabKeyboard(core->display, CurrentTime);
            menu_open = 0;
        }
        XDestroyWindow(core->display, menu_window);
        menu_window = None;
    }

    if (XQueryTree(core->display, core->root, &root_return, &parent_return,
                   &children, &count)) {
        /* Every kill is queued before anything is flushed, and the manager's
           own windows are skipped: see window_belongs_to_manager(). */
        for (unsigned int i = 0; i < count; i++) {
            if (window_belongs_to_manager(core, children[i])) {
                continue;
            }
            XKillClient(core->display, children[i]);
        }
        if (children) {
            XFree(children);
        }
    }
    XSync(core->display, False);
    core->running = 0;
}

/* Ask the machine to restart or to power off, trying each way a Debian may be
   set up until one is accepted.
 *
 * The first way is sudo, because that is the way that works on a Debian where
 * the user may restart the machine: reboot and poweroff are root's, and a
 * session started by the greeter runs outside logind, so the polkit rule that
 * would let its owner do it is not matched — the plain call is refused, and
 * that refusal was being thrown away. systemctl and loginctl follow, and the
 * plain programs last, for a machine with no service manager at all.
 *
 * Every way is judged by its exit status before the next is tried: a way that
 * exists and refuses is not a way that works, and the first form of this
 * stopped at the first program it found. */
static int try_power(const char *verb, const char *direct_program) {
    char *sudo_service[] = { "sudo", "-n", "systemctl", (char *)verb, NULL };
    if (run_program(sudo_service) == 0) return 0;

    char *sudo_direct[]  = { "sudo", "-n", (char *)direct_program, NULL };
    if (run_program(sudo_direct) == 0) return 0;

    char *via_service[]  = { "systemctl", (char *)verb, NULL };
    if (run_program(via_service) == 0) return 0;

    char *via_login[]    = { "loginctl", (char *)verb, NULL };
    if (run_program(via_login) == 0) return 0;

    char *direct[]       = { (char *)direct_program, NULL };
    if (run_program(direct) == 0) return 0;

    return -1;
}

/* The last way, taken only when none of the above were allowed: a terminal,
   because it is the one place on this desktop where a password can be typed.
   sudo's own prompt has nowhere to go when sudo is run from a menu, and a
   request that waits forever for a password nobody can enter is the same as no
   request at all. The window is opened and the command run inside it, so the
   prompt lands where it can be answered. */
static void power_in_terminal(const char *verb) {
    const char *terminal = wm_terminal_program();
    if (!terminal) {
        fprintf(stderr, "gnuchanwm: could not %s: no terminal to ask in\n", verb);
        return;
    }

    char command[128];
    snprintf(command, sizeof(command), "sudo systemctl %s", verb);

    char *modern[] = { (char *)terminal, "-e", "sh", "-c", command, NULL };
    if (wm_spawn(terminal, modern) == 0) return;

    char *classic[] = { (char *)terminal, "-e", "sudo", "systemctl",
                        (char *)verb, NULL };
    wm_spawn(terminal, classic);
}

static void action_reboot(WmCore *core) {
    (void)core;
    if (try_power("reboot", "/sbin/reboot") != 0) {
        power_in_terminal("reboot");
    }
}

static void action_shutdown(WmCore *core) {
    (void)core;
    if (try_power("poweroff", "/sbin/poweroff") != 0) {
        power_in_terminal("poweroff");
    }
}

/* --- the table ------------------------------------------------------------- */

static const MenuEntry SYSTEM_ENTRIES[] = {
    { "logout",   action_logout   },
    { "shutdown", action_shutdown },
    { "reboot",   action_reboot   },
};

/* One page, and it is the system's. see the note at the top of the file for
   why there is no settings page beside it. */
static const MenuTab MENU_TABS[] = {
    { "system",   SYSTEM_ENTRIES,
      (int)(sizeof(SYSTEM_ENTRIES) / sizeof(SYSTEM_ENTRIES[0])) },
};

#define MENU_TAB_COUNT ((int)(sizeof(MENU_TABS) / sizeof(MENU_TABS[0])))

/* The height of the row of page names.
 *
 * A row of one name is a label for a choice that cannot be made, so with a
 * single page the row is not drawn: the menu opens straight onto its entries
 * and the space the row would take goes to the body. Every part that has to
 * know — the layout, the drawing and the hit test — asks here, so the three
 * cannot disagree about whether the row is there. */
static int menu_tab_bar_height(void) {
    return MENU_TAB_COUNT > 1 ? MENU_TAB_HEIGHT : 0;
}

/* Lay the menu out for the active tab: where each tab name sits and where the
   body's rows sit. Both the drawing and the hit test answer from these, so a
   click always lands on what was drawn. */
static void menu_layout(WmCore *core) {
    XftFont *font = core->style.font;
    int inset = MENU_PADDING / 2;
    int tab_bar_height = menu_tab_bar_height();

    int x = MENU_PADDING;
    for (int i = 0; i < MENU_TAB_COUNT; i++) {
        if (tab_bar_height == 0) {
            /* No row is drawn, so no box is laid out: a box of zero size is
               what the hit test reads as "not a tab". */
            tab_box[i].x = 0;
            tab_box[i].y = 0;
            tab_box[i].width = 0;
            tab_box[i].height = 0;
            continue;
        }
        int width = wm_style_text_width(core->display, font, MENU_TABS[i].name)
                  + 2 * MENU_PADDING;
        tab_box[i].x = x;
        tab_box[i].y = inset;
        tab_box[i].width = width;
        tab_box[i].height = MENU_TAB_HEIGHT;
        x += width + MENU_TAB_GAP;
    }
    int tab_bar_width = x;

    const MenuTab *tab = &MENU_TABS[menu_tab];
    int body_width;
    int body_height;
    if (tab->count > 0) {
        body_width = 0;
        for (int i = 0; i < tab->count; i++) {
            int width = wm_style_text_width(core->display, font,
                                            tab->entries[i].label)
                      + 3 * MENU_PADDING;
            if (width > body_width) {
                body_width = width;
            }
        }
        body_height = tab->count * MENU_ROW_HEIGHT;
    } else {
        body_width = wm_style_text_width(core->display, font,
                                         "(nothing here yet)")
                   + 3 * MENU_PADDING;
        body_height = MENU_ROW_HEIGHT;
    }

    menu_width = body_width;
    if (tab_bar_width + MENU_PADDING > menu_width) {
        menu_width = tab_bar_width + MENU_PADDING;
    }
    if (menu_width < MENU_MIN_WIDTH) {
        menu_width = MENU_MIN_WIDTH;
    }
    /* The body starts below the row of names, and the row is only there when
       there is more than one page: tab_bar_height is 0 for a single page, so
       the entries sit against the top edge and the menu is that much shorter. */
    menu_height = tab_bar_height + MENU_PADDING + body_height + MENU_PADDING;

    int rows = tab->count > 0 ? tab->count : 1;
    for (int i = 0; i < rows; i++) {
        entry_box[i].x = inset;
        entry_box[i].y = tab_bar_height + inset + i * MENU_ROW_HEIGHT;
        entry_box[i].width = menu_width - MENU_PADDING;
        entry_box[i].height = MENU_ROW_HEIGHT;
    }
}

static void menu_draw(WmCore *core) {
    if (!menu_open || menu_window == None) {
        return;
    }
    Display *display = core->display;
    GC gc = core->gc;
    XftFont *font = core->style.font;
    int ascent = font ? font->ascent : 8;
    int descent = font ? font->descent : 2;

    XSetForeground(display, gc, core->style.panel);
    XFillRectangle(display, menu_window, gc, 0, 0,
                   (unsigned int)menu_width, (unsigned int)menu_height);

    /* The tabs, when there is more than one page to name. The active one
       carries the accent and the hovered one the raised colour, so which page
       is showing and which page a click would open are two different pictures.
       With a single page there is no row of names: tab_box is zero-sized for
       that case, and the loop would draw the one name against the top-left
       corner, over the entries it is supposed to sit above. */
    if (menu_tab_bar_height() > 0) {
        for (int i = 0; i < MENU_TAB_COUNT; i++) {
            int active = (i == menu_tab);
            int hovered = (menu_hover == i);
            if (active || hovered) {
                XSetForeground(display, gc,
                               active ? core->style.accent : core->style.field);
                XFillRectangle(display, menu_window, gc,
                               tab_box[i].x, tab_box[i].y,
                               (unsigned int)tab_box[i].width,
                               (unsigned int)tab_box[i].height);
            }
            int baseline = tab_box[i].y
                         + (tab_box[i].height + ascent - descent) / 2;
            wm_style_text(display, core->screen, menu_window, font,
                          tab_box[i].x + MENU_PADDING, baseline,
                          MENU_TABS[i].name,
                          active ? core->style.text : core->style.text_muted);
        }
    }

    const MenuTab *tab = &MENU_TABS[menu_tab];
    if (tab->count == 0) {
        int baseline = entry_box[0].y
                     + (entry_box[0].height + ascent - descent) / 2;
        wm_style_text(display, core->screen, menu_window, font,
                      entry_box[0].x + MENU_PADDING, baseline,
                      "(nothing here yet)", core->style.text_muted);
    } else {
        for (int i = 0; i < tab->count; i++) {
            int hovered = (menu_hover == MENU_HOVER_ENTRY + i);
            if (hovered) {
                XSetForeground(display, gc, core->style.field);
                XFillRectangle(display, menu_window, gc,
                               entry_box[i].x, entry_box[i].y,
                               (unsigned int)entry_box[i].width,
                               (unsigned int)entry_box[i].height);
            }
            int baseline = entry_box[i].y
                         + (entry_box[i].height + ascent - descent) / 2;
            wm_style_text(display, core->screen, menu_window, font,
                          entry_box[i].x + MENU_PADDING, baseline,
                          tab->entries[i].label,
                          hovered ? core->style.text : core->style.text_muted);

            /* A hairline under every row but the last, so the rows read as
               separate choices rather than one block of text. */
            if (i + 1 < tab->count) {
                int line_y = entry_box[i].y + entry_box[i].height;
                XSetForeground(display, gc, core->style.panel_edge);
                XDrawLine(display, menu_window, gc,
                          entry_box[i].x + MENU_PADDING, line_y,
                          entry_box[i].x + entry_box[i].width - MENU_PADDING,
                          line_y);
            }
        }
    }

    /* The edge, last so it sits over the fills. */
    XSetForeground(display, gc, core->style.accent);
    XDrawRectangle(display, menu_window, gc, 0, 0,
                   (unsigned int)(menu_width - 1),
                   (unsigned int)(menu_height - 1));

    XFlush(display);
}

static void menu_close(WmCore *core) {
    if (!menu_open) {
        return;
    }
    XUngrabPointer(core->display, CurrentTime);
    XUngrabKeyboard(core->display, CurrentTime);
    XUnmapWindow(core->display, menu_window);
    XFlush(core->display);
    menu_open = 0;
    menu_hover = -1;
}

/* Which part of the menu a point is on: a tab index, MENU_HOVER_ENTRY + a row,
   or -1 for nothing. */
static int menu_hit(int x, int y) {
    for (int i = 0; i < MENU_TAB_COUNT; i++) {
        if (inside(x, y, tab_box[i].x, tab_box[i].y,
                   tab_box[i].width, tab_box[i].height)) {
            return i;
        }
    }
    const MenuTab *tab = &MENU_TABS[menu_tab];
    for (int i = 0; i < tab->count; i++) {
        if (inside(x, y, entry_box[i].x, entry_box[i].y,
                   entry_box[i].width, entry_box[i].height)) {
            return MENU_HOVER_ENTRY + i;
        }
    }
    return -1;
}

static void menu_open_at(WmCore *core, int root_x, int root_y) {
    menu_layout(core);

    /* Keep it on the screen: a menu opened near the bottom-right corner would
       otherwise put its last rows where they cannot be reached. */
    if (root_x + menu_width > core->width) {
        root_x = core->width - menu_width;
    }
    if (root_y + menu_height > core->height) {
        root_y = core->height - menu_height;
    }
    if (root_x < 0) root_x = 0;
    if (root_y < 0) root_y = 0;

    XMoveResizeWindow(core->display, menu_window, root_x, root_y,
                      (unsigned int)menu_width, (unsigned int)menu_height);
    XMapRaised(core->display, menu_window);

    /* The grab is what makes the next click the menu's: without it, clicking
       off a menu would also press whatever was underneath it. */
    int status = XGrabPointer(core->display, menu_window, False,
                              ButtonPressMask | PointerMotionMask,
                              GrabModeAsync, GrabModeAsync,
                              None, None, CurrentTime);
    /* The keyboard as well, so Escape can close it. A menu that can only be
       dismissed by clicking somewhere feels stuck — and the click that
       dismisses it may be the one the user did not mean to make. Escape is
       what a hand reaches for, so the keyboard is held for as long as the
       menu is up. A grab that fails is not fatal: the menu still works, and
       Escape is then simply nobody's key rather than the menu's. */
    XGrabKeyboard(core->display, menu_window, False,
                  GrabModeAsync, GrabModeAsync, CurrentTime);
    menu_open = 1;
    menu_hover = -1;
    menu_draw(core);
    if (status != GrabSuccess) {
        /* Up but unable to take the click: taking it down is better than
           leaving a menu that does nothing. */
        menu_close(core);
    }
}

static void menu_press(WmCore *core, XButtonEvent *event) {
    /* Every press arrives with coordinates inside the menu, because the menu
       holds the grab. A point outside it is therefore how "clicked away"
       looks. */
    if (event->x < 0 || event->y < 0 ||
        event->x >= menu_width || event->y >= menu_height) {
        menu_close(core);
        return;
    }

    int hit = menu_hit(event->x, event->y);
    if (hit < 0) {
        return;   /* inside the frame but on no row: nothing to do */
    }
    if (hit < MENU_HOVER_ENTRY) {
        /* A tab: switch to it and resize the window to the new body. The
           position is kept, so the menu does not jump as it is used. */
        menu_tab = (int)hit;
        menu_hover = -1;
        menu_layout(core);
        XResizeWindow(core->display, menu_window,
                      (unsigned int)menu_width, (unsigned int)menu_height);
        menu_draw(core);
        return;
    }

    void (*action)(WmCore *) = MENU_TABS[menu_tab]
                                   .entries[hit - MENU_HOVER_ENTRY].action;
    menu_close(core);
    if (action) {
        action(core);
    }
}

static int menu_init(WmCore *core) {
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.panel;
    attributes.event_mask = ExposureMask | ButtonPressMask | KeyPressMask |
                            PointerMotionMask | LeaveWindowMask;

    menu_window = XCreateWindow(core->display, core->root,
                                0, 0, MENU_MIN_WIDTH, 100, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWBackPixel | CWEventMask,
                                &attributes);
    if (menu_window == None) {
        return -1;
    }
    menu_open = 0;
    menu_tab = 0;
    menu_hover = -1;
    return 0;
}

/* Open the menu at a point on the screen. Public because two things ask for it
   and they are not the same thing: the desktop's own button, and the right
   button anywhere, which is where a person expects a menu. A menu that is
   already open is left alone rather than moved. */
void wm_menu_open(WmCore *core, int root_x, int root_y) {
    if (core && !menu_open) {
        menu_open_at(core, root_x, root_y);
    }
}

int wm_menu_is_open(void) {
    return menu_open;
}

static void menu_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case ButtonPress:
        if (menu_open) {
            menu_press(core, &event->xbutton);
            break;
        }
        /* Whichever button the script said opens the menu — by default the
           right one — and only on the desktop itself.
         *
         * The window test is not decoration. A press on a frame or on a
           program's own pixels is reported here as well, because the root is
           what a redirected press belongs to; without the test a right click
           anywhere opened the desktop's menu, which is a menu that belongs to
           the desktop appearing over a window that has one of its own. On a
           window the button is left alone, which is what lets the program
           under the pointer answer it. */
        if (event->xbutton.window == core->root &&
            wm_config_mouse_action(&core->config,
                                   event->xbutton.button) == WM_MOUSE_MENU) {
            menu_open_at(core, event->xbutton.x_root, event->xbutton.y_root);
        }
        break;

    case KeyPress:
        /* Escape takes it down. The keyboard is grabbed for exactly this, so
           the key is the menu's only while the menu is up; any other key is
           ignored rather than acted on, because a menu is not a text field and
           swallowing keys it does not use would make it a keyboard trap. */
        if (menu_open &&
            XLookupKeysym(&event->xkey, 0) == XK_Escape) {
            menu_close(core);
        }
        break;

    case MotionNotify:
        if (menu_open) {
            int hit = menu_hit(event->xmotion.x, event->xmotion.y);
            if (hit != menu_hover) {
                menu_hover = hit;
                menu_draw(core);
            }
        }
        break;

    case Expose:
        if (menu_open && event->xexpose.window == menu_window) {
            menu_draw(core);
        }
        break;

    case LeaveNotify:
        if (menu_open && menu_hover != -1) {
            menu_hover = -1;
            menu_draw(core);
        }
        break;

    default:
        break;
    }
}

static void menu_cleanup(WmCore *core) {
    if (menu_open) {
        XUngrabPointer(core->display, CurrentTime);
        XUngrabKeyboard(core->display, CurrentTime);
        menu_open = 0;
    }
    if (menu_window != None) {
        XDestroyWindow(core->display, menu_window);
        menu_window = None;
    }
}

const WmModule wm_menu_module = {
    .name = "menu",
    .init = menu_init,
    .event = menu_event,
    .cleanup = menu_cleanup,
};
