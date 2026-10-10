/*
 * net_dns.c — read and set the resolver.
 *
 * Two sources, the modern one preferred. resolvectl is asked first: it is
 * systemd-resolved's own tool and answers per-link, which is what a network
 * manager wants — the servers a wired link uses and the servers a wireless one
 * uses are different questions on the same machine. When resolvectl is not
 * installed, /etc/resolv.conf is read instead, which is the classic single
 * answer every program understands.
 *
 * Setting is the same split: resolvectl applies to one link and takes effect
 * at once; without it, the servers are written to /etc/resolv.conf, which needs
 * root — hence the same password window the rest of the desktop uses.
 */
#include "net_dns.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Whether `text` is a plausible IPv4 or IPv6 address. Checked so a typo is
   caught before it becomes the resolver the machine cannot reach. A full parse
   is not needed: the shape is enough to stop "1.1.1" or "google" being applied,
   and the resolver itself rejects a name it cannot use. */
int net_dns_valid_server(const char *text) {
    if (!text || !text[0]) {
        return 0;
    }
    int digits = 0;
    int dots = 0;
    int colons = 0;
    for (const char *p = text; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            digits++;
        } else if (*p == '.') {
            dots++;
        } else if (*p == ':') {
            colons++;
        } else if (isxdigit((unsigned char)*p) && colons > 0) {
            /* An IPv6 hextet. */
        } else {
            return 0;
        }
    }
    if (colons > 0) {
        return 1;                     /* an IPv6 address shape */
    }
    return digits > 0 && dots == 3;   /* an IPv4 address shape */
}

/* Whether an address is one a person would ever type in a DNS box, which is
   the test for showing it. IPv6 link-local addresses (fe80::) and the loopback
   resolver (::1, 127.0.0.1) are what a router hands out on its own and what a
   stub resolver runs on; they are not a server anyone chose, and in a pair of
   boxes that ask "which server do you want" they are noise. Windows shows none
   of them, and neither does this: a real DNS address, v4 or global v6, is
   kept. */
static int is_showable_server(const char *address) {
    if (!address || !address[0]) {
        return 0;
    }
    if (strncmp(address, "fe80:", 5) == 0 ||
        strncmp(address, "FE80:", 5) == 0) {
        return 0;                     /* IPv6 link-local */
    }
    if (strcmp(address, "::1") == 0 || strcmp(address, "127.0.0.1") == 0 ||
        strcmp(address, "0.0.0.0") == 0) {
        return 0;                     /* loopback / unset */
    }
    return 1;
}

/* Walk a whitespace-separated run of addresses from `text`, appending each that
   a person could have chosen to `list` until `max` is reached. Used by both
   readers, because resolvectl prints its servers space-separated on one line.
   A link-local or loopback address is skipped, so the list a person sees is the
   servers that were actually configured. */
static void collect_servers(NetDnsList *list, const char *text, int max) {
    while (text && *text && list->count < max) {
        while (*text == ' ' || *text == '\t' || *text == ',') {
            text++;
        }
        if (!*text || *text == '\n') {
            return;
        }
        char address[NET_TEXT];
        int at = 0;
        while (*text && *text != ' ' && *text != '\t' && *text != ',' &&
               *text != '\n' && at < NET_TEXT - 1) {
            address[at++] = *text++;
        }
        address[at] = '\0';
        /* resolvectl marks a link-local server with its link, "fe80::1%wlan0";
           the part after the percent is the interface, not the address, and is
           dropped so the address reads as it would be typed. */
        char *percent = strchr(address, '%');
        if (percent) {
            *percent = '\0';
        }
        if (is_showable_server(address)) {
            snprintf(list->servers[list->count], NET_TEXT, "%s", address);
            list->count++;
        }
    }
}

/* Read the servers resolvectl reports for a link (or the global scope when
   `link` is empty). Several lines, one per link: "Link 2 (wlan0): 1.1.1.1".
   Only the line whose link matches is read; the global answer is the line that
   begins "Global:". Returns 0 when something was read. */
