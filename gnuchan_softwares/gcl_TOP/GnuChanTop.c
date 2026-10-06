/*
 * GnuChanTop.c — the entry point.
 *
 * This file is deliberately thin: it parses the two arguments, enters the
 * terminal, runs the loop, and restores the terminal. Everything it calls is a
 * module with a single job, and the restoring is the one thing that has to
 * happen on every path out — a normal quit, a signal, an error part way — which
 * is why the terminal is entered here and left here and nowhere else.
 *
 *     GnuChanTop            run the monitor
 *     GnuChanTop --help     a short usage note
 *     GnuChanTop --version  the version line
 *
 * An argument that is not one of those is refused rather than ignored, because a
 * person who typed something meant something by it.
 */
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "top_ui.h"
#include "top_term.h"

#define PROGRAM_NAME    "GnuChanTop"
#define PROGRAM_VERSION "1.0"

/* --- the usage note -------------------------------------------------------- */

static void print_help(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("\n");
    printf("A system monitor: CPU and GPU usage across the top, the process\n");
    printf("list below — the shape btop has.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s              run the monitor\n", PROGRAM_NAME);
    printf("  %s --help       this note\n", PROGRAM_NAME);
    printf("  %s --version    the version\n", PROGRAM_NAME);
    printf("\n");
    printf("Keys:\n");
    printf("  up/down      move the process cursor\n");
    printf("  page up/down move by a screen\n");
    printf("  home/end     jump to the top or the bottom\n");
    printf("  k            kill the selected process (asks first)\n");
    printf("  s            change the sort column\n");
    printf("  q, Esc       quit\n");
    printf("\n");
    printf("The settings are read from\n");
    printf("  ~/.config/GnuChanTop/GnuChanTop.py\n");
    printf("and every value has a default, so the program runs whether or not\n");
    printf("that file is there.\n");
}

/* --- the entry point ------------------------------------------------------- */

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

    /* The locale has to be set for the braille characters to be written as the
       UTF-8 they are and for the terminal to read the width of a cell. Without
       it the graph is a row of question marks. */
    setlocale(LC_ALL, "");

    if (top_term_enter() != 0) {
        fprintf(stderr, "%s: standard input is not a terminal\n", PROGRAM_NAME);
        return 1;
    }

    TopState state;
    if (top_ui_init(&state) != 0) {
        top_term_restore();
        fprintf(stderr, "%s: could not read the machine's state\n", PROGRAM_NAME);
        return 1;
    }

    top_ui_run(&state);

    top_procs_free(&state.procs);
    top_term_restore();
    return 0;
}
