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
 *                          action=gcl_spawn.RunProgram(command=default_terminal)),
 *         gcl_key.MultiKey(keys=[super_key1, "F4"],
 *                          action=gcl_window.Close()),
 *     ]
 *
 * The action is a call, not a string. The config module parses the script,
 * resolves the modifier names the script uses (super_key1) into the masks X
 * wants, and writes each binding's action as the call's own name —
 * "gcl_spawn.RunProgram" — with the call's `command` argument resolved beside
 * it. This file gives that name its meaning: the leaf after the dot is matched
 * whole against the handful of things the WM itself does, and the command is
 * passed to the action so `command=default_terminal` opens the terminal the
 * script named rather than a name it guessed.
 *
 * When the script binds nothing — a machine with no config — the built-in
 * table below is used instead, so a fresh session still has Alt+Enter.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "wm_core.h"
#include "wm_compositor.h"
#include "wm_desktop.h"
#include "wm_frame.h"
#include "wm_spawn.h"
#include "wm_config.h"
#include "wm_workspace.h"

/* An action, and the command the binding gave it. Most actions ignore the
   command — Close closes the focused window whatever it was told — and the one
   that uses it, spawn, runs it. Passing it here rather than reading a global
   is what makes the table a table of calls rather than a table of names. */
typedef void (*KeyAction)(WmCore *core, const char *command);

typedef struct KeyBinding {
    unsigned int modifiers;
    KeySym keysym;
    KeyAction action;
    char command[WM_CONFIG_TEXT_LENGTH];
} KeyBinding;

/* --- the actions a key can be bound to ------------------------------------- */

/* Run what the binding named, or open a terminal when it named nothing.
 *
 * The command is a command LINE — `RunProgram(command="rofi -show run")` — so
 * it goes through wm_spawn_command(), which splits it into the words the
 * process must be started with. Handing the whole line to wm_spawn() instead
 * asked the kernel for a program whose name contained a space, which no such
 * program answers to: the run key did nothing and the terminal fallback was
 * never reached, because the fork itself had already succeeded.
 *
 * A binding with no command — `RunProgram()` with nothing said, or a script
 * that only named a terminal to fall back to — opens the session's terminal. */
static void action_spawn_terminal(WmCore *core, const char *command) {
    (void)core;
    if (command && command[0]) {
        if (wm_spawn_command(command) == 0) {
            return;
        }
    }
    wm_spawn_terminal();
}

/* Close whatever has the focus. The focused window is a client, so closing it
   means finding the frame that holds it. */
