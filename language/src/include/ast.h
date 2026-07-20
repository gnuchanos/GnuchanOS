#ifndef GCL_AST_H
#define GCL_AST_H

#include "tokens.h"
#include <stddef.h>

// ============================================================
// GCL AST Node Types
// ============================================================

typedef enum {
    // ── Top Level ─────────────────────────────────────────
    AST_PROGRAM = 0,           // translation unit (root)
    AST_PREP_INCLUDE,          // #include
    AST_PREP_LIB,              // #lib
    AST_PREP_EXTERN,           // #extern
    AST_PREP_DEFINE,           // #define
    AST_PREP_UNDEF,            // #undef
    AST_PREP_IFDEF,            // #ifdef
    AST_PREP_IFNDEF,           // #ifndef
    AST_PREP_IF,               // #if
    AST_PREP_ELIF,             // #elif
    AST_PREP_ELSE,             // #else
    AST_PREP_ENDIF,            // #endif
    AST_PREP_ERROR,            // #error
    AST_PREP_PRAGMA,           // #pragma
    AST_PREP_LINE,             // #line
    AST_GCL_COMMENT,           // # ...
    AST_GCL_COMMENT_BLOCK,     // #| ... |#
    AST_GCL_COMMENT_CPP,       // #// ...

    // ── Declarations ──────────────────────────────────────
    AST_VAR_DECL,              // int x = 5;
    AST_FUNC_DECL,             // int add(int, int);
    AST_FUNC_DEF,              // int add(int a, int b) { ... }
    AST_FUNC_PARAM,            // single parameter in function
    AST_STRUCT_DECL,           // struct Player { ... };
    AST_ENUM_DECL,             // enum Weapon { ... };
    AST_UNION_DECL,            // union Value { ... };
    AST_TYPEDEF_DECL,          // typedef ...;
    AST_ENUM_MEMBER,           // member in enum

    // ── Statements ────────────────────────────────────────
    AST_COMPOUND,              // { ... }
    AST_IF,                    // if (cond) stmt [else stmt]
    AST_FOR,                   // for (init; cond; incr) stmt
    AST_WHILE,                 // while (cond) stmt
    AST_DO_WHILE,              // do stmt while (cond)
    AST_SWITCH,                // switch (expr) stmt
    AST_CASE,                  // case expr: stmt
    AST_DEFAULT,               // default: stmt
    AST_RETURN,                // return [expr];
    AST_BREAK,                 // break;
    AST_CONTINUE,              // continue;
    AST_EXPR_STMT,             // expr;

    // ── Expressions ───────────────────────────────────────
    AST_BINARY,                // a + b, a = b, etc.
    AST_UNARY,                 // -a, !a, ++a, etc.
    AST_TERNARY,               // a ? b : c
    AST_CAST,                  // (type)expr
    AST_SIZEOF,                // sizeof(type) or sizeof expr
    AST_CALL,                  // func(args)
    AST_SUBSCRIPT,             // arr[index]
    AST_MEMBER,                // struct.member
    AST_MEMBER_PTR,            // ptr->member
    AST_ASSIGN,                // a = b (also compound: +=, etc.)
    AST_POSTFIX_INC,           // a++
    AST_POSTFIX_DEC,           // a--
    AST_PREFIX_INC,            // ++a
    AST_PREFIX_DEC,            // --a

    // ── Literals ──────────────────────────────────────────
    AST_INT_LITERAL,           // 5, 0xFF, 0b1010
    AST_FLOAT_LITERAL,         // 1.5f, 2.5
    AST_CHAR_LITERAL,          // 'A', '\n'
    AST_STRING_LITERAL,        // "hello"
    AST_IDENTIFIER,            // variable reference
    AST_BOOL_LITERAL,          // true / false

    // ── Array / Initializer ───────────────────────────────
    AST_ARRAY_INIT,            // { val0, val1, ... }
    AST_DESIGNATED_INIT,       // .member = val (future)
    AST_STRING_ARRAY,          // char *arr[] = {...}

    AST_COUNT
} GclAstNodeType;

// ── Forward declarations ──────────────────────────────────
typedef struct GclAstNode GclAstNode;
typedef struct GclAstList GclAstList;

// ── Type representation (forward) ─────────────────────────
typedef enum {
    TYPE_NONE = 0,
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_UNION,
    TYPE_TYPEDEF_ALIAS,
} GclTypeCategory;

typedef struct GclType {
    GclTypeCategory category;
    const char      *name;           // "int", "char", "Player", etc.
    struct GclType  *base_type;      // for pointers/arrays: pointee type
    int              array_size;     // for arrays: -1 = unspecified []
    int              is_unsigned;    // for primitives
    int              is_const;
    int              is_static;
    int              pointer_depth;  // 0 = not pointer, 1 = *, 2 = **, etc.
    // for function types:
    struct GclAstList *param_types;
    struct GclType    *return_type;
} GclType;

