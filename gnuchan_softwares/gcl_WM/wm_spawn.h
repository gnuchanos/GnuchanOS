/*
 * wm_spawn.h — the program launcher every module shares.
 */
#ifndef GNUCHANWM_SPAWN_H
#define GNUCHANWM_SPAWN_H

/* The terminal the settings script named, or empty to forget it. The script's
   own answer is what Alt+Enter and the first window open, so this is called
   with the parsed config before either of them runs. A name that cannot be
   run is not remembered: a typo must fall through to the candidates below
   rather than leave the key opening nothing. */
void wm_spawn_set_terminal(const char *program);

/* The terminal Alt+Enter opens, or NULL when this machine has none. Read from
   the settings script first, then $TERMINAL, then the usual programs. */
const char *wm_terminal_program(void);

/* fork + execvp. Returns 0 when the child was started, -1 otherwise. */
int wm_spawn(const char *program, char *const argv[]);

/* Run a written command line — RunProgram(command="rofi -show run").
 *
 * The script's argument is a command LINE and not the name of a program, so it
 * is split into words first — white space separates them, quotes group them, a
 * backslash escapes — and the words become argv. wm_spawn() cannot do this on
 * its own: it is handed words already, which is what it is for. Returns 0 when
 * the program was started, -1 when the line was empty or the child could not
 * be forked. */
int wm_spawn_command(const char *command);

/* Open the terminal found by wm_terminal_program(). */
int wm_spawn_terminal(void);

/* Open the terminal showing a file's contents and wait for Enter before it
   closes. Used to put a start-up log in front of the user when the WM could
   not start — a display manager leaves no other way to see it. */
int wm_spawn_terminal_displaying(const char *path);

#endif /* GNUCHANWM_SPAWN_H */
