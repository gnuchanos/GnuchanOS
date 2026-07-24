#include "preprocessor.h"
#include "defines.h"
#include "colors.h"
#include "lexer.h"
#include "parser.h"
#include "parse_directive.h"
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
        ast_free(g_included[i].ast);
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

    /* Ensure a '/' or '\\' separator between directory and filename */
    int needs_sep = (dlen > 0 && src_dir[dlen - 1] != '/' && src_dir[dlen - 1] != '\\');
    const char *sep = needs_sep ? "/" : "";

    snprintf(full, sizeof(full), "%.*s%s%.*s", (int)dlen, src_dir, sep, (int)flen, f);
    char *content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%s%.*s.gcsf", (int)dlen, src_dir, sep, (int)flen, f);
    content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%s%.*s.gclib", (int)dlen, src_dir, sep, (int)flen, f);
    content = file_read(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%s%.*s.h", (int)dlen, src_dir, sep, (int)flen, f);
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

/* ---------- parse braced list values from define value ---------- */
/* Given "#define numbers {1, 2, 3, 4, 5}", extract the individual items.
   Returns a linked list of AstNode* (NODE_IDENT, NODE_NUMBER, NODE_STRING) via ->next. */
static AstNode *parse_list_values(const char *value) {
    if (!value || value[0] != '{') return NULL;
    AstNode dummy = {0}; AstNode *tail = &dummy;
    const char *p = value + 1; /* skip '{' */
    while (*p && *p != '}') {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '}' || *p == '\0') break;
        const char *start = p;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\') p++;
                if (*p) p++;
            }
            if (*p == '"') p++;
        } else {
            while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t') p++;
        }
        size_t len = p - start;
        if (len > 0) {
            /* Determine node kind */
            NodeKind k = NODE_IDENT;
            if (len >= 2 && start[0] == '"') k = NODE_STRING;
            else {
                int all_digit = 1;
                for (size_t i = 0; i < len; i++)
                    if (!(start[i] >= '0' && start[i] <= '9') && start[i] != '.' && start[i] != '-')
                        { all_digit = 0; break; }
                if (all_digit && len > 0) k = NODE_NUMBER;
            }
            AstNode *item = make_node(k, start, len, 0, 0);
            tail->next = item; tail = item;
        }
        /* skip comma */
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
    }
    return dummy.next;
}

/* ---------- #for loop expansion ---------- */

/* Expand a #for loop: given the for_node and the body nodes (up to #endfor),
   returns a linked list of unrolled body nodes with variable substitution.
   
   Supports two forms:
     #for var in list         — single var, substitutes with each value
     #for i and val in enumerate(list) — two vars: i (index 1-based) + val
*/
static AstNode *expand_for_loop(AstNode *for_node, AstNode *body_start) {
    if (!for_node) return NULL;

    int is_enumerate = 0;
    const char *iter_name = NULL;

    if (for_node->right && for_node->right->value &&
        strcmp(for_node->right->value, "enumerate") == 0) {
        /* enumerate(list_name): list name is in for_node->value */
        is_enumerate = 1;
        iter_name = for_node->value;
    } else {
        /* Normal #for: iterable name from for_node->right */
        iter_name = for_node->right ? for_node->right->value : NULL;
    }
    if (!iter_name) return NULL;

    /* Resolve iterable through defines */
    const char *iter_value = defines_get(iter_name);
    if (!iter_value) return NULL;

    /* Parse the values list from the define value */
    AstNode *values = parse_list_values(iter_value);
    if (!values) return NULL;

    /* Collect variable names from for_node->left linked list */
    AstNode *first_var = for_node->left;
    if (!first_var) { ast_free(values); return NULL; }
    const char *var1 = first_var->value;
    const char *var2 = first_var->next ? first_var->next->value : NULL;

    /* For enumerate, we need two variables */
    if (is_enumerate && !var2) { ast_free(values); return NULL; }

    /* For each value, clone body and substitute variable(s) */
    AstNode dummy = {0}; AstNode *tail = &dummy;
    int index = 1;
    AstNode *val = values;
    while (val) {
        for (AstNode *b = body_start; b && b->kind != NODE_ENDFOR; b = b->next) {
            AstNode *clone = ast_clone(b);
            if (is_enumerate) {
                /* Generate index as string for substitution */
                char idx_buf[32];
                snprintf(idx_buf, sizeof(idx_buf), "%d", index);
                ast_for_substitute(clone, var1, idx_buf);
                ast_for_substitute(clone, var2, val->value);
            } else {
                ast_for_substitute(clone, var1, val->value);
            }
            if (!dummy.next) { dummy.next = clone; tail = clone; }
            else { tail->next = clone; tail = clone; }
        }
        val = val->next;
        index++;
    }

    ast_free(values);
    return dummy.next;
}

