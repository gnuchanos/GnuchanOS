/*
 * sl_auth.c — check a password against the system's accounts, through PAM.
 *
 * The shape is the same as gcl_DM/dm_auth.c: a conversation callback that hands
 * PAM the one password it is asked for, and a start/authenticate/account/end
 * sequence. The differences are the service name ("gnuchansl", not
 * "gnuchandm") and nothing else — the reasoning is in sl_auth.h.
 *
 * A wrong password is not an error here: it is the ordinary answer, returned as
 * 0, and the caller shows it as a wrong password rather than as a fault.
 */
#include <security/pam_appl.h>
#include <stdlib.h>
#include <string.h>

#include "sl_auth.h"

/* The password is handed to PAM through this, because the conversation callback
   has no other way to reach it. `used` makes sure the password is given out
   once: PAM may ask twice (once for the password, once for something else), and
   a lock screen has one secret.
 */
typedef struct AuthAnswers {
    const char *password;
    int used;
} AuthAnswers;

static int auth_conversation(int count, const struct pam_message **messages,
                             struct pam_response **responses, void *data) {
    AuthAnswers *answers = (AuthAnswers *)data;
    if (count <= 0 || !responses) {
        return PAM_CONV_ERR;
    }

    struct pam_response *reply = (struct pam_response *)calloc(
        (size_t)count, sizeof(struct pam_response));
    if (!reply) {
        return PAM_BUF_ERR;
    }

    for (int i = 0; i < count; i++) {
        if (messages[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            if (!answers->used) {
                reply[i].resp =
                    strdup(answers->password ? answers->password : "");
                answers->used = 1;
            }
        } else if (messages[i]->msg_style == PAM_PROMPT_ECHO_ON) {
            /* A prompt for something to be echoed — a user name — is answered
               empty: the user is already known, and this lock screen has no
               name field. */
            reply[i].resp = strdup("");
        }
    }

    *responses = reply;
    return PAM_SUCCESS;
}

int sl_auth_check(const char *username, const char *password) {
    if (!username || !username[0] || !password) {
        return 0;
    }

    AuthAnswers answers = { password, 0 };
    struct pam_conv conversation = { auth_conversation, &answers };

    pam_handle_t *pam = NULL;
    int status = pam_start("gnuchansl", username, &conversation, &pam);
    if (status != PAM_SUCCESS) {
        return 0;
    }

    status = pam_authenticate(pam, 0);
    if (status == PAM_SUCCESS) {
        /* The account check is what refuses an account that is locked, expired
           or outside its allowed hours — the same check a login makes, so a
           password cannot get past this screen into an account that would then
           be refused. */
        status = pam_acct_mgmt(pam, 0);
    }

    pam_end(pam, status);
    return status == PAM_SUCCESS ? 1 : 0;
}
