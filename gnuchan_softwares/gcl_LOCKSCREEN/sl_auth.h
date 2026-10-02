/*
 * sl_auth.h — check a password against the system's accounts.
 *
 * PAM is used, not /etc/shadow, for the same reason gcl_DM/dm_auth.c uses it:
 * PAM is what the rest of Debian uses, so a password that works with `su`
 * works here, an account locked by `passwd -l` is locked here, and a PAM module
 * added later needs no change to this file.
 *
 * The service name is "gnuchansl", kept apart from the display manager's
 * "gnuchandm" on purpose: the two are different programs and a policy written
 * for one must not silently become the policy for the other. Until someone
 * writes /etc/pam.d/gnuchansl the system's default applies, which is what a
 * lock screen wants.
 */
#ifndef GNUCHANSL_AUTH_H
#define GNUCHANSL_AUTH_H

/* Check a password for the named user against the system's accounts. Returns 1
   when the password is right and the account is allowed to log in, 0 when it is
   not. An empty user name or password is never right, so it is 0 without asking
   PAM anything. */
int sl_auth_check(const char *username, const char *password);

#endif /* GNUCHANSL_AUTH_H */
