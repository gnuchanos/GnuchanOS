#ifndef GCL_ERROR_H
#define GCL_ERROR_H

#include "types.h"

void error(const char *code, size_t line, size_t col, const char *msg);

#endif
