#include "codegen_c.h"
#include "codegen.h"
#include "defines.h"
#include <stdio.h>
#include <string.h>

/* ---------- helpers ---------- */

/* GCL platform names → C preprocessor macros */
static const char *map_platform(const char *name) {
    if (!name) return "0";
    if (strcmp(name, "gnu")    == 0) return "__gnu_linux__";
    if (strcmp(name, "linux")  == 0) return "__linux__";
    if (strcmp(name, "windows")== 0) return "_WIN32";
    if (strcmp(name, "win64")  == 0) return "_WIN64";
    if (strcmp(name, "macos")  == 0 || strcmp(name, "mac")     == 0) return "__APPLE__";
    if (strcmp(name, "apple")  == 0) return "__APPLE__";
    if (strcmp(name, "bsd")    == 0) return "__FreeBSD__";
    return name; /* unknown — pass through as-is */
}

static void emit_c_condition(AstNode *n) {
    if (!n) { fprintf(g_codegen_out, "0"); return; }
    if (n->left && n->left->kind == NODE_IDENT && strcmp(n->left->value, "defined") == 0) {
        fprintf(g_codegen_out, "defined(%s)", n->value ? n->value : "");
        return;
    }
    if (n->left && n->left->kind == NODE_IDENT) {
        const char *op = n->left->value;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
            strcmp(op, "<") == 0  || strcmp(op, ">") == 0  ||
            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
            fprintf(g_codegen_out, "%s %s %s",
                n->value ? n->value : "0", op, n->right ? n->right->value : "0");
            return;
        }
    }
    if (n->value && n->value[0]) {
        const char *mapped = map_platform(n->value);
        fprintf(g_codegen_out, "defined(%s)", mapped);
        for (AstNode *a = n->right; a; a = a->next)
            if (a->value) fprintf(g_codegen_out, " || defined(%s)", map_platform(a->value));
        return;
    }
    fprintf(g_codegen_out, "0");
}

