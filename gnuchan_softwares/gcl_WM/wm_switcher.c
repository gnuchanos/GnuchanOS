/*
 * wm_switcher.c — the window list, and the gesture that walks it.
 *
 * What Alt+` does, in order:
 *
 *   pressed        the windows on this workspace are collected, in the order
 *                  they were last used, and the second of them is chosen —
 *                  the first being the window the user is already in, so the
 *                  one press that opens the switcher is also the one that
 *                  moves to the window before this one. That is what the old
 *                  switch key did, and a switcher that opened on the window
 *                  the user was already in would be a switcher whose first
 *                  press does nothing.
 *   pressed again  the choice moves on, wrapping at the end
 *   released       the chosen window comes forward and the switcher goes
 *   left clicked   on a picture: that window comes forward
 *   Escape         nothing comes forward
 *
 * The keyboard and the pointer are both held for the length of the gesture.
 * The keyboard because the release of the modifier is what confirms the
 * choice, and a release is not something a passive grab on one key delivers;
 * the pointer because a click has to land on a picture rather than on whatever
 * is underneath it.
 *
 * --- the modifier ---------------------------------------------------------
 *
 * The gesture is bound to Alt+` and to Super+` , and either modifier being let
 * go is what confirms it. Which one is being held is read from the key
 * release: a KeyRelease of a key that was down when the switcher opened. That
 * is a broader test than "the release of the exact modifier the grab was taken
 * for", and it is the right one — a user who opens the switcher with Alt and
 * then presses and releases Super should not have the switcher confirmed by a
 * key they were not holding when it opened.
 *
 * The one thing the test has to survive is auto-repeat: a held modifier is
 * reported by the server as a release and a press, over and over, and taking
 * the first of those releases as "the hand let go" would confirm the choice
 * the instant the key went down. The release is therefore only believed when
 * the press that follows it is not already waiting in the queue — see
 * release_is_repeat().
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_switcher.h"

/* The key the gesture is bound to. XK_grave is the key above Tab on a US
   layout — the one that writes ` and ~ — and it is what the user asked for.
   The keysym is what is compared rather than the keycode, so a layout that
   puts ` somewhere else still gets a key that writes it. */
#define WM_SWITCHER_KEYSYM XK_grave

/* The modifiers the gesture answers to: Alt and Super, the two this desktop
   uses for everything else. Both are grabbed, and the one that is actually
   held is worked out from the event's own state. */
static const unsigned int SWITCHER_MODIFIERS[] = { Mod1Mask, Mod4Mask };
#define SWITCHER_MODIFIER_COUNT \
    ((unsigned int)(sizeof(SWITCHER_MODIFIERS) / sizeof(SWITCHER_MODIFIERS[0])))

/* The one switcher. One exists, so its state lives here rather than on the
   core — see wm_switcher.h. */
static WmSwitcher switcher;

/* The modifiers that were down when the gesture started, so a modifier the
   user was not holding cannot confirm it. Read from the opening press's own
   state with the lock bits taken off, which is what `state & ~(LockMask |
   Mod2Mask)` is everywhere else in this manager. */
static unsigned int held_modifiers = 0;

/* Whether the keyboard and the pointer are held. Tracked separately from
   switcher.open because the two are not the same claim: an open switcher with
   no grab is one the user is looking at and cannot confirm. */
static int input_held = 0;

/* --- the list ------------------------------------------------------------- */

/* Add one frame to the list, if it is not there already. A window appears once
   whatever the order it is walked in — the MRU order holds each window once,
   but the frame table is walked as well, and the same window could be reached
   from both.
 *
 * A minimised window is in the list on purpose: it is one of the windows the
 * user has open, and this is the one place on this desktop where it can be
 * reached from, because there is no task list to click it in. Its picture is
 * the one thing it cannot give — nothing of it is on the screen — so the cell
 * falls back to its icon. */
static void cell_add(WmCore *core, Window client) {
    if (switcher.count >= WM_SWITCHER_MAX_CELLS || client == None) {
        return;
    }
    for (int i = 0; i < switcher.count; i++) {
        if (switcher.cells[i].client == client) {
            return;
        }
    }

    /* Only a window the manager holds can be activated, and only a frame has
       the client and the title the cell is made of. */
    WmFrame *frame = wm_frame_find(core, client);
    if (!frame) {
        return;
    }

    WmSwitcherCell *cell = &switcher.cells[switcher.count];
    memset(cell, 0, sizeof(*cell));
    cell->client = frame->client;
    cell->frame = frame->frame;
    cell->minimized = frame->minimized;
    cell->workspace = frame->workspace;
    snprintf(cell->name, sizeof(cell->name), "%s",
             frame->has_name && frame->name[0] ? frame->name : "window");
    switcher.count++;
}

