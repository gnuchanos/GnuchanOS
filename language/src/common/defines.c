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

void defines_init(void) {
    head = NULL;
    extern_head = NULL;
    extern_iter = NULL;
}

void defines_set(const char *name, const char *value) {
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

void defines_add_extern(const char *name) {
    /* dedup: skip if already in list */
    for (ExternEntry *ee = extern_head; ee; ee = ee->next)
        if (strcmp(ee->name, name) == 0)
            return;

    ExternEntry *entry = malloc(sizeof(ExternEntry));
    entry->name = strdup(name);
    entry->next = extern_head;
    extern_head = entry;
}

int defines_next_extern(const char **name) {
    /* pass NULL to reset iterator to head */
    if (name == NULL) { extern_iter = extern_head; return 0; }
    /* end of list: stop, do NOT auto-restart */
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
