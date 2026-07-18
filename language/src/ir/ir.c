#include "ir.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------- */
/*  Module                                                            */
/* ----------------------------------------------------------------- */

IRModule *ir_module_new(void) {
    IRModule *m = (IRModule*)calloc(1, sizeof(IRModule));
    if (m) m->next_insn_id = 1;
    return m;
}

void ir_module_free(IRModule *mod) {
    if (!mod) return;
    IRFunction *f = mod->functions;
    while (f) {
        IRFunction *next = f->next;
        ir_func_free(f);
        f = next;
    }
    free(mod);
}

/* ----------------------------------------------------------------- */
/*  Function                                                          */
/* ----------------------------------------------------------------- */

IRFunction *ir_func_new(const char *name, GclType *ret,
                         GclType **params, int pcount) {
    IRFunction *f = (IRFunction*)calloc(1, sizeof(IRFunction));
    if (!f) return NULL;
    f->name = name;
    f->ret_type = ret;
    f->param_types = params;
    f->param_count = pcount;
    return f;
}

void ir_func_free(IRFunction *f) {
    if (!f) return;
    IRBlock *b = f->blocks;
    while (b) {
        IRBlock *next = b->next;
        IRInsn *insn = b->first;
        while (insn) {
            IRInsn *inext = insn->next;
            ir_insn_free(insn);
            insn = inext;
        }
        free(b);
        b = next;
    }
    free(f);
}

/* ----------------------------------------------------------------- */
/*  Blocks                                                            */
/* ----------------------------------------------------------------- */

IRBlock *ir_block_new(int id, const char *label) {
    IRBlock *b = (IRBlock*)calloc(1, sizeof(IRBlock));
    if (!b) return NULL;
    b->id = id;
    b->label = label;
    return b;
}

void ir_block_add_insn(IRBlock *block, IRInsn *insn) {
    if (!block->first) {
        block->first = block->last = insn;
    } else {
        block->last->next = insn;
        insn->prev = block->last;
        block->last = insn;
    }
    block->insn_count++;
}

/* ----------------------------------------------------------------- */
/*  Instructions                                                      */
/* ----------------------------------------------------------------- */

IRInsn *ir_insn_new(IROpcode op, IROperand dst,
                     IROperand src1, IROperand src2) {
    IRInsn *insn = (IRInsn*)calloc(1, sizeof(IRInsn));
    if (!insn) return NULL;
    insn->opcode = op;
    insn->dst = dst;
    insn->src1 = src1;
    insn->src2 = src2;
    return insn;
}

void ir_insn_free(IRInsn *insn) {
    free(insn);
}

/* ----------------------------------------------------------------- */
/*  Operands                                                          */
/* ----------------------------------------------------------------- */

IROperand ir_const_int(int64_t val) {
    IROperand op = { IR_CONST_INT, type_long(), {0} };
    op.data.int_val = val;
    return op;
}

IROperand ir_const_float(double val) {
    IROperand op = { IR_CONST_FLOAT, type_double(), {0} };
    op.data.float_val = val;
    return op;
}

IROperand ir_const_string(const char *str) {
    IROperand op = { IR_CONST_STRING, type_pointer(type_char()), {0} };
    op.data.name = str;
    return op;
}

IROperand ir_local(int slot, GclType *type) {
    IROperand op = { IR_LOCAL, type, {0} };
    op.data.slot = slot;
    return op;
}

IROperand ir_temp(int slot, GclType *type) {
    IROperand op = { IR_TEMP, type, {0} };
    op.data.slot = slot;
    return op;
}

IROperand ir_label(int id) {
    IROperand op = { IR_LABEL, NULL, {0} };
    op.data.label_id = id;
    return op;
}

IROperand ir_func(const char *name) {
    IROperand op = { IR_FUNC, NULL, {0} };
    op.data.name = name;
    return op;
}

IROperand ir_none(void) {
    IROperand op = { IR_NONE, NULL, {0} };
    return op;
}

/* ----------------------------------------------------------------- */
/*  Debug / Dump                                                      */
/* ----------------------------------------------------------------- */

static const char *opcode_name(IROpcode op) {
    switch (op) {
    case IR_NOP:  return "nop";
    case IR_ADD:  return "add";  case IR_SUB:  return "sub";
    case IR_MUL:  return "mul";  case IR_DIV:  return "div";
    case IR_MOD:  return "mod";
    case IR_AND:  return "and";  case IR_OR:   return "or";
    case IR_XOR:  return "xor";  case IR_SHL:  return "shl";
    case IR_SHR:  return "shr";
    case IR_NEG:  return "neg";  case IR_NOT:  return "not";
    case IR_BNOT: return "bnot";
    case IR_EQ:   return "eq";   case IR_NE:   return "ne";
    case IR_LT:   return "lt";   case IR_LE:   return "le";
    case IR_GT:   return "gt";   case IR_GE:   return "ge";
    case IR_ALLOCA: return "alloca";
    case IR_LOAD: return "load"; case IR_STORE: return "store";
    case IR_LEA:  return "lea";
    case IR_MEMSET: return "memset";
    case IR_JMP:  return "jmp";  case IR_BR:   return "br";
    case IR_CALL: return "call"; case IR_RET:  return "ret";
    case IR_PHI:  return "phi";
    case IR_PUSH: return "push";
    case IR_PRINTF: return "printf";
    case IR_LABEL_DECL: return "label";
    case IR_COMMENT: return "#";
    }
    return "?";
}

