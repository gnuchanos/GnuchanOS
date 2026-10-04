/*
 * wm_frame.c — the frame a managed window lives in: the table, what a frame is
 * made of, and the changes that move or resize it.
 *
 * The shape of a frame is fixed and simple:
 *
 *     +--------------------------------------+
 *     | title          [-] [ ] [x]           |  WM_TITLE_HEIGHT
 *     +--------------------------------------+
 *     |                                      |
 *     |            the client                |  client_height
 *     |                                      |
 *     +--------------------------------------+
 *
 * The title bar is drawn on the frame itself rather than on a window of its
 * own. One window means one place to click, one exposure to answer and one
 * thing to move, and a bar that is 22 pixels tall needs nothing more.
 *
 * The client is placed at (WM_FRAME_BORDER, WM_TITLE_HEIGHT) inside the frame
 * and is never told about the bar: reparenting is what makes that work. The
 * program still believes it is a window on the root, because the X server
 * translates its coordinates, and every window manager relies on that.
 *
 * --- this file was four files' worth of work and is now four -----------------
 *
 * The frame module grew into one file doing four unrelated things, which is how
 * a change to the way a window is dragged ends up able to break the way one is
 * created. It is split along the lines the code already had:
 *
 *   wm_frame_geometry.c    where the chrome is and how large a frame is, as
 *                          arithmetic over a WmFrame — no window, no display
 *   wm_frame_draw.c        what a frame looks like: the bar, the border, the
 *                          title and the three buttons
 *   wm_frame_interact.c    what a click, a drag, a resize and an exposure do,
 *                          which is the only part that reads X events
 *   wm_frame_fullscreen.c  the two states that change what the client IS:
 *                          fullscreen, and fitted-to-the-frame
 *
 * This file keeps the rest: the frame table, the frame's name, the geometry
 * APPLIED to the windows (frame_apply and the clamps around it), the state
 * changes that only move a window, and creating and destroying a frame. The
 * helpers those files share are declared in wm_frame_internal.h.
 */
/* ============================================================================
 * !! KRITIK UYARILAR — BU IKI HATAYI BIR DAHA CIKARMA !!
 *
 * Bu iki sorun defalarca bildirildi ve ikisi de SU IKI kurala baglidir. Kodda
 * bir sey "sadelestirirken" bunlari bozma.
 *
 * (1) "WINE ILE OYUN ACINCA TITLE BAR VE BORDER GORUNMUYOR."
 *     SEBEP: client, frame'in COCUK penceresidir ve frame'i ORTER. Bir Wine
 *     fullscreen oyunu kendini root'ta sanip her karede (0,0)'a tasinir.
 *     Client (0,0)'da kalirsa frame'in baslik cubugunu ve kenarligini
 *     tamamen kapatir.
 *     KURAL: chrome (baslik + kenarlik) HER pencerede, fullscreen dahil,
 *     KORUNUR. frame_border_of() ve frame_title_of() HER ZAMAN gercek
 *     chrome'u dondurur; HICBIR yerde bu degerleri fullscreen'de sifira
 *     indirme. fullscreen bayragi chrome'u DUSURMEZ.
 *     Ayrica client, frame icinde (border, title) konumunda tutulur; onu
 *     (0,0)'a tasima.
 *
 * (2) "OYUN ACILINCA TITLE BAR DAHIL FLICKER OLUYOR / OYUN KENDI KENDINE
 *     YUKARI ASAGI OYNUYOR / PERFORMANS BITIYOR."
 *     SEBEP: oyun her karede kendi geometrisini (0,0 + ekran boyutu) yeniden
 *     dayatir. WM bunu her karede geri alirsa ikisi saniyede ~60 kez
 *     cekisir; hem titreme hem CPU yuku bundandir.
 *     KURAL: client'in KENDI move/resize'i SUNUCU tarafindan UYGULANMAMALI.
 *     Bu yuzden frame'e SubstructureRedirectMask secilir (bkz. wm_frame_create)
 *     ve gelen ConfigureRequest fullscreen pencerede wm_manage.c'de
 *     REDDEDILIR. Boylece oyun hic hareket edemez; cekisme ve titreme biter.
 *     Fullscreen pencerede client'i "geri koymak" icin frame_apply() (tam
 *     redraw) CAGRIMA — o yolu gerek birakma.
 *
 * (3) "PENCERE COZUNURLUGU SABIT KALSIN, ZORLA BOYUTLANDIRMA."
 *     Fullscreen kabinin boyutu YALNIZCA oyunun degistirdigi MOD
 *     cozunurlugunden gelir (wm_frame_set_fullscreen_size, wm_randr.c cagirir).
 *     Client'in bildirdigi ekran-boyutlu rapor kabı BUYUTMEZ.
 * ============================================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_desktop.h"
#include "wm_frame.h"
#include "wm_frame_geometry.h"
#include "wm_frame_internal.h"
#include "wm_workspace.h"

/* The chrome — the border, the title bar and the buttons — and the frame's
   whole size are arithmetic over a WmFrame and live in wm_frame_geometry.c.
   They were in this file and every one of them touched nothing but the frame,
   so they are asked for rather than worked out again here. */

/* --- the table ------------------------------------------------------------ */

WmFrame *wm_frame_find(WmCore *core, Window client) {
    if (client == None) {
        return NULL;
    }
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].client == client) {
            return &core->frames[i];
        }
    }
    return NULL;
}

WmFrame *wm_frame_find_by_frame(WmCore *core, Window frame) {
    if (frame == None) {
        return NULL;
    }
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].frame == frame) {
            return &core->frames[i];
        }
    }
    return NULL;
}

/* --- naming --------------------------------------------------------------- */

/* A window's name, from the two places one is written. _NET_WM_NAME is UTF-8
   and is what every toolkit sets today; WM_NAME is the older, Latin-1 one and
   is what a bare X program such as xterm still uses. Reading both is what
   makes the bar say "xterm" rather than "window". */
static void frame_read_name(WmCore *core, WmFrame *frame) {
    frame->name[0] = '\0';
    frame->has_name = 0;

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    Atom utf8 = wm_atom(core, "UTF8_STRING");

    if (XGetWindowProperty(core->display, frame->client, core->net_wm_name,
                           0, 1024, False, utf8, &actual_type, &actual_format,
                           &items, &after, &data) == Success) {
        if (data && actual_type == utf8 && actual_format == 8 && items > 0) {
            snprintf(frame->name, sizeof(frame->name), "%.*s", (int)items,
                     (char *)data);
            frame->has_name = frame->name[0] != '\0';
        }
        if (data) {
            XFree(data);
        }
    }

    if (!frame->has_name) {
        char *legacy = NULL;
        if (XFetchName(core->display, frame->client, &legacy) && legacy) {
            snprintf(frame->name, sizeof(frame->name), "%s", legacy);
            frame->has_name = frame->name[0] != '\0';
            XFree(legacy);
        }
    }

    if (!frame->has_name) {
        snprintf(frame->name, sizeof(frame->name), "window");
    }
}

