#include "ir_to_bc.h"
#include "../ir/ir_builder.h"
#include "../type/type.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

IRToBC *ir_to_bc_new(void) {
    IRToBC *t = (IRToBC*)calloc(1, sizeof(IRToBC));
    if (!t) return NULL;
    t->bc = bc_module_new();
    t->func_names = NULL;
    t->func_indices = NULL;
    t->func_count = 0;
    t->label_offsets = NULL;
    t->label_count = 0;
    t->rega.next_reg = 0;
    t->rega.next_lreg = 0;
    t->rega.next_freg = 0;
    t->rega.temp_to_reg = NULL;
    t->rega.temp_to_lreg = NULL;
    t->rega.temp_to_freg = NULL;
    t->rega.temp_count = 0;
    t->rega.local_reg_base = 0;
    t->rega.local_lreg_base = 0;
    t->rega.local_freg_base = 0;
    return t;
}

void ir_to_bc_free(IRToBC *t) {
    if (!t) return;
    bc_module_free(t->bc);
    free(t->func_names);
    free(t->func_indices);
    free(t->label_offsets);
    free(t->rega.temp_to_reg);
    free(t->rega.temp_to_lreg);
    free(t->rega.temp_to_freg);
    free(t);
}

#define SLOT_BYTES 8

static int slot_offset(int slot) {
    return slot * SLOT_BYTES;
}

static int ensure_temp_capacity(IRToBC_RegAlloc *r, int temp_slot) {
    if (temp_slot < 0) return -1;
    if (temp_slot >= r->temp_count) {
        int old = r->temp_count;
        r->temp_count = temp_slot + 16;
        r->temp_to_reg = (int*)realloc(r->temp_to_reg, r->temp_count * sizeof(int));
        r->temp_to_lreg = (int*)realloc(r->temp_to_lreg, r->temp_count * sizeof(int));
        r->temp_to_freg = (int*)realloc(r->temp_to_freg, r->temp_count * sizeof(int));
        for (int i = old; i < r->temp_count; i++) {
            r->temp_to_reg[i] = -1;
            r->temp_to_lreg[i] = -1;
            r->temp_to_freg[i] = -1;
        }
    }
    return temp_slot;
}

static int get_ireg(IRToBC_RegAlloc *r, int temp_slot) {
    if (ensure_temp_capacity(r, temp_slot) < 0) return -1;
    if (r->temp_to_reg[temp_slot] < 0)
        r->temp_to_reg[temp_slot] = r->next_reg++;
    return r->temp_to_reg[temp_slot];
}

static int get_lreg(IRToBC_RegAlloc *r, int temp_slot) {
    if (ensure_temp_capacity(r, temp_slot) < 0) return -1;
    if (r->temp_to_lreg[temp_slot] < 0)
        r->temp_to_lreg[temp_slot] = r->next_lreg++;
    return r->temp_to_lreg[temp_slot];
}

static int get_freg(IRToBC_RegAlloc *r, int temp_slot) {
    if (ensure_temp_capacity(r, temp_slot) < 0) return -1;
    if (r->temp_to_freg[temp_slot] < 0)
        r->temp_to_freg[temp_slot] = r->next_freg++;
    return r->temp_to_freg[temp_slot];
}

static void set_label_offset(IRToBC *t, int label_id, int offset) {
    if (label_id < 0) return;
    if (label_id >= t->label_count) {
        int old = t->label_count;
        t->label_count = label_id + 16;
        t->label_offsets = (int*)realloc(t->label_offsets, t->label_count * sizeof(int));
        for (int i = old; i < t->label_count; i++)
            t->label_offsets[i] = -1;
    }
    t->label_offsets[label_id] = offset;
}

static int get_label_offset(IRToBC *t, int label_id) {
    if (label_id < 0 || label_id >= t->label_count) return -1;
    return t->label_offsets[label_id];
}

static int op_is_float(IROperand *op) {
    if (!op || !op->type) return 0;
    TypeKind k = op->type->kind;
    return (k == TYPE_FLOAT || k == TYPE_DOUBLE || k == TYPE_LONG_DOUBLE);
}

