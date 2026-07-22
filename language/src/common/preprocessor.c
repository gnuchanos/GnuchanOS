#include "preprocessor.h"
#include "defines.h"
#include "colors.h"
#include "lexer.h"
#include "parser.h"
#include "io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- included file tracking ---------- */

#define MAX_INCLUDED 256
static struct {
    char        name[256];
    AstNode    *ast;
    int         is_lib;
} g_included[MAX_INCLUDED];
static int g_included_count = 0;

void preprocessor_init(void) {
    g_included_count = 0;
}

int preprocess_included_count(void)           { return g_included_count; }
const char *preprocess_included_name(int i)   { return g_included[i].name; }
AstNode *preprocess_included_ast(int i)       { return g_included[i].ast; }
int preprocess_included_is_lib(int i)         { return g_included[i].is_lib; }

void preprocess_free_included(void) {
    for (int i = 0; i < g_included_count; i++) {
        free(g_included[i].ast);
        g_included[i].ast = NULL;
    }
    g_included_count = 0;
}

/* ---------- file resolution ---------- */

char *preprocess_resolve_path(const char *src_dir, const char *filename) {
    const char *f = filename;
    if (f[0] == '"' || f[0] == '<') f++;
    size_t flen = strlen(f);
    if (flen > 0 && (f[flen - 1] == '"' || f[flen - 1] == '>')) flen--;
    size_t dlen = strlen(src_dir);
    char full[2048];

    snprintf(full, sizeof(full), "%.*s%.*s", (int)dlen, src_dir, (int)flen, f);
    char *content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.gcsf", (int)dlen, src_dir, (int)flen, f);
    content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.gclib", (int)dlen, src_dir, (int)flen, f);
    content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.h", (int)dlen, src_dir, (int)flen, f);
    content = file_read(full); if (content) return content;
    return NULL;
}

/* ---------- symbol table helpers ---------- */

static int exec_define(AstNode *n) {
    if (!n || n->kind != NODE_DEFINE) return 0;
    defines_set(n->value, n->left ? n->left->value : "");
    return 1;
}

static void exec_extern_block(AstNode *n) {
    if (!n || n->kind != NODE_EXTERN_C_BLOCK) return;
    AstNode *inner = n->left;
    while (inner) {
        if (inner->kind == NODE_DEFINE) {
            const char *sym = inner->left ? inner->left->value : "";
            defines_set(inner->value, sym);
            defines_add_extern(sym);
        }
        inner = inner->next;
    }
}

/* ---------- condition evaluation ---------- */

static int eval_name(const char *name) {
    return name ? defines_exists(name) : 0;
}

static int smart_compare(const char *lv, const char *rv) {
    if (!lv) lv = "0";
    if (!rv) rv = "0";
    char *le = NULL, *re = NULL;
    long ln = strtol(lv, &le, 10), rn = strtol(rv, &re, 10);
    if (le && *le == '\0' && re && *re == '\0') {
        if (ln < rn) return -1;
        if (ln > rn) return 1;
        return 0;
    }
    return strcmp(lv, rv);
}

static int eval_condition(AstNode *n) {
    if (!n) return 0;
    if (n->left && n->left->kind == NODE_IDENT && strcmp(n->left->value, "defined") == 0)
        return defines_exists(n->value);
    if (n->left && n->left->kind == NODE_IDENT) {
        const char *op = n->left->value;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "<") == 0 ||
            strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
            const char *lv = defines_get(n->value);
            const char *rv = n->right ? n->right->value : NULL;
            if (!lv) lv = "0";
            if (!rv) rv = "0";
            int cmp = smart_compare(lv, rv);
            if (strcmp(op, "==") == 0) return cmp == 0;
            if (strcmp(op, "!=") == 0) return cmp != 0;
            if (strcmp(op, "<")  == 0) return cmp < 0;
            if (strcmp(op, ">")  == 0) return cmp > 0;
            if (strcmp(op, "<=") == 0) return cmp <= 0;
            if (strcmp(op, ">=") == 0) return cmp >= 0;
            return 0;
        }
    }
    if (eval_name(n->value)) return 1;
    AstNode *alt = n->right;
    while (alt) { if (eval_name(alt->value)) return 1; alt = alt->next; }
    return 0;
}

/* ---------- inline preprocessor ---------- */

