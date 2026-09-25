/*
 * wm_keys.c — the key binding table.
 *
 * Every key the WM itself answers is registered here. A grab is taken on the
 * root window so the key reaches the WM whatever window has focus, which is
 * the whole point: Alt+Enter has to open a terminal whether the pointer is
 * over a terminal, a browser or the desktop.
 *
 * The table is data, not code: a binding is a (modifiers, keysym, action)
 * row, and adding one is adding a row. That is what keeps "which key does
 * what" in one readable place as the WM grows.
 */
#include <stdio.h>
#include <string.h>

#include <X11/keysym.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_spawn.h"

typedef void (*KeyAction)(WmCore *core);

typedef struct KeyBinding {
    unsigned int modifiers;
    KeySym keysym;
    KeyAction action;
    const char *description;
} KeyBinding;

/* --- the actions a key can be bound to ------------------------------------- */

static void action_spawn_terminal(WmCore *core) {
    (void)core;
    wm_spawn_terminal();
}

/* Close whatever has the focus. The focused window is a client, so closing it
   means finding the frame that holds it. */
static void action_close_focused(WmCore *core) {
    WmFrame *frame = wm_frame_find(core, core->focused);
    if (frame) {
        wm_frame_close(core, frame);
    }
}

/* Alt+Tab: go to the next window.
 *
 * The window the switch starts from is the focused one, and the switch itself
 * is wm_frame_activate(), which is also what a click and the focus module use.
 * So there is one answer to "go to this window" and the key only chooses which
 * window that is. Minimised windows are restored by the same call, which is
 * what makes the key the way back from the minimise button.
 *
 * Nothing is drawn for the switch: a list of windows on screen needs a grab
 * and a release to end it, and what a machine with one window open sees is a
 * pop-up listing one name. The window that arrives is its own announcement. */
static void action_switch_window(WmCore *core) {
    WmFrame *next = wm_frame_next(core, core->focused);
    if (next) {
        wm_frame_activate(core, next);
    }
}

/* --- the table ------------------------------------------------------------- */

static const KeyBinding BINDINGS[] = {
    /* Alt+Enter opens the default terminal. Mod1Mask is Alt on every layout
       that matters; Mod4Mask (Super) is the alternative people remap to, so
       both are bound to the same action. */
    { Mod1Mask, XK_Return, action_spawn_terminal, "open terminal" },
    { Mod4Mask, XK_Return, action_spawn_terminal, "open terminal" },
    { Mod1Mask, XK_F4, action_close_focused, "close window" },
    { Mod4Mask, XK_F4, action_close_focused, "close window" },
    { Mod1Mask, XK_Tab, action_switch_window, "next window" },
    { Mod4Mask, XK_Tab, action_switch_window, "next window" },
};

#define BINDING_COUNT (sizeof(BINDINGS) / sizeof(BINDINGS[0]))

/* Grab every binding on the root. A grab that fails because another client
   already owns the key is reported and then skipped: one key that cannot be
   bound must not stop the rest of the table from being bound. */
static int keys_init(WmCore *core) {
    int bound = 0;
    for (unsigned int i = 0; i < BINDING_COUNT; i++) {
        const KeyBinding *binding = &BINDINGS[i];
        KeyCode code = XKeysymToKeycode(core->display, binding->keysym);
        if (code == 0) {
            continue;
        }
        /* The modifier combination is grabbed with every lock modifier added
           on top, so CapsLock or NumLock being on does not stop the binding
           from firing. */
        unsigned int combos[] = {
            binding->modifiers,
            binding->modifiers | LockMask,
            binding->modifiers | Mod2Mask,
            binding->modifiers | LockMask | Mod2Mask,
        };
        for (unsigned int j = 0; j < sizeof(combos) / sizeof(combos[0]); j++) {
            XGrabKey(core->display, code, combos[j], core->root, True,
                     GrabModeAsync, GrabModeAsync);
        }
        bound++;
    }
    XSync(core->display, False);
    fprintf(stderr, "gnuchanwm: %d key binding(s) registered\n", bound);
    return 0;
}

/* Find the action for a pressed key, or NULL. The lock modifiers are masked
   off because the grab was taken with them included. */
static KeyAction keys_lookup(WmCore *core, XKeyEvent *event) {
    KeySym keysym = XLookupKeysym(event, 0);
    unsigned int state = event->state &
                         ~(LockMask | Mod2Mask);
    for (unsigned int i = 0; i < BINDING_COUNT; i++) {
        const KeyBinding *binding = &BINDINGS[i];
        if (binding->keysym == keysym && binding->modifiers == state) {
            return binding->action;
        }
    }
    (void)core;
    return NULL;
}

static void keys_event(WmCore *core, XEvent *event) {
    if (event->type != KeyPress) {
        return;
    }
    KeyAction action = keys_lookup(core, &event->xkey);
    if (action) {
        action(core);
    }
}

static void keys_cleanup(WmCore *core) {
    for (unsigned int i = 0; i < BINDING_COUNT; i++) {
        const KeyBinding *binding = &BINDINGS[i];
        KeyCode code = XKeysymToKeycode(core->display, binding->keysym);
        if (code == 0) {
            continue;
        }
        unsigned int combos[] = {
            binding->modifiers,
            binding->modifiers | LockMask,
            binding->modifiers | Mod2Mask,
            binding->modifiers | LockMask | Mod2Mask,
        };
        for (unsigned int j = 0; j < sizeof(combos) / sizeof(combos[0]); j++) {
            XUngrabKey(core->display, code, combos[j], core->root);
        }
    }
    XSync(core->display, False);
}

const WmModule wm_keys_module = {
    .name = "keys",
    .init = keys_init,
    .event = keys_event,
    .cleanup = keys_cleanup,
};
