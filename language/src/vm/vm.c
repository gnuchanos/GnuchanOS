#include "vm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>

/* Forward declaration for format parser */
static void vm_printf_format(VM *vm, BCInsn *insn, const char *fmt, int32_t *args, int arg_count);

VM *vm_new(int reg_count, int lreg_count, int freg_count,
           int stack_size, int fstack_size,
           int local_bytes) {
    VM *vm = (VM*)calloc(1, sizeof(VM));
    if (!vm) return NULL;

    vm->reg_count   = reg_count  > 0 ? reg_count   : 32;
    vm->lreg_count  = lreg_count > 0 ? lreg_count  : 32;
    vm->freg_count  = freg_count > 0 ? freg_count  : 32;

    vm->regs  = (int32_t*)calloc(vm->reg_count,  sizeof(int32_t));
    vm->lregs = (int64_t*)calloc(vm->lreg_count, sizeof(int64_t));
    vm->fregs = (double*) calloc(vm->freg_count, sizeof(double));

    vm->stack_size  = stack_size  > 0 ? stack_size  : 256;
    vm->fstack_size = fstack_size > 0 ? fstack_size : 64;
    vm->stack  = (int32_t*)calloc(vm->stack_size,  sizeof(int32_t));
    vm->fstack = (double*) calloc(vm->fstack_size, sizeof(double));
    vm->stack_top  = -1;
    vm->fstack_top = -1;

    vm->local_bytes = local_bytes > 0 ? local_bytes : 4096;
    vm->local_mem = (uint8_t*)calloc(vm->local_bytes, 1);

    vm->ret_stack_size = 64;
    vm->ret_stack = (int*)calloc(vm->ret_stack_size, sizeof(int));
    vm->ret_stack_top = -1;

    vm->pc = 0;
    vm->running = 1;
    vm->func_addrs = NULL;
    vm->func_count = 0;
    vm->debug_on = 0;
    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;
    free(vm->regs);
    free(vm->lregs);
    free(vm->fregs);
    free(vm->stack);
    free(vm->fstack);
    free(vm->local_mem);
    free(vm->ret_stack);
    free(vm->func_addrs);
    free(vm);
}

void vm_set_debug(VM *vm, int on) {
    if (vm) vm->debug_on = on;
}

void vm_dump_state(VM *vm, FILE *out) {
    if (!vm) return;
    fprintf(out, "VM State: pc=%d running=%d local_bytes=%d\n", 
            vm->pc, vm->running, vm->local_bytes);
    fprintf(out, "  Int Regs:  ");
    for (int i = 0; i < vm->reg_count && i < 8; i++)
        fprintf(out, "r%d=%d ", i, vm->regs[i]);
    fprintf(out, "\n  Long Regs: ");
    for (int i = 0; i < vm->lreg_count && i < 4; i++)
        fprintf(out, "lr%d=%lld ", i, (long long)vm->lregs[i]);
    fprintf(out, "\n  Float Regs:");
    for (int i = 0; i < vm->freg_count && i < 4; i++)
        fprintf(out, " fr%d=%g ", i, vm->fregs[i]);
    fprintf(out, "\n  Stack: top=%d  FStack: top=%d\n",
            vm->stack_top, vm->fstack_top);
}

static void vm_error(VM *vm, BCInsn *insn, const char *msg) {
    fprintf(stderr, "[VM ERROR] pc=%d: %s\n", vm->pc, msg);
    if (insn && insn->src_file)
        fprintf(stderr, "  at %s:%d (%s)\n", insn->src_file, insn->src_line,
                insn->func_name ? insn->func_name : "?");
    vm->running = 0;
}

/* Check local memory bounds */
static int check_mem(VM *vm, BCInsn *insn, int addr, int size) {
    if (addr < 0 || addr + size > vm->local_bytes) {
        vm_error(vm, insn, "local memory access out of bounds");
        return 0;
    }
    return 1;
}

