/*
 * dm_form.c — the login form's state: what was typed, where the keyboard is.
 */
#include <stdio.h>
#include <string.h>

#include "dm_core.h"

void dm_form_type(DmCore *core, char c) {
    if ((unsigned char)c < 0x20 || (unsigned char)c == 0x7f) return;
    if (core->focused == DM_FIELD_USERNAME) {
        if (core->username_length + 1 >= DM_MAX_USERNAME) return;
        core->username[core->username_length++] = c;
        core->username[core->username_length] = '\0';
    } else {
        if (core->password_length + 1 >= DM_MAX_PASSWORD) return;
        core->password[core->password_length++] = c;
        core->password[core->password_length] = '\0';
    }
    core->message[0] = '\0';
    core->message_is_error = 0;
}

void dm_form_backspace(DmCore *core) {
    if (core->focused == DM_FIELD_USERNAME) {
        if (core->username_length > 0) core->username[--core->username_length] = '\0';
    } else {
        if (core->password_length > 0) core->password[--core->password_length] = '\0';
    }
    core->message[0] = '\0';
    core->message_is_error = 0;
}

void dm_form_next_field(DmCore *core) {
    core->focused = (DmField)((core->focused + 1) % DM_FIELD_COUNT);
}

void dm_form_set_message(DmCore *core, const char *message, int is_error) {
    if (!message) {
        core->message[0] = '\0';
        core->message_is_error = 0;
        return;
    }
    snprintf(core->message, sizeof(core->message), "%s", message);
    core->message_is_error = is_error;
}

void dm_form_clear_password(DmCore *core) {
    memset(core->password, 0, sizeof(core->password));
    core->password_length = 0;
}
