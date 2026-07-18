#include "semantic.h"
#include "../type/type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ================================================================= */
/*  Symbol Table                                                      */
/* ================================================================= */

#define SYM_HASH_SIZE 127

static SymTable *sym_table_new(void) {
    SymTable *t = (SymTable*)calloc(1, sizeof(SymTable));
    if (!t) return NULL;
    t->capacity = SYM_HASH_SIZE;
    t->buckets = (SymEntry**)calloc(t->capacity, sizeof(SymEntry*));
    t->count = 0;
    return t;
}

static void sym_table_free(SymTable *t) {
    if (!t) return;
    for (int i = 0; i < t->capacity; i++) {
        SymEntry *e = t->buckets[i];
        while (e) {
            SymEntry *next = e->next;
            free(e);
            e = next;
        }
    }
    free(t->buckets);
    free(t);
}

static unsigned int sym_hash(const char *s) {
    unsigned int h = 0;
    while (*s) { h = h * 31 + (unsigned char)*s++; }
    return h;
}

static SymEntry *sym_table_lookup(SymTable *t, const char *name) {
    if (!t || !name) return NULL;
    unsigned int idx = sym_hash(name) % t->capacity;
    SymEntry *e = t->buckets[idx];
    while (e) {
        if (strcmp(e->name, name) == 0) return e;
        e = e->next;
    }
    return NULL;
}

static int sym_table_add(SymTable *t, SymEntry *e) {
    if (!t || !e) return 0;
    unsigned int idx = sym_hash(e->name) % t->capacity;
    e->next = t->buckets[idx];
    t->buckets[idx] = e;
    t->count++;
    return 1;
}

/* ================================================================= */
/*  Scope Stack                                                       */
/* ================================================================= */

void scope_enter(SemanticState *s) {
    ScopeNode *sn = (ScopeNode*)calloc(1, sizeof(ScopeNode));
    if (!sn) return;
    sn->depth = s->scope ? s->scope->depth + 1 : 0;
    sn->table = sym_table_new();
    sn->parent = s->scope;
    s->scope = sn;
}

void scope_exit(SemanticState *s) {
    if (!s->scope) return;
    ScopeNode *old = s->scope;
    s->scope = old->parent;
    sym_table_free(old->table);
    free(old);
}

int scope_add_var(SemanticState *s, const char *name, GclType *type,
                  int line, SymKind kind) {
    if (!s->scope) return 0;
    SymEntry *existing = sym_table_lookup(s->scope->table, name);
    if (existing) {
        sem_error(s, SEM_ERR_REDECLARED_VAR, line, 0,
                  "'%s' already declared in this scope", name);
        return 0;
    }
    SymEntry *e = (SymEntry*)calloc(1, sizeof(SymEntry));
    if (!e) return 0;
    e->kind = kind;
    e->name = name;
    e->type = type;
    e->depth = s->scope->depth;
    e->declared_line = line;
    e->is_initialized = 0;
    e->is_used = 0;
    return sym_table_add(s->scope->table, e);
}

SymEntry *scope_lookup(SemanticState *s, const char *name) {
    ScopeNode *sn = s->scope;
    while (sn) {
        SymEntry *e = sym_table_lookup(sn->table, name);
        if (e) return e;
        sn = sn->parent;
    }
    return NULL;
}

/* ================================================================= */
/*  Semantic State helpers                                            */
/* ================================================================= */

SemanticState *semantic_new(void) {
    SemanticState *s = (SemanticState*)calloc(1, sizeof(SemanticState));
    if (!s) return NULL;
    s->error_cap = 16;
    s->errors = (SemError*)calloc(s->error_cap, sizeof(SemError));
    s->error_count = 0;
    s->warnings = 0;
    s->scope = NULL;
    s->current_func_ret_type = NULL;
    scope_enter(s); /* global scope */
    return s;
}

void semantic_free(SemanticState *s) {
    if (!s) return;
    while (s->scope) scope_exit(s);
    free(s->errors);
    free(s);
}

/* ================================================================= */
/*  Error / Warning reporting                                         */
/* ================================================================= */

void sem_error(SemanticState *s, SemErrorCode code, int line, int col,
               const char *fmt, ...) {
    if (s->error_count >= s->error_cap) {
        s->error_cap *= 2;
        s->errors = (SemError*)realloc(s->errors,
                                        s->error_cap * sizeof(SemError));
    }
    SemError *e = &s->errors[s->error_count++];
    e->code = code;
    e->line = line;
    e->col = col;
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    e->msg = strdup(buf);
    fprintf(stderr, "  Semantic error at %d:%d: %s\n", line, col, buf);
}