static void action_close_focused(WmCore *core, const char *command) {
    (void)command;
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
static void action_switch_window(WmCore *core, const char *command) {
    (void)command;
    WmFrame *previous = wm_focus_previous(core);
    if (previous) {
        wm_frame_activate(core, previous);
    }
}

/* Fit the focused window's content into its frame, or stop doing it.
 *
 * This is the key for a program that will not resize: the frame can be made
 * small but the program goes on drawing at its own size, so most of it is
 * outside the frame and gone. With this on, the window is put back to the size
 * it drew at and the picture of it is fitted into the frame instead — the
 * whole interface, smaller. See wm_compositor.h for the mechanism and what it
 * costs.
 *
 * A window the server cannot do this to, or one that cannot be scaled at all,
 * is left exactly as it was and says so in the log. Nothing about the desktop
 * changes for it, which is the property that makes the key safe to press. */
static void action_toggle_scaling(WmCore *core, const char *command) {
    (void)command;
    WmFrame *frame = wm_frame_find(core, core->focused);
    if (frame) {
        wm_frame_toggle_scaling(core, frame);
    }
}

/* Ctrl+Alt+R: read the settings script again, whatever it looks like on disk.
 *
 * This is the by-hand path, and it is deliberately not the same as the idle
 * tick's: the tick skips a file that has not changed, and a person who pressed
 * the key is asking to see it applied now. So the read is forced, and the two
 * things the config feeds that the config module cannot reach itself are redone
 * with it — the bar, which is drawn by the desktop module, and every frame's
 * geometry, which the reload already put right through wm_config_apply. A
 * script that could not be read does not change the desktop: wm_config_apply is
 * not reached, and the reason is put in a window by the config module. */
static void action_reload_config(WmCore *core, const char *command) {
    (void)command;
    if (wm_config_reload_forced(core)) {
        /* wm_config_apply() already put every frame's border right and drew it
           again; the bar is the one thing it does not reach, because the bar
           belongs to the desktop module. So only the bar is repainted here. */
        wm_desktop_repaint(core);
        fprintf(stderr, "gnuchanwm: hot reload applied (Ctrl+Alt+R)\n");
    }
}

/* Screen saver: run the session's screen saver now, on purpose.
 *
 * This is the key a person presses when they are leaving the desk and want the
 * saver up before the idle timer would have raised it, so the saver is started
 * with its "at once" flag — GnuChanSS --once — rather than left to wait out the
 * idle time it would otherwise wait for. The binding may name its own command
 * (`ScreenSaver(command="GnuChanSS --effect 3dwall")`); with none written, the
 * session's own saver is looked for on PATH by name.
 *
 * It is a background program: the key press starts it and returns, and the
 * saver covers the screen on its own. Nothing here waits for it. */
static void action_screen_saver(WmCore *core, const char *command) {
    (void)core;
    if (command && command[0]) {
        wm_spawn_command(command);
        return;
    }
    if (wm_spawn_command("GnuChanSS --once") != 0) {
        wm_spawn_command("/usr/local/bin/GnuChanSS --once");
    }
}

/* Lock screen: put the session's lock screen up now.
 *
 * The lock screen does not return until the right password has been typed, so
 * it is started and the WM goes on drawing behind it: the lock window covers
 * everything while it is up. The binding may name its own command; with none
 * written, the session's own locker is looked for by name, then in the usual
 * install location. */
static void action_lock_screen(WmCore *core, const char *command) {
    (void)core;
    if (command && command[0]) {
        wm_spawn_command(command);
        return;
    }
    if (wm_spawn_command("GnuChanSL") != 0) {
        wm_spawn_command("/usr/local/bin/GnuChanSL");
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
 * Super+Shift+1 is the other half of the same idea: it sends the focused
 * window to that workspace and leaves the user where they are. The two reads
 * of the number are the two things a hand does with a desk — go to it, or put
 * something on it — and they are bound together here so a session with four
 * workspaces has four of each rather than one set that stops where the other
 * carries on.
 *
 * Each number needs its own function, because a key action takes no workspace
 * argument — the grabbed key is a function pointer and nothing else. That is
 * what the tables below are: one function per workspace for each of the two
 * things, and MOD4 and MOD4+SHIFT pointed at them. */
#define WM_WORKSPACE_KEY_MAX 12

static void action_workspace_0(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 0); }
static void action_workspace_1(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 1); }
static void action_workspace_2(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 2); }
static void action_workspace_3(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 3); }
static void action_workspace_4(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 4); }
static void action_workspace_5(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 5); }
static void action_workspace_6(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 6); }
static void action_workspace_7(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 7); }
static void action_workspace_8(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 8); }
static void action_workspace_9(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 9); }
static void action_workspace_10(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 10); }
static void action_workspace_11(WmCore *core, const char *command) { (void)command; wm_workspace_switch(core, 11); }

static const KeyAction WORKSPACE_ACTIONS[WM_WORKSPACE_KEY_MAX] = {
    action_workspace_0,  action_workspace_1,  action_workspace_2,
    action_workspace_3,  action_workspace_4,  action_workspace_5,
    action_workspace_6,  action_workspace_7,  action_workspace_8,
    action_workspace_9,  action_workspace_10, action_workspace_11,
};

/* Super+Shift+1..N: send the focused window to that workspace and stay here.
   The same twelve numbers as above, and the mirror image of them — one set
   goes to a desk, the other sends something to it. */
static void action_move_to_0(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 0); }
static void action_move_to_1(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 1); }
static void action_move_to_2(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 2); }
static void action_move_to_3(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 3); }
static void action_move_to_4(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 4); }
static void action_move_to_5(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 5); }
static void action_move_to_6(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 6); }
static void action_move_to_7(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 7); }
static void action_move_to_8(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 8); }
static void action_move_to_9(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 9); }
static void action_move_to_10(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 10); }
static void action_move_to_11(WmCore *core, const char *command) { (void)command; wm_workspace_move(core, 11); }

