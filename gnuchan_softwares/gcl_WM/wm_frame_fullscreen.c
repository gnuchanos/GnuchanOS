/*
 * wm_frame_fullscreen.c — the two states a frame can be put into that change
 * what the client is: fullscreen, and fitted-to-the-frame.
 *
 * Both are asked for by the client or by a key, both change how the window is
 * drawn, and neither is a thing that happens to a frame in the ordinary run of
 * a session — which is why they are their own file and not mixed in with the
 * dragging and the resizing.
 *
 * FULLSCREEN on this desktop is a STATE, and the window stays a container:
 * the EWMH property is written, the window is raised and focused, and the
 * chrome is KEPT. It is not resized to the screen and the title bar is not
 * dropped, because a window with no title bar and no close button is a window
 * the user cannot get out of. The full section below says why in full, and the
 * warning in it is not decoration: an earlier version did drop the chrome and
 * the user could no longer move or close the window.
 *
 * FITTING is the opposite trade. A program that ignores its resize keeps
 * drawing at the size it chose, so the frame is drawn with a scaled picture of
 * the whole of it rather than hiding the part that does not fit.
 */
#include <stdio.h>

#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_frame.h"
#include "wm_frame_geometry.h"
#include "wm_frame_internal.h"

/* --- fitting a window's content into it ----------------------------------- */

int wm_frame_is_scaled(const WmFrame *frame) {
    return frame ? frame->scaled : 0;
}

void wm_frame_toggle_scaling(WmCore *core, WmFrame *frame) {
    if (!core || !frame) {
        return;
    }

    if (frame->scaled) {
        /* Off. The window goes back to being the server's to draw, and it is
           given the frame's size first so the swap happens at the size the
           user was looking at — rather than the window jumping back to the
           size it asked for when it opened, which is the size the frame was
           built to avoid. */
        wm_compositor_unscale(core, frame->client);
        frame->scaled = 0;
        frame_apply(core, frame);
        frame_notify_configure(core, frame);
        XSync(core->display, False);
        wm_frame_draw(core, frame);
        return;
    }

    /* On. What is being given up has to be said: the window is about to be
       drawn at the size it chose rather than the size of the frame, and the
       hand that pressed the key may have expected the frame to stay exactly as
       it is. It does — the frame is not touched — but what is inside it
       changes, and that is the whole of the feature. */
    if (wm_compositor_scale(core, frame->client) != 0) {
        fprintf(stderr, "gnuchanwm: this window cannot be scaled to its frame; "
                        "it is left as it was.\n");
        return;
    }

    frame->scaled = 1;

    /* The window is put back to the size its content is drawn at. It is moved
       and resized directly rather than through frame_apply(), which skips the
       resize for a scaled window: the resize is wanted here, once, to put the
       window back to the size its content is drawn at. Everything after this
       goes the other way — the frame is fitted to the window. */
    XMoveResizeWindow(core->display, frame->client,
                      frame_border_of(frame), frame_title_of(frame),
                      (unsigned int)frame->natural_width,
                      (unsigned int)frame->natural_height);
    XSync(core->display, False);

    /* The window has to have drawn at its new size before the picture of it is
       anything but the old frame, so it is drawn now and drawn again by the
       damage the client sends as it paints. */
    wm_frame_draw(core, frame);
    fprintf(stderr, "gnuchanwm: '%s' is now drawn fitted to its frame\n",
            frame->has_name && frame->name[0] ? frame->name : "window");
}

/* --- fullscreen ----------------------------------------------------------- */

int wm_frame_is_fullscreen(const WmFrame *frame) {
    return frame ? frame->fullscreen : 0;
}

/* Write the fullscreen state into the client's own _NET_WM_STATE property.
 *
 * The ClientMessage is only half of the conversation. A client that asked the
 * manager for the state reads the answer back out of its own property rather
 * than taking the grant on trust: a toolkit that sets the state, is told the
 * manager speaks EWMH, and gets no property back concludes the request was
 * ignored and asks again for ever. The list is a single atom — the fullscreen
 * member — and an empty list when the state is dropped, which reads exactly as
 * "the states I am in, of which there are none". */
