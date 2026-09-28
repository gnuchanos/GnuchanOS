/*
 * wm_tray.c — the freedesktop system tray, so a running program can leave an
 * icon on the bar.
 *
 * The protocol is small and is written out in wm_tray.h. What this file does
 * is the three things the protocol needs:
 *
 *   1. Claim _NET_SYSTEM_TRAY_S<screen>. That selection is the tray: a program
 *      that wants to dock finds its owner and sends it a message. Claiming it
 *      is also what tells every program already running that a tray exists,
 *      through the MANAGER ClientMessage sent round the root when the claim is
 *      made — a program started before the session's tray would otherwise
 *      never look again.
 *
 *   2. Own a dock window, a child of the root, that the icons are reparented
 *      into. It is the root's child and not a bar's because a window's
 *      children must share its depth, and an icon may be drawn at a depth the
 *      bar is not — 32 bits with an alpha channel, on a server that has one.
 *
 *   3. Answer SYSTEM_TRAY_REQUEST_DOCK by taking the program's icon window
 *      into the dock, and lay the docked icons out along the bar.
 *
 * Nothing here draws an icon. The icon is a window the program owns, the
 * program draws it, and a click on it reaches the program — which is what
 * makes the click open the program's own menu rather than one this manager
 * would have to invent. The manager's whole part is the room.
 */
#include <stdio.h>
#include <string.h>

#include "wm_core.h"
#include "wm_desktop.h"
#include "wm_tray.h"

/* The opcodes the tray protocol defines in _NET_SYSTEM_TRAY_OPCODE. Only the
   first is acted on; the other two are a program's balloon message, which this
   tray does not draw, and are named so a message that is one is not mistaken
   for a dock request. */
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

/* The tray's own state. One tray exists, so its state lives here rather than
   on the core: a second tray is not a thing that can exist, and the selection
   is the machine's answer to "where is the tray" — one owner at a time. */
static Window tray_selection = None;   /* _NET_SYSTEM_TRAY_S<screen>         */
static Window tray_window = None;      /* the dock the icons are put into    */
static Atom   tray_opcode = None;      /* _NET_SYSTEM_TRAY_OPCODE            */

/* The docked icons, in the order they arrived. Every one is another process's
   window; what is kept here is the window id and nothing else, because
   everything about an icon — its size, its picture, what clicking it does — is
   the program's own. */
static Window tray_icons[WM_TRAY_MAX_ICONS];
static int tray_icon_count = 0;

/* Whether anything asked the tray where to sit this time round. Set by
   wm_tray_place() and cleared by wm_tray_begin(), and read by
   wm_tray_finish(): a bar that drew no group box leaves it clear, and the dock
   is then put away rather than left where it last was. */
static int tray_placed = 0;

/* The visual the dock and its icons are drawn at. It is the screen's own
   unless the server has a 32-bit TrueColor visual, which is preferred because
   an icon with an alpha channel can only be embedded where the depth matches —
   and every modern program's tray icon has one. */
static Visual *tray_visual = NULL;
static int tray_depth = 0;

/* --- the visual ----------------------------------------------------------- */

/* The best visual for a tray icon: 32 bits, TrueColor. This is the visual
   _NET_SYSTEM_TRAY_VISUAL publishes, and a client that reads it makes its icon
   window with it — so the two agree and the reparent below is allowed. When
   the server has none (a plain X server with no composite extension) the
   screen's own visual is used and icons are opaque, which is what they are on
   such a server anyway. */
static void tray_choose_visual(WmCore *core) {
    XVisualInfo wanted;
    XVisualInfo *found = NULL;
    int count = 0;

    if (tray_visual && tray_depth) {
        return;
    }

    memset(&wanted, 0, sizeof(wanted));
    wanted.screen = core->screen;
    wanted.depth = 32;
    wanted.class = TrueColor;
    found = XGetVisualInfo(core->display,
                           VisualScreenMask | VisualDepthMask | VisualClassMask,
                           &wanted, &count);
    if (found && count > 0) {
        tray_visual = found[0].visual;
        tray_depth = 32;
    }
    if (found) {
        XFree(found);
    }
    if (!tray_visual) {
        tray_visual = DefaultVisual(core->display, core->screen);
        tray_depth = DefaultDepth(core->display, core->screen);
    }
}

/* --- the icons ------------------------------------------------------------ */

/* Whether an icon is already docked. A program that asks twice is not given
   two places: the second dock request for the same window is the same icon. */
