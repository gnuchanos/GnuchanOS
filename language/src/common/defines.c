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

#define MAX_RECURSION_DEPTH 32
#define MAX_VISITED 64

/* built-in platform defines */
static void add_builtin_defines(void) {
#ifdef _WIN32
    defines_set("windows", "1");
#elif defined(__linux__)
    defines_set("linux", "1");
    defines_set("gnu", "1");
#elif defined(__APPLE__)
    defines_set("apple", "1");
#endif
    defines_set("GCL_VERSION", "1");
}

void defines_init(void) {
    head = NULL;
    extern_head = NULL;
    extern_iter = NULL;
    add_builtin_defines();
}

void defines_set(const char *name, const char *value) {
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

/* Recursive helper: expand value, up to depth limit.
   visited/vcount track names seen in this resolution chain for cycle detection. */
static const char *defines_get_r(const char *name, int depth, const char **visited, int *vcount) {
    if (depth > MAX_RECURSION_DEPTH) return NULL;
    /* Cycle detection: check if already visited in this chain */
    for (int i = 0; i < *vcount; i++) {
        if (strcmp(visited[i], name) == 0) return NULL;  /* cycle detected */
    }
    for (DefineEntry *e = head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            /* Try to expand the value itself — if value looks like an identifier, resolve it */
            const char *val = e->value;
            if (val && val[0] &&
                ((val[0] >= 'a' && val[0] <= 'z') ||
                 (val[0] >= 'A' && val[0] <= 'Z') ||
                 val[0] == '_')) {
                /* Check if it resolves to something different (avoid infinite loop A→A) */
                int is_self = (strcmp(val, name) == 0);
                if (!is_self) {
                    /* Check if val is itself a defined name */
                    int found = 0;
                    for (DefineEntry *inner = head; inner; inner = inner->next) {
                        if (strcmp(inner->name, val) == 0) { found = 1; break; }
                    }
                    if (found) {
                        /* Push current name onto visited stack before recursing */
                        if (*vcount < MAX_VISITED) {
                            visited[*vcount] = name;
                            (*vcount)++;
                        }
                        const char *expanded = defines_get_r(val, depth + 1, visited, vcount);
                        if (*vcount > 0) (*vcount)--;
                        else break;
                        if (expanded) return expanded;
                    }
                }
            }
            return val;
        }
    }
    return NULL;
}

const char *defines_get(const char *name) {
    const char *visited[MAX_VISITED];
    int vcount = 0;
    return defines_get_r(name, 0, visited, &vcount);
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
