/*
 * runner_launch.h — starting the program that was chosen.
 *
 * The whole point of the launcher is this one call, and it is small on
 * purpose. Two things are started: a program from the list, whose command was
 * read off a .desktop file; and a command line the user typed, which is one
 * command and no more.
 *
 * The child is detached from the launcher properly, and that is the part worth
 * getting right. fork() alone would leave the program a child of a process
 * that is about to exit, and the two things that follow from that are both
 * wrong: the program inherits the launcher's display connection unless it is
 * closed first, and it holds the terminal the launcher was started from. So
 * the child is put in a session of its own with setsid(), its standard streams
 * are pointed at nothing, and the display connection is closed in the child —
 * after which it is a program the launcher happened to start, and nothing
 * more.
 *
 * A program marked Terminal=true is run inside a terminal window instead,
 * because that is what the .desktop file said it needs: a program with no
 * window of its own that asks for a place to be run in.
 */
#ifndef GNUCHANRUNNER_LAUNCH_H
#define GNUCHANRUNNER_LAUNCH_H

#include <X11/Xlib.h>

#include "runner_apps.h"

/* Run one program from the list. The program is run with the terminal it asks
   for when it asked for one. The display is closed in the child, so the
   program does not inherit the launcher's connection to the server. Returns 0
   when the child was started, -1 when it could not be — a program with no
   command, or a fork that failed. */
int runner_launch_program(Display *display, const RunnerProgram *program);

/* Run a command line the user typed.
 *
 * The line is split the same way a .desktop file's Exec is — white space
 * separates words, quotes group them — and run without a shell: what was typed
 * is a program and its arguments, not a script. A line that is empty, or whose
 * first word is nothing, starts nothing. */
int runner_launch_command(Display *display, const char *command);

/* The terminal a program marked Terminal=true is run in: $TERMINAL when it is
   set and runnable, then the usual programs in the order a machine is most
   likely to have one. NULL when this machine has none, in which case the
   program is run with no terminal and whatever it prints goes nowhere — which
   is worse than a terminal, and better than not starting it at all. */
const char *runner_launch_terminal(void);

#endif /* GNUCHANRUNNER_LAUNCH_H */
