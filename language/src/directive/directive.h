/* directive.h — GCL preprocessor directive handler */
#ifndef GCL_DIRECTIVE_H
#define GCL_DIRECTIVE_H
#include "../include/token.h"

/* Process a token stream and write output to file.
   Handles: #include, #lib, #extern, #define, #undef,
   #ifdef, #ifndef, #if, #elif, #else, #endif,
   #error, #pragma message, #line, comments, macro expansion.
   Returns 0 on success, -1 on error. */
int directive_process(TokenCtx *ctx, FILE *out);
#endif
