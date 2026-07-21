#ifndef GCL_CODEGEN_H
#define GCL_CODEGEN_H

#include "types.h"
#include <stdio.h>

typedef enum {
    MODE_LEXER,
    MODE_PARSER,
    MODE_AST,
    MODE_IR,
    MODE_CODEGEN,
    MODE_EXEC,
} CodegenMode;

typedef struct {
    CodegenMode mode;
    FILE       *output;       /* where to write output (stdout if NULL) */
    const char *base_name;    /* -o basename (no extension), NULL = no output */
    int         debug_flag;
} CodegenOpts;

/* shared output file for all codegen modules */
extern FILE *g_codegen_out;

void codegen_emit(AstNode *prog, CodegenOpts *opts);

#endif
