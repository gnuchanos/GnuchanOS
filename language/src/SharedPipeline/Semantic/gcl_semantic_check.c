#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

/* Basic semantic check — walks AST and validates structure */

extern GclDiagBag *gcl_semantic_get_diag(void);
extern const char *gcl_semantic_get_file(void);

static void check_node(const GclAstNode *node) {
    if (!node) return;
    GclDiagBag *diag = gcl_semantic_get_diag();
    const char *file = gcl_semantic_get_file();

    switch (node->kind) {
    case AST_FUNC_DECL:
        if (!node->str_value) {
            gcl_diag_add(diag, GCL_DIAG_ERROR, node->line, node->col, file,
                         "function declaration missing name");
        }
        break;
    case AST_VAR_DECL:
        if (!node->str_value) {
            gcl_diag_add(diag, GCL_DIAG_ERROR, node->line, node->col, file,
                         "variable declaration missing name");
        }
        break;
    case AST_PP_ERROR:
        if (node->str_value) {
            gcl_diag_add(diag, GCL_DIAG_ERROR, node->line, node->col, file,
                         "%s", node->str_value);
        }
        break;
    case AST_PP_WARNING:
        if (node->str_value) {
            gcl_diag_add(diag, GCL_DIAG_WARNING, node->line, node->col, file,
                         "%s", node->str_value);
        }
        break;
    default:
        break;
    }

    for (int i = 0; i < node->child_count; i++) {
        check_node(node->children[i]);
    }
}

int gcl_semantic_check(const GclAstNode *root) {
    if (!root) return 0;
    check_node(root);
    GclDiagBag *diag = gcl_semantic_get_diag();
    return diag->error_count;
}
