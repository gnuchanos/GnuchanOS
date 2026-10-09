/*
 * wifi_privilege.h — running this program with the privileges it needs.
 *
 * The manager does two things that need to be root: it clears the kernel's
 * rfkill blocks and reloads the wireless driver, both of which the kernel
 * refuses to a normal user. It also lets nmcli change a connection without the
 * polkit conversation that a bare "not authorized" is the fruit of on a machine
 * with no polkit agent running — which is this desktop.
 *
 * So the program re-runs itself through sudo at start-up, once, and the copy
 * that opens the window is root. This is the same choice the installers make,
 * and it is deliberate: a wifi window that half-works because every second
 * action is refused is worse than a window that asks for the password once.
 */
#ifndef GNUCHANWIFI_PRIVILEGE_H
#define GNUCHANWIFI_PRIVILEGE_H

/* Make sure the program is running as root, re-running it through sudo if it is
   not.
 *
 * This either raises the privileges and never returns — the process image is
 * replaced by the sudo'd copy — or it returns. It returns 0 when nothing needs
 * doing (already root, or a re-run was already tried and this is that re-run),
 * and -1 when the program could not be re-run at all (no sudo). A -1 is not
 * fatal on its own: the caller carries on and the actions that need root simply
 * fail with a message, which is better than a window that will not open.
 *
 * The DISPLAY and XAUTHORITY of the invoking session are passed through the
 * sudo so the copy that opens the window can still reach the X server, and HOME
 * is passed through so it reads the config out of the user's own home and not
 * root's. */
int wifi_privilege_ensure(int argc, char **argv);

#endif /* GNUCHANWIFI_PRIVILEGE_H */
