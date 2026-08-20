#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcl_common.h"

void *gcl_safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return ptr;
}

void *gcl_safe_calloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory (%zu * %zu bytes)\n", count, size);
        exit(1);
    }
    return ptr;
}

void *gcl_safe_realloc(void *ptr, size_t size) {
    void *newptr = realloc(ptr, size);
    if (!newptr && size > 0) {
        fprintf(stderr, "gcl: fatal: out of memory on realloc (%zu bytes)\n", size);
        exit(1);
    }
    return newptr;
}

void gcl_safe_free(void *ptr) {
    free(ptr);
}
