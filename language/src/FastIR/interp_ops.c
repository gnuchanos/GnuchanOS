#include <stdint.h>
#include "../SharedPipeline/Ir/gcl_ir.h"

/* Forward declarations from interp_core.c */
typedef struct InterpState InterpState;
typedef struct { int kind; int64_t i_val; double f_val; const char *s_val; } Val;

extern Val val_int(int64_t v);
extern Val val_float(double v);
extern int64_t val_to_int(Val v);
extern void push(InterpState *st, Val v);
extern Val pop(InterpState *st);

/* Arithmetic operations */
void op_add(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) + val_to_int(b)));
}

void op_sub(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) - val_to_int(b)));
}

void op_mul(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) * val_to_int(b)));
}

void op_div(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    int64_t bv = val_to_int(b);
    push(st, val_int(bv ? val_to_int(a) / bv : 0));
}

void op_mod(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    int64_t bv = val_to_int(b);
    push(st, val_int(bv ? val_to_int(a) % bv : 0));
}

void op_neg(InterpState *st) {
    Val a = pop(st);
    push(st, val_int(-val_to_int(a)));
}

/* Logical operations */
void op_not(InterpState *st) {
    Val a = pop(st);
    extern int val_truthy(Val v);
    push(st, val_int(!val_truthy(a)));
}

void op_and(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    extern int val_truthy(Val v);
    push(st, val_int(val_truthy(a) && val_truthy(b)));
}

void op_or(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    extern int val_truthy(Val v);
    push(st, val_int(val_truthy(a) || val_truthy(b)));
}

/* Comparison operations */
void op_eq(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) == val_to_int(b)));
}

void op_neq(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) != val_to_int(b)));
}

void op_lt(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) < val_to_int(b)));
}

void op_gt(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) > val_to_int(b)));
}

void op_lte(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) <= val_to_int(b)));
}

void op_gte(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) >= val_to_int(b)));
}

/* Bitwise operations */
void op_bitnot(InterpState *st) {
    Val a = pop(st);
    push(st, val_int(~val_to_int(a)));
}

void op_bitand(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) & val_to_int(b)));
}

void op_bitor(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) | val_to_int(b)));
}

void op_bitxor(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) ^ val_to_int(b)));
}

void op_shl(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) << val_to_int(b)));
}

void op_shr(InterpState *st) {
    Val b = pop(st);
    Val a = pop(st);
    push(st, val_int(val_to_int(a) >> val_to_int(b)));
}
