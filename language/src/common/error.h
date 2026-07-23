#ifndef GCL_ERROR_H
#define GCL_ERROR_H

#include "types.h"

void error_at(const char *filename, const char *code, size_t line, size_t col, const char *msg, const char *source);
void warn_at(const char *filename, const char *code, size_t line, size_t col, const char *msg);

/* Set current source buffer for source line display in error messages */
void error_set_source(const char *source);

/* convenience: error with "<source>" as filename (old API — no source line) */
static inline void error(const char *code, size_t line, size_t col, const char *msg) {
    error_at("<source>", code, line, col, msg, NULL);
}
static inline void warn(const char *code, size_t line, size_t col, const char *msg) {
    warn_at("<source>", code, line, col, msg);
}

#endif
