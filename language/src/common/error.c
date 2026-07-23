#include "error.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Current source buffer for extracting source lines */
static const char *g_error_source = NULL;

void error_set_source(const char *source) {
    g_error_source = source;
}

/* Extract the Nth line (1-based) from source into buf (max_len). Returns pointer to buf. */
static const char *get_source_line(const char *source, size_t target_line, char *buf, size_t max_len) {
    if (!source) return NULL;
    size_t current = 1;
    const char *line_start = source;
    while (current < target_line) {
        const char *nl = strchr(line_start, '\n');
        if (!nl) return NULL;
        line_start = nl + 1;
        current++;
    }
    const char *nl = strchr(line_start, '\n');
    size_t len;
    if (nl) {
        len = nl - line_start;
    } else {
        len = strlen(line_start);
    }
    if (len >= max_len) len = max_len - 1;
    memcpy(buf, line_start, len);
    buf[len] = '\0';
    /* Trim trailing whitespace for display */
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    return buf;
}

void error_at(const char *filename, const char *code, size_t line, size_t col, const char *msg, const char *source) {
    const char *src = source ? source : g_error_source;
    fprintf(stderr, "%serror[%s]%s: %s\n", CLR_RED, code, CLR_RESET, msg);
    fprintf(stderr, "  %s-->%s %s:%zu:%zu\n", CLR_DIM, CLR_RESET, filename ? filename : "<unknown>", line, col);
    fprintf(stderr, "   |\n");
    char line_buf[1024];
    if (get_source_line(src, line, line_buf, sizeof(line_buf))) {
        fprintf(stderr, "%s%zu |%s %s\n", CLR_DIM, line, CLR_RESET, line_buf);
    } else {
        fprintf(stderr, "%zu | %s\n", line, "(source not available)");
    }
    fprintf(stderr, "   | %*s%s^ %s%s\n", (int)(col > 0 ? col - 1 : 0), "", CLR_RED, msg, CLR_RESET);
    exit(1);
}

void warn_at(const char *filename, const char *code, size_t line, size_t col, const char *msg) {
    fprintf(stderr, "%swarning[%s]%s: %s\n", CLR_YELLOW, code, CLR_RESET, msg);
    fprintf(stderr, "  %s-->%s %s:%zu:%zu\n", CLR_DIM, CLR_RESET, filename ? filename : "<unknown>", line, col);
    fprintf(stderr, "   |\n");
    char line_buf[1024];
    if (get_source_line(g_error_source, line, line_buf, sizeof(line_buf))) {
        fprintf(stderr, "%s%zu |%s %s\n", CLR_DIM, line, CLR_RESET, line_buf);
    } else {
        fprintf(stderr, "%zu | %s\n", line, "(source not available)");
    }
    fprintf(stderr, "   | %*s%s^ %s%s\n", (int)(col > 0 ? col - 1 : 0), "", CLR_YELLOW, msg, CLR_RESET);
}