static const KeyAction WORKSPACE_MOVE_ACTIONS[WM_WORKSPACE_KEY_MAX] = {
    action_move_to_0,  action_move_to_1,  action_move_to_2,
    action_move_to_3,  action_move_to_4,  action_move_to_5,
    action_move_to_6,  action_move_to_7,  action_move_to_8,
    action_move_to_9,  action_move_to_10, action_move_to_11,
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
    { Mod1Mask, XK_Return, action_spawn_terminal, "" },
    { Mod4Mask, XK_Return, action_spawn_terminal, "" },
    { Mod1Mask, XK_F4,     action_close_focused,  "" },
    { Mod4Mask, XK_F4,     action_close_focused,  "" },
    { Mod1Mask, XK_Tab,    action_switch_window,  "" },
    { Mod4Mask, XK_Tab,    action_switch_window,  "" },
    { Mod4Mask, XK_s,      action_toggle_scaling, "" },
    /* Alt+S raises the screen saver and Alt+L and Super+L lock the screen.
       They are the keys a hand reaches for on the way out of a room, so they
       are built in rather than waiting for a script. Alt+S runs
       GnuChanSS --once (the saver now, not after the idle time); the two lock
       keys run GnuChanSL. A script that binds any of these itself keeps its own
       binding — see keys_init(). */
    { Mod1Mask, XK_s,      action_screen_saver,   "" },
    { Mod1Mask, XK_l,      action_lock_screen,    "" },
    { Mod4Mask, XK_l,      action_lock_screen,    "" },
    /* Last, and it has to stay last: keys_init() adds this one entry when the
       script DID bind something, because it is the way back from a script that
       bound nothing usable. Anything added below it would take its place. */
    { ControlMask | Mod1Mask, XK_r, action_reload_config, "" },
};

#define BUILT_IN_COUNT (sizeof(BUILT_IN) / sizeof(BUILT_IN[0]))

/* The room the table needs: the script's own bindings, the built-in ones that
   stand behind them, and the two sets of workspace keys the session's number
   of desks is bound from.
 *
 * The workspace keys are counted here and not left out, and that is the fix
 * for a table that used to be able to overflow without saying so: the loop
 * that adds them stops at this ceiling, so a session that bound the script's
 * full 64 AND had twelve workspaces would have the last of the move keys
 * dropped silently — a Super+Shift+N that looks bound and is not. Counting
 * them makes the ceiling the truth. */
#define MAX_BINDINGS \
    ((int)(WM_CONFIG_MAX_BINDINGS + BUILT_IN_COUNT + 2 * WM_WORKSPACE_KEY_MAX))

/* The bindings the session actually has, and how many. Filled at start-up
   from the script when it binds keys, and from the table above when it does
   not. */
static KeyBinding bindings[MAX_BINDINGS];
static int binding_count = 0;

/* Which of the WM's own actions a written action names. The script writes the
   action as a call — `gcl_spawn.RunProgram`, `gcl_window.Close`,
   `gcl_window.Switch` — and the config module keeps the call's own name. What
   tells the actions apart is the word after the last dot, compared whole:
   matching a substring would make `gcl_compositor.Reboot` fire reboot and a
   name that merely contains "close" fire close. The comparison is
   case-insensitive because a person writes Close and may write close. */
