#ifndef GCL_CODEGEN_H
#define GCL_CODEGEN_H

#include <stdio.h>
#include "ast.h"

// ============================================================
// GCL Code Generator — Transpile GCL AST to C source
// ============================================================

typedef struct {
    FILE *out;
    int   indent_level;
    int   need_semicolon;
} GclCodegen;

void codegen_init(GclCodegen *cg, FILE *out);
void codegen_generate(GclCodegen *cg, GclAstNode *program);
void codegen_generate_to_file(GclAstNode *program, const char *filename);

#endif // GCL_CODEGEN_H
