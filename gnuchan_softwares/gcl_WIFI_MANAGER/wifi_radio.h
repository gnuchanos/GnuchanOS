/*
 * wifi_radio.h — the wifi switch itself: on or off.
 *
 * This is the one control that is not about a network but about the radio: a
 * person flying, or saving battery, turns wifi off and no scan finds anything.
 * NetworkManager keeps that state and nmcli reads and sets it, so this is two
 * calls and a query, and nothing more.
 */
#ifndef GNUCHANWIFI_RADIO_H
#define GNUCHANWIFI_RADIO_H

#include "wifi_shell.h"   /* WIFI_TEXT — the size of the messages here */

/* Whether the wifi radio is on. Returns 1 when it is, 0 when it is off, and -1
   when the state could not be read (no nmcli, no NetworkManager). A caller that
   gets -1 shows nothing rather than guessing, because a switch drawn in the
   wrong position is worse than no switch. */
int wifi_radio_on(void);

/* Turn the wifi radio on or off. Returns 0 on success; on failure -1 and a
   short message — nmcli's own words where it gave any — in `error`. */
int wifi_radio_set(int on, char *error, unsigned int size);

#endif /* GNUCHANWIFI_RADIO_H */
