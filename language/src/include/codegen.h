#ifndef GCL_CODEGEN_H
#define GCL_CODEGEN_H

#include "gcl.h"
#include "ast.h"

void codegen_generate(FILE *out, Node *prog);

#endif
