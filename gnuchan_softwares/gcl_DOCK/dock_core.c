/*
 * dock_core.c — the X connection, the dock window, and the loop.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <X11/Xatom.h>

#include "dock_core.h"
#include "dock_draw.h"
#include "dock_menu.h"
#include "dock_shape.h"

/* The size and place the dock window was last configured to. The window is
   resized only when these change — a window reconfigured on every repaint is a
   window the server strips the shape region off, and with the region gone the
   square corners would show again: the rounding would blink away every few
   hundred milliseconds and come back. Between resizes the window keeps its
   shape, so the shape is applied only when the window is reconfigured. */
static int dock_window_shaped;
static int dock_window_width;
static int dock_window_height;
static int dock_window_x;
static int dock_window_y;

/* The height of the strip along the very bottom of the screen that brings the
   dock back when the pointer reaches it, and how long the dock waits before it
   drops again once the pointer has left it. The delay is what lets a hand cross
   the small gap between the strip and the dock, and pass over the dock's own
   icons, without the dock falling away underneath it. */
#define DOCK_TRIGGER_HEIGHT 3
#define DOCK_HIDE_DELAY_MS 400

/* A monotonic clock in milliseconds. The hide delay measures how long
   something took, so a wall clock that jumps when the time is set would make
   the dock hide at once or never. */
static unsigned long core_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

static int core_error(Display *display, XErrorEvent *error) {
    if (error->error_code == BadWindow || error->error_code == BadMatch ||  error->error_code == BadDrawable || error->error_code == BadGC) {
        return 0;
    }
    char text[256];
    XGetErrorText(display, error->error_code, text, sizeof(text));
    fprintf(stderr, "gnuchandock: X error: %s\n", text);
    return 0;
}

static void core_resolve_palette(DockCore *core) {
    core->background = dock_config_colour(core->display, core->screen,  core->config.background, 0x1a0b2e);
    core->edge = dock_config_colour(core->display, core->screen, core->config.background_edge, 0x7b2cbf);
    core->field = dock_config_colour(core->display, core->screen, core->config.field, 0x241033);
    core->text = dock_config_colour(core->display, core->screen, core->config.text, 0xe0c3fc);
    core->accent = dock_config_colour(core->display, core->screen, core->config.accent, 0xc77dff);
    core->badge = dock_config_colour(core->display, core->screen, core->config.badge, 0x1a0b2e);
}

static void core_close_font(DockCore *core) {
    if (core->font) {
        XftFontClose(core->display, core->font);
        core->font = NULL;
    }
}

static void core_open_font(DockCore *core) {
    core_close_font(core);
    core->font = XftFontOpenName(core->display, core->screen, core->config.font);
    if (!core->font) {
        core->font = XftFontOpenName(core->display, core->screen, "monospace:pixelsize=11");
    }
}

/* Whether a window is fullscreen, read from the EWMH state list it carries.
   _NET_WM_STATE is a list of atoms and _NET_WM_STATE_FULLSCREEN is the member
   that means "this window owns the whole screen". A client that never set the
   property is not fullscreen, which is every ordinary window. */
static int core_window_is_fullscreen(DockCore *core, Window window) {
    if (window == None) {
        return 0;
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    int fullscreen = 0;

    if (XGetWindowProperty(core->display, window, core->net_wm_state,
                           0, 32, False, XA_ATOM, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_type == XA_ATOM && actual_format == 32) {
            Atom *atoms = (Atom *)data;
            for (unsigned long i = 0; i < items; i++) {
                if (atoms[i] == core->net_wm_state_fullscreen) {
                    fullscreen = 1;
                    break;
                }
            }
        }
        if (data) {
            XFree(data);
        }
    }
    return fullscreen;
}

/* Whether a fullscreen window is on the screen NOW, asked at the moment the
   answer is used. The build-time value (core->fullscreen_active) is only as
   fresh as the last rebuild, and a window can go fullscreen without any of the
   root properties the dock watches changing at all — so the one decision that
   must not be wrong, "may the dock come forward", is taken from a fresh read
   of the active window here rather than from the cached one. It is a round trip
   or two, paid only when the pointer reaches the bottom of the screen. */
static int core_fullscreen_now(DockCore *core) {
    Window active = None;
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(core->display, core->root, core->net_active_window,
                           0, 1, False, XA_WINDOW, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_format == 32 && items > 0) {
            active = (Window)(*(unsigned long *)data);
        }
        if (data) {
            XFree(data);
        }
    }
    return core_window_is_fullscreen(core, active);
}

