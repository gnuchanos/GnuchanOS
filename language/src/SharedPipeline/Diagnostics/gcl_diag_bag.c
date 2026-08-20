#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../gcl.h"

void gcl_diag_bag_init(GclDiagBag *bag) {
    memset(bag, 0, sizeof(GclDiagBag));
}

void gcl_diag_add(GclDiagBag *bag, GclDiagLevel level, int line, int col,
                  const char *file, const char *fmt, ...) {
    if (bag->count >= GCL_DIAG_MAX) return;
    GclDiagEntry *e = &bag->entries[bag->count];
    e->level = level;
    e->line = line;
    e->col = col;
    if (file) {
        size_t flen = strlen(file);
        if (flen >= sizeof(e->file)) flen = sizeof(e->file) - 1;
        memcpy(e->file, file, flen);
        e->file[flen] = '\0';
    } else {
        e->file[0] = '\0';
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
    bag->count++;
    if (level == GCL_DIAG_ERROR) bag->error_count++;
    else if (level == GCL_DIAG_WARNING) bag->warning_count++;
}