void wm_frame_update_name(WmCore *core, WmFrame *frame) {
    char previous[sizeof(frame->name)];
    snprintf(previous, sizeof(previous), "%s", frame->name);
    frame_read_name(core, frame);
    if (strcmp(previous, frame->name) != 0) {
        wm_frame_draw(core, frame);
    }
}

/* The drawing — the title bar, the border and the buttons — lives in
   wm_frame_draw.c. It touches nothing but the frame, the style and the
   compositor, so it is kept apart from the window handling here. */

/* --- geometry, applied ---------------------------------------------------- */

/* Tell the client what it actually got. A reparenting window manager is
   required to do this: the client asked for a position and a size and the
   manager gave it a different position, and a toolkit that watches for the
   change draws nothing until it hears about it.
 *
 * A scaled window is not told anything, and that is not an omission: it was
 * never resized. Telling it a size it does not have — the frame's, smaller
 * than the client — would make a program that DOES listen resize itself to fit
 * a picture that is already being made to fit it, and the two would fight.
 * The client keeps believing it is the size it asked for, which is true.
 *
 * This is `frame_notify_configure` rather than a file-static one because
 * wm_frame_interact.c and wm_frame_fullscreen.c resize windows too, and they
 * must send the same notification `frame_apply` sends. */
void frame_notify_configure(WmCore *core, WmFrame *frame) {
    XEvent event;

    if (frame->scaled) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.xconfigure.type = ConfigureNotify;
    event.xconfigure.display = core->display;
    event.xconfigure.event = frame->client;
    event.xconfigure.window = frame->client;
    event.xconfigure.x = frame_border_of(frame);
    event.xconfigure.y = frame_title_of(frame);
    event.xconfigure.width = frame->client_width;
    event.xconfigure.height = frame->client_height;
    event.xconfigure.border_width = 0;
    event.xconfigure.above = None;
    event.xconfigure.override_redirect = False;
    XSendEvent(core->display, frame->client, False, StructureNotifyMask, &event);
}

/* Put everything where the frame's own numbers say it goes. Every change goes
   through here, so the frame window, the client inside it and the bar drawn on
   it cannot disagree.
 *
 * This is `frame_apply` rather than a file-static one because
 * wm_frame_interact.c (a resize drag) and wm_frame_fullscreen.c (a scaling
 * toggle, a fullscreen resize) end there too, and every one of them has to
 * move the frame, the client and the bar as ONE change. */
void frame_apply(WmCore *core, WmFrame *frame) {
    XMoveResizeWindow(core->display, frame->frame, frame->x, frame->y,
                      (unsigned int)frame_width(frame),
                      (unsigned int)frame_height(frame));

    /* A scaled window is put back at its corner and NOT resized. Its content
       is drawn at the size its program chose and fitted to the frame, so
       resizing it here would be resizing the very thing being scaled — and
       the whole problem is that the program ignores the resize and draws at
       its own size anyway. Moving it is all that is left to do: the picture
       of it is then drawn into the frame by wm_frame_draw. */
    if (frame->scaled) {
        XMoveWindow(core->display, frame->client,
                    frame_border_of(frame), frame_title_of(frame));
    } else {
        XMoveResizeWindow(core->display, frame->client,
                          frame_border_of(frame), frame_title_of(frame),
                          (unsigned int)frame->client_width,
                          (unsigned int)frame->client_height);
    }
    wm_frame_draw(core, frame);
}

/* Put every open frame's border at the width the desktop's style now says.
 *
 * A script that changed set_window_border_width() has changed a number the
 * geometry of every frame is computed from, so each frame is told the new
 * width and put back together: the frame window is resized, the client inside
 * it is moved, and the border is drawn again. Frames that are not open are
 * skipped, and a screen with none is a no-op. */
void wm_frame_apply_border(WmCore *core) {
    int width = style_border_width(core);
    core->style.border_width = width;

    for (int i = 0; i < core->frame_count; i++) {
        WmFrame *frame = &core->frames[i];

        /* A frame that is put away has no pixels to rearrange; it is given
           the new width and drawn when it comes back. */
        if (frame->minimized) {
            frame->border = width;
            continue;
        }

        if (frame->border == width) {
            continue;
        }

        frame->border = width;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
    }
    XFlush(core->display);
}

void wm_frame_move(WmCore *core, WmFrame *frame, int x, int y) {
    frame->x = x;
    frame->y = y;

    /* The window is moved and nothing is redrawn. The bar's picture is inside
       the window and moves with it, so the pixels are already right — and
       clearing or repainting them here is what used to flash on every motion
       event of a drag. */
    XMoveWindow(core->display, frame->frame, x, y);
    XFlush(core->display);
}

void wm_frame_raise(WmCore *core, WmFrame *frame) {
    XRaiseWindow(core->display, frame->frame);
    /* The bar is left where it is, below the windows. It used to be put back
       on top from here, and that is exactly what trapped a window dragged onto
       the bar: the bar's own raise arrived right after the window's and covered
       it again, so the window could never come out in front. The bar lives at
       the bottom of the stack now (see wm_desktop_lower_bar), so a window moved
       over it covers it and the bar is seen again the moment the window moves
       off. */
    XFlush(core->display);
}

/* ---------- size hints: the steps a client asks to be resized in ----------

   A terminal has no size in pixels; it has a grid, and the grid is whole
   character cells. It says so in WM_NORMAL_HINTS: a BASE size, which is the
   window's own chrome, and an INCREMENT, which is one cell. A window manager
   that ignores those numbers and gives the window whatever width the pointer
   asks for produces a size that is not a whole number of cells. The terminal
   draws the cells that fit and leaves the strip past the last one exactly as
   it was — and nothing the program can print reaches that strip, so no
   `clear` and no full-screen program ever repaints it. The text that stood
   there when the window had a different size then stays on the screen for the
   life of the window. Snapping to the hints is what keeps the window a whole
   number of cells.

   The hints are read from the client on every motion rather than cached: a
   program may change them at any time — a terminal does when its font
   changes — and one round trip during a drag is nothing beside the resize
   itself.

   They are here rather than beside their caller because two files need them:
   the resize drag (wm_frame_interact.c) snaps to them and so does maximise
   below. */
