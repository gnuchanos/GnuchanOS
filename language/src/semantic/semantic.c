#include "ast.h"
#include "errors.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ============================================================
// GCL Semantic Analysis — Symbol Table + Type Checking
// ============================================================

// ── Symbol kinds ──────────────────────────────────────────
typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_PARAMETER,
    SYM_TYPE,
} SymKind;

// ── Symbol entry ──────────────────────────────────────────
typedef struct Symbol {
    char       *name;
    SymKind     kind;
    GclType    *type;
    int         line;
    int         col;
    int         is_defined;      // 0 = declared, 1 = body present
    struct Symbol *next;         // hash chain
} Symbol;

// ── Scope ─────────────────────────────────────────────────
typedef struct Scope {
    Symbol     **table;          // hash table
    int          size;           // bucket count
    int          depth;          // 0 = global, 1 = function, 2+ = blocks
    struct Scope *parent;
} Scope;

#define HASH_SIZE 64

static Scope *current_scope = NULL;
static int semantic_errors = 0;
static int have_main_function = 0;

// ── Hash function ─────────────────────────────────────────
static unsigned hash_name(const char *name) {
    unsigned h = 5381;
    while (*name) h = ((h << 5) + h) + (unsigned char)*name++;
    return h % HASH_SIZE;
}

// ── Scope management ──────────────────────────────────────
static Scope *scope_create(Scope *parent, int depth) {
    Scope *s = (Scope *)calloc(1, sizeof(Scope));
    s->table = (Symbol **)calloc(HASH_SIZE, sizeof(Symbol *));
    s->size = HASH_SIZE;
    s->parent = parent;
    s->depth = depth;
    return s;
}

static void scope_free(Scope *s) {
    if (!s) return;
    for (int i = 0; i < s->size; i++) {
        Symbol *sym = s->table[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            // Don't free type — it's owned by AST nodes
            free(sym);
            sym = next;
        }
    }
    free(s->table);
    free(s);
}

static void scope_enter(Scope *s) {
    current_scope = s;
}

static void scope_exit(Scope *s) {
    current_scope = s->parent;
    scope_free(s);
}

// ── Symbol table operations ───────────────────────────────
static Symbol *scope_lookup(Scope *s, const char *name) {
    if (!s) return NULL;
    unsigned h = hash_name(name);
    Symbol *sym = s->table[h];
    while (sym) {
        if (strcmp(sym->name, name) == 0) return sym;
        sym = sym->next;
    }
    return NULL;
}

static Symbol *scope_lookup_chain(Scope *s, const char *name) {
    while (s) {
        Symbol *sym = scope_lookup(s, name);
        if (sym) return sym;
        s = s->parent;
    }
    return NULL;
}

static Symbol *scope_insert(Scope *s, const char *name, SymKind kind,
                             GclType *type, int line, int col) {
    // Check for redeclaration in current scope
    Symbol *existing = scope_lookup(s, name);
    if (existing) {
        GclSourceLoc loc = {"", line, col};
        char buf[256];
        snprintf(buf, sizeof(buf), "redeclaration of '%s'", name);
        error_syntax(E012, loc, buf, NULL, NULL);
        semantic_errors++;
        return existing;
    }

    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    sym->name = strdup(name);
    sym->kind = kind;
    sym->type = type;
    sym->line = line;
    sym->col = col;
    sym->is_defined = (kind == SYM_FUNCTION ? 1 : 0);

    unsigned h = hash_name(name);
    sym->next = s->table[h];
    s->table[h] = sym;
    return sym;
}

// ── Helper: GclSourceLoc from AST node ────────────────────
static GclSourceLoc node_loc(GclAstNode *node) {
    GclSourceLoc loc = {"", node ? node->loc.line : 0, node ? node->loc.col : 0};
    return loc;
}

// ── Type inference for literals ───────────────────────────
static GclType *type_of_literal(GclAstNode *node) {
    switch (node->type) {
    case AST_INT_LITERAL:   return type_int();
    case AST_FLOAT_LITERAL: return type_double();
    case AST_CHAR_LITERAL:  return type_char();
    case AST_STRING_LITERAL:return type_char_ptr();
    case AST_BOOL_LITERAL:  return type_bool();
    default:                return type_int();
    }
}

