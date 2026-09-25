/*
 * dm_input.c — turn keys and clicks into changes of the form.
 *
 * The keyboard walks every control, not just the two fields: Tab (and the
 * arrow keys) move through username, password, sign in, reboot and shut down,
 * and Enter or Space acts on whatever has the focus. On a machine with no
 * pointer that is the only way to reach the buttons, so it is the whole point
 * of the module rather than a convenience.
 *
 * This module reads input and nothing else: it never draws and never starts
 * anything. When the user asks for something that outlives the event — a
 * login, a reboot — it records that in core->action and leaves the entry
 * point to carry it out once the loop is back at the top.
 */
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "dm_core.h"

/* The action the focus asks for, carried out at the top of the loop. */
static void request(DmCore *core, DmAction action) {
    core->action = action;
}

static void activate_signin(DmCore *core) {
    if (core->username_length == 0) {
        dm_form_set_message(core, "enter a user name", 1);
        core->focus = DM_FOCUS_USERNAME;
        return;
    }
    request(core, DM_ACTION_LOGIN);
}

/* Enter and Space do what the focused control means. On a field, Enter moves
   to the next control — pressing it after typing a name should not submit a
   blank password. */
static void activate(DmCore *core) {
    switch (core->focus) {
    case DM_FOCUS_USERNAME:
    case DM_FOCUS_PASSWORD:
        dm_form_focus_next(core);
        break;
    case DM_FOCUS_SIGNIN:
        activate_signin(core);
        break;
    case DM_FOCUS_REBOOT:
        request(core, DM_ACTION_REBOOT);
        break;
    case DM_FOCUS_SHUTDOWN:
        request(core, DM_ACTION_SHUTDOWN);
        break;
    default:
        break;
    }
}

static int focus_is_button(DmCore *core) {
    return !dm_focus_is_field(core->focus);
}

static void key_press(DmCore *core, XKeyEvent *event) {
    char buffer[32];
    KeySym keysym = NoSymbol;
    int length = XLookupString(event, buffer, (int)sizeof(buffer), &keysym, NULL);
    int shift = (event->state & ShiftMask) != 0;

    switch (keysym) {
    case XK_Return:
    case XK_KP_Enter:
        activate(core);
        return;

    /* Tab walks forward, Shift+Tab back. ISO_Left_Tab is what Shift+Tab
       arrives as on most layouts. */
    case XK_Tab:
    case XK_ISO_Left_Tab:
        if (shift) dm_form_focus_prev(core);
        else       dm_form_focus_next(core);
        return;

    /* The arrows are the same movement by another name: on a screen of
       vertical controls, down and Tab mean the same thing. */
    case XK_Down:
    case XK_Right:
        dm_form_focus_next(core);
        return;
    case XK_Up:
    case XK_Left:
        dm_form_focus_prev(core);
        return;

    case XK_BackSpace:
        dm_form_backspace(core);
        return;

    case XK_Escape:
        dm_form_clear_password(core);
        core->username[0] = '\0';
        core->username_length = 0;
        core->focus = DM_FOCUS_USERNAME;
        dm_form_set_message(core, NULL, 0);
        return;

    /* Space activates a button. On a field it is an ordinary character, so
       this case only claims the key when the focus is not a field. */
    case XK_space:
        if (focus_is_button(core)) {
            activate(core);
            return;
        }
        break;

    default:
        break;
    }

    if (length > 0 && !(event->state & ControlMask)) {
        dm_form_type(core, buffer[0]);
    }
}

/* A click on a control moves the keyboard there; a click on a button also
   presses it, because that is what clicking a button means. */
static void button_press(DmCore *core, XButtonEvent *event) {
    int field = dm_login_field_at(core, event->x, event->y);
    if (field >= 0) {
        core->focus = (DmFocus)field;
        return;
    }

    switch (dm_login_button_at(core, event->x, event->y)) {
    case DM_BUTTON_SIGNIN:
        core->focus = DM_FOCUS_SIGNIN;
        activate_signin(core);
        break;
    case DM_BUTTON_REBOOT:
        core->focus = DM_FOCUS_REBOOT;
        request(core, DM_ACTION_REBOOT);
        break;
    case DM_BUTTON_SHUTDOWN:
        core->focus = DM_FOCUS_SHUTDOWN;
        request(core, DM_ACTION_SHUTDOWN);
        break;
    case DM_BUTTON_NONE:
        break;
    }
}

static void input_event(DmCore *core, XEvent *event) {
    switch (event->type) {
    case KeyPress:
        key_press(core, &event->xkey);
        break;
    case ButtonPress:
        button_press(core, &event->xbutton);
        break;
    case FocusIn:
        XSetInputFocus(core->display, core->window, RevertToPointerRoot, CurrentTime);
        break;
    default:
        break;
    }
}

const DmModule dm_input_module = {
    .name = "input", .init = NULL, .event = input_event, .draw = NULL, .cleanup = NULL,
};
