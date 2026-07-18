#include "bytecode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

BytecodeModule *bc_module_new(void) {
    BytecodeModule *m = (BytecodeModule*)calloc(1, sizeof(BytecodeModule));
    if (!m) return NULL;
    m->capacity = 512;
    m->code = (BCInsn*)calloc(m->capacity, sizeof(BCInsn));
    m->count = 0;
    m->register_count = 0;
    m->float_reg_count = 0;
    m->local_bytes = 0;
    m->string_pool = NULL;
    m->string_pool_size = 0;
    m->string_pool_cap = 0;
    m->string_count = 0;
    return m;
}

void bc_module_free(BytecodeModule *m) {
    if (!m) return;
    free(m->code);
    free(m->string_pool);
    free(m);
}

int bc_emit(BytecodeModule *m, BCOpcode op,
            int32_t dst, int32_t src1, int32_t src2, int32_t src3) {
    return bc_emit_f(m, op, dst, src1, src2, src3, -1, -1, -1);
}

int bc_emit_f(BytecodeModule *m, BCOpcode op,
             int32_t dst, int32_t src1, int32_t src2, int32_t src3,
             int32_t dst_f, int32_t src1_f, int32_t src2_f) {
    if (!m) return -1;
    if (m->count >= m->capacity) {
        m->capacity *= 2;
        m->code = (BCInsn*)realloc(m->code, m->capacity * sizeof(BCInsn));
    }
    BCInsn *insn = &m->code[m->count++];
    insn->op = op;
    insn->dst = dst;
    insn->src1 = src1;
    insn->src2 = src2;
    insn->src3 = src3;
    insn->dst_f = dst_f;
    insn->src1_f = src1_f;
    insn->src2_f = src2_f;
    insn->insn_id = 0;
    insn->src_file = NULL;
    insn->src_line = 0;
    insn->src_col = 0;
    insn->func_name = NULL;

    /* Track int register usage */
    int max_reg = dst;
    if (src1 > max_reg) max_reg = src1;
    if (src2 > max_reg) max_reg = src2;
    if (max_reg + 1 > m->register_count)
        m->register_count = max_reg + 1;

    /* Track float register usage */
    int max_freg = dst_f;
    if (src1_f > max_freg) max_freg = src1_f;
    if (src2_f > max_freg) max_freg = src2_f;
    if (max_freg + 1 > m->float_reg_count)
        m->float_reg_count = max_freg + 1;

    return m->count - 1;
}

void bc_set_location(BytecodeModule *m, int idx,
                     int insn_id, const char *file,
                     int line, int col, const char *func) {
    if (!m || idx < 0 || idx >= m->count) return;
    m->code[idx].insn_id = insn_id;
    m->code[idx].src_file = file;
    m->code[idx].src_line = line;
    m->code[idx].src_col = col;
    m->code[idx].func_name = func;
}

int bc_add_string(BytecodeModule *m, const char *str, int len) {
    if (!m || !str) return 0;
    /* Ensure null-terminated */
    int sz = (len > 0) ? len : (int)strlen(str);
    if (m->string_pool_size + sz + 1 > m->string_pool_cap) {
        int new_cap = m->string_pool_cap ? m->string_pool_cap * 2 : 4096;
        while (new_cap < m->string_pool_size + sz + 1)
            new_cap *= 2;
        m->string_pool = (uint8_t*)realloc(m->string_pool, new_cap);
        m->string_pool_cap = new_cap;
    }
    int offset = m->string_pool_size;
    memcpy(m->string_pool + offset, str, sz);
    m->string_pool[offset + sz] = '\0';
    m->string_pool_size += sz + 1;
    m->string_count++;
    return offset;
}

