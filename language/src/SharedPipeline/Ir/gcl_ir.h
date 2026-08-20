/*
 * gcl_ir.h — Intermediate Representation for GCL
 */

#ifndef GCL_IR_H
#define GCL_IR_H

#include "../Common/gcl_common.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

typedef enum {
    IR_NOP,
    IR_PUSH_INT,
    IR_PUSH_FLOAT,
    IR_PUSH_STRING,
    IR_PUSH_NULL,
    IR_POP,
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_NEG, IR_NOT, IR_BITNOT,
    IR_EQ, IR_NEQ, IR_LT, IR_GT, IR_LTE, IR_GTE,
    IR_AND, IR_OR,
    IR_BITAND, IR_BITOR, IR_BITXOR, IR_SHL, IR_SHR,
    IR_LOAD, IR_STORE,
    IR_LOAD_GLOBAL, IR_STORE_GLOBAL,
    IR_CALL,
    IR_RET,
    IR_JMP, IR_JZ, IR_JNZ,
    IR_LABEL,
    IR_MEMBER,
    IR_MEMBER_ASSIGN,
    IR_INDEX,
    IR_INDEX_ASSIGN,
    /* Part 3: scanf "%type" target — s_val=target var, i_val=type kind
     * (0=string, 1=int, 2=float, 3=char). Safe stdin read: overflow is
     * detected, truncated and #warning'd, always null-terminated. */
    IR_SCANF,
    /* Part 4/5: variable.free() — s_val=target var. Sets the var to null;
     * freeing an already-null var emits a double-free #warning. */
    IR_FREE,
    /* Part 5: gcMalloc/malloc reserve — s_val=target var, i_val=reserve
     * capacity, arg_count=extra auto-grow step (0 for plain malloc). */
    IR_GCMALLOC,
    IR_PRINT,
    IR_HALT
} GclIrOp;

typedef struct {
    GclIrOp     op;
    int64_t     i_val;
    double      f_val;
    const char *s_val;
    int         arg_count;
} GclIrInstr;

#define GCL_IR_MAX 4096
#define GCL_IR_ARR_MAX 128

typedef struct {
    const char *name;
    int         rows;
    int         cols;
} GclIrArrayInfo;

#define GCL_IR_CLASS_MAX 64

typedef struct {
    GclIrInstr  instrs[GCL_IR_MAX];
    int         count;
    GclArena   *arena;
    GclDiagBag *diag;
    /* Static array dimension table (filled during IR gen from AST_VAR_DECL) */
    GclIrArrayInfo arrays[GCL_IR_ARR_MAX];
    int         array_count;
    /* Class names (filled during IR gen from AST_CLASS_DECL).
     * Constructor calls FATHER() / CHILD(...) skip argument pushing so
     * the instance string is the only value left on the stack. */
    const char *class_names[GCL_IR_CLASS_MAX];
    int         class_count;
} GclIrProgram;

void gcl_ir_init(GclIrProgram *prog, GclArena *arena, GclDiagBag *diag);
int  gcl_ir_emit(GclIrProgram *prog, GclIrOp op, int64_t i_val, double f_val,
                 const char *s_val, int arg_count);
void gcl_ir_gen(GclIrProgram *prog, const GclAstNode *ast);
void gcl_ir_dump(const GclIrProgram *prog);

const char *gcl_ir_op_name(GclIrOp op);

#endif /* GCL_IR_H */
