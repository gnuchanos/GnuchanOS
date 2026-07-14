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
        /* no flags — launch interactive shell */
        return FLAG_OK;
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

    /* Collect unique categories in order of first appearance */
    const char *cats[16];
    int ncats = 0;
    for (int i = 0; i < g_flag_count; i++) {
        const char *c = g_flags[i].category;
        if (!c) continue;
        int found = 0;
        for (int j = 0; j < ncats; j++) {
            if (strcmp(cats[j], c) == 0) { found = 1; break; }
        }
        if (!found && ncats < 16) cats[ncats++] = c;
    }

    /* Flags without a category (always shown first) */
    int any_uncat = 0;
    for (int i = 0; i < g_flag_count; i++) {
        if (!g_flags[i].category) { any_uncat = 1; break; }
    }
    if (any_uncat) {
        printf("flags:\n");
        for (int i = 0; i < g_flag_count; i++) {
            const Flag* f = &g_flags[i];
            if (f->category) continue;
            printf("  ");
            if (f->alias)
                printf("-%s, ", f->alias);
            else
                printf("    ");
            printf("--%-14s  %s\n", f->name, f->description);
        }
        printf("\n");
    }

    /* Flags grouped by category */
    for (int ci = 0; ci < ncats; ci++) {
        printf("%s:\n", cats[ci]);
        for (int i = 0; i < g_flag_count; i++) {
            const Flag* f = &g_flags[i];
            if (!f->category || strcmp(f->category, cats[ci]) != 0) continue;
            printf("    ");
            if (f->alias)
                printf("-%s, ", f->alias);
            else
                printf("    ");
            printf("--%-14s  %s\n", f->name, f->description);
        }
        printf("\n");
    }
}
