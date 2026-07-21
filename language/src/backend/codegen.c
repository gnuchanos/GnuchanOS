#include "codegen.h"
#include "defines.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static FILE *g_out = NULL;

static const char *resolve(const char *name) {
    const char *v = defines_get(name);
    return v ? v : name;
}

/* ---- debug / interactive print (used in .gcdebug) ---- */

static void print_val(AstNode *arg) {
    if (!arg) return;
    if (arg->kind == NODE_STRING) {
        size_t len = arg->len;
        const char *s = arg->value;
        if (len >= 2 && s[0] == '"') { s++; len -= 2; }
        fprintf(g_out, "%.*s", (int)len, s);
    } else if (arg->kind == NODE_IDENT) {
        const char *rv = resolve(arg->value);
        size_t l = strlen(rv);
        if (l >= 2 && rv[0] == '"') { rv++; l -= 2; }
        fprintf(g_out, "%.*s", (int)l, rv);
    } else if (arg->kind == NODE_NUMBER) {
        fprintf(g_out, "%.*s", (int)arg->len, arg->value);
    }
}

static void emit_debug(AstNode *n) {
    if (!n) return;
    AstNode *arg = n->left;
    while (arg) { print_val(arg); arg = arg->next; }
    fprintf(g_out, "\n");
}

/* ---- IR dump: dependency chain + resolved defines ---- */

static const char *node_kind_name(NodeKind k) {
    switch (k) {
        case NODE_LIB:     return "#lib";
        case NODE_INCLUDE: return "#include";
        case NODE_EXTERN:  return "#extern";
        default:           return "?";
    }
}

static void emit_ir(AstNode *prog) {
    fprintf(g_out, "; ── IR Dump ──\n");

    /* Phase 1: dependency chain — show #lib, #include, #extern in order */
    fprintf(g_out, "; Dependency chain:\n");
    AstNode *n = prog->left;
    int dep_count = 0;
    while (n) {
        if (n->kind == NODE_LIB || n->kind == NODE_INCLUDE || n->kind == NODE_EXTERN) {
            const char *fname = n->left ? n->left->value : "?";
            /* strip quotes/brackets for display */
            const char *f = fname;
            if (f[0] == '"' || f[0] == '<') f++;
            size_t flen = strlen(f);
            if (flen > 0 && (f[flen - 1] == '"' || f[flen - 1] == '>')) flen--;
            fprintf(g_out, "  %-10s %.*s\n", node_kind_name(n->kind), (int)flen, f);
            dep_count++;
        }
        n = n->next;
    }
    if (dep_count == 0) fprintf(g_out, "  (none)\n");

    /* Phase 2: resolved defines table */
    fprintf(g_out, "\n; Resolved defines:\n");
    n = prog->left;
    int def_count = 0;
    while (n) {
        if (n->kind == NODE_DEFINE) {
            const char *val = n->left ? n->left->value : "";
            fprintf(g_out, "  %-20s = %s\n", n->value, val);
            def_count++;
        }
        n = n->next;
    }
    if (def_count == 0) fprintf(g_out, "  (none)\n");

    /* Phase 3: extern symbols */
    fprintf(g_out, "\n; Extern symbols:\n");
    const char *en;
    defines_next_extern(NULL);
    int ext_count = 0;
    while (defines_next_extern(&en)) {
        fprintf(g_out, "  %s\n", en);
        ext_count++;
    }
    if (ext_count == 0) fprintf(g_out, "  (none)\n");

    /* Phase 4: #debug statements (full stack for codegen) */
    fprintf(g_out, "\n; #debug statements:\n");
    n = prog->left;
    int dbg_count = 0;
    while (n) {
        if (n->kind == NODE_DEBUG) {
            fprintf(g_out, "  #debug ");
            /* save/restore g_out because print_val writes to it directly */
            FILE *saved = g_out;
            g_out = stdout;
            /* ensure g_out points to the same stream */
            g_out = saved;
            AstNode *arg = n->left;
            while (arg) {
                fprintf(g_out, " ");
                if (arg->kind == NODE_STRING) {
                    size_t len = arg->len;
                    const char *s = arg->value;
                    if (len >= 2 && s[0] == '"') { s++; len -= 2; }
                    fprintf(g_out, "\"%.*s\"", (int)len, s);
                } else if (arg->kind == NODE_IDENT) {
                    /* check if namespace.member: show as is, or resolved */
                    const char *rv = resolve(arg->value);
                    if (strcmp(rv, arg->value) != 0) {
                        /* resolved — show the resolved value */
                        size_t l = strlen(rv);
                        if (l >= 2 && rv[0] == '"' && rv[l-1] == '"') {
                            fprintf(g_out, "\"%.*s\"", (int)(l-2), rv+1);
                        } else {
                            fprintf(g_out, "%s", rv);
                        }
                    } else {
                        fprintf(g_out, "%s", arg->value);
                    }
                } else if (arg->kind == NODE_NUMBER) {
                    fprintf(g_out, "%.*s", (int)arg->len, arg->value);
                }
                arg = arg->next;
            }
            fprintf(g_out, "\n");
            dbg_count++;
        }
        n = n->next;
    }
    if (dbg_count == 0) fprintf(g_out, "  (none)\n");
    fprintf(g_out, ";\n");
}