// ── Result type of binary operation ───────────────────────
static GclType *type_of_binary(GclTokenType op, GclType *lt, GclType *rt) {
    (void)op;
    if (!lt || !rt) return type_int();
    // For arithmetic/bitwise/relational: promote to wider type
    if (type_is_float(lt) || type_is_float(rt)) return type_double();
    // For comparison/logical: always int (bool)
    switch (op) {
    case TOK_GT: case TOK_LT: case TOK_GE: case TOK_LE:
    case TOK_EQ: case TOK_NE: case TOK_AND: case TOK_OR:
        return type_int(); // C returns int for comparisons
    default: break;
    }
    // For arithmetic: the wider of the two
    if (type_size(lt) > type_size(rt)) return lt;
    return rt;
}

// ── Forward declarations ──────────────────────────────────
static GclType *analyze_expr(GclAstNode *node);
static void analyze_stmt(GclAstNode *node, GclType *expected_return);
static void analyze_decl(GclAstNode *node);

// ── Expression analysis (returns inferred type) ───────────
static GclType *analyze_expr(GclAstNode *node) {
    if (!node) return type_int();

    switch (node->type) {
    // ── Literals ──────────────────────────────────────────
    case AST_INT_LITERAL:
    case AST_FLOAT_LITERAL:
    case AST_CHAR_LITERAL:
    case AST_STRING_LITERAL:
    case AST_BOOL_LITERAL:
        node->value_type = type_of_literal(node);
        return node->value_type;

    // ── Identifier ────────────────────────────────────────
    case AST_IDENTIFIER: {
        Symbol *sym = scope_lookup_chain(current_scope, node->data.ident.name);
        if (!sym) {
            GclSourceLoc loc = node_loc(node);
            char buf[256];
            snprintf(buf, sizeof(buf), "undeclared variable '%s'", node->data.ident.name);
            error_syntax(E011, loc, buf, NULL, NULL);
            semantic_errors++;
            node->value_type = type_int();
            return node->value_type;
        }
        node->value_type = sym->type;
        return node->value_type;
    }

    // ── Binary ────────────────────────────────────────────
    case AST_BINARY: {
        GclType *lt = analyze_expr(node->data.binary.lhs);
        GclType *rt = analyze_expr(node->data.binary.rhs);
        node->value_type = type_of_binary(node->data.binary.op, lt, rt);
        return node->value_type;
    }

    // ── Unary ─────────────────────────────────────────────
    case AST_UNARY: {
        GclType *ot = analyze_expr(node->data.unary.operand);
        GclTokenType op = node->data.unary.op;
        // Address-of: &x → pointer to x's type
        if (op == TOK_AMPERSAND) {
            node->value_type = type_pointer(ot);
            return node->value_type;
        }
        // Dereference: *ptr → base type of pointer
        if (op == TOK_STAR) {
            if (type_is_pointer(ot)) {
                node->value_type = ot->base_type;
            } else {
                node->value_type = type_int();
                error_syntax(E013, node_loc(node),
                    "dereference of non-pointer type", NULL, NULL);
                semantic_errors++;
            }
            return node->value_type;
        }
        // -, ~, !, ++, -- → same type as operand
        node->value_type = ot;
        return node->value_type;
    }

    // ── Ternary ───────────────────────────────────────────
    case AST_TERNARY: {
        analyze_expr(node->data.ternary.cond);
        GclType *tt = analyze_expr(node->data.ternary.true_expr);
        GclType *ft = analyze_expr(node->data.ternary.false_expr);
        if (type_is_arithmetic(tt) && type_is_arithmetic(ft)) {
            if (type_size(tt) > type_size(ft)) node->value_type = tt;
            else node->value_type = ft;
        } else {
            node->value_type = tt;
        }
        return node->value_type;
    }

    // ── Cast ──────────────────────────────────────────────
    case AST_CAST: {
        analyze_expr(node->data.cast.expr);
        node->value_type = node->data.cast.target_type;
        return node->value_type;
    }

    // ── Call ──────────────────────────────────────────────
    case AST_CALL: {
        analyze_expr(node->data.call.callee);
        if (node->data.call.args) {
            for (GclAstNode *a = node->data.call.args->head; a; a = a->next)
                analyze_expr(a);
        }
        // Check if callee is a known function
        if (node->data.call.callee->type == AST_IDENTIFIER) {
            Symbol *sym = scope_lookup_chain(current_scope,
                node->data.call.callee->data.ident.name);
            if (sym) {
                // Check arg count vs param count
                GclType *ft = sym->type;
                if (ft && ft->category == TYPE_FUNCTION && ft->param_types) {
                    int expected = ft->param_types->count;
                    int actual = node->data.call.args ? node->data.call.args->count : 0;
                    if (expected != actual && !(sym->kind == SYM_FUNCTION && ft->is_unsigned)) {
                        // only warn if not variadic
                        GclSourceLoc loc = node_loc(node);
                        char buf[256];
                        snprintf(buf, sizeof(buf), "function '%s' expects %d arg(s), got %d",
                            node->data.call.callee->data.ident.name, expected, actual);
                        error_warning(W002, loc, buf, NULL, NULL);
                    }
                }
                if (ft && ft->category == TYPE_FUNCTION && ft->return_type) {
                    node->value_type = ft->return_type;
                    return node->value_type;
                }
            }
        }
        node->value_type = type_int();
        return node->value_type;
    }

    // ── Subscript ─────────────────────────────────────────
    case AST_SUBSCRIPT: {
        GclType *bt = analyze_expr(node->data.subscript.base);
        analyze_expr(node->data.subscript.index);
        if (type_is_array(bt)) {
            node->value_type = bt->base_type;
        } else if (type_is_pointer(bt)) {
            node->value_type = bt->base_type;
        } else {
            node->value_type = type_int();
            error_syntax(E013, node_loc(node),
                "subscript on non-array, non-pointer type", NULL, NULL);
            semantic_errors++;
        }
        return node->value_type;
    }

    // ── Member ────────────────────────────────────────────
    case AST_MEMBER:
    case AST_MEMBER_PTR:
        analyze_expr(node->data.member.base);
        node->value_type = type_int(); // stub — proper struct typing later
        return node->value_type;

    // ── Assign ────────────────────────────────────────────
    case AST_ASSIGN: {
        GclType *lt = analyze_expr(node->data.assign.lhs);
        GclType *rt = analyze_expr(node->data.assign.rhs);
        if (!type_compatible(rt, lt)) {
            GclSourceLoc loc = node_loc(node);
            error_warning(W002, loc, "assignment from incompatible type", NULL, NULL);
        }
        node->value_type = lt;
        return node->value_type;
    }

    // ── Sizeof ────────────────────────────────────────────
    case AST_SIZEOF:
        if (node->data.sizeof_expr.expr)
            analyze_expr(node->data.sizeof_expr.expr);
        node->value_type = type_int(); // sizeof returns size_t/int
        return node->value_type;

    // ── Array Init ────────────────────────────────────────
    case AST_ARRAY_INIT:
        if (node->data.array_init.elements) {
            for (GclAstNode *e = node->data.array_init.elements->head; e; e = e->next)
                analyze_expr(e);
        }
        node->value_type = type_int();
        return node->value_type;

    default:
        node->value_type = type_int();
        return node->value_type;
    }
}

