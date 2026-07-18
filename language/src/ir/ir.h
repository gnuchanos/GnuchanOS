#ifndef GCL_IR_H
#define GCL_IR_H

#include "../type/type.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ================================================================= */
/*  GCL IR — Intermediate Representation (3-Address Code)            */
/* ================================================================= */

/* ----------------------------------------------------------------- */
/*  IR Operand Types                                                  */
/* ----------------------------------------------------------------- */
typedef enum {
    IR_NONE,        /* bos operand */
    IR_CONST_INT,   /* integer sabit */
    IR_CONST_FLOAT, /* float sabit */
    IR_CONST_STRING,/* string sabit (data.name = processed string) */
    IR_LOCAL,       /* local degisken (stack slot) */
    IR_GLOBAL,      /* global degisken */
    IR_TEMP,        /* gecici (SSA temp) */
    IR_FUNC,        /* fonksiyon adi */
    IR_LABEL,       /* label (branch target) */
} IROperandKind;

typedef struct {
    IROperandKind kind;
    GclType      *type;      /* operand tipi */
    union {
        int64_t  int_val;
        double   float_val;
        int      slot;       /* LOCAL / TEMP slot index */
        int      label_id;   /* LABEL / FUNC */
        const char *name;    /* GLOBAL / FUNC / CONST_STRING name */
    } data;
} IROperand;

/* ----------------------------------------------------------------- */
/*  IR Instruction Types                                              */
/* ----------------------------------------------------------------- */
typedef enum {
    IR_NOP,
    /* Aritmetik / mantiksal */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_AND, IR_OR,  IR_XOR, IR_SHL, IR_SHR,
    /* Unary */
    IR_NEG, IR_NOT, IR_BNOT,
    /* Karsilastirma */
    IR_EQ,  IR_NE,  IR_LT,  IR_LE,  IR_GT,  IR_GE,
    /* Bellek */
    IR_ALLOCA,      /* stack alani ayir */
    IR_LOAD,        /* *ptr → temp */
    IR_STORE,       /* val → *ptr */
    IR_LEA,         /* &(base + offset) → temp */
    IR_MEMSET,      /* blok sifirlama */
    /* Kontrol akisi */
    IR_JMP,         /* unconditional jump */
    IR_BR,          /* conditional branch */
    IR_CALL,        /* fonksiyon cagrisi */
    IR_RET,         /* return */
    IR_PHI,         /* SSA phi node */
    /* Stack operations */
    IR_PUSH,        /* push temp/const onto operand stack */
    /* Built-in functions */
    IR_PRINTF,      /* printf(format_string_operand, arg_count) - args already on stack */
    /* Meta */
    IR_LABEL_DECL,  /* label bildirimi */
    IR_COMMENT,
} IROpcode;

typedef struct IRInsn {
    IROpcode   opcode;
    IROperand  dst;       /* hedef (varsa) */
    IROperand  src1;      /* kaynak 1 */
    IROperand  src2;      /* kaynak 2 */
    IROperand  src3;      /* kaynak 3 (opsiyonel) */
    /* Metadata: Instruction ID */
    int        insn_id;   /* benzersiz instruction ID */
    const char *src_file;
    int        src_line;
    int        src_col;
    const char *func_name; /* ait oldugu fonksiyon */
    struct IRInsn *prev;
    struct IRInsn *next;
} IRInsn;

/* ----------------------------------------------------------------- */
/*  IR Basic Block                                                    */
/* ----------------------------------------------------------------- */
typedef struct IRBlock {
    int         id;
    IRInsn     *first;
    IRInsn     *last;
    int         insn_count;
    const char *label;         /* block label (NULL ise entry) */
    /* CFG baglantilari */
    struct IRBlock *succ[2];   /* en fazla 2 successor */
    struct IRBlock *pred[16];  /* predecessor listesi */
    int         npred;
    struct IRBlock *next;      /* linked list */
} IRBlock;

/* ----------------------------------------------------------------- */
/*  IR Function                                                       */
/* ----------------------------------------------------------------- */
typedef struct IRFunction {
    const char *name;
    GclType    *ret_type;
    GclType   **param_types;
    int         param_count;
    int         local_count;   /* kac tane local var */
    int         temp_count;    /* gecici degisken sayisi */
    IRBlock    *entry_block;
    IRBlock    *exit_block;
    IRBlock    *blocks;        /* block listesi */
    int         block_count;
    struct IRFunction *next;
} IRFunction;

/* ----------------------------------------------------------------- */
/*  IR Module (tum program)                                           */
/* ----------------------------------------------------------------- */
typedef struct {
    IRFunction *functions;
    int         func_count;
    int         next_insn_id;  /* global instruction ID counter */
} IRModule;

/* ----------------------------------------------------------------- */
/*  API                                                               */
/* ----------------------------------------------------------------- */

/* Module */
IRModule *ir_module_new(void);
void      ir_module_free(IRModule *mod);

/* Function */
IRFunction *ir_func_new(const char *name, GclType *ret, GclType **params, int pcount);
void        ir_func_free(IRFunction *f);

/* Blocks */
IRBlock *ir_block_new(int id, const char *label);
void     ir_block_add_insn(IRBlock *block, IRInsn *insn);

/* Instructions (builder) */
IRInsn *ir_insn_new(IROpcode op, IROperand dst, IROperand src1, IROperand src2);
void    ir_insn_free(IRInsn *insn);

/* Operand olusturucular */
IROperand ir_const_int(int64_t val);
IROperand ir_const_float(double val);
IROperand ir_const_string(const char *str);
IROperand ir_local(int slot, GclType *type);
IROperand ir_temp(int slot, GclType *type);
IROperand ir_label(int id);
IROperand ir_func(const char *name);
IROperand ir_none(void);

/* Debug / dump */
void ir_dump_module(IRModule *mod, FILE *out);
void ir_dump_func(IRFunction *f, FILE *out);
void ir_dump_insn(IRInsn *insn, FILE *out);
void ir_dump_operand(IROperand *op, FILE *out);

#endif /* GCL_IR_H */
