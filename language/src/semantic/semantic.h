#ifndef GCL_SEMANTIC_H
#define GCL_SEMANTIC_H

#include "ast.h"

/**
 * Run semantic analysis on the parsed AST.
 * Returns 0 on success, number of errors on failure.
 */
int semantic_analyze(GclAstNode *program);

#endif
