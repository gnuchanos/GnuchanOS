// shell.c — GCL interactive shell
#include "shell/shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_INPUT 1024

static void show_help(void) {
    printf(
        "GCL Interactive Shell\n"
        "  help          — show this help\n"
        "  exit          — quit the shell\n"
        "\n"
        "  Flags (also available in CLI mode):\n"
        "    -version     — show GCL version\n"
        "    -lexer       — run lexer on a file\n"
        "    -parser      — run parser on a file\n"
        "    -ast         — dump AST\n"
        "    -ir          — dump IR\n"
        "    -codegen     — generate C code\n"
        "    -debug       — enable debug output\n"
        "    -linclude    — add include path\n"
        "    -llib        — add library path\n"
        "    -lextend     — add extension module\n"
        "    -o <file>    — output file\n"
    );
}

void gcl_shell_run(void) {
    char line[MAX_INPUT];

    printf("GCL Shell (type 'help' for commands, 'exit' to quit)\n");

    for (;;) {
        printf("gcl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;

        // strip trailing newline
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("bye.\n");
            break;
        }

        if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
            show_help();
            continue;
        }

        // TODO: parse line as if it were CLI args, feed to flags/pipeline
        printf("gcl: unknown command '%s' (try 'help')\n", line);
    }
}
