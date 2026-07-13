#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -llib requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Library path: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_llib_init(void) {
    Flag f = {
        .name = "llib",
        .alias = NULL,
        .description = "Add library search path (like gcc -L)",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
