#include "flags.h"
#include <stdio.h>
#include <string.h>

/*
  gcl -run [file]            Run with IR Executor
  gcl -run --debug-ir        Run with IR instruction trace
  gcl -run file.gcsf         Run specific file
*/

static FlagResult handler(int argc, char* argv[], int* i) {
    printf("IR Executor mode");

    if (*i + 1 < argc) {
        const char* next = argv[*i + 1];
        if (next[0] == '-' && next[1] == '-') {
            if (strcmp(next + 2, "debug-ir") == 0) {
                printf(" [debug-ir]");
                (*i)++;
            }
        } else if (next[0] != '-') {
            printf(" file=%s", next);
            (*i)++;
        }
    }

    printf("\n");
    return FLAG_EXIT;
}

void flag_run_init(void) {
    Flag f = {
        .name = "run",
        .alias = NULL,
        .category = "Compiler Pipeline",
        .description = "Run with IR Executor (run.gcdata)",
        .handler = handler,
        .needs_value = false
    };
    flag_register(f);
}
