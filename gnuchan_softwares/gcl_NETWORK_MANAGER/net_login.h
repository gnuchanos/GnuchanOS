/*
 * net_login.h — the administrator password, asked for in a window.
 *
 * Changing the DNS servers and bringing an interface up or down are root's to
 * do, so the manager has to become root — and asking for the password is the
 * one part of that a person sees. It is a WINDOW and not a line of text,
 * because the manager is a window: a program that opens a window and then stops
 * at a prompt nobody can see looks like a program that opened nothing.
 *
 * The dialog is drawn in the manager's own palette and font — it is handed the
 * same config — so the one thing the person sees before the manager opens looks
 * like the manager.
 */
#ifndef GNUCHANNET_LOGIN_H
#define GNUCHANNET_LOGIN_H

#include "net_config.h"

/* The most a password typed here may be. */
#define NET_LOGIN_MAX 256

/* Show the password window and wait. On success the typed password is copied
   into `password` (which is `size` bytes) and 0 is returned; on cancel, on a
   window that cannot be made, or on no display, -1 is returned and `password`
   is left empty.

   The password may be empty and that is not a cancel: a machine whose sudo asks
   for nothing gets through with an empty line. */
int net_login_prompt(const NetConfig *config, char *password,
                     unsigned int size);

#endif /* GNUCHANNET_LOGIN_H */
