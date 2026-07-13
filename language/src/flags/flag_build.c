#include "flags.h"
#include <stdio.h>
#include <string.h>

/*
  gcl -build --wasm       Generate WASM
  gcl -build --native     Generate native binary
*/

static FlagResult handler(int argc, char* argv[], int* i) {
    printf("Build mode");

    if (*i + 1 < argc) {
        const char* next = argv[*i + 1];
        if (strcmp(next, "--wasm") == 0) {
            printf(" [wasm]");
            (*i)++;
        } else if (strcmp(next, "--native") == 0) {
            printf(" [native]");
            (*i)++;
        }
    }

    printf("\n");
    return FLAG_EXIT;
}

void flag_build_init(void) {
    Flag f = {
        .name = "build",
        .alias = NULL,
        .description = "Build project (--wasm, --native)",
        .handler = handler,
        .needs_value = false
    };
    flag_register(f);
}