void ir_dump_operand(IROperand *op, FILE *out) {
    switch (op->kind) {
    case IR_NONE:   fprintf(out, "_"); break;
    case IR_CONST_INT:   fprintf(out, "%ld", (long)op->data.int_val); break;
    case IR_CONST_FLOAT: fprintf(out, "%g", op->data.float_val); break;
    case IR_CONST_STRING: fprintf(out, "\"%s\"", op->data.name ? op->data.name : ""); break;
    case IR_LOCAL:  fprintf(out, "%%l%d", op->data.slot); break;
    case IR_GLOBAL: fprintf(out, "@%s", op->data.name); break;
    case IR_TEMP:   fprintf(out, "%%t%d", op->data.slot); break;
    case IR_FUNC:   fprintf(out, "@%s", op->data.name); break;
    case IR_LABEL:  fprintf(out, ".L%d", op->data.label_id); break;
    }
    if (op->type) fprintf(out, ":%s", type_name(op->type));
}

void ir_dump_insn(IRInsn *insn, FILE *out) {
    fprintf(out, "  ");
    if (insn->insn_id)
        fprintf(out, "[%%%d] ", insn->insn_id);
    switch (insn->opcode) {
    case IR_LABEL_DECL:
        if (insn->dst.kind == IR_LABEL)
            fprintf(out, ".L%d:\n", insn->dst.data.label_id);
        else
            fprintf(out, "%s:\n", insn->dst.data.name);
        return;
    case IR_COMMENT:
        fprintf(out, "# %s\n", insn->dst.data.name ? insn->dst.data.name : "");
        return;
    case IR_JMP:
        fprintf(out, "jmp ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, "\n");
        return;
    case IR_BR: {
        fprintf(out, "br ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, ", ");
        ir_dump_operand(&insn->src1, out);
        fprintf(out, ", ");
        ir_dump_operand(&insn->src2, out);
        fprintf(out, "\n");
        return;
    }
    case IR_RET:
        fprintf(out, "ret ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, "\n");
        return;
    case IR_STORE:
        fprintf(out, "store ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, ", ");
        ir_dump_operand(&insn->src1, out);
        fprintf(out, "\n");
        return;
    case IR_ALLOCA:
        fprintf(out, "alloca ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, "\n");
        return;
    case IR_PUSH:
        fprintf(out, "push ");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, "\n");
        return;
    case IR_PRINTF:
        fprintf(out, "printf fmt=");
        ir_dump_operand(&insn->dst, out);
        fprintf(out, ", args=");
        ir_dump_operand(&insn->src1, out);
        fprintf(out, "\n");
        return;
    default:
        if (insn->dst.kind != IR_NONE) {
            ir_dump_operand(&insn->dst, out);
            fprintf(out, " = ");
        }
        fprintf(out, "%s ", opcode_name(insn->opcode));
        ir_dump_operand(&insn->src1, out);
        if (insn->src2.kind != IR_NONE) {
            fprintf(out, ", ");
            ir_dump_operand(&insn->src2, out);
        }
        if (insn->src3.kind != IR_NONE) {
            fprintf(out, ", ");
            ir_dump_operand(&insn->src3, out);
        }
        fprintf(out, "\n");
        break;
    }
}

void ir_dump_func(IRFunction *f, FILE *out) {
    fprintf(out, "\n;; Function %s (locals=%d, temps=%d)\n",
            f->name, f->local_count, f->temp_count);
    fprintf(out, "fun %s(", f->name);
    for (int i = 0; i < f->param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        fprintf(out, "%s", type_name(f->param_types[i]));
    }
    fprintf(out, ") -> %s\n", type_name(f->ret_type));

    IRBlock *b = f->blocks;
    while (b) {
        fprintf(out, ".L%d:", b->id);
        if (b->label) fprintf(out, "  ;; %s", b->label);
        fprintf(out, "\n");
        IRInsn *insn = b->first;
        while (insn) {
            ir_dump_insn(insn, out);
            insn = insn->next;
        }
        b = b->next;
    }
    fprintf(out, "endfun\n");
}

void ir_dump_module(IRModule *mod, FILE *out) {
    fprintf(out, "; GCL IR Module\n");
    IRFunction *f = mod->functions;
    while (f) {
        ir_dump_func(f, out);
        f = f->next;
    }
}
