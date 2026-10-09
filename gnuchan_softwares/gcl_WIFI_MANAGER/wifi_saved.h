/*
 * wifi_saved.h — the profiles NetworkManager remembers.
 *
 * A saved connection is one the machine has joined before and kept the details
 * of: the password, and whether to join it automatically. Those are the two
 * things a person changes about a network they already know, and the two things
 * they change are "forget it" and "do not join it by itself any more".
 *
 * This file reads the saved profiles and answers those two questions. It is
 * separate from wifi_nm.c because the questions are about PROFILES rather than
 * about the air: a network in range is not a profile and a profile need not be
 * in range.
 */
#ifndef GNUCHANWIFI_SAVED_H
#define GNUCHANWIFI_SAVED_H

#include "wifi_shell.h"

/* The most profiles kept. Far more than a laptop has, so the ceiling is a bound
   on memory and not on the list a person sees. */
#define WIFI_MAX_SAVED 64

typedef struct WifiSavedList {
    /* The connection NAMES, which for a simple wifi profile is the SSID. A
       name is what every nmcli command that changes a profile is given. */
    char names[WIFI_MAX_SAVED][WIFI_TEXT];
    /* Whether each is set to join by itself, told apart from the name so the
       window can show it without a second call. */
    int autoconnect[WIFI_MAX_SAVED];
    int count;
} WifiSavedList;

/* Fill `list` from the saved profiles. Returns 0 on success, -1 when nmcli
   could not be run; a call that ran but found nothing is a success with
   count 0. */
int wifi_saved_load(WifiSavedList *list);

/* The names alone, in the shape wifi_nm_scan() wants them: `out` is an array of
   WIFI_TEXT-sized names, and the return is how many were written. This is what
   lets a scan mark a network as saved without wifi_nm.c knowing what a profile
   is. */
int wifi_saved_names(const WifiSavedList *list, char (*out)[WIFI_TEXT],
                     int max);

/* Forget a saved profile: the password and the details go with it. Returns 0 on
   success; on failure -1 and a short message in `error`. */
int wifi_saved_forget(const char *name, char *error, unsigned int size);

/* Turn a profile's "join by itself" on or off. Returns 0 on success; on failure
   -1 and a short message in `error`. */
int wifi_saved_set_autoconnect(const char *name, int on,
                               char *error, unsigned int size);

#endif /* GNUCHANWIFI_SAVED_H */