/* ---- AST dump ---- */

static void emit_ast(AstNode *n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) fprintf(g_out, "  ");
    switch (n->kind) {
        case NODE_PROGRAM:       fprintf(g_out, "PROGRAM\n"); break;
        case NODE_INCLUDE:       fprintf(g_out, "INCLUDE: %s\n", n->left ? n->left->value : "?"); break;
        case NODE_LIB:           fprintf(g_out, "LIB: %s\n", n->left ? n->left->value : "?"); break;
        case NODE_EXTERN:        fprintf(g_out, "EXTERN: %s\n", n->left ? n->left->value : "?"); break;
        case NODE_DEFINE:        fprintf(g_out, "DEFINE: %s = %s\n", n->value, n->left ? n->left->value : "?"); break;
        case NODE_DEBUG:         fprintf(g_out, "DEBUG:\n"); break;
        case NODE_EXTERN_C_BLOCK:fprintf(g_out, "EXTERN \"c\" BLOCK:\n"); break;
        case NODE_IDENT:         fprintf(g_out, "IDENT: %s\n", n->value); break;
        case NODE_STRING:        fprintf(g_out, "STRING: %s\n", n->value); break;
        case NODE_NUMBER:        fprintf(g_out, "NUMBER: %s\n", n->value); break;
        default:                 fprintf(g_out, "?\n"); break;
    }
    if (n->left)  emit_ast(n->left, depth + 1);
    if (n->right) emit_ast(n->right, depth + 1);
    if (n->next)  emit_ast(n->next, depth);
}

/* ---- codegen: translate to compilable C ---- */