static const char *bc_op_name(BCOpcode op) {
    switch (op) {
    case BC_NOP:    return "nop";
    case BC_ADD:    return "add";    case BC_SUB: return "sub";
    case BC_MUL:    return "mul";    case BC_DIV: return "div";
    case BC_MOD:    return "mod";
    case BC_AND:    return "and";    case BC_OR:  return "or";
    case BC_XOR:    return "xor";    case BC_SHL: return "shl";
    case BC_SHR:    return "shr";
    case BC_NEG:    return "neg";    case BC_NOT: return "not";
    case BC_BNOT:   return "bnot";
    case BC_EQ:     return "eq";     case BC_NE:  return "ne";
    case BC_LT:     return "lt";     case BC_LE:  return "le";
    case BC_GT:     return "gt";     case BC_GE:  return "ge";
    case BC_LADD:   return "ladd";   case BC_LSUB: return "lsub";
    case BC_LMUL:   return "lmul";   case BC_LDIV: return "ldiv";
    case BC_LMOD:   return "lmod";   case BC_LNEG: return "lneg";
    case BC_LEQ:    return "leq";    case BC_LNE:  return "lne";
    case BC_LLT:    return "llt";    case BC_LLE:  return "lle";
    case BC_LGT:    return "lgt";    case BC_LGE:  return "lge";
    case BC_FADD:   return "fadd";   case BC_FSUB: return "fsub";
    case BC_FMUL:   return "fmul";   case BC_FDIV: return "fdiv";
    case BC_FNEG:   return "fneg";
    case BC_FEQ:    return "feq";    case BC_FNE:  return "fne";
    case BC_FLT:    return "flt";    case BC_FLE:  return "fle";
    case BC_FGT:    return "fgt";    case BC_FGE:  return "fge";
    case BC_LOAD:   return "load";
    case BC_STORE:  return "store";
    case BC_LOADI:  return "loadi";
    case BC_LOADL:  return "loadl";
    case BC_LOADF:  return "loadf";
    case BC_LOADD:  return "loadd";
    case BC_LOADA:  return "loada";
    case BC_MOV:    return "mov";
    case BC_MOVL:   return "movl";
    case BC_MOVF:   return "movf";
    case BC_LOAD64: return "load64";
    case BC_STORE64: return "store64";
    case BC_FLOAD:  return "fload";
    case BC_FSTR:   return "fstr";
    case BC_I2L:    return "i2l";    case BC_I2F: return "i2f";
    case BC_I2D:    return "i2d";
    case BC_L2I:    return "l2i";    case BC_L2F: return "l2f";
    case BC_L2D:    return "l2d";
    case BC_F2I:    return "f2i";    case BC_F2L: return "f2l";
    case BC_F2D:    return "f2d";
    case BC_D2F:    return "d2f";
    case BC_JMP:    return "jmp";
    case BC_BR:     return "br";
    case BC_CALL:   return "call";
    case BC_RET:    return "ret";
    case BC_RETI:   return "reti";
    case BC_RETL:   return "retl";
    case BC_RETF:   return "retf";
    case BC_PUSH:   return "push";
    case BC_POP:    return "pop";
    case BC_PUSHF:  return "pushf";
    case BC_POPF:   return "popf";
    case BC_PRINTF: return "printf";
    case BC_TRAP:   return "trap";
    case BC_HALT:   return "halt";
    case BC_LOAD8:  return "load8";
    case BC_STORE8: return "store8";
    case BC_LOAD16: return "load16";
    case BC_STORE16: return "store16";
    case BC_COMMENT: return "#";
    }
    return "?";
}

