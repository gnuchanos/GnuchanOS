/*
 * top_term.c — raw mode, the alternate screen, and reading keys.
 *
 * The terminal is put into raw mode with tcsetattr and the previous termios is
 * kept, because the one thing this must never do is leave a shell in raw mode.
 * The alternate screen (the `1049` escape) is entered so the monitor does not
 * scroll the person's scrollback away, and the cursor is hidden so the frame
 * does not flicker around a caret.
 *
 * A key arrives as bytes. An arrow is the escape `ESC [ A` and its siblings;
 * those are read as a whole sequence, and a lone ESC that is not the start of
 * one is the Escape key. Read is done with a short poll so the main loop can
 * wake on its timer between keys.
 *
 * Signals matter here for the same reason the raw mode does: a SIGINT or
 * SIGTERM must restore the terminal before the process dies, so a handler sets a
 * flag the main loop checks, rather than letting the default action kill the
 * process with the terminal still raw. SIGWINCH sets a resize flag.
 */
#include "top_term.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved_termios;
static int            is_raw = 0;
static int            resized = 0;
static int            quit_requested = 0;

/* --- signals --------------------------------------------------------------- */

static void on_resize(int signal_number) {
    (void)signal_number;
    resized = 1;
}

static void on_quit(int signal_number) {
    (void)signal_number;
    quit_requested = 1;
}

/* --- entering and leaving -------------------------------------------------- */

/* Write the escapes that enter the alternate screen, hide the cursor and clear
   it. Doing this as one write keeps them from being split across a redraw. */
static void write_enter_escapes(void) {
    fputs("\033[?1049h"   /* alternate screen */
          "\033[?25l"     /* hide the cursor */
          "\033[2J"       /* clear */
          "\033[H",       /* home */
          stdout);
    fflush(stdout);
}

int top_term_enter(void) {
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) {
        return -1;
    }

    struct termios raw = saved_termios;
    /* No line buffering, no echo, no control keys doing their job, no input
       translation: the monitor reads every byte itself. */
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return -1;
    }
    is_raw = 1;

    signal(SIGWINCH, on_resize);
    signal(SIGINT, on_quit);
    signal(SIGTERM, on_quit);

    write_enter_escapes();
    return 0;
}

void top_term_restore(void) {
    if (!is_raw) {
        return;
    }
    /* Leave the alternate screen and show the cursor, then put the terminal
       settings back. The order does not matter to a terminal, but the escapes
       first means a failure in tcsetattr still leaves a usable screen. */
    fputs("\033[?25h"     /* show the cursor */
          "\033[?1049l",  /* leave the alternate screen */
          stdout);
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    is_raw = 0;
}

TopSize top_term_size(void) {
    TopSize size;
    size.rows = 24;
    size.cols = 80;
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0) {
        if (window.ws_row > 0) {
            size.rows = window.ws_row;
        }
        if (window.ws_col > 0) {
            size.cols = window.ws_col;
        }
    }
    return size;
}

/* --- reading a key --------------------------------------------------------- */

/* Wait for bytes with a timeout, and read what arrived into `buffer`. Returns
   the number of bytes, or 0 on timeout. */
static int read_bytes(char *buffer, size_t size, int timeout_ms) {
    struct pollfd descriptor;
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    int ready = poll(&descriptor, 1, timeout_ms);
    if (ready <= 0) {
        return 0;
    }
    ssize_t got = read(STDIN_FILENO, buffer, size);
    if (got <= 0) {
        return 0;
    }
    return (int)got;
}

int top_term_read_key(TopKey *key, int timeout_ms) {
    key->kind = TOP_KEY_NONE;
    key->ch = '\0';

    char buffer[8];
    int got = read_bytes(buffer, sizeof(buffer), timeout_ms);
    if (got <= 0) {
        return 0;
    }

    unsigned char first = (unsigned char)buffer[0];

    /* An escape sequence: ESC [ <letter>, or a lone ESC. */
    if (first == 0x1b) {
        if (got >= 3 && buffer[1] == '[') {
            switch (buffer[2]) {
                case 'A': key->kind = TOP_KEY_UP;        return 1;
                case 'B': key->kind = TOP_KEY_DOWN;      return 1;
                case 'H': key->kind = TOP_KEY_HOME;      return 1;
                case 'F': key->kind = TOP_KEY_END;       return 1;
                case '5': key->kind = TOP_KEY_PAGE_UP;   return 1; /* ESC [ 5 ~ */
                case '6': key->kind = TOP_KEY_PAGE_DOWN; return 1; /* ESC [ 6 ~ */
                default:  key->kind = TOP_KEY_ESCAPE;    return 1;
            }
        }
        key->kind = TOP_KEY_ESCAPE;
        return 1;
    }

    /* A printable key. */
    if (first == '\r' || first == '\n') {
        key->kind = TOP_KEY_ENTER;
        return 1;
    }
    key->kind = TOP_KEY_CHAR;
    key->ch = (char)first;
    return 1;
}

/* --- the signal flags ------------------------------------------------------ */

int top_term_take_resized(void) {
    int value = resized;
    resized = 0;
    return value;
}

int top_term_take_quit(void) {
    int value = quit_requested;
    quit_requested = 0;
    return value;
}