/* ---------- inline preprocessor ---------- */

AstNode *preprocess_inline_ex(AstNode *prog, int keep_all) {
    AstNode dummy = {0}; AstNode *tail = &dummy;
    AstNode *n = prog->left;
    #define CSTACK_MAX 1024
    int cstack[CSTACK_MAX] = {0}; int csp = 0; cstack[0] = 1;

    while (n) {
        AstNode *next = n->next;
        
        /* #for handling: expand loop, skip body up to #endfor */
        if (n->kind == NODE_FOR) {
            /* Collect body nodes until #endfor */
            AstNode *body_start = n->next;
            /* Find matching #endfor */
            AstNode *endfor = NULL;
            AstNode *scan = n->next;
            while (scan) {
                if (scan->kind == NODE_ENDFOR) { endfor = scan; break; }
                scan = scan->next;
            }
            if (endfor && !keep_all) {
                AstNode *expanded = expand_for_loop(n, body_start);
                if (expanded) {
                    /* Append expanded nodes directly */
                    for (AstNode *e = expanded; e; ) {
                        AstNode *enext = e->next;
                        tail->next = e; tail = e;
                        e = enext;
                    }
                    /* Skip for node, body, and endfor */
                    AstNode *kill = n;
                    while (kill != endfor) {
                        AstNode *knext = kill->next;
                        kill->next = NULL;
                        ast_free(kill);
                        kill = knext;
                    }
                    next = endfor->next;
                    endfor->next = NULL;
                    ast_free(endfor);
                    n = next;
                    continue;
                } else {
                    /* Failed to expand — emit #for node as-is so body isn't silently lost */
                    fprintf(stderr, CLR_RED "error:" CLR_RESET " #for expansion failed\n");
                    n->next = NULL; tail->next = n; tail = n;
                    n = next;
                    continue;
                }
            } else if (endfor && keep_all) {
                /* keep_all mode: pass through for node + skip endfor */
                n->next = NULL; tail->next = n; tail = n;
                /* skip body and endfor */
                while (next && next->kind != NODE_ENDFOR) {
                    AstNode *tmp = next->next;
                    next->next = NULL; ast_free(next);
                    next = tmp;
                }
                if (next) { AstNode *tmp = next->next; next->next = NULL; ast_free(next); next = tmp; }
                n = next;
                continue;
            } else {
                /* no matching endfor — error, fall through */
                fprintf(stderr, CLR_RED "error:" CLR_RESET " #for without matching #endfor\n");
                n->next = NULL; tail->next = n; tail = n;
                n = next;
                continue;
            }
        }

        if (n->kind == NODE_IFDEF || n->kind == NODE_IFNDEF || n->kind == NODE_IF) {
            if (csp + 1 >= CSTACK_MAX) break;
            csp++;
            if (cstack[csp - 1] != 1) cstack[csp] = 0;
            else {
                int ok = 0;
                if (n->kind == NODE_IFDEF) ok = defines_exists(n->value);
                else if (n->kind == NODE_IFNDEF) ok = !defines_exists(n->value);
                else ok = eval_condition(n);
                cstack[csp] = ok ? 1 : 0;
            }
            if (keep_all) { n->next = NULL; tail->next = n; tail = n; }
            else { n->next = NULL; ast_free(n); }
            n = next; continue;
        }
        if (n->kind == NODE_ELIF) {
            if (csp < 1) {
                fprintf(stderr, CLR_RED "error[E112]:" CLR_RESET " #elif without matching #if\n");
                fprintf(stderr, "  line=%zu col=%zu\n", n->line, n->col);
                exit(1);
            }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp - 1] == 1) cstack[csp] = eval_condition(n) ? 1 : 0;
            if (keep_all) { n->next = NULL; tail->next = n; tail = n; }
            else { n->next = NULL; ast_free(n); }
            n = next; continue;
        }
        if (n->kind == NODE_ELSE) {
            if (csp < 1) {
                fprintf(stderr, CLR_RED "error[E113]:" CLR_RESET " #else without matching #if\n");
                fprintf(stderr, "  line=%zu col=%zu\n", n->line, n->col);
                exit(1);
            }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp] == 0 && cstack[csp - 1] == 1) cstack[csp] = 1;
            if (keep_all) { n->next = NULL; tail->next = n; tail = n; }
            else { n->next = NULL; ast_free(n); }
            n = next; continue;
        }
        if (n->kind == NODE_ENDIF) {
            if (csp < 1) {
                fprintf(stderr, CLR_RED "error[E114]:" CLR_RESET " #endif without matching #if\n");
                fprintf(stderr, "  line=%zu col=%zu\n", n->line, n->col);
                exit(1);
            }
            csp--;
            if (keep_all) { n->next = NULL; tail->next = n; tail = n; }
            else { n->next = NULL; ast_free(n); }
            n = next; continue;
        }

        int active = 1;
        for (int i = 0; i <= csp; i++) { if (cstack[i] != 1) { active = 0; break; } }
        if (!active && !keep_all) { n->next = NULL; ast_free(n); n = next; continue; }

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
            n->kind == NODE_DEBUG || n->kind == NODE_RAW || n->kind == NODE_FOR ||
            n->kind == NODE_ENDFOR || active || keep_all) {
            n->next = NULL; tail->next = n; tail = n;
        }
        n = next;
    }
    if (csp > 0) {
        fprintf(stderr, CLR_RED "error[E114]:" CLR_RESET " unclosed #if/#ifdef/#ifndef (missing #endif)\n");
        exit(1);
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

/* Strip known file extensions for dedup normalization */
static void strip_gcl_extension(char *name) {
    size_t len = strlen(name);
    const char *exts[] = { ".gcsf", ".gclib", ".h" };
    for (int i = 0; i < 3; i++) {
        size_t elen = strlen(exts[i]);
        if (len > elen && strcmp(name + len - elen, exts[i]) == 0) {
            name[len - elen] = '\0';
            return;
        }
    }
}

void preprocess_load(AstNode *prog, const char *src_dir, const char *linclude_dir, const char *llib_dir) {
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

                /* Normalize: strip extension for dedup so "math" and "math.gcsf" match */
                strip_gcl_extension(trimmed);

                if (!already_included(trimmed)) {
                    char *content = NULL;
                    /* Try source directory first */
                    content = preprocess_resolve_path(src_dir, fname);
                    /* Then -linclude directory for #include files */
                    if (!content && n->kind == NODE_INCLUDE && linclude_dir && linclude_dir[0])
                        content = preprocess_resolve_path(linclude_dir, fname);
                    /* Then -llib directory for #lib files */
                    if (!content && n->kind == NODE_LIB && llib_dir && llib_dir[0])
                        content = preprocess_resolve_path(llib_dir, fname);
                    /* Then the reverse (lib from include dir, include from lib dir) */
                    if (!content && llib_dir && llib_dir[0])
                        content = preprocess_resolve_path(llib_dir, fname);
                    if (!content && linclude_dir && linclude_dir[0])
                        content = preprocess_resolve_path(linclude_dir, fname);
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
