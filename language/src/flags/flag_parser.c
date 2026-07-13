#include "flags.h"
#include <stdio.h>

/*
  gcl -parser file.gcsf   run parser and output AST
*/

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -parser requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    printf("Parser: %s\n", argv[*i]);
    return FLAG_EXIT;
}

void flag_parser_init(void) {
    Flag f = {
        .name = "parser",
        .alias = NULL,
        .description = "Run parser on file and output AST",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