void frame_read_size_hints(WmCore *core, WmFrame *frame, WmSizeHints *hints) {
    XSizeHints raw;
    long supplied = 0;

    memset(hints, 0, sizeof(*hints));
    hints->width_inc = 1;
    hints->height_inc = 1;

    if (!XGetWMNormalHints(core->display, frame->client, &raw, &supplied)) {
        return;
    }

    /* The increments are counted FROM the base. A program that names a base
       gives it in PBaseSize; one that gives only a minimum means the minimum
       is the base, which is what the ICCCM says the minimum stands for. */
    if (supplied & PBaseSize) {
        hints->base_width = raw.base_width;
        hints->base_height = raw.base_height;
    } else if (supplied & PMinSize) {
        hints->base_width = raw.min_width;
        hints->base_height = raw.min_height;
    }
    if ((supplied & PResizeInc) && raw.width_inc > 0 && raw.height_inc > 0) {
        hints->width_inc = raw.width_inc;
        hints->height_inc = raw.height_inc;
    }
    if (supplied & PMinSize) {
        hints->min_width = raw.min_width;
        hints->min_height = raw.min_height;
    }
}

/* The size nearest to `value` that the hints allow. `base` is where the
   increments are counted from (0 for a program that named none — a multiple of
   the increment FROM ZERO is then the grid), `increment` is one step, and
   `minimum` is what the manager will not go below. A value already at or under
   the base is left alone: it is the smallest size the client describes and
   snapping it would round it below that. */
int frame_hint_size(int value, int base, int increment, int minimum) {
    int steps;

    if (increment > 1 && value > base) {
        steps = (value - base + increment / 2) / increment;
        value = base + steps * increment;
    }
    if (value < minimum) {
        value = minimum;
    }
    return value;
}

/* Keep a frame's TITLE BAR reachable after the client asks to be resized.
 *
 * THE SIZE IS NOT TOUCHED — and that is the correction. This function used to
 * hold the client's size to the workarea, and it was wrong for exactly the
 * case it was written for. A program may ask to be bigger than the screen (the
 * raylib demo opens at 1600x900 on a 1366x768 display); shrinking the CLIENT
 * is not how a window manager answers that. A program that listens rearranges
 * itself and never notices, but a program that does NOT — a GL game, anything
 * drawing at a fixed size — goes on drawing at the size it chose, so the frame
 * shrank and the content did not: the right-hand side and the bottom of the
 * window were outside it and simply not there. The buttons were gone, the map
 * was gone. That is a window that looks broken, and only the scaling key
 * (wm_frame_toggle_scaling) could put it right.
 *
 * Every window manager lets a program open the size it asked for; the part
 * that does not fit is off the screen and the user moves the window to see it.
 * That is what this does now.
 *
 * What is still worth doing is keeping the window from being LOST: a frame
 * whose title bar is off the top or the left cannot be grabbed and dragged
 * back, so it is pulled in by as much as it takes. A window that is merely
 * larger than the screen is left where it is — there is no place to put it
 * where both edges are inside, and moving it would only hide more of it. */
void frame_clamp_to_workarea(WmCore *core, WmFrame *frame) {
    int screen_width = core->desktop_width > 1 ? core->desktop_width
                                               : (core->width > 1 ? core->width
                                                                  : DisplayWidth(core->display,
                                                                                 core->screen));
    int screen_height = core->desktop_height > 1 ? core->desktop_height
                                                 : (core->height > 1 ? core->height
                                                                    : DisplayHeight(core->display,
                                                                                   core->screen));
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;

    wm_config_workarea(&core->config, screen_width, screen_height,
                       &area_x, &area_y, &area_width, &area_height);

    /* Pull the frame back so its right and bottom edges stay inside the area —
       but only when the frame is small enough to fit. A window larger than the
       screen has no position at which both edges are inside, and taking the
       overflow off its left would move it further off than it already was. */
    if (frame_width(frame) <= area_width &&
        frame->x + frame_width(frame) > area_x + area_width) {
        frame->x = area_x + area_width - frame_width(frame);
    }
    if (frame_height(frame) <= area_height &&
        frame->y + frame_height(frame) > area_y + area_height) {
        frame->y = area_y + area_height - frame_height(frame);
    }
    if (frame->x < area_x) {
        frame->x = area_x;
    }
    if (frame->y < area_y) {
        frame->y = area_y;
    }
}

/* The largest a client on this desktop may be, and the clamp that holds a
   frame to it. Both are defined with the workarea helpers further down and
   declared here, because the size paths above them are where they are needed
   first. */
static void frame_size_limits(WmCore *core, const WmFrame *frame,
                              int *max_width, int *max_height);
void frame_clamp_size_to_screen(WmCore *core, WmFrame *frame);
void frame_clamp_size_to_workarea(WmCore *core, WmFrame *frame);

void wm_frame_resize(WmCore *core, WmFrame *frame, int width, int height) {
    /* A scaled window's frame is the user's to size, not the client's. The
       client is being drawn at the size it chose and the frame is fitted to
       it, so a client that asks to be resized is asking for the one thing
       that would undo the scaling — its content would then be drawn at a size
       nobody is scaling from. The request is dropped here rather than acted
       on, and the window keeps the size the hand gave it. */
    if (frame->scaled) {
        return;
    }

    if (width > 1) {
        frame->client_width = width;
    }
    if (height > 1) {
        frame->client_height = height;
    }

    /* The size IS held to the desktop, and this is the one thing a window
       manager can do about a window larger than the screen. X keeps no pixels
       for the part that falls past the screen's edge and sends no Expose when
       that part is dragged back into view, so the edge of a window bigger than
       the desktop stays blank until the window is made smaller. A program that
       asks for more than fits is given the fit; a picture that genuinely wants
       the whole of itself is shown with the scaling key, which is what
       wm_frame_toggle_scaling() is for and which this clamp does not touch. */
    frame_clamp_size_to_screen(core, frame);
    frame_clamp_to_workarea(core, frame);
    frame_apply(core, frame);
    frame_notify_configure(core, frame);
}

/* A client that moved or resized itself. Its position is meaningless inside a
   frame — the bar lives above it — so only the size is taken and the position
   is put back to where the frame wants it.
 *
 * The position has to be put back, and that is the whole reason this test is
 * not "the size changed". Once a client is reparented it is a GRANDCHILD of
 * the root, and the root's structure redirection only covers the root's own
 * children — so a move the program makes itself is not redirected and does not
 * arrive as a ConfigureRequest. It arrives here, as a ConfigureNotify, with
 * the client already sitting wherever it put itself INSIDE the frame. Only
 * checking the size let that stand: the client slid away from (border, title)
 * and nothing brought it back.
 *
 * That is exactly what a GLFW program does — raylib's SetWindowPosition calls
 * XMoveWindow — so the two courts of the pong demo placed themselves at their
 * requested SCREEN coordinates measured from the frame's corner instead: the
 * Lua court, at x=60 in an 804 pixel frame, drew its court out past its frame,
 * and the Python court at x=880 fell outside the frame completely, leaving its
 * frame showing nothing but the border colour.
 *
 * Nothing is redrawn when nothing changed, which is still what keeps a plain
 * restack from repainting the title bar: a client whose size and place are both
 * already right returns above. */
