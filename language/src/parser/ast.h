#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "../lexer/token.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION_DEF,
    AST_VAR_DECL,
    AST_IF,
    AST_FOR,
    AST_WHILE,
    AST_RETURN,
    AST_BLOCK,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_FUNC_CALL,
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_STRING,
    AST_IDENTIFIER,
} AstType;

typedef struct AstNode {
    AstType type;
    union {
        struct { struct AstNode **stmts; int count; } program;
        struct { struct AstNode *type; const char *name; struct AstNode *body; struct AstNode **params; int pcount; } func;
        struct { struct AstNode *type; const char *name; struct AstNode *init; } var;
        struct { struct AstNode *cond; struct AstNode *then; struct AstNode *els; } if_stmt;
        struct { struct AstNode *init; struct AstNode *cond; struct AstNode *step; struct AstNode *body; } for_stmt;
        struct { struct AstNode *cond; struct AstNode *body; } while_stmt;
        struct { struct AstNode *val; } ret;
        struct { struct AstNode **stmts; int count; } block;
        struct { int op; struct AstNode *left; struct AstNode *right; } bin;
        struct { int op; struct AstNode *operand; } un;
        struct { const char *name; struct AstNode **args; int acount; } call;
        long int_val;
        double float_val;
        const char *str_val;
        const char *id;
    } data;
    int line, col;
} AstNode;

AstNode *ast_alloc(AstType type, int line, int col);
void ast_free(AstNode *node);
void ast_dump(AstNode *node, FILE *out, int depth);

#endif