/* Fill the list, most recently used first.
 *
 * The order is the focus history's and not the frame table's, and that is the
 * whole reason this is a list of its own: the frame table is creation order,
 * so the second entry is "the second program that was started" rather than
 * "the window before this one". The history is already kept by wm_focus.c for
 * exactly this purpose — one press of the key undoes one move — so it is read
 * here and the frame table is only walked for the windows the history has
 * forgotten, which are the ones that have never been focused.
 *
 * Only the current workspace's windows are collected: a window on another
 * desktop has no picture anywhere on this one, and bringing it forward from
 * here would be switching two things at once — the window and the desktop it
 * lives on. Switching desktops is what Super+1..N is for.
 */
static void list_build(WmCore *core) {
    switcher.count = 0;
    switcher.selected = -1;

    for (int i = 0; i < core->focus_history_count; i++) {
        Window window = core->focus_history[i];
        WmFrame *frame = wm_frame_find(core, window);
        if (!frame || frame->workspace != core->current_workspace) {
            continue;
        }
        cell_add(core, window);
    }

    for (int i = 0; i < core->frame_count; i++) {
        WmFrame *frame = &core->frames[i];
        if (frame->workspace != core->current_workspace) {
            continue;
        }
        cell_add(core, frame->client);
    }

    /* The window the user is in is first, whatever the history said, because
       the first entry is the one place this list is read from as "where I am"
       — see wm_switcher_open, which chooses the entry after it. A window that
       has just opened and not yet been focused is put at the back rather than
       at the front, so opening a program does not change where one press of
       the key lands.
     *
     * The reordering is a rotation and not a sort: the entry that is the
     * focused window is moved to the front, everything before it slides back
     * by one, and the order of the rest is untouched. */
    if (core->focused != None && switcher.count > 1) {
        int at = -1;
        for (int i = 0; i < switcher.count; i++) {
            if (switcher.cells[i].client == core->focused) {
                at = i;
                break;
            }
        }
        if (at > 0) {
            WmSwitcherCell moved = switcher.cells[at];
            for (int i = at; i > 0; i--) {
                switcher.cells[i] = switcher.cells[i - 1];
            }
            switcher.cells[0] = moved;
        }
    }
}

/* --- the input ------------------------------------------------------------ */

/* Whether a release of a modifier is really the hand letting go, rather than
   the release half of the server's auto-repeat.
 *
 * A held key with auto-repeat on is reported as a release and a press in the
 * same instant, over and over. The two are told apart by looking for the press
 * that the release would be followed by: `same_keycode` and `same_time`
 * together are what Xlib calls a repeat pair, and a release with one waiting
 * behind it is not a hand letting go.
 *
 * The press is put back when it is found, because the next pass of the loop
 * has to see it: swallowing it here would lose the event that the repeat test
 * of the pass after that depends on. */
static int release_is_repeat(WmCore *core, XKeyEvent *release) {
    XEvent next;
    if (!XCheckTypedEvent(core->display, KeyPress, &next)) {
        return 0;
    }
    if (next.xkey.keycode != release->keycode ||
        next.xkey.time != release->time) {
        XPutBackEvent(core->display, &next);
        return 0;
    }
    return 1;
}

/* Whether the key that was released is one of the modifiers the gesture was
   opened with. A key that was not down when the switcher opened is not one
   letting go of it. */
static int release_confirms(XKeyEvent *release) {
    unsigned int modifier = release->state & (ShiftMask | ControlMask |
                                              Mod1Mask | Mod2Mask |
                                              Mod3Mask | Mod4Mask | Mod5Mask);
    return (held_modifiers & modifier) != 0;
}

/* Take the keyboard and the pointer for the length of the gesture.
 *
 * The keyboard is what makes the release of the modifier reach us at all: a
 * passive grab on one key combination delivers that combination's presses and
 * the releases of its own key, and the release of Alt is neither. While the
 * explicit grab is held every key is delivered here, which is the only way to
 * hear the hand let go.
 *
 * Both are taken asynchronously, so neither freezes the server: a grab that
 * took the keyboard in sync mode would hold it until this manager answered,
 * and a manager that is drawing a grid of pictures is not in a position to
 * answer anything promptly. */
