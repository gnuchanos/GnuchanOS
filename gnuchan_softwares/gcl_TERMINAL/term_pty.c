/*
 * term_pty.c — the child process and the pseudo terminal it runs on.
 *
 * See term_pty.h for why this is a PTY and not a pipe, and for the two checks
 * btop makes at start-up that this file exists to satisfy.
 *
 * --- where the size is set, and why it is there ---
 *
 * forkpty() takes the window size as an argument and applies it to the slave
 * BEFORE the child runs. That ordering is the whole point: a full-screen
 * program's first act is to ask for the window size, and a program that gets
 * zero by zero gives up. Applying the size afterwards would leave a window of
 * time for the child to ask and be told nothing.
 */
 
#define _POSIX_C_SOURCE 200809L
/* _DEFAULT_SOURCE is asked for by the build anyway (-D_DEFAULT_SOURCE), and
   defining it here as well is a redefinition warning on every compile. It is
   left to the build so there is one place that decides. */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__linux__)
#  include <pty.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#  include <util.h>
#else
#  include <pty.h>
#endif

#include "term_pty.h"

/* The process environment. It is declared here because <unistd.h> only exposes
   it under _GNU_SOURCE, and this is the one file that replaces it: the child
   is given a TERM that describes THIS terminal, so an inherited TERM=xterm
   must not survive the exec. */
extern char **environ;

int term_pty_spawn(TermPty *pty, char *const argv[], char *const envp[],
                   int cols, int rows) {
    memset(pty, 0, sizeof(*pty));
    pty->master = -1;
    pty->child = -1;
    pty->cols = cols;
    pty->rows = rows;

    /* TIOCGWINSZ answers with exactly these fields, and a program that asks
       gets the number of CELLS — not pixels — which is why the pixel fields
       stay zero: a terminal emulator that does not know its own pixel size is
       telling the truth, and a program that wants pixels divides by the cell
       size or does not ask. */
    struct winsize size;
    memset(&size, 0, sizeof(size));
    size.ws_col = (unsigned short)cols;
    size.ws_row = (unsigned short)rows;

    int master = -1;
    pid_t child = forkpty(&master, NULL, NULL, &size);
    if (child < 0) {
        return -1;
    }

    if (child == 0) {
        /* --- the child ---
         *
         * forkpty() has already made the slave this process's controlling
         * terminal and has already put stdin, stdout and stderr on it. What is
         * left is to become the program.
         *
         * The environment is REPLACED rather than added to, because the
         * terminal passes a TERM that describes itself and an inherited one
         * would win: a session started from a shell that had TERM=xterm would
         * hand the child xterm, and the child would then use sequences this
         * terminal does not implement. */
        if (envp != NULL) {
            environ = (char **)envp;
        }
        execvp(argv[0], argv);

        /* A program that could not be started is reported ON THE PTY: this
           process has no terminal of its own, and a silent exit is a window
           that closes for no visible reason. */
        fprintf(stderr, "gcl_terminal: cannot run '%s': %s\r\n",
                argv[0], strerror(errno));
        _exit(127);
    }

    pty->master = master;
    pty->child = child;

    /* Non-blocking, because the reads and writes happen in the event loop. A
       blocking read on a program that has nothing to say would stop the window
       from redrawing, and a blocking write would freeze it the moment the
       child was busy — see term_pty_write(). */
    int flags = fcntl(pty->master, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(pty->master, F_SETFL, flags | O_NONBLOCK);
    }

    /* A shell that exits is a normal end and not a fault. The default
       disposition is already to ignore SIGCHLD, but it is set explicitly here
       because a session that inherited SIG_IGN from its parent would otherwise
       never get the child reaped. */
    signal(SIGCHLD, SIG_DFL);
    return 0;
}

void term_pty_close(TermPty *pty) {
    if (pty->master >= 0) {
        close(pty->master);
        pty->master = -1;
    }
    if (pty->child > 0) {
        /* The child is asked to leave rather than killed outright: a shell in
           the middle of writing a file should get the chance to finish, and
           SIGHUP is what a terminal closing sends. */
        kill(pty->child, SIGHUP);
        int status = 0;
        waitpid(pty->child, &status, 0);
        pty->child = -1;
    }
}

int term_pty_pump(TermPty *pty, TermVt *vt) {
    if (pty->master < 0) {
        return -1;
    }

    /* Read until there is nothing left. A program that draws a screen writes
       it in many pieces — btop writes its box outlines, then each panel, then
       the cursor position — and each is a separate write. A read that stopped
       after the first would draw the screen in whatever order the pieces
       arrived and leave the rest until the next event, which on a quiet
       session is never. */
    char buffer[8192];
    int total = 0;
    for (;;) {
        ssize_t got = read(pty->master, buffer, sizeof(buffer));

        if (got > 0) {
            term_vt_feed(vt, buffer, (int)got);
            total += (int)got;
            continue;
        }

        if (got == 0) {
            /* The child closed its end: the shell exited. */
            pty->child_exited = 1;
            return -1;
        }

        /* A read of a PTY master answers EIO when the slave side is gone, which
           is the same news as a zero read. */
        if (errno == EIO) {
            pty->child_exited = 1;
            return -1;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Everything waiting has been read. This is the normal end. */
            return total;
        }
        if (errno == EINTR) {
            /* A signal arrived mid-read — this happens on every window resize,
               when SIGWINCH lands — and the read is worth trying again. */
            continue;
        }
        return total;
    }
}

int term_pty_write(TermPty *pty, const char *bytes, int len) {
    if (pty->master < 0 || len <= 0) {
        return 0;
    }

    /* A write that would block is DROPPED rather than retried.
     *
     * A program that is busy drawing does not read its input and the PTY's
     * buffer fills. Waiting for it would stop the event loop — no redraw, no
     * resize, no second keystroke — and the terminal would freeze exactly when
     * the program is busiest, which is when a user is most likely to be
     * typing.
     *
     * Dropping loses the keystroke. That is the trade every terminal makes: a
     * key lost into a program that is not reading is worth less than a window
     * that stops responding.
     */
    ssize_t written = write(pty->master, bytes, (size_t)len);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        return -1;
    }
    return (int)written;
}

void term_pty_vt_write(void *user, const char *bytes, int len) {
    TermPty *pty = (TermPty *)user;
    if (pty == NULL) {
        return;
    }
    /* The same road as a keystroke: it is bytes going TO the child. */
    term_pty_write(pty, bytes, len);
}

void term_pty_set_size(TermPty *pty, int cols, int rows) {
    if (pty->master < 0 || cols < 1 || rows < 1) {
        return;
    }
    pty->cols = cols;
    pty->rows = rows;

    struct winsize size;
    memset(&size, 0, sizeof(size));
    size.ws_col = (unsigned short)cols;
    size.ws_row = (unsigned short)rows;

    /* TIOCSWINSZ on the master sets it on the slave, which is the terminal the
       child sees. The kernel raises SIGWINCH to the child's process group as
       part of this — it does not have to be sent by hand, and sending it as
       well would deliver it twice. */
    ioctl(pty->master, TIOCSWINSZ, &size);
}

int term_pty_is_alive(const TermPty *pty) {
    return pty->master >= 0 && !pty->child_exited;
}
