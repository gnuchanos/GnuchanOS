#include "codegen.h"
#include "codegen_ast.h"
#include "codegen_ir.h"
#include "codegen_c.h"
#include "codegen_debug.h"

FILE *g_codegen_out = NULL;

void codegen_emit(AstNode *prog, CodegenOpts *opts) {
    g_codegen_out = opts->output ? opts->output : stdout;

    switch (opts->mode) {
    case MODE_LEXER:
        break;
    case MODE_PARSER:
    case MODE_AST:
        codegen_ast_emit(prog);
        break;
    case MODE_IR:
        codegen_ir_emit(prog);
        break;
    case MODE_CODEGEN:
        codegen_c_emit(prog);
        break;
    case MODE_EXEC:
        codegen_debug_emit(prog);
        break;
    }
}