// ── Statement analysis ────────────────────────────────────
static void analyze_stmt(GclAstNode *node, GclType *expected_return) {
    if (!node) return;

    switch (node->type) {
    case AST_COMPOUND: {
        // Create a new scope for the block
        Scope *block_scope = scope_create(current_scope, current_scope->depth + 1);
        scope_enter(block_scope);
        if (node->data.compound.statements) {
            for (GclAstNode *s = node->data.compound.statements->head; s; s = s->next)
                analyze_decl(s);
        }
        scope_exit(block_scope);
        break;
    }

    case AST_IF:
        analyze_expr(node->data.if_stmt.cond);
        if (node->data.if_stmt.then_branch)
            analyze_stmt(node->data.if_stmt.then_branch, expected_return);
        if (node->data.if_stmt.else_branch)
            analyze_stmt(node->data.if_stmt.else_branch, expected_return);
        break;

    case AST_FOR: {
        Scope *for_scope = scope_create(current_scope, current_scope->depth + 1);
        scope_enter(for_scope);
        if (node->data.for_stmt.init) {
            if (node->data.for_stmt.init->type == AST_VAR_DECL)
                analyze_decl(node->data.for_stmt.init);
            else
                analyze_expr(node->data.for_stmt.init);
        }
        if (node->data.for_stmt.cond) analyze_expr(node->data.for_stmt.cond);
        if (node->data.for_stmt.incr) analyze_expr(node->data.for_stmt.incr);
        if (node->data.for_stmt.body)
            analyze_stmt(node->data.for_stmt.body, expected_return);
        scope_exit(for_scope);
        break;
    }

    case AST_WHILE:
        analyze_expr(node->data.while_stmt.cond);
        if (node->data.while_stmt.body)
            analyze_stmt(node->data.while_stmt.body, expected_return);
        break;

    case AST_DO_WHILE:
        if (node->data.do_while.body)
            analyze_stmt(node->data.do_while.body, expected_return);
        analyze_expr(node->data.do_while.cond);
        break;

    case AST_SWITCH:
        analyze_expr(node->data.switch_stmt.expr);
        if (node->data.switch_stmt.body)
            analyze_stmt(node->data.switch_stmt.body, expected_return);
        break;

    case AST_CASE:
        if (node->data.case_stmt.value) analyze_expr(node->data.case_stmt.value);
        if (node->data.case_stmt.stmt)
            analyze_stmt(node->data.case_stmt.stmt, expected_return);
        break;

    case AST_RETURN:
        if (node->data.return_stmt.expr) {
            GclType *rt = analyze_expr(node->data.return_stmt.expr);
            if (expected_return && !type_compatible(rt, expected_return)) {
                error_warning(W002, node_loc(node),
                    "return type mismatch", NULL, NULL);
            }
        } else if (expected_return && !type_is_void(expected_return)) {
            error_warning(W002, node_loc(node),
                "missing return value in non-void function", NULL, NULL);
        }
        break;

    case AST_BREAK:
    case AST_CONTINUE:
    case AST_EXPR_STMT:
        break;

    case AST_VAR_DECL:
        analyze_decl(node);
        break;

    default:
        // Expression statement
        analyze_expr(node);
        break;
    }
}

