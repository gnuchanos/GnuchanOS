#include "flags.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FLAGS 64

static Flag g_flags[MAX_FLAGS];
static int  g_flag_count = 0;

void flag_register(const Flag f) {
    if (g_flag_count >= MAX_FLAGS) {
        fprintf(stderr, "error: too many flags registered\n");
        return;
    }
    g_flags[g_flag_count++] = f;
}

FlagResult flag_process(int argc, char* argv[]) {
    if (argc < 2) {
        /* no flags — print help and exit */
        flag_print_help();
        return FLAG_EXIT;
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        /* skip non-flag arguments (positional — e.g. filename) */
        if (arg[0] != '-') continue;

        const char* flag_name = arg + 1;  /* skip leading '-' */

        /* handle -- double-dash forms */
        if (arg[1] == '-') {
            if (arg[2] == '\0') continue; /* bare '--' ends flag parsing */
            flag_name = arg + 2;
        }

        const Flag* f = flag_find(flag_name);
        if (!f) {
            fprintf(stderr, "error: unknown flag '-%s'\n", flag_name);
            return FLAG_ERROR;
        }

        FlagResult r = f->handler(argc, argv, &i);
        if (r != FLAG_OK) return r;
    }

    return FLAG_OK;
}

const Flag* flag_find(const char* name) {
    for (int i = 0; i < g_flag_count; i++) {
        if (strcmp(g_flags[i].name, name) == 0) return &g_flags[i];
        if (g_flags[i].alias && strcmp(g_flags[i].alias, name) == 0)
            return &g_flags[i];
    }
    return NULL;
}

void flag_print_help(void) {
    printf("gcLang Compiler / Runner\n");
    printf("usage: gcl [flags] [file]\n\n");
    printf("flags:\n");
    for (int i = 0; i < g_flag_count; i++) {
        const Flag* f = &g_flags[i];
        printf("  ");
        if (f->alias) {
            printf("-%s, ", f->alias);
        } else {
            printf("    ");
        }
        printf("--%-14s  %s\n", f->name, f->description);
    }
    printf("\n");
}