static int op_is_long(IROperand *op) {
    if (!op || !op->type) return 0;
    TypeKind k = op->type->kind;
    return (k == TYPE_LONG || k == TYPE_LONG_LONG);
}

static int op_width(IROperand *op) {
    if (!op || !op->type) return 4;
    return (int)op->type->size;
}

typedef struct {
    int ireg;
    int lreg;
    int freg;
    int is_imm;
    int32_t imm_val;
    int64_t limm_val;
    double  fimm_val;
    int string_pool_off;
} TransOp;

static TransOp trans_op(IRToBC *t, IROperand *op) {
    TransOp r;
    memset(&r, 0, sizeof(r));
    r.ireg = -1; r.lreg = -1; r.freg = -1; r.string_pool_off = -1;
    if (!op) return r;

    int is_float = op_is_float(op);
    int is_long  = op_is_long(op);

    switch (op->kind) {
    case IR_CONST_INT:
        r.is_imm = 1;
        r.imm_val = (int32_t)op->data.int_val;
        r.limm_val = op->data.int_val;
        r.fimm_val = (double)op->data.int_val;
        break;
    case IR_CONST_FLOAT:
        r.is_imm = 1;
        r.fimm_val = op->data.float_val;
        r.imm_val = (int32_t)op->data.float_val;
        r.limm_val = (int64_t)op->data.float_val;
        break;
    case IR_CONST_STRING: {
        const char *str = op->data.name;
        if (str) {
            r.string_pool_off = bc_add_string(t->bc, str, (int)strlen(str));
            r.is_imm = 1;
            r.imm_val = r.string_pool_off;
        }
        break;
    }
    case IR_LOCAL: {
        int offset = slot_offset(op->data.slot);
        r.ireg = t->rega.next_reg++;
        bc_emit(t->bc, BC_LOADI, r.ireg, offset, 0, 0);
        break;
    }
    case IR_TEMP:
        if (is_float)
            r.freg = get_freg(&t->rega, op->data.slot);
        else if (is_long)
            r.lreg = get_lreg(&t->rega, op->data.slot);
        else
            r.ireg = get_ireg(&t->rega, op->data.slot);
        break;
    case IR_GLOBAL:
        r.ireg = get_ireg(&t->rega, op->data.slot);
        break;
    case IR_LABEL:
        r.imm_val = op->data.label_id;
        break;
    default:
        break;
    }
    return r;
}

