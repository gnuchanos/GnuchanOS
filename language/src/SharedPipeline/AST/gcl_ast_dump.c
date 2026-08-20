#include <stdio.h>
#include "gcl_ast.h"

void gcl_ast_dump(const GclAstNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s", gcl_ast_kind_name(node->kind));
    if (node->str_value) printf(" \"%s\"", node->str_value);
    if (node->kind == AST_INT_LIT) printf(" %lld", (long long)node->int_value);
    if (node->kind == AST_FLOAT_LIT) printf(" %f", node->float_value);
    if (node->type_name) printf(" [type:%s%s]", node->type_name, node->is_pointer ? "*" : "");
    printf(" @%d:%d\n", node->line, node->col);
    for (int i = 0; i < node->child_count; i++) {
        gcl_ast_dump(node->children[i], indent + 1);
    }
}
