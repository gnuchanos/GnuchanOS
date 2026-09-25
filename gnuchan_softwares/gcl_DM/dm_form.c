/*
 * dm_form.c — the login form's state: what was typed, where the keyboard is.
 */
#include <stdio.h>
#include <string.h>

#include "dm_core.h"

/* A focus target is a text field when it is one of the first two controls.
   The DmFocus values were chosen to make this the whole test. */
int dm_focus_is_field(DmFocus focus) {
    return focus == DM_FOCUS_USERNAME || focus == DM_FOCUS_PASSWORD;
}

static void clear_message(DmCore *core) {
    core->message[0] = '\0';
    core->message_is_error = 0;
}

void dm_form_type(DmCore *core, char c) {
    if ((unsigned char)c < 0x20 || (unsigned char)c == 0x7f) return;

    if (core->focus == DM_FOCUS_USERNAME) {
        if (core->username_length + 1 >= DM_MAX_USERNAME) return;
        core->username[core->username_length++] = c;
        core->username[core->username_length] = '\0';
    } else if (core->focus == DM_FOCUS_PASSWORD) {
        if (core->password_length + 1 >= DM_MAX_PASSWORD) return;
        core->password[core->password_length++] = c;
        core->password[core->password_length] = '\0';
    } else {
        /* The keyboard is on a button: a letter is not meant for the form. */
        return;
    }
    clear_message(core);
}

void dm_form_backspace(DmCore *core) {
    if (core->focus == DM_FOCUS_USERNAME) {
        if (core->username_length > 0) core->username[--core->username_length] = '\0';
    } else if (core->focus == DM_FOCUS_PASSWORD) {
        if (core->password_length > 0) core->password[--core->password_length] = '\0';
    } else {
        return;
    }
    clear_message(core);
}

DmFocus dm_form_focus_next(DmCore *core) {
    core->focus = (DmFocus)((core->focus + 1) % DM_FOCUS_COUNT);
    return core->focus;
}

DmFocus dm_form_focus_prev(DmCore *core) {
    core->focus = (DmFocus)((core->focus + DM_FOCUS_COUNT - 1) % DM_FOCUS_COUNT);
    return core->focus;
}

void dm_form_set_message(DmCore *core, const char *message, int is_error) {
    if (!message) {
        clear_message(core);
        return;
    }
    snprintf(core->message, sizeof(core->message), "%s", message);
    core->message_is_error = is_error;
}

void dm_form_clear_password(DmCore *core) {
    memset(core->password, 0, sizeof(core->password));
    core->password_length = 0;
}
