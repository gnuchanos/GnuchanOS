#ifndef GCL_IR_BUILDER_H
#define GCL_IR_BUILDER_H

#include "ir.h"
#include "../parser/ast.h"
#include "../semantic/semantic.h"

/* ================================================================= */
/*  GCL IR Builder — AST → IR (3-Address Code) Translator            */
/* ================================================================= */

/* Context per function */
typedef struct {
    IRFunction   *ir_func;
    IRBlock      *current_block;
    IRModule     *module;
    SemanticState *sem_state;  /* optional: type info from semantic */
    int           next_temp;
    int           next_label;
    int           next_local;
    /* Source location track */
    const char   *src_file;
    int           src_line;
    int           src_col;
    const char   *func_name;
    /* String table for string literals */
    const char  **strings;
    int           string_count;
    int           string_cap;
} IRBuilder;

/* API */
IRBuilder *ir_builder_new(IRModule *mod);
void       ir_builder_free(IRBuilder *b);

/* Ana donusturucu: AST → IR */
IRFunction *ir_build_function(IRBuilder *b, AstNode *func_node);
void        ir_build_program(IRBuilder *b, AstNode *prog);

/* Yardimcilar */
IRBlock    *ir_builder_new_block(IRBuilder *b, const char *label);
IRInsn     *ir_builder_emit(IRBuilder *b, IROpcode op,
                             IROperand dst, IROperand src1, IROperand src2);
int         ir_builder_new_temp(IRBuilder *b, GclType *type);
int         ir_builder_new_local(IRBuilder *b, GclType *type);

/* Instruction ID atama */
void ir_builder_set_location(IRBuilder *b, int line, int col);

#endif /* GCL_IR_BUILDER_H */