static int read_resolvectl(NetDnsList *list, const NetConfig *config,
                           const char *link) {
    char command[NET_TEXT * 4];
    if (link && link[0]) {
        char quoted[NET_TEXT * 2];
        net_shell_quote(link, quoted, sizeof(quoted));
        snprintf(command, sizeof(command), "%s dns %s", config->resolvectl,
                 quoted);
    } else {
        snprintf(command, sizeof(command), "%s dns", config->resolvectl);
    }

    static char output[NET_TEXT * 8];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        return -1;
    }

    char *line = output;
    while (line && *line) {
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }

        /* The servers are what follows the last colon on the line. For a link
           line that is "Link 2 (wlan0): 1.1.1.1"; for the global one it is
           "Global: 1.1.1.1". The link's own line is wanted when a link was
           named, the global one otherwise. */
        int wanted = 0;
        if (link && link[0]) {
            wanted = (strstr(line, link) != NULL && strncmp(line, "Global", 6));
        } else {
            wanted = (strncmp(line, "Global", 6) == 0);
        }
        if (wanted) {
            char *colon = strrchr(line, ':');
            if (colon) {
                collect_servers(list, colon + 1, NET_MAX_DNS);
                if (list->count > 0) {
                    snprintf(list->source, sizeof(list->source), "resolvectl");
                    return 0;
                }
            }
        }

        line = newline ? newline + 1 : NULL;
    }
    return -1;
}

/* Read the nameservers from /etc/resolv.conf. One per "nameserver" line. */
static int read_resolv_conf(NetDnsList *list) {
    FILE *file = fopen("/etc/resolv.conf", "r");
    if (!file) {
        return -1;
    }
    char line[NET_TEXT];
    while (fgets(line, sizeof(line), file) && list->count < NET_MAX_DNS) {
        char *content = line;
        while (*content == ' ' || *content == '\t') {
            content++;
        }
        if (strncmp(content, "nameserver", 10) != 0) {
            continue;
        }
        char *value = content + 10;
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        char address[NET_TEXT];
        int at = 0;
        while (*value && *value != ' ' && *value != '\t' &&
               *value != '\n' && at < NET_TEXT - 1) {
            address[at++] = *value++;
        }
        address[at] = '\0';
        /* The same filter the resolvectl reader uses: a link-local or loopback
           server is what the network handed out on its own, not something a
           person chose, and it does not belong in a box that asks which server
           to use. This is the path that showed the stray fe80:: entries — the
           file, not resolvectl, is where a router's own link-local resolver
           lands, and this reader was copying it straight through. */
        if (is_showable_server(address)) {
            snprintf(list->servers[list->count], NET_TEXT, "%s", address);
            list->count++;
        }
    }
    fclose(file);
    if (list->count > 0) {
        snprintf(list->source, sizeof(list->source), "resolv.conf");
        return 0;
    }
    return -1;
}

int net_dns_load(NetDnsList *list, const NetConfig *config,
                 const char *link) {
    if (!list) {
        return -1;
    }
    memset(list, 0, sizeof(*list));

    if (net_shell_have(config->resolvectl)) {
        if (read_resolvectl(list, config, link) == 0) {
            return 0;
        }
        /* resolvectl is installed but answered nothing for this link, which is
           what a link with no servers of its own looks like; the file is the
           honest fallback. */
        list->count = 0;
    }

    return read_resolv_conf(list);
}

/* Write the servers into /etc/resolv.conf, replacing every nameserver line and
   keeping every other line (search domains, options) as it was. This is the
   path for a machine with no resolvectl. Needs root. */
static int write_resolv_conf(const char *const *servers, int count,
                             char *error, unsigned int size) {
    FILE *in = fopen("/etc/resolv.conf", "r");
    char kept[NET_TEXT * 8];
    kept[0] = '\0';
    if (in) {
        char line[NET_TEXT];
        while (fgets(line, sizeof(line), in)) {
            char *content = line;
            while (*content == ' ' || *content == '\t') {
                content++;
            }
            if (strncmp(content, "nameserver", 10) == 0) {
                continue;   /* the ones being replaced */
            }
            size_t used = strlen(kept);
            if (used + strlen(line) + 1 < sizeof(kept)) {
                snprintf(kept + used, sizeof(kept) - used, "%s", line);
            }
        }
        fclose(in);
    }

    FILE *out = fopen("/etc/resolv.conf", "w");
    if (!out) {
        if (error && size) {
            snprintf(error, size,
                     "could not write /etc/resolv.conf (needs root)");
        }
        return -1;
    }
    for (int i = 0; i < count; i++) {
        fprintf(out, "nameserver %s\n", servers[i]);
    }
    if (kept[0]) {
        fputs(kept, out);
    }
    fclose(out);
    return 0;
}

