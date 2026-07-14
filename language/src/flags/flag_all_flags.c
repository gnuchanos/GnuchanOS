#include "flags.h"
#include <stdio.h>
#include <string.h>

static FlagResult handler(int argc, char* argv[], int* i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -all_flags requires a file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    const char* file = argv[*i];
    printf("Pipeline: %s", file);

    if (*i + 2 < argc && strcmp(argv[*i + 1], "-o") == 0) {
        const char* output = argv[*i + 2];
        printf(" -o %s", output);
        *i += 2;
    }

    printf("\n");
    return FLAG_EXIT;
}

void flag_all_flags_init(void) {
    Flag f = {
        .name = "all_flags",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Full pipeline: file.gcsf -o path/output",
        .handler = handler,
        .needs_value = true
    };
    flag_register(f);
}
