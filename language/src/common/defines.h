#ifndef GCL_DEFINES_H
#define GCL_DEFINES_H

#include "types.h"

typedef struct DefineEntry {
    const char         *name;
    const char         *value;
    struct DefineEntry *next;
} DefineEntry;

void defines_init(void);
void defines_set(const char *name, const char *value);
const char *defines_get(const char *name);

/* track extern symbols for codegen: "extern const char *name;" */
void defines_add_extern(const char *name);

/* iterate externs: returns 1 and sets *name, advances internal cursor.
   Pass NULL to reset cursor. Returns 0 when done. */
int defines_next_extern(const char **name);

void defines_free(void);

#endif