static int tray_has_icon(Window icon) {
    for (int i = 0; i < tray_icon_count; i++) {
        if (tray_icons[i] == icon) {
            return 1;
        }
    }
    return 0;
}

/* Forget an icon that is gone, so the room the tray asks for does not count a
   window the server no longer has. */
static void tray_forget_icon(Window icon) {
    for (int i = 0; i < tray_icon_count; i++) {
        if (tray_icons[i] == icon) {
            for (int j = i + 1; j < tray_icon_count; j++) {
                tray_icons[j - 1] = tray_icons[j];
            }
            tray_icon_count--;
            return;
        }
    }
}

/* Put every icon back on the root. Used by the cleanup, where the dock is
   about to be destroyed: an icon left inside a window that goes away would be
   a program's window with a dead parent, which the server does not allow. */
static void tray_release_icons(WmCore *core) {
    for (int i = 0; i < tray_icon_count; i++) {
        XReparentWindow(core->display, tray_icons[i], core->root, 0, 0);
    }
    tray_icon_count = 0;
}

/* Take a program's icon window into the dock.
 *
 * The icon is reparented here and moved into place by the next layout, which
 * is the same call that places every other icon — so an icon that docks while
 * others are already there does not work out where it goes itself.
 *
 * A window whose depth does not match the dock's cannot be reparented into it:
 * the server refuses with BadMatch. That is checked first, because the
 * alternative is an X error per dock request, and the reason is logged rather
 * than the icon silently going nowhere. */
static void tray_dock_icon(WmCore *core, Window icon) {
    if (icon == None || icon == tray_window) {
        return;
    }
    if (tray_has_icon(icon)) {
        return;
    }
    if (tray_icon_count >= WM_TRAY_MAX_ICONS) {
        fprintf(stderr, "gnuchanwm: tray: no room for another icon\n");
        return;
    }

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, icon, &attributes)) {
        return;
    }
    if (attributes.depth != tray_depth) {
        fprintf(stderr,
                "gnuchanwm: tray: an icon at depth %d cannot dock into a "
                "tray at depth %d\n", attributes.depth, tray_depth);
        return;
    }

    XReparentWindow(core->display, icon, tray_window, 0, 0);
    /* The icon is watched for going away, so its place is given up when it
       does: a destroyed icon and one only unmapped look the same in the array,
       and a tray that kept a dead window would ask for room it does not use. */
    XSelectInput(core->display, icon, StructureNotifyMask);
    XMapWindow(core->display, icon);

    tray_icons[tray_icon_count++] = icon;

    /* The bar is drawn again so the room the tray takes is worked out afresh:
       the group box is measured from the number of docked icons, and without
       this an icon that docked after the last repaint would draw over whatever
       the bar had next to it. */
    wm_desktop_repaint(core);
}

/* --- laying the icons out -------------------------------------------------- */

/* The side an icon is drawn at: the bar's thickness, so the icon fills the
   strip it sits in. Both the room the bar measures and the size each icon is
   moved to come from here, so the two are the same number. */
static int tray_icon_side(int thickness) {
    int side = thickness;
    if (side < 8) {
        side = 8;
    }
    return side;
}

int wm_tray_extent(WmCore *core, int thickness) {
    int side = tray_icon_side(thickness);
    (void)core;
    if (tray_icon_count <= 0) {
        return 0;
    }
    return tray_icon_count * side + (tray_icon_count - 1) * WM_TRAY_GAP;
}

/* Put every icon inside the dock, in a row along the bar's own axis. The
   icons are moved inside the dock, which is the dock's own coordinate space —
   the dock itself is placed by wm_tray_place() below. */
static void tray_layout(WmCore *core, int vertical, int width, int height,
                        int thickness) {
    int side = tray_icon_side(thickness);
    int cursor = 0;

    for (int i = 0; i < tray_icon_count; i++) {
        Window icon = tray_icons[i];
        if (vertical) {
            XMoveResizeWindow(core->display, icon, (width - side) / 2, cursor,
                              (unsigned int)side, (unsigned int)side);
            cursor += side + WM_TRAY_GAP;
        } else {
            XMoveResizeWindow(core->display, icon, cursor, (height - side) / 2,
                              (unsigned int)side, (unsigned int)side);
            cursor += side + WM_TRAY_GAP;
        }
        XMapWindow(core->display, icon);
    }
}

