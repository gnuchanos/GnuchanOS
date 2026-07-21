#include "defines.h"
#include <stdlib.h>
#include <string.h>

static DefineEntry *head = NULL;

typedef struct ExternEntry {
    const char *name;
    struct ExternEntry *next;
} ExternEntry;

static ExternEntry *extern_head = NULL;
static ExternEntry *extern_iter = NULL;

/* built-in platform defines */
static void add_builtin_defines(void) {
    /* platform */
#ifdef _WIN32
    defines_set("windows", "1");
#elif defined(__linux__)
    defines_set("linux", "1");
    defines_set("gnu", "1");
#elif defined(__APPLE__)
    defines_set("apple", "1");
#endif

    /* GCL version */
    defines_set("GCL_VERSION", "1");
}

void defines_init(void) {
    head = NULL;
    extern_head = NULL;
    extern_iter = NULL;
    add_builtin_defines();
}

void defines_set(const char *name, const char *value) {
    /* overwrite if exists */
    for (DefineEntry *e = head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            free((void*)e->value);
            e->value = strdup(value);
            return;
        }
    }
    DefineEntry *e = malloc(sizeof(DefineEntry));
    e->name = strdup(name);
    e->value = strdup(value);
    e->next = head;
    head = e;
}

const char *defines_get(const char *name) {
    for (DefineEntry *e = head; e; e = e->next)
        if (strcmp(e->name, name) == 0)
            return e->value;
    return NULL;
}

void defines_undef(const char *name) {
    DefineEntry *prev = NULL;
    for (DefineEntry *e = head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            if (prev) prev->next = e->next;
            else head = e->next;
            free((void*)e->name);
            free((void*)e->value);
            free(e);
            return;
        }
        prev = e;
    }
}

int defines_exists(const char *name) {
    return defines_get(name) != NULL;
}

void defines_add_extern(const char *name) {
    for (ExternEntry *ee = extern_head; ee; ee = ee->next)
        if (strcmp(ee->name, name) == 0)
            return;

    ExternEntry *entry = malloc(sizeof(ExternEntry));
    entry->name = strdup(name);
    entry->next = extern_head;
    extern_head = entry;
}

int defines_next_extern(const char **name) {
    if (name == NULL) { extern_iter = extern_head; return 0; }
    if (extern_iter == NULL) return 0;
    *name = extern_iter->name;
    extern_iter = extern_iter->next;
    return 1;
}

void defines_free(void) {
    DefineEntry *e = head;
    while (e) { DefineEntry *n = e->next; free((void*)e->name); free((void*)e->value); free(e); e = n; }
    head = NULL;
    ExternEntry *ee = extern_head;
    while (ee) { ExternEntry *n = ee->next; free((void*)ee->name); free(ee); ee = n; }
    extern_head = NULL; extern_iter = NULL;
}