static KeyAction action_for(const char *written) {
    if (!written || !written[0]) {
        return NULL;
    }
    const char *dot = strrchr(written, '.');
    const char *leaf = dot ? dot + 1 : written;

    if (strcasecmp(leaf, "RunProgram") == 0 ||
        strcasecmp(leaf, "Spawn") == 0 ||
        strcasecmp(leaf, "OpenTerminal") == 0) {
        return action_spawn_terminal;
    }
    if (strcasecmp(leaf, "Close") == 0) {
        return action_close_focused;
    }
    if (strcasecmp(leaf, "Switch") == 0 ||
        strcasecmp(leaf, "NextWindow") == 0 ||
        strcasecmp(leaf, "FocusPrevious") == 0) {
        return action_switch_window;
    }
    if (strcasecmp(leaf, "Reload") == 0) {
        return action_reload_config;
    }
    /* The screen saver and the lock screen are two actions, not one, and the
       names are matched whole so a script can bind either: ScreenSaver (its
       usual spelling) starts GnuChanSS, LockScreen starts GnuChanSL. The
       spellings a person might write are all accepted; nothing matches on a
       substring, so `LockScreenSaver` would not slip into either. */
    if (strcasecmp(leaf, "ScreenSaver") == 0 ||
        strcasecmp(leaf, "Screensaver") == 0 ||
        strcasecmp(leaf, "ScreenSave") == 0) {
        return action_screen_saver;
    }
    if (strcasecmp(leaf, "LockScreen") == 0 ||
        strcasecmp(leaf, "Lockscreen") == 0 ||
        strcasecmp(leaf, "Lock") == 0) {
        return action_lock_screen;
    }
    return NULL;
}

/* Turn one written binding into one the grab can use: its key name into the
   keysym X keys the grab by, and its action into the function to call. A key
   name X does not know is reported and skipped, because a binding that cannot
   be resolved must not stop the rest of the table from being bound. */
static int add_config_binding(WmCore *core, const WmBinding *binding) {
    if (binding_count >= MAX_BINDINGS) {
        return -1;
    }
    if (!binding->key[0] || binding->modifiers == 0) {
        return -1;
    }

    KeySym keysym = XStringToKeysym(binding->key);
    if (keysym == NoSymbol && binding->key[0]) {
        char cap[WM_CONFIG_TEXT_LENGTH];
        snprintf(cap, sizeof(cap), "%s", binding->key);
        cap[0] = (char)toupper((unsigned char)cap[0]);
        keysym = XStringToKeysym(cap);
    }
    if (keysym == NoSymbol) {
        fprintf(stderr, "gnuchanwm: config: unknown key '%s'\n", binding->key);
        wm_config_show_message(core,
                               "GnuChanWM: a key binding cannot be used",
                               "this key name is not one X knows",
                               binding->key);
        return -1;
    }

    KeyAction action = action_for(binding->action);
    if (!action) {
        /* The action names something the runtime does that this window
           manager does not: reported once at start-up, so a person can see
           why the key does not answer. It is shown in a window as well as
           written to the log, because a key that silently does nothing is
           the one fault a person cannot tell from a key that was never
           bound. */
        fprintf(stderr, "gnuchanwm: config: action '%s' is not one this WM does\n",
                binding->action);
        wm_config_show_message(core,
                               "GnuChanWM: a key binding cannot be used",
                               "this action is not one the window manager does",
                               binding->action);
        return -1;
    }

    KeyBinding *entry = &bindings[binding_count];
    memset(entry, 0, sizeof(*entry));
    entry->modifiers = binding->modifiers;
    entry->keysym = keysym;
    entry->action = action;
    /* The command the action was given, if it named one. It is what makes
       `RunProgram(command=default_terminal)` open the terminal the script
       configured rather than whatever the session's default happens to be. */
    snprintf(entry->command, sizeof(entry->command), "%s", binding->command);
    binding_count++;
    return 0;
}

/* Add one workspace key: `workspace`'s number, under `modifiers`, doing
   `actions[workspace]`.
 *
 * A combination the script already bound is left alone. The script's table is
 * what a person wrote down, and a built-in convenience does not get to take a
 * key out from under it — what these two sets guarantee is that a desk is
 * always reachable and always somewhere a window can be sent, which is the
 * floor a session needs, not a ceiling on what it may bind.
 *
 * A keysym X has no key for ends the set rather than being skipped: the
 * numbers run in order from 1, so there is no number past the one it cannot
 * find that it would find either. */