/* All warnings are disabled – empty body */
void sem_warning(SemanticState *s, SemErrorCode code, int line, int col,
                 const char *fmt, ...) {
    (void)s;
    (void)code;
    (void)line;
    (void)col;
    (void)fmt;
}

/* ================================================================= */
/*  Type-checking helpers                                             */
/* ================================================================= */

int type_check_assignment(SemanticState *s, GclType *dst, GclType *src,
                           int line, int col) {
    /* void alinamaz */
    if (dst->kind == TYPE_VOID || src->kind == TYPE_VOID) {
        sem_error(s, SEM_ERR_TYPE_MISMATCH, line, col,
                  "cannot use void type in assignment");
        return 0;
    }
    /* pointer = 0 (NULL) */
    if (dst->kind == TYPE_POINTER && src->kind == TYPE_INT &&
        src == type_int()) {
        return 1; /* NULL atamasi */
    }
    /* pointer = pointer (base type esitligi) */
    if (dst->kind == TYPE_POINTER && src->kind == TYPE_POINTER) {
        if (type_equal(dst->data.pointer.base, src->data.pointer.base))
            return 1;
        sem_error(s, SEM_ERR_TYPE_MISMATCH, line, col,
                  "incompatible pointer types: %s vs %s",
                  type_name(dst), type_name(src));
        return 0;
    }
    /* char = string literal (char x[] = "str") */
    if (dst->kind == TYPE_CHAR && src->kind == TYPE_POINTER &&
        src->data.pointer.base && src->data.pointer.base->kind == TYPE_CHAR)
        return 1;
    /* numeric turler arasi otomatik donusum */
    if (type_is_numeric(dst) && type_is_numeric(src))
        return 1;
    /* ayni tipler */
    if (type_equal(dst, src)) return 1;
    sem_error(s, SEM_ERR_TYPE_MISMATCH, line, col,
              "cannot assign %s to %s", type_name(src), type_name(dst));
    return 0;
}

int type_check_comparison(SemanticState *s, GclType *a, GclType *b,
                           int line, int col) {
    if (type_is_arithmetic(a) && type_is_arithmetic(b)) return 1;
    if (a->kind == TYPE_POINTER && b->kind == TYPE_POINTER) return 1;
    if (a->kind == TYPE_POINTER && b->kind == TYPE_INT) return 1;
    if (a->kind == TYPE_INT && b->kind == TYPE_POINTER) return 1;
    sem_error(s, SEM_ERR_TYPE_MISMATCH, line, col,
              "cannot compare %s with %s", type_name(a), type_name(b));
    return 0;
}

/* ================================================================= */
/*  Overflow analizi (integer literal)                                */
/* ================================================================= */

static int check_overflow(GclType *type, int64_t val, int line, int col) {
    if (!type_is_integer(type)) return 0;
    int64_t min = 0, max = 0;
    switch (type->kind) {
    case TYPE_CHAR:  min = -128; max = 127; break;
    case TYPE_SHORT: min = -32768; max = 32767; break;
    case TYPE_INT:   min = -2147483648LL; max = 2147483647LL; break;
    case TYPE_LONG:
    case TYPE_LONG_LONG: return 0; /* 64-bit signed, zaten int64 */;
    default: return 0;
    }
    if (val < min || val > max) {
        fprintf(stderr, "  Warning at %d:%d: integer overflow %ld outside %s range\n",
                line, col, (long)val, type_name(type));
        return 1;
    }
    return 0;
}

/* ================================================================= */
/*  AST Analysis (ana recursive walk)                                 */
/* ================================================================= */

static GclType *analyze_expr(SemanticState *s, AstNode *node);

