/*
 * runner_apps.h — the programs a launcher can start.
 *
 * A program is a name to search for and a command to run. Where the two come
 * from is not this header's business: most are read off the .desktop files a
 * package installs, and some a settings script adds itself. Both end up in the
 * same table, because a launcher does not care which kind a program is — only
 * that typing its name finds it.
 *
 * The command is kept as the argv a .desktop file writes — `Exec=firefox
 * --new-window %u` — with the field codes the specification defines stripped
 * out, because this launcher never passes it a file. What is left is what to
 * run, split at the spaces the line was written with.
 */
#ifndef GNUCHANRUNNER_APPS_H
#define GNUCHANRUNNER_APPS_H

#include "runner_config.h"

/* How many words one program's command may hold: the program, its arguments,
   and the argv[0] slot the exec call needs at the end. A longer command is
   truncated rather than refused — a program with a very long command line is
   still a program. */
#define RUNNER_MAX_COMMAND_WORDS 32

typedef struct RunnerProgram {
    /* The name shown and searched: the .desktop Name, or what the script
       called the program. */
    char name[RUNNER_TEXT_LENGTH];

    /* The line under the name in the list, when the entry had one: the
       .desktop Comment, which is what the program says about itself. Empty
       for a program a script added. */
    char comment[RUNNER_TEXT_LENGTH];

    /* What to run, as the words of a command line. `count` is how many there
       are, and the array is NUL-terminated for execvp's sake. */
    char command[RUNNER_MAX_COMMAND_WORDS][RUNNER_TEXT_LENGTH];
    int count;

    /* Whether the program takes no display of its own — Terminal=true in the
       .desktop file, which means "run this in a terminal window". Kept as a
       flag rather than resolved here, because this file has no display and
       the launcher's own choice of terminal is what it lands on. */
    int in_terminal;
} RunnerProgram;

/* The whole list, and how many of it are filled. The entries are held
   separately from their names because one program is in it only once and the
   scan may see it twice: the same .desktop file in two directories, the
   system's and the user's. */
typedef struct RunnerProgramList {
    RunnerProgram *items;
    int count;
    int capacity;
} RunnerProgramList;

/* Make an empty list, and free it again. runner_apps_free() releases every
   command inside every program as well as the array itself. */
void runner_apps_init(RunnerProgramList *list);
void runner_apps_free(RunnerProgramList *list);

/* Scan every directory the config names, then add every program the config
   adds itself. A directory that cannot be read is skipped — a machine with no
   /usr/share/applications is a machine with nothing installed there, not a
   launcher that refuses to start. Returns the number of programs found. */
int runner_apps_load(RunnerProgramList *list, const RunnerConfig *config);

/* Add one program by hand, the way a script does. Returns 0 when it was
   added, -1 when the name or the command was empty. */
int runner_apps_add(RunnerProgramList *list, const char *name,
                    const char *comment, const char *command);

/* Split a written command line into the words of a program. White space
   separates the words; a double or single quote groups them; a backslash
   escapes the next character. The result is written into the program's own
   array, and its `count` is set. Returns 0 when at least one word was read. */
int runner_apps_split_command(RunnerProgram *program, const char *command);

#endif /* GNUCHANRUNNER_APPS_H */
