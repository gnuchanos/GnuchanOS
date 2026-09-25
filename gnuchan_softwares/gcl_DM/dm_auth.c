/*
 * dm_auth.c — check a user name and a password against the system's accounts.
 *
 * PAM is used, not /etc/shadow, because PAM is what the rest of Debian uses: a
 * password that works with `su` works here, an account locked by `passwd -l` is
 * locked here, and a PAM module added later needs no change to this file. The
 * service name is "gnuchandm", so the policy is the system's until someone
 * writes /etc/pam.d/gnuchandm.
 */
#include <security/pam_appl.h>
#include <stdlib.h>
#include <string.h>

#include "dm_core.h"

typedef struct AuthAnswers {
    const char *password;
    int used;
} AuthAnswers;

static int auth_conversation(int count, const struct pam_message **messages,
                             struct pam_response **responses, void *data) {
    AuthAnswers *answers = (AuthAnswers *)data;
    if (count <= 0 || !responses) return PAM_CONV_ERR;

    struct pam_response *reply =
        (struct pam_response *)calloc((size_t)count, sizeof(struct pam_response));
    if (!reply) return PAM_BUF_ERR;

    for (int i = 0; i < count; i++) {
        if (messages[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            if (!answers->used) {
                reply[i].resp = strdup(answers->password ? answers->password : "");
                answers->used = 1;
            }
        } else if (messages[i]->msg_style == PAM_PROMPT_ECHO_ON) {
            reply[i].resp = strdup("");
        }
    }

    *responses = reply;
    return PAM_SUCCESS;
}

int dm_auth_check(const char *username, const char *password) {
    if (!username || !username[0] || !password) return 0;

    AuthAnswers answers = { password, 0 };
    struct pam_conv conversation = { auth_conversation, &answers };

    pam_handle_t *pam = NULL;
    int status = pam_start("gnuchandm", username, &conversation, &pam);
    if (status != PAM_SUCCESS) return 0;

    status = pam_authenticate(pam, 0);
    if (status == PAM_SUCCESS) status = pam_acct_mgmt(pam, 0);

    pam_end(pam, status);
    return status == PAM_SUCCESS ? 1 : 0;
}
