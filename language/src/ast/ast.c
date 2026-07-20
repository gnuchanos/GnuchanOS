#include "ast.h"
#include "tokens.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// GCL AST Implementation
// ============================================================

// ── Node allocation ───────────────────────────────────────
GclAstNode *ast_node_create(GclAstNodeType type, int line, int col) {
    GclAstNode *node = (GclAstNode *)calloc(1, sizeof(GclAstNode));
    if (!node) return NULL;
    node->type = type;
    node->loc.line = line;
    node->loc.col = col;
    node->value_type = NULL;
    node->lexeme = NULL;
    node->length = 0;
    node->next = NULL;
    return node;
}

void ast_node_free(GclAstNode *node) {
    if (!node) return;
    switch (node->type) {
    case AST_STRING_LITERAL:
        free(node->data.string_lit.str);
        break;
    case AST_IDENTIFIER:
        free(node->data.ident.name);
        break;
    case AST_VAR_DECL:
        type_free(node->data.var_decl.var_type);
        free(node->data.var_decl.name);
        ast_node_free(node->data.var_decl.init);
        break;
    case AST_FUNC_DEF:
    case AST_FUNC_DECL:
        type_free(node->data.func.return_type);
        free(node->data.func.name);
        ast_list_free(node->data.func.params);
        ast_node_free(node->data.func.body);
        break;
    case AST_BINARY:
        ast_node_free(node->data.binary.lhs);
        ast_node_free(node->data.binary.rhs);
        break;
    case AST_UNARY:
        ast_node_free(node->data.unary.operand);
        break;
    case AST_TERNARY:
        ast_node_free(node->data.ternary.cond);
        ast_node_free(node->data.ternary.true_expr);
        ast_node_free(node->data.ternary.false_expr);
        break;
    case AST_CALL:
        ast_node_free(node->data.call.callee);
        ast_list_free(node->data.call.args);
        break;
    case AST_ASSIGN:
        ast_node_free(node->data.assign.lhs);
        ast_node_free(node->data.assign.rhs);
        break;
    case AST_CAST:
        type_free(node->data.cast.target_type);
        ast_node_free(node->data.cast.expr);
        break;
    case AST_IF:
        ast_node_free(node->data.if_stmt.cond);
        ast_node_free(node->data.if_stmt.then_branch);
        ast_node_free(node->data.if_stmt.else_branch);
        break;
    case AST_FOR:
        ast_node_free(node->data.for_stmt.init);
        ast_node_free(node->data.for_stmt.cond);
        ast_node_free(node->data.for_stmt.incr);
        ast_node_free(node->data.for_stmt.body);
        break;
    case AST_WHILE:
    case AST_DO_WHILE:
        ast_node_free(node->data.while_stmt.cond);
        ast_node_free(node->data.while_stmt.body);
        break;
    case AST_SWITCH:
        ast_node_free(node->data.switch_stmt.expr);
        ast_node_free(node->data.switch_stmt.body);
        break;
    case AST_CASE:
        ast_node_free(node->data.case_stmt.value);
        ast_node_free(node->data.case_stmt.stmt);
        break;
    case AST_RETURN:
        ast_node_free(node->data.return_stmt.expr);
        break;
    case AST_COMPOUND:
        ast_list_free(node->data.compound.statements);
        break;
    case AST_SUBSCRIPT:
        ast_node_free(node->data.subscript.base);
        ast_node_free(node->data.subscript.index);
        break;
    case AST_MEMBER:
    case AST_MEMBER_PTR:
        ast_node_free(node->data.member.base);
        free(node->data.member.member);
        break;
    case AST_ARRAY_INIT:
        ast_list_free(node->data.array_init.elements);
        break;
    case AST_STRUCT_DECL:
    case AST_ENUM_DECL:
    case AST_UNION_DECL:
        free(node->data.struct_decl.name);
        ast_list_free(node->data.struct_decl.members);
        break;
    case AST_ENUM_MEMBER:
        free(node->data.enum_member.name);
        ast_node_free(node->data.enum_member.value);
        break;
    case AST_TYPEDEF_DECL:
        type_free(node->data.typedef_decl.original_type);
        free(node->data.typedef_decl.alias);
        break;
    case AST_PREP_INCLUDE:
    case AST_PREP_EXTERN:
    case AST_PREP_LIB:
        free(node->data.prep_include.path);
        break;
    case AST_PREP_DEFINE:
        free(node->data.prep_define.name);
        free(node->data.prep_define.value);
        break;
    case AST_PREP_ERROR:
    case AST_PREP_PRAGMA:
        free(node->data.prep_error.message);
        break;
    case AST_GCL_COMMENT:
    case AST_GCL_COMMENT_BLOCK:
    case AST_GCL_COMMENT_CPP:
        free(node->data.comment.text);
        break;
    default:
        break;
    }
    if (node->value_type) type_free(node->value_type);
    free(node);
}

