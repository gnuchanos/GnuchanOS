/*
 * wifi_rfkill.h — the kernel's radio kill switches, and the driver reload.
 *
 * rfkill is the kernel's own layer for "the radio is switched off": a laptop's
 * physical switch (fn+F11 on the Vostro) sets a HARD block, and software can set
 * a SOFT one. When either is set, no scan finds anything and the whole window is
 * empty — which is the one state a person cannot see the cause of without being
 * told.
 *
 * This file reads both kinds, clears them, and — the piece a stuck Atheros card
 * needs — reloads the wireless driver. On the Vostro the driver reads the
 * hardware switch line, decides the radio is off, and only a reload with the
 * switch cleared brings it back; that is exactly what Reboot_Wifi.py does by
 * hand, and this is the same three commands behind a key.
 */
#ifndef GNUCHANWIFI_RFKILL_H
#define GNUCHANWIFI_RFKILL_H

#include "wifi_shell.h"

/* Whether the wireless radio is blocked by the KERNEL, and how.
 *
 * A hard block is a physical switch and cannot be cleared by software alone; a
 * soft block can. Both are reported so the window can say which one it is,
 * because the two are different things to a person: one is "flip the switch",
 * the other is "press w". */
typedef struct WifiBlock {
    int hard;   /* 1 when any wlan device has a hardware block */
    int soft;   /* 1 when any wlan device has a software block */
} WifiBlock;

/* Read the blocks on the wlan devices. Returns 0 on success (even when nothing
   is blocked), -1 when /sys/class/rfkill could not be read at all. */
int wifi_rfkill_state(WifiBlock *block);

/* Clear every soft block (and try the hard ones, which the kernel will refuse
   unless the switch is physically on). Runs `rfkill unblock all`. Returns 0 on
   success; on failure -1 and a short message in `error`. */
int wifi_rfkill_unblock(char *error, unsigned int size);

/* The kernel module of the wireless interface `device` — the driver's own name,
   read from /sys/class/net/<device>/device/driver. Written into `out` (empty
   when it cannot be read) and returned. This is what a reload has to be told the
   name of; asking the kernel is right where a config value would be a guess. */
void wifi_rfkill_driver(const char *device, char *out, unsigned int size);

/* The whole of Reboot_Wifi.py, behind one call: clear the blocks, take the
   driver out, and put it back. `module` is the driver name (from
   wifi_rfkill_driver, or a config fallback); an empty one skips the reload and
   only unblocks. Returns 0 on success; on failure -1 and a short message. The
   network drops for a second while the driver is out, which is expected and is
   why this is a deliberate key press and not something done at start-up. */
int wifi_rfkill_restart(const char *module, char *error, unsigned int size);

#endif /* GNUCHANWIFI_RFKILL_H */