/* Read the two root properties that say WHAT to show: which desktop is current
   and which window has the focus. Called at the start of every build, before
   the slots are gathered, so the gathering can use them.
 *
 * A manager that publishes neither leaves have_desktop at 0 and current_desktop
 * at 0, and the gathering then shows every window — the safe answer, and the one
 * that keeps the dock usable on a plain X session with no EWMH manager. */
static void core_read_desktop_state(DockCore *core) {
    core->have_desktop = 0;
    core->current_desktop = 0;
    core->active_window = None;

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(core->display, core->root, core->net_current_desktop,
                           0, 1, False, XA_CARDINAL, &actual_type,
                           &actual_format, &items, &after, &data) == Success) {
        /* The value comes back as one unsigned long per item whatever the
           format, so a 32-bit card is read as a long and not as an int. */
        if (data && actual_format == 32 && items > 0) {
            core->current_desktop = (long)(*(unsigned long *)data);
            core->have_desktop = 1;
        }
        if (data) {
            XFree(data);
        }
    }

    data = NULL;
    actual_type = None;
    if (XGetWindowProperty(core->display, core->root, core->net_active_window,
                           0, 1, False, XA_WINDOW, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_format == 32 && items > 0) {
            core->active_window = (Window)(*(unsigned long *)data);
        }
        if (data) {
            XFree(data);
        }
    }

    /* Whether the active window is fullscreen, read here beside the two above
       so every build knows it before the stack is decided. A fullscreen window
       owns the whole screen and the dock must not come forward over it; a
       maximised one is not this, so the dock still may. */
    core->fullscreen_active =
        core_window_is_fullscreen(core, core->active_window);
}

static void core_load_fixed_icons(DockCore *core) {
    dock_icon_free(core->display, &core->settings_icon);
    dock_icon_free(core->display, &core->terminal_icon);

    dock_icon_load_file(core->display, core->root, core->visual, core->depth,
                        &core->settings_icon, core->config.settings_icon,
                        core->config.icon_size, core->background);
    dock_icon_load_file(core->display, core->root, core->visual, core->depth,
                        &core->terminal_icon, core->config.terminal_icon,
                        core->config.icon_size, core->background);
}

static void core_set_window_properties(DockCore *core) {
    XStoreName(core->display, core->window, "GnuChanDock");

    Atom type = core->net_wm_window_type_dock;
    XChangeProperty(core->display, core->window, core->net_wm_window_type,
                    XA_ATOM, 32, PropModeReplace, (unsigned char *)&type, 1);
}

static void core_set_strut(DockCore *core) {
    Atom strut = XInternAtom(core->display, "_NET_WM_STRUT", False);
    /* Nothing is reserved. The dock is not a bar the desktop owes space to: it
       rests behind the windows and comes forward only when the pointer reaches
       the bottom, so a window may use the whole screen and cover it. A strut
       here would keep maximised windows short of a dock the user did not ask to
       see. */
    long values[4] = { 0, 0, 0, 0 };
    XChangeProperty(core->display, core->window, strut, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)values, 4);
}

static Window core_create_window(DockCore *core) {
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->background;
    attributes.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                            PointerMotionMask | EnterWindowMask |
                            LeaveWindowMask | StructureNotifyMask;

    return XCreateWindow(core->display, core->root, core->x, core->y,
                         (unsigned int)core->width, (unsigned int)core->height,
                         0, core->depth, InputOutput, core->visual,
                         CWOverrideRedirect | CWBackPixel | CWEventMask,
                         &attributes);
}

