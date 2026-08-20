/*
 * gcl_common.h — Common types, arena allocator, string intern, memory utils
 */

#ifndef GCL_COMMON_H
#define GCL_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Arena Allocator ───────────────────────────────── */

#define GCL_ARENA_BLOCK_SIZE (64 * 1024)

typedef struct GclArenaBlock {
    struct GclArenaBlock *next;
    size_t                used;
    size_t                capacity;
    char                  data[];
} GclArenaBlock;

typedef struct {
    GclArenaBlock *head;
    GclArenaBlock *current;
    size_t         total_allocated;
} GclArena;

void  gcl_arena_init(GclArena *arena);
void *gcl_arena_alloc(GclArena *arena, size_t size);
char *gcl_arena_strdup(GclArena *arena, const char *str);
char *gcl_arena_strndup(GclArena *arena, const char *str, size_t len);
void  gcl_arena_free(GclArena *arena);

/* ── String Intern ─────────────────────────────────── */

#define GCL_INTERN_BUCKETS 512

typedef struct GclInternEntry {
    struct GclInternEntry *next;
    const char            *str;
    size_t                 len;
    uint32_t               hash;
} GclInternEntry;

typedef struct {
    GclInternEntry *buckets[GCL_INTERN_BUCKETS];
    GclArena       *arena;
} GclStringIntern;

void        gcl_intern_init(GclStringIntern *intern, GclArena *arena);
const char *gcl_intern(GclStringIntern *intern, const char *str, size_t len);
const char *gcl_intern_cstr(GclStringIntern *intern, const char *str);
void        gcl_intern_free(GclStringIntern *intern);

/* ── Memory Utils ──────────────────────────────────── */

void *gcl_safe_malloc(size_t size);
void *gcl_safe_calloc(size_t count, size_t size);
void *gcl_safe_realloc(void *ptr, size_t size);
void  gcl_safe_free(void *ptr);

#endif /* GCL_COMMON_H */
