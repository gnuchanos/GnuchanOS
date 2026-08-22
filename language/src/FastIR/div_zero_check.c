/* Fix division/modulo by zero - use this in gcl_interp.c main loops */

/* For IR_DIV case: */
if (bv == 0) {
    fprintf(stderr, "gcl: error: division by zero\n");
    push(&st, val_int(0));
} else {
    push(&st, val_int(val_to_int(a) / bv));
}

/* For IR_MOD case: */
if (bv == 0) {
    fprintf(stderr, "gcl: error: modulo by zero\n");
    push(&st, val_int(0));
} else {
    push(&st, val_int(val_to_int(a) % bv));
}