void wm_tray_place(WmCore *core, Window bar, int vertical, int x, int y,
                   int width, int height, unsigned long background) {
    if (!core || tray_window == None || bar == None) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }

    /* The cell's place inside the bar, as a place on the screen. The dock is
       the root's child, so this is the one conversion between the two spaces. */
    int screen_x = x;
    int screen_y = y;
    Window child = None;
    XTranslateCoordinates(core->display, bar, core->root, x, y,
                          &screen_x, &screen_y, &child);

    XMoveResizeWindow(core->display, tray_window, screen_x, screen_y,
                      (unsigned int)width, (unsigned int)height);

    /* The dock is painted in the cell's own colour, so it is the bar's strip
       that shows behind the icons rather than a rectangle of its own — the
       difference between a tray that is part of the bar and a box stuck on
       it. */
    XSetWindowBackground(core->display, tray_window, background);
    XClearWindow(core->display, tray_window);

    /* Above the bar: the dock is the root's child while the bar sits at the
       bottom of the stack, so without this the bar's strip would cover the
       icons. */
    XRaiseWindow(core->display, tray_window);
    XMapWindow(core->display, tray_window);

    tray_layout(core, vertical, width, height, vertical ? width : height);
    tray_placed = 1;
    XFlush(core->display);
}

void wm_tray_begin(WmCore *core) {
    (void)core;
    tray_placed = 0;
}

void wm_tray_finish(WmCore *core) {
    if (!core || tray_window == None) {
        return;
    }
    if (!tray_placed) {
        /* Nothing asked for the tray this time: the group box is in no bar, so
           the dock has no place to be and is put away. The icons are left
           where they are — they are the programs' windows, and the tray comes
           back when a bar draws a group box again. */
        XUnmapWindow(core->display, tray_window);
    }
    XFlush(core->display);
}

void wm_tray_raise(WmCore *core) {
    if (!core || tray_window == None || !tray_placed) {
        return;
    }
    XRaiseWindow(core->display, tray_window);
    XFlush(core->display);
}

int wm_tray_is_window(Window window) {
    return window != None && window == tray_window;
}

/* --- the protocol ---------------------------------------------------------- */

/* Whether a tray is wanted at all: a session with no bar has nowhere to put an
   icon, so a tray that claimed the selection would take a program's icon and
   have no room to show it — worse than no tray, because the program would then
   believe it was reachable. Asked once, by the module's init. */
static int tray_wanted(WmCore *core) {
    return core && core->config.bar_count > 0;
}

/* Tell every program on the display that a tray now exists.
 *
 * This is the MANAGER message, and it is not optional: a program started
 * before the tray — which is every program of a session that has been running
 * for a while — found no owner when it looked, and this is what tells it to
 * look again. The message carries the tray's selection atom and the dock
 * window, which is what the program answers. */
static void tray_announce(WmCore *core) {
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = core->root;
    event.xclient.message_type = wm_atom(core, "MANAGER");
    event.xclient.format = 32;
    event.xclient.data.l[0] = (long)CurrentTime;
    event.xclient.data.l[1] = (long)tray_selection;
    event.xclient.data.l[2] = (long)tray_window;

    XSendEvent(core->display, core->root, False, StructureNotifyMask, &event);
    XFlush(core->display);
}

/* Publish on the dock what the protocol says a program may read: which way the
   icons are laid out, and which visual to make an icon window with. */
static void tray_publish(WmCore *core) {
    long orientation = 0;   /* 0 is horizontal, 1 is vertical */
    VisualID visual_id = XVisualIDFromVisual(tray_visual);
    long icon_size = 24;

    if (core->config.bar_count > 0 && core->config.bars[0].size > 0) {
        icon_size = core->config.bars[0].size;
    }

    XChangeProperty(core->display, tray_window,
                    wm_atom(core, "_NET_SYSTEM_TRAY_ORIENTATION"),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&orientation, 1);

    XChangeProperty(core->display, tray_window,
                    wm_atom(core, "_NET_SYSTEM_TRAY_VISUAL"),
                    XA_VISUALID, 32, PropModeReplace,
                    (unsigned char *)&visual_id, 1);

    /* What a program sizes its icon to. It is the bar's own thickness, which
       is what every icon on this bar is drawn at. */
    XChangeProperty(core->display, tray_window,
                    wm_atom(core, "_NET_SYSTEM_TRAY_ICON_SIZE"),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&icon_size, 1);
}