void dock_core_relayout(DockCore *core) {
    int icon = core->config.icon_size;
    int gap = core->config.gap;
    int padding = core->config.padding;
    int label_band = 0;
    if (core->config.label_enabled && core->font) {
        label_band = core->font->ascent + core->font->descent + 4;
    }

    /* The magnify is reserved on both axes, because a hovered icon grows by it
       in every direction and its plate reaches half of it past the icon's edge.
       Without the horizontal share the end icons of the row would be drawn over
       the body's own edge — the row would spill out of the panel it sits in. */
    int count = core->item_count > 0 ? core->item_count : 1;
    int width = padding * 2 + count * icon + (count - 1) * gap +
                core->config.magnify;
    int height = padding * 2 + icon + core->config.magnify + label_band;

    int screen_width = DisplayWidth(core->display, core->screen);
    int screen_height = DisplayHeight(core->display, core->screen);

    core->width = width;
    core->height = height;
    core->x = (screen_width - width) / 2;
    /* The dock always sits the margin above the bottom edge: it is not moved to
       show or hide. It is STACKED instead (see core_reveal and core_hide), left
       behind the windows so a covered dock is not seen, and raised above them
       when the pointer reaches the bottom of the screen. */
    core->y = screen_height - height - core->config.margin;

    /* Only reconfigured when the size or place actually changed. A window
       reconfigured on every repaint would have its shape region stripped on
       every repaint, and its square corners would show again and again; the
       shape is set with the reconfiguration and kept until the next one. */
    if (core->window == None) {
        return;
    }
    if (dock_window_shaped && dock_window_width == core->width &&
        dock_window_height == core->height && dock_window_x == core->x &&
        dock_window_y == core->y) {
        return;
    }

    XMoveResizeWindow(core->display, core->window, core->x, core->y,
                      (unsigned int)core->width, (unsigned int)core->height);
    /* The window's own square corners are cut off: the body is drawn round,
       and the window behind it must be round too, or its four corners would
       show as sharp triangles around the round body. */
    dock_shape_round(core->display, core->window, core->width, core->height,
                     core->config.corner);
    core_set_strut(core);

    dock_window_shaped = 1;
    dock_window_width = core->width;
    dock_window_height = core->height;
    dock_window_x = core->x;
    dock_window_y = core->y;
}

/* Bring the dock to the FRONT, over the windows. Called when the pointer
   reaches the dock itself or the strip along the bottom of the screen. The dock
   does not move: it is raised in the stacking order, so it comes forward where
   it already sits, and it is redrawn because a window that covered it left
   nothing of it on the screen to uncover. */
static void core_reveal(DockCore *core) {
    core->hide_pending = 0;
    if (core->revealed || core->window == None) {
        return;
    }
    /* A FULLSCREEN window owns the whole screen and the dock stays behind it.
       This is the line asked for: the dock comes forward over a MAXIMISED
       window — an ordinary window taking the workarea, no different from any
       other — but never over a fullscreen one, where a strip along the bottom
       is in a game's or a film's way. The answer is read fresh
       (core_fullscreen_now) rather than taken from the last build, because a
       window can go fullscreen without any of the root properties the dock
       watches changing. */
    if (core_fullscreen_now(core)) {
        return;
    }
    core->revealed = 1;
    XRaiseWindow(core->display, core->window);
    dock_draw(core);
}

/* Begin putting the dock back, after the delay. The pointer leaving the dock —
   for a window, or for the strip on its way out — starts this, and reaching the
   dock or the strip again calls it off. */
static void core_schedule_hide(DockCore *core) {
    if (!core->revealed) {
        return;
    }
    core->hide_pending = 1;
    core->hide_at_ms = core_now_ms() + DOCK_HIDE_DELAY_MS;
}

/* Put the dock back BEHIND the windows: lower it so a window over its place
   covers it, and put the strip along the very bottom back on top so it is what
   the pointer meets when it comes down again. The dock does not move; only the
   stacking order changes. */
static void core_hide(DockCore *core) {
    core->hide_pending = 0;
    if (!core->revealed) {
        return;
    }
    core->revealed = 0;
    core->hover_index = -1;
    if (core->window != None) {
        XLowerWindow(core->display, core->window);
    }
    if (core->trigger != None) {
        XRaiseWindow(core->display, core->trigger);
    }
}

/* Put the strip along the bottom of the screen back on top of the windows, so
   the pointer meets it when it comes down. Called when a window was moved or
   raised over it. Nothing happens while the dock is OUT: the dock is what the
   pointer meets then, and the strip is not in the way.

   This is the piece that was missing. The strip is raised at start-up, but the
   manager raises the windows it manages, and a raise puts a window above the
   strip — and with the strip covered the pointer reaching the bottom of the
   screen is a move into that window and NOT into the strip, so the dock is
   never told and never comes forward. Raising the strip here, on the very
   ConfigureNotify the raise produces, is what puts it back where the pointer
   can find it. */
