/*
 * gcl_parser.h — GCL AST tipleri.
 */

#ifndef GCL_PARSER_H
#define GCL_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include "gcl_lexer.h"

typedef struct GclStmt GclStmt;

typedef enum {
    AST_EXPR_INT = 0,
    AST_EXPR_FLOAT,
    AST_EXPR_STRING,
    AST_EXPR_VAR,
    AST_EXPR_BINOP,
    AST_EXPR_UNOP,
    AST_EXPR_CALL,
    AST_EXPR_ASSIGN,
    AST_EXPR_MEMBER,
    AST_EXPR_ARRAY,
    AST_EXPR_INIT_LIST,
} GclExprKind;

typedef struct GclExpr GclExpr;

/* Dizi/member index erişimi: expr[expr] */
typedef struct {
    GclExpr *name;        /* dizi ismi (AST_EXPR_VAR) */
    GclExpr *index;       /* index ifadesi */
    char *member_name;    /* varsa .member (req: arr[i].member) */
} GclArrayAccess;

typedef enum {
    OP_ADD = 0, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
    OP_NOT, OP_BITAND, OP_BITOR, OP_BITXOR, OP_BITNOT,
    OP_ASSIGN,
} GclBinOp;

typedef enum {
    STMT_EXPR = 0,
    STMT_VAR_DECL,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_RETURN,
    STMT_BLOCK,
    STMT_FUNC_DECL,
    STMT_SWITCH,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_TYPEDEF,
    STMT_ENUM,
    STMT_STRUCT_DECL,
} GclStmtKind;

typedef struct {
    char *name;
    char **params;
    int param_count;
    char *return_type;   /* fonksiyon dönüş tipi (Hello, void, int, ...) */
    /* body: ilk stmt bloğu */
    GclStmt *body;
} GclFuncDecl;

typedef struct {
    char *name;
    char *type_name;
    GclExpr *init;
    int array_size;   /* 0=array değil, -1=char[] boş, N=char[N] boyut */
    int array_inner;  /* optional second dimension for char arrays: char a[3][20] */
    int is_pointer;   /* 1=char *ptr */
    int is_const;     /* 1=const değişken (atama yapılamaz) */
    int is_global;    /* 1=global değişken (blok dışına taşmaz) */
} GclVarDecl;

typedef struct {
    GclExpr *cond;
    GclStmt *then;
    GclStmt *else_;
} GclIf;

typedef struct {
    GclExpr *cond;
    GclStmt *body;
} GclWhile;

typedef struct {
    GclExpr *var;
    GclExpr *cond;
    GclExpr *inc;
    GclStmt *body;
} GclFor;

typedef struct {
    char *name;
    GclStmt **cases;
    GclExpr **case_vals;
    int case_count;
    GclStmt *default_case;
} GclSwitch;

/* typedef int number; — alias -> base */
typedef struct {
    char *alias_name;
    char *base_type;
} GclTypedefInfo;

/* enum Day { MONDAY, TUESDAY, ... } — üye adları + başlangıç değeri */
typedef struct {
    char *enum_name;
    char **const_names;
    int const_count;
} GclEnumInfo;

/* struct Student { char name[20]; int age; } */
typedef struct {
    char *struct_name;
    char **member_names;
    char **member_types;
    int member_count;
} GclStructInfo;

/* Struct değişken member'ı */
typedef struct GclStructValue GclStructValue;
struct GclStructValue {
    char *name;
    double num;
    char *str;
    int is_string;
    char *decl_type;   /* üyenin bildirim tipi (int8, float32, ...) — clamp için */
    struct GclStructValue *next;
    struct GclStructValue *members; /* nested struct değeri */
};

struct GclStmt {
    GclStmtKind kind;
    struct GclStmt *next;
    union {
        GclExpr *expr;
        GclVarDecl var_decl;
        GclIf if_;
        GclWhile while_;
        GclFor for_;
        GclFuncDecl func_decl;
        GclSwitch switch_;
        struct { GclStmt **stmts; int count; } block;
        struct { GclExpr *expr; char *type_name; } ret;
        char *break_label;
        GclTypedefInfo typedef_info;
        GclEnumInfo enum_info;
        GclStructInfo struct_info;
    } u;
};

typedef struct {
    GclStmt **stmts;
    int count;
} GclProgram;

struct GclExpr {
    GclExprKind kind;
    struct GclExpr *left;
    struct GclExpr *right;
    GclBinOp op;
    double num;
    char *str;
    char *name;
    struct GclExpr **args;
    int arg_count;
    char *member_name;
};

GclProgram *gcl_parse(GclTokenList *tokens, char **error_msg);
void gcl_program_free(GclProgram *prog);

#endif /* GCL_PARSER_H */
