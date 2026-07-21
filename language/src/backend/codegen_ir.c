#include "codegen_ir.h"
#include "codegen.h"
#include "codegen_debug.h"
#include "defines.h"
#include "colors.h"
#include <stdio.h>
#include <string.h>

/* ---------- helpers ---------- */

static const char *node_kind_name(NodeKind k) {
    switch (k) {
        case NODE_LIB:     return "#lib";
        case NODE_INCLUDE: return "#include";
        case NODE_EXTERN:  return "#extern";
        default:           return "?";
    }
}

static void emit_if_condition(AstNode *n) {
    if (!n) return;
    if (n->left && n->left->kind == NODE_IDENT && strcmp(n->left->value, "defined") == 0) {
        fprintf(g_codegen_out, "%sdefined(%s)%s", CLR_CYAN, n->value ? n->value : "", CLR_RESET);
    } else if (n->left && n->left->kind == NODE_IDENT) {
        fprintf(g_codegen_out, "%s%s %s %s%s", CLR_MAGENTA, n->value ? n->value : "?",
            n->left->value, n->right ? n->right->value : "?", CLR_RESET);
    } else if (n->value && n->value[0]) {
        fprintf(g_codegen_out, "%s%s%s", CLR_MAGENTA, n->value, CLR_RESET);
        AstNode *alt = n->right;
        while (alt) {
            fprintf(g_codegen_out, " / %s%s%s", CLR_MAGENTA, alt->value, CLR_RESET);
            alt = alt->next;
        }
    }
}

/* ---------- conditional block recursive dump ---------- */

