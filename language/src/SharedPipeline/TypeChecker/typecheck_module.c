
/* === From: gcl_typecheck_init.c === */
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


/* === From: gcl_typecheck_walk.c === */
#include <stdio.h>
#include <string.h>
#include "../Common/gcl_common.h"
#include "../AST/gcl_ast.h"
#include "../gcl.h"

extern GclDiagBag *gcl_typecheck_get_diag(void);
extern const char *gcl_typecheck_get_file(void);

/* Basic type checking walk — validates types on declarations */

static void walk_node(const GclAstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case AST_FUNC_DECL:
        if (node->type_name && strcmp(node->type_name, "void") != 0) {
            /* Non-void function: check if body has return */
            int has_body = 0;
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i] && node->children[i]->kind == AST_BLOCK) {
                    has_body = 1;
                }
            }
            (void)has_body;
        }
        break;
    case AST_VAR_DECL:
        /* Type validation placeholder */
        break;
    default:
        break;
    }

    for (int i = 0; i < node->child_count; i++) {
        walk_node(node->children[i]);
    }
}

int gcl_typecheck_walk(const GclAstNode *root) {
    if (!root) return 0;
    walk_node(root);
    GclDiagBag *diag = gcl_typecheck_get_diag();
    return diag->error_count;
}


