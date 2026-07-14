#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -ir requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("IR: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_ir_init(void) {
    Flag f = {
        .name = "ir",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Lex+parse+lower IR and output IR instructions",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