static void emit_ir_conditional_block(AstNode *n, int depth) {
    if (!n) return;
    const char *indent = "  ";
    for (int i = 0; i < depth; i++) fprintf(g_codegen_out, "%s", indent);

    if (n->kind == NODE_IFDEF) {
        int defined = defines_exists(n->value);
        const char *active = defined ? CLR_GREEN "(active)" CLR_RESET : CLR_RED "(inactive)" CLR_RESET;
        fprintf(g_codegen_out, "%s#ifdef%s %s%s%s %s\n", CLR_CYAN, CLR_RESET,
            CLR_MAGENTA, n->value ? n->value : "?", CLR_RESET, active);
    } else if (n->kind == NODE_IFNDEF) {
        int defined = defines_exists(n->value);
        const char *active = !defined ? CLR_GREEN "(active)" CLR_RESET : CLR_RED "(inactive)" CLR_RESET;
        fprintf(g_codegen_out, "%s#ifndef%s %s%s%s %s\n", CLR_CYAN, CLR_RESET,
            CLR_MAGENTA, n->value ? n->value : "?", CLR_RESET, active);
    } else if (n->kind == NODE_IF) {
        fprintf(g_codegen_out, "%s#if%s ", CLR_CYAN, CLR_RESET);
        emit_if_condition(n);
        fprintf(g_codegen_out, "\n");
    }

    /* walk children */
    AstNode *child = n->next;
    while (child) {
        /* nested conditional */
        if (child->kind == NODE_IFDEF || child->kind == NODE_IFNDEF || child->kind == NODE_IF) {
            emit_ir_conditional_block(child, depth + 1);
            int inner_depth = 1;
            while (child && inner_depth > 0) {
                if (child->kind == NODE_IFDEF || child->kind == NODE_IFNDEF || child->kind == NODE_IF) inner_depth++;
                else if (child->kind == NODE_ENDIF) inner_depth--;
                child = child->next;
            }
            continue;
        }

        /* #elif */
        if (child->kind == NODE_ELIF) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, "%s#elif%s ", CLR_CYAN, CLR_RESET);
            emit_if_condition(child);
            fprintf(g_codegen_out, "\n");
            /* body between #elif and next directive */
            AstNode *body = child->next;
            while (body && body->kind != NODE_ELIF && body->kind != NODE_ELSE &&
                   body->kind != NODE_ENDIF && body->kind != NODE_IFDEF &&
                   body->kind != NODE_IFNDEF && body->kind != NODE_IF) {
                if (body->kind == NODE_DEFINE) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    const char *val = body->left ? body->left->value : "";
                    fprintf(g_codegen_out, CLR_CYAN "#define" CLR_RESET " %s " CLR_MAGENTA "%s" CLR_RESET "\n", body->value, val);
                } else if (body->kind == NODE_RAW) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_DIM "%.*s" CLR_RESET, (int)body->len, body->value);
                } else if (body->kind == NODE_MESSAGE) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_YELLOW "message: %s" CLR_RESET "\n", body->left ? body->left->value : "");
                } else if (body->kind == NODE_ERROR) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_RED "#error: %s" CLR_RESET "\n", body->left ? body->left->value : "");
                }
                body = body->next;
            }
            child = body;
            continue;
        }

        /* #else */
        if (child->kind == NODE_ELSE) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, CLR_CYAN "#else" CLR_RESET "\n");
            AstNode *body = child->next;
            while (body && body->kind != NODE_ENDIF && body->kind != NODE_ELIF &&
                   body->kind != NODE_IFDEF && body->kind != NODE_IFNDEF && body->kind != NODE_IF) {
                if (body->kind == NODE_DEFINE) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    const char *val = body->left ? body->left->value : "";
                    fprintf(g_codegen_out, CLR_CYAN "#define" CLR_RESET " %s " CLR_MAGENTA "%s" CLR_RESET "\n", body->value, val);
                } else if (body->kind == NODE_RAW) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_DIM "%.*s" CLR_RESET, (int)body->len, body->value);
                } else if (body->kind == NODE_MESSAGE) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_YELLOW "message: %s" CLR_RESET "\n", body->left ? body->left->value : "");
                } else if (body->kind == NODE_ERROR) {
                    for (int i = 0; i < depth + 2; i++) fprintf(g_codegen_out, "%s", indent);
                    fprintf(g_codegen_out, CLR_RED "#error: %s" CLR_RESET "\n", body->left ? body->left->value : "");
                }
                body = body->next;
            }
            child = body;
            continue;
        }

        /* #endif */
        if (child->kind == NODE_ENDIF) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, "%s#endif" CLR_RESET "\n", CLR_CYAN);
            return;
        }

        /* body content */
        if (child->kind == NODE_DEFINE) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            const char *val = child->left ? child->left->value : "";
            fprintf(g_codegen_out, CLR_CYAN "#define" CLR_RESET " %s " CLR_MAGENTA "%s" CLR_RESET "\n", child->value, val);
        } else if (child->kind == NODE_RAW) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, CLR_DIM "%.*s" CLR_RESET, (int)child->len, child->value);
        } else if (child->kind == NODE_MESSAGE) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, CLR_YELLOW "message: %s" CLR_RESET "\n", child->left ? child->left->value : "");
        } else if (child->kind == NODE_ERROR) {
            for (int i = 0; i < depth + 1; i++) fprintf(g_codegen_out, "%s", indent);
            fprintf(g_codegen_out, CLR_RED "#error: %s" CLR_RESET "\n", child->left ? child->left->value : "");
        }
        child = child->next;
    }
}

/* ---------- debug-statement dump ---------- */

static void emit_ir_debug_statements(AstNode *prog) {
    AstNode *n = prog->left;
    int count = 0;
    while (n) {
        if (n->kind == NODE_DEBUG) {
            fprintf(g_codegen_out, "  %s#debug%s ", CLR_CYAN, CLR_RESET);
            AstNode *arg = n->left;
            while (arg) {
                if (arg->kind == NODE_STRING) {
                    size_t len = arg->len;
                    const char *s = arg->value;
                    if (len >= 2 && s[0] == '"') { s++; len -= 2; }
                    fprintf(g_codegen_out, "%s\"%.*s\"%s ", CLR_DIM, (int)len, s, CLR_RESET);
                } else if (arg->kind == NODE_IDENT) {
                    const char *rv = codegen_resolve(arg->value);
                    if (strcmp(rv, arg->value) != 0) {
                        size_t l = strlen(rv);
                        if (l >= 2 && rv[0] == '"' && rv[l-1] == '"')
                            fprintf(g_codegen_out, "%s\"%.*s\"%s ", CLR_DIM, (int)(l-2), rv+1, CLR_RESET);
                        else
                            fprintf(g_codegen_out, "%s%s%s ", CLR_MAGENTA, rv, CLR_RESET);
                    } else {
                        fprintf(g_codegen_out, "%s%s%s ", CLR_CYAN, arg->value, CLR_RESET);
                    }
                } else if (arg->kind == NODE_NUMBER) {
                    fprintf(g_codegen_out, "%s%.*s%s ", CLR_MAGENTA, (int)arg->len, arg->value, CLR_RESET);
                }
                arg = arg->next;
            }
            fprintf(g_codegen_out, "\n");
            count++;
        }
        n = n->next;
    }
    if (count == 0) fprintf(g_codegen_out, "  " CLR_DIM "(none)" CLR_RESET "\n");
}