/* ============================================================================
 * !! PERFORMANS UYARISI — BU FONKSIYON BIR CPU DONGUSU BASLATABILIR !!
 *
 * Bu fonksiyon client'in _NET_WM_STATE property'sini YAZAR. Ve wm_frame_create()
 * bu client'a PropertyChangeMask secmis durumdadir ("The client is
 * watched for its name..." yanindaki XSelectInput). Yani burada yazilan her
 * property, X sunucusundan YENI BIR PropertyNotify olarak geri doner ve
 * manage_property() tekrar cagrilir.
 *
 * EGER manage_property() bu bildirimi KOSULSUZ olarak
 * wm_frame_set_fullscreen() -> frame_publish_fullscreen_state() seklinde geri
 * besleyecek olursa su dongu olusur:
 *
 *      YAZ -> PropertyNotify -> TEKRAR YAZ -> TEKRAR BILDIR -> ...
 *
 * Bu dongu saniyede binlerce kez doner, HICBIR log satiri uretmez (bu yuzden
 * teshisi zordur) ve oyun/fullscreen bir pencere acikken hem GnuChanWM'i hem
 * Xorg'u %80-100 CPU'ya cikarip oyunu dondurur. Teshis edilen belirti: masaustu
 * BOSTA %0 CPU, ama fullscreen bir pencere acilinca WM ve Xorg CPU'su patlar.
 *
 * BU YUZDEN: bu fonksiyonu cagiran HER yol, yazmadan once durumun GERCEKTEN
 * degistigini dogrulamalidir:
 *   - wm_manage.c: manage_property -> wanted != wm_frame_is_fullscreen(frame)
 *   - wm_frame_fullscreen.c: wm_frame_set_fullscreen -> zaten ayni durumdaysa erken cikar
 * Bu kontrol kaldirilirsa dongu geri gelir. Kaldirma.
 * ==========================================================================*/
void frame_publish_fullscreen_state(WmCore *core, WmFrame *frame, int on) {
    if (frame->client == None || core->net_wm_state == None) {
        return;
    }
    if (on) {
        Atom states[1];
        states[0] = core->net_wm_state_fullscreen;
        XChangeProperty(core->display, frame->client, core->net_wm_state,
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)states, 1);
    } else {
        XChangeProperty(core->display, frame->client, core->net_wm_state,
                        XA_ATOM, 32, PropModeReplace, NULL, 0);
    }
}

/* Take the whole screen, or give it back. See wm_frame.h for the contract;
 * what follows is why it has to exist at all.
 *
 * A Direct3D game — every one of them, and GTA Vice City is the example this
 * was written against — asks for the whole screen and then does its own
 * arithmetic about where its picture goes. What it needs is for the manager to
 * answer "you have the whole screen, and here is the size that is", which in
 * EWMH is one state and one ConfigureNotify. What it gets from a manager that
 * does not answer is nothing at the moment it asks, so it falls back to
 * resizing and moving itself to (0,0) at the screen's size and CHECKING every
 * frame that it is still there.
 *
 * That fallback is the whole of the bug. The client believes it is a window on
 * the root; the manager has put it inside a frame at (border, title) so the
 * bar is above it. The client moves itself to (0,0); wm_frame_sync() takes
 * that as a SCREEN place and moves the FRAME so the client would sit at (0,0)
 * — which puts the client back at (border, title) inside it — and the client,
 * seeing it is no longer at (0,0), moves itself again. Two programs each
 * undoing the other thirty times a second is a picture that slides up and down
 * and never settles.
 *
 * With the state answered none of that happens: the client is told it has the
 * whole screen, the chrome is dropped so that a window at (0,0) the size of
 * the screen really IS at (0,0) the size of the screen, and the client has
 * nothing to correct. It is still inside a frame — it always is — but the
 * frame is exactly the screen and the client is exactly the frame, so the two
 * agree and neither moves.
 *
 * A second thing is fixed at the same time. Wine, unable to get fullscreen the
 * EWMH way, reaches for the display instead and changes the MODE — the whole
 * desktop is resized under the window manager, which is what "the game changed
 * the size of the wm screen" is. Answering the state is what it is waiting
 * for, and once answered it leaves the mode alone.
 *
 * The screen is read from the display rather than from the workarea: fullscreen
 * means the whole screen, bar included, which is the point — the alternative is
 * a "fullscreen" game with a strip of desktop down one side. */
