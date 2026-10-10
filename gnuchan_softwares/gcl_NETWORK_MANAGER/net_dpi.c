/*
 * net_dpi.c — read whether a DPI bypass is there, and switch it on and off.
 *
 * See net_dpi.h for what this is and why. What is here is the three answers the
 * button needs: whether zapret is installed at all (so the button can be drawn
 * as unavailable and do nothing), whether the bypass is running now (so the
 * button points the right way), and the start and stop themselves.
 *
 * The interface is zapret's OWN init script rather than a bare `nfqws`
 * invocation. nfqws is useless on its own: it reads packets out of an NFQUEUE,
 * and the firewall rules that put packets into that queue are what make it do
 * anything. Those rules and the daemon together are what zapret's installer set
 * up, and the script it left behind is what brings the whole thing up and takes
 * it down. Running that script is therefore what "start" and "stop" mean on
 * every kind of machine, and it is the same script the systemd unit and the
 * OpenRC service both call.
 */
#include "net_dpi.h"

#include <stdio.h>
#include <string.h>

/* The daemon zapret's NFQUEUE mode runs: it fragments the first packet so a
   filter reading the name inside it never sees the name in one piece. Its
   being up is how the bypass is known to be running — the init script starts
   it and stops it. */
#define NET_DPI_DAEMON "nfqws"

/* Where zapret's portable init script lands when its installer runs, used when
   the config names nothing. It is the script every init system wraps, so
   running it directly is what start and stop mean regardless of how the machine
   boots. */
#define NET_DPI_INIT "/opt/zapret/init.d/sysv/zapret"

/* The init script this run uses: the config's own path when it names one, the
   installed one otherwise. Written into `out`. */
static void dpi_init_path(const NetConfig *config, char *out,
                          unsigned int size) {
    if (config && config->dpi_init[0]) {
        snprintf(out, size, "%s", config->dpi_init);
    } else {
        snprintf(out, size, "%s", NET_DPI_INIT);
    }
}

int net_dpi_available(const NetConfig *config) {
    char script[NET_TEXT * 2];
    dpi_init_path(config, script, sizeof(script));
    if (net_shell_have(script)) {
        return 1;
    }
    /* A machine where the daemon is on PATH but the script is somewhere the
       config does not name can still have a bypass that runs; the button is
       offered so it can be stopped. */
    return net_shell_have(NET_DPI_DAEMON);
}

int net_dpi_active(const NetConfig *config) {
    (void)config;   /* the daemon is the same wherever the script lives */

    /* `pgrep -x` matches the process NAME exactly, so a process that merely
       carries "nfqws" in its arguments is not mistaken for the daemon. A zero
       status is a match, and a match is a running bypass. */
    char command[NET_TEXT];
    snprintf(command, sizeof(command), "pgrep -x %s", NET_DPI_DAEMON);

    static char output[NET_TEXT];
    return net_shell_run(command, output, sizeof(output)) == 0;
}

int net_dpi_set(const NetConfig *config, int on, char *error,
                unsigned int size) {
    if (error && size) {
        error[0] = '\0';
    }

    char script[NET_TEXT * 2];
    dpi_init_path(config, script, sizeof(script));

    if (!net_shell_have(script)) {
        if (error && size) {
            snprintf(error, size,
                     "zapret was not found at %s, so there is no bypass to %s",
                     script, on ? "start" : "stop");
        }
        return -1;
    }

    char quoted[NET_TEXT * 2];
    net_shell_quote(script, quoted, sizeof(quoted));

    char command[NET_TEXT * 4];
    snprintf(command, sizeof(command), "%s %s", quoted,
             on ? "start" : "stop");

    /* The script narrates what it does on both streams; they are joined by
       net_shell_run(), so the first line of a refusal is what is shown. */
    static char output[NET_TEXT * 4];
    if (net_shell_run(command, output, sizeof(output)) != 0) {
        if (error && size) {
            char *newline = strchr(output, '\n');
            if (newline) {
                *newline = '\0';
            }
            snprintf(error, size, "%s",
                     output[0] ? output
                               : (on ? "the DPI bypass could not be started"
                                     : "the DPI bypass could not be stopped"));
        }
        return -1;
    }
    return 0;
}