/* ---------- public entry ---------- */

void codegen_ir_emit(AstNode *prog) {
    fprintf(g_codegen_out, CLR_PURPLE "; -- IR Dump --" CLR_RESET "\n");

    /* Phase 1: dependency chain */
    fprintf(g_codegen_out, CLR_PURPLE "; Dependency chain:" CLR_RESET "\n");
    AstNode *n = prog->left;
    int dep_count = 0;
    while (n) {
        if (n->kind == NODE_LIB || n->kind == NODE_INCLUDE || n->kind == NODE_EXTERN) {
            const char *fname = n->left ? n->left->value : "?";
            const char *f = fname;
            if (f[0] == '"' || f[0] == '<') f++;
            size_t flen = strlen(f);
            if (flen > 0 && (f[flen - 1] == '"' || f[flen - 1] == '>')) flen--;
            fprintf(g_codegen_out, "  %s%-10s%s %s%.*s%s\n",
                CLR_CYAN, node_kind_name(n->kind), CLR_RESET,
                CLR_DIM, (int)flen, f, CLR_RESET);
            dep_count++;
        }
        n = n->next;
    }
    if (dep_count == 0) fprintf(g_codegen_out, "  " CLR_DIM "(none)" CLR_RESET "\n");

    /* Phase 2: resolved defines */
    fprintf(g_codegen_out, "\n" CLR_PURPLE "; Resolved defines:" CLR_RESET "\n");
    n = prog->left;
    int def_count = 0;
    while (n) {
        if (n->kind == NODE_DEFINE) {
            const char *val = n->left ? n->left->value : "";
            fprintf(g_codegen_out, "  %s%-20s%s = %s%s%s\n",
                CLR_CYAN, n->value, CLR_RESET,
                CLR_MAGENTA, val, CLR_RESET);
            def_count++;
        }
        n = n->next;
    }
    if (def_count == 0) fprintf(g_codegen_out, "  " CLR_DIM "(none)" CLR_RESET "\n");

    /* Phase 3: extern symbols */
    fprintf(g_codegen_out, "\n" CLR_PURPLE "; Extern symbols:" CLR_RESET "\n");
    const char *en;
    defines_next_extern(NULL);
    int ext_count = 0;
    while (defines_next_extern(&en)) {
        fprintf(g_codegen_out, "  %s%s%s\n", CLR_YELLOW, en, CLR_RESET);
        ext_count++;
    }
    if (ext_count == 0) fprintf(g_codegen_out, "  " CLR_DIM "(none)" CLR_RESET "\n");

    /* Phase 4: conditional blocks */
    fprintf(g_codegen_out, "\n" CLR_PURPLE "; Conditional blocks:" CLR_RESET "\n");
    n = prog->left;
    int cond_count = 0;
    while (n) {
        if (n->kind == NODE_IFDEF || n->kind == NODE_IFNDEF || n->kind == NODE_IF) {
            emit_ir_conditional_block(n, 1);
            cond_count++;
            int depth = 1;
            n = n->next;
            while (n && depth > 0) {
                if (n->kind == NODE_IFDEF || n->kind == NODE_IFNDEF || n->kind == NODE_IF) depth++;
                else if (n->kind == NODE_ENDIF) depth--;
                n = n->next;
            }
            continue;
        }
        n = n->next;
    }
    if (cond_count == 0) fprintf(g_codegen_out, "  " CLR_DIM "(none)" CLR_RESET "\n");

    /* Phase 5: #debug statements */
    fprintf(g_codegen_out, "\n" CLR_PURPLE "; #debug statements:" CLR_RESET "\n");
    emit_ir_debug_statements(prog);
    fprintf(g_codegen_out, CLR_DIM ";" CLR_RESET "\n");
}
