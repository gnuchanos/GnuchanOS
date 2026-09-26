/*
 * wm_keys.c — the key binding table.
 *
 * Every key the WM itself answers is registered here. A grab is taken on the
 * root window so the key reaches the WM whatever window has focus, which is
 * the whole point: Alt+Enter has to open a terminal whether the pointer is
 * over a terminal, a browser or the desktop.
 *
 * The table comes from the settings script, which writes it as data:
 *
 *     gcl_keys.all = [
 *         gcl_key.MultiKey(keys=[super_key1, "return"],
 *                          action="gcl_spawn.RunProgram(command=default_terminal)"),
 *     ]
 *
 * The config module parses that into config->bindings, resolving the modifier
 * names the script uses (super_key1) into the masks X wants. This file gives
 * each binding's action its meaning: the action is a string from the script,
 * and a string is not something a window manager can call, so it is matched
 * against the handful of things the WM itself does.
 *
 * When the script binds nothing — a machine with no config — the built-in
 * table below is used instead, so a fresh session still has Alt+Enter.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <X11/keysym.h>

#include "wm_core.h"
#include "wm_frame.h"
#include "wm_spawn.h"
#include "wm_config.h"
#include "wm_workspace.h"

typedef void (*KeyAction)(WmCore *core);

typedef struct KeyBinding {
    unsigned int modifiers;
    KeySym keysym;
    KeyAction action;
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

/* Go back to the window that was in use before this one.
 *
 * The target is the most recently used window, not the next one in the frame
 * table: that is what every desktop's switcher means, and it is what makes one
 * press undo one move — pressing it twice goes there and back. */
static void action_switch_window(WmCore *core) {
    WmFrame *previous = wm_focus_previous(core);
    if (previous) {
        wm_frame_activate(core, previous);
    }
}

static void action_reload_config(WmCore *core) {
    if (wm_config_reload(core) == 0) {
        fprintf(stderr, "gnuchanwm: hot reload applied\n");
    }
}

/* --- switching workspace --------------------------------------------------
 *
 * Super+1 is the first workspace, Super+2 the second, and so on up to whatever
 * number the script named — see wm_workspace_count(), which reads it from the
 * layout widget the bar draws. There are no bindings past that number, so a
 * session with four workspaces has no Super+5 to press rather than a key that
 * does nothing.
 *
 * They are built in rather than written in the script because the number of
 * them is the script's already: binding them there too would be the same
 * number written twice, and the two would drift. What the script decides is
 * how many there are; this decides what reaching them looks like.
 *
 * Each number needs its own function, because a key action takes no argument —
 * the grabbed key is a function pointer and nothing else. That is what the
 * table below is: one function per workspace, and MOD4+1..N pointed at them. */
#define WM_WORKSPACE_KEY_MAX 12

static void action_workspace_0(WmCore *core) { wm_workspace_switch(core, 0); }
static void action_workspace_1(WmCore *core) { wm_workspace_switch(core, 1); }
static void action_workspace_2(WmCore *core) { wm_workspace_switch(core, 2); }
static void action_workspace_3(WmCore *core) { wm_workspace_switch(core, 3); }
static void action_workspace_4(WmCore *core) { wm_workspace_switch(core, 4); }
static void action_workspace_5(WmCore *core) { wm_workspace_switch(core, 5); }
static void action_workspace_6(WmCore *core) { wm_workspace_switch(core, 6); }
static void action_workspace_7(WmCore *core) { wm_workspace_switch(core, 7); }
static void action_workspace_8(WmCore *core) { wm_workspace_switch(core, 8); }
static void action_workspace_9(WmCore *core) { wm_workspace_switch(core, 9); }
static void action_workspace_10(WmCore *core) { wm_workspace_switch(core, 10); }
static void action_workspace_11(WmCore *core) { wm_workspace_switch(core, 11); }

static const KeyAction WORKSPACE_ACTIONS[WM_WORKSPACE_KEY_MAX] = {
    action_workspace_0,  action_workspace_1,  action_workspace_2,
    action_workspace_3,  action_workspace_4,  action_workspace_5,
    action_workspace_6,  action_workspace_7,  action_workspace_8,
    action_workspace_9,  action_workspace_10, action_workspace_11,
};

