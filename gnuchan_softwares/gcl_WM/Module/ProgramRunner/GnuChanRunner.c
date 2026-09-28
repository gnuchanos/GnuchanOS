/*
 * GnuChanRunner.c — the entry point.
 *
 * The launcher is one window and one purpose: type a few letters, press Enter,
 * and the program starts. Everything it is made of is in the files beside this
 * one — the settings are read by runner_config.c, the programs by
 * runner_apps.c, the match by runner_match.c, the window by runner_ui.c — and
 * what is left for this file is the order to call them in and what to do with
 * the answer.
 *
 *     GnuChanRunner            open the launcher
 *     GnuChanRunner --config F read the settings at F
 *     GnuChanRunner --list     print what the scan found and exit
 *     GnuChanRunner --version  print the version and exit
 *
 * It is started the way any program is: from a key binding in GnuChanWM's own
 * settings script —
 *
 *     gcl_key.MultiKey(keys=[super_key1, "d"],
 *                      action=gcl_spawn.RunProgram(command="GnuChanRunner"))
 *
 * — or from a terminal, which is how it is tried while it is being written.
 * It knows nothing about the window manager and does not need one: the display
 * is the only thing it asks for.
 *
 * The program is started by this file and not by the window, because that is
 * the difference between a launcher that can be tried by running it and one
 * that starts a program the moment it is opened. The window's whole job is to
 * say what was chosen; the running of it is here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_apps.h"
#include "runner_config.h"
#include "runner_launch.h"
#include "runner_ui.h"

#define GNUCHANRUNNER_VERSION "0.1.0"

/* Print every program the scan finds, one per line, in the form
   "command  --  name". This is what --list is for: the one way to see what the
   launcher will offer without opening a window, which is what makes a machine
   where a program is missing something that can be looked at rather than
   guessed at. */
static int list_programs(const char *config_path) {
    RunnerConfig config;
    char found[RUNNER_TEXT_LENGTH * 2];
    const char *path = config_path;

    if (!path || !path[0]) {
        path = runner_config_path(found, sizeof(found));
    }
    if (runner_config_load(&config, path) != 0) {
        fprintf(stderr, "gnuchanrunner: %s; using the defaults\n",
                config.error);
    }

    RunnerProgramList list;
    runner_apps_init(&list);
    runner_apps_load(&list, &config);

    for (int i = 0; i < list.count; i++) {
        printf("%-28s  %s\n", list.items[i].command[0], list.items[i].name);
    }
    fprintf(stderr, "gnuchanrunner: %d program(s)\n", list.count);

    runner_apps_free(&list);
    return 0;
}

/* Read the settings, open the window, and run whatever was chosen.
 *
 * The order is the whole of this file: read, open, wait, and — when something
 * was chosen — start it after the window is gone. Starting it after is not
 * tidiness: the child is forked from this process, and a window that is still
 * holding the keyboard at the moment of the fork is a window the child
 * inherits the grab of. Closing first means the fork happens with nothing held
 * and nothing on screen.
 */
static int run_launcher(const char *config_path) {
    RunnerUi ui;

    if (runner_ui_open(&ui, config_path) != 0) {
        runner_ui_close(&ui);
        return 1;
    }

    int chosen = runner_ui_run(&ui);

    /* What to run is read out before the close, because the close frees the
       list the chosen index points into. The command line is copied for the
       same reason: it lives in the ui's own buffer. */
    int program_index = ui.launch_selected;
    char command[RUNNER_MAX_QUERY];
    snprintf(command, sizeof(command), "%s", ui.launch_command);

    RunnerProgram chosen_program;
    int have_program = 0;
    if (chosen == 0 && program_index >= 0 &&
        program_index < ui.programs.count) {
        chosen_program = ui.programs.items[program_index];
        have_program = 1;
    }

    Display *display = ui.display;
    if (have_program) {
        runner_launch_program(display, &chosen_program);
    } else if (chosen == 0 && command[0]) {
        runner_launch_command(display, command);
    }

    runner_ui_close(&ui);
    return 0;
}

static void print_help(void) {
    printf("usage: GnuChanRunner [--config FILE] [--list] [--version] [--help]\n"
           "\n"
           "Opens the program launcher: type a few letters of a program's\n"
           "name, press Enter, and it starts. Escape closes it.\n"
           "\n"
           "  --config FILE  read the settings at FILE instead of the one\n"
           "                 under ~/.config/GnuChanRunner/\n"
           "  --list         print the programs the scan found and exit\n"
           "  --version      print the version and exit\n");
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    int list_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("GnuChanRunner %s\n", GNUCHANRUNNER_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--list") == 0) {
            list_only = 1;
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "gnuchanrunner: --config needs a file name\n");
                return 2;
            }
            config_path = argv[++i];
            continue;
        }
        fprintf(stderr, "gnuchanrunner: unknown option '%s'\n", argv[i]);
        print_help();
        return 2;
    }

    if (list_only) {
        return list_programs(config_path);
    }
    return run_launcher(config_path);
}
