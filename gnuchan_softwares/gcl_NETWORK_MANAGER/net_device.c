/*
 * net_device.c — read the interfaces from nmcli and ip.
 *
 * Two sources, joined by interface name. NetworkManager (through nmcli) knows
 * the managed state and the active profile; the kernel (through ip) knows every
 * interface that exists and its live address and flags. Reading only nmcli
 * misses a bridge made by hand; reading only ip misses which profile is on a
 * connection. So the kernel list is the spine and NetworkManager's answer is
 * layered over it, which is why a device nmcli never heard of still shows and a
 * managed one shows its connection name.
 */
#include "net_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The kind of a nmcli type word or a kernel link name. */
static NetDeviceKind kind_of_type(const char *type) {
    if (!type || !type[0]) {
        return NET_DEVICE_OTHER;
    }
    if (strcmp(type, "ethernet") == 0) {
        return NET_DEVICE_ETHERNET;
    }
    if (strcmp(type, "wifi") == 0 || strcmp(type, "wlan") == 0 ||
        strcmp(type, "wireless") == 0) {
        return NET_DEVICE_WIFI;
    }
    if (strcmp(type, "loopback") == 0 || strncmp(type, "lo", 2) == 0) {
        return NET_DEVICE_LOOPBACK;
    }
    return NET_DEVICE_OTHER;
}

