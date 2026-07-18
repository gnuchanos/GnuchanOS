#ifndef GCL_VM_H
#define GCL_VM_H

#include <stdio.h>
#include "bytecode.h"

/* ================================================================= */
/*  GCL Register VM — executes BytecodeModule                         */
/* ================================================================= */

typedef struct {
    /* Int32 registers */
    int32_t *regs;
    int      reg_count;
    /* Int64 registers */
    int64_t *lregs;
    int      lreg_count;
    /* Float/double registers (always stored as double) */
    double  *fregs;
    int      freg_count;
    /* Flat byte-addressable local memory */
    uint8_t *local_mem;
    int      local_bytes;
    /* Operand stack (int32) */
    int32_t *stack;
    int      stack_size;
    int      stack_top;
    /* Float stack */
    double  *fstack;
    int      fstack_size;
    int      fstack_top;
    int      pc;
    int      running;
    /* Function call table: bytecode offset per function index */
    int     *func_addrs;
    int      func_count;
    /* Return stack for calls */
    int     *ret_stack;
    int      ret_stack_top;
    int      ret_stack_size;
    /* Debug */
    int      debug_on;
    const char *last_file;
    int      last_line;
    int      last_col;
    /* Return value registers (r0, lr0, fr0 hold main() return) */
} VM;

/* ----------------------------------------------------------------- */
/*  API                                                               */
/* ----------------------------------------------------------------- */
VM  *vm_new(int reg_count, int lreg_count, int freg_count,
            int stack_size, int fstack_size,
            int local_bytes);
void vm_free(VM *vm);
int  vm_run(VM *vm, BytecodeModule *bc);
void vm_set_debug(VM *vm, int on);
void vm_dump_state(VM *vm, FILE *out);

#endif /* GCL_VM_H */
