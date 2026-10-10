/*
 * GnuChanSettings.c — the entry point.
 *
 * One command: open the control panel. Everything it does is done by the two
 * halves behind it — settings_ui.c for what a person does, settings_draw.c for
 * what they see — and this file is only the door.
 *
 * The settings of the panel ITSELF are not configurable and have no file: the
 * panel is the thing that edits the OTHER programs' files, and a settings
 * program whose own colours lived in a file it was editing would be a knot
 * worth not tying. Its palette is in settings_style.c, in one place.
 */
#include <stdio.h>
#include <string.h>

#include "settings_ui.h"

#define PROGRAM_NAME    "GnuChanSettings"
#define PROGRAM_VERSION "1.0"

static void print_help(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("\n");
    printf("The desktop's control panel: change the settings of the Gnuchan\n");
    printf("programs by clicking, not by editing their files. A change is\n");
    printf("kept on screen until Save writes it, so nothing on disk changes\n");
    printf("until the button is pressed.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s              open the panel\n", PROGRAM_NAME);
    printf("  %s --help       this note\n", PROGRAM_NAME);
    printf("  %s --version    the version\n", PROGRAM_NAME);
    printf("\n");
    printf("Each program's settings live under ~/.config, in the file the\n");
    printf("program itself reads; this panel is a window over those files.\n");
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

    SettingsUi ui;
    if (settings_ui_open(&ui) != 0) {
        return 1;
    }

    settings_ui_run(&ui);
    settings_ui_close(&ui);
    return 0;
}
