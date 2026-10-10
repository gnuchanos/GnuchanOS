/*
 * net_shell.h — running a network command and reading what it says, in one
 * place.
 *
 * Every module here talks to the system the same way: build a command, run it,
 * read its output. That is three things — the command runner, the quoting and
 * the terse-field splitter — and they live here so net_device.c, net_address.c
 * and net_dns.c do not each carry their own copy.
 *
 * The tools this program drives are nmcli (NetworkManager), ip (the kernel's
 * own network state) and resolvectl (systemd's resolver). They are asked in the
 * order that prefers the higher-layer tool: nmcli knows about connections and
 * profiles, ip knows the live kernel state, resolvectl knows the resolver.
 * net_device.c and net_dns.c decide which to ask for each question; this file
 * only knows how to run whatever they build.
 *
 * The output format every caller reads from nmcli is its terse one — `-t`,
 * fields separated by colons, with a colon or a backslash inside a value
 * escaped — so net_shell_split() undoes that escaping here as well. ip's own
 * output is line-oriented and read whole.
 */
#ifndef GNUCHANNET_SHELL_H
#define GNUCHANNET_SHELL_H

/* The most text any one field, name or command may hold. 256 is far above any
   interface name, address or profile name, and it is the ceiling that keeps a
   malformed answer from making anything allocate without end. */
#define NET_TEXT 256

/* Run a command through the shell and copy stdout and stderr together into
   `out`. Returns the exit status, or 127 when the command could not be run at
   all. Both streams are joined so a failure nmcli narrated on stderr is read
   back the same way as its output. */
int net_shell_run(const char *command, char *out, unsigned int size);

/* Quote one argument so the shell passes it through whole, with a single quote
   inside it handled. `out` should be about twice the text plus a few. */
void net_shell_quote(const char *in, char *out, unsigned int size);

/* Copy the next terse field of `line` into `out`, and return a pointer to the
   character after the field's terminating colon, or NULL at the end of the
   line. A NULL line gives an empty field and NULL back, so a caller walking a
   record can stop at the first NULL. */
const char *net_shell_split(const char *line, char *out, unsigned int size);

/* Whether a program is on PATH. Used to decide whether resolvectl exists at
   all, so the resolver question can be asked of nmcli instead when it does
   not. */
int net_shell_have(const char *program);

#endif /* GNUCHANNET_SHELL_H */
