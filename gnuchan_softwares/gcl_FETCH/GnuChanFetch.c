/*
 * GnuChanFetch.c — the entry point.
 *
 * GnuChanFetch is a small program in the shape of neofetch: it prints a short
 * set of facts about the machine it is run on, with the GnuchanOS logo drawn
 * beside them. It is configured by a script — see fetch_config.h for the shape,
 * and GnuChanFetch_config/GnuChanFetch.py for the file that ships — and it is a
 * C program, not a shell script, so it does the reading itself rather than
 * calling out to uname, free and dpkg for answers a file already holds.
 *
 * This file is deliberately thin. All it does is decide what to read, in what
 * order, and hand the result to fetch_draw(). Everything it calls is a module
 * with a single job: fetch_config reads the settings, fetch_image loads the
 * picture, fetch_info reads the machine, fetch_draw prints the whole thing.
 *
 *     GnuChanFetch            print the facts, with the picture on the left
 *     GnuChanFetch --help     a short usage note
 *     GnuChanFetch --version  the version line
 *
 * An argument that is not one of those is refused rather than ignored, because
 * a person who typed something meant something by it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fetch_config.h"
#include "fetch_image.h"
#include "fetch_draw.h"

#define PROGRAM_NAME    "GnuChanFetch"
#define PROGRAM_VERSION "1.0"

/* --- whether the terminal draws pictures ------------------------------------
 *
 * GnuChanTerm draws a picture as the image it is — see term_image.h in
 * gcl_TERMINAL — and every other terminal draws it as text, if at all. Which
 * one this is has to be asked of the ENVIRONMENT and of nothing else: there is
 * no sequence that asks "can you draw a picture" and no answer that would come
 * back before the output has to start.
 *
 * The test is the two names GnuChanTerm sets for the shell it starts — TERM is
 * `gcl-256color` and TERM_PROGRAM is `GnuChanTerm`. Either one is enough, and
 * both are checked because a program may be run with only one of them forward
 * (a `sudo` that drops TERM_PROGRAM, a wrapper that rewrites TERM).
 *
 * Anything else — tmux, an xterm, a plain console — is answered with 0, and the
 * block-fallback in fetch_draw.c draws the picture as the field of half-blocks
 * every terminal understands. That path is kept whole on purpose: it is what
 * makes this program work over ssh into a machine that has never heard of
 * GnuChanTerm, and it is the reason the graphics path can be opted into rather
 * than assumed. */
static int terminal_draws_pictures(void) {
    const char *term = getenv("TERM");
    if (term != NULL && strcmp(term, "gcl-256color") == 0) {
        return 1;
    }
    const char *program = getenv("TERM_PROGRAM");
    if (program != NULL && strcmp(program, "GnuChanTerm") == 0) {
        return 1;
    }
    return 0;
}

/* --- the usage note -------------------------------------------------------- */

static void print_help(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("\n");
    printf("Print facts about this machine, with the GnuchanOS logo beside\n");
    printf("them — a fetch program in the shape of neofetch.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s              print the facts\n", PROGRAM_NAME);
    printf("  %s --help       this note\n", PROGRAM_NAME);
    printf("  %s --version    the version\n", PROGRAM_NAME);
    printf("\n");
    printf("The settings are read from\n");
    printf("  ~/.config/GnuChanFetch/GnuChanFetch.py\n");
    printf("and every value has a default, so the program runs whether or\n");
    printf("not that file is there. The picture it draws is the one that\n");
    printf("file names with Image=, \".png\" by default.\n");
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

    FetchConfig config;
    if (fetch_config_load_default(&config) != 0) {
        /* No settings file is normal, and the defaults are already laid down;
           only a file that could not be READ is worth a word, and it goes to
           stderr so it cannot corrupt the output. */
        if (config.error[0] && strcmp(config.error,
                                       "no settings file was found") != 0) {
            fprintf(stderr, "%s: %s\n", PROGRAM_NAME, config.error);
            fprintf(stderr, "%s: using the built-in defaults\n", PROGRAM_NAME);
        }
    }

    /* The picture is the one the settings name. A picture that will not load is
       not a failure of the whole program: the facts are still printed, just
       without a picture beside them. */
    FetchImage image;
    memset(&image, 0, sizeof(image));
    if (config.image[0]) {
        if (fetch_image_load(&image, config.image, config.image_rows) != 0) {
            fprintf(stderr, "%s: drawing the facts without a picture\n",
                    PROGRAM_NAME);
        }
    }

    fetch_draw(&config, &image, terminal_draws_pictures());
    fetch_image_free(&image);
    return 0;
}
