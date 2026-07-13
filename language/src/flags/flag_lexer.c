#include "flags.h"
#include <stdio.h>

/*
  gcl -lexer file.gcsf   run lexer on file and output tokens
*/

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -lexer requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Lexer: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_lexer_init(void) {
    Flag f = {
        .name = "lexer",
        .alias = NULL,
        .description = "Run lexer on file and output tokens",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
