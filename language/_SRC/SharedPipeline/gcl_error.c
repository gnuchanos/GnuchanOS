/*
 * gcl_error.c — GCL hata tipleri.
 */

#include "gcl_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct GclError {
    int kind;         /* 0=io, 1=alloc, 2=runtime, 3=parse */
    char *msg;
    int line;
    int col;
    char *formatted;
};

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

GclError *make_error(int kind, const char *msg, int line, int col) {
    GclError *e = (GclError *)calloc(1, sizeof(GclError));
    if (!e) return NULL;
    e->kind = kind;
    e->msg = dup_str(msg);
    e->line = line;
    e->col = col;
    return e;
}

GclError *gcl_error_io(const char *msg) { return make_error(0, msg, 0, 0); }
GclError *gcl_error_alloc(const char *msg) { return make_error(1, msg, 0, 0); }
GclError *gcl_error_runtime(const char *msg) { return make_error(2, msg, 0, 0); }
GclError *gcl_error_parse(const char *msg, int line, int col) {
    return make_error(3, msg, line, col);
}

void gcl_error_free(GclError *err) {
    if (!err) return;
    free(err->msg);
    free(err->formatted);
    free(err);
}

const char *gcl_error_msg(const GclError *err) {
    return err ? (err->msg ? err->msg : "") : "";
}

char *gcl_error_format(GclError *err) {
    if (!err) return NULL;
    if (err->formatted) return err->formatted;
    char buf[1024];
    switch (err->kind) {
        case 0: snprintf(buf, sizeof(buf), "IO error: %s", err->msg ? err->msg : ""); break;
        case 1: snprintf(buf, sizeof(buf), "Alloc error: %s", err->msg ? err->msg : ""); break;
        case 2: snprintf(buf, sizeof(buf), "Runtime error: %s", err->msg ? err->msg : ""); break;
        case 3:
            if (err->line > 0)
                snprintf(buf, sizeof(buf), "Parse error at %d:%d: %s", err->line, err->col, err->msg ? err->msg : "");
            else
                snprintf(buf, sizeof(buf), "Parse error: %s", err->msg ? err->msg : "");
            break;
        default: snprintf(buf, sizeof(buf), "Error: %s", err->msg ? err->msg : ""); break;
    }
    err->formatted = dup_str(buf);
    return err->formatted;
}
