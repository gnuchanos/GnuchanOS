/*
 * runner_apps.c — the list of programs: where it is filled from, and how a
 * written command line becomes the words to run.
 *
 * Three jobs live here, and they are one file because they are one thing: the
 * table of programs. It is made and freed here, filled from the directories
 * and from the script's own additions here, and the command line that a
 * program carries is split into its words here.
 *
 * The scan is deliberately shallow and bounded. It reads one directory at a
 * time, takes only files whose names end in .desktop, and stops at a ceiling:
 * the launcher is for starting what a person uses, and a machine with a
 * thousand entries in the directory is a machine whose list wants narrowing,
 * not a machine to read a thousand files on every key press.
 *
 * Reading is done once, at start, and the list is then held in memory. A
 * launcher that re-read the directory on every query would be a launcher that
 * stutters on the first letter, which is the one moment it must not.
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_apps.h"
#include "runner_desktop.h"

/* --- the list ------------------------------------------------------------- */

void runner_apps_init(RunnerProgramList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void runner_apps_free(RunnerProgramList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Room for one more program, growing by a step rather than one at a time: the
   scan adds several hundred in a row, and one reallocation each would be
   hundreds of copies of everything already read. */
static RunnerProgram *list_grow(RunnerProgramList *list) {
    if (list->count < list->capacity) {
        return &list->items[list->count];
    }
    int capacity = list->capacity > 0 ? list->capacity * 2 : 64;
    RunnerProgram *grown = realloc(list->items,
                                   (size_t)capacity * sizeof(RunnerProgram));
    if (!grown) {
        return NULL;
    }
    list->items = grown;
    list->capacity = capacity;
    return &list->items[list->count];
}

int runner_apps_add(RunnerProgramList *list, const char *name,
                    const char *comment, const char *command) {
    if (!list || !name || !name[0] || !command || !command[0]) {
        return -1;
    }
    if (list->count >= RUNNER_MAX_DESKTOP_ENTRIES + RUNNER_MAX_EXTRA_PROGRAMS) {
        return -1;
    }

    /* One name is one program. A name already in the list is not added again:
       the .desktop file is the same one seen twice, or the script named a
       program the system already has, and a launcher that showed both would
       show the same program twice with no way to tell them apart. */
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return -1;
        }
    }

    RunnerProgram *program = list_grow(list);
    if (!program) {
        return -1;
    }
    memset(program, 0, sizeof(*program));
    snprintf(program->name, sizeof(program->name), "%s", name);
    snprintf(program->comment, sizeof(program->comment), "%s",
             comment ? comment : "");

    if (runner_apps_split_command(program, command) != 0) {
        return -1;
    }
    list->count++;
    return 0;
}

/* --- a written command line becomes words --------------------------------- */

/* Split a command line into the words to run.
 *
 * The rule is the shell's own, kept small: white space separates words, a
 * double or single quote groups a word that may have spaces in it, and a
 * backslash escapes the next character. Nothing is expanded — no variables,
 * no globs — because this is the command a .desktop file or a settings script
 * wrote, and expanding something the writer did not write would be inventing a
 * command they did not ask for. */
int runner_apps_split_command(RunnerProgram *program, const char *command) {
    program->count = 0;
    if (!command) {
        return -1;
    }

    const char *cursor = command;
    while (*cursor && program->count < RUNNER_MAX_COMMAND_WORDS - 1) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char *word = program->command[program->count];
        int written = 0;
        char quote = '\0';

        while (*cursor) {
            char c = *cursor;
            if (quote == '\0' && (c == ' ' || c == '\t')) {
                break;
            }
            if (c == '\\' && cursor[1] != '\0') {
                cursor++;
                c = *cursor;
            } else if (quote == '\0' && (c == '"' || c == '\'')) {
                quote = c;
                cursor++;
                continue;
            } else if (c == quote) {
                quote = '\0';
                cursor++;
                continue;
            }
            if (written < RUNNER_TEXT_LENGTH - 1) {
                word[written++] = c;
            }
            cursor++;
        }
        word[written] = '\0';

        if (written > 0) {
            program->count++;
        }
    }

    return program->count > 0 ? 0 : -1;
}

/* --- the scan ------------------------------------------------------------- */

/* Whether a directory entry's name ends in .desktop. An entry that does not is
   not a program's entry, and there are far more of those than programs. */
static int has_desktop_suffix(const char *name) {
    size_t length = strlen(name);
    const char *suffix = ".desktop";
    size_t suffix_length = strlen(suffix);
    if (length <= suffix_length) {
        return 0;
    }
    return strcmp(name + length - suffix_length, suffix) == 0;
}

/* Read one directory and add every program its .desktop files describe. A
   directory that cannot be opened is skipped with nothing on stderr: a
   machine with no /usr/share/applications has nothing installed there, which
   is not a fault to report. */
static void scan_directory(RunnerProgramList *list, const char *path,
                           const RunnerConfig *config) {
    DIR *directory = opendir(path);
    if (!directory) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (list->count >= RUNNER_MAX_DESKTOP_ENTRIES) {
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;       /* . and .. and anything hidden */
        }
        if (!has_desktop_suffix(entry->d_name)) {
            continue;
        }

        char full[RUNNER_TEXT_LENGTH * 2];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        RunnerProgram program;
        if (runner_desktop_read(full, &program, config->show_no_display,
                                config->show_hidden) != 0) {
            continue;
        }

        /* The list's own add, so a name seen twice — the system's entry and
           the user's overriding one — is kept once. */
        runner_apps_add(list, program.name, program.comment,
                        program.command[0]);
        /* The command is a set of words, and runner_apps_add takes a written
           line, so the first word is not the whole command. The program is
           copied in directly instead when the line would lose its arguments. */
        if (list->count > 0 &&
            strcmp(list->items[list->count - 1].name, program.name) == 0) {
            /* Replace what the name-based add could not carry: the whole
               command, exactly as the file wrote it after the field codes
               were taken out. */
            list->items[list->count - 1] = program;
        }
    }
    closedir(directory);
}

/* The directories the scan reads: what the script named, or the default when
   it named none. */
static int config_has_dirs(const RunnerConfig *config) {
    return config && config->desktop_dir_count > 0;
}

int runner_apps_load(RunnerProgramList *list, const RunnerConfig *config) {
    if (!list || !config) {
        return 0;
    }

    if (config_has_dirs(config)) {
        for (int i = 0; i < config->desktop_dir_count; i++) {
            scan_directory(list, config->desktop_dirs[i], config);
        }
    } else {
        scan_directory(list, RUNNER_DEFAULT_SYSTEM_DIRS, config);

        /* The user's own directory as well: a program installed by hand puts
           its entry under the home, and a launcher that read only the system's
           would miss everything the person added themselves. */
        const char *home = getenv("HOME");
        if (home && home[0]) {
            char user_dir[RUNNER_TEXT_LENGTH * 2];
            snprintf(user_dir, sizeof(user_dir),
                     "%s/.local/share/applications", home);
            scan_directory(list, user_dir, config);
        }
    }

    /* The script's own programs last, so they are in the list whatever the
       directories held — a program the person wrote down is a program they
       mean to be able to start. */
    for (int i = 0; i < config->extra_program_count; i++) {
        runner_apps_add(list, config->extra_programs[i].name, "",
                        config->extra_programs[i].command);
    }

    return list->count;
}