/* Find an interface by name in the list, or NULL. */
static NetDevice *find_device(NetDeviceList *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

/* Append an interface if there is room, returning it. */
static NetDevice *add_device(NetDeviceList *list, const char *name) {
    if (!name || !name[0] || list->count >= NET_MAX_DEVICES) {
        return NULL;
    }
    NetDevice *device = &list->items[list->count];
    memset(device, 0, sizeof(*device));
    snprintf(device->name, sizeof(device->name), "%s", name);
    device->state = -1;
    device->up = 0;
    device->managed = 0;
    list->count++;
    return device;
}

/* --- the kernel: `ip -o link show` gives every interface that exists ---
 *
 * One line per interface, with the flags in a `<...>` group. The interface name
 * is the second colon-separated field: "2: eth0: <...>". The hardware address
 * is the "link/ether" field when the link has one. */
static void read_links(NetDeviceList *list, const NetConfig *config) {
    char command[NET_TEXT * 8];
    snprintf(command, sizeof(command), "%s -o link show", config->ip);

    static char output[NET_TEXT * NET_MAX_DEVICES * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        return;
    }

    char *line = output;
    while (line && *line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        /* "2: eth0: <BROADCAST,...> mtu 1500 state UP ... link/ether aa:...".
           The index is before the first colon; the name is between the first
           colon-space and the next colon. */
        char *first = strchr(line, ':');
        if (first) {
            char *name_start = first + 1;
            while (*name_start == ' ') {
                name_start++;
            }
            char *name_end = strchr(name_start, ':');
            if (name_end) {
                *name_end = '\0';

                NetDevice *device = find_device(list, name_start);
                if (!device) {
                    device = add_device(list, name_start);
                }
                if (device) {
                    device->kind = kind_of_type(name_start);
                    /* The flags are the `<...>` group right after the name,
                       and UP inside it means the interface is
                       administratively up. The group's own angle brackets
                       delimit the search, so the "state UP" that follows on
                       the same line cannot be mistaken for the flag. */
                    device->up = 0;
                    char *flags_open = strchr(name_end + 1, '<');
                    char *flags_close = flags_open
                                            ? strchr(flags_open, '>')
                                            : NULL;
                    if (flags_open && flags_close) {
                        for (char *p = flags_open; p + 1 < flags_close; p++) {
                            if (p[0] == 'U' && p[1] == 'P' &&
                                (p == flags_open + 1 || p[-1] == ',')) {
                                device->up = 1;
                                break;
                            }
                        }
                    }

                    /* The MAC, the field after "link/ether". */
                    char *ether = strstr(name_end + 1, "link/ether ");
                    if (ether) {
                        ether += 11;
                        char mac[NET_TEXT];
                        int at = 0;
                        while (*ether && *ether != ' ' && at < NET_TEXT - 1) {
                            mac[at++] = *ether++;
                        }
                        mac[at] = '\0';
                        snprintf(device->mac, sizeof(device->mac), "%s", mac);
                    }
                }
            }
        }

        line = newline ? newline + 1 : NULL;
    }
}

/* --- the kernel: `ip -o -4 addr show` gives the IPv4 address of each ---
 *
 * One line per address: "2: eth0    inet 192.168.1.5/24 brd ...". The interface
 * name is after the index, and the address follows "inet". Only the first
 * address of an interface is kept — that is the one a person reads. */
static void read_addresses(NetDeviceList *list, const NetConfig *config) {
    char command[NET_TEXT * 8];
    snprintf(command, sizeof(command), "%s -o -4 addr show", config->ip);

    static char output[NET_TEXT * NET_MAX_DEVICES * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        return;
    }

    char *line = output;
    while (line && *line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        char *inet = strstr(line, "inet ");
        if (inet) {
            /* The interface name is before "inet": "2: eth0    inet ...". */
            char *first = strchr(line, ':');
            if (first) {
                char *name_start = first + 1;
                while (*name_start == ' ') {
                    name_start++;
                }
                char *name_end = name_start;
                while (*name_end && *name_end != ' ') {
                    name_end++;
                }
                char saved = *name_end;
                *name_end = '\0';
                NetDevice *device = find_device(list, name_start);
                *name_end = saved;

                if (device && !device->address[0]) {
                    inet += 5;
                    char address[NET_TEXT];
                    int at = 0;
                    while (*inet && *inet != ' ' && *inet != '/' &&
                           at < NET_TEXT - 1) {
                        address[at++] = *inet++;
                    }
                    address[at] = '\0';
                    snprintf(device->address, sizeof(device->address), "%s",
                             address);
                }
            }
        }

        line = newline ? newline + 1 : NULL;
    }
}

/* --- NetworkManager: `nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device
 * status` gives the managed state ---
 *
 * One line per device: "eth0:ethernet:connected:Wired connection 1". A device
 * here that the kernel list already has has its state and connection filled
 * in; one the kernel list somehow missed is added. */
static void read_nmcli(NetDeviceList *list, const NetConfig *config) {
    char command[NET_TEXT * 8];
    snprintf(command, sizeof(command),
             "%s -t -f DEVICE,TYPE,STATE,CONNECTION device status",
             config->nmcli);

    static char output[NET_TEXT * NET_MAX_DEVICES * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        return;
    }

    char *line = output;
    while (line && *line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        char name[NET_TEXT];
        char type[NET_TEXT];
        char state[NET_TEXT];
        char connection[NET_TEXT];
        const char *rest = net_shell_split(line, name, sizeof(name));
        rest = net_shell_split(rest, type, sizeof(type));
        rest = net_shell_split(rest, state, sizeof(state));
        net_shell_split(rest, connection, sizeof(connection));

        if (name[0]) {
            NetDevice *device = find_device(list, name);
            if (!device) {
                device = add_device(list, name);
            }
            if (device) {
                snprintf(device->type, sizeof(device->type), "%s", type);
                device->kind = kind_of_type(type);
                device->managed = 1;
                device->state = (strcmp(state, "connected") == 0) ? 1 : 0;
                if (!device->connection[0] && connection[0] &&
                    strcmp(connection, "--") != 0) {
                    snprintf(device->connection, sizeof(device->connection),
                             "%s", connection);
                }
            }
        }

        line = newline ? newline + 1 : NULL;
    }
}

int net_device_list(NetDeviceList *list, const NetConfig *config) {
    if (!list) {
        return -1;
    }
    list->count = 0;

    read_links(list, config);
    read_addresses(list, config);

    /* NetworkManager is asked after the kernel list, so a device it manages is
       found already present and only gains its state; a device it knows and the
       kernel list does not is added. When neither tool ran the list is empty
       and that is reported by the count. */
    read_nmcli(list, config);

    return list->count > 0 ? 0 : -1;
}

void net_device_first_wifi(const NetDeviceList *list, char *out,
                           unsigned int size) {
    if (size) {
        out[0] = '\0';
    }
    if (!list) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].kind == NET_DEVICE_WIFI) {
            snprintf(out, size, "%s", list->items[i].name);
            return;
        }
    }
}

int net_device_set_up(const NetConfig *config, const char *name, int up,
                      char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }
    if (!name || !name[0]) {
        if (error && size) {
            snprintf(error, size, "no interface chosen");
        }
        return -1;
    }

    char quoted[NET_TEXT * 2];
    net_shell_quote(name, quoted, sizeof(quoted));

    char command[NET_TEXT * 8];
    snprintf(command, sizeof(command), "%s link set %s %s", config->ip,
             quoted, up ? "up" : "down");

    static char output[NET_TEXT * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "could not change the interface");
        }
        return -1;
    }
    return 0;
}
