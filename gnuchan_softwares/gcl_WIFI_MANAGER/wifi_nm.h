/*
 * wifi_nm.h — the NetworkManager side, through nmcli.
 *
 * This is the only file that knows how a wireless network is found and joined,
 * and it knows it as one thing: run nmcli and read what it says. There is no
 * libnm here and no D-Bus here, because nmcli is already installed on every
 * machine that has NetworkManager — which is every machine this desktop runs
 * on — and a wifi manager that needed a second library to be built would be
 * one that does not build on the laptop it is for.
 *
 * Everything is a plain function over plain data. No X, no window, nothing to
 * draw: wifi_ui.c calls these and shows the answer.
 *
 *     wifi_nm_set_program("nmcli")   which nmcli to run (from the config)
 *     wifi_nm_device(name)           the wifi interface, e.g. "wlan0"
 *     wifi_nm_scan(&list)            every network in range, right now
 *     wifi_nm_connect(dev, ssid, ...) join one, with a password if given
 *     wifi_nm_disconnect(dev)        leave the one that is joined
 *     wifi_nm_rescan(dev)            ask for a fresh scan (best effort)
 */
#ifndef GNUCHANWIFI_NM_H
#define GNUCHANWIFI_NM_H

/* The most networks one scan keeps, and the longest text any one field may
   hold. Both are ceilings on what a malformed or hostile answer can make this
   allocate; a screen does not show sixty-four networks and no SSID is 256
   characters long. */
#define WIFI_MAX_NETWORKS 64
#define WIFI_TEXT 256

/* One network as nmcli reported it. Plain data, filled by wifi_nm_scan(). */
typedef struct WifiNetwork {
    char ssid[WIFI_TEXT];   /* the name a person reads                 */
    int signal;             /* 0..100, the strength nmcli gave         */
    int secured;            /* 1 when joining needs a password         */
    int in_use;             /* 1 when this is the joined network       */
} WifiNetwork;

typedef struct WifiList {
    WifiNetwork items[WIFI_MAX_NETWORKS];
    int count;
} WifiList;

/* Which nmcli to run. The config sets this; the default is "nmcli", found on
   PATH. Kept here so a machine whose nmcli is somewhere unusual can say so. */
void wifi_nm_set_program(const char *program);

/* The wireless interface's name, written into `out` — "wlan0" and the like —
   or an empty string when the machine has no wireless interface at all. This
   is asked once when the window opens and kept: every connect, disconnect and
   rescan names it. */
void wifi_nm_device(char *out, unsigned int size);

/* Fill `list` from a scan. Returns 0 on success, -1 when nmcli could not be
   run; a scan that ran but found nothing is a success with count 0. */
int wifi_nm_scan(WifiList *list);

/* Join `ssid` on `device`. A NULL or empty `password` connects without one,
   which is what a saved network needs. Returns 0 on success; on failure -1 and
   a short message — nmcli's own words where it gave any — in `error`. */
int wifi_nm_connect(const char *device, const char *ssid,
                    const char *password, char *error, unsigned int size);

/* Leave whatever `device` is joined to. Same return convention as connect. */
int wifi_nm_disconnect(const char *device, char *error, unsigned int size);

/* Ask NetworkManager for a fresh scan. Best effort: a rescan that is refused
   because one is already running is not an error worth reporting, so this
   returns nothing. */
void wifi_nm_rescan(const char *device);

#endif /* GNUCHANWIFI_NM_H */