// ── Declaration analysis ──────────────────────────────────
static void analyze_decl(GclAstNode *node) {
    if (!node) return;

    switch (node->type) {
    // ── Variable Declaration ──────────────────────────────
    case AST_VAR_DECL: {
        const char *name = node->data.var_decl.name;
        GclType   *type = node->data.var_decl.var_type;
        if (!name || !type) return;

        Symbol *sym = scope_insert(current_scope, name, SYM_VARIABLE,
                                    type, node->loc.line, node->loc.col);
        if (!sym) return;
        sym->is_defined = 1;

        // Analyze initializer
        if (node->data.var_decl.init) {
            GclType *init_type = analyze_expr(node->data.var_decl.init);
            if (init_type && !type_compatible(init_type, type)) {
                // Allow char literal → int (promotion OK)
                if (!(type_is_integer(type) &&
                      node->data.var_decl.init->type == AST_CHAR_LITERAL)) {
                    error_warning(W002, node_loc(node),
                        "initializer type mismatch", NULL, NULL);
                }
            }
        }
        node->value_type = type;
        break;
    }

    // ── Function Definition ───────────────────────────────
    case AST_FUNC_DEF: {
        const char *name = node->data.func.name;
        GclType   *ret  = node->data.func.return_type;
        if (!name) return;

        // Check for main
        if (strcmp(name, "main") == 0)
            have_main_function = 1;

        // Insert into global/parent scope
        Scope *target = current_scope;
        // Functions go into the parent scope if we're inside a function
        // For top-level, current_scope is global
        Symbol *sym = scope_lookup(target, name);
        if (sym) {
            // Redeclaration check
            if (sym->is_defined) {
                GclSourceLoc loc = node_loc(node);
                char buf[256];
                snprintf(buf, sizeof(buf), "redefinition of function '%s'", name);
                error_syntax(E012, loc, buf, NULL, NULL);
                semantic_errors++;
                break;
            }
            // Forward declaration match — update is_defined
            sym->is_defined = 1;
        } else {
            sym = scope_insert(target, name, SYM_FUNCTION, ret,
                              node->loc.line, node->loc.col);
        }
        if (!sym) break;

        // Create function scope for params + body
        Scope *func_scope = scope_create(target, target->depth + 1);
        scope_enter(func_scope);

        // Register parameters
        if (node->data.func.params) {
            for (GclAstNode *p = node->data.func.params->head; p; p = p->next) {
                if (p->data.param.name && strlen(p->data.param.name) > 0) {
                    scope_insert(func_scope, p->data.param.name, SYM_PARAMETER,
                                p->data.param.param_type, p->loc.line, p->loc.col);
                }
            }
        }

        // Analyze body
        if (node->data.func.body) {
            analyze_stmt(node->data.func.body, ret);
        }

        scope_exit(func_scope);
        node->value_type = ret;
        break;
    }

    // ── Statements (same as analyze_stmt) ──────────────────
    case AST_COMPOUND:
    case AST_IF:
    case AST_FOR:
    case AST_WHILE:
    case AST_DO_WHILE:
    case AST_SWITCH:
    case AST_CASE:
    case AST_RETURN:
    case AST_BREAK:
    case AST_CONTINUE:
        analyze_stmt(node, NULL);
        break;

    // ── Expression statements ─────────────────────────────
    default:
        analyze_expr(node);
        break;
    }
}

