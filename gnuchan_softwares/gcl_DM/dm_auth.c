/*
 * dm_auth.c — check a user name and a password, and open the session.
 *
 * PAM is used, not /etc/shadow, because PAM is what the rest of Debian uses: a
 * password that works with `su` works here, an account locked by `passwd -l` is
 * locked here, and a PAM module added later needs no change to this file. The
 * service name is "gnuchandm", so the policy is the system's until someone
 * writes /etc/pam.d/gnuchandm.
 *
 * WHY pam_open_session IS CALLED, AND NOT ONLY pam_authenticate.
 *
 * Checking a password and opening a session are two different jobs, and doing
 * only the first is a login that works and a desktop that is crippled.
 * pam_open_session is the call on which pam_systemd runs, and pam_systemd is
 * what registers the login with systemd-logind. logind is in turn what creates
 * the user's runtime directory, /run/user/$UID, and starts the session D-Bus
 * on it. A greeter that stops at pam_authenticate logs the user in and leaves
 * that directory absent, so the environment dm_session.c builds is missing
 * XDG_RUNTIME_DIR and DBUS_SESSION_BUS_ADDRESS: the desktop starts with no
 * session bus, the notification server holds its name on no bus, and every
 * daemon that wants a bus either gives up or spawns a private, invisible one
 * nothing else can reach.
 *
 * The handle is therefore held for the whole session rather than closed when
 * the password checks out. pam_end is what tears the bus and the runtime
 * directory back down, so it is called from dm_auth_end() once the session the
 * user was in has ended — not here, at login. dm_session.c looks the two paths
 * up after this returns, which is why the order matters: the session must be
 * OPEN before the environment is built.
 */
#include <security/pam_appl.h>
#include <stdio.h>
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

int dm_auth_check(DmCore *core) {
    if (!core || !core->username[0] || !core->password[0]) return 0;

    /* A previous login that was never ended would leak its handle; there is at
     * most one login at a time here, so any leftover is closed first. */
    if (core->pam) {
        pam_end(core->pam, PAM_SUCCESS);
        core->pam = NULL;
    }

    AuthAnswers answers = { core->password, 0 };
    struct pam_conv conversation = { auth_conversation, &answers };

    pam_handle_t *pam = NULL;
    int status = pam_start("gnuchandm", core->username, &conversation, &pam);
    if (status != PAM_SUCCESS) return 0;

    status = pam_authenticate(pam, 0);
    if (status == PAM_SUCCESS) {
        status = pam_acct_mgmt(pam, 0);
    }

    if (status != PAM_SUCCESS) {
        pam_end(pam, status);
        return 0;
    }

    /* The session, not just the password. pam_systemd runs here and is what
     * creates /run/user/$UID and the session bus. A failure is not fatal to the
     * login — the user can still be let in on a machine with no systemd — but
     * it is the difference between a desktop with a bus and one without, so it
     * is reported and the login still proceeds. */
    status = pam_open_session(pam, 0);
    if (status != PAM_SUCCESS) {
        fprintf(stderr,
                "gnuchandm: pam_open_session did not succeed (status %d); the "
                "session may start without a runtime directory or a bus\n",
                status);
    }

    core->pam = pam;
    return 1;
}

void dm_auth_end(DmCore *core) {
    if (!core || !core->pam) return;
    pam_close_session(core->pam, 0);
    pam_end(core->pam, PAM_SUCCESS);
    core->pam = NULL;
}
