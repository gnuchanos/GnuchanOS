#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -lextend requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Extension path: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_lextend_init(void) {
    Flag f = {
        .name = "lextend",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Add extension search path",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
