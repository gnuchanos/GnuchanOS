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