static void core_keep_trigger_on_top(DockCore *core) {
    if (core->revealed || core->trigger == None) {
        return;
    }
    /* Not over a fullscreen window either: the strip is three pixels along the
       very bottom and a fullscreen game or film should not have it lying over
       its last rows. The same line core_reveal draws, drawn the other way. */
    if (core_fullscreen_now(core)) {
        return;
    }
    XRaiseWindow(core->display, core->trigger);
}

/* Decide which way the stack should be, from where the pointer actually is and
   not from the crossing that woke us. The dock window and the strip can hand
   the pointer back and forth, and the Leave of one arrives around the Enter of
   the other in an order that is not guaranteed; reading the pointer once, here,
   makes the answer the same whatever that order was. A crossing is rare, so the
   single XQueryPointer it costs is no redraw-storm. */
static void core_update_hover(DockCore *core) {
    Window root_return;
    Window child_return;
    int root_x = 0;
    int root_y = 0;
    int win_x = 0;
    int win_y = 0;
    unsigned int mask = 0;
    int screen_height = DisplayHeight(core->display, core->screen);
    int over_trigger;
    int over_dock = 0;

    if (!XQueryPointer(core->display, core->root, &root_return, &child_return,
                       &root_x, &root_y, &win_x, &win_y, &mask)) {
        return;
    }

    /* The strip along the very bottom, and the dock's own rectangle: either one
       under the pointer is the pointer wanting the dock. The strip is checked
       by the bottom edge alone because it spans the whole screen width. */
    over_trigger = root_y >= screen_height - DOCK_TRIGGER_HEIGHT;
    if (core->window != None) {
        over_dock = root_x >= core->x && root_x < core->x + core->width &&
                    root_y >= core->y && root_y < core->y + core->height;
    }

    if (over_trigger || over_dock) {
        core_reveal(core);
        return;
    }
    if (core->revealed) {
        core_schedule_hide(core);
    }
}

void dock_core_refresh(DockCore *core) {
    /* What to show is read first: the gathering below decides which windows are
       on the current desktop and which one is focused from these two values. */
    core_read_desktop_state(core);
    dock_items_build(core);
    dock_core_relayout(core);
    dock_draw(core);
    /* Which way the stack goes, decided from the fullscreen state and whether
       the pointer is on the dock.
     *
     * A FULLSCREEN window owns the screen: the dock goes behind it and the
     * strip stays behind it too, so neither shows over a game or a film. That
     * is the line between the two kinds of window this is about — a MAXIMISED
     * window is an ordinary window taking the workarea, and the dock comes
     * forward over it; a FULLSCREEN one is not.
     *
     * Otherwise, a dock nobody is pointing at belongs behind the windows, and
     * the strip belongs on top so the pointer still finds it at the bottom. */
    if (core->fullscreen_active) {
        if (core->window != None) {
            XLowerWindow(core->display, core->window);
        }
        if (core->trigger != None) {
            XLowerWindow(core->display, core->trigger);
        }
        core->revealed = 0;
        core->hover_index = -1;
    } else if (!core->revealed && core->window != None) {
        XLowerWindow(core->display, core->window);
        if (core->trigger != None) {
            XRaiseWindow(core->display, core->trigger);
        }
    }
    core->dirty = 0;
}

void dock_core_restyle(DockCore *core) {
    /* The list is drawn in the old colours, so it is put away before they
       change; the next click on a slot opens a fresh one. */
    dock_menu_close(&core->menu);
    /* The label colours are cached by the name they came from (see
       dock_draw.c), so a restyle has to drop them or a script that changed a
       colour would keep drawing the old one. */
    dock_draw_forget_colours(core);
    /* The per-program icons are cached too (see dock_items.c), and they were
       scaled and composited for the size and the background now being
       replaced: they are dropped so the next build makes them again in the new
       dock. */
    dock_items_free_icons(core);
    dock_config_load_default(&core->config);
    core_resolve_palette(core);
    core_open_font(core);
    core_load_fixed_icons(core);
    dock_core_refresh(core);
}

void dock_core_run_command(const char *command) {
    if (!command || !command[0]) {
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, WNOHANG);
    }
}