static void add_workspace_binding(int workspace, unsigned int modifiers,
                                  const KeyAction *actions) {
    KeySym keysym;
    int taken = 0;

    if (binding_count >= MAX_BINDINGS) {
        return;
    }
    keysym = workspace_keysym(workspace);
    if (keysym == NoSymbol) {
        return;
    }

    for (int i = 0; i < binding_count; i++) {
        if (bindings[i].keysym == keysym &&
            bindings[i].modifiers == modifiers) {
            taken = 1;
            break;
        }
    }
    if (taken) {
        return;
    }

    bindings[binding_count].modifiers = modifiers;
    bindings[binding_count].keysym = keysym;
    bindings[binding_count].action = actions[workspace];
    bindings[binding_count].command[0] = '\0';
    binding_count++;
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
        add_config_binding(core, &core->config.bindings[i]);
    }
    if (binding_count == 0) {
        for (unsigned int i = 0; i < BUILT_IN_COUNT; i++) {
            bindings[binding_count++] = BUILT_IN[i];
        }
    } else {
        bindings[binding_count++] = BUILT_IN[BUILT_IN_COUNT - 1];
    }

    /* Super+1..N and Super+Shift+1..N, one pair per workspace the script asked
       for. They are added after the script's own bindings so a script that
       bound Super+1 to something else keeps it: the script's table is what a
       person wrote, and it wins over a built-in convenience. What this
       guarantees is that a desk is always reachable and always a place a
       window can be sent, which is the floor a session needs — and the count
       comes from the config, so a session with four numbers has four pairs and
       not six. */
    int workspace_count = wm_workspace_count(core);
    for (int i = 0; i < workspace_count && i < WM_WORKSPACE_KEY_MAX; i++) {
        add_workspace_binding(i, Mod4Mask, WORKSPACE_ACTIONS);
        add_workspace_binding(i, Mod4Mask | ShiftMask,
                              WORKSPACE_MOVE_ACTIONS);
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

/* The key currently held down, as the pair its grab was made with, so a key
   that is held and repeating is not acted on again.
 *
 * Holding a key makes the server send a stream of press AND release events —
 * auto-repeat is a press and a release, over and over — and without this the
 * action would fire on every one. That is what left twenty screen savers on
 * the screen: Alt+S held for a moment started one on each repeat. A binding
 * is therefore acted on once, on the press that is NOT an auto-repeat, and
 * not again until the key is let go. */
static KeySym held_keysym = NoSymbol;
static unsigned int held_modifiers = 0;

/* The binding a pressed key names, or NULL. The lock modifiers are masked off
   because the grab was taken with them included. The whole binding is returned
   rather than just its action, because the action is called with the command
   the binding was written with. */
static const KeyBinding *keys_lookup(WmCore *core, XKeyEvent *event) {
    KeySym keysym = XLookupKeysym(event, 0);
    unsigned int state = event->state & ~(LockMask | Mod2Mask);
    for (int i = 0; i < binding_count; i++) {
        if (bindings[i].keysym == keysym && bindings[i].modifiers == state) {
            return &bindings[i];
        }
    }
    (void)core;
    return NULL;
}

static void keys_event(WmCore *core, XEvent *event) {
    if (event->type == KeyRelease) {
        /* A real release lets the key be used again; the release that is half
           of an auto-repeat does not, which is what keeps a held key from
           firing repeatedly. Between the press and a release that follows it
           within a few milliseconds there is a KeyPress already queued behind
           it — that is how X reports a repeat — and that is the release to
           ignore. */
        if (XPending(core->display) > 0) {
            XEvent next;
            XPeekEvent(core->display, &next);
            if (next.type == KeyPress &&
                next.xkey.time == event->xkey.time) {
                return;   /* the release half of an auto-repeat */
            }
        }
        KeySym keysym = XLookupKeysym(&event->xkey, 0);
        if (keysym == held_keysym) {
            held_keysym = NoSymbol;
            held_modifiers = 0;
        }
        return;
    }

    if (event->type != KeyPress) {
        return;
    }

    const KeyBinding *binding = keys_lookup(core, &event->xkey);
    if (!binding || !binding->action) {
        return;
    }

    /* The first press acts; a repeat of the same held key does not. */
    KeySym keysym = binding->keysym;
    unsigned int state = event->xkey.state & ~(LockMask | Mod2Mask);
    if (keysym == held_keysym && state == held_modifiers) {
        return;
    }
    held_keysym = keysym;
    held_modifiers = state;
    binding->action(core, binding->command);
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
