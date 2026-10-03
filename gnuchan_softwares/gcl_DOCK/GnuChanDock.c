/*
 * GnuChanDock.c — the entry point.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dock_core.h"

#define PROGRAM_NAME    "GnuChanDock"
#define PROGRAM_VERSION "1.0"

static void print_help(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("\n");
    printf("A strip of icons along the bottom of the screen: the settings\n");
    printf("icon and the terminal always, then the programs that are open.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s              show the dock\n", PROGRAM_NAME);
    printf("  %s --help       this note\n", PROGRAM_NAME);
    printf("  %s --version    the version\n", PROGRAM_NAME);
    printf("\n");
    printf("The settings are read from\n");
    printf("  ~/.config/GnuChanDock/GnuChanDock.py\n");
    printf("and every value has a default, so the dock runs whether or not\n");
    printf("that file is there.\n");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            return 0;
        }
        fprintf(stderr, "%s: unknown argument '%s'\n", PROGRAM_NAME, argv[i]);
        fprintf(stderr, "run '%s --help' for the usage\n", PROGRAM_NAME);
        return 2;
    }

    DockCore core;
    if (dock_core_init(&core) != 0) {
        return 1;
    }

    dock_core_run(&core);
    dock_core_shutdown(&core);
    return 0;
}
