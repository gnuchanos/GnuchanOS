#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Common/gcl_common.h"

/* GCL GC operations — collect, sweep, finalize */

extern int gcl_gc_get_count(void);

void gcl_gc_collect(void) {
    /* Placeholder: no real GC cycle yet.
     * In the future this will do mark-sweep. */
}

void gcl_gc_sweep(void) {
    /* Placeholder */
}

void gcl_gc_shutdown(void) {
    /* Final cleanup */
    gcl_gc_collect();
}