void bc_dump(BytecodeModule *m, FILE *out) {
    if (!m) return;
    fprintf(out, ";; Bytecode Module (iregs=%d, fregs=%d, lo_bytes=%d, insns=%d, strings=%d)\n",
            m->register_count, m->float_reg_count, m->local_bytes, m->count, m->string_count);
    
    if (m->string_count > 0) {
        fprintf(out, ";; String pool:\n");
        int off = 0;
        for (int i = 0; i < m->string_count; i++) {
            if (off < m->string_pool_size) {
                fprintf(out, ";;   [%d] @%d: \"%s\"\n", i, off, (const char*)(m->string_pool + off));
                off += strlen((const char*)(m->string_pool + off)) + 1;
            }
        }
    }
    
    for (int i = 0; i < m->count; i++) {
        BCInsn *insn = &m->code[i];
        if (insn->op == BC_COMMENT) {
            fprintf(out, "  # %s\n", insn->src_file ? insn->src_file : "");
            continue;
        }
        fprintf(out, "  [%d] ", i);
        if (insn->insn_id)
            fprintf(out, "%%%d ", insn->insn_id);
        fprintf(out, "%s", bc_op_name(insn->op));

        switch (insn->op) {
        case BC_LOADI:
            fprintf(out, " r%d, %d", insn->dst, insn->src1); break;
        case BC_LOADL:
            fprintf(out, " lr%d, %lld", insn->dst,
                    (long long)(((int64_t)insn->src1 << 32) | (uint32_t)insn->src2));
            break;
        case BC_LOADF: {
            float fv;
            uint32_t bits = (uint32_t)(int32_t)insn->src1;
            memcpy(&fv, &bits, 4);
            fprintf(out, " fr%d, %g", insn->dst_f, (double)fv); break;
        }
        case BC_LOADD: {
            double dv;
            uint64_t bits = ((uint64_t)(uint32_t)(int32_t)insn->src1 << 32)
                          | (uint32_t)(int32_t)insn->src2;
            memcpy(&dv, &bits, 8);
            fprintf(out, " fr%d, %g", insn->dst_f, dv); break;
        }
        case BC_LOADA:
            fprintf(out, " r%d, &%d", insn->dst, insn->src1); break;
        case BC_JMP:
        case BC_CALL:
            fprintf(out, " %d", insn->dst); break;
        case BC_BR:
            fprintf(out, " r%d, %d, %d", insn->dst, insn->src1, insn->src2); break;
        case BC_RETI: case BC_RETL: case BC_RETF:
        case BC_RET:
            break;
        case BC_STORE:
            fprintf(out, " [r%d], r%d", insn->dst, insn->src1); break;
        case BC_LOAD:
            fprintf(out, " r%d, [r%d]", insn->dst, insn->src1); break;
        case BC_MOV:
            fprintf(out, " r%d, r%d", insn->dst, insn->src1); break;
        case BC_MOVL:
            fprintf(out, " lr%d, lr%d", insn->dst, insn->src1); break;
        case BC_MOVF:
            fprintf(out, " fr%d, fr%d", insn->dst_f, insn->src1_f); break;
        case BC_PUSH:  fprintf(out, " r%d", insn->dst); break;
        case BC_POP:   fprintf(out, " r%d", insn->dst); break;
        case BC_PUSHF: fprintf(out, " fr%d", insn->dst_f); break;
        case BC_POPF:  fprintf(out, " fr%d", insn->dst_f); break;
        case BC_LOAD8:  fprintf(out, " r%d, [r%d:8]", insn->dst, insn->src1); break;
        case BC_STORE8: fprintf(out, " [r%d:8], r%d", insn->dst, insn->src1); break;
        case BC_LOAD16: fprintf(out, " r%d, [r%d:16]", insn->dst, insn->src1); break;
        case BC_STORE16: fprintf(out, " [r%d:16], r%d", insn->dst, insn->src1); break;
        case BC_LOAD64: fprintf(out, " lr%d, [r%d:64]", insn->dst, insn->src1); break;
        case BC_STORE64: fprintf(out, " [r%d:64], lr%d", insn->dst, insn->src1); break;
        case BC_FLOAD:  fprintf(out, " fr%d, [r%d:f64]", insn->dst_f, insn->src1); break;
        case BC_FSTR:   fprintf(out, " [r%d:f64], fr%d", insn->dst, insn->src1_f); break;
        case BC_PRINTF: fprintf(out, " args=%d fmt=r%d", insn->dst, insn->src2); break;
        /* Float arith */
        case BC_FADD: case BC_FSUB: case BC_FMUL: case BC_FDIV:
        case BC_FEQ:  case BC_FNE:  case BC_FLT:  case BC_FLE:
        case BC_FGT:  case BC_FGE:
            fprintf(out, " fr%d, fr%d, fr%d", insn->dst_f, insn->src1_f, insn->src2_f);
            break;
        case BC_FNEG:
            fprintf(out, " fr%d, fr%d", insn->dst_f, insn->src1_f); break;
        /* Conversions */
        case BC_I2F: case BC_I2D:
            fprintf(out, " fr%d, r%d", insn->dst_f, insn->src1); break;
        case BC_L2F: case BC_L2D:
            fprintf(out, " fr%d, lr%d", insn->dst_f, insn->src1); break;
        case BC_F2I: case BC_F2L:
            fprintf(out, " r%d, fr%d", insn->dst, insn->src1_f); break;
        case BC_F2D: case BC_D2F:
            fprintf(out, " fr%d, fr%d", insn->dst_f, insn->src1_f); break;
        case BC_I2L: case BC_L2I:
            fprintf(out, " r%d, r%d", insn->dst, insn->src1); break;
        /* Int64 arith */
        case BC_LADD: case BC_LSUB: case BC_LMUL: case BC_LDIV: case BC_LMOD:
        case BC_LEQ:  case BC_LNE:  case BC_LLT:  case BC_LLE:
        case BC_LGT:  case BC_LGE:
            fprintf(out, " lr%d, lr%d, lr%d", insn->dst, insn->src1, insn->src2);
            break;
        case BC_LNEG:
            fprintf(out, " lr%d, lr%d", insn->dst, insn->src1); break;
        default:
            fprintf(out, " r%d, r%d, r%d", insn->dst, insn->src1, insn->src2);
            break;
        }
        fprintf(out, "\n");
    }
}