void ast_free_all(GclAstNode *root) {
    GclAstNode *curr = root;
    while (curr) {
        GclAstNode *next = curr->next;
        ast_node_free(curr);
        curr = next;
    }
}

// ── List operations ───────────────────────────────────────
GclAstList *ast_list_create(void) {
    GclAstList *list = (GclAstList *)calloc(1, sizeof(GclAstList));
    return list;
}

void ast_list_append(GclAstList *list, GclAstNode *node) {
    if (!list || !node) return;
    // Find the tail of the incoming chain and count nodes
    GclAstNode *chain_tail = node;
    int chain_len = 1;
    while (chain_tail->next) {
        chain_tail = chain_tail->next;
        chain_len++;
    }
    if (!list->head) {
        list->head = node;
        list->tail = chain_tail;
    } else {
        list->tail->next = node;
        list->tail = chain_tail;
    }
    list->count += chain_len;
}

void ast_list_free(GclAstList *list) {
    if (!list) return;
    GclAstNode *curr = list->head;
    while (curr) {
        GclAstNode *next = curr->next;
        ast_node_free(curr);
        curr = next;
    }
    free(list);
}

// ── Convenience constructors ──────────────────────────────

static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len + 1);
    return d;
}

GclAstNode *ast_int_literal(long long val, int line, int col) {
    GclAstNode *node = ast_node_create(AST_INT_LITERAL, line, col);
    node->data.int_lit.int_val = val;
    return node;
}

GclAstNode *ast_float_literal(double val, int is_long_double, int line, int col) {
    GclAstNode *node = ast_node_create(AST_FLOAT_LITERAL, line, col);
    node->data.float_lit.float_val = val;
    node->data.float_lit.is_long_double = is_long_double;
    return node;
}

GclAstNode *ast_char_literal(int code, int line, int col) {
    GclAstNode *node = ast_node_create(AST_CHAR_LITERAL, line, col);
    node->data.char_lit.code = code;
    node->data.char_lit.escape = 0;
    return node;
}

GclAstNode *ast_string_literal(const char *str, int line, int col) {
    GclAstNode *node = ast_node_create(AST_STRING_LITERAL, line, col);
    node->data.string_lit.str = strdup_safe(str);
    return node;
}

GclAstNode *ast_identifier(const char *name, int line, int col) {
    GclAstNode *node = ast_node_create(AST_IDENTIFIER, line, col);
    node->data.ident.name = strdup_safe(name);
    return node;
}

GclAstNode *ast_bool_literal(int value, int line, int col) {
    GclAstNode *node = ast_node_create(AST_BOOL_LITERAL, line, col);
    node->data.int_lit.int_val = value ? 1 : 0;
    return node;
}

GclAstNode *ast_binary(GclTokenType op, GclAstNode *lhs, GclAstNode *rhs) {
    GclAstNode *node = ast_node_create(AST_BINARY, lhs ? lhs->loc.line : 0,
                                       lhs ? lhs->loc.col : 0);
    node->data.binary.op = op;
    node->data.binary.lhs = lhs;
    node->data.binary.rhs = rhs;
    return node;
}