void wm_frame_sync(WmCore *core, WmFrame *frame) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, frame->client, &attributes)) {
        return;
    }

    /* A scaled window resizes itself as much as it likes: the content is drawn
       at whatever size the program chose, and that size is what is scaled FROM.
       So the client's new size is taken as the window's NATURAL size — the size
       its content is drawn at — and the frame is not touched. Its own picture
       is not thrown away here either: the compositor watches ConfigureNotify
       for the same client and drops the pixmap it can no longer use, which is
       where that belongs, because the pixmap is its to keep.

       Nothing is returned early even when neither moved nor resized is set for
       a scaled window, because the compositor still has to be asked to draw it
       again. */
    if (frame->scaled) {
        if (attributes.width > 1) {
            frame->natural_width = attributes.width;
        }
        if (attributes.height > 1) {
            frame->natural_height = attributes.height;
        }
        /* A move is not taken from a scaled window: it believes it is a window
           on the root, and the frame is where it actually is. Letting its own
           idea of where it is move the frame would let a program that draws
           wherever it likes drag its own title bar around. */
        wm_frame_draw(core, frame);
        return;
    }

    /* A fullscreen-like window keeps its chrome and its place, and the client's
       own MOVE is refused.

       That refusal is the whole fix for "the title bar disappeared". A Wine
       game that went fullscreen drags itself to (0,0) on every frame — it
       believes it is a window on the root and wants the screen's corner. This
       manager took that as a SCREEN place and moved the FRAME so the client
       would sit at (0,0); the frame went to (-border, -title), which pushed the
       frame's own title bar up off the top of the screen. The window then had
       no title bar and no close button, and nothing could bring them back.

       A fullscreen window is a container like any other (diagram.md: a window
       is never the screen), so its frame is placed by the manager and stays
       where it is: the client's move is dropped and the client is put back
       inside the frame at (border, title), which is where its pixels are drawn
       anyway. Only the SIZE is taken from the client — clamped to the desktop —
       so a game that resized itself is followed. Its chrome is never dropped. */
    if (frame->fullscreen) {
        /* The size the frame is about to be given. It comes from the resolution
           the client asked for when it changed the display mode (see
           wm_frame_set_fullscreen_size); when none was seen it is the client's
           own reported size. Wine's outer window is the size of the SCREEN it
           saw, so the asked-for resolution — not that — is what the container
           follows; taking the screen-sized report would leave the game a small
           picture in a field of black.

           The comparison is OLD against NEW after the clamps, and not the
           wanted size against the stored one. The wanted size may be larger
           than the workarea allows, so it is clamped; a test against the
           un-clamped wanted size would then be true on every single sync — the
           clamp lands on one value, the wanted size stays another — and the
           frame would be resized on every event the client sends, which is a
           busy loop that pegs a core for as long as the game runs. Comparing
           the frame's size before and after the clamp has no such gap. */
        int old_width = frame->client_width;
        int old_height = frame->client_height;

        if (frame->fullscreen_width > 1) {
            frame->client_width = frame->fullscreen_width;
        }
        if (frame->fullscreen_height > 1) {
            frame->client_height = frame->fullscreen_height;
        }
        frame_clamp_size_to_workarea(core, frame);
        frame_clamp_to_workarea(core, frame);

        /* The container's size comes from the RESOLUTION the game asked for by
           changing the display mode (wm_frame_set_fullscreen_size), never from
           the client's own screen-sized report. So the window stays the size
           the game chose, and it is not stretched back out to the screen. */
        if (frame->client_width != old_width ||
            frame->client_height != old_height) {
            frame_apply(core, frame);
            return;
        }

        /* The client is put back at its corner whenever the game has moved or
           resized it away — which a Wine fullscreen game does on every drawn
           frame, because it believes it is an ordinary window on the root and
           keeps restoring its own (0,0) screen-sized geometry.
         *
         * THE CHROME AND THE COST ARE BOTH WHY THIS IS A BARE MOVE. The client
           is a CHILD of the frame, so a client at (0,0) the size of the screen
           is drawn OVER the frame: it covers the title bar and the border
           completely, and the window cannot be grabbed or closed. Putting it
           back is what keeps the chrome visible. Doing it with frame_apply()
           instead — the earlier version — also rebuilt and redrew the title bar
           on every one of those frames, which is what turned a running game
           into a core-burning tug of war. XMoveResizeWindow moves the client
           and NOTHING is redrawn: the bar has not changed and has no reason to
           be drawn again, so a game's re-assertions cost a move each and the
           picture stays put. */
        if (attributes.x != frame_border_of(frame) ||
            attributes.y != frame_title_of(frame) ||
            attributes.width != frame->client_width ||
            attributes.height != frame->client_height) {
            XMoveResizeWindow(core->display, frame->client,
                              frame_border_of(frame), frame_title_of(frame),
                              (unsigned int)frame->client_width,
                              (unsigned int)frame->client_height);
            XFlush(core->display);
        }
        return;
    }

    int moved = attributes.x != frame_border_of(frame) ||
                attributes.y != frame_title_of(frame);
    int resized = attributes.width != frame->client_width ||
                  attributes.height != frame->client_height;
    if (!moved && !resized) {
        return;
    }

    if (resized) {
        if (attributes.width > 1) {
            frame->client_width = attributes.width;
        }
        if (attributes.height > 1) {
            frame->client_height = attributes.height;
        }

        /* A window that resized ITSELF is held to the screen exactly as one
           whose resize the manager was asked for: it is the same window either
           way, and a window bigger than the screen has the same blank edge
           whichever path made it that size. */
        frame_clamp_size_to_screen(core, frame);
    }

    /* Where the frame sits once the client's move, if any, is honoured.
     *
     * A move is TAKEN, not undone, and it is taken as a SCREEN place. The
     * server reports the client's new place relative to its parent, which is
     * the frame — but the program has no idea it is inside a frame. It
     * believes it is a window on the root, so the coordinates it asked for are
     * the coordinates it wanted ON THE SCREEN, and the frame is put where the
     * client would then sit where it asked: the client's own place less the
     * frame's chrome.
     *
     * Reading it as a delta from where the client already was would not
     * survive a client that asks twice — every move measured from the place
     * the previous one was taken to, so the window walks across the screen by
     * the sum of its own coordinates. It is read as an absolute place, which
     * is what raylib's SetWindowPosition relies on.
     *
     * THE CLAMP RUNS BEFORE THE COMPARISON, and that ordering is the whole of
     * this fix. A game that re-places itself at the screen corner every drawn
     * frame — SDL and Wine both do — asks for a frame place of (0 - border,
     * 0 - title). The workarea clamp then pulls that back to (0, 0), which is
     * where the frame already was. Comparing the UN-clamped wish (-border)
     * against the frame's stored place (0) is true on every single frame, so
     * the frame was rebuilt — the title bar redrawn with a fresh pixmap and a
     * fresh Xft pass — sixty times a second for as long as the game ran: a
     * window manager pegging a core, and the X server behind it, for nothing.
     * Clamping first and comparing what the clamp produced leaves the honest
     * answer, which is "nothing moved", and nothing is redrawn. */
    int old_x = frame->x;
    int old_y = frame->y;

    if (moved) {
        frame->x = attributes.x - frame_border_of(frame);
        frame->y = attributes.y - frame_title_of(frame);
    }
    frame_clamp_to_workarea(core, frame);

    /* The frame is put back together — the client moved, the title bar redrawn
       — only when its size changed or its PLACE really moved. When the place
       is the same, the client is put back in its corner and NOTHING is
       redrawn: the cheap move of one window, no pixmap, no text. */
    if (resized || frame->x != old_x || frame->y != old_y) {
        frame_apply(core, frame);
        return;
    }

    XMoveWindow(core->display, frame->client,
                frame_border_of(frame), frame_title_of(frame));
    XFlush(core->display);
}

