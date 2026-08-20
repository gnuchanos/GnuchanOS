#include <string.h>
#include "gcl_ast.h"

GclAstNode *gcl_ast_new(GclArena *arena, GclAstKind kind, int line, int col) {
    GclAstNode *node = (GclAstNode *)gcl_arena_alloc(arena, sizeof(GclAstNode));
    memset(node, 0, sizeof(GclAstNode));
    node->kind = kind;
    node->line = line;
    node->col = col;
    return node;
}

void gcl_ast_add_child(GclAstNode *parent, GclAstNode *child) {
    if (!parent || !child) return;
    if (parent->child_count >= GCL_AST_MAX_CHILDREN) return;
    parent->children[parent->child_count++] = child;
}
