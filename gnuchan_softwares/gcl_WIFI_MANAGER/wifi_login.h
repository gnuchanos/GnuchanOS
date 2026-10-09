/*
 * wifi_login.h — the administrator password, asked for in a window.
 *
 * This manager changes the radio and reloads the wireless driver, both of which
 * only root may do, so it has to become root — and asking for the password is
 * the one part of that a person sees. It is a WINDOW and not a line of text,
 * because the manager is a window: a program that opens a window and then stops
 * at a password prompt in some terminal it is not attached to is a program that
 * appears to do nothing, which is exactly what "where is the prompt" means.
 *
 * The dialog is drawn in the manager's own palette and font — it is handed the
 * same config — so the one thing the person sees before the manager opens looks
 * like the manager.
 */
#ifndef GNUCHANWIFI_LOGIN_H
#define GNUCHANWIFI_LOGIN_H

#include "wifi_config.h"

/* The most a password typed here may be. The same ceiling the manager's own
   password line uses; nothing about a key is longer. */
#define WIFI_LOGIN_MAX 256

/* Show the password window and wait. On success the typed password is copied
   into `password` (which is `size` bytes) and 0 is returned; on cancel, on a
   window that cannot be made, or on no display, -1 is returned and `password`
   is left empty.
 *
 * The password may be empty and that is not a cancel: a machine whose sudo is
 * set to ask for nothing gets through with an empty line, and treating the
 * empty answer as "no" would make this manager refuse to run there. */
int wifi_login_prompt(const WifiConfig *config, char *password,
                      unsigned int size);

#endif /* GNUCHANWIFI_LOGIN_H */
