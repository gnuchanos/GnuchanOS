#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -linclude requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Include path: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_linclude_init(void) {
    Flag f = {
        .name = "linclude",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Add include search path (like gcc -I)",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
