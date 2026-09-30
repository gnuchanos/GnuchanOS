/*
 * runner_ui.h — the window, the query, and the loop that reads keys.
 *
 * Everything the launcher is made of comes together here: the display, the
 * palette, the list of programs, and what has been typed so far. The state is
 * one struct rather than a set of globals because there is exactly one
 * launcher window per process, and one struct is what makes that visible.
 *
 * Two things the launcher does with the window are worth stating, because both
 * are what make it feel like a launcher rather than a window:
 *
 *   It is override-redirect, so the window manager — which is GnuChanWM, on
 *   this desktop — never frames it. A launcher with a title bar and a close
 *   button is not a launcher.
 *
 *   It holds the keyboard and the pointer while it is up, so the first
 *   keystroke is the launcher's rather than whatever had focus before it, and
 *   a click anywhere closes it. This is the same grab the desktop's menu uses,
 *   for the same reason.
 *
 * The launcher is a program like any other on the session: it is started from
 * a key binding, it draws its own window, and it exits when it is done. It
 * does not know GnuChanWM exists.
 */
#ifndef GNUCHANRUNNER_UI_H
#define GNUCHANRUNNER_UI_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "runner_apps.h"
#include "runner_config.h"
#include "runner_match.h"
#include "runner_style.h"

/* The most a query may be. Far longer than any program's name, and the ceiling
   is what keeps a key held down from growing the buffer without end. */
#define RUNNER_MAX_QUERY 256

typedef struct RunnerUi {
    Display *display;
    int screen;
    Window root;
    Window window;
    GC gc;

    int screen_width;
    int screen_height;
    int width;              /* the window's own size, worked out at open */
    int height;

    RunnerConfig config;
    RunnerStyle style;

    RunnerProgramList programs;   /* every program, read once, held */
    RunnerMatches matches;        /* what the query finds, best first */

    /* What has been typed. The caret is always at the end: this launcher has
       no use for moving it, and a caret that cannot be moved is one fewer
       thing to explain. */
    char query[RUNNER_MAX_QUERY];
    int query_length;

    /* Which match is chosen, into `matches`, and the first one the list is
       scrolled to. Two numbers rather than one so the chosen row and the top
       of the visible list are told apart: they are only the same until the
       list is longer than the window. */
    int selected;
    int scroll;

    /* The row that runs a command line rather than a program: the index into
       `matches` it takes, or -1 when no command line is offered this time.
       Worked out on every query, because whether one is offered depends on
       what was typed. */
    int command_row;

    /* Set when something was chosen, and read by the caller as "this is what
       to run". The UI itself starts nothing — see runner_ui_run() — because a
       UI that starts programs cannot be tried without starting them. */
    int launch_selected;          /* the program row, or -1 for none        */
    char launch_command[RUNNER_MAX_QUERY];

    int running;
} RunnerUi;

/* Open the display and make the window. Returns 0 on success, -1 when there is
   no display or the window could not be made. `config_path` is the settings
   file to read; an empty path uses the one runner_config_path() finds. */
int runner_ui_open(RunnerUi *ui, const char *config_path);

/* Read keys and redraw until something is chosen or the launcher is dismissed.
   Returns 0 when a program or a command was chosen — which is then in
   ui->launch_selected and ui->launch_command — and -1 when it was dismissed
   with nothing chosen. */
int runner_ui_run(RunnerUi *ui);

/* Close the window, release the grabs, and free everything the open made. Safe
   to call whether or not the open succeeded. */
void runner_ui_close(RunnerUi *ui);

#endif /* GNUCHANRUNNER_UI_H */
