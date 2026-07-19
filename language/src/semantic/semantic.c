/* Semantic: sembol tablosu + scope yonetimi + AST traversal */
#include "gcl.h"
#include "ast.h"
#include "semantic.h"
#include "error.h"

static const char *g_src = 0;

void sem_init(SemState *s) { s->cur = 0; s->errors = 0; }

static Scope *scope_new(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    if (!s) return 0;
    s->parent = parent;
    s->level = parent ? parent->level + 1 : 0;
    return s;
}

void sem_enter_scope(SemState *s) {
    Scope *ns = scope_new(s->cur);
    if (ns) s->cur = ns;
}
void sem_exit_scope(SemState *s) {
    if (s->cur) s->cur = s->cur->parent;
}

int sem_add_sym(SemState *s, const char *name, int type, SourceLoc loc) {
    if (!s->cur) return 0;
    for (Sym *p = s->cur->syms; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            error_report(E_REDECLARED, loc, g_src, "'%s' already declared", name);
            s->errors++; return 0;
        }
    }
    Sym *sym = calloc(1, sizeof(Sym));
    if (!sym) return 0;
    strncpy(sym->name, name, 255); sym->name[255] = 0;
    sym->type = type; sym->scope_level = s->cur->level;
    sym->next = s->cur->syms; s->cur->syms = sym;
    return 1;
}

Sym *sem_lookup(SemState *s, const char *name) {
    for (Scope *sc = s->cur; sc; sc = sc->parent) {
        for (Sym *p = sc->syms; p; p = p->next) {
            if (strcmp(p->name, name) == 0) return p;
        }
    }
    return 0;
}

static void sem_node(SemState *s, Node *n) {
    if (!n) return;
    switch (n->type) {
    case N_PROGRAM:
        sem_enter_scope(s);
        for (Node *c = n->left ? n->left : n->next; c; c = c->next)
            sem_node(s, c);
        break;
    case N_VAR_DECL:
        sem_add_sym(s, n->name, 0, n->loc);
        if (n->left) sem_node(s, n->left);
        break;
    case N_IDENT:
        if (!sem_lookup(s, n->name)) {
            error_report(E_UNDECLARED, n->loc, g_src, "'%s' not declared", n->name);
            s->errors++;
        }
        break;
    case N_BINARY:
        sem_node(s, n->left); sem_node(s, n->right);
        break;
    case N_UNARY: case N_POSTFIX:
        sem_node(s, n->left);
        break;
    case N_ASSIGN:
        sem_node(s, n->left); sem_node(s, n->right);
        break;
    case N_TERNARY:
        sem_node(s, n->cond); sem_node(s, n->left); sem_node(s, n->right);
        break;
    default: break;
    }
}

int sem_analyze(SemState *s, Node *prog, const char *src) {
    g_src = src;
    sem_node(s, prog);
    return s->errors;
}
