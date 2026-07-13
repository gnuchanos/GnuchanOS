#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -codegen requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Codegen: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_codegen_init(void) {
    Flag f = {
        .name = "codegen",
        .alias = NULL,
        .description = "Lex+parse+codegen and output C code",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
