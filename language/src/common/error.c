#include "error.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>

void error(const char *code, size_t line, size_t col, const char *msg) {
    fprintf(stderr, "%serror[%s]%s: %s\n", CLR_RED, code, CLR_RESET, msg);
    fprintf(stderr, "  %s-->%s <source>:%zu:%zu\n", CLR_DIM, CLR_RESET, line, col);
    fprintf(stderr, "   |\n");
    fprintf(stderr, "%zu | \n", line);
    fprintf(stderr, "   | %*s%s^%s\n", (int)col, "", CLR_RED, CLR_RESET);
    exit(1);
}

void warn(const char *code, size_t line, size_t col, const char *msg) {
    fprintf(stderr, "%swarning[%s]%s: %s\n", CLR_YELLOW, code, CLR_RESET, msg);
    fprintf(stderr, "  %s-->%s <source>:%zu:%zu\n", CLR_DIM, CLR_RESET, line, col);
    fprintf(stderr, "   |\n");
    fprintf(stderr, "%zu | \n", line);
    fprintf(stderr, "   | %*s%s^%s\n", (int)col, "", CLR_YELLOW, CLR_RESET);
}
