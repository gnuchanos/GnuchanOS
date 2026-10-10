/*
 * net_privilege.h — running this program with the privileges it needs.
 *
 * The manager does two things that need to be root: it writes /etc/resolv.conf
 * when there is no resolvectl to talk to, and it can bring an interface up or
 * down with `ip link set`. Both are the kernel's and the resolver's to refuse a
 * normal user.
 *
 * The password is asked for in a WINDOW (net_login.c), not on a terminal. That
 * is the whole point: a manager started from the launcher has no terminal, and
 * one that stopped at a prompt nobody could see would look like a program that
 * opened nothing. The password is handed to sudo on its standard input — sudo
 * -S reads it from there — so no terminal is needed.
 */
#ifndef GNUCHANNET_PRIVILEGE_H
#define GNUCHANNET_PRIVILEGE_H

#include "net_config.h"

/* Make sure the program is running as root, re-running it through sudo if it is
 * not.
 *
 * When the caller is already root this returns 0 and the caller carries on.
 * When it is not, this shows the password window, re-runs the program through
 * sudo with the password on its standard input, waits for that copy to finish,
 * and then EXITS — it does not return to a caller, because the copy that opened
 * the window is the one that mattered and this process was only its launcher.
 *
 * `config` is the settings the password window is drawn in. It may be NULL.
 *
 * Returns -1 — and the caller should carry on WITHOUT root — only when there is
 * no display to ask on or no sudo to ask with; the privileged actions then
 * report their own failures, which is better than a window that will not open.
 */
int net_privilege_ensure(int argc, char **argv, const NetConfig *config);

#endif /* GNUCHANNET_PRIVILEGE_H */
