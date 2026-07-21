#ifndef GCL_CODEGEN_DEBUG_H
#define GCL_CODEGEN_DEBUG_H

#include "types.h"

/* resolve a name through the defines table */
const char *codegen_resolve(const char *name);

/* emit a single #debug statement to g_codegen_out */
void codegen_debug_emit(AstNode *prog);

#endif
