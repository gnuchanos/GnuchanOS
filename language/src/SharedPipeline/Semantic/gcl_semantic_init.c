#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

/* Semantic analysis context */
typedef struct {
    GclDiagBag *diag;
    const char *filepath;
} GclSemantic;

static GclSemantic g_sem;

void gcl_semantic_init(GclDiagBag *diag, const char *filepath) {
    g_sem.diag = diag;
    g_sem.filepath = filepath ? filepath : "<input>";
}

GclDiagBag *gcl_semantic_get_diag(void) {
    return g_sem.diag;
}

const char *gcl_semantic_get_file(void) {
    return g_sem.filepath;
}
