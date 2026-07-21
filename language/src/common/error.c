#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void error(const char *code, size_t line, size_t col, const char *msg) {
    fprintf(stderr, "error[%s]: %s\n", code, msg);
    fprintf(stderr, "  --> <source>:%zu:%zu\n", line, col);
    fprintf(stderr, "   |\n");
    fprintf(stderr, "%zu | \n", line);
    fprintf(stderr, "   | %*s^\n", (int)col, "");
    exit(1);
}
