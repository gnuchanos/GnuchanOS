/*
 * dm_input.c — turn keys and clicks into changes of the form.
 *
 * The keyboard walks every control, not just the two fields: Tab (and the
 * arrow keys) move through username, password, sign in, the session selector,
 * reboot and shut down, and Enter or Space acts on whatever has the focus. On
 * a machine with no pointer that is the only way to reach the controls, so it
 * is the whole point of the module rather than a convenience.
 *
 * The pointer is the other half. A click moves the keyboard to whatever it
 * landed on and then acts on it — a click on a button presses it — and the
 * pointer moving over the dropped-down session list highlights the row under
 * it, which is what tells the user which one they are about to choose. A
 * session list that did not highlight would be a list of names with no way to
 * tell where the click will land.
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

static void session_toggle(DmCore *core) {
    if (core->session_count <= 0) {
        dm_form_set_message(core, "no sessions are installed", 1);
        return;
    }
    core->session_open = !core->session_open;
    core->session_hover = -1;
}

static void session_choose(DmCore *core, int index) {
    if (index < 0 || index >= core->session_count) {
        return;
    }
    core->session_selected = index;
    core->session_open = 0;
    core->session_hover = -1;
}

static void activate_signin(DmCore *core) {
    if (core->username_length == 0) {
        dm_form_set_message(core, "enter a user name", 1);
        core->focus = DM_FOCUS_USERNAME;
        return;
    }
    if (core->password_length == 0) {
        dm_form_set_message(core, "enter a password", 1);
        core->focus = DM_FOCUS_PASSWORD;
        return;
    }
    if (core->session_count <= 0) {
        dm_form_set_message(core, "no session is installed to start", 1);
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
    case DM_FOCUS_SESSION:
        session_toggle(core);
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
        /* Escape closes the session list first, then clears the form: a list
           the user opened with the keyboard is dismissed by the key that
           dismisses things, before that key throws away what they typed. */
        if (core->session_open) {
            core->session_open = 0;
            core->session_hover = -1;
            return;
        }
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
    /* The open session list is over everything else, so it is tested first: a
       click inside it chooses a row, and never falls through to the control
       that happens to be underneath. */
    int row = dm_login_session_row_at(core, event->x, event->y);
    if (row >= 0) {
        session_choose(core, row);
        return;
    }
    if (core->session_open) {
        /* A click anywhere else while the list is open only closes it: the
           list is a menu, and the first click outside a menu dismisses it. */
        core->session_open = 0;
        core->session_hover = -1;
        return;
    }

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
    case DM_BUTTON_SESSION:
        core->focus = DM_FOCUS_SESSION;
        session_toggle(core);
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

/* The pointer moving over the open list highlights the row under it. Only the
   highlight is kept — nothing is chosen until the click — because moving the
   pointer is not choosing, and a list that selected on hover would start a
   different session than the one the user clicked. */
static void pointer_motion(DmCore *core, XMotionEvent *event) {
    if (!core->session_open) {
        return;
    }
    int row = dm_login_session_row_at(core, event->x, event->y);
    if (row != core->session_hover) {
        core->session_hover = row;
        dm_core_redraw(core);
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
    case MotionNotify:
        pointer_motion(core, &event->xmotion);
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