static void emit_alu(IRToBC *t, BCOpcode int_op, BCOpcode long_op, BCOpcode float_op,
                     IROperand *dop, IROperand *s1op, IROperand *s2op) {
    int is_float = op_is_float(dop);
    int is_long  = op_is_long(dop);

    TransOp td = trans_op(t, dop);
    TransOp ts1 = trans_op(t, s1op);
    TransOp ts2 = trans_op(t, s2op);

    if (is_float) {
        if (ts1.is_imm && ts1.freg < 0) {
            int tmp = t->rega.next_freg++;
            float fv = (float)ts1.fimm_val;
            uint32_t bits;
            memcpy(&bits, &fv, 4);
            bc_emit_f(t->bc, BC_LOADF, 0, (int32_t)bits, 0, 0, -1, tmp, -1);
            ts1.freg = tmp;
            ts1.is_imm = 0;
        }
        if (ts2.is_imm && ts2.freg < 0) {
            int tmp = t->rega.next_freg++;
            float fv = (float)ts2.fimm_val;
            uint32_t bits;
            memcpy(&bits, &fv, 4);
            bc_emit_f(t->bc, BC_LOADF, 0, (int32_t)bits, 0, 0, -1, tmp, -1);
            ts2.freg = tmp;
            ts2.is_imm = 0;
        }
        if (td.freg >= 0 && ts1.freg >= 0) {
            if (float_op == BC_FNEG)
                bc_emit_f(t->bc, BC_FNEG, 0, 0, 0, 0, td.freg, ts1.freg, -1);
            else if (ts2.freg >= 0)
                bc_emit_f(t->bc, float_op, 0, 0, 0, 0, td.freg, ts1.freg, ts2.freg);
        }
    } else if (is_long) {
        if (ts1.is_imm && ts1.lreg < 0) {
            int tmp = t->rega.next_lreg++;
            int hi = (int32_t)(ts1.limm_val >> 32);
            int lo = (int32_t)(ts1.limm_val & 0xFFFFFFFF);
            bc_emit(t->bc, BC_LOADL, tmp, hi, lo, 0);
            ts1.lreg = tmp;
            ts1.is_imm = 0;
        }
        if (ts2.is_imm && ts2.lreg < 0) {
            int tmp = t->rega.next_lreg++;
            int hi = (int32_t)(ts2.limm_val >> 32);
            int lo = (int32_t)(ts2.limm_val & 0xFFFFFFFF);
            bc_emit(t->bc, BC_LOADL, tmp, hi, lo, 0);
            ts2.lreg = tmp;
            ts2.is_imm = 0;
        }
        if (td.lreg >= 0 && ts1.lreg >= 0) {
            if (long_op == BC_LNEG)
                bc_emit(t->bc, BC_LNEG, td.lreg, ts1.lreg, 0, 0);
            else if (ts2.lreg >= 0)
                bc_emit(t->bc, long_op, td.lreg, ts1.lreg, ts2.lreg, 0);
        }
    } else {
        if (ts1.is_imm && ts1.ireg < 0) {
            int tmp = t->rega.next_reg++;
            bc_emit(t->bc, BC_LOADI, tmp, ts1.imm_val, 0, 0);
            ts1.ireg = tmp;
            ts1.is_imm = 0;
        }
        if (ts2.is_imm && ts2.ireg < 0) {
            int tmp = t->rega.next_reg++;
            bc_emit(t->bc, BC_LOADI, tmp, ts2.imm_val, 0, 0);
            ts2.ireg = tmp;
            ts2.is_imm = 0;
        }
        if (td.ireg >= 0 && ts1.ireg >= 0) {
            if (int_op == BC_NEG || int_op == BC_NOT || int_op == BC_BNOT)
                bc_emit(t->bc, int_op, td.ireg, ts1.ireg, 0, 0);
            else if (ts2.ireg >= 0)
                bc_emit(t->bc, int_op, td.ireg, ts1.ireg, ts2.ireg, 0);
        }
    }
}

