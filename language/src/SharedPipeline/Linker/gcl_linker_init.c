#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"
#include "../gcl.h"

/* GCL Linker — resolves #include and #lib references.
 * For now this is a placeholder that logs resolved includes. */

void gcl_linker_init(void) {
    /* Nothing to initialize yet */
}

int gcl_linker_resolve(const char *filepath, GclDiagBag *diag) {
    (void)filepath;
    (void)diag;
    /* Placeholder: all includes resolved at parse time for now */
    return 0;
}
