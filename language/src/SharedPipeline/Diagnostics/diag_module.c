
/* === From: gcl_diag_bag.c === */
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


/* === From: gcl_diag_print.c === */
#include <stdio.h>
#include "../gcl.h"

#define COLOR_RED    "\033[38;2;160;59;255m"
#define COLOR_YELLOW "\033[38;2;200;180;0m"
#define COLOR_BLUE   "\033[38;2;95;5;179m"
#define COLOR_RESET  "\033[0m"

void gcl_diag_print_all(const GclDiagBag *bag) {
    for (int i = 0; i < bag->count; i++) {
        const GclDiagEntry *e = &bag->entries[i];
        const char *color = COLOR_BLUE;
        const char *label = "debug";
        if (e->level == GCL_DIAG_ERROR) { color = COLOR_RED; label = "error"; }
        else if (e->level == GCL_DIAG_WARNING) { color = COLOR_YELLOW; label = "warning"; }
        fprintf(stderr, "%s%s:%d:%d: %s: %s%s\n",
                color, e->file, e->line, e->col, label, e->message, COLOR_RESET);
    }
}


