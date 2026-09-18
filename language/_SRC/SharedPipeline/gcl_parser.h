/*
 * gcl_parser.h — GCL AST types.
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
    /* B9: `i++` İFADENİN DEĞERİ eski değerdir, `++i` yeni değerdir. Eskiden
       ikisi de `i = i + 1` atamasına indirgeniyordu ve postfix de YENİ değeri
       döndürüyordu: `int j = i++;` → j = 6 (C'de 5). */
    AST_EXPR_POSTINC,
    AST_EXPR_PREINC,
    /* B11 — C-tipi cast: `(int)7.5`. left = operand, str = target type name.
       Eskiden `(int)expr` bir değişken gibi ayrıştırılıp `expected ';'`
       veriyordu (yanlış sonuç üretmiyordu ama cast hiç desteklenmiyordu). */
    AST_EXPR_CAST,
} GclExprKind;

typedef struct GclExpr GclExpr;

/* Array/member index access: expr[expr] */
typedef struct {
    GclExpr *name;        /* array name (AST_EXPR_VAR) */
    GclExpr *index;       /* index expression */
    char *member_name;    /* optional .member (e.g. arr[i].member) */
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
    /* Hata yakalama (try/catch/finally/throw). */
    STMT_TRY,
    STMT_THROW,
    /* Temizlik (defer): govde blogu kapanirken (LIFO) calisan eylem. */
    STMT_DEFER,
} GclStmtKind;

/* TURN 26 — a function parameter is a NAME plus a TYPE. These used to be two
   PARALLEL arrays (`char **params` + `char **param_types`) that had to be kept
   in lockstep by hand. The struct-parameter branch wrote only the first one and
   grew the second with `realloc`, whose new tail is NOT zeroed, so a struct
   parameter that landed past a growth boundary left an UNINITIALIZED pointer in
   the type array; the runner then `strdup`'d and `free`'d it (undefined
   behaviour; latent on this build because the fresh heap happened to be zero).
   One array whose two fields are written TOGETHER, by a single helper, makes a
   half-written parameter structurally impossible. */
typedef struct {
    char *name;   /* parameter name (owned by the AST) */
    char *type;   /* declared type; NULL when it cannot be resolved (a
                     struct/typedef/native type name is a single token) */
} GclParam;

typedef struct {
    char *name;
    GclParam *params;    /* param_count entries; grow/shrink through helpers */
    int param_count;
    char *return_type;   /* function return type (Hello, void, int, ...) */
    /* body: first stmt block */
    GclStmt *body;
} GclFuncDecl;

typedef struct {
    char *name;
    char *type_name;
    GclExpr *init;
    int array_size;   /* 0=not an array, -1=empty char[], N=char[N] size */
    int array_inner;  /* optional second dimension for char arrays: char a[3][20] */
    int is_pointer;   /* 1=char *ptr */
    int is_const;     /* 1=const variable (cannot be assigned) */
    int is_global;    /* 1=global variable (does not leak out of the block) */
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
    char *name;           /* legacy: set when the condition is a plain variable */
    GclExpr *cond;        /* full condition expression (switch (v + 1), f(x), ...) */
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

/* enum Day { MONDAY, TUESDAY, ... } — member names + start value */
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

/* Struct variable member */
typedef struct GclStructValue GclStructValue;
struct GclStructValue {
    char *name;
    double num;
    char *str;
    int is_string;
    char *decl_type;   /* the member's declared type (int8, float32, ...) — for clamping */
    struct GclStructValue *next;
    struct GclStructValue *members; /* nested struct value */
};

struct GclStmt {
    GclStmtKind kind;
    struct GclStmt *next;
    /* Source position of the token that started this statement. The runtime
       reads these so a failure can name the file and line instead of printing
       a bare sentence with no location at all. */
    int line;
    int col;
    int len;
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
        /* try { body } catch (err_name) { catch_body } finally { finally_body }
           — `err_name`, `catch_body` ve `finally_body` NULL olabilir; parser
           en az bir `catch`/`finally` dali oldugunu dogrular. */
        struct {
            GclStmt *body;          /* try blogu (zorunlu) */
            char *err_name;         /* catch (err) icindeki ad; NULL = adsiz */
            GclStmt *catch_body;    /* NULL = catch dali yok */
            GclStmt *finally_body;  /* NULL = finally dali yok */
        } try_;
        /* throw <expr>; / raise <expr>;  — expr NULL ise "throw;" */
        struct { GclExpr *expr; } throw_;
        /* defer <stmt>;  — govde blogu/iterasyon kapanirken (LIFO) calisir. */
        struct { GclStmt *stmt; } defer_;
    } u;
};

typedef struct {
    GclStmt **stmts;
    int count;
} GclProgram;

struct GclExpr {
    GclExprKind kind;
    /* TRAP — a CALL keeps its callee in `left`, NEVER in `name`:

           call->left == AST_EXPR_VAR      plain call      -> callee in `left->name`
           call->left == AST_EXPR_MEMBER   qualified call  -> `left->member_name`
                                           (e.g. Raylib.InitWindow(...))

       `name` is only ever the identifier of an AST_EXPR_VAR. Reading
       `call->name` yields NULL and looks like a parser bug; a test written
       that way went red on a CORRECT parser, and the test — not the code — was
       the defect. Read the callee from `left` and nowhere else; the reference
       implementation is `callee_name()` in gcl_parser.c. */
    struct GclExpr *left;
    struct GclExpr *right;
    GclBinOp op;
    double num;
    char *str;
    char *name;
    struct GclExpr **args;
    int arg_count;
    char *member_name;
    /* Source position of the token that STARTED this expression. Filled in by
       the parser; read by the runtime so "undefined variable 'x'" can point at
       the line the user actually wrote. */
    int line;
    int col;
    int len;
};

/* Parse the token stream. `error_msg` receives one flattened message on
   failure (deprecated shape — use gcl_parse_diag()). */
GclProgram *gcl_parse(GclTokenList *tokens, char **error_msg);

/* Diagnostics-aware parser. On failure returns NULL and appends rich
   diagnostics to `diags` (may be NULL). Every diagnostic carries a stable
   code, a span with a real length, and usually a note and a hint. */
GclProgram *gcl_parse_diag(GclTokenList *tokens, struct GclDiagList *diags);

void gcl_program_free(GclProgram *prog);

#endif /* GCL_PARSER_H */