static void analyze_stmt(SemanticState *s, AstNode *node) {
    if (!node) return;

    switch (node->type) {

    case AST_BLOCK:
        scope_enter(s);
        for (int i = 0; i < node->data.block.count; i++)
            analyze_stmt(s, node->data.block.stmts[i]);
        /* Uyar: kullanilmayan degiskenler – disabled */
        scope_exit(s);
        break;

    case AST_VAR_DECL: {
        GclType *type = ast_to_type(node->data.var.type);
        scope_add_var(s, node->data.var.name, type, node->line, SYM_VARIABLE);
        if (node->data.var.init) {
            GclType *init_type = analyze_expr(s, node->data.var.init);
            if (init_type) {
                type_check_assignment(s, type, init_type, node->line, node->col);
                SymEntry *e = scope_lookup(s, node->data.var.name);
                if (e) e->is_initialized = 1;
            }
        }
        break;
    }

    case AST_RETURN: {
        GclType *ret_type = s->current_func_ret_type;
        if (node->data.ret.val) {
            GclType *expr_type = analyze_expr(s, node->data.ret.val);
            if (ret_type && expr_type)
                type_check_assignment(s, ret_type, expr_type, node->line, node->col);
        } else {
            if (ret_type && ret_type->kind != TYPE_VOID)
                sem_error(s, SEM_ERR_RETURN_TYPE, node->line, node->col,
                          "expected return value of type %s", type_name(ret_type));
        }
        break;
    }

    case AST_IF:
        analyze_expr(s, node->data.if_stmt.cond);
        analyze_stmt(s, node->data.if_stmt.then);
        if (node->data.if_stmt.els)
            analyze_stmt(s, node->data.if_stmt.els);
        break;

    case AST_WHILE:
        analyze_expr(s, node->data.while_stmt.cond);
        analyze_stmt(s, node->data.while_stmt.body);
        break;

    case AST_FOR:
        if (node->data.for_stmt.init)
            analyze_stmt(s, node->data.for_stmt.init);
        if (node->data.for_stmt.cond)
            analyze_expr(s, node->data.for_stmt.cond);
        if (node->data.for_stmt.step)
            analyze_expr(s, node->data.for_stmt.step);
        if (node->data.for_stmt.body)
            analyze_stmt(s, node->data.for_stmt.body);
        break;

    default:
        /* expression statement */
        analyze_expr(s, node);
        break;
    }
}

static GclType *analyze_expr(SemanticState *s, AstNode *node) {
    if (!node) return NULL;

    switch (node->type) {

    case AST_LITERAL_INT: {
        int64_t val = node->data.int_val;
        GclType *t = type_int();
        /* Kucukse char/short'a sigar */
        if (val >= -128 && val <= 127)     t = type_char();
        else if (val >= -32768 && val <= 32767) t = type_short();
        /* overflow uyarisi */
        check_overflow(t, val, node->line, node->col);
        return t;
    }

    case AST_LITERAL_FLOAT:
        return type_double();

    case AST_STRING:
        /* string → const char* */
        return type_pointer(type_char());

    case AST_IDENTIFIER: {
        SymEntry *e = scope_lookup(s, node->data.id);
        if (!e) {
            sem_error(s, SEM_ERR_UNDECLARED_VAR, node->line, node->col,
                      "undeclared variable '%s'", node->data.id);
            return type_int(); /* kurtarma */
        }
        e->is_used = 1;
        /* Uninitialized variable uyarisi – disabled */
        return e->type;
    }

    case AST_FUNC_CALL: {
        /* Simdilik built-in fonksiyonlari kabul et */
        const char *fname = node->data.call.name;
        /* Tum argumentleri analiz et */
        for (int i = 0; i < node->data.call.acount; i++)
            analyze_expr(s, node->data.call.args[i]);
        /* printf, scanf gibi variadic built-in'ler int doner */
        if (strcmp(fname, "printf") == 0 ||
            strcmp(fname, "scanf") == 0 ||
            strcmp(fname, "puts") == 0 ||
            strcmp(fname, "gets") == 0 ||
            strcmp(fname, "malloc") == 0 ||
            strcmp(fname, "free") == 0)
            return type_int();
        return type_int(); /* varsayilan */
    }

    case AST_BINARY_OP: {
        GclType *left = analyze_expr(s, node->data.bin.left);
        GclType *right = analyze_expr(s, node->data.bin.right);
        int op = node->data.bin.op;
        if (!left || !right) return type_int();
        /* Division by zero check */
        if ((op == '/' || op == '%') &&
            node->data.bin.right->type == AST_LITERAL_INT &&
            node->data.bin.right->data.int_val == 0) {
            sem_error(s, SEM_ERR_DIV_BY_ZERO, node->line, node->col,
                      "division by zero");
        }
        /* Karsilastirma operatorleri */
        if (op == TOKEN_EQEQ || op == TOKEN_BANGEQ ||
            op == TOKEN_LT || op == TOKEN_GT ||
            op == TOKEN_LE || op == TOKEN_GE) {
            type_check_comparison(s, left, right, node->line, node->col);
            return type_int(); /* karsilastirma sonucu int */
        }
        /* Mantiksal operatorler */
        if (op == TOKEN_ANDAND || op == TOKEN_OROR) {
            return type_int();
        }
        /* Aritmetik operatorler */
        if (type_is_arithmetic(left) && type_is_arithmetic(right)) {
            /* Sayisal promosyon: ikisi de sayisal ise genis olanı don */
            if (type_is_float(left) || type_is_float(right))
                return type_double();
            return type_int();
        }
        /* Pointer + int (pointer arithmetic) */
        if (left->kind == TYPE_POINTER && type_is_integer(right))
            return left;
        if (type_is_integer(left) && right->kind == TYPE_POINTER)
            return right;
        sem_error(s, SEM_ERR_INVALID_OPERATION, node->line, node->col,
                  "invalid operands to binary expression: %s and %s",
                  type_name(left), type_name(right));
        return type_int();
    }

    case AST_UNARY_OP: {
        GclType *operand = analyze_expr(s, node->data.un.operand);
        int op = node->data.un.op;
        if (op == TOKEN_AMPERSAND) {
            /* & → pointer al */
            return type_pointer(operand);
        }
        if (op == TOKEN_STAR) {
            /* * → dereference */
            if (operand && operand->kind == TYPE_POINTER)
                return operand->data.pointer.base;
            sem_error(s, SEM_ERR_TYPE_MISMATCH, node->line, node->col,
                      "cannot dereference non-pointer type %s",
                      type_name(operand));
            return type_int();
        }
        return operand;
    }

    default:
        return type_int();
    }
}

