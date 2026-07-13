#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    (void)argc; (void)argv; (void)i;
    printf("gcLang version 1.0.0\n");
    printf("__GCLANG_VER__ = 100\n");
    return FLAG_EXIT;
}

void flag_version_init(void) {
    Flag f = {
        .name = "version",
        .alias = "v",
        .description = "Show version information",
        .handler = handler,
        .needs_value = false
    };
    flag_register(f);
}
