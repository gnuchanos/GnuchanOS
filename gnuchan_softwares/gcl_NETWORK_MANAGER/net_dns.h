/*
 * net_dns.h — reading and setting the DNS servers.
 *
 * This is the one thing a network manager can change that the wifi manager
 * never touches: which resolver a machine asks. There are two places that
 * answer, and this file prefers the modern one:
 *
 *   systemd-resolved, through `resolvectl`. Per-link, kept across a reconnect,
 *   and the thing a modern Debian actually asks. `resolvectl dns <link> <...>`
 *   sets it for one interface.
 *
 *   /etc/resolv.conf, the classic file. Read as a fallback when resolvectl is
 *   not there, and the nameservers it lists are what the machine is using.
 *
 * A set is applied through resolvectl when it exists — so the change is live
 * and per-link — and otherwise written to /etc/resolv.conf. Writing that file
 * needs root, which is why this program asks for it the way the wifi manager
 * does; a machine with no resolvectl and no root can still READ the servers,
 * which is what the display half of this needs.
 */
#ifndef GNUCHANNET_DNS_H
#define GNUCHANNET_DNS_H

#include "net_config.h"

/* The most nameservers kept. Four covers every real configuration. */
#define NET_MAX_DNS 4

typedef struct NetDnsList {
    char servers[NET_MAX_DNS][NET_TEXT];
    int count;
    /* Which source answered, for the line under the field: "resolvectl" or
       "resolv.conf". Empty when neither could be read. */
    char source[NET_TEXT];
} NetDnsList;

/* Fill `list`. When `link` names an interface and resolvectl is present, that
 * link's own servers are read; otherwise the global ones. Returns 0 when
 * something was read, -1 when neither source could be asked. */
int net_dns_load(NetDnsList *list, const NetConfig *config,
                 const char *link);

/* Set the servers to `servers` (a list of `count` addresses). With resolvectl
 * this applies to `link` when it names one, or to the global scope otherwise;
 * without it, the servers are written to /etc/resolv.conf, which needs root.
 * An empty list restores the automatic servers (resolvectl revert). Returns 0
 * on success; on failure -1 and a short message in `error`. */
int net_dns_set(const NetConfig *config, const char *link,
                const char *const *servers, int count,
                char *error, unsigned int size);

/* Whether the address looks like an IPv4 or IPv6 nameserver, so a typo is
 * caught before it is applied. Returns 1 when it is plausible. */
int net_dns_valid_server(const char *text);

#endif /* GNUCHANNET_DNS_H */