void dock_core_activate(DockCore *core, Window client) {
    if (client == None) {
        return;
    }
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = client;
    event.xclient.message_type = core->net_active_window;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(core->display, core->root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(core->display);
}

static void core_handle_click(DockCore *core, XButtonEvent *press) {
    if (press->button != Button1) {
        return;
    }
    int index = dock_items_index_at(core, press->x, press->y);
    if (index < 0) {
        return;
    }
    DockItem *item = &core->items[index];

    /* A category — two of a kind, or the terminal with a terminal open — holds
       more than one window, and a click on it opens the list of what it holds
       rather than bringing one of them forward. The list is placed over the
       slot the click landed on, the way the row of the clicked slot would. */
    if (dock_item_is_category(item)) {
        /* The same slot clicked again is the list's own close: the click that
           raised the list puts it back. */
        if (dock_menu_owned_by(&core->menu, index)) {
            dock_menu_close(&core->menu);
            return;
        }
        /* The list's left edge sits over the icon that opened it, so the
           slot's offset is measured from the dock's own left edge and the
           whole thing is then placed at the dock's screen position. */
        int pitch = core->config.icon_size + core->config.gap;
        int row_left = core->config.padding + core->config.magnify / 2;
        int screen_x = core->x + row_left + index * pitch;
        dock_menu_open(&core->menu, core, item, index, screen_x);
        return;
    }

    /* A plain slot: any list that is up is answered and put away first, then
       the slot does the one thing it does. */
    dock_menu_close(&core->menu);

    switch (item->kind) {
    case DOCK_ITEM_SETTINGS:
        /* The settings icon opens the desktop's control panel — GnuChanSettings,
           which edits every program's settings file by clicking. When the panel
           is already open its window is in this very slot (see dock_items.c),
           and a click raises THAT window instead of launching a second panel:
           one gear, one panel, and clicking the gear again brings the open
           panel forward. Only when none is open does it launch the command.
           The command is the one the settings file names (SettingsCommand),
           which defaults to "GnuChanSettings" on PATH, so the dock does not
           have to know where the binary lives; a session whose PATH does not
           reach it can name the full path instead. */
        if (item->window_count > 0) {
            dock_core_activate(core, item->windows[0]);
        } else {
            dock_core_run_command(item->command);
        }
        break;
    case DOCK_ITEM_TERMINAL:
        dock_core_run_command(item->command);
        break;
    case DOCK_ITEM_WINDOW:
        dock_core_activate(core, item->windows[0]);
        break;
    default:
        /* A group that is somehow not a category: nothing to run. */
        break;
    }
}

static void core_handle_motion(DockCore *core, XMotionEvent *motion) {
    int index = dock_items_index_at(core, motion->x, motion->y);
    if (index != core->hover_index) {
        core->hover_index = index;
        dock_draw(core);
    }
}

static void core_handle_leave(DockCore *core) {
    if (core->hover_index != -1) {
        core->hover_index = -1;
        dock_draw(core);
    }
}

void dock_core_run(DockCore *core) {
    core->running = 1;
    int dock_fd = ConnectionNumber(core->display);

    /* The dock wakes on the changes it DRAWS: a window opening or closing,
       the client list being republished, focus moving. The root is watched
       for exactly those and nothing else. This is what replaced a loop that
       rebuilt and repainted its whole row several times a second whether or
       not anything had happened — the redraw storm that cost a core and
       pushed the X server's 2D acceleration into a GPU hang. An idle desktop
       now wakes this program not at all: it sits in select() and costs
       nothing until something it shows has actually changed. */
    XSelectInput(core->display, core->root,
                 PropertyChangeMask | SubstructureNotifyMask);
    XFlush(core->display);

    while (core->running) {
        while (XPending(core->display) > 0) {
            XEvent event;
            XNextEvent(core->display, &event);

            /* The list is a window of its own and its events arrive on the
               same connection as the dock's. It is offered each one first, so
               a click on a row is answered before the dock reads it as a click
               on the bar behind. */
            if (dock_menu_event(&core->menu, &event)) {
                continue;
            }

            switch (event.type) {
            case Expose:
                /* The dock's own pixels were uncovered: paint them at once,
                   it is not a rebuild. */
                if (event.xexpose.count == 0) {
                    dock_draw(core);
                }
                break;
            case ButtonPress:
                core_handle_click(core, &event.xbutton);
                break;
            case MotionNotify:
                core_handle_motion(core, &event.xmotion);
                break;
            case EnterNotify:
            case LeaveNotify:
                /* The pointer crossed into or out of one of the dock's own
                   windows — the dock itself, or the strip along the bottom of
                   the screen. Which way the stack goes is decided from where
                   the pointer is NOW (see core_update_hover) and not from which
                   window fired the event: the dock window and the strip overlap,
                   so their Enter and Leave arrive in pairs and in an order that
                   is not guaranteed. The icons are repainted only when the
                   pointer left the dock itself, and the dock is not dropped here
                   — core_update_hover() schedules that, with the delay. */
                if (event.type == LeaveNotify &&
                    event.xcrossing.window == core->window) {
                    core_handle_leave(core);
                }
                core_update_hover(core);
                break;
            case PropertyNotify:
                /* The client list, or which window is active, changed: the
                   row the dock draws is out of date and is rebuilt on the way
                   out of the event loop. Any other property on the root is
                   not something the dock shows. */
                if (event.xproperty.atom == core->net_client_list ||
                    event.xproperty.atom == core->net_active_window ||
                    event.xproperty.atom == core->net_current_desktop) {
                    core->dirty = 1;
                }
                break;
            case MapNotify:
            case UnmapNotify:
                /* A window appeared or left. Its own Map/Unmap arrives on the
                   dock's window too — that one is ignored, or the dock would
                   mark itself dirty every time it mapped — and anything else
                   may have changed the row. A new window is also above the
                   strip, so the strip is put back on top. */
                if (event.xany.window != core->window) {
                    core->dirty = 1;
                    core_keep_trigger_on_top(core);
                }
                break;
            case ConfigureNotify:
                /* A window was moved, resized or RAISED, and a raise is what
                   puts one above the strip along the bottom of the screen — see
                   core_keep_trigger_on_top. A move during a drag is the same
                   ConfigureNotify, and the raise it causes is one request the
                   server batches with the rest, so a drag costs no round trip.
                   The dock's OWN windows are skipped: raising our strip arrives
                   right back here, and raising on it would be a loop that never
                   ends. The row is not marked dirty — nothing the dock DRAWS has
                   changed, only the stack. */
                if (event.xconfigure.window != core->window &&
                    event.xconfigure.window != core->trigger) {
                    core_keep_trigger_on_top(core);
                }
                break;
            case DestroyNotify:
                if (event.xdestroywindow.window == core->window) {
                    core->running = 0;
                } else {
                    core->dirty = 1;
                }
                break;
            default:
                break;
            }
        }

        if (!core->running) {
            break;
        }

        /* Rebuild and repaint only when something the row shows changed.
           Between changes this is never reached, and the loop falls into the
           wait below with nothing to draw. */
        if (core->dirty) {
            dock_core_refresh(core);
        }

        /* Wait for the next thing the server has to say, and for NOTHING ELSE.
         *
         * There is no timer here, and that is the point. The dock draws what
         * the screen has open, and the screen does not change on a clock: no
         * window opens because a second passed. A timeout would wake this
         * program every so often to rebuild and repaint a row that is already
         * correct — the redraw storm the event-driven loop above was written
         * to end — and on a two-core laptop with no GPU acceleration that is a
         * core spent redrawing a picture that never changed.
         *
         * NULL as the timeout blocks until an event arrives. Every change the
         * dock shows — a window opening or closing, the client list or the
         * active window changing — is delivered on this connection and wakes
         * the select. An idle desktop wakes this program not at all. */
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(dock_fd, &read_set);

        /* The wait is forever unless a hide is pending, in which case it is
           only until the delay runs out. That is the one timer the dock has,
           and it exists only while the dock is about to be put away — an idle
           desktop still wakes this program not at all. */
        struct timeval timeout;
        struct timeval *wait = NULL;
        if (core->hide_pending) {
            unsigned long now = core_now_ms();
            long remaining = (long)core->hide_at_ms - (long)now;
            if (remaining < 0) {
                remaining = 0;
            }
            timeout.tv_sec = remaining / 1000;
            timeout.tv_usec = (remaining % 1000) * 1000;
            wait = &timeout;
        }

        int ready = select(dock_fd + 1, &read_set, NULL, NULL, wait);
        if (ready == 0) {
            /* The hide delay ran out with nothing else to do: put it away. */
            if (core->hide_pending && core_now_ms() >= core->hide_at_ms) {
                core_hide(core);
            }
            continue;
        }
        if (ready < 0) {
            /* A signal, or a connection that has gone: either way the next
             * pass of the loop re-reads whatever it can and waits again. */
            continue;
        }
    }
}

int dock_core_init(DockCore *core) {
    memset(core, 0, sizeof(*core));
    core->hover_index = -1;

    XSetErrorHandler(core_error);

    core->display = XOpenDisplay(NULL);
    if (!core->display) {
        fprintf(stderr, "gnuchandock: cannot open a display\n");
        return -1;
    }
    core->screen = DefaultScreen(core->display);
    core->root = RootWindow(core->display, core->screen);
    core->visual = DefaultVisual(core->display, core->screen);
    core->depth = DefaultDepth(core->display, core->screen);
    core->colormap = DefaultColormap(core->display, core->screen);

    core->net_client_list = XInternAtom(core->display, "_NET_CLIENT_LIST", False);
    core->net_wm_name = XInternAtom(core->display, "_NET_WM_NAME", False);
    core->utf8_string = XInternAtom(core->display, "UTF8_STRING", False);
    core->net_active_window = XInternAtom(core->display, "_NET_ACTIVE_WINDOW",
                                          False);
    core->net_wm_window_type = XInternAtom(core->display, "_NET_WM_WINDOW_TYPE",
                                           False);
    core->net_wm_window_type_dock =
        XInternAtom(core->display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    core->net_wm_window_type_desktop =
        XInternAtom(core->display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    core->net_current_desktop =
        XInternAtom(core->display, "_NET_CURRENT_DESKTOP", False);
    core->net_wm_desktop = XInternAtom(core->display, "_NET_WM_DESKTOP", False);
    core->net_wm_state = XInternAtom(core->display, "_NET_WM_STATE", False);
    core->net_wm_state_fullscreen =
        XInternAtom(core->display, "_NET_WM_STATE_FULLSCREEN", False);

    dock_config_load_default(&core->config);
    core_resolve_palette(core);
    core_open_font(core);

    core->gc = XCreateGC(core->display, core->root, 0, NULL);

    core->width = 1;
    core->height = 1;
    core->x = 0;
    core->y = 0;
    core->window = core_create_window(core);
    if (core->window == None) {
        fprintf(stderr, "gnuchandock: cannot create the window\n");
        return -1;
    }
    core_set_window_properties(core);

    /* The strip along the very bottom of the screen that brings the dock back.
       It is an InputOnly window — it is never drawn, only felt — and it is the
       pointer's target for the whole time the dock is away. */
    {
        int screen_width = DisplayWidth(core->display, core->screen);
        int screen_height = DisplayHeight(core->display, core->screen);
        XSetWindowAttributes trigger_attributes;
        memset(&trigger_attributes, 0, sizeof(trigger_attributes));
        trigger_attributes.override_redirect = True;
        trigger_attributes.event_mask = EnterWindowMask | LeaveWindowMask;
        core->trigger = XCreateWindow(
            core->display, core->root,
            0, screen_height - DOCK_TRIGGER_HEIGHT,
            (unsigned int)screen_width, (unsigned int)DOCK_TRIGGER_HEIGHT,
            0, 0, InputOnly, CopyFromParent,
            CWOverrideRedirect | CWEventMask, &trigger_attributes);
        if (core->trigger != None) {
            XMapRaised(core->display, core->trigger);
        }
    }

    core_load_fixed_icons(core);
    dock_items_build(core);
    dock_core_relayout(core);

    XMapWindow(core->display, core->window);
    /* The dock starts BEHIND the windows, and the strip starts on top: a dock
       that came up in front would be over the desktop the moment the session
       began, which is the thing the auto-hide exists to avoid. */
    XLowerWindow(core->display, core->window);
    if (core->trigger != None) {
        XRaiseWindow(core->display, core->trigger);
    }
    XFlush(core->display);
    return 0;
}

void dock_core_shutdown(DockCore *core) {
    dock_menu_close(&core->menu);
    dock_items_clear(core);
    /* The per-program icons the cache owns, which dock_items_clear() left
       alone: every pixmap must be freed while the display is still open. */
    dock_items_free_icons(core);
    dock_icon_free(core->display, &core->settings_icon);
    dock_icon_free(core->display, &core->terminal_icon);
    core_close_font(core);
    if (core->gc && core->display) {
        XFreeGC(core->display, core->gc);
        core->gc = NULL;
    }
    if (core->trigger != None && core->display) {
        XDestroyWindow(core->display, core->trigger);
        core->trigger = None;
    }
    if (core->window != None && core->display) {
        XDestroyWindow(core->display, core->window);
        core->window = None;
    }
    if (core->display) {
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
