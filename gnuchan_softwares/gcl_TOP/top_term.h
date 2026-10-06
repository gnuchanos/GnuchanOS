/*
 * top_term.h — the terminal: raw mode, its size, and the keys read from it.
 *
 * A monitor owns the screen while it runs, so it puts the terminal into raw
 * mode — no line buffering, no echo, no signals from the control keys — draws
 * its frames, and puts the terminal back exactly as it found it before it
 * exits. The saving and restoring are the whole reason this is a module: a
 * program that changed the terminal and died without restoring it leaves the
 * person's shell unusable, and that is worse than the monitor not running.
 */
#ifndef GNUCHANTOP_TERM_H
#define GNUCHANTOP_TERM_H

typedef struct {
    int rows;
    int cols;
} TopSize;

/* What a key press was. Arrows and a few letters are named; anything else is
   TOP_KEY_NONE or TOP_KEY_CHAR with the byte itself. */
typedef enum {
    TOP_KEY_NONE = 0,
    TOP_KEY_UP,
    TOP_KEY_DOWN,
    TOP_KEY_PAGE_UP,
    TOP_KEY_PAGE_DOWN,
    TOP_KEY_HOME,
    TOP_KEY_END,
    TOP_KEY_ENTER,
    TOP_KEY_ESCAPE,
    TOP_KEY_CHAR
} TopKeyKind;

typedef struct {
    TopKeyKind kind;
    char       ch;   /* valid when kind is TOP_KEY_CHAR */
} TopKey;

/* Put the terminal into raw mode and hide the cursor. Returns 0, or -1 when
   stdin is not a terminal. The previous settings are saved for top_term_restore.
   A handler is installed for SIGINT/SIGTERM/SIGWINCH so a signal restores the
   terminal and reports the resize. */
int top_term_enter(void);

/* Put the terminal back the way it was, show the cursor, and leave the alternate
   screen. Safe to call more than once. */
void top_term_restore(void);

/* The size of the terminal in cells, read fresh so a resize is picked up. */
TopSize top_term_size(void);

/* Read one key without blocking forever: waits up to `timeout_ms` and returns
   0 and a TOP_KEY_NONE when nothing arrived. Returns 1 when a key was read. */
int top_term_read_key(TopKey *key, int timeout_ms);

/* Whether a SIGWINCH arrived since the last call, clearing the flag. */
int top_term_take_resized(void);

/* Whether a quit signal (SIGINT/SIGTERM) arrived, clearing the flag. */
int top_term_take_quit(void);

#endif /* GNUCHANTOP_TERM_H */
