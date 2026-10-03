/*
 * term_pty.h — the child process and the pseudo terminal it runs on.
 *
 * A terminal does not run a program; it runs a program somewhere else and
 * pretends to be a terminal to it. The somewhere else is a PTY: a pair of
 * devices where one side is the program's stdin, stdout and stderr, and the
 * other is this process's end. Everything the program prints arrives on the
 * master side as bytes, and everything typed is written back the same way.
 *
 * --- why a PTY and not a pipe ---
 *
 * A pipe would carry the bytes and nothing else. A program like btop would not
 * start: it calls isatty() on its own output and refuses to run if the answer
 * is no, because a program that draws a full screen has nothing sensible to
 * draw on a pipe. `ls` without a terminal does not colour its output. `sudo`
 * will not read a password. The whole class of programs that behave
 * differently when they are watched behaves as if nobody were watching —
 * which, on a pipe, is true.
 *
 * A PTY is a terminal as far as the program is concerned: isatty() says yes,
 * the window size is a real number it can ask for, and the line discipline is
 * there. That is what makes a terminal emulator possible at all.
 *
 * --- the two things that make btop run ---
 *
 * btop checks two things at start-up and quits with a message if either fails:
 *
 *   isatty(STDIN)      "No tty detected! btop++ needs an interactive shell"
 *   TIOCGWINSZ         "Failed to get size of terminal!"
 *
 * forkpty() answers the first, because the child's stdin IS the slave side of
 * a terminal. The second is answered by the size being SET on the slave before
 * the child is given a chance to ask — that is what the `winsize` argument to
 * forkpty() is for, and why the size is passed to him rather than applied
 * afterwards. A terminal that opened the PTY and then set the size would leave
 * a window of time for the child to ask and be told zero by zero, and btop
 * would spin a hundred times and quit.
 */
 
#ifndef GNUCHANTERM_PTY_H
#define GNUCHANTERM_PTY_H

#include <sys/types.h>

#include "term_vt.h"

typedef struct TermPty {
    int   master;      /* this process's end of the pair                  */
    pid_t child;       /* the program at the other end                    */

    /* Set when the child has been reaped. The loop reads it to stop: a
       terminal whose shell has exited has nothing left to show, and staying
       open on an empty screen would leave a window that does nothing. */
    int   child_exited;

    /* The grid's size in cells, kept so a resize can be pushed down without
       the caller having to pass it again. */
    int   cols;
    int   rows;
} TermPty;

/* --- life ----------------------------------------------------------------- */

/* Start `argv[0]` on a new PTY of `cols` by `rows` cells.
 *
 * The size is not optional and it is not set afterwards: it is passed on
 * before the child runs a single instruction, because the first thing a
 * full-screen program does is ask for it and it will not ask twice.
 *
 * `envp` is the environment the child gets — TERM, and whatever the session
 * decided — and the array must be NULL terminated. Returns 0 on success. */
int  term_pty_spawn(TermPty *pty, char *const argv[], char *const envp[],
                    int cols, int rows);

/* Close the master and wait for the child. Safe on a PTY that never spawned
   anything. */
void term_pty_close(TermPty *pty);

/* --- the two directions --------------------------------------------------- */

/* Read everything waiting and feed it to the parser. Returns the number of
   bytes read, or -1 when the child has closed its end — which is how a
   terminal learns its shell exited.
 *
 * The read loops until the PTY has nothing left. A program that prints a full
 * screen sends it in many small writes, and a read that stopped after one
 * would leave the rest until the next event — which is a screen that redraws
 * in pieces. */
int  term_pty_pump(TermPty *pty, TermVt *vt);

/* Write bytes to the child. This is what a keystroke becomes. Returns the
   number written, which is less than `len` when the child is not reading — a
   program busy drawing does not read its input, and the terminal must not
   block on it or the whole window freezes. */
int  term_pty_write(TermPty *pty, const char *bytes, int len);

/* Tell the child the window is now this many cells. TIOCSWINSZ on the master
   sets it on the slave, and the kernel raises SIGWINCH to the child's process
   group as part of that — it does not have to be sent by hand. */
void term_pty_set_size(TermPty *pty, int cols, int rows);

/* Whether the child is still running. */
int  term_pty_is_alive(const TermPty *pty);

/* The write callback the escape parser is given, so its answers to a program's
   queries come back through the same PTY. See TermVtHost. */
void term_pty_vt_write(void *user, const char *bytes, int len);

#endif /* GNUCHANTERM_PTY_H */