static int is_number_str(const char *s) {
    if (!s || !*s) return 0;
    if (*s == '-' || *s == '+') s++;
    int has_dot = 0;
    while (*s) {
        if (*s == '.') { if (has_dot) return 0; has_dot = 1; }
        else if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static void emit_c_debug(AstNode *n) {
    /* collect format string and args */
    char fmt[4096] = "";
    char args[4096] = "";
    AstNode *arg = n->left;
    int arg_count = 0;

    while (arg) {
        if (arg->kind == NODE_STRING) {
            size_t len = arg->len;
            const char *s = arg->value;
            if (len >= 2 && s[0] == '"') { s++; len -= 2; }
            /* escape any % or \ for C string literal */
            for (size_t i = 0; i < len; i++) {
                if (s[i] == '%') { strcat(fmt, "%%"); }
                else if (s[i] == '\\') { strcat(fmt, "\\\\"); }
                else if (s[i] == '"') { strcat(fmt, "\\\""); }
                else { int fl = (int)strlen(fmt); fmt[fl] = s[i]; fmt[fl+1] = '\0'; }
            }
        } else if (arg->kind == NODE_IDENT) {
            const char *name = arg->value;
            const char *rv = resolve(name);
            if (strcmp(rv, name) != 0) {
                /* resolved: use the value as a literal */
                if (is_number_str(rv)) {
                    strcat(fmt, "%s");
                    if (arg_count++) strcat(args, ", ");
                    strcat(args, "\"");
                    strcat(args, rv);
                    strcat(args, "\"");
                } else {
                    /* string literal or extern name: use %s + the resolved name */
                    strcat(fmt, "%s");
                    if (arg_count++) strcat(args, ", ");
                    /* if resolved value is the extern name (no quotes), use as variable */
                    size_t rl = strlen(rv);
                    if (rl >= 2 && rv[0] == '"' && rv[rl-1] == '"') {
                        /* string literal from define */
                        char tmp[1024];
                        snprintf(tmp, sizeof(tmp), "\"%.*s\"", (int)(rl-2), rv+1);
                        strcat(args, tmp);
                    } else {
                        strcat(args, rv);
                    }
                }
            } else {
                /* unresolved: %s + raw name */
                strcat(fmt, "%s");
                if (arg_count++) strcat(args, ", ");
                strcat(args, name);
            }
        } else if (arg->kind == NODE_NUMBER) {
            int has_dot = 0;
            for (size_t i = 0; i < arg->len; i++)
                if (arg->value[i] == '.') { has_dot = 1; break; }
            strcat(fmt, has_dot ? "%s" : "%s");
            if (arg_count++) strcat(args, ", ");
            strcat(args, "\"");
            for (size_t i = 0; i < arg->len; i++) {
                int al = (int)strlen(args);
                args[al] = arg->value[i]; args[al+1] = '\0';
            }
            strcat(args, "\"");
        }
        arg = arg->next;
    }

    if (arg_count > 0) {
        fprintf(g_out, "    printf(\"%s\\n\", %s);\n", fmt, args);
    } else {
        fprintf(g_out, "    printf(\"%s\\n\");\n", fmt);
    }
}

static void emit_codegen_c(AstNode *prog) {
    fprintf(g_out, "#include <stdio.h>\n\n");

    /* emit fallback definitions for extern symbols (real DLL can override) */
    const char *en;
    defines_next_extern(NULL); /* reset iterator */
    while (defines_next_extern(&en)) {
        fprintf(g_out, "const char *%s = \"%s\";\n", en, en);
    }

    /* emit C #define for all gcl defines (non-extern) */
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_DEFINE) {
            const char *val = n->left ? n->left->value : "";
            if (val[0] == '"') {
                fprintf(g_out, "#define %s %s\n", n->value, val);
            } else if (is_number_str(val)) {
                fprintf(g_out, "#define %s %s\n", n->value, val);
            } else {
                fprintf(g_out, "#define %s \"%s\"\n", n->value, val);
            }
        }
        n = n->next;
    }

    fprintf(g_out, "\nint main(void) {\n");

    n = prog->left;
    while (n) {
        if (n->kind == NODE_DEBUG) emit_c_debug(n);
        n = n->next;
    }

    fprintf(g_out, "    return 0;\n}\n");
}

/* ---- public entry ---- */

void codegen_emit(AstNode *prog, CodegenOpts *opts) {
    g_out = opts->output ? opts->output : stdout;

    switch (opts->mode) {
        case MODE_LEXER:
            break;
        case MODE_PARSER:
        case MODE_AST:
            emit_ast(prog, 0);
            break;
        case MODE_IR:
            emit_ir(prog);
            break;
        case MODE_CODEGEN:
            emit_codegen_c(prog);
            break;
        case MODE_EXEC:
        default:
            {
                AstNode *n = prog->left;
                while (n) {
                    if (n->kind == NODE_DEBUG) emit_debug(n);
                    n = n->next;
                }
            }
            break;
    }
}
