#ifndef GCL_IR_TO_BC_H
#define GCL_IR_TO_BC_H

#include "bytecode.h"
#include "../ir/ir.h"
#include "../ir/ir_builder.h"

/* ================================================================= */
/*  GCL IR → Register Bytecode Translator                                 */
/* ================================================================= */

/* Per-type register file tracking */
typedef struct {
    int next_reg;   /* int32 register allocator */
    int next_lreg;  /* int64 register allocator */
    int next_freg;  /* float/double register allocator */

    /* Temp slot → register mapping for each type */
    int *temp_to_reg;    /* int32 */
    int *temp_to_lreg;   /* int64 */
    int *temp_to_freg;   /* float/double */
    int  temp_count;

    /* Local slot base for each type */
    int local_reg_base;
    int local_lreg_base;
    int local_freg_base;
} IRToBC_RegAlloc;

typedef struct {
    BytecodeModule *bc;
    IRToBC_RegAlloc rega;
    /* Function info */
    const char   **func_names;
    int           *func_indices;
    int            func_count;
    /* Label → bytecode offset mapping */
    int           *label_offsets;
    int            label_count;
} IRToBC;

/* API */
IRToBC *ir_to_bc_new(void);
void    ir_to_bc_free(IRToBC *t);
BytecodeModule *ir_to_bc_translate(IRModule *ir_mod, IRBuilder *builder);
void    ir_to_bc_set_strings(IRToBC *t, const char **strings, int count);

#endif /* GCL_IR_TO_BC_H */
