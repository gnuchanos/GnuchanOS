#include "flags.h"
#include <stdio.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    (void)argc; (void)argv; (void)i;
    flag_print_help();
    return FLAG_EXIT;
}

void flag_help_init(void) {
    Flag f = {
        .name = "help",
        .alias = "h",
        .description = "Show this help message",
        .handler = handler,
        .needs_value = false
    };
    flag_register(f);
}
