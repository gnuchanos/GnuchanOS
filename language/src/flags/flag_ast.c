#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -ast requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("AST: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_ast_init(void) {
    Flag f = {
        .name = "ast",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Lex+parse file and output AST tree",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
