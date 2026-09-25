/*
 * wm_menu.c — the desktop's right-click menu.
 *
 * A window manager with no panel and no taskbar has exactly one place the user
 * can point at that is not a window: the desktop itself. This is what the
 * desktop's own button does — it opens a menu of the things that belong to the
 * session rather than to a window: its settings, and ending it.
 *
 * The menu has tabs, because "settings" and "system" are different kinds of
 * decision — one changes how the session runs, the other ends it — and one
 * flat list of both would put "shutdown" a careless click away from
 * "wallpaper". A tab is a name in the row at the top; the body shows the
 * entries of the active one. The table is data: adding a page is adding a row.
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

#include "wm_core.h"
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
static void action_logout(WmCore *core) {
    Window root_return = None;
    Window parent_return = None;
    Window *children = NULL;
    unsigned int count = 0;

    if (menu_window != None) {
        if (menu_open) {
            XUngrabPointer(core->display, CurrentTime);
            menu_open = 0;
        }
        XDestroyWindow(core->display, menu_window);
        menu_window = None;
    }

    if (XQueryTree(core->display, core->root, &root_return, &parent_return,
                   &children, &count)) {
        for (unsigned int i = 0; i < count; i++) {
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

static const MenuTab MENU_TABS[] = {
    /* The settings page is deliberately empty: it is where the settings will
       go, and a page that exists and says so is more honest than one that is
       silently missing. */
    { "settings", NULL, 0 },
    { "system",   SYSTEM_ENTRIES,
      (int)(sizeof(SYSTEM_ENTRIES) / sizeof(SYSTEM_ENTRIES[0])) },
};

#define MENU_TAB_COUNT ((int)(sizeof(MENU_TABS) / sizeof(MENU_TABS[0])))

/* Lay the menu out for the active tab: where each tab name sits and where the
   body's rows sit. Both the drawing and the hit test answer from these, so a
   click always lands on what was drawn. */
static void menu_layout(WmCore *core) {
    XFontStruct *font = core->style.font;
    int inset = MENU_PADDING / 2;

    int x = MENU_PADDING;
    for (int i = 0; i < MENU_TAB_COUNT; i++) {
        int width = wm_style_text_width(font, MENU_TABS[i].name)
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
            int width = wm_style_text_width(font, tab->entries[i].label)
                      + 3 * MENU_PADDING;
            if (width > body_width) {
                body_width = width;
            }
        }
        body_height = tab->count * MENU_ROW_HEIGHT;
    } else {
        body_width = wm_style_text_width(font, "(nothing here yet)")
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
    menu_height = MENU_TAB_HEIGHT + MENU_PADDING + body_height + MENU_PADDING;

    int rows = tab->count > 0 ? tab->count : 1;
    for (int i = 0; i < rows; i++) {
        entry_box[i].x = inset;
        entry_box[i].y = MENU_TAB_HEIGHT + inset + i * MENU_ROW_HEIGHT;
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
    XFontStruct *font = core->style.font;
    int ascent = font ? font->ascent : 8;
    int descent = font ? font->descent : 2;

    XSetForeground(display, gc, core->style.panel);
    XFillRectangle(display, menu_window, gc, 0, 0,
                   (unsigned int)menu_width, (unsigned int)menu_height);

    /* The tabs. The active one carries the accent and the hovered one the
       raised colour, so which page is showing and which page a click would
       open are two different pictures. */
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
        wm_style_text(display, menu_window, gc, font,
                      tab_box[i].x + MENU_PADDING, baseline, MENU_TABS[i].name,
                      active ? core->style.text : core->style.text_muted);
    }

    const MenuTab *tab = &MENU_TABS[menu_tab];
    if (tab->count == 0) {
        int baseline = entry_box[0].y
                     + (entry_box[0].height + ascent - descent) / 2;
        wm_style_text(display, menu_window, gc, font,
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
            wm_style_text(display, menu_window, gc, font,
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
    attributes.event_mask = ExposureMask | ButtonPressMask |
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

static void menu_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case ButtonPress:
        if (menu_open) {
            menu_press(core, &event->xbutton);
        } else if (event->xbutton.button == Button3 &&
                   event->xbutton.window == core->root) {
            menu_open_at(core, event->xbutton.x_root, event->xbutton.y_root);
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
