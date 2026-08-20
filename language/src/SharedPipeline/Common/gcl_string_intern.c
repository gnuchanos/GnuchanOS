#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcl_common.h"

static uint32_t fnv1a(const char *str, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)str[i];
        h *= 16777619u;
    }
    return h;
}

void gcl_intern_init(GclStringIntern *intern, GclArena *arena) {
    memset(intern->buckets, 0, sizeof(intern->buckets));
    intern->arena = arena;
}

const char *gcl_intern(GclStringIntern *intern, const char *str, size_t len) {
    uint32_t hash = fnv1a(str, len);
    uint32_t idx = hash % GCL_INTERN_BUCKETS;
    GclInternEntry *e = intern->buckets[idx];
    while (e) {
        if (e->hash == hash && e->len == len && memcmp(e->str, str, len) == 0) {
            return e->str;
        }
        e = e->next;
    }
    char *dup = gcl_arena_strndup(intern->arena, str, len);
    GclInternEntry *entry = (GclInternEntry *)gcl_arena_alloc(intern->arena, sizeof(GclInternEntry));
    entry->str = dup;
    entry->len = len;
    entry->hash = hash;
    entry->next = intern->buckets[idx];
    intern->buckets[idx] = entry;
    return dup;
}

const char *gcl_intern_cstr(GclStringIntern *intern, const char *str) {
    if (!str) return NULL;
    return gcl_intern(intern, str, strlen(str));
}

void gcl_intern_free(GclStringIntern *intern) {
    memset(intern->buckets, 0, sizeof(intern->buckets));
    intern->arena = NULL;
}