/* The keysym for the number key that reaches a workspace: Super+1 is XK_1,
   which is the row above the letters, exactly as a desktop spells it. */
static KeySym workspace_keysym(int workspace) {
    switch (workspace) {
    case 0:  return XK_1;
    case 1:  return XK_2;
    case 2:  return XK_3;
    case 3:  return XK_4;
    case 4:  return XK_5;
    case 5:  return XK_6;
    case 6:  return XK_7;
    case 7:  return XK_8;
    case 8:  return XK_9;
    case 9:  return XK_0;
    case 10: return XK_minus;
    case 11: return XK_equal;
    default: return NoSymbol;
    }
}

/* --- the built-in table ---------------------------------------------------- */

/* The keys a session has before any script is read. Alt+Enter opens the
   terminal, Alt+Tab goes back a window, Alt+F4 closes the focused one, and
   Ctrl+Alt+R reloads the script by hand — the key that exists for a machine
   whose config the automatic reload could not read. */
static const KeyBinding BUILT_IN[] = {
    { Mod1Mask, XK_Return, action_spawn_terminal },
    { Mod4Mask, XK_Return, action_spawn_terminal },
    { Mod1Mask, XK_F4,     action_close_focused },
    { Mod4Mask, XK_F4,     action_close_focused },
    { Mod1Mask, XK_Tab,    action_switch_window },
    { Mod4Mask, XK_Tab,    action_switch_window },
    { ControlMask | Mod1Mask, XK_r, action_reload_config },
};

#define BUILT_IN_COUNT (sizeof(BUILT_IN) / sizeof(BUILT_IN[0]))
#define MAX_BINDINGS ((int)(WM_CONFIG_MAX_BINDINGS + BUILT_IN_COUNT))

/* The bindings the session actually has, and how many. Filled at start-up
   from the script when it binds keys, and from the table above when it does
   not. */
static KeyBinding bindings[MAX_BINDINGS];
static int binding_count = 0;

/* Which of the WM's own actions a written action names. The script writes
   actions as calls into a runtime the desktop shares; this is the small set
   of them this window manager can answer itself. Anything else is reported
   when the key is pressed rather than silently doing nothing. */
static KeyAction action_for(const char *written) {
    if (!written || !written[0]) {
        return NULL;
    }
    if (strstr(written, "RunProgram") || strstr(written, "spawn") ||
        strstr(written, "terminal") || strstr(written, "Terminal")) {
        return action_spawn_terminal;
    }
    if (strstr(written, "close") || strstr(written, "Close")) {
        return action_close_focused;
    }
    if (strstr(written, "switch") || strstr(written, "Switch") ||
        strstr(written, "focus_previous") || strstr(written, "next_window")) {
        return action_switch_window;
    }
    if (strstr(written, "reload") || strstr(written, "Reload")) {
        return action_reload_config;
    }
    return NULL;
}

/* Turn one written binding into one the grab can use: its key name into the
   keysym X keys the grab by, and its action into the function to call. A key
   name X does not know is reported and skipped, because a binding that cannot
   be resolved must not stop the rest of the table from being bound. */
static int add_config_binding(const WmBinding *binding) {
    if (binding_count >= MAX_BINDINGS) {
        return -1;
    }
    if (!binding->key[0] || binding->modifiers == 0) {
        return -1;
    }

    KeySym keysym = XStringToKeysym(binding->key);
    if (keysym == NoSymbol && binding->key[0]) { char cap[WM_CONFIG_TEXT_LENGTH]; snprintf(cap, sizeof(cap), "%s", binding->key); cap[0] = (char)toupper((unsigned char)cap[0]); keysym = XStringToKeysym(cap); }
    if (keysym == NoSymbol) {
        fprintf(stderr, "gnuchanwm: config: unknown key '%s'\n", binding->key);
        return -1;
    }

    KeyAction action = action_for(binding->action);
    if (!action) {
        /* The action names something the runtime does that this window
           manager does not: reported once at start-up, so a person can see
           why the key does not answer. */
        fprintf(stderr, "gnuchanwm: config: action '%s' is not one this WM does\n",
                binding->action);
        return -1;
    }

    bindings[binding_count].modifiers = binding->modifiers;
    bindings[binding_count].keysym = keysym;
    bindings[binding_count].action = action;
    binding_count++;
    return 0;
}

