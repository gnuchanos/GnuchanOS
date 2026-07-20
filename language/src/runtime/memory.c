#include <stdlib.h>

// ============================================================
// GCL Runtime Memory — Stub (v0.1)
// ============================================================

#include <stddef.h>

void *area(size_t size) {
    // Stub: just wraps malloc for now
    return malloc(size);
}

void arena_init(void) {}
void arena_reset(void) {}
