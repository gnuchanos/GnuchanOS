/* lexer.h — GCL preprocessor lexer */
#ifndef GCL_LEXER_H
#define GCL_LEXER_H
#include "../include/token.h"

TokenCtx lexer_run(const char *path);
#endif
