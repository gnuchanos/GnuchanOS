#include "flags.h"
#include <stdio.h>
#include <string.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -wasm requires a mode argument (raylib, binding, export)\n");
        return FLAG_ERROR;
    }
    (*i)++;
    const char* mode = argv[*i];

    if (strcmp(mode, "raylib") == 0) {
        printf("WASM: raylib\n");
    } else if (strcmp(mode, "binding") == 0) {
        printf("WASM: binding\n");
    } else if (strcmp(mode, "export") == 0) {
        printf("WASM: export\n");
    } else {
        fprintf(stderr, "error: unknown wasm mode '%s' (use: raylib, binding, export)\n", mode);
        return FLAG_ERROR;
    }
    return FLAG_EXIT;
}

void flag_wasm_init(void) {
    Flag f = {
        .name = "wasm",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "WASM mode (raylib, binding, export)",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
