/*
 * wifi_nm.h — the networks: scanning, joining, and what is joined now.
 *
 * This is the NetworkManager side of the list — the networks in range, joining
 * one, leaving one, and the answer to "what am I on and what is my address".
 * nmcli does the work; see wifi_shell.c for the running and the parsing. The
 * radio (the wifi switch itself) is wifi_radio.c and the saved connections are
 * wifi_saved.c: three questions, three files.
 */
#ifndef GNUCHANWIFI_NM_H
#define GNUCHANWIFI_NM_H

#include "wifi_shell.h"

/* The most networks one scan keeps. Far more than a screen shows, so the ceiling
   is a bound on memory and not on the list a person sees. */
#define WIFI_MAX_NETWORKS 64

/* One network as nmcli reported it. Plain data, filled by wifi_nm_scan(). */
typedef struct WifiNetwork {
    char ssid[WIFI_TEXT];   /* the name a person reads                 */
    int signal;             /* 0..100, the strength nmcli gave         */
    int secured;            /* 1 when joining needs a password         */
    int in_use;             /* 1 when this is the joined network       */
    int saved;              /* 1 when a saved connection is for it     */
} WifiNetwork;

typedef struct WifiList {
    WifiNetwork items[WIFI_MAX_NETWORKS];
    int count;
} WifiList;

/* The wireless interface's name, written into `out` — "wlan0" and the like —
   or an empty string when the machine has no wireless interface at all. Asked
   once when the window opens and kept: every connect, disconnect and rescan
   names it. */
void wifi_nm_device(char *out, unsigned int size);

/* Fill `list` from a scan. Returns 0 on success, -1 when nmcli could not be
   run; a scan that ran but found nothing is a success with count 0. `saved`
   is the list of saved profiles — wifi_saved.c fills it — and a network whose
   SSID is in it is marked as saved, so the window can tell "I know this one"
   from "I would have to ask for a password".

   `device` is the wireless interface the scan is asked of. The scan is
   requested and WAITED FOR before the list is read — one command that both
   asked and read would print NetworkManager's cached list, because the scan it
   had just asked for would not have finished. That is what kept a switched-off
   network on the list however often Rescan was pressed. */
int wifi_nm_scan(WifiList *list, const char *device,
                 const char (*saved_ssids)[WIFI_TEXT], int saved_count);

/* Join `ssid`. A NULL or empty `password` connects without one, which is what a
   saved network needs. Returns 0 on success; on failure -1 and a short message
   — nmcli's own words where it gave any — in `error`. */
int wifi_nm_connect(const char *device, const char *ssid,
                    const char *password, char *error, unsigned int size);

/* Leave whatever `device` is joined to. Same return convention as connect. */
int wifi_nm_disconnect(const char *device, char *error, unsigned int size);

/* Ask NetworkManager for a fresh scan. Best effort: a rescan that is refused
   because one is already running is not an error worth reporting. */
void wifi_nm_rescan(const char *device);

/* What is joined right now: the SSID written into `ssid` (empty when nothing is
   connected) and the interface's IPv4 address into `address` (empty when there
   is none). Both are what the top of the window shows so a person can see the
   state without opening another tool. Returns 0 when something is connected. */
int wifi_nm_active(const char *device, char *ssid, unsigned int ssid_size,
                   char *address, unsigned int address_size);

#endif /* GNUCHANWIFI_NM_H */