/* The area a maximised window gets: the screen less the bar. Read from the
   core rather than from the frame, because the frame is what is being changed
   to match it, and taken through wm_config_workarea() so a maximised window
   and a newly opened one agree about where the desktop ends. Without this a
   maximised window covers the bar, and the bar is the one thing on this
   desktop that has to stay reachable — there is no panel behind it. */
static void frame_screen_size(WmCore *core, int *x, int *y,
                              int *width, int *height) {
    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);
    wm_config_workarea(&core->config, screen_width, screen_height,
                       x, y, width, height);
}

/* The largest a client on this desktop may be: THE SCREEN less the frame's own
   chrome.
 *
 * It was the WORKAREA — the screen less the bar — and that was wrong, and it
 * is the whole of "the screen gets smaller". A game that asks for the screen's
 * height was given the screen's height less the bar's, so it came up short by
 * a strip, noticed, asked again, and never got what it asked for. The bar is
 * not the game's problem; a window is allowed to be as tall as the display it
 * is on and to cover the bar while it is there, which is what a big window has
 * always done.
 *
 * The limit still exists, and for the reason it always did: X keeps no pixels
 * for the part of a window that falls past the screen's edge and raises no
 * Expose when that part is dragged back in, so a window larger than the screen
 * has a blank edge that never repaints. A window AT the screen's size has no
 * such edge. The two are not the same size and this is the line between them.
 *
 * A MAXIMISED window is a different thing and is not routed through here — see
 * wm_frame_maximize(), which asks for the workarea deliberately, because a
 * maximised window is meant to stop at the bar. */
static void frame_size_limits(WmCore *core, const WmFrame *frame,
                              int *max_width, int *max_height) {
    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);

    int wide = screen_width - 2 * frame_border_of(frame);
    int tall = screen_height - frame_title_of(frame) - frame_border_of(frame);
    *max_width = wide > 1 ? wide : 1;
    *max_height = tall > 1 ? tall : 1;
}

void frame_clamp_size_to_screen(WmCore *core, WmFrame *frame) {
    int max_width = 0;
    int max_height = 0;

    frame_size_limits(core, frame, &max_width, &max_height);
    if (frame->client_width > max_width) {
        frame->client_width = max_width;
    }
    if (frame->client_height > max_height) {
        frame->client_height = max_height;
    }
}

/* Cap a FULLSCREEN window's client to the WORKAREA less the chrome.
 *
 * The ordinary size limit (frame_clamp_size_to_screen) is the SCREEN less the
 * chrome, which lets a window's bottom edge sit past the desktop bar. That edge
 * is then off the visible area, and X keeps no pixels there and raises no
 * Expose when it is dragged back — the strip stays unpainted, which is the
 * "black screen at the bottom" that was reported. The WORKAREA is the part of
 * the screen a window can actually show, so a fullscreen window is capped to it
 * and its whole frame — title bar and bottom border included — stays on screen. */
void frame_clamp_size_to_workarea(WmCore *core, WmFrame *frame) {
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;

    frame_screen_size(core, &area_x, &area_y, &area_width, &area_height);

    int max_width = area_width - 2 * frame_border_of(frame);
    int max_height = area_height - frame_title_of(frame)
                     - frame_border_of(frame);
    if (max_width < 1) {
        max_width = 1;
    }
    if (max_height < 1) {
        max_height = 1;
    }
    if (frame->client_width > max_width) {
        frame->client_width = max_width;
    }
    if (frame->client_height > max_height) {
        frame->client_height = max_height;
    }
}

void wm_frame_maximize(WmCore *core, WmFrame *frame) {
    if (frame->maximized) {
        /* The same button puts it back where it was. */
        frame->maximized = 0;
        frame->x = frame->restore_x;
        frame->y = frame->restore_y;
        frame->client_width = frame->restore_width;
        frame->client_height = frame->restore_height;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
        return;
    }

    frame->restore_x = frame->x;
    frame->restore_y = frame->y;
    frame->restore_width = frame->client_width;
    frame->restore_height = frame->client_height;

    /* The workarea, not the screen: a maximised window stops at the bar. Its
       client still has the title bar above it, so the client's height gives
       back the title bar's room as well. */
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    frame_screen_size(core, &area_x, &area_y, &area_width, &area_height);

    frame->maximized = 1;
    /* Keep a 5px margin to all sides so the window still covers the screen
       visually but does not slide under the top bar or squeeze against the
       desktop edge. */
    int inset = 5;
    frame->x = area_x + inset;
    frame->y = area_y + inset;
    frame->client_width = area_width - 2 * frame_border_of(frame) - 2 * inset;
    frame->client_height = area_height - frame_title_of(frame)
                           - frame_border_of(frame) - 2 * inset;
    if (frame->client_width < 1) frame->client_width = 1;
    if (frame->client_height < 1) frame->client_height = 1;

    /* The size is snapped DOWN to the client's own resize steps.
     *
     * A maximised window almost never lands on a whole number of the client's
     * cells: the workarea is some arbitrary number of pixels and a terminal's
     * cell is another, so `area / cell` leaves a remainder. The client is told
     * the whole cells it has and draws into them, and the remainder is a strip
     * of background it never paints — the blank column down the right edge and
     * the blank row along the bottom that "nano does not scale" is. Snapping
     * DOWN (not to nearest) keeps the window inside the workarea the maximise
     * was asked to fit, and leaves the remainder outside the frame where it
     * belongs: on the desktop, not inside the window.
     *
     * A client that named no increments has a step of one pixel and passes
     * through unchanged, so every other window maximises exactly as before. */
    WmSizeHints hints;
    frame_read_size_hints(core, frame, &hints);
    if (hints.width_inc > 1) {
        int steps = (frame->client_width - hints.base_width) / hints.width_inc;
        if (steps < 0) steps = 0;
        frame->client_width = hints.base_width + steps * hints.width_inc;
    }
    if (hints.height_inc > 1) {
        int steps = (frame->client_height - hints.base_height) / hints.height_inc;
        if (steps < 0) steps = 0;
        frame->client_height = hints.base_height + steps * hints.height_inc;
    }
    if (frame->client_width < 1) frame->client_width = 1;
    if (frame->client_height < 1) frame->client_height = 1;

    frame_apply(core, frame);
    frame_notify_configure(core, frame);
    wm_frame_raise(core, frame);
    wm_focus_set(core, frame->client);
}

