#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"

/* GCL Garbage Collector — reference counting + mark-sweep placeholder */

static int gc_initialized = 0;
static int gc_alloc_count = 0;

void gcl_gc_init(void) {
    gc_initialized = 1;
    gc_alloc_count = 0;
}

int gcl_gc_is_init(void) {
    return gc_initialized;
}

void gcl_gc_track(void *ptr) {
    if (!ptr) return;
    gc_alloc_count++;
    (void)ptr;
}

int gcl_gc_get_count(void) {
    return gc_alloc_count;
}
