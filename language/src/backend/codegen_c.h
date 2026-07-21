#ifndef GCL_CODEGEN_C_H
#define GCL_CODEGEN_C_H

#include "types.h"

/* emit full compilable C source with main() wrapper */
void codegen_c_emit(AstNode *prog);

/* emit C source WITHOUT main() — for sub-module .c files */
void codegen_c_emit_source(AstNode *prog);

/* emit a .h header with only defines, extern decls, pragmas */
void codegen_c_emit_header(AstNode *prog, const char *include_guard_name);

#endif
