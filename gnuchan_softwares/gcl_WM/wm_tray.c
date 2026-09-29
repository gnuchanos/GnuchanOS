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
 *
 * --- the dock's depth ------------------------------------------------------
 *
 * The dock and every icon in it have to be the same depth: XReparentWindow
 * refuses a mismatch with BadMatch, and the depth a program makes its icon
 * window at is the program's own choice. There are two answers in the wild,
 * and they are not the same program:
 *
 *   the screen's own depth      Steam, and every program that is not a toolkit
 *                                — it makes an ordinary window and expects the
 *                                manager to take it as it is
 *   32 bits with an alpha        GTK, and the toolkits, when the tray tells
 *   channel                      them one exists
 *
 * This used to prefer the 32-bit visual and then refuse anything else, which
 * is exactly backwards: the program that ignores the hint is the one that ends
 * up with no icon at all, and the group box on the bar stays empty with
 * nothing on the screen to say why.
 *
 * So the dock is made at the SCREEN'S OWN visual and that is what
 * _NET_SYSTEM_TRAY_VISUAL publishes. A toolkit reads it and makes its icon to
 * match, so the two agree before either has to give way. A program that makes
 * its icon at another depth anyway is met on its own terms: the dock is made
 * again at that depth while it is still empty — see tray_dock_icon — because
 * the client's own answer beats a hint it did not take. A dock that already
 * holds icons cannot change its depth under them, and an icon of a second
 * depth is refused and said so in the log; one dock is one depth, and that is
 * a rule of X rather than a choice made here.
 */
#include <stdio.h>
#include <string.h>

#include "wm_core.h"
#include "wm_desktop.h"
#include "wm_switcher.h"
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

/* The visual and depth the dock and its icons are drawn at, and the colormap
   that goes with them. The colormap is made here when the visual is not the
   screen's own, and it is this process's to free: one made and forgotten is a
   resource held on the server for the life of the session, and the dock is
   made again whenever a program asks for another depth. */
static Visual *tray_visual = NULL;
static int tray_depth = 0;
static Colormap tray_colormap = None;

/* The direction the icons are laid out in, as the protocol's own number: 0 is
   horizontal and 1 is vertical. Published on the dock so a program sizing its
   icon knows which way the tray runs, and set from the bar's own pose by
   wm_tray_place(). */
static long tray_orientation = 0;

/* --- the dock window ------------------------------------------------------ */

/* A dock and the colormap it was made with. A pair rather than two globals
   because the dock is made more than once — at start, and again when a
   program's icon asks for another depth — and the old one has to be taken
   down in the right order: the window first, and the colormap only once
   nothing is drawing through it. */
typedef struct TrayDock {
    Window window;
    Colormap colormap;
    int colormap_owned;   /* ours to free, or the screen's to keep */
} TrayDock;

/* Make a dock window at the current visual. It starts 1x1 and is placed by
   wm_tray_place() the first time a bar draws a group box: what a program needs
   is that the window exists and that its owner owns the selection, not that it
   is anywhere yet. Returns 0 on success. */
static int tray_create_dock(WmCore *core, TrayDock *dock) {
    memset(dock, 0, sizeof(*dock));

    if (tray_visual == DefaultVisual(core->display, core->screen)) {
        /* The screen's own colormap already goes with this visual, and one
           made beside it would be a second palette for the same screen. */
        dock->colormap = DefaultColormap(core->display, core->screen);
        dock->colormap_owned = 0;
    } else {
        dock->colormap = XCreateColormap(core->display, core->root,
                                         tray_visual, AllocNone);
        dock->colormap_owned = 1;
        if (dock->colormap == None) {
            fprintf(stderr, "gnuchanwm: tray: cannot make a colormap\n");
            return -1;
        }
    }

    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.override_redirect = True;
    attributes.background_pixel = core->style.panel;
    attributes.event_mask = StructureNotifyMask | ExposureMask;
    attributes.colormap = dock->colormap;

    dock->window = XCreateWindow(core->display, core->root,
                                 0, 0, 1, 1, 0,
                                 (unsigned int)tray_depth, InputOutput,
                                 tray_visual,
                                 CWOverrideRedirect | CWBackPixel |
                                 CWEventMask | CWColormap, &attributes);
    if (dock->window == None) {
        fprintf(stderr, "gnuchanwm: tray: cannot make the dock window\n");
        if (dock->colormap_owned) {
            XFreeColormap(core->display, dock->colormap);
        }
        memset(dock, 0, sizeof(*dock));
        return -1;
    }
    return 0;
}