// ── Source Location ───────────────────────────────────────
typedef struct {
    int line;
    int col;
} GclLoc;

// ── AST Node ──────────────────────────────────────────────
struct GclAstNode {
    GclAstNodeType  type;
    GclLoc          loc;
    GclType        *value_type;      // type info (filled by semantic analysis)
    const char     *lexeme;          // original text (for identifiers/literals)
    int             length;

    // ── Node-specific data ────────────────────────────────
    union {
        // ── Literals ──────────────────────────────────────
        struct {
            long long int_val;
        } int_lit;

        struct {
            double float_val;
            int    is_long_double;
        } float_lit;

        struct {
            int  code;              // Unicode codepoint
            char escape;            // original escape if any
        } char_lit;

        struct {
            char *str;              // owned copy
        } string_lit;

        // ── Identifier ────────────────────────────────────
        struct {
            char *name;             // owned copy
        } ident;

        // ── Binary Expression ─────────────────────────────
        struct {
            GclTokenType op;
            GclAstNode  *lhs;
            GclAstNode  *rhs;
        } binary;

        // ── Unary Expression ──────────────────────────────
        struct {
            GclTokenType op;        // -, ~, !, ++, -- (prefix)
            GclAstNode  *operand;
        } unary;

        // ── Ternary ───────────────────────────────────────
        struct {
            GclAstNode *cond;
            GclAstNode *true_expr;
            GclAstNode *false_expr;
        } ternary;

        // ── Cast ──────────────────────────────────────────
        struct {
            GclType    *target_type;
            GclAstNode *expr;
        } cast;

        // ── Sizeof ────────────────────────────────────────
        struct {
            GclType    *target_type;  // for sizeof(type)
            GclAstNode *expr;         // for sizeof expr
        } sizeof_expr;

        // ── Function Call ─────────────────────────────────
        struct {
            GclAstNode  *callee;
            GclAstList  *args;
        } call;

        // ── Assignment ────────────────────────────────────
        struct {
            GclTokenType op;        // =, +=, -=, etc.
            GclAstNode  *lhs;
            GclAstNode  *rhs;
        } assign;

        // ── Variable Declaration ──────────────────────────
        struct {
            GclType    *var_type;
            char       *name;
            GclAstNode *init;        // optional initializer
            int         is_const;
            int         is_static;
        } var_decl;

        // ── Function ──────────────────────────────────────
        struct {
            GclType    *return_type;
            char       *name;
            GclAstList *params;
            GclAstNode *body;        // AST_COMPOUND
            int         is_variadic;
        } func;

        // ── Function Parameter ────────────────────────────
        struct {
            GclType *param_type;
            char    *name;
        } param;

        // ── Control Flow ──────────────────────────────────
        struct {
            GclAstNode *cond;
            GclAstNode *then_branch;
            GclAstNode *else_branch; // optional
        } if_stmt;

        struct {
            GclAstNode *init;        // optional
            GclAstNode *cond;        // optional
            GclAstNode *incr;        // optional
            GclAstNode *body;
        } for_stmt;

        struct {
            GclAstNode *cond;
            GclAstNode *body;
        } while_stmt;

        struct {
            GclAstNode *body;
            GclAstNode *cond;
        } do_while;

        struct {
            GclAstNode *expr;
            GclAstNode *body;        // AST_COMPOUND (switch body)
        } switch_stmt;

        struct {
            GclAstNode *value;       // case value (or NULL for default)
            GclAstNode *stmt;
        } case_stmt;

        struct {
            GclAstNode *expr;        // optional (for void return)
        } return_stmt;

        // ── Preprocessor ──────────────────────────────────
        struct {
            char *path;
            int   is_system;         // <> vs ""
        } prep_include;

        struct {
            char *name;
            char *value;             // optional
        } prep_define;

        struct {
            char *name;
        } prep_undef;

        struct {
            char *condition;
            GclAstList *body;        // list of preprocessor nodes
            GclAstList *elif_chain;
            GclAstNode *else_branch;
        } prep_if;

        struct {
            char *condition;
            GclAstList *body;
        } prep_elif;

        struct {
            char *message;
        } prep_error;

        struct {
            char *message;
        } prep_pragma;

        struct {
            int line;
            char *filename;          // optional
        } prep_line;

        // ── Comments (stripped but preserved) ─────────────
        struct {
            char *text;
        } comment;