AstNode *preprocess_inline_ex(AstNode *prog, int keep_all) {
    AstNode dummy = {0}; AstNode *tail = &dummy;
    AstNode *n = prog->left;
    #define CSTACK_MAX 64
    int cstack[CSTACK_MAX] = {0}; int csp = 0; cstack[0] = 1;

    while (n) {
        AstNode *next = n->next;
        if (n->kind == NODE_IFDEF || n->kind == NODE_IFNDEF || n->kind == NODE_IF) {
            csp++; if (csp >= CSTACK_MAX) break;
            if (cstack[csp - 1] != 1) cstack[csp] = 0;
            else {
                int ok = 0;
                if (n->kind == NODE_IFDEF) ok = defines_exists(n->value);
                else if (n->kind == NODE_IFNDEF) ok = !defines_exists(n->value);
                else ok = eval_condition(n);
                cstack[csp] = ok ? 1 : 0;
            }
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ELIF) {
            if (csp <= 0) { n = next; continue; }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp - 1] == 1) cstack[csp] = eval_condition(n) ? 1 : 0;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ELSE) {
            if (csp <= 0) { n = next; continue; }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp] == 0 && cstack[csp - 1] == 1) cstack[csp] = 1;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ENDIF) {
            if (csp > 0) csp--;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }

        int active = 1;
        for (int i = 0; i <= csp; i++) { if (cstack[i] != 1) { active = 0; break; } }
        if (!active && !keep_all) { n = next; continue; }

        if (active) {
            if (n->kind == NODE_DEFINE) exec_define(n);
            else if (n->kind == NODE_UNDEF) defines_undef(n->value);
            else if (n->kind == NODE_EXTERN_C_BLOCK) exec_extern_block(n);
            else if (n->kind == NODE_ERROR) {
                fprintf(stderr, CLR_RED "#error:" CLR_RESET " %s\n", n->left ? n->left->value : "");
            } else if (n->kind == NODE_MESSAGE && !keep_all && n->left && n->left->value) {
                printf(CLR_CYAN "#message:" CLR_RESET " %s\n", n->left->value);
            }
        }

        if (n->kind == NODE_DEFINE || n->kind == NODE_UNDEF || n->kind == NODE_EXTERN_C_BLOCK ||
            n->kind == NODE_ERROR || n->kind == NODE_MESSAGE || n->kind == NODE_EXTERN ||
            n->kind == NODE_INCLUDE || n->kind == NODE_LIB || n->kind == NODE_PRAGMA ||
            n->kind == NODE_DEBUG || n->kind == NODE_RAW || active) {
            n->next = NULL; tail->next = n; tail = n;
        }
        n = next;
    }
    #undef CSTACK_MAX
    return dummy.next;
}

/* ---------- load all #include / #lib ---------- */

static int already_included(const char *name) {
    for (int i = 0; i < g_included_count; i++)
        if (strcmp(g_included[i].name, name) == 0) return 1;
    return 0;
}

static void register_included(const char *name, AstNode *ast, int is_lib) {
    if (g_included_count >= MAX_INCLUDED) return;
    const char *start = name;
    if (start[0] == '<' || start[0] == '"') start++;
    size_t len = strlen(start);
    if (len > 0 && (start[len-1] == '>' || start[len-1] == '"')) len--;
    if (len >= sizeof(g_included[0].name)) len = sizeof(g_included[0].name) - 1;
    memcpy(g_included[g_included_count].name, start, len);
    g_included[g_included_count].name[len] = '\0';
    g_included[g_included_count].ast    = ast;
    g_included[g_included_count].is_lib = is_lib;
    g_included_count++;
}

void preprocess_load(AstNode *prog, const char *src_dir) {
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_INCLUDE || n->kind == NODE_LIB) {
            const char *fname = n->left ? n->left->value : NULL;
            if (fname) {
                const char *start = fname;
                if (start[0] == '"' || start[0] == '<') start++;
                size_t len = strlen(start);
                if (len > 0 && (start[len-1] == '"' || start[len-1] == '>')) len--;
                char trimmed[256];
                if (len >= sizeof(trimmed)) len = sizeof(trimmed) - 1;
                memcpy(trimmed, start, len);
                trimmed[len] = '\0';

                if (!already_included(trimmed)) {
                    char *content = preprocess_resolve_path(src_dir, fname);
                    if (content) {
                        Lexer il; lexer_init(&il, content, fname);
                        Parser *ip = parser_new(&il);
                        AstNode *iprog = parser_parse(ip);
                        iprog->left = preprocess_inline(iprog);
                        register_included(trimmed, iprog, n->kind == NODE_LIB ? 1 : 0);
                        free(ip); free(content);
                    }
                }
            }
        }
        n = n->next;
    }
}
