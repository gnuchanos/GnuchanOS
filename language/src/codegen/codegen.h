#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parser/ast.h"
#include <stdio.h>

void codegen_emit(AstNode *ast, FILE *out);

#endif