        // ── Struct / Enum / Union ─────────────────────────
        struct {
            char       *name;        // optional (anonymous)
            GclAstList *members;     // struct: VarDecl nodes, enum: AST_ENUM_MEMBER
        } struct_decl;               // also used for union

        struct {
            char       *name;
            GclAstNode *value;       // optional explicit value
        } enum_member;

        // ── Typedef ───────────────────────────────────────
        struct {
            GclType *original_type;
            char    *alias;
        } typedef_decl;

        // ── Array Initializer ─────────────────────────────
        struct {
            GclAstList *elements;    // list of AST nodes (expressions/sub-inits)
        } array_init;

        // ── Subscript / Member ────────────────────────────
        struct {
            GclAstNode *base;
            GclAstNode *index;
        } subscript;

        struct {
            GclAstNode *base;
            char       *member;
        } member;

        // ── Compound Statement ────────────────────────────
        struct {
            GclAstList *statements;
        } compound;
    } data;

    // ── List pointers (for list nodes) ────────────────────
    GclAstNode *next;              // sibling in list
};

// ── AST List (linked list wrapper) ────────────────────────
struct GclAstList {
    GclAstNode *head;
    GclAstNode *tail;
    int         count;
};

// ── AST API ───────────────────────────────────────────────

// Allocation
GclAstNode *ast_node_create(GclAstNodeType type, int line, int col);
void        ast_node_free(GclAstNode *node);
void        ast_free_all(GclAstNode *root);

// List operations
GclAstList *ast_list_create(void);
void        ast_list_append(GclAstList *list, GclAstNode *node);
void        ast_list_free(GclAstList *list);

// Convenience constructors
GclAstNode *ast_int_literal(long long val, int line, int col);
GclAstNode *ast_float_literal(double val, int is_long_double, int line, int col);
GclAstNode *ast_char_literal(int code, int line, int col);
GclAstNode *ast_string_literal(const char *str, int line, int col);
GclAstNode *ast_identifier(const char *name, int line, int col);
GclAstNode *ast_bool_literal(int value, int line, int col);

GclAstNode *ast_binary(GclTokenType op, GclAstNode *lhs, GclAstNode *rhs);
GclAstNode *ast_unary(GclTokenType op, GclAstNode *operand);
GclAstNode *ast_ternary(GclAstNode *cond, GclAstNode *t, GclAstNode *f);
GclAstNode *ast_cast(GclType *type, GclAstNode *expr);
GclAstNode *ast_call(GclAstNode *callee, GclAstList *args);
GclAstNode *ast_assign(GclTokenType op, GclAstNode *lhs, GclAstNode *rhs);

GclAstNode *ast_var_decl(GclType *type, const char *name, GclAstNode *init, int line, int col);
GclAstNode *ast_func_def(GclType *ret, const char *name, GclAstList *params,
                         GclAstNode *body, int variadic, int line, int col);
GclAstNode *ast_func_param(GclType *type, const char *name, int line, int col);

GclAstNode *ast_if(GclAstNode *cond, GclAstNode *then_b, GclAstNode *else_b);
GclAstNode *ast_for(GclAstNode *init, GclAstNode *cond, GclAstNode *incr, GclAstNode *body);
GclAstNode *ast_while(GclAstNode *cond, GclAstNode *body);
GclAstNode *ast_do_while(GclAstNode *body, GclAstNode *cond);
GclAstNode *ast_switch(GclAstNode *expr, GclAstNode *body);
GclAstNode *ast_case(GclAstNode *value, GclAstNode *stmt);
GclAstNode *ast_return(GclAstNode *expr);
GclAstNode *ast_break(int line, int col);
GclAstNode *ast_compound(GclAstList *stmts);

GclAstNode *ast_subscript(GclAstNode *base, GclAstNode *index);
GclAstNode *ast_member(GclAstNode *base, const char *member);
GclAstNode *ast_sizeof_type(GclType *type, int line, int col);
GclAstNode *ast_sizeof_expr(GclAstNode *expr);

GclAstNode *ast_array_init(GclAstList *elements, int line, int col);
GclAstNode *ast_struct_decl(const char *name, GclAstList *members, int line, int col);
GclAstNode *ast_enum_decl(const char *name, GclAstList *members, int line, int col);
GclAstNode *ast_enum_member(const char *name, GclAstNode *value, int line, int col);
GclAstNode *ast_typedef_decl(GclType *type, const char *alias, int line, int col);

GclAstNode *ast_prep_include(const char *path, int is_system, int line, int col);
GclAstNode *ast_prep_define(const char *name, const char *value, int line, int col);
GclAstNode *ast_prep_comment(const char *text, int text_len, int line, int col);

// Dump / Debug
void ast_dump(GclAstNode *node, int indent);

#endif // GCL_AST_H