static void trans_insn(IRToBC *t, IRInsn *insn) {
    if (!insn) return;
    int idx = t->bc->count;

    if (insn->opcode == IR_LABEL_DECL) {
        if (insn->dst.kind == IR_LABEL)
            set_label_offset(t, insn->dst.data.label_id, t->bc->count);
        return;
    }

    switch (insn->opcode) {
    case IR_NOP:
    case IR_COMMENT:
    case IR_ALLOCA:
        break;

    case IR_ADD: emit_alu(t, BC_ADD, BC_LADD, BC_FADD, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_SUB: emit_alu(t, BC_SUB, BC_LSUB, BC_FSUB, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_MUL: emit_alu(t, BC_MUL, BC_LMUL, BC_FMUL, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_DIV: emit_alu(t, BC_DIV, BC_LDIV, BC_FDIV, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_MOD: emit_alu(t, BC_MOD, BC_LMOD, BC_DIV,  &insn->dst, &insn->src1, &insn->src2); break;
    case IR_AND: emit_alu(t, BC_AND, BC_AND, BC_AND, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_OR:  emit_alu(t, BC_OR,  BC_OR,  BC_OR,  &insn->dst, &insn->src1, &insn->src2); break;
    case IR_XOR: emit_alu(t, BC_XOR, BC_XOR, BC_XOR, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_SHL: emit_alu(t, BC_SHL, BC_SHL, BC_SHL, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_SHR: emit_alu(t, BC_SHR, BC_SHR, BC_SHR, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_EQ:  emit_alu(t, BC_EQ,  BC_LEQ, BC_FEQ, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_NE:  emit_alu(t, BC_NE,  BC_LNE, BC_FNE, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_LT:  emit_alu(t, BC_LT,  BC_LLT, BC_FLT, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_LE:  emit_alu(t, BC_LE,  BC_LLE, BC_FLE, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_GT:  emit_alu(t, BC_GT,  BC_LGT, BC_FGT, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_GE:  emit_alu(t, BC_GE,  BC_LGE, BC_FGE, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_NEG: emit_alu(t, BC_NEG, BC_LNEG, BC_FNEG, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_NOT: emit_alu(t, BC_NOT, BC_NOT, BC_NOT, &insn->dst, &insn->src1, &insn->src2); break;
    case IR_BNOT:emit_alu(t, BC_BNOT, BC_BNOT, BC_BNOT, &insn->dst, &insn->src1, &insn->src2); break;

    case IR_LOAD: {
        /* Load: dst = *src1 (handle local/temp). Simplified for now - just copy */
        TransOp td = trans_op(t, &insn->dst);
        TransOp ts = trans_op(t, &insn->src1);
        if (td.ireg >= 0 && ts.ireg >= 0)
            bc_emit(t->bc, BC_MOV, td.ireg, ts.ireg, 0, 0);
        else if (td.freg >= 0 && ts.freg >= 0)
            bc_emit_f(t->bc, BC_MOVF, 0, 0, 0, 0, td.freg, ts.freg, -1);
        break;
    }

    case IR_STORE: {
        /* Store: *dst = src1 (handle local/temp) */
        TransOp td = trans_op(t, &insn->dst);
        TransOp ts = trans_op(t, &insn->src1);
        if (td.ireg >= 0 && ts.ireg >= 0) {
            if (op_is_float(&insn->src1) || op_is_float(&insn->dst)) {
                /* Float store - load local addr, store float */
            } else {
                bc_emit(t->bc, BC_MOV, td.ireg, ts.ireg, 0, 0);
            }
        }
        break;
    }

    case IR_PUSH: {
        TransOp tv = trans_op(t, &insn->dst);
        if (tv.ireg >= 0) {
            bc_emit(t->bc, BC_PUSH, tv.ireg, 0, 0, 0);
        } else if (tv.is_imm && tv.string_pool_off >= 0) {
            int tmp_reg = t->rega.next_reg++;
            bc_emit(t->bc, BC_LOADI, tmp_reg, tv.string_pool_off, 0, 0);
            bc_emit(t->bc, BC_PUSH, tmp_reg, 0, 0, 0);
        } else if (tv.is_imm) {
            int tmp_reg = t->rega.next_reg++;
            bc_emit(t->bc, BC_LOADI, tmp_reg, tv.imm_val, 0, 0);
            bc_emit(t->bc, BC_PUSH, tmp_reg, 0, 0, 0);
        }
        break;
    }

    case IR_PRINTF: {
        TransOp tf = trans_op(t, &insn->dst);
        TransOp ta = trans_op(t, &insn->src1);
        int arg_count = (int)ta.imm_val;
        int fmt_reg = t->rega.next_reg++;
        bc_emit(t->bc, BC_LOADI, fmt_reg, ta.string_pool_off >= 0 ? tf.string_pool_off : 0, 0, 0);
        /* BC_PRINTF: dst = arg_count, src2 = format pool address */
        bc_emit(t->bc, BC_PRINTF, arg_count, 0, fmt_reg, 0);
        break;
    }

    case IR_LEA: {
        int slot = insn->src1.data.slot;
        int offset = slot_offset(slot);
        if (insn->dst.kind == IR_TEMP) {
            int ireg = get_ireg(&t->rega, insn->dst.data.slot);
            bc_emit(t->bc, BC_LOADI, ireg, offset, 0, 0);
        }
        break;
    }

    case IR_JMP: {
        int label = (insn->dst.kind == IR_LABEL) ? insn->dst.data.label_id : -1;
        int off = get_label_offset(t, label);
        if (off >= 0) bc_emit(t->bc, BC_JMP, off, 0, 0, 0);
        break;
    }

    case IR_BR: {
        TransOp tc = trans_op(t, &insn->dst);
        int tl = (insn->src1.kind == IR_LABEL) ? get_label_offset(t, insn->src1.data.label_id) : -1;
        int fl = (insn->src2.kind == IR_LABEL) ? get_label_offset(t, insn->src2.data.label_id) : -1;
        if (tc.ireg >= 0 && tl >= 0 && fl >= 0)
            bc_emit(t->bc, BC_BR, tc.ireg, tl, fl, 0);
        break;
    }

    case IR_CALL: {
        const char *fname = insn->src1.kind == IR_FUNC ? insn->src1.data.name : NULL;
        if (fname && (strcmp(fname, "printf") == 0 || strcmp(fname, "puts") == 0)) {
            break; /* handled by IR_PRINTF */
        }
        int fidx = -1;
        if (fname) {
            for (int i = 0; i < t->func_count; i++) {
                if (t->func_names[i] && strcmp(t->func_names[i], fname) == 0) {
                    fidx = t->func_indices[i];
                    break;
                }
            }
        }
        if (fidx >= 0)
            bc_emit(t->bc, BC_CALL, 0, fidx, 0, 0);
        break;
    }

    case IR_RET: {
        IROperand *val = &insn->dst;
        if (val->kind != IR_NONE) {
            TransOp tv = trans_op(t, val);
            if (tv.ireg >= 0) {
                if (tv.ireg != 0)
                    bc_emit(t->bc, BC_MOV, 0, tv.ireg, 0, 0);
                bc_emit(t->bc, BC_RETI, 0, 0, 0, 0);
            } else if (tv.is_imm) {
                bc_emit(t->bc, BC_LOADI, 0, tv.imm_val, 0, 0);
                bc_emit(t->bc, BC_RETI, 0, 0, 0, 0);
            }
        } else {
            bc_emit(t->bc, BC_RET, 0, 0, 0, 0);
        }
        break;
    }

    case IR_MEMSET:
    case IR_PHI:
        break;

    default:
        break;
    }

    if (insn->insn_id > 0) {
        int li = t->bc->count - 1;
        if (li >= idx && li < t->bc->count)
            bc_set_location(t->bc, li, insn->insn_id,
                            insn->src_file, insn->src_line, insn->src_col,
                            insn->func_name);
    }
}

/* ----------------------------------------------------------------- */
/*  Main entry point                                                  */
/* ----------------------------------------------------------------- */

BytecodeModule *ir_to_bc_translate(IRModule *ir_mod, IRBuilder *builder) {
    if (!ir_mod) return NULL;
    IRToBC *t = ir_to_bc_new();
    if (!t) return NULL;

    (void)builder; /* strings are emitted via bc_add_string in trans_op */

    int nf = 0;
    IRFunction *f = ir_mod->functions;
    while (f) { nf++; f = f->next; }

    if (nf > 0) {
        t->func_names = (const char**)calloc(nf, sizeof(const char*));
        t->func_indices = (int*)calloc(nf, sizeof(int));
    }

    f = ir_mod->functions;
    int idx = 0;
    while (f) {
        t->func_names[idx] = f->name;
        t->func_indices[idx] = idx;

        int entry_offset = bc_emit(t->bc, BC_NOP, 0, 0, 0, 0);
        set_label_offset(t, idx, entry_offset);

        int local_needed = (f->local_count + 1) * SLOT_BYTES;
        if (local_needed > t->bc->local_bytes)
            t->bc->local_bytes = local_needed;

        t->rega.local_reg_base = f->local_count;
        t->rega.local_lreg_base = 0;
        t->rega.local_freg_base = 0;

        /* First pass: record all label declarations */
        IRBlock *b = f->blocks;
        while (b) {
            IRInsn *insn = b->first;
            while (insn) {
                if (insn->opcode == IR_LABEL_DECL && insn->dst.kind == IR_LABEL)
                    set_label_offset(t, insn->dst.data.label_id, t->bc->count);
                insn = insn->next;
            }
            b = b->next;
        }

        /* Second pass: translate instructions */
        b = f->blocks;
        while (b) {
            IRInsn *insn = b->first;
            while (insn) {
                trans_insn(t, insn);
                insn = insn->next;
            }
            b = b->next;
        }

        idx++;
        f = f->next;
    }

    t->func_count = nf;
    
    bc_emit(t->bc, BC_HALT, 0, 0, 0, 0);
    if (t->bc->local_bytes < 256) t->bc->local_bytes = 256;

    BytecodeModule *r = t->bc;
    t->bc = NULL;
    ir_to_bc_free(t);
    return r;
}