void wm_frame_set_fullscreen(WmCore *core, WmFrame *frame, int on) {
    if (!core || !frame || frame->frame == None) {
        return;
    }
    on = on ? 1 : 0;
    if (frame->fullscreen == on) {
        /* Already in the state that was asked for, and the request is still
           ANSWERED. "Already there" happens whenever the size test in
           wm_frame_create() or wm_frame_resize() got there first — and a
           client that asked with a state message and hears nothing back
           concludes the manager ignored it and asks again, for ever. The
           answer is the property, so it is written even when nothing about the
           window has to change.

           Written only on the way IN: a window that is not fullscreen and is
           told it is not has nothing to be told, and publishing an empty list
           on every unrelated call would fight a client that is setting the
           property itself. */
        /* !! DONGU RISKI: burada da property YAZILIYOR. Ayni deger tekrar tekrar
           yazilirsa manage_property -> set_fullscreen -> burada -> yaz ... sonsuz
           dongusu olusur (bkz. frame_publish_fullscreen_state uyarisi). Bu dala
           yalnizca cagiran taraf durumun GERCEKTEN degistigini dogruladiginda
           gelinmelidir; mevcut cagiranlar bunu garanti eder
           (wm_manage.c: wanted != wm_frame_is_fullscreen). */
        if (on) {
            frame_publish_fullscreen_state(core, frame, 1);
        }
        return;
    }

    /* THE GEOMETRY IS NOT TOUCHED AND THE CHROME IS NOT DROPPED — which is what
       a fullscreen request means on this desktop.

       A window here is a CONTAINER on the desktop. A client that asks for
       fullscreen is asking to be BIG, not to become the screen: the manager
       answers the STATE (it writes the EWMH property, which is what every
       toolkit — Wine included — actually waits for), brings the window forward
       and gives it the keyboard, and stops there. The title bar and the border
       stay on, so a game the user wants out of can still be closed, and the
       window keeps the size its own ConfigureRequest asked for.

       Resizing the window to the WHOLE SCREEN here would undo the point of the
       container: it would be the manager letting a client's idea of "the
       screen" decide the shape of the desktop, and a game would come up as a
       borderless surface covering everything — not a window at all. It would
       also be the stretch that makes a game that draws at a fixed size fight
       the manager. The desktop's own size is protected separately, by the randr
       guard in wm_randr.c, which is the half that stops a client resizing the
       SCREEN itself. */
    frame->fullscreen = on;
    frame_publish_fullscreen_state(core, frame, on);

    /* THE GEOMETRY IS NOT TOUCHED. A fullscreen request on this desktop is
       answered as a STATE — the EWMH property is written, which is what a
       toolkit like Wine actually waits for — and nothing else. The window is
       NOT resized to the screen and its chrome is NOT dropped: it stays a
       container on the desktop, exactly like every other window. "The window
       is not the screen" is the whole point of the diagram (diagram.md), and a
       window stretched to the screen is a window with no title bar and no
       close button — the trap this file exists to avoid. The desktop's own
       size is protected separately, by wm_randr.c. */
    if (on) {
        wm_frame_raise(core, frame);
        wm_focus_set(core, frame->client);
    } else {
        /* The state is dropped, so the resolution the client asked for while
           it held is forgotten with it: a window that leaves fullscreen is an
           ordinary window again, sized by its own report like every other. */
        frame->fullscreen_width = 0;
        frame->fullscreen_height = 0;
    }
}

/* Give the fullscreen window the resolution its client asked for by changing
 * the display mode.
 *
 * A Direct3D game cannot say how large it wants to be in any of the ordinary
 * ways — it publishes no size hint and creates its own window at whatever size
 * Wine decides, which is the size of the SCREEN Wine saw. The one place it does
 * state its resolution is the display mode: it asks the server for the mode it
 * draws at, and wm_randr.c catches that change and calls here with the
 * resolution before putting the desktop's own mode back.
 *
 * That resolution becomes the size of the CONTAINER. The window is not the
 * screen (diagram.md); the client asked for a resolution, so the container is
 * that resolution, chrome kept, and the game is shown at the size it chose
 * rather than stretched to the screen — which is what left it a small picture
 * in a field of black. The newest fullscreen window is the one that just
 * changed the mode, so it is the one given the size. */
void wm_frame_set_fullscreen_size(WmCore *core, int width, int height) {
    WmFrame *target = NULL;

    if (width < 1 || height < 1) {
        return;
    }

    for (int i = core->frame_count - 1; i >= 0; i--) {
        if (core->frames[i].fullscreen && !core->frames[i].minimized) {
            target = &core->frames[i];
            break;
        }
    }
    if (!target) {
        return;
    }

    /* Already this size: nothing to do. Without this the mode change a
       fullscreen game asks for is answered by a window resize on EVERY round,
       and a game that keeps re-requesting the mode (Wine and SDL both do) is
       then resized over and over while it starts — a fight that reads as the
       game hanging before it has drawn a frame. */
    if (target->fullscreen_width == width &&
        target->fullscreen_height == height) {
        return;
    }

    target->fullscreen_width = width;
    target->fullscreen_height = height;
    target->client_width = width;
    target->client_height = height;
    frame_clamp_size_to_workarea(core, target);
    frame_clamp_to_workarea(core, target);
    frame_apply(core, target);
    frame_notify_configure(core, target);

    fprintf(stderr,
            "gnuchanwm: frame: fullscreen client asked for %dx%d; "
            "container set to %dx%d\n",
            width, height, target->client_width, target->client_height);
}
