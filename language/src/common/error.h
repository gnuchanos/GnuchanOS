#ifndef GCL_ERROR_H
#define GCL_ERROR_H

#include "types.h"

void error_at(const char *filename, const char *code, size_t line, size_t col, const char *msg);
void warn_at(const char *filename, const char *code, size_t line, size_t col, const char *msg);

/* convenience: error with "<source>" as filename (old API) */
static inline void error(const char *code, size_t line, size_t col, const char *msg) {
    error_at("<source>", code, line, col, msg);
}
static inline void warn(const char *code, size_t line, size_t col, const char *msg) {
    warn_at("<source>", code, line, col, msg);
}

#endif
