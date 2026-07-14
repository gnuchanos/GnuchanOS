/*
 * gcl — gcLang Compiler / Runner
 *
 * Entry point. Parses CLI flags via the modular flags/ system.
 * Each flag is a separate module in flags/ registered during startup.
 *
 * If no flags are given, an interactive shell (REPL) is launched.
 */

#include "flags/flags.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void run_shell(void) {
    printf("gcLang Interactive Shell\n");
    printf("Type 'help' for commands, 'exit' to quit.\n\n");

    char line[1024];

    for (;;) {
        printf("gcl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip empty lines */
        if (len == 0) continue;

        /* Handle built-in commands */
        if (strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) {
            printf("bye!\n");
            break;
        }

        if (strcmp(line, "help") == 0 || strcmp(line, "h") == 0 || strcmp(line, "?") == 0) {
            flag_print_help();
            continue;
        }

        /* Unknown command */
        printf("unknown command: %s\n", line);
    }
}

int main(int argc, char* argv[]) {
    /* register all flag modules */
    flags_init();

    /* No arguments → launch interactive shell */
    if (argc < 2) {
        run_shell();
        return 0;
    }

    /* process argv — each handler is called in order */
    FlagResult r = flag_process(argc, argv);

    /* deferred execution: -luarun, -dll, -so flags may have been set */
    if (r == FLAG_OK) {
        r = flag_luarun_execute();
    }
    /* deferred execution: -pyrun, -pydll, -pyso flags */
    if (r == FLAG_OK) {
        r = flag_pyrun_execute();
    }

    switch (r) {
        case FLAG_OK:
            return 0;
        case FLAG_EXIT:
            return 0;
        case FLAG_ERROR:
            return 1;
    }

    return 0;
}