/* ================================================================= */
/*  Format string parser for BC_PRINTF                                */
/*  Handles: %d, %i, %u, %x, %X, %ld, %lu, %lx, %lX, %lld, %llu,   */
/*           %s, %c, %f, %lf, %e, %g, %%, %p, %h, %hh, %li, %lu     */
/*           Also %.Nf for float precision                            */
/* ================================================================= */
static void vm_printf_format(VM *vm, BCInsn *insn, const char *fmt,
                              int32_t *args, int arg_count) {
    int arg_idx = 0;
    char buf[256];
    int buf_len = 0;
    (void)insn;

#define FLUSH do { if (buf_len > 0) { fwrite(buf, 1, buf_len, stdout); buf_len = 0; } } while(0)

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            buf[buf_len++] = *p;
            if (buf_len >= 255) FLUSH;
            continue;
        }
        FLUSH;
        p++; /* skip '%' */
        if (!*p) break;

        /* Handle %% */
        if (*p == '%') {
            buf[buf_len++] = '%';
            if (buf_len >= 255) FLUSH;
            continue;
        }

        /* Skip precision like .2, .6, etc */
        int has_precision = 0;
        int precision = 0;
        if (*p == '.') {
            has_precision = 1;
            p++;
            while (*p >= '0' && *p <= '9') {
                precision = precision * 10 + (*p - '0');
                p++;
            }
            if (!*p) break;
            /* go back; the precision flag will be used when we see the type */
            p--; /* we'll re-enter next loop iteration */
            continue;
        }

        /* Length modifiers */
        int is_long = 0;
        int is_llong = 0;
        int is_short = 0;
        if (*p == 'l') {
            p++;
            if (*p == 'l') { is_llong = 1; p++; }
            else { is_long = 1; }
            if (!*p) break;
        } else if (*p == 'h') {
            p++;
            if (*p == 'h') { is_short = 2; p++; }
            else { is_short = 1; }
            if (!*p) break;
        }

        /* Get the value */
        if (arg_idx >= arg_count && *p != 's' && *p != '%') {
            fprintf(stderr, "[VM] printf: missing argument for %%");
            continue;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            if (is_llong) {
                /* For 64-bit values packed as two int32s: hi, lo */
                /* Currently args are 32-bit, so handle normally */
                fprintf(stdout, "%lld", (long long)(int64_t)args[arg_idx++]);
            } else if (is_long) {
                fprintf(stdout, "%ld", (long)args[arg_idx++]);
            } else if (is_short == 1) {
                fprintf(stdout, "%hd", (short)args[arg_idx++]);
            } else if (is_short == 2) {
                fprintf(stdout, "%hhd", (signed char)args[arg_idx++]);
            } else {
                fprintf(stdout, "%d", args[arg_idx++]);
            }
            break;
        }
        case 'u': {
            if (is_llong) {
                fprintf(stdout, "%llu", (unsigned long long)(uint64_t)args[arg_idx++]);
            } else if (is_long) {
                fprintf(stdout, "%lu", (unsigned long)args[arg_idx++]);
            } else {
                fprintf(stdout, "%u", (unsigned int)args[arg_idx++]);
            }
            break;
        }
        case 'x': {
            if (is_llong) {
                fprintf(stdout, "%llx", (unsigned long long)(uint64_t)args[arg_idx++]);
            } else if (is_long) {
                fprintf(stdout, "%lx", (unsigned long)args[arg_idx++]);
            } else {
                fprintf(stdout, "%x", (unsigned int)args[arg_idx++]);
            }
            break;
        }
        case 'X': {
            if (is_llong) {
                fprintf(stdout, "%llX", (unsigned long long)(uint64_t)args[arg_idx++]);
            } else if (is_long) {
                fprintf(stdout, "%lX", (unsigned long)args[arg_idx++]);
            } else {
                fprintf(stdout, "%X", (unsigned int)args[arg_idx++]);
            }
            break;
        }
        case 'c': {
            fprintf(stdout, "%c", (char)args[arg_idx++]);
            break;
        }
        case 's': {
            int str_addr = args[arg_idx++];
            if (str_addr >= 0 && str_addr < vm->local_bytes) {
                fprintf(stdout, "%s", (const char*)(vm->local_mem + str_addr));
            } else if (str_addr >= 0 && str_addr < 65536) {
                /* Might be a string pool offset - check via local_mem */
                /* For now try to read from local_mem even if out of bounds */
                fprintf(stdout, "(str@%d)", str_addr);
            } else {
                fprintf(stdout, "(bad str 0x%x)", str_addr);
            }
            break;
        }
        case 'f':
        case 'F': {
            /* Float/double args: stored as int32_t bits for float, or we convert from int */
            if (has_precision && precision > 0) {
                char fmtbuf[32];
                snprintf(fmtbuf, sizeof(fmtbuf), "%%.%df", precision);
                int32_t bits = args[arg_idx++];
                float fval;
                memcpy(&fval, &bits, 4);
                fprintf(stdout, fmtbuf, (double)fval);
            } else {
                int32_t bits = args[arg_idx++];
                float fval;
                memcpy(&fval, &bits, 4);
                fprintf(stdout, "%f", (double)fval);
            }
            break;
        }
        case 'e':
        case 'E': {
            int32_t bits = args[arg_idx++];
            float fval;
            memcpy(&fval, &bits, 4);
            fprintf(stdout, *p == 'e' ? "%e" : "%E", (double)fval);
            break;
        }
        case 'g':
        case 'G': {
            int32_t bits = args[arg_idx++];
            float fval;
            memcpy(&fval, &bits, 4);
            fprintf(stdout, "%g", (double)fval);
            break;
        }
        case 'p': {
            fprintf(stdout, "0x%p", (void*)(uintptr_t)args[arg_idx++]);
            break;
        }
        case 'n': {
            /* %n writes count to int* - not supported, skip */
            break;
        }
        default:
            /* Unknown format specifier - print literally */
            buf[buf_len++] = '%';
            buf[buf_len++] = *p;
            if (buf_len >= 255) FLUSH;
            break;
        }
    }
    FLUSH;
    fflush(stdout);
}

