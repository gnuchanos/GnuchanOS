/*
 * net_dpi.h — turning the DPI bypass on and off.
 *
 * A network that blocks a site by reading the name inside a TCP packet — the
 * TLS SNI on port 443 — resets the connection the moment it reads it. That is
 * why a blocked site opens in Chromium (which reaches it over QUIC, UDP, that
 * the filter does not read) but not in ping, curl, or any ordinary program on
 * the system resolver. The way to make the REST of the machine reach it too is
 * a DPI desynchroniser: zapret's nfqws fragments the first packet so the name
 * is never readable in one piece, and the filter lets it through.
 *
 * This file is only the switch. It runs zapret's own init script to start and
 * stop the daemon, and looks at whether nfqws is running to know the state. It
 * does not carry zapret, build it, or choose its strategy — that is the
 * installer's job (see makefile.py, which puts zapret under /opt/zapret); this
 * is what a person presses once it is there.
 *
 * All three calls need root: starting nfqws inserts firewall rules. The manager
 * re-runs itself through sudo at start (see net_privilege.c), so by the time a
 * button is pressed it is already root.
 */
#ifndef GNUCHANNET_DPI_H
#define GNUCHANNET_DPI_H

#include "net_config.h"

/* Whether a DPI bypass is installed to switch on: zapret's init script is where
   the config says, or nfqws is on PATH. Returns 1 when there is something to
   start, 0 when there is not — in which case the button is drawn as unavailable
   and does nothing. */
int net_dpi_available(const NetConfig *config);

/* Whether the bypass is running now. `pgrep` for nfqws, which is the daemon
   zapret's NFQUEUE mode runs; when it is up the packets are being fragmented.
   Returns 1 when running, 0 when not. */
int net_dpi_active(const NetConfig *config);

/* Start (`on` non-zero) or stop the bypass, through zapret's init script.
   Returns 0 on success; on failure -1 and a short message in `error`. */
int net_dpi_set(const NetConfig *config, int on, char *error, unsigned int size);

#endif /* GNUCHANNET_DPI_H */