int net_dns_set(const NetConfig *config, const char *link,
                const char *const *servers, int count,
                char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    /* Every address is checked before anything is applied, so a list with one
       typo is refused whole rather than half-applied into a resolver that
       cannot reach anything. */
    for (int i = 0; i < count; i++) {
        if (!net_dns_valid_server(servers[i])) {
            if (error && size) {
                snprintf(error, size, "\"%s\" is not an address", servers[i]);
            }
            return -1;
        }
    }

    if (net_shell_have(config->resolvectl)) {
        char command[NET_TEXT * 8];
        char quote_link[NET_TEXT * 2];
        if (link && link[0]) {
            net_shell_quote(link, quote_link, sizeof(quote_link));
        } else {
            quote_link[0] = '\0';
        }

        if (count == 0) {
            /* No servers asked for means "go back to automatic", which
               resolvectl does with revert. */
            if (link && link[0]) {
                snprintf(command, sizeof(command), "%s revert %s",
                         config->resolvectl, quote_link);
            } else {
                snprintf(command, sizeof(command), "%s revert",
                         config->resolvectl);
            }
        } else {
            char list_text[NET_TEXT * NET_MAX_DNS * 2] = "";
            for (int i = 0; i < count; i++) {
                char quoted[NET_TEXT * 2];
                net_shell_quote(servers[i], quoted, sizeof(quoted));
                size_t used = strlen(list_text);
                snprintf(list_text + used, sizeof(list_text) - used, " %s",
                         quoted);
            }
            if (link && link[0]) {
                snprintf(command, sizeof(command), "%s dns %s%s",
                         config->resolvectl, quote_link, list_text);
            } else {
                /* The special name "~." is systemd-resolved's own spelling for
                   the global scope: it is the routing domain that matches
                   every name, so servers set against it answer everything. */
                snprintf(command, sizeof(command), "%s dns ~.%s",
                         config->resolvectl, list_text);
            }
        }

        static char output[NET_TEXT * 4];
        if (net_shell_run(command, output, sizeof(output)) != 0) {
            if (error && size) {
                char *newline = strchr(output, '\n');
                if (newline) {
                    *newline = '\0';
                }
                snprintf(error, size, "%s",
                         output[0] ? output : "resolvectl refused the change");
            }
            return -1;
        }
        return 0;
    }

    return write_resolv_conf(servers, count, error, size);
}

int net_dns_secure(const NetConfig *config, const char *link, int on,
                   char *error, unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    /* Encrypted DNS is systemd-resolved's to do; there is no equivalent in
       /etc/resolv.conf, which is a plain list of addresses with nowhere to say
       "and talk TLS". A machine without resolvectl therefore cannot be made
       secure from here, and saying so is the honest answer rather than writing
       a file that changes nothing. */
    if (!net_shell_have(config->resolvectl)) {
        if (error && size) {
            snprintf(error, size,
                     "resolvectl is not installed, so encrypted DNS is not "
                     "available (install systemd-resolved)");
        }
        return -1;
    }

    /* `resolvectl dnsovertls <link> yes` turns DNS-over-TLS on for that link;
       with no link it applies to the global scope. The name is quoted the same
       way every other argument in this file is. */
    char command[NET_TEXT * 4];
    if (link && link[0]) {
        char quoted[NET_TEXT * 2];
        net_shell_quote(link, quoted, sizeof(quoted));
        snprintf(command, sizeof(command), "%s dnsovertls %s %s",
                 config->resolvectl, quoted, on ? "yes" : "no");
    } else {
        snprintf(command, sizeof(command), "%s dnsovertls %s",
                 config->resolvectl, on ? "yes" : "no");
    }

    static char output[NET_TEXT * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output : "resolvectl refused the change");
        }
        return -1;
    }
    return 0;
}