/* ================================================================= */
/*  Main API: semantic_analyze                                        */
/* ================================================================= */

SemanticResult semantic_analyze(AstNode *ast) {
    SemanticState *s = semantic_new();
    if (!ast) {
        SemanticResult r = {1, 0};
        return r;
    }

    /* Global scope'ta built-in printf bildirimi */
    scope_add_var(s, "printf", type_int(), 0, SYM_FUNCTION);
    scope_add_var(s, "scanf", type_int(), 0, SYM_FUNCTION);
    scope_add_var(s, "puts", type_int(), 0, SYM_FUNCTION);
    scope_add_var(s, "malloc", type_pointer(type_void()), 0, SYM_FUNCTION);
    scope_add_var(s, "free", type_void(), 0, SYM_FUNCTION);

    /* Global true/false degiskenleri (bool tipinde) */
    scope_add_var(s, "true", type_bool(), 0, SYM_VARIABLE);
    {
        SymEntry *e = scope_lookup(s, "true");
        if (e) e->is_initialized = 1;
    }
    scope_add_var(s, "false", type_bool(), 0, SYM_VARIABLE);
    {
        SymEntry *e = scope_lookup(s, "false");
        if (e) e->is_initialized = 1;
    }

    /* Program = list of function defs */
    if (ast->type == AST_PROGRAM) {
        for (int i = 0; i < ast->data.program.count; i++) {
            AstNode *func = ast->data.program.stmts[i];
            if (func->type != AST_FUNCTION_DEF) continue;
            /* Fonksiyonu global scope'a ekle */
            GclType *ret_type = ast_to_type(func->data.func.type);
            scope_add_var(s, func->data.func.name, ret_type,
                          func->line, SYM_FUNCTION);
            /* Function scope */
            scope_enter(s);
            s->current_func_ret_type = ret_type;
            /* Parametreleri ekle */
            for (int j = 0; j < func->data.func.pcount; j++) {
                AstNode *param = func->data.func.params[j];
                GclType *ptype = ast_to_type(param->data.var.type);
                scope_add_var(s, param->data.var.name, ptype,
                              param->line, SYM_PARAMETER);
                SymEntry *e = scope_lookup(s, param->data.var.name);
                if (e) e->is_initialized = 1; /* param her zaman init */
            }
            /* Body */
            analyze_stmt(s, func->data.func.body);
            s->current_func_ret_type = NULL;
            scope_exit(s);
        }
    }

    SemanticResult r;
    r.errors = s->error_count;
    r.warnings = s->warnings;
    semantic_free(s);
    return r;
}