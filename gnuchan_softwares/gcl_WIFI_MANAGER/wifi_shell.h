/*
 * wifi_shell.h — running nmcli and reading what it says, in one place.
 *
 * Every module that talks to NetworkManager talks to it the same way: build a
 * command, run it through the shell, read its output. That is three things —
 * the program's name, the command runner, and the quoting — and they live here
 * so wifi_nm.c, wifi_radio.c and wifi_saved.c do not each carry their own copy.
 *
 * The output format every caller reads is nmcli's terse one — `-t`, fields
 * separated by colons, with a colon or a backslash inside a value escaped — so
 * wifi_shell_split() undoes that escaping here as well. A caller that wants the
 * fields of a line asks for them one at a time.
 */
#ifndef GNUCHANWIFI_SHELL_H
#define GNUCHANWIFI_SHELL_H

/* The most text any one field, name or command may hold. 256 is far above any
   SSID, interface name or file name, and it is the ceiling that keeps a
   malformed answer from making anything allocate without end. */
#define WIFI_TEXT 256

/* Which nmcli to run: "nmcli" by default, so PATH decides, and a config may
   name an absolute path instead. */
void wifi_shell_set_program(const char *program);
const char *wifi_shell_program(void);

/* Run a command through the shell and copy stdout and stderr together into
   `out`. Returns the exit status, or 127 when the command could not be run at
   all. Both streams are joined so a failure the program narrated on stderr is
   read back the same way as its output. */
int wifi_shell_run(const char *command, char *out, unsigned int size);

/* Quote one argument so the shell passes it through whole, with a single quote
   inside it handled. `out` should be about twice the text plus a few. */
void wifi_shell_quote(const char *in, char *out, unsigned int size);

/* Copy the next terse field of `line` into `out`, and return a pointer to the
   character after the field's terminating colon, or NULL at the end of the
   line. A NULL line gives an empty field and NULL back, so a caller walking a
   record can stop at the first NULL. */
const char *wifi_shell_split(const char *line, char *out, unsigned int size);

#endif /* GNUCHANWIFI_SHELL_H */