/* Take a dock down: the window first, and the colormap only after it, so
   nothing is ever drawing through a colormap that has been freed. */
static void tray_discard_dock(WmCore *core, TrayDock *dock) {
    if (dock->window != None) {
        XDestroyWindow(core->display, dock->window);
        dock->window = None;
    }
    if (dock->colormap_owned && dock->colormap != None) {
        XFreeColormap(core->display, dock->colormap);
    }
    dock->colormap = None;
    dock->colormap_owned = 0;
}

/* --- the visual ----------------------------------------------------------- */

/* The visual the dock is drawn at.
 *
 * It starts as the screen's own, and that is the whole fix for a program whose
 * icon never appeared: a toolkit asks the tray what visual to use by reading
 * _NET_SYSTEM_TRAY_VISUAL, but Steam does not ask — it makes an ordinary
 * window at the screen's depth and sends it. A dock at 32 bits cannot take a
 * window at 24, and the icon was dropped with a line in a log nobody reads.
 * Starting at the screen's own depth is what makes the program that does not
 * ask work, and the toolkit that does ask agrees with it. */
static void tray_choose_visual(WmCore *core) {
    if (tray_visual && tray_depth) {
        return;
    }
    tray_visual = DefaultVisual(core->display, core->screen);
    tray_depth = DefaultDepth(core->display, core->screen);
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
   icons are laid out, which visual to make an icon window with, and how big an
   icon is expected to be. */
static void tray_publish(WmCore *core) {
    VisualID visual_id;
    long icon_size = 24;

    if (tray_window == None) {
        return;
    }
    visual_id = XVisualIDFromVisual(tray_visual);

    if (core->config.bar_count > 0 && core->config.bars[0].size > 0) {
        icon_size = core->config.bars[0].size;
    }

    XChangeProperty(core->display, tray_window,
                    wm_atom(core, "_NET_SYSTEM_TRAY_ORIENTATION"),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&tray_orientation, 1);

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

/* Make the dock again at another depth, for a program that made its icon at
   one of its own.
 *
 * Only possible while the dock is empty, and that is a rule of X rather than a
 * precaution: a docked icon is a child of the dock, and a child can only be
 * moved to a parent of its own depth, so a dock that already holds icons
 * cannot change its depth under them. Returns 0 when the dock was made again
 * and the icon can be taken.
 *
 * The selection is moved to the new dock before the old one is destroyed, so
 * there is no moment when the tray has no owner and a program looking for it
 * finds none. */
static int tray_recreate_dock(WmCore *core, Visual *visual, int depth) {
    TrayDock old;
    TrayDock fresh;

    if (tray_icon_count > 0 || !visual || depth <= 0) {
        return -1;
    }

    old.window = tray_window;
    old.colormap = tray_colormap;
    old.colormap_owned = tray_colormap != None &&
                         tray_colormap != DefaultColormap(core->display,
                                                          core->screen);

    tray_visual = visual;
    tray_depth = depth;
    if (tray_create_dock(core, &fresh) != 0) {
        /* The old dock is still the tray: nothing about it was changed. */
        tray_visual = DefaultVisual(core->display, core->screen);
        tray_depth = DefaultDepth(core->display, core->screen);
        return -1;
    }

    tray_window = fresh.window;
    tray_colormap = fresh.colormap;
    XSetSelectionOwner(core->display, tray_selection, tray_window, CurrentTime);
    tray_discard_dock(core, &old);

    tray_publish(core);
    tray_announce(core);
    fprintf(stderr, "gnuchanwm: tray: the dock was made again at depth %d, "
                    "for a program's own icon\n", depth);
    return 0;
}

/* Take a program's icon window into the dock.
 *
 * The icon is reparented here and moved into place by the next layout, which
 * is the same call that places every other icon — so an icon that docks while
 * others are already there does not work out where it goes itself.
 *
 * A window whose depth does not match the dock's cannot be reparented into it:
 * the server refuses with BadMatch. The two ways out are taken in order of how
 * much they cost: the dock is made again at the icon's depth when it is still
 * empty, which is the case for the first program to dock and so for the one
 * that sets the depth for the session; and an icon of a second depth arriving
 * after that is refused, with the reason logged rather than the icon silently
 * going nowhere. */
static void tray_dock_icon(WmCore *core, Window icon) {
    XWindowAttributes attributes;

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

    if (!XGetWindowAttributes(core->display, icon, &attributes)) {
        return;
    }
    if (attributes.depth != tray_depth) {
        if (tray_recreate_dock(core, attributes.visual, attributes.depth) != 0) {
            fprintf(stderr,
                    "gnuchanwm: tray: an icon at depth %d cannot dock into a "
                    "tray at depth %d\n", attributes.depth, tray_depth);
            return;
        }
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
        } else {
            XMoveResizeWindow(core->display, icon, cursor, (height - side) / 2,
                              (unsigned int)side, (unsigned int)side);
        }
        cursor += side + WM_TRAY_GAP;
        XMapWindow(core->display, icon);
    }
}

void wm_tray_place(WmCore *core, Window bar, int vertical, int x, int y,
                   int width, int height, unsigned long background) {
    int screen_x;
    int screen_y;
    Window child = None;

    if (!core || tray_window == None || bar == None) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }

    /* The bar's pose is what the protocol's orientation means, and a program
       that reads it lays its own icon out to match. It is published when it
       changes rather than on every repaint, because a property write per
       second per bar is work for an answer that almost never moves. */
    long orientation = vertical ? 1 : 0;
    if (orientation != tray_orientation) {
        tray_orientation = orientation;
        tray_publish(core);
    }

    /* The cell's place inside the bar, as a place on the screen. The dock is
       the root's child, so this is the one conversion between the two spaces. */
    screen_x = x;
    screen_y = y;
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

    /* And over the switcher's overlay only if that is not up: the tray
       belongs over the bar, and the switcher's pictures belong over the
       tray. */
    wm_switcher_raise(core);
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

static int tray_init(WmCore *core) {
    char selection_name[64];
    TrayDock dock;
    Window owner;

    if (!tray_wanted(core)) {
        fprintf(stderr,
                "gnuchanwm: tray: no bar is configured, so no tray is offered\n");
        return 0;
    }

    tray_choose_visual(core);

    snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d",
             core->screen);
    tray_selection = wm_atom(core, selection_name);
    tray_opcode = wm_atom(core, "_NET_SYSTEM_TRAY_OPCODE");

    /* The dock is created whether or not a bar currently draws a group box,
       because a program may dock at any time and the window it docks into has
       to exist. */
    if (tray_create_dock(core, &dock) != 0) {
        return 0;
    }
    tray_window = dock.window;
    tray_colormap = dock.colormap;

    tray_publish(core);

    /* The claim itself. Owning _NET_SYSTEM_TRAY_S<screen> is what makes this
       the tray: a program looks the selection up, finds the dock window as its
       owner, and sends its icon there. Another tray already holding the
       selection is left alone — two trays fighting over one selection is one
       of them losing icons at random. */
    owner = XGetSelectionOwner(core->display, tray_selection);
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
        if (tray_colormap != None &&
            tray_colormap != DefaultColormap(core->display, core->screen)) {
            XFreeColormap(core->display, tray_colormap);
        }
        tray_colormap = None;
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