// ── Main entry point ──────────────────────────────────────
int semantic_analyze(GclAstNode *program) {
    if (!program) return 0;

    semantic_errors = 0;
    have_main_function = 0;

    // Create global scope
    Scope *global = scope_create(NULL, 0);

    // Register builtin functions
    scope_insert(global, "printf", SYM_FUNCTION, type_int(), 0, 0);
    scope_insert(global, "malloc", SYM_FUNCTION, type_char_ptr(), 0, 0);
    scope_insert(global, "calloc", SYM_FUNCTION, type_char_ptr(), 0, 0);
    scope_insert(global, "realloc", SYM_FUNCTION, type_char_ptr(), 0, 0);
    scope_insert(global, "free", SYM_FUNCTION, type_void(), 0, 0);
    scope_insert(global, "strlen", SYM_FUNCTION, type_int(), 0, 0);

    scope_enter(global);

    // Walk top-level declarations
    for (GclAstNode *node = program->next; node; node = node->next) {
        // Skip preprocessor and comments
        switch (node->type) {
        case AST_PREP_INCLUDE: case AST_PREP_EXTERN: case AST_PREP_LIB:
        case AST_PREP_DEFINE: case AST_PREP_UNDEF:
        case AST_PREP_IFDEF: case AST_PREP_IFNDEF: case AST_PREP_IF:
        case AST_PREP_ELIF: case AST_PREP_ELSE: case AST_PREP_ENDIF:
        case AST_PREP_ERROR: case AST_PREP_PRAGMA: case AST_PREP_LINE:
        case AST_GCL_COMMENT: case AST_GCL_COMMENT_BLOCK: case AST_GCL_COMMENT_CPP:
            continue;
        default: break;
        }
        analyze_decl(node);
    }

    // Check if main exists when -run mode is used (optional check)
    // Note: we don't enforce main existence here; it's optional

    scope_exit(global);

    // Return error count
    if (semantic_errors > 0)
        return semantic_errors;
    return 0;
}
