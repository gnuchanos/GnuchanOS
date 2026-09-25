/*
 * dm_input.c — turn keys and clicks into changes of the form.
 *
 * This module reads the keyboard and the pointer and nothing else: it never
 * draws and never starts anything. When the user asks for something that
 * outlives the event — a login, a reboot — it records that in core->action and
 * leaves the entry point to carry it out once the loop is back at the top.
 */
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "dm_core.h"

static void key_return(DmCore *core) {
    if (core->focused == DM_FIELD_USERNAME && core->password_length == 0) {
        dm_form_next_field(core);
        return;
    }
    if (core->username_length == 0) {
        dm_form_set_message(core, "enter a user name", 1);
        return;
    }
    core->action = DM_ACTION_LOGIN;
}

static void key_press(DmCore *core, XKeyEvent *event) {
    char buffer[32];
    KeySym keysym = NoSymbol;
    int length = XLookupString(event, buffer, (int)sizeof(buffer), &keysym, NULL);

    switch (keysym) {
    case XK_Return:
    case XK_KP_Enter:
        key_return(core);
        return;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        dm_form_next_field(core);
        return;
    case XK_BackSpace:
        dm_form_backspace(core);
        return;
    case XK_Escape:
        dm_form_clear_password(core);
        core->username[0] = '\0';
        core->username_length = 0;
        core->focused = DM_FIELD_USERNAME;
        dm_form_set_message(core, NULL, 0);
        return;
    default:
        break;
    }

    if (length > 0 && !(event->state & ControlMask)) {
        dm_form_type(core, buffer[0]);
    }
}

static void button_press(DmCore *core, XButtonEvent *event) {
    int field = dm_login_field_at(core, event->x, event->y);
    if (field >= 0) {
        core->focused = (DmField)field;
        return;
    }

    switch (dm_login_button_at(core, event->x, event->y)) {
    case DM_BUTTON_SIGNIN:
        if (core->username_length == 0) {
            dm_form_set_message(core, "enter a user name", 1);
            break;
        }
        core->action = DM_ACTION_LOGIN;
        break;
    case DM_BUTTON_REBOOT:
        core->action = DM_ACTION_REBOOT;
        break;
    case DM_BUTTON_SHUTDOWN:
        core->action = DM_ACTION_SHUTDOWN;
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
