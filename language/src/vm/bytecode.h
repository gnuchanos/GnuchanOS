#ifndef GCL_BYTECODE_H
#define GCL_BYTECODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ================================================================= */
/*  GCL Register Bytecode — instruction set                           */
/* ================================================================= */

typedef enum {
    BC_NOP,
    /* Aritmetik — int32 */
    BC_ADD, BC_SUB, BC_MUL, BC_DIV, BC_MOD,
    BC_AND, BC_OR,  BC_XOR, BC_SHL, BC_SHR,
    BC_NEG, BC_NOT, BC_BNOT,
    /* Karşılaştırma — int32 */
    BC_EQ,  BC_NE,  BC_LT,  BC_LE,  BC_GT,  BC_GE,
    /* Aritmetik — int64 */
    BC_LADD, BC_LSUB, BC_LMUL, BC_LDIV, BC_LMOD,
    BC_LNEG,
    /* Karşılaştırma — int64 */
    BC_LEQ, BC_LNE, BC_LLT, BC_LLE, BC_LGT, BC_LGE,
    /* Aritmetik — float/double (32-bit float registers) */
    BC_FADD, BC_FSUB, BC_FMUL, BC_FDIV,
    BC_FNEG,
    /* Karşılaştırma — float */
    BC_FEQ, BC_FNE, BC_FLT, BC_FLE, BC_FGT, BC_FGE,
    /* Bellek — flat byte-addressed local memory */
    BC_LOAD,    /* reg = *(int32_t*)(local_mem + reg)       int32 load */
    BC_STORE,   /* *(int32_t*)(local_mem + reg) = reg       int32 store */
    BC_LOAD8,   /* reg = *(int8_t*)(local_mem + reg)        sign-extended */
    BC_STORE8,  /* *(int8_t*)(local_mem + reg) = (int8_t)reg */
    BC_LOAD16,  /* reg = *(int16_t*)(local_mem + reg)       sign-extended */
    BC_STORE16, /* *(int16_t*)(local_mem + reg) = (int16_t)reg */
    BC_LOAD64,  /* lreg = *(int64_t*)(local_mem + reg)      int64 load */
    BC_STORE64, /* *(int64_t*)(local_mem + reg) = lreg      int64 store */
    BC_FLOAD,   /* freg = *(double*)(local_mem + reg)       float/double load */
    BC_FSTR,    /* *(double*)(local_mem + reg) = freg       float/double store */
    /* Immediate loads */
    BC_LOADI,  /* reg = imm (int32) */
    BC_LOADL,  /* lreg = imm (int64 packed in src1:src2) */
    BC_LOADF,  /* freg = imm (float bits in src1) */
    BC_LOADD,  /* freg = imm (double bits in src1:src2) */
    BC_LOADA,  /* reg = byte_offset_of_local_slot */
    BC_MOV,    /* reg = reg (int32) */
    BC_MOVL,   /* lreg = lreg (int64) */
    BC_MOVF,   /* freg = freg (double) */
    /* Tür dönüşümleri */
    BC_I2L,    /* int32 → int64  */
    BC_I2F,    /* int32 → float (→freg as double) */
    BC_I2D,    /* int32 → double */
    BC_L2I,    /* int64 → int32  */
    BC_L2F,    /* int64 → float */
    BC_L2D,    /* int64 → double */
    BC_F2I,    /* float/double → int32 */
    BC_F2L,    /* float/double → int64 */
    BC_F2D,    /* float → double (same reg file, just copy) */
    BC_D2F,    /* double → float */
    /* Kontrol */
    BC_JMP,   /* jmp addr          */
    BC_BR,    /* br cond_reg, t_addr, f_addr */
    BC_CALL,  /* reg = call func_index  */
    BC_RET,   /* ret  (value in r0/lr0/fr0) */
    BC_RETI,  /* ret with int32 return value in r0 */
    BC_RETL,  /* ret with int64 return value in lr0 */
    BC_RETF,  /* ret with double return value in fr0 */
    /* Stack ops */
    BC_PUSH,  /* push reg (int32) */
    BC_POP,   /* reg = pop */
    BC_PUSHF, /* push freg */
    BC_POPF,  /* freg = pop */
    /* System */
    BC_PRINTF,  /* printf(string_reg) — built-in */
    BC_TRAP,  /* runtime break */
    BC_HALT,
    BC_COMMENT,
} BCOpcode;

typedef struct {
    BCOpcode op;
    int32_t  dst;      /* hedef register (int) */
    int32_t  src1;     /* kaynak register / imm / addr */
    int32_t  src2;     /* kaynak register / imm / addr */
    int32_t  src3;     /* ek (opsiyonel) */
    int32_t  dst_f;    /* hedef float register (-1 = kullanma) */
    int32_t  src1_f;   /* kaynak float register */
    int32_t  src2_f;   /* kaynak float register */
    /* Metadata */
    int      insn_id;
    const char *src_file;
    int      src_line;
    int      src_col;
    const char *func_name;
} BCInsn;

typedef struct {
    BCInsn *code;
    int     count;
    int     capacity;
    int     register_count;   /* kullanılan int register sayısı */
    int     float_reg_count;  /* kullanılan float register sayısı */
    int     local_bytes;      /* flat local memory size needed */
    /* String pool for format strings (printf) */
    uint8_t *string_pool;
    int      string_pool_size;
    int      string_pool_cap;
    int      string_count;    /* number of strings stored */
} BytecodeModule;

/* ----------------------------------------------------------------- */
/*  API                                                               */
/* ----------------------------------------------------------------- */
BytecodeModule *bc_module_new(void);
void            bc_module_free(BytecodeModule *m);
int             bc_emit(BytecodeModule *m, BCOpcode op,
                        int32_t dst, int32_t src1, int32_t src2, int32_t src3);
int             bc_emit_f(BytecodeModule *m, BCOpcode op,
                         int32_t dst, int32_t src1, int32_t src2, int32_t src3,
                         int32_t dst_f, int32_t src1_f, int32_t src2_f);
void            bc_set_location(BytecodeModule *m, int idx,
                                int insn_id, const char *file,
                                int line, int col, const char *func);
/* String pool: add a null-terminated string, returns byte offset in pool */
int             bc_add_string(BytecodeModule *m, const char *str, int len);
void            bc_dump(BytecodeModule *m, FILE *out);

#endif /* GCL_BYTECODE_H */
