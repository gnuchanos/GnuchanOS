#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcl_common.h"

static GclArenaBlock *arena_new_block(size_t min_size) {
    size_t cap = GCL_ARENA_BLOCK_SIZE;
    if (min_size > cap) cap = min_size;
    GclArenaBlock *b = (GclArenaBlock *)malloc(sizeof(GclArenaBlock) + cap);
    if (!b) {
        fprintf(stderr, "gcl: fatal: arena OOM (%zu bytes)\n", cap);
        exit(1);
    }
    b->next = NULL;
    b->used = 0;
    b->capacity = cap;
    return b;
}

void gcl_arena_init(GclArena *arena) {
    arena->head = arena_new_block(GCL_ARENA_BLOCK_SIZE);
    arena->current = arena->head;
    arena->total_allocated = 0;
}

void *gcl_arena_alloc(GclArena *arena, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;
    GclArenaBlock *cur = arena->current;
    if (cur->used + aligned > cur->capacity) {
        GclArenaBlock *nb = arena_new_block(aligned);
        cur->next = nb;
        arena->current = nb;
        cur = nb;
    }
    void *ptr = cur->data + cur->used;
    cur->used += aligned;
    arena->total_allocated += aligned;
    return ptr;
}

char *gcl_arena_strdup(GclArena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *dst = (char *)gcl_arena_alloc(arena, len + 1);
    memcpy(dst, str, len + 1);
    return dst;
}

char *gcl_arena_strndup(GclArena *arena, const char *str, size_t len) {
    if (!str) return NULL;
    char *dst = (char *)gcl_arena_alloc(arena, len + 1);
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

void gcl_arena_free(GclArena *arena) {
    GclArenaBlock *b = arena->head;
    while (b) {
        GclArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    arena->head = NULL;
    arena->current = NULL;
    arena->total_allocated = 0;
}