GclAstNode *ast_unary(GclTokenType op, GclAstNode *operand) {
    GclAstNode *node = ast_node_create(AST_UNARY,
                                       operand ? operand->loc.line : 0,
                                       operand ? operand->loc.col : 0);
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

GclAstNode *ast_ternary(GclAstNode *cond, GclAstNode *t, GclAstNode *f) {
    GclAstNode *node = ast_node_create(AST_TERNARY,
                                       cond ? cond->loc.line : 0,
                                       cond ? cond->loc.col : 0);
    node->data.ternary.cond = cond;
    node->data.ternary.true_expr = t;
    node->data.ternary.false_expr = f;
    return node;
}

GclAstNode *ast_cast(GclType *type, GclAstNode *expr) {
    GclAstNode *node = ast_node_create(AST_CAST,
                                       expr ? expr->loc.line : 0,
                                       expr ? expr->loc.col : 0);
    node->data.cast.target_type = type;
    node->data.cast.expr = expr;
    return node;
}

GclAstNode *ast_call(GclAstNode *callee, GclAstList *args) {
    GclAstNode *node = ast_node_create(AST_CALL,
                                       callee ? callee->loc.line : 0,
                                       callee ? callee->loc.col : 0);
    node->data.call.callee = callee;
    node->data.call.args = args;
    return node;
}

GclAstNode *ast_assign(GclTokenType op, GclAstNode *lhs, GclAstNode *rhs) {
    GclAstNode *node = ast_node_create(AST_ASSIGN,
                                       lhs ? lhs->loc.line : 0,
                                       lhs ? lhs->loc.col : 0);
    node->data.assign.op = op;
    node->data.assign.lhs = lhs;
    node->data.assign.rhs = rhs;
    return node;
}

GclAstNode *ast_var_decl(GclType *type, const char *name, GclAstNode *init,
                          int line, int col) {
    GclAstNode *node = ast_node_create(AST_VAR_DECL, line, col);
    node->data.var_decl.var_type = type;
    node->data.var_decl.name = strdup_safe(name);
    node->data.var_decl.init = init;
    node->data.var_decl.is_const = 0;
    node->data.var_decl.is_static = 0;
    return node;
}

GclAstNode *ast_func_def(GclType *ret, const char *name, GclAstList *params,
                          GclAstNode *body, int variadic, int line, int col) {
    GclAstNode *node = ast_node_create(AST_FUNC_DEF, line, col);
    node->data.func.return_type = ret;
    node->data.func.name = strdup_safe(name);
    node->data.func.params = params;
    node->data.func.body = body;
    node->data.func.is_variadic = variadic;
    return node;
}

GclAstNode *ast_func_param(GclType *type, const char *name, int line, int col) {
    GclAstNode *node = ast_node_create(AST_FUNC_PARAM, line, col);
    node->data.param.param_type = type;
    node->data.param.name = strdup_safe(name);
    return node;
}

GclAstNode *ast_if(GclAstNode *cond, GclAstNode *then_b, GclAstNode *else_b) {
    GclAstNode *node = ast_node_create(AST_IF,
                                       cond ? cond->loc.line : 0,
                                       cond ? cond->loc.col : 0);
    node->data.if_stmt.cond = cond;
    node->data.if_stmt.then_branch = then_b;
    node->data.if_stmt.else_branch = else_b;
    return node;
}

GclAstNode *ast_for(GclAstNode *init, GclAstNode *cond, GclAstNode *incr,
                     GclAstNode *body) {
    GclAstNode *node = ast_node_create(AST_FOR,
                                       init ? init->loc.line : 0,
                                       init ? init->loc.col : 0);
    node->data.for_stmt.init = init;
    node->data.for_stmt.cond = cond;
    node->data.for_stmt.incr = incr;
    node->data.for_stmt.body = body;
    return node;
}

GclAstNode *ast_while(GclAstNode *cond, GclAstNode *body) {
    GclAstNode *node = ast_node_create(AST_WHILE,
                                       cond ? cond->loc.line : 0,
                                       cond ? cond->loc.col : 0);
    node->data.while_stmt.cond = cond;
    node->data.while_stmt.body = body;
    return node;
}

GclAstNode *ast_do_while(GclAstNode *body, GclAstNode *cond) {
    GclAstNode *node = ast_node_create(AST_DO_WHILE,
                                       body ? body->loc.line : 0,
                                       body ? body->loc.col : 0);
    node->data.do_while.body = body;
    node->data.do_while.cond = cond;
    return node;
}

GclAstNode *ast_switch(GclAstNode *expr, GclAstNode *body) {
    GclAstNode *node = ast_node_create(AST_SWITCH,
                                       expr ? expr->loc.line : 0,
                                       expr ? expr->loc.col : 0);
    node->data.switch_stmt.expr = expr;
    node->data.switch_stmt.body = body;
    return node;
}

GclAstNode *ast_case(GclAstNode *value, GclAstNode *stmt) {
    GclAstNode *node = ast_node_create(AST_CASE,
                                       value ? value->loc.line : 0,
                                       value ? value->loc.col : 0);
    node->data.case_stmt.value = value;
    node->data.case_stmt.stmt = stmt;
    return node;
}

GclAstNode *ast_return(GclAstNode *expr) {
    GclAstNode *node = ast_node_create(AST_RETURN,
                                       expr ? expr->loc.line : 0,
                                       expr ? expr->loc.col : 0);
    node->data.return_stmt.expr = expr;
    return node;
}

GclAstNode *ast_break(int line, int col) {
    return ast_node_create(AST_BREAK, line, col);
}

GclAstNode *ast_compound(GclAstList *stmts) {
    GclAstNode *node = ast_node_create(AST_COMPOUND, 0, 0);
    node->data.compound.statements = stmts;
    return node;
}

GclAstNode *ast_subscript(GclAstNode *base, GclAstNode *index) {
    GclAstNode *node = ast_node_create(AST_SUBSCRIPT,
                                       base ? base->loc.line : 0,
                                       base ? base->loc.col : 0);
    node->data.subscript.base = base;
    node->data.subscript.index = index;
    return node;
}

GclAstNode *ast_member(GclAstNode *base, const char *member) {
    GclAstNode *node = ast_node_create(AST_MEMBER,
                                       base ? base->loc.line : 0,
                                       base ? base->loc.col : 0);
    node->data.member.base = base;
    node->data.member.member = strdup_safe(member);
    return node;
}

GclAstNode *ast_sizeof_type(GclType *type, int line, int col) {
    GclAstNode *node = ast_node_create(AST_SIZEOF, line, col);
    node->data.sizeof_expr.target_type = type;
    node->data.sizeof_expr.expr = NULL;
    return node;
}

GclAstNode *ast_sizeof_expr(GclAstNode *expr) {
    GclAstNode *node = ast_node_create(AST_SIZEOF,
                                       expr ? expr->loc.line : 0,
                                       expr ? expr->loc.col : 0);
    node->data.sizeof_expr.target_type = NULL;
    node->data.sizeof_expr.expr = expr;
    return node;
}

GclAstNode *ast_array_init(GclAstList *elements, int line, int col) {
    GclAstNode *node = ast_node_create(AST_ARRAY_INIT, line, col);
    node->data.array_init.elements = elements;
    return node;
}

GclAstNode *ast_struct_decl(const char *name, GclAstList *members, int line, int col) {
    GclAstNode *node = ast_node_create(AST_STRUCT_DECL, line, col);
    node->data.struct_decl.name = strdup_safe(name);
    node->data.struct_decl.members = members;
    return node;
}

GclAstNode *ast_enum_decl(const char *name, GclAstList *members, int line, int col) {
    GclAstNode *node = ast_node_create(AST_ENUM_DECL, line, col);
    node->data.struct_decl.name = strdup_safe(name);
    node->data.struct_decl.members = members;
    return node;
}

GclAstNode *ast_enum_member(const char *name, GclAstNode *value, int line, int col) {
    GclAstNode *node = ast_node_create(AST_ENUM_MEMBER, line, col);
    node->data.enum_member.name = strdup_safe(name);
    node->data.enum_member.value = value;
    return node;
}

GclAstNode *ast_typedef_decl(GclType *type, const char *alias, int line, int col) {
    GclAstNode *node = ast_node_create(AST_TYPEDEF_DECL, line, col);
    node->data.typedef_decl.original_type = type;
    node->data.typedef_decl.alias = strdup_safe(alias);
    return node;
}

GclAstNode *ast_prep_include(const char *path, int is_system, int line, int col) {
    GclAstNode *node = ast_node_create(AST_PREP_INCLUDE, line, col);
    node->data.prep_include.path = strdup_safe(path);
    node->data.prep_include.is_system = is_system;
    return node;
}

GclAstNode *ast_prep_define(const char *name, const char *value, int line, int col) {
    GclAstNode *node = ast_node_create(AST_PREP_DEFINE, line, col);
    node->data.prep_define.name = strdup_safe(name);
    node->data.prep_define.value = strdup_safe(value);
    return node;
}

GclAstNode *ast_prep_comment(const char *text, int text_len, int line, int col) {
    GclAstNode *node = ast_node_create(AST_GCL_COMMENT, line, col);
    if (text && text_len > 0) {
        char *d = (char *)malloc(text_len + 1);
        if (d) {
            memcpy(d, text, text_len);
            d[text_len] = '\0';
            node->data.comment.text = d;
        }
    }
    return node;
}

// ── Dump / Debug ──────────────────────────────────────────

static void indent_print(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static const char *ast_node_type_name(GclAstNodeType type) {
    static const char *names[] = {
        [AST_PROGRAM]          = "Program",
        [AST_PREP_INCLUDE]     = "Include",
        [AST_PREP_EXTERN]      = "Extern",
        [AST_PREP_DEFINE]      = "Define",
        [AST_VAR_DECL]         = "VarDecl",
        [AST_FUNC_DECL]        = "FuncDecl",
        [AST_FUNC_DEF]         = "FuncDef",
        [AST_STRUCT_DECL]      = "StructDecl",
        [AST_ENUM_DECL]        = "EnumDecl",
        [AST_COMPOUND]         = "Compound",
        [AST_IF]               = "If",
        [AST_FOR]              = "For",
        [AST_WHILE]            = "While",
        [AST_DO_WHILE]         = "DoWhile",
        [AST_SWITCH]           = "Switch",
        [AST_CASE]             = "Case",
        [AST_RETURN]           = "Return",
        [AST_BREAK]            = "Break",
        [AST_BINARY]           = "Binary",
        [AST_UNARY]            = "Unary",
        [AST_TERNARY]          = "Ternary",
        [AST_CAST]             = "Cast",
        [AST_SIZEOF]           = "Sizeof",
        [AST_CALL]             = "Call",
        [AST_SUBSCRIPT]        = "Subscript",
        [AST_MEMBER]           = "Member",
        [AST_ASSIGN]           = "Assign",
        [AST_INT_LITERAL]      = "IntLit",
        [AST_FLOAT_LITERAL]    = "FloatLit",
        [AST_CHAR_LITERAL]     = "CharLit",
        [AST_STRING_LITERAL]   = "StrLit",
        [AST_IDENTIFIER]       = "Ident",
        [AST_BOOL_LITERAL]     = "BoolLit",
        [AST_ARRAY_INIT]       = "ArrayInit",
    };
    if (type >= 0 && type < AST_COUNT && names[type]) return names[type];
    return "???";
}

void ast_dump(GclAstNode *node, int indent) {
    if (!node) {
        indent_print(indent);
        printf("(null)\n");
        return;
    }
    indent_print(indent);
    printf("%s", ast_node_type_name(node->type));

    switch (node->type) {
    case AST_IDENTIFIER:
        printf(" '%s'", node->data.ident.name);
        break;
    case AST_INT_LITERAL:
        printf(" %lld", node->data.int_lit.int_val);
        break;
    case AST_FLOAT_LITERAL:
        printf(" %g", node->data.float_lit.float_val);
        break;
    case AST_CHAR_LITERAL:
        printf(" '%c' (0x%x)", node->data.char_lit.code, node->data.char_lit.code);
        break;
    case AST_STRING_LITERAL:
        printf(" \"%s\"", node->data.string_lit.str);
        break;
    case AST_BOOL_LITERAL:
        printf(" %s", node->data.int_lit.int_val ? "true" : "false");
        break;
    case AST_VAR_DECL:
        printf(" %s", node->data.var_decl.name ? node->data.var_decl.name : "?");
        break;
    case AST_FUNC_DEF:
    case AST_FUNC_DECL:
        printf(" %s", node->data.func.name ? node->data.func.name : "?");
        break;
    case AST_PREP_INCLUDE:
        printf(" %s", node->data.prep_include.path ? node->data.prep_include.path : "");
        break;
    case AST_BINARY:
        printf(" '%s'", token_type_name(node->data.binary.op));
        break;
    default:
        break;
    }
    printf(" [%d:%d]\n", node->loc.line, node->loc.col);

    // Recurse
    switch (node->type) {
    case AST_PROGRAM:
        for (GclAstNode *c = node->next; c; c = c->next) ast_dump(c, indent + 1);
        break;
    case AST_VAR_DECL:
        if (node->data.var_decl.init) ast_dump(node->data.var_decl.init, indent + 1);
        break;
    case AST_FUNC_DEF:
        if (node->data.func.params) {
            for (GclAstNode *p = node->data.func.params->head; p; p = p->next)
                ast_dump(p, indent + 1);
        }
        if (node->data.func.body) ast_dump(node->data.func.body, indent + 1);
        break;
    case AST_BINARY:
        ast_dump(node->data.binary.lhs, indent + 1);
        ast_dump(node->data.binary.rhs, indent + 1);
        break;
    case AST_UNARY:
        ast_dump(node->data.unary.operand, indent + 1);
        break;
    case AST_CALL:
        ast_dump(node->data.call.callee, indent + 1);
        if (node->data.call.args) {
            for (GclAstNode *a = node->data.call.args->head; a; a = a->next)
                ast_dump(a, indent + 1);
        }
        break;
    case AST_IF:
        ast_dump(node->data.if_stmt.cond, indent + 1);
        ast_dump(node->data.if_stmt.then_branch, indent + 1);
        if (node->data.if_stmt.else_branch) ast_dump(node->data.if_stmt.else_branch, indent + 1);
        break;
    case AST_FOR:
        if (node->data.for_stmt.init) ast_dump(node->data.for_stmt.init, indent + 1);
        if (node->data.for_stmt.cond) ast_dump(node->data.for_stmt.cond, indent + 1);
        if (node->data.for_stmt.incr) ast_dump(node->data.for_stmt.incr, indent + 1);
        ast_dump(node->data.for_stmt.body, indent + 1);
        break;
    case AST_WHILE:
        ast_dump(node->data.while_stmt.cond, indent + 1);
        ast_dump(node->data.while_stmt.body, indent + 1);
        break;
    case AST_COMPOUND:
        if (node->data.compound.statements) {
            for (GclAstNode *s = node->data.compound.statements->head; s; s = s->next)
                ast_dump(s, indent + 1);
        }
        break;
    case AST_RETURN:
        if (node->data.return_stmt.expr) ast_dump(node->data.return_stmt.expr, indent + 1);
        break;
    case AST_ARRAY_INIT:
        if (node->data.array_init.elements) {
            for (GclAstNode *e = node->data.array_init.elements->head; e; e = e->next)
                ast_dump(e, indent + 1);
        }
        break;
    default:
        break;
    }
}