int vm_run(VM *vm, BytecodeModule *bc) {
    if (!vm || !bc || !bc->code) return -1;

    BCInsn *code = bc->code;
    int     count = bc->count;

    vm->running = 1;
    vm->pc = (vm->func_addrs && vm->func_count > 0) ? vm->func_addrs[0] : 0;
    if (vm->pc < 0 || vm->pc >= count) vm->pc = 0;

    while (vm->running && vm->pc >= 0 && vm->pc < count) {
        BCInsn *insn = &code[vm->pc];
        int32_t a, b;
        int64_t la, lb;
        double  da, db;

        if (vm->debug_on) {
            fprintf(stderr, "[VM] pc=%d op=%d\n", vm->pc, insn->op);
        }

        switch (insn->op) {

        /* ========================================================== */
        /*  NOP / COMMENT                                              */
        /* ========================================================== */
        case BC_NOP:
        case BC_COMMENT:
            vm->pc++;
            break;

        /* ========================================================== */
        /*  Int32 ALU                                                  */
        /* ========================================================== */
        case BC_LOADI:
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = insn->src1;
            vm->pc++;
            break;

        case BC_MOV:
            if (insn->dst >= 0 && insn->dst < vm->reg_count &&
                insn->src1 >= 0 && insn->src1 < vm->reg_count)
                vm->regs[insn->dst] = vm->regs[insn->src1];
            vm->pc++;
            break;

        case BC_ADD:
        case BC_SUB:
        case BC_MUL:
        case BC_DIV:
        case BC_MOD:
        case BC_AND:
        case BC_OR:
        case BC_XOR:
        case BC_SHL:
        case BC_SHR:
            a = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            b = (insn->src2 >= 0 && insn->src2 < vm->reg_count) ? vm->regs[insn->src2] : insn->src2;
            if (insn->dst < 0 || insn->dst >= vm->reg_count) { vm->pc++; break; }
            switch (insn->op) {
            case BC_ADD: vm->regs[insn->dst] = a + b; break;
            case BC_SUB: vm->regs[insn->dst] = a - b; break;
            case BC_MUL: vm->regs[insn->dst] = a * b; break;
            case BC_DIV: if (b == 0) { vm_error(vm, insn, "div by zero"); break; } vm->regs[insn->dst] = a / b; break;
            case BC_MOD: if (b == 0) { vm_error(vm, insn, "mod by zero"); break; } vm->regs[insn->dst] = a % b; break;
            case BC_AND: vm->regs[insn->dst] = a & b; break;
            case BC_OR:  vm->regs[insn->dst] = a | b; break;
            case BC_XOR: vm->regs[insn->dst] = a ^ b; break;
            case BC_SHL: vm->regs[insn->dst] = a << b; break;
            case BC_SHR: vm->regs[insn->dst] = (int32_t)((uint32_t)a >> b); break;
            default: break;
            }
            vm->pc++;
            break;

        case BC_NEG:
            a = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : 0;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = -a;
            vm->pc++;
            break;

        case BC_NOT:
            a = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : 0;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = !a;
            vm->pc++;
            break;

        case BC_BNOT:
            a = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : 0;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = ~a;
            vm->pc++;
            break;

        case BC_EQ: case BC_NE: case BC_LT: case BC_LE: case BC_GT: case BC_GE:
            a = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            b = (insn->src2 >= 0 && insn->src2 < vm->reg_count) ? vm->regs[insn->src2] : insn->src2;
            if (insn->dst >= 0 && insn->dst < vm->reg_count) {
                switch (insn->op) {
                case BC_EQ: vm->regs[insn->dst] = (a == b) ? 1 : 0; break;
                case BC_NE: vm->regs[insn->dst] = (a != b) ? 1 : 0; break;
                case BC_LT: vm->regs[insn->dst] = (a < b) ? 1 : 0; break;
                case BC_LE: vm->regs[insn->dst] = (a <= b) ? 1 : 0; break;
                case BC_GT: vm->regs[insn->dst] = (a > b) ? 1 : 0; break;
                case BC_GE: vm->regs[insn->dst] = (a >= b) ? 1 : 0; break;
                default: break;
                }
            }
            vm->pc++;
            break;

        /* ========================================================== */
        /*  Int64 ALU                                                  */
        /* ========================================================== */
        case BC_LOADL: {
            int64_t val = ((int64_t)(uint32_t)(int32_t)insn->src1 << 32)
                        | (uint32_t)(int32_t)insn->src2;
            if (insn->dst >= 0 && insn->dst < vm->lreg_count)
                vm->lregs[insn->dst] = val;
            vm->pc++;
            break;
        }

        case BC_MOVL:
            if (insn->dst >= 0 && insn->dst < vm->lreg_count &&
                insn->src1 >= 0 && insn->src1 < vm->lreg_count)
                vm->lregs[insn->dst] = vm->lregs[insn->src1];
            vm->pc++;
            break;

        case BC_LADD: case BC_LSUB: case BC_LMUL: case BC_LDIV: case BC_LMOD:
            la = (insn->src1 >= 0 && insn->src1 < vm->lreg_count) ? vm->lregs[insn->src1] : (int64_t)insn->src1;
            lb = (insn->src2 >= 0 && insn->src2 < vm->lreg_count) ? vm->lregs[insn->src2] : (int64_t)insn->src2;
            if (insn->dst >= 0 && insn->dst < vm->lreg_count) {
                switch (insn->op) {
                case BC_LADD: vm->lregs[insn->dst] = la + lb; break;
                case BC_LSUB: vm->lregs[insn->dst] = la - lb; break;
                case BC_LMUL: vm->lregs[insn->dst] = la * lb; break;
                case BC_LDIV: if (lb == 0) { vm_error(vm, insn, "int64 div by zero"); break; } vm->lregs[insn->dst] = la / lb; break;
                case BC_LMOD: if (lb == 0) { vm_error(vm, insn, "int64 mod by zero"); break; } vm->lregs[insn->dst] = la % lb; break;
                default: break;
                }
            }
            vm->pc++;
            break;

        case BC_LNEG:
            la = (insn->src1 >= 0 && insn->src1 < vm->lreg_count) ? vm->lregs[insn->src1] : 0;
            if (insn->dst >= 0 && insn->dst < vm->lreg_count)
                vm->lregs[insn->dst] = -la;
            vm->pc++;
            break;

        case BC_LEQ: case BC_LNE: case BC_LLT: case BC_LLE: case BC_LGT: case BC_LGE:
            la = (insn->src1 >= 0 && insn->src1 < vm->lreg_count) ? vm->lregs[insn->src1] : (int64_t)insn->src1;
            lb = (insn->src2 >= 0 && insn->src2 < vm->lreg_count) ? vm->lregs[insn->src2] : (int64_t)insn->src2;
            if (insn->dst >= 0 && insn->dst < vm->lreg_count) {
                switch (insn->op) {
                case BC_LEQ: vm->lregs[insn->dst] = (la == lb) ? 1 : 0; break;
                case BC_LNE: vm->lregs[insn->dst] = (la != lb) ? 1 : 0; break;
                case BC_LLT: vm->lregs[insn->dst] = (la < lb) ? 1 : 0; break;
                case BC_LLE: vm->lregs[insn->dst] = (la <= lb) ? 1 : 0; break;
                case BC_LGT: vm->lregs[insn->dst] = (la > lb) ? 1 : 0; break;
                case BC_LGE: vm->lregs[insn->dst] = (la >= lb) ? 1 : 0; break;
                default: break;
                }
            }
            vm->pc++;
            break;

        /* ========================================================== */
        /*  Float ALU                                                  */
        /* ========================================================== */
        case BC_LOADF: {
            float fval;
            uint32_t bits = (uint32_t)(int32_t)insn->src1;
            memcpy(&fval, &bits, 4);
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count)
                vm->fregs[insn->dst_f] = (double)fval;
            vm->pc++;
            break;
        }

        case BC_LOADD: {
            double dval;
            uint64_t bits = ((uint64_t)(uint32_t)(int32_t)insn->src1 << 32)
                          | (uint32_t)(int32_t)insn->src2;
            memcpy(&dval, &bits, 8);
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count)
                vm->fregs[insn->dst_f] = dval;
            vm->pc++;
            break;
        }

        case BC_MOVF:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count)
                vm->fregs[insn->dst_f] = vm->fregs[insn->src1_f];
            vm->pc++;
            break;

        case BC_FADD: case BC_FSUB: case BC_FMUL: case BC_FDIV:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count &&
                insn->src2_f >= 0 && insn->src2_f < vm->freg_count) {
                da = vm->fregs[insn->src1_f];
                db = vm->fregs[insn->src2_f];
                switch (insn->op) {
                case BC_FADD: vm->fregs[insn->dst_f] = da + db; break;
                case BC_FSUB: vm->fregs[insn->dst_f] = da - db; break;
                case BC_FMUL: vm->fregs[insn->dst_f] = da * db; break;
                case BC_FDIV: 
                    if (db == 0.0) { vm_error(vm, insn, "float div by zero"); break; }
                    vm->fregs[insn->dst_f] = da / db; break;
                default: break;
                }
            }
            vm->pc++;
            break;

        case BC_FNEG:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count)
                vm->fregs[insn->dst_f] = -vm->fregs[insn->src1_f];
            vm->pc++;
            break;

        case BC_FEQ: case BC_FNE: case BC_FLT: case BC_FLE: case BC_FGT: case BC_FGE:
            if (insn->src1_f >= 0 && insn->src1_f < vm->freg_count &&
                insn->src2_f >= 0 && insn->src2_f < vm->freg_count) {
                da = vm->fregs[insn->src1_f];
                db = vm->fregs[insn->src2_f];
                int result = 0;
                switch (insn->op) {
                case BC_FEQ: result = (da == db) ? 1 : 0; break;
                case BC_FNE: result = (da != db) ? 1 : 0; break;
                case BC_FLT: result = (da < db) ? 1 : 0; break;
                case BC_FLE: result = (da <= db) ? 1 : 0; break;
                case BC_FGT: result = (da > db) ? 1 : 0; break;
                case BC_FGE: result = (da >= db) ? 1 : 0; break;
                default: break;
                }
                if (insn->dst >= 0 && insn->dst < vm->reg_count)
                    vm->regs[insn->dst] = result;
            }
            vm->pc++;
            break;

        /* ========================================================== */
        /*  Memory — flat byte-addressable                             */
        /* ========================================================== */
        case BC_LOADA:
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = insn->src1;
            vm->pc++;
            break;

        case BC_LOAD: {
            int addr = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 4)) break;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = *(int32_t*)(vm->local_mem + addr);
            vm->pc++;
            break;
        }

        case BC_STORE: {
            int addr = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : insn->dst;
            int32_t val = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 4)) break;
            *(int32_t*)(vm->local_mem + addr) = val;
            vm->pc++;
            break;
        }

        case BC_LOAD8: {
            int addr = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 1)) break;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = (int32_t)*(int8_t*)(vm->local_mem + addr);
            vm->pc++;
            break;
        }

        case BC_STORE8: {
            int addr = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : insn->dst;
            int32_t val = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 1)) break;
            *(int8_t*)(vm->local_mem + addr) = (int8_t)val;
            vm->pc++;
            break;
        }

        case BC_LOAD16: {
            int addr = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 2)) break;
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = (int32_t)*(int16_t*)(vm->local_mem + addr);
            vm->pc++;
            break;
        }

        case BC_STORE16: {
            int addr = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : insn->dst;
            int32_t val = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 2)) break;
            *(int16_t*)(vm->local_mem + addr) = (int16_t)val;
            vm->pc++;
            break;
        }

        case BC_LOAD64: {
            int addr = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 8)) break;
            if (insn->dst >= 0 && insn->dst < vm->lreg_count)
                vm->lregs[insn->dst] = *(int64_t*)(vm->local_mem + addr);
            vm->pc++;
            break;
        }

        case BC_STORE64: {
            int addr = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : insn->dst;
            int64_t val = (insn->src1 >= 0 && insn->src1 < vm->lreg_count) ? vm->lregs[insn->src1] : (int64_t)insn->src1;
            if (!check_mem(vm, insn, addr, 8)) break;
            *(int64_t*)(vm->local_mem + addr) = val;
            vm->pc++;
            break;
        }

        case BC_FLOAD: {
            int addr = (insn->src1 >= 0 && insn->src1 < vm->reg_count) ? vm->regs[insn->src1] : insn->src1;
            if (!check_mem(vm, insn, addr, 8)) break;
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count)
                vm->fregs[insn->dst_f] = *(double*)(vm->local_mem + addr);
            vm->pc++;
            break;
        }

        case BC_FSTR: {
            int addr = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : insn->dst;
            double val = (insn->src1_f >= 0 && insn->src1_f < vm->freg_count) ? vm->fregs[insn->src1_f] : 0.0;
            if (!check_mem(vm, insn, addr, 8)) break;
            *(double*)(vm->local_mem + addr) = val;
            vm->pc++;
            break;
        }

        /* ========================================================== */
        /*  Type Conversions                                           */
        /* ========================================================== */
        case BC_I2L:
            if (insn->dst >= 0 && insn->dst < vm->lreg_count &&
                insn->src1 >= 0 && insn->src1 < vm->reg_count)
                vm->lregs[insn->dst] = (int64_t)vm->regs[insn->src1];
            vm->pc++;
            break;

        case BC_I2F:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1 >= 0 && insn->src1 < vm->reg_count)
                vm->fregs[insn->dst_f] = (double)(float)vm->regs[insn->src1];
            vm->pc++;
            break;

        case BC_I2D:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1 >= 0 && insn->src1 < vm->reg_count)
                vm->fregs[insn->dst_f] = (double)vm->regs[insn->src1];
            vm->pc++;
            break;

        case BC_L2I:
            if (insn->dst >= 0 && insn->dst < vm->reg_count &&
                insn->src1 >= 0 && insn->src1 < vm->lreg_count)
                vm->regs[insn->dst] = (int32_t)vm->lregs[insn->src1];
            vm->pc++;
            break;

        case BC_L2F:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1 >= 0 && insn->src1 < vm->lreg_count)
                vm->fregs[insn->dst_f] = (double)(float)vm->lregs[insn->src1];
            vm->pc++;
            break;

        case BC_L2D:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1 >= 0 && insn->src1 < vm->lreg_count)
                vm->fregs[insn->dst_f] = (double)vm->lregs[insn->src1];
            vm->pc++;
            break;

        case BC_F2I:
            if (insn->dst >= 0 && insn->dst < vm->reg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count)
                vm->regs[insn->dst] = (int32_t)vm->fregs[insn->src1_f];
            vm->pc++;
            break;

        case BC_F2L:
            if (insn->dst >= 0 && insn->dst < vm->lreg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count)
                vm->lregs[insn->dst] = (int64_t)vm->fregs[insn->src1_f];
            vm->pc++;
            break;

        case BC_F2D:
        case BC_D2F:
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count &&
                insn->src1_f >= 0 && insn->src1_f < vm->freg_count)
                vm->fregs[insn->dst_f] = vm->fregs[insn->src1_f];
            vm->pc++;
            break;

        /* ========================================================== */
        /*  Control Flow                                               */
        /* ========================================================== */
        case BC_JMP:
            vm->pc = insn->dst;
            break;

        case BC_BR:
            a = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : 0;
            vm->pc = a ? insn->src1 : insn->src2;
            break;

        case BC_CALL:
            if (insn->dst >= 0 && insn->dst < vm->func_count && vm->func_addrs) {
                if (vm->ret_stack_top + 1 >= vm->ret_stack_size) {
                    vm_error(vm, insn, "return stack overflow");
                    break;
                }
                vm->ret_stack[++vm->ret_stack_top] = vm->pc + 1;
                vm->pc = vm->func_addrs[insn->dst];
            } else {
                vm_error(vm, insn, "invalid function call");
            }
            break;

        case BC_RET:
        case BC_RETI:
        case BC_RETL:
        case BC_RETF:
            if (vm->ret_stack_top >= 0) {
                vm->pc = vm->ret_stack[vm->ret_stack_top--];
            } else {
                vm->running = 0;
            }
            break;

        /* ========================================================== */
        /*  Stack ops                                                  */
        /* ========================================================== */
        case BC_PUSH:
            if (vm->stack_top + 1 >= vm->stack_size) {
                vm_error(vm, insn, "stack overflow");
                break;
            }
            a = (insn->dst >= 0 && insn->dst < vm->reg_count) ? vm->regs[insn->dst] : 0;
            vm->stack[++vm->stack_top] = a;
            vm->pc++;
            break;

        case BC_POP:
            if (vm->stack_top < 0) {
                vm_error(vm, insn, "stack underflow");
                break;
            }
            if (insn->dst >= 0 && insn->dst < vm->reg_count)
                vm->regs[insn->dst] = vm->stack[vm->stack_top--];
            vm->pc++;
            break;

        case BC_PUSHF:
            if (vm->fstack_top + 1 >= vm->fstack_size) {
                vm_error(vm, insn, "float stack overflow");
                break;
            }
            da = (insn->dst_f >= 0 && insn->dst_f < vm->freg_count) ? vm->fregs[insn->dst_f] : 0.0;
            vm->fstack[++vm->fstack_top] = da;
            vm->pc++;
            break;

        case BC_POPF:
            if (vm->fstack_top < 0) {
                vm_error(vm, insn, "float stack underflow");
                break;
            }
            if (insn->dst_f >= 0 && insn->dst_f < vm->freg_count)
                vm->fregs[insn->dst_f] = vm->fstack[vm->fstack_top--];
            vm->pc++;
            break;

        /* ========================================================== */
        /*  printf handler                                             */
        /* ========================================================== */
        case BC_PRINTF: {
            /* Format string is at addr in reg[insn->src1] */
            /* insn->dst contains arg count */
            /* insn->src2 contains format string address (or reg index) */
            int arg_count = insn->dst;
            int fmt_addr;
            
            if (insn->src2 >= 0 && insn->src2 < vm->reg_count) {
                fmt_addr = vm->regs[insn->src2];
            } else {
                fmt_addr = insn->src2;
            }
            
            /* Collect args from stack (pushed in order) */
            int32_t args[64];
            int nargs = arg_count > 64 ? 64 : arg_count;
            
            /* Args were pushed in order, so pop them in reverse */
            for (int i = nargs - 1; i >= 0; i--) {
                if (vm->stack_top >= 0) {
                    args[i] = vm->stack[vm->stack_top--];
                } else {
                    args[i] = 0;
                }
            }
            
            const char *fmt_str;
            if (fmt_addr >= 0 && fmt_addr < vm->local_bytes) {
                fmt_str = (const char*)(vm->local_mem + fmt_addr);
            } else if (bc->string_pool && bc->string_count > 0 && fmt_addr < bc->string_pool_size) {
                fmt_str = (const char*)(bc->string_pool + fmt_addr);
            } else {
                fmt_str = "(bad format string)";
            }
            
            vm_printf_format(vm, insn, fmt_str, args, nargs);
            vm->regs[0] = nargs; /* printf returns char count */
            vm->pc++;
            break;
        }

        case BC_HALT:
            vm->running = 0;
            break;

        case BC_TRAP:
            fprintf(stderr, "[VM] TRAP at pc=%d\n", vm->pc);
            vm_dump_state(vm, stderr);
            vm->running = 0;
            break;

        default:
            fprintf(stderr, "[VM] Unknown opcode %d at pc=%d\n", insn->op, vm->pc);
            vm->running = 0;
            break;
        }
    }

    /* Return value is r0 (standard return register) */
    return vm->regs[0];
}
