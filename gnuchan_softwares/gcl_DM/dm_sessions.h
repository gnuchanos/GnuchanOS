/*
 * dm_sessions.h — the sessions this machine offers, read from disk.
 *
 * A display manager does not decide what a session is; the freedesktop entry
 * specification does, and every desktop on Debian writes one file per session
 * into /usr/share/xsessions. Reading those files is what makes the greeter
 * offer whatever the machine actually has installed — including the window
 * manager this project ships — instead of a list that has to be kept in step
 * with apt by hand.
 *
 * A file looks like this:
 *
 *     [Desktop Entry]
 *     Name=GnuChanWM
 *     Comment=GnuchanOS window manager
 *     Exec=/usr/local/bin/GnuChanWM
 *     Type=Application
 *
 * Only Name and Exec are needed: the name is what the user reads and the exec
 * is what is run, and a session's icon, comment or desktop names change
 * nothing about starting it.
 */
#ifndef GNUCHANDM_SESSIONS_H
#define GNUCHANDM_SESSIONS_H

/* How many sessions are offered. A machine with more than this installed is
   not a machine this greeter has to be designed around. */
#define DM_MAX_SESSIONS 32

#define DM_SESSION_NAME 128
#define DM_SESSION_EXEC 512

typedef struct DmSession {
    char name[DM_SESSION_NAME];   /* what the user reads                    */
    char exec[DM_SESSION_EXEC];   /* what is run, as it appears in Exec=    */
} DmSession;

/* Read the sessions installed on this machine into `out`.
 *
 * The list is sorted by name so the order does not depend on what readdir
 * happens to return, and GnuChanWM is put first when it is present: it is this
 * system's own session and the one a user of this system wants by default.
 *
 * Returns how many were found, which may be 0 — a machine with no sessions
 * installed is a machine whose greeter can only offer a terminal, and the
 * caller decides what to do about that rather than being handed a fabricated
 * entry it did not ask for.
 */
int dm_sessions_scan(DmSession *out, int max);

/* The index of the session called `name`, or -1. Used to select the one
 * written into the greeter's own configuration. */
int dm_sessions_find(const DmSession *sessions, int count, const char *name);

#endif /* GNUCHANDM_SESSIONS_H */
