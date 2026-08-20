#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

/* Type checker context */
static GclDiagBag *tc_diag = NULL;
static const char *tc_file = NULL;

void gcl_typecheck_init(GclDiagBag *diag, const char *filepath) {
    tc_diag = diag;
    tc_file = filepath ? filepath : "<input>";
}

GclDiagBag *gcl_typecheck_get_diag(void) {
    return tc_diag;
}

const char *gcl_typecheck_get_file(void) {
    return tc_file;
}
