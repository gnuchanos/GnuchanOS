#include <stdio.h>
#include "gcl_ir.h"

void gcl_ir_dump(const GclIrProgram *prog) {
    printf("--- IR Dump (%d instructions) ---\n", prog->count);
    for (int i = 0; i < prog->count; i++) {
        const GclIrInstr *ins = &prog->instrs[i];
        printf("  [%04d] %-14s", i, gcl_ir_op_name(ins->op));
        switch (ins->op) {
        case IR_PUSH_INT:
            printf(" %lld", (long long)ins->i_val);
            break;
        case IR_PUSH_FLOAT:
            printf(" %f", ins->f_val);
            break;
        case IR_PUSH_STRING:
            if (ins->s_val) printf(" \"%s\"", ins->s_val);
            break;
        case IR_LOAD:
        case IR_STORE:
        case IR_LOAD_GLOBAL:
        case IR_STORE_GLOBAL:
        case IR_LABEL:
        case IR_MEMBER:
            if (ins->s_val) printf(" %s", ins->s_val);
            break;
        case IR_CALL:
            if (ins->s_val) printf(" %s", ins->s_val);
            printf(" (argc=%d)", ins->arg_count);
            break;
        case IR_JMP:
        case IR_JZ:
        case IR_JNZ:
            printf(" -> %lld", (long long)ins->i_val);
            break;
        case IR_INDEX_ASSIGN:
            if (ins->s_val) printf(" %s", ins->s_val);
            break;
        default:
            break;
        }
        printf("\n");
    }
    printf("--- End IR ---\n");
}
