/*
 * wm_spawn.h — the program launcher every module shares.
 */
#ifndef GNUCHANWM_SPAWN_H
#define GNUCHANWM_SPAWN_H

/* The terminal Alt+Enter opens, or NULL when this machine has none. Read from
   $TERMINAL first, then from the usual programs. */
const char *wm_terminal_program(void);

/* fork + execvp. Returns 0 when the child was started, -1 otherwise. */
int wm_spawn(const char *program, char *const argv[]);

/* Open the terminal found by wm_terminal_program(). */
int wm_spawn_terminal(void);

#endif /* GNUCHANWM_SPAWN_H */
