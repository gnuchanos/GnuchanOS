#include "flags.h"
#include <stdio.h>
#include <string.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -debug requires a mode (run, tokens, ast)\n");
        return FLAG_ERROR;
    }
    (*i)++;
    const char* mode = argv[*i];

    if (strcmp(mode, "run") == 0) {
        printf("Debug: run\n");
    } else if (strcmp(mode, "tokens") == 0) {
        printf("Debug: tokens\n");
    } else if (strcmp(mode, "ast") == 0) {
        printf("Debug: ast\n");
    } else {
        fprintf(stderr, "error: unknown debug mode '%s' (use: run, tokens, ast)\n", mode);
        return FLAG_ERROR;
    }
    return FLAG_EXIT;
}

void flag_debug_init(void) {
    Flag f = {
        .name = "debug",
        .alias = "d",
        .category = "Compiler Pipeline",
        .description = "Debug mode (run, tokens, ast)",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
