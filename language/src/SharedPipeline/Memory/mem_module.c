
/* === From: gcl_mem_alloc.c === */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Common/gcl_common.h"

/* GCL runtime allocator — wraps arena for interpreter use.
 * Tracks allocations for GC integration. */

#define GCL_MEM_TRACK_MAX 4096

static void *gcl_mem_tracked[GCL_MEM_TRACK_MAX];
static int   gcl_mem_track_count = 0;

void *gcl_mem_alloc(size_t size) {
    void *ptr = gcl_safe_malloc(size);
    if (gcl_mem_track_count < GCL_MEM_TRACK_MAX) {
        gcl_mem_tracked[gcl_mem_track_count++] = ptr;
    }
    return ptr;
}

void gcl_mem_free(void *ptr) {
    if (!ptr) return;
    for (int i = 0; i < gcl_mem_track_count; i++) {
        if (gcl_mem_tracked[i] == ptr) {
            gcl_mem_tracked[i] = gcl_mem_tracked[--gcl_mem_track_count];
            break;
        }
    }
    free(ptr);
}

void gcl_mem_free_all(void) {
    for (int i = 0; i < gcl_mem_track_count; i++) {
        free(gcl_mem_tracked[i]);
        gcl_mem_tracked[i] = NULL;
    }
    gcl_mem_track_count = 0;
}


/* === From: gcl_mem_init.c === */
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