static int is_c_ident(const char *s) {
    if (!s || !*s) return 0;
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_')) return 0;
    for (s++; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
              (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}

static void fmt_append(char *fmt, const char *s, size_t len) {
    int fl = (int)strlen(fmt);
    if (fl + (int)len >= 4095) return;
    memcpy(fmt + fl, s, len);
    fmt[fl + len] = '\0';
}

static void emit_debug_fmt(AstNode *arg, char *fmt, char *args, int *ac) {
    if (!arg) return;
    if (arg->kind == NODE_STRING) {
        size_t len = arg->len; const char *s = arg->value;
        if (len >= 2 && s[0] == '"') { s++; len -= 2; }
        fmt_append(fmt, s, len);
    } else if (arg->kind == NODE_IDENT) {
        /* resolve through defines table */
        const char *resolved = defines_get(arg->value);
        if (!resolved) resolved = arg->value;

        if (strcmp(resolved, arg->value) == 0) {
            /* not a define — treat as C variable */
            strcat(fmt, "%s"); if (*ac > 0) strcat(args, ", "); (*ac)++;
            strcat(args, arg->value);
        } else if (resolved[0] == '"') {
            /* string literal — embed without quotes */
            size_t rlen = strlen(resolved);
            if (rlen >= 2) fmt_append(fmt, resolved + 1, rlen - 2);
        } else if (is_c_ident(resolved)) {
            /* resolves to another identifier — use %s with that name */
            strcat(fmt, "%s"); if (*ac > 0) strcat(args, ", "); (*ac)++;
            strcat(args, resolved);
        } else {
            /* number or other value — embed directly */
            fmt_append(fmt, resolved, strlen(resolved));
        }
    } else if (arg->kind == NODE_NUMBER) {
        fmt_append(fmt, arg->value, arg->len);
    }
}

static void emit_node(AstNode *n, int indent) {
    if (!n) return;
    char ind[32]; memset(ind, ' ', indent * 4); ind[indent * 4] = '\0';

    switch (n->kind) {
    case NODE_IFDEF:  fprintf(g_codegen_out, "%s#ifdef %s\n", ind, n->value ? n->value : ""); break;
    case NODE_IFNDEF: fprintf(g_codegen_out, "%s#ifndef %s\n", ind, n->value ? n->value : ""); break;
    case NODE_IF:
        fprintf(g_codegen_out, "%s#if ", ind); emit_c_condition(n); fprintf(g_codegen_out, "\n"); break;
    case NODE_ELIF:
        fprintf(g_codegen_out, "%s#elif ", ind); emit_c_condition(n); fprintf(g_codegen_out, "\n"); break;
    case NODE_ELSE:   fprintf(g_codegen_out, "%s#else\n", ind); break;
    case NODE_ENDIF:  fprintf(g_codegen_out, "%s#endif\n", ind); break;
    case NODE_DEFINE: {
        const char *v = n->left ? n->left->value : NULL;
        if (!v || !v[0]) fprintf(g_codegen_out, "%s#define %s\n", ind, n->value ? n->value : "");
        else fprintf(g_codegen_out, "%s#define %s %s\n", ind, n->value ? n->value : "", v);
        break;
    }
    case NODE_UNDEF:  fprintf(g_codegen_out, "%s#undef %s\n", ind, n->value ? n->value : ""); break;
    case NODE_ERROR:  fprintf(g_codegen_out, "%s#error %s\n", ind, n->left ? n->left->value : ""); break;
    case NODE_DEBUG: {
        char fmt[4096] = "", args[4096] = ""; int ac = 0;
        for (AstNode *a = n->left; a; a = a->next) emit_debug_fmt(a, fmt, args, &ac);
        if (ac > 0) fprintf(g_codegen_out, "%sprintf(\"%s\\n\", %s);\n", ind, fmt, args);
        else fprintf(g_codegen_out, "%sprintf(\"%s\\n\");\n", ind, fmt);
        break;
    }
    case NODE_MESSAGE:
        if (n->left && n->left->value) {
            fprintf(g_codegen_out, "%sprintf(\"", ind);
            for (const char *c = n->left->value; *c; c++) {
                if (*c == '\\') fputs("\\\\", g_codegen_out);
                else if (*c == '"') fputs("\\\"", g_codegen_out);
                else fputc(*c, g_codegen_out);
            }
            fprintf(g_codegen_out, "\\n\");\n");
        }
        break;
    case NODE_RAW:
        if (n->value) fprintf(g_codegen_out, "%s%.*s", ind, (int)n->len, n->value);
        break;
    case NODE_EXTERN_C_BLOCK:
        if (n->left)
            for (AstNode *inner = n->left; inner; inner = inner->next)
                if (inner->kind == NODE_DEFINE)
                    emit_node(inner, indent);
        break;
    default: break;
    }
}

static int is_runtime(NodeKind k) {
    return k == NODE_DEBUG || k == NODE_MESSAGE || k == NODE_RAW;
}

static int is_conditional(NodeKind k) {
    return k == NODE_IFDEF || k == NODE_IFNDEF || k == NODE_IF ||
           k == NODE_ELIF || k == NODE_ELSE || k == NODE_ENDIF || k == NODE_ERROR;
}

/* ---------- header emission ---------- */

void codegen_c_emit_header(AstNode *prog, const char *guard) {
    (void)guard;
    fprintf(g_codegen_out, "#pragma once\n\n");

    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_DEFINE) {
            emit_node(n, 0);
        } else if (n->kind == NODE_EXTERN_C_BLOCK && n->left) {
            for (AstNode *inner = n->left; inner; inner = inner->next) {
                if (inner->kind == NODE_DEFINE) {
                    /* emit as #define alias (not extern const char*) */
                    fprintf(g_codegen_out, "#define %s %s\n",
                        inner->value,
                        inner->left ? inner->left->value : "");
                }
            }
        } else if (n->kind == NODE_EXTERN && n->left) {
            const char *f = n->left->value;
            if (f[0] == '"' || f[0] == '<') f++;
            size_t fl = strlen(f);
            if (fl > 0 && (f[fl-1] == '"' || f[fl-1] == '>')) fl--;
            fprintf(g_codegen_out, "/* #extern %.*s */\n", (int)fl, f);
        }
        n = n->next;
    }
}

/* ---------- sub-module .c (no defines, no externs, no main) ---------- */

void codegen_c_emit_source(AstNode *prog) {
    AstNode *n = prog->left;
    int need_stdio = 0;
    while (n) { if (is_runtime(n->kind)) { need_stdio = 1; break; } n = n->next; }
    if (need_stdio) fprintf(g_codegen_out, "#include <stdio.h>\n\n");

    n = prog->left;
    while (n) {
        if (is_runtime(n->kind) || is_conditional(n->kind))
            emit_node(n, 0);
        n = n->next;
    }
}

/* ---------- main .c ---------- */

void codegen_c_emit(AstNode *prog) {
    /* check if we need stdio */
    int need_stdio = 0;
    AstNode *n = prog->left;
    while (n) { if (is_runtime(n->kind)) { need_stdio = 1; break; } n = n->next; }
    if (need_stdio) fprintf(g_codegen_out, "#include <stdio.h>\n\n");

    /* extern symbol definitions (only in main .c) */
    const char *en;
    defines_next_extern(NULL);
    int has_ext = 0;
    while (defines_next_extern(&en)) {
        fprintf(g_codegen_out, "const char *%s = \"%s\";\n", en, en);
        has_ext = 1;
    }
    if (has_ext) fprintf(g_codegen_out, "\n");

    /* defines + extern block aliases from main AST */
    n = prog->left;
    while (n) {
        if (n->kind == NODE_DEFINE || n->kind == NODE_EXTERN_C_BLOCK)
            emit_node(n, 0);
        n = n->next;
    }

    /* main() */
    if (need_stdio) {
        fprintf(g_codegen_out, "\nint main(void) {\n");
        n = prog->left;
        while (n) {
            if (is_runtime(n->kind) || is_conditional(n->kind))
                emit_node(n, 1);
            n = n->next;
        }
        fprintf(g_codegen_out, "    return 0;\n}\n");
    }
}
