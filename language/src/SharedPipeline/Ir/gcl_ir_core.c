#include <stdio.h>
#include <string.h>
#include "gcl_ir.h"

void gcl_ir_init(GclIrProgram *prog, GclArena *arena, GclDiagBag *diag) {
    memset(prog->instrs, 0, sizeof(prog->instrs));
    prog->count = 0;
    prog->arena = arena;
    prog->diag = diag;
}

int gcl_ir_emit(GclIrProgram *prog, GclIrOp op, int64_t i_val, double f_val,
                const char *s_val, int arg_count) {
    if (prog->count >= GCL_IR_MAX) {
        gcl_diag_add(prog->diag, GCL_DIAG_ERROR, 0, 0, "<ir>",
                     "IR instruction limit reached (%d)", GCL_IR_MAX);
        return -1;
    }
    GclIrInstr *instr = &prog->instrs[prog->count];
    instr->op = op;
    instr->i_val = i_val;
    instr->f_val = f_val;
    instr->s_val = s_val;
    instr->arg_count = arg_count;
    return prog->count++;
}

const char *gcl_ir_op_name(GclIrOp op) {
    switch (op) {
    case IR_NOP:          return "NOP";
    case IR_PUSH_INT:     return "PUSH_INT";
    case IR_PUSH_FLOAT:   return "PUSH_FLOAT";
    case IR_PUSH_STRING:  return "PUSH_STRING";
    case IR_PUSH_NULL:    return "PUSH_NULL";
    case IR_POP:          return "POP";
    case IR_ADD:          return "ADD";
    case IR_SUB:          return "SUB";
    case IR_MUL:          return "MUL";
    case IR_DIV:          return "DIV";
    case IR_MOD:          return "MOD";
    case IR_NEG:          return "NEG";
    case IR_NOT:          return "NOT";
    case IR_BITNOT:       return "BITNOT";
    case IR_EQ:           return "EQ";
    case IR_NEQ:          return "NEQ";
    case IR_LT:           return "LT";
    case IR_GT:           return "GT";
    case IR_LTE:          return "LTE";
    case IR_GTE:          return "GTE";
    case IR_AND:          return "AND";
    case IR_OR:           return "OR";
    case IR_BITAND:       return "BITAND";
    case IR_BITOR:        return "BITOR";
    case IR_BITXOR:       return "BITXOR";
    case IR_SHL:          return "SHL";
    case IR_SHR:          return "SHR";
    case IR_LOAD:         return "LOAD";
    case IR_STORE:        return "STORE";
    case IR_LOAD_GLOBAL:  return "LOAD_GLOBAL";
    case IR_STORE_GLOBAL: return "STORE_GLOBAL";
    case IR_STRUCT_COPY:  return "STRUCT_COPY";
    case IR_CALL:         return "CALL";
    case IR_RET:          return "RET";
    case IR_JMP:          return "JMP";
    case IR_JZ:           return "JZ";
    case IR_JNZ:          return "JNZ";
    case IR_LABEL:        return "LABEL";
    case IR_MEMBER:       return "MEMBER";
    case IR_MEMBER_ASSIGN: return "MEMBER_ASSIGN";
    case IR_INDEX:        return "INDEX";
    case IR_INDEX_ASSIGN: return "INDEX_ASSIGN";
    case IR_SCANF: return "SCANF";
    case IR_FREE:  return "FREE";
    case IR_GCMALLOC: return "GCMALLOC";
    case IR_PRINT:        return "PRINT";
    case IR_HALT:         return "HALT";
    }
    return "???";
}
