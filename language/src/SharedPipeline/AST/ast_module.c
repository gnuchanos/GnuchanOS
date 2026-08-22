#include <string.h>
#include <stdio.h>
#include "gcl_ast.h"

/* Part 1: AST node creation and manipulation */
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

/* Part 2: AST kind names */
const char *gcl_ast_kind_name(GclAstKind kind) {
    switch (kind) {
    case AST_PROGRAM:        return "Program";
    case AST_FUNC_DECL:      return "FuncDecl";
    case AST_VAR_DECL:       return "VarDecl";
    case AST_PARAM:          return "Param";
    case AST_BLOCK:          return "Block";
    case AST_RETURN_STMT:    return "Return";
    case AST_IF_STMT:        return "If";
    case AST_WHILE_STMT:     return "While";
    case AST_FOR_STMT:       return "For";
    case AST_EXPR_STMT:      return "ExprStmt";
    case AST_CALL_EXPR:      return "Call";
    case AST_BINARY_EXPR:    return "BinaryExpr";
    case AST_UNARY_EXPR:     return "UnaryExpr";
    case AST_ASSIGN_EXPR:    return "Assign";
    case AST_MEMBER_EXPR:    return "Member";
    case AST_INDEX_EXPR:     return "Index";
    case AST_IDENT_EXPR:     return "Ident";
    case AST_INT_LIT:        return "IntLit";
    case AST_FLOAT_LIT:      return "FloatLit";
    case AST_STRING_LIT:     return "StringLit";
    case AST_CHAR_LIT:       return "CharLit";
    case AST_BOOL_LIT:       return "BoolLit";
    case AST_NULL_LIT:       return "NullLit";
    case AST_PP_INCLUDE:     return "PPInclude";
    case AST_PP_EXTERN:      return "PPExtern";
    case AST_PP_REGISTER:    return "PPRegister";
    case AST_PP_DEFINE:      return "PPDefine";
    case AST_PP_IF:          return "PPIf";
    case AST_PP_ERROR:       return "PPError";
    case AST_PP_WARNING:     return "PPWarning";
    case AST_PP_DEBUG:       return "PPDebug";
    case AST_STRUCT_DECL:    return "StructDecl";
    case AST_ENUM_DECL:      return "EnumDecl";
    case AST_CLASS_DECL:     return "ClassDecl";
    case AST_TYPEDEF_DECL:   return "TypedefDecl";
    case AST_BREAK_STMT:     return "Break";
    case AST_CONTINUE_STMT:  return "Continue";
    case AST_SWITCH_STMT:    return "Switch";
    case AST_CASE_CLAUSE:    return "Case";
    case AST_CAST_EXPR:      return "Cast";
    case AST_SIZEOF_EXPR:    return "Sizeof";
    case AST_ARRAY_LIT:      return "ArrayLit";
    case AST_KIND_COUNT:     return "???";
    }
    return "???";
}

/* Part 3: AST dump/debug */
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