/* The next window that is still on screen, after the given one and wrapping
   around.
 *
 * This is not the order the switcher key walks: that one goes through the
 * recent-focus order and may land on a minimised window, because it has to be
 * able to reach them. Here the window the keyboard is on is being taken away,
 * so the focus has to land on something the user can see - and walking on to
 * the window just put away would make the minimise button undo itself. */
static WmFrame *frame_next_visible(WmCore *core, Window client) {
    int start = -1;
    for (int i = 0; i < core->frame_count; i++) {
        if (core->frames[i].client == client) {
            start = i;
            break;
        }
    }

    for (int step = 1; step <= core->frame_count; step++) {
        int i = (start + step) % core->frame_count;
        if (i < 0) {
            i += core->frame_count;
        }
        if (!core->frames[i].minimized &&
            core->frames[i].workspace == core->current_workspace) {
            return &core->frames[i];
        }
    }
    return NULL;
}

void wm_frame_minimize(WmCore *core, WmFrame *frame) {
    if (frame->minimized) {
        return;
    }
    frame->minimized = 1;
    XUnmapWindow(core->display, frame->frame);

    /* The keyboard cannot stay on a window that is not on the screen, so it
       goes to one that still is. */
    if (core->focused == frame->client) {
        core->focused = 0;
        WmFrame *next = frame_next_visible(core, frame->client);
        if (next) {
            wm_frame_activate(core, next);
        } else {
            XSetInputFocus(core->display, core->root, RevertToPointerRoot,
                           CurrentTime);
        }
    }
    XFlush(core->display);
}

void wm_frame_restore(WmCore *core, WmFrame *frame) {
    if (!frame->minimized) {
        return;
    }
    frame->minimized = 0;
    if (frame->workspace != core->current_workspace) { return; }
    XMapRaised(core->display, frame->frame);

    /* A window that was put away comes back whole. The frame and the client
       inside it are both mapped by the server with one call, but the bar's
       picture is the manager's, so it is drawn again. */
    wm_frame_draw(core, frame);
    XFlush(core->display);
}

void wm_frame_activate(WmCore *core, WmFrame *frame) {
    if (frame->minimized) {
        wm_frame_restore(core, frame);

        /* The map has to have reached the server before the keyboard can be
           given to the window. Every focus decision is made through
           XGetWindowAttributes, and that reports a window as not viewable
           until the server has processed the map request — so without this
           wait a restored window comes back without the focus, which is what
           made the switcher look like it did nothing to a window that had
           been minimised. */
        XSync(core->display, False);
    }
    wm_frame_raise(core, frame);
    wm_focus_set(core, frame->client);
}

/* The scaling toggle and the fullscreen state live in wm_frame_fullscreen.c:
   they change what the client IS, unlike the state changes above, which only
   change where it is. The clicks, drags and resizes live in
   wm_frame_interact.c, which is the only file of the frame that reads X
   events. */

/* --- creating and destroying ---------------------------------------------- */

