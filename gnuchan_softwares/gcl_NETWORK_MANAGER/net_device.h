/*
 * net_device.h — the network interfaces this machine has, and their state.
 *
 * This is the general view the wifi manager does not give: every interface the
 * kernel has — wired, wireless, virtual, loopback — with its state, its address
 * and whether it is up. The wifi manager sees the wireless one; this sees them
 * all, which is what makes it a network manager rather than a wifi one.
 *
 * The interfaces come from nmcli's device list, which is NetworkManager's own
 * view and the one that matters on a managed machine. A device nmcli does not
 * know about but the kernel does — a bridge someone made by hand — is picked up
 * from `ip -o link` as well, so the list is the kernel's interfaces with
 * NetworkManager's state layered on where it has any. The address of each is
 * read from `ip -o addr`, which is the live kernel answer.
 */
#ifndef GNUCHANNET_DEVICE_H
#define GNUCHANNET_DEVICE_H

#include "net_config.h"

/* The most interfaces one read keeps. A machine with more than this has a list
   longer than any window shows, and the ceiling is a bound on memory. */
#define NET_MAX_DEVICES 32

/* What kind of interface it is, which decides the icon and whether the wifi
   manager can be pointed at it. */
typedef enum NetDeviceKind {
    NET_DEVICE_OTHER = 0,
    NET_DEVICE_ETHERNET,
    NET_DEVICE_WIFI,
    NET_DEVICE_LOOPBACK,
} NetDeviceKind;

/* One interface. Plain data, filled by net_device_list(). */
typedef struct NetDevice {
    char name[NET_TEXT];        /* "eth0", "wlan0", "lo"                  */
    char type[NET_TEXT];        /* nmcli's own type word: "ethernet", ... */
    NetDeviceKind kind;
    int state;                  /* 1 connected, 0 disconnected, -1 unknown */
    char connection[NET_TEXT];  /* the active profile's name, or empty     */
    char address[NET_TEXT];     /* the IPv4 address, or empty              */
    char mac[NET_TEXT];         /* the hardware address, or empty          */
    int up;                     /* 1 when the kernel reports it UP         */
    int managed;                /* 1 when NetworkManager manages it        */
} NetDevice;

typedef struct NetDeviceList {
    NetDevice items[NET_MAX_DEVICES];
    int count;
} NetDeviceList;

/* Fill `list` with every interface, NetworkManager's view plus the kernel's.
   `config` names the tools to run. Returns 0 on success, -1 when neither nmcli
   nor ip could be run — in which case count is 0. */
int net_device_list(NetDeviceList *list, const NetConfig *config);

/* The first wireless interface, written into `out` (empty when there is none).
   This is what the "Open Wi-Fi" button checks before it launches the wifi
   manager, so a machine with no wireless adapter does not open a window that
   can only say "no wireless". */
void net_device_first_wifi(const NetDeviceList *list, char *out,
                           unsigned int size);

/* Bring an interface up or down: `ip link set <name> up|down`. Returns 0 on
   success; on failure -1 and a short message in `error`. */
int net_device_set_up(const NetConfig *config, const char *name, int up,
                      char *error, unsigned int size);

#endif /* GNUCHANNET_DEVICE_H */