/* --- grabbing -------------------------------------------------------------- */

/* Grab every binding on the root. A grab that fails because another client
   already owns the key is reported by the X error handler and then skipped by
   X: one key that cannot be bound must not stop the rest. */
static int keys_init(WmCore *core) {
    binding_count = 0;

    /* The script's bindings first, then the reload key that always has to be
       there — it is the way back from a config that bound nothing usable. */
    for (int i = 0; i < core->config.binding_count; i++) {
        add_config_binding(&core->config.bindings[i]);
    }
    if (binding_count == 0) {
        for (unsigned int i = 0; i < BUILT_IN_COUNT; i++) {
            bindings[binding_count++] = BUILT_IN[i];
        }
    } else {
        bindings[binding_count++] = BUILT_IN[BUILT_IN_COUNT - 1];
    }

    /* Super+1..N, one per workspace the script asked for. They are added after
       the script's own bindings so a script that bound Super+1 to something
       else keeps it: the script's table is what a person wrote, and it wins
       over a built-in convenience. What this guarantees is that a workspace
       is always reachable, which is the floor a session needs — and the count
       comes from the config, so a session with four numbers has four keys and
       not six. */
    int workspace_count = wm_workspace_count(core);
    for (int i = 0; i < workspace_count && i < WM_WORKSPACE_KEY_MAX; i++) {
        if (binding_count >= MAX_BINDINGS) {
            break;
        }
        KeySym keysym = workspace_keysym(i);
        if (keysym == NoSymbol) {
            break;
        }
        int taken = 0;
        for (int j = 0; j < binding_count; j++) {
            if (bindings[j].keysym == keysym &&
                bindings[j].modifiers == Mod4Mask) {
                taken = 1;
                break;
            }
        }
        if (taken) {
            continue;
        }
        bindings[binding_count].modifiers = Mod4Mask;
        bindings[binding_count].keysym = keysym;
        bindings[binding_count].action = WORKSPACE_ACTIONS[i];
        binding_count++;
    }

    for (int i = 0; i < binding_count; i++) {
        KeyCode code = XKeysymToKeycode(core->display, bindings[i].keysym);
        if (code == 0) {
            continue;
        }
        /* The combination is grabbed with every lock modifier added on top,
           so CapsLock or NumLock being on does not stop the binding. */
        unsigned int combos[] = {
            bindings[i].modifiers,
            bindings[i].modifiers | LockMask,
            bindings[i].modifiers | Mod2Mask,
            bindings[i].modifiers | LockMask | Mod2Mask,
        };
        for (unsigned int j = 0; j < sizeof(combos) / sizeof(combos[0]); j++) {
            XGrabKey(core->display, code, combos[j], core->root, True,
                     GrabModeAsync, GrabModeAsync);
        }
    }
    XSync(core->display, False);
    fprintf(stderr, "gnuchanwm: %d key binding(s) registered\n", binding_count);
    return 0;
}

/* Find the action for a pressed key, or NULL. The lock modifiers are masked
   off because the grab was taken with them included. */
static KeyAction keys_lookup(WmCore *core, XKeyEvent *event) {
    KeySym keysym = XLookupKeysym(event, 0);
    unsigned int state = event->state & ~(LockMask | Mod2Mask);
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i].keysym == keysym && bindings[i].modifiers == state) {
            return bindings[i].action;
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
    for (int i = 0; i < binding_count; i++) {
        KeyCode code = XKeysymToKeycode(core->display, bindings[i].keysym);
        if (code == 0) {
            continue;
        }
        unsigned int combos[] = {
            bindings[i].modifiers,
            bindings[i].modifiers | LockMask,
            bindings[i].modifiers | Mod2Mask,
            bindings[i].modifiers | LockMask | Mod2Mask,
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
    .tick = NULL,
    .interval_ms = 0,
    .cleanup = keys_cleanup,
};