WmFrame *wm_frame_create(WmCore *core, Window client) {
    if (wm_frame_find(core, client) != NULL) {
        return NULL;
    }
    if (core->frame_count >= WM_MAX_FRAMES) {
        fprintf(stderr, "gnuchanwm: too many windows (max %d)\n", WM_MAX_FRAMES);
        return NULL;
    }

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(core->display, client, &attributes)) {
        return NULL;
    }
    if (attributes.override_redirect) {
        return NULL;
    }

    WmFrame *frame = &core->frames[core->frame_count];
    memset(frame, 0, sizeof(*frame));
    frame->client = client;
    wm_workspace_place(core, frame);
    frame->client_width = attributes.width > 1 ? attributes.width : 80;
    frame->client_height = attributes.height > 1 ? attributes.height : 24;
    frame->border = style_border_width(core);

    /* New windows are centred on the primary/default monitor rather than being
       dropped at 0,0. The default monitor is the one the desktop uses most, so
       this keeps opening windows in the visible usable area and avoids the
       "block stuck at the top-left" behaviour. */
    int screen_width = core->width > 1 ? core->width
                                       : DisplayWidth(core->display,
                                                      core->screen);
    int screen_height = core->height > 1 ? core->height
                                         : DisplayHeight(core->display,
                                                         core->screen);
    int centered_x = (screen_width - frame_width(frame)) / 2;
    int centered_y = (screen_height - frame_height(frame)) / 2;
    frame->x = centered_x;
    frame->y = centered_y;

    /* The size the client asked for: the size its content is drawn at, and the
       size scaling puts the window back to — see wm_frame_toggle_scaling().
       The size is no longer cut down to the workarea on the way in, so for a
       window that has not resized itself since, this IS its size; a window
       that does resize itself has this updated by wm_frame_sync(). */
    frame->natural_width = frame->client_width;
    frame->natural_height = frame->client_height;

    /* A window's TITLE BAR is kept clear of the bar and on the screen. The
       screen's top-left corner is not where a window belongs when a bar is
       along the top: the frame's title bar would open underneath the strip and
       the window could not be dragged out from under it.

       Only a window that would land outside is moved. A window that fits where
       it asked to be is left exactly there, because a manager that repositions
       a window the user's program placed deliberately is a manager that makes
       every program's own saved geometry meaningless. The SIZE is not touched
       — see the note below. */
    int area_x = 0;
    int area_y = 0;
    int area_width = 0;
    int area_height = 0;
    wm_config_workarea(&core->config, core->width, core->height,
                       &area_x, &area_y, &area_width, &area_height);

    if (frame->x < area_x) {
        frame->x = area_x;
    }
    if (frame->y < area_y) {
        frame->y = area_y;
    }
    if (frame->x < 0) {
        frame->x = 0;
    }
    if (frame->y < 0) {
        frame->y = 0;
    }

    /* PENCERE EKRANA SIGDIRILIR.

       ESKI DAVRANIS VE NEDEN YANLISTI: pencere, program ne istediyse O
       boyutta birakiliyordu; ekrana sigmayan kisim ekranin disinda kaliyordu
       ve "kullanici pencereyi kaydirip gorur" varsayiliyordu. Bu varsayim
       X'te TUTMAZ: ekranin disina tasan pikseller icin X sunucusu arka bellek
       tutmaz, pencere yukari kaydirildiginda o bolge icin Expose uretilmez ve
       alt kenar — BORDER'I DAHIL — bos/siyah kalir. Kullanici bunu "pencerenin
       alti siliniyor, scale yapana kadar duzelmiyor" diye bildirdi ve
       gozlem TAM OLARAK dogru: olceklenmis bir pencerenin cercevesi elle
       kucultuldugu icin ekrana sigar ve sorun gorunmez.

       Neden boyut kirpmak SIMDI dogru: eski not "GL oyunu kendi boyutunda
       cizmeye devam eder" diyordu, ama bu ancak pencere EKRANDAN BUYUK
       birakilirsa sorun olur. Ekrana sigdirilan bir pencerede cerceve de
       kuculur; icerik cerceveye sigar ve hicbir sey disarida kalmaz. Zaten
       bunu yapmayan bir program icin OLCEKLEME anahtari var
       (wm_frame_toggle_scaling) ve o yol da ekrana sigdirmanin USTUNE kurulur.

       Kirpilan sey YALNIZCA buyukluktur; programin kendi bildirdigi natural
       boyut da AYNI sayiya cekilir, yoksa sonradan acilan olcekleme pencereyi
       yeniden ekranin disina tasirdi. */
    frame_clamp_size_to_screen(core, frame);
    if (frame->client_width < 1) {
        frame->client_width = 1;
    }
    if (frame->client_height < 1) {
        frame->client_height = 1;
    }
    /* Now that the size can no longer exceed the desktop, pulling the right
       and bottom edges inside actually moves the frame instead of being
       refused: frame_clamp_to_workarea() declines to move a window too big to
       fit, and after the clamp above there is none. A window the program
       opened larger than the screen therefore lands with all four edges
       visible rather than with its bottom-right off the screen. Done before
       the natural size is recorded, so the two agree — a scaling press later
       must put the window back to a size the desktop can show. */
    frame_clamp_to_workarea(core, frame);
    frame->natural_width = frame->client_width;
    frame->natural_height = frame->client_height;

    frame->frame = XCreateSimpleWindow(
        core->display, core->root, frame->x, frame->y,
        (unsigned int)frame_width(frame),
        (unsigned int)frame_height(frame),
        0, 0, core->style.panel);
    if (frame->frame == None) {
        return NULL;
    }

    /* The frame is override-redirect, and this has to be set before it is
       mapped. The root redirects structure, so a normal window's map comes
       back to us as a MapRequest; without this the manager would receive a
       request for its own frame and wrap the frame in another frame, for
       ever. An override-redirect window is what the server maps without
       asking, which is exactly what the manager's own chrome has to be. */
    XSetWindowAttributes override_attributes;
    memset(&override_attributes, 0, sizeof(override_attributes));
    override_attributes.override_redirect = True;
    /* The bar is not cleared between redraws: every pixel of it is painted in
       wm_frame_draw, into the copy, before the copy is put up. Letting the
       server clear it first would defeat that and bring the flicker back. */
    override_attributes.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(core->display, frame->frame,
                            CWOverrideRedirect | CWBitGravity,
                            &override_attributes);

    /* The bar is drawn on the frame, so the frame is what is clicked, dragged
       and exposed. It also watches for the pointer entering it, because moving
       onto a title bar is how a user chooses a window, and focus follows the
       pointer. EnterNotify does not propagate up the tree, so each window that
       should react to the pointer has to ask for it itself. */
    XSelectInput(core->display, frame->frame,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | EnterWindowMask |
                 SubstructureRedirectMask);

    /* SubstructureRedirectMask on the FRAME, and this is what stops a game
       from shaking its own title bar.

       The client is a CHILD of the frame. With the redirect selected on the
       frame, the client's own XMoveWindow/XResizeWindow calls are NOT applied
       by the server — they arrive here as ConfigureRequest events instead, and
       wm_manage.c answers them. A Wine fullscreen game re-places itself at the
       screen corner and re-sizes itself to the screen on every drawn frame;
       without this redirect those moves take effect, the manager moves the
       client back, and the two trade the window back and forth sixty times a
       second — the picture slides up and down and the title bar flickers,
       which is the fault this removes.

       With it, the client's moves never happen at all: the client is put at
       (border, title) once and stays exactly there, so the chrome is never
       covered and nothing flickers. The redirect costs nothing for a program
       that never moves itself, which is almost all of them. */

    /* The client is watched for its name, its size, and the pointer arriving
       on it. Without EnterWindowMask here the pointer would focus nothing when
       it moved straight onto the program's own window.

       !! PERFORMANS UYARISI — PropertyChangeMask BIR DONGU KAYNAGIDIR !!
       Bu maskeyi secmek, WM'nin BU client'in property'lerine YAPTIGI her
       yazmanin da bir PropertyNotify olarak geri gelmesi demektir. WM
       _NET_WM_STATE'i hem OKUR (manage_property) hem YAZAR
       (frame_publish_fullscreen_state). Eger yazma "durum gercekten degisti mi"
       kontrolu olmadan yapilirsa sonsuz YAZ -> BILDIR -> YAZ dongusu olusur ve
       fullscreen bir pencere acikken CPU somurulur (bkz. wm_frame_fullscreen.c
       icindeki frame_publish_fullscreen_state uyarisi). Maskeyi buradan
       KALDIRMA — _NET_WM_STATE degisimlerini kacirirsin; bunun yerine yazma
       yolunu koru. */
    XSelectInput(core->display, client,
                 PropertyChangeMask | StructureNotifyMask | EnterWindowMask);

    /* A click on the program's own pixels is taken with a passive grab rather
       than by asking to be told about button presses through the event mask.

       The mask is the obvious way and it does not work: a program that handles
       the mouse itself — a terminal with mouse reporting, which is what this
       session opens — already has the button selected on its window, and
       asking for it too is refused with BadAccess, which takes the whole mask
       down with it. The manager then hears nothing at all from that window,
       clicks included, and that is exactly the window a user clicks.

       A grab does not collide. It is held per button and per window, so the
       program's own interest in the button does not stop this from registering,
       and AnyModifier means the click is taken whether or not a modifier is
       held. owner_events is False so the grab is the manager's, not the
       program's, and the press waits here (GrabModeSync) until it is decided
       what to do with it — see the ButtonPress case in wm_frame_interact.c,
       where it is replayed to the program so a click meant for the program
       still reaches it. */
    XGrabButton(core->display, Button1, AnyModifier, client, False,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);

    /* Alt+left and Alt+right are the manager's — move and resize — and they
       need grabs of their own on the program's window, because without one the
       press goes straight to the program and the manager never hears it.

       The grab is taken for the modifier combination and not for the button
       alone, which is what keeps the plain click on the program: a passive
       grab only activates when exactly its modifiers are down, so a Button1
       press with no Alt held does not match this grab and falls through to the
       any-modifier one above. Where both could match, the press is handled the
       same way either way — the handler tests the modifiers itself — so it does
       not matter which of the two the server picks.

       The event mask is not just the press. Once a passive button grab has
       activated, its event mask is what is reported for the whole grab, and
       the grab lasts until the button comes back up. So asking for motion and
       release here is what makes the window follow the pointer: without them
       the grab exists but delivers nothing after the first press, and the
       window only moves as far as the first motion event before the pointer
       leaves it.

       Every lock combination is grabbed beside the bare one, so a session with
       CapsLock or NumLock on still moves windows. */
    for (unsigned int locks = 0; locks < 4; locks++) {
        unsigned int modifiers = Mod1Mask;
        if (locks & 1) modifiers |= LockMask;
        if (locks & 2) modifiers |= Mod2Mask;

        XGrabButton(core->display, Button1, modifiers, client, False,
                    ButtonPressMask | PointerMotionMask | ButtonReleaseMask,
                    GrabModeSync, GrabModeAsync, None, None);
        XGrabButton(core->display, Button3, modifiers, client, False,
                    ButtonPressMask | PointerMotionMask | ButtonReleaseMask,
                    GrabModeSync, GrabModeAsync, None, None);
    }

    /* A safety net: if this window manager dies, the X server puts the client
       back on the root instead of leaving it inside a window nobody owns. */
    XAddToSaveSet(core->display, client);

    /* Reparenting a mapped window makes the server unmap it first, and that
       unmap comes back to us as an UnmapNotify. Counting it here is what tells
       it apart from the user's program hiding itself. */
    if (attributes.map_state == IsViewable) {
        core->ignore_unmaps++;
    }

    /* WM_STATE says "this window is managed", and it is the test a task list,
       a pager or any other watcher uses to tell a client from the chrome.
       The protocol requires it of a reparenting manager, and without it
       _NET_CLIENT_LIST stays empty however many windows are open. */
    long wm_state[2] = { 1 /* NormalState */, None };
    XChangeProperty(core->display, client, core->wm_state, core->wm_state,
                    32, PropModeReplace, (unsigned char *)wm_state, 2);

    XSetWindowBorderWidth(core->display, client, 0);
    XReparentWindow(core->display, client, frame->frame,
                    frame_border_of(frame), frame_title_of(frame));
    XMapWindow(core->display, client);
    XMapWindow(core->display, frame->frame);

    core->frame_count++;

    frame_read_name(core, frame);
    wm_frame_read_icon(core, frame);
    wm_frame_draw(core, frame);
    return frame;
}

