#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"

/* GCL Memory subsystem — placeholder init.
 * The real memory management uses the arena from Common. */

static int gcl_mem_initialized = 0;

void gcl_mem_init(void) {
    gcl_mem_initialized = 1;
}

int gcl_mem_is_init(void) {
    return gcl_mem_initialized;
}