static int tray_init(WmCore *core) {
    if (!tray_wanted(core)) {
        fprintf(stderr,
                "gnuchanwm: tray: no bar is configured, so no tray is offered\n");
        return 0;
    }

    tray_choose_visual(core);

    char selection_name[64];
    snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d",
             core->screen);
    tray_selection = wm_atom(core, selection_name);
    tray_opcode = wm_atom(core, "_NET_SYSTEM_TRAY_OPCODE");

    /* The dock, at the screen's own depth unless a 32-bit visual was found —
       see tray_choose_visual(). It is created whether or not a bar currently
       draws a group box, because a program may dock at any time and the window
       it docks into has to exist. */
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.panel;
    attributes.event_mask = StructureNotifyMask | ExposureMask;
    attributes.colormap = XCreateColormap(core->display, core->root,
                                          tray_visual, AllocNone);

    tray_window = XCreateWindow(core->display, core->root,
                                0, 0, 1, 1, 0,
                                tray_depth, InputOutput, tray_visual,
                                CWOverrideRedirect | CWBackPixel |
                                CWEventMask | CWColormap, &attributes);
    if (tray_window == None) {
        fprintf(stderr, "gnuchanwm: tray: cannot make the dock window\n");
        XFreeColormap(core->display, attributes.colormap);
        return 0;
    }

    tray_publish(core);

    /* The claim itself. Owning _NET_SYSTEM_TRAY_S<screen> is what makes this
       the tray: a program looks the selection up, finds the dock window as its
       owner, and sends its icon there. Another tray already holding the
       selection is left alone — two trays fighting over one selection is one
       of them losing icons at random. */
    Window owner = XGetSelectionOwner(core->display, tray_selection);
    if (owner != None) {
        fprintf(stderr,
                "gnuchanwm: tray: %s is already owned by 0x%lx; not claiming "
                "it\n", selection_name, (unsigned long)owner);
    } else {
        XSetSelectionOwner(core->display, tray_selection, tray_window,
                           CurrentTime);
        if (XGetSelectionOwner(core->display, tray_selection) != tray_window) {
            fprintf(stderr, "gnuchanwm: tray: the claim on %s was refused\n",
                    selection_name);
        } else {
            tray_announce(core);
            fprintf(stderr, "gnuchanwm: tray: offering %s at depth %d\n",
                    selection_name, tray_depth);
        }
    }

    XFlush(core->display);
    return 0;
}

static void tray_cleanup(WmCore *core) {
    if (!core || !core->display) {
        return;
    }
    /* Every icon goes back to the root before the dock does: an icon left
       inside a window that is destroyed with it would be a program's window
       with a dead parent. */
    tray_release_icons(core);

    if (tray_window != None) {
        if (XGetSelectionOwner(core->display, tray_selection) == tray_window) {
            XSetSelectionOwner(core->display, tray_selection, None,
                               CurrentTime);
        }
        XDestroyWindow(core->display, tray_window);
        tray_window = None;
    }
    tray_placed = 0;
    XFlush(core->display);
}

static void tray_event(WmCore *core, XEvent *event) {
    if (tray_window == None) {
        return;
    }

    switch (event->type) {
    case ClientMessage:
        /* A dock request, from a program that found the selection's owner. The
           message is a ClientMessage of _NET_SYSTEM_TRAY_OPCODE whose third
           word is the icon window. Any other opcode is a program's balloon
           message, which this tray does not draw. */
        if (event->xclient.message_type != tray_opcode) {
            break;
        }
        if ((long)event->xclient.data.l[1] != SYSTEM_TRAY_REQUEST_DOCK) {
            break;
        }
        tray_dock_icon(core, (Window)event->xclient.data.l[2]);
        break;

    case DestroyNotify:
        /* An icon that has gone. It is forgotten, so the room the tray asks
           for does not count it, and the bar is drawn again so the room it
           takes shrinks with it. */
        if (tray_has_icon(event->xdestroywindow.window)) {
            tray_forget_icon(event->xdestroywindow.window);
            wm_desktop_repaint(core);
        }
        break;

    case ReparentNotify:
        /* An icon that left the dock — the program took it back, or moved it
           somewhere else. It is dropped for the same reason a destroyed one
           is, unless the reparent is the one this module just made. */
        if (event->xreparent.parent != tray_window &&
            tray_has_icon(event->xreparent.window)) {
            tray_forget_icon(event->xreparent.window);
            wm_desktop_repaint(core);
        }
        break;

    default:
        break;
    }
}

const WmModule wm_tray_module = {
    .name = "tray",
    .init = tray_init,
    .event = tray_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = tray_cleanup,
};