void wm_frame_destroy(WmCore *core, WmFrame *frame, int client_gone) {
    /* A window that was being drawn scaled is given back to the server before
       anything else. Leaving it redirected would leave it in the one state
       this module exists to avoid: a window nobody draws. It is done here and
       not left for the compositor's own cleanup because the client may outlive
       the frame — a reparented window is put back on the root below, and it
       has to be the server's again when it gets there. */
    if (frame->scaled) {
        wm_compositor_unscale(core, frame->client);
        frame->scaled = 0;
    }

    /* A window that was fullscreen is told it is not, before it is given back
       or destroyed. The state lives in the CLIENT's own property, and a client
       that outlives this manager — one that is put back on the root below —
       would otherwise still be carrying "I am fullscreen" and would ask the
       next manager for a screen it already gave back, or draw itself at the
       screen's size inside whatever frame it was put in. */
    if (frame->fullscreen) {
        frame_publish_fullscreen_state(core, frame, 0);
        frame->fullscreen = 0;
    }

    /* The icon is this process's pixmap, not the client's, so it does not go
       away with the client and is freed here by the code that made it. */
    wm_frame_free_icon(core, frame);
    if (!client_gone) {
        /* Put the client back where the desktop found it, so a program that
           outlives this window manager is not left fatherless. */
        XRemoveFromSaveSet(core->display, frame->client);
        XReparentWindow(core->display, frame->client, core->root,
                        frame->x, frame->y);
    }
    if (frame->buffer != None) {
        XFreePixmap(core->display, frame->buffer);
        frame->buffer = None;
    }
    if (frame->frame != None) {
        XDestroyWindow(core->display, frame->frame);
    }
    if (core->focused == frame->client) {
        core->focused = 0;
    }
    /* A window that is gone must leave the focus order too, or the switcher
       would try to activate a window the server no longer has. */
    wm_focus_forget(core, frame->client);

    /* Take the frame out of the table by moving the last one into its place.
       Order does not matter here, and this keeps the array packed. */
    int index = (int)(frame - core->frames);
    core->frames[index] = core->frames[core->frame_count - 1];
    core->frame_count--;
    XFlush(core->display);
}

void wm_frame_close(WmCore *core, WmFrame *frame) {
    Atom *protocols = NULL;
    int count = 0;
    int polite = 0;

    if (XGetWMProtocols(core->display, frame->client, &protocols, &count)) {
        for (int i = 0; i < count; i++) {
            if (protocols[i] == core->wm_delete_window) {
                polite = 1;
                break;
            }
        }
        if (protocols) {
            XFree(protocols);
        }
    }

    if (polite) {
        /* Ask, so the program can save its work and its own prompt appears. */
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.xclient.type = ClientMessage;
        event.xclient.window = frame->client;
        event.xclient.message_type = core->wm_protocols;
        event.xclient.format = 32;
        event.xclient.data.l[0] = (long)core->wm_delete_window;
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(core->display, frame->client, False, NoEventMask, &event);
    } else {
        /* A program that cannot be asked has no other way to be stopped. */
        XKillClient(core->display, frame->client);
    }
    XFlush(core->display);
}
