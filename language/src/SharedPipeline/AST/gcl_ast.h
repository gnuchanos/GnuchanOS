/*
 * gcl_ast.h — AST node definitions for GCL
 */

#ifndef GCL_AST_H
#define GCL_AST_H

#include "../Common/gcl_common.h"
#include "../Common/gcl_token.h"

/* ── AST Node Kinds ──────────────────────────────── */

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_VAR_DECL,
    AST_PARAM,
    AST_BLOCK,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_EXPR_STMT,
    AST_CALL_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_ASSIGN_EXPR,
    AST_MEMBER_EXPR,
    AST_INDEX_EXPR,
    AST_IDENT_EXPR,
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_STRING_LIT,
    AST_CHAR_LIT,
    AST_BOOL_LIT,
    AST_NULL_LIT,
    AST_PP_INCLUDE,
    AST_PP_EXTERN,
    AST_PP_REGISTER,
    AST_PP_DEFINE,
    AST_PP_IF,
    AST_PP_ERROR,
    AST_PP_WARNING,
    AST_PP_DEBUG,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_CLASS_DECL,
    AST_TYPEDEF_DECL,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_SWITCH_STMT,
    AST_CASE_CLAUSE,
    AST_CAST_EXPR,
    AST_SIZEOF_EXPR,
    AST_ARRAY_LIT,
    AST_KIND_COUNT
} GclAstKind;

/* ── AST Node ────────────────────────────────────── */

#define GCL_AST_MAX_CHILDREN 128

typedef struct GclAstNode {
    GclAstKind          kind;
    GclToken            token;        /* primary token */
    const char         *str_value;    /* interned string (name, op, etc.) */
    int64_t             int_value;
    double              float_value;
    int                 child_count;
    struct GclAstNode  *children[GCL_AST_MAX_CHILDREN];
    /* type info (filled by semantic/typecheck) */
    const char         *type_name;
    int                 is_pointer;
    int                 array_dim;
    int                 is_const;      /* 1 if variable declared with const */
    int                 line;
    int                 col;
} GclAstNode;

/* ── API ─────────────────────────────────────────── */

GclAstNode *gcl_ast_new(GclArena *arena, GclAstKind kind, int line, int col);
void        gcl_ast_add_child(GclAstNode *parent, GclAstNode *child);
const char *gcl_ast_kind_name(GclAstKind kind);
void        gcl_ast_dump(const GclAstNode *node, int indent);

#endif /* GCL_AST_H */