static int grab_input(WmCore *core) {
    int keyboard = XGrabKeyboard(core->display, core->root, False,
                                 GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(core->display, core->root, False,
                 ButtonPressMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XSync(core->display, False);

    if (keyboard != GrabSuccess) {
        /* Nothing can confirm a switcher whose keyboard is not held, so it is
           not put up: a grid of pictures that cannot be dismissed and cannot
           choose anything is worse than a key that did nothing. */
        fprintf(stderr, "gnuchanwm: switcher: the keyboard could not be "
                        "grabbed (%d)\n", keyboard);
        return 0;
    }
    input_held = 1;
    return 1;
}

static void ungrab_input(WmCore *core) {
    if (!input_held) {
        return;
    }
    XUngrabKeyboard(core->display, CurrentTime);
    XUngrabPointer(core->display, CurrentTime);
    XSync(core->display, False);
    input_held = 0;
}

/* --- opening, moving, choosing -------------------------------------------- */

int wm_switcher_open(WmCore *core) {
    if (!core) {
        return 0;
    }

    /* Already up: every press after the first moves the choice, which is what
       holding the key means. */
    if (switcher.open) {
        wm_switcher_advance(core, 1);
        return 1;
    }

    list_build(core);
    if (switcher.count == 0) {
        return 0;
    }

    /* The windows have to be collected, but a switcher with one window in it
       is a switcher with nothing to switch to: one press would open a grid of
       one picture and the next would choose the window the user is already in.
       Nothing is offered and nothing is taken. */
    if (switcher.count == 1) {
        return 0;
    }

    /* The second entry is chosen before the pictures are read: the current
       window is first by construction, so this is the window the user was in
       before this one, and opening the switcher with it chosen is what makes
       one press of the key do what the old switch key did. The screen is
       captured with that choice already made, so the highlight is painted into
       the snapshot rather than drawn on after it. */
    switcher.selected = 1;
    switcher.open = 1;

    if (wm_switcher_view_open(core, &switcher) != 0) {
        switcher.open = 0;
        switcher.count = 0;
        switcher.selected = -1;
        return 0;
    }
    if (!grab_input(core)) {
        wm_switcher_view_close(core);
        switcher.open = 0;
        switcher.count = 0;
        switcher.selected = -1;
        return 0;
    }
    return 1;
}

void wm_switcher_advance(WmCore *core, int step) {
    if (!switcher.open || switcher.count <= 0) {
        return;
    }
    switcher.selected += step;
    while (switcher.selected < 0) {
        switcher.selected += switcher.count;
    }
    while (switcher.selected >= switcher.count) {
        switcher.selected -= switcher.count;
    }
    wm_switcher_view_show(core, &switcher);
}

void wm_switcher_commit(WmCore *core) {
    Window chosen = None;

    if (!switcher.open) {
        return;
    }
    if (switcher.selected >= 0 && switcher.selected < switcher.count) {
        chosen = switcher.cells[switcher.selected].client;
    }

    /* The overlay goes before the window is activated, and that is not
       tidiness: bringing a window forward raises it, and a raise under an
       overlay that is still up would put the window behind the pictures of it.
       The pictures are of the desktop as it was a moment ago anyway, so
       nothing is lost by taking them down first. */
    wm_switcher_view_close(core);
    ungrab_input(core);

    switcher.open = 0;
    switcher.count = 0;
    switcher.selected = -1;

    if (chosen != None) {
        WmFrame *frame = wm_frame_find(core, chosen);
        if (frame) {
            wm_frame_activate(core, frame);
        }
    }
    XFlush(core->display);
}

void wm_switcher_cancel(WmCore *core) {
    if (!switcher.open) {
        return;
    }
    wm_switcher_view_close(core);
    ungrab_input(core);
    switcher.open = 0;
    switcher.count = 0;
    switcher.selected = -1;
    XFlush(core->display);
}

void wm_switcher_raise(WmCore *core) {
    (void)core;
    wm_switcher_overlay_raise();
}

/* --- the module ------------------------------------------------------------ */

/* Grab the gesture on the root, for both modifiers and for every lock
   combination. The grab is what makes the key reach this manager whatever has
   the focus, which is the same reason every key in wm_keys.c is grabbed the
   same way.
 *
 * The key is grabbed here rather than written into wm_keys.c's table on
 * purpose. It is not an action a script can bind or unbind — the switcher is a
 * gesture, and its release is part of it — and a table entry would be an entry
 * whose action does half of what the key does. What a script decides is what
 * the switcher does; this decides that the key is the switcher's.
   */
static void switcher_grab(WmCore *core, int take) {
    KeyCode code = XKeysymToKeycode(core->display, WM_SWITCHER_KEYSYM);
    if (code == 0) {
        return;
    }
    for (unsigned int i = 0; i < SWITCHER_MODIFIER_COUNT; i++) {
        unsigned int combos[] = {
            SWITCHER_MODIFIERS[i],
            SWITCHER_MODIFIERS[i] | LockMask,
            SWITCHER_MODIFIERS[i] | Mod2Mask,
            SWITCHER_MODIFIERS[i] | LockMask | Mod2Mask,
        };
        for (unsigned int j = 0; j < sizeof(combos) / sizeof(combos[0]); j++) {
            if (take) {
                XGrabKey(core->display, code, combos[j], core->root, True,
                         GrabModeAsync, GrabModeAsync);
            } else {
                XUngrabKey(core->display, code, combos[j], core->root);
            }
        }
    }
    XSync(core->display, False);
}

static int switcher_init(WmCore *core) {
    memset(&switcher, 0, sizeof(switcher));
    switcher.selected = -1;
    input_held = 0;
    held_modifiers = 0;
    switcher_grab(core, 1);
    return 0;
}

/* Whether a key event is the gesture's own key, by keysym rather than by
   keycode: a layout that moves ` to another key still opens the switcher from
   the key that writes it. */
static int is_switcher_key(XKeyEvent *key) {
    KeySym symbol = XLookupKeysym(key, 0);
    return symbol == WM_SWITCHER_KEYSYM;
}

static void switcher_key_press(WmCore *core, XKeyEvent *key) {
    if (!switcher.open) {
        if (!is_switcher_key(key)) {
            return;
        }
        /* The modifiers held at this instant are the ones the gesture is made
           of. Taken with the lock bits off, so CapsLock being on does not make
           the release test below impossible to satisfy. */
        held_modifiers = key->state & (ShiftMask | ControlMask | Mod1Mask |
                                       Mod2Mask | Mod3Mask | Mod4Mask |
                                       Mod5Mask);
        held_modifiers &= ~(LockMask | Mod2Mask);
        wm_switcher_open(core);
        return;
    }

    /* Up: the same key again moves the choice on, which is what holding it
       does — the server's own auto-repeat turns a held key into a stream of
       presses, and each one steps the switcher. */
    if (is_switcher_key(key)) {
        wm_switcher_advance(core, 1);
        return;
    }
    /* Tab and the arrows as well, so the gesture works from the keys a hand
       is already on. */
    switch (XLookupKeysym(key, 0)) {
    case XK_Tab:
    case XK_Down:
    case XK_Right:
        wm_switcher_advance(core, 1);
        break;
    case XK_ISO_Left_Tab:
    case XK_Up:
    case XK_Left:
        wm_switcher_advance(core, -1);
        break;
    case XK_Escape:
        wm_switcher_cancel(core);
        break;
    default:
        break;
    }
}

static void switcher_event(WmCore *core, XEvent *event) {
    switch (event->type) {
    case KeyPress:
        switcher_key_press(core, &event->xkey);
        break;

    case KeyRelease:
        /* The hand letting go of the modifier is what confirms the choice,
           and that is the whole point of holding the keyboard: a passive grab
           on one combination would never deliver this. A release that is the
           other half of the server's auto-repeat is not a hand letting go —
           see release_is_repeat() — and neither is the release of a key that
           was not down when the gesture started. */
        if (!switcher.open) {
            break;
        }
        if (!release_confirms(&event->xkey)) {
            break;
        }
        if (release_is_repeat(core, &event->xkey)) {
            break;
        }
        wm_switcher_commit(core);
        break;

    case ButtonPress:
        /* A left click on a picture chooses that window; anywhere else is a
           click that meant "not this", which takes the switcher down. The
           click arrives with screen coordinates because the pointer is held
           against the root. */
        if (!switcher.open || event->xbutton.button != Button1) {
            break;
        }
        {
            int cell = wm_switcher_view_cell_at(core, event->xbutton.x_root,
                                                event->xbutton.y_root);
            if (cell < 0) {
                wm_switcher_cancel(core);
            } else {
                switcher.selected = cell;
                wm_switcher_commit(core);
            }
        }
        break;

    case MotionNotify:
        if (switcher.open) {
            wm_switcher_view_motion(core, &switcher,
                                    event->xmotion.x_root,
                                    event->xmotion.y_root);
        }
        break;

    case Expose:
        if (switcher.open &&
            event->xexpose.window == wm_switcher_overlay_window() &&
            event->xexpose.count == 0) {
            wm_switcher_view_show(core, &switcher);
        }
        break;

    default:
        break;
    }
}

static void switcher_cleanup(WmCore *core) {
    if (switcher.open) {
        wm_switcher_view_close(core);
    }
    ungrab_input(core);
    switcher_grab(core, 0);
    switcher.open = 0;
    switcher.count = 0;
    switcher.selected = -1;
}

const WmModule wm_switcher_module = {
    .name = "switcher",
    .init = switcher_init,
    .event = switcher_event,
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = switcher_cleanup,
};
