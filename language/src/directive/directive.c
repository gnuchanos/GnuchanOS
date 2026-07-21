/* directive.c — GCL preprocessor handler */
#include "../include/gcl.h"
#include "../include/token.h"
#include "../lexer/lexer.h"
#include "directive.h"

#define MAX_MACROS 256
#define MAX_INCLUDE_DEPTH 32

typedef struct { char name[256]; char val[1024]; } Macro;
static Macro macros[MAX_MACROS];
static int macro_n = 0;

static void mac_set(const char *n, const char *v) {
    for (int i = 0; i < macro_n; i++)
        if (strcmp(macros[i].name, n) == 0) { strncpy(macros[i].val, v, 1023); return; }
    if (macro_n < MAX_MACROS) {
        strncpy(macros[macro_n].name, n, 255);
        strncpy(macros[macro_n].val, v, 1023);
        macro_n++;
    }
}
static const char *mac_get(const char *n) {
    for (int i = 0; i < macro_n; i++)
        if (strcmp(macros[i].name, n) == 0) return macros[i].val;
    return NULL;
}
static void mac_del(const char *n) {
    for (int i = 0; i < macro_n; i++)
        if (strcmp(macros[i].name, n) == 0) { macros[i] = macros[--macro_n]; return; }
}

/* Expand macros in a line, write to out */
static void expand_line(const char *s, int len, FILE *out) {
    char buf[8192]; int bi = 0;
    for (int i = 0; i < len && bi < 8190; i++) {
        int matched = 0;
        for (int m = 0; m < macro_n; m++) {
            int ml = (int)strlen(macros[m].name);
            if (i + ml <= len && strncmp(s + i, macros[m].name, ml) == 0) {
                char next = (i + ml < len) ? s[i + ml] : '\0';
                if (next == '\0' || next == ' ' || next == '\t' || next == '\n' ||
                    next == ';' || next == ',' || next == ')' || next == '(' ||
                    next == '[' || next == ']' || next == '+' || next == '-' ||
                    next == '*' || next == '/' || next == '%' || next == '=' ||
                    next == '<' || next == '>' || next == '!' || next == '.' ||
                    next == '&' || next == '|' || next == '^') {
                    int vl = (int)strlen(macros[m].val);
                    if (bi + vl < 8190) { memcpy(buf + bi, macros[m].val, vl); bi += vl; }
                    i += ml - 1; matched = 1; break;
                }
            }
        }
        if (!matched) buf[bi++] = s[i];
    }
    buf[bi] = '\0';
    fputs(buf, out);
}

/* Forward decl for recursive #include */
static int process_ctx(TokenCtx *ctx, FILE *out, int *pos, int depth);

static int process_ctx(TokenCtx *ctx, FILE *out, int *pos, int depth) {
    if (depth > MAX_INCLUDE_DEPTH) { fprintf(stderr, "gcl: max include depth\n"); return -1; }

    while (*pos < ctx->count) {
        Token t = ctx->tokens[(*pos)++];

        switch (t.type) {
        case TOK_EOF: return 0;

        case TOK_TEXT:
            expand_line(t.text, t.len, out);
            fputc('\n', out);
            break;

        case TOK_PP_INCLUDE: {
            /* Parse path from token text: <path> or "path" */
            const char *p = t.text; while (*p == ' ' || *p == '\t') p++;
            char fname[512]; int fi = 0;
            if (*p == '<') { p++; while (*p && *p != '>' && fi < 510) fname[fi++] = *p++; }
            else if (*p == '"') { p++; while (*p && *p != '"' && fi < 510) fname[fi++] = *p++; }
            else { while (*p && *p != ' ' && *p != '\t' && *p != '\n' && fi < 510) fname[fi++] = *p++; }
            fname[fi] = '\0';
            /* Recursive include */
            TokenCtx inc = lexer_run(fname);
            if (inc.tokens) { int ip = 0; process_ctx(&inc, out, &ip, depth + 1); free(inc.tokens); }
            break;
        }

        case TOK_PP_LIB:
            fprintf(out, "/* #lib %.*s */\n", t.len, t.text);
            break;

        case TOK_PP_EXTERN:
            fprintf(out, "/* #extern %.*s */\n", t.len, t.text);
            break;

        case TOK_PP_DEFINE: {
            const char *p = t.text; while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';
            while (*p == ' ' || *p == '\t') p++;
            char val[1024]; int vi = 0;
            while (*p && *p != '\n' && vi < 1023) val[vi++] = *p++;
            val[vi] = '\0';
            while (vi > 0 && (val[vi-1] == ' ' || val[vi-1] == '\t')) val[--vi] = '\0';
            mac_set(name, val);
            fprintf(out, "#define %s %s\n", name, val);
            break;
        }

        case TOK_PP_UNDEF: {
            const char *p = t.text; while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\n' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';
            mac_del(name);
            fprintf(out, "#undef %s\n", name);
            break;
        }

        case TOK_PP_IFDEF:
        case TOK_PP_IFNDEF: {
            int is_ndef = (t.type == TOK_PP_IFNDEF);
            const char *p = t.text; while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '\n' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';
            int defined = (mac_get(name) != NULL);
            int skip = is_ndef ? defined : !defined;

            int nest = 1;
            while (nest > 0 && *pos < ctx->count) {
                Token nt = ctx->tokens[(*pos)++];
                if (nt.type == TOK_PP_IFDEF || nt.type == TOK_PP_IFNDEF || nt.type == TOK_PP_IF) nest++;
                else if (nt.type == TOK_PP_ENDIF) { nest--; if (nest == 0) break; }
                else if (nt.type == TOK_PP_ELSE && nest == 1) { skip = !skip; continue; }
                if (nest == 1 && !skip) {
                    if (nt.type == TOK_TEXT) { expand_line(nt.text, nt.len, out); fputc('\n', out); }
                    else if (nt.type == TOK_PP_DEFINE) {
                        /* process nested define */
                        const char *dp = nt.text; while (*dp == ' ' || *dp == '\t') dp++;
                        char dn[256]; int dni = 0;
                        while (*dp && *dp != ' ' && *dp != '\t' && *dp != '\n' && dni < 255) dn[dni++] = *dp++;
                        dn[dni] = '\0';
                        while (*dp == ' ' || *dp == '\t') dp++;
                        mac_set(dn, dp);
                        fprintf(out, "#define %s %s\n", dn, dp);
                    }
                }
            }
            break;
        }

        case TOK_PP_IF: {
            const char *p = t.text; while (*p == ' ' || *p == '\t') p++;
            char name[256]; int ni = 0;
            while (*p && *p != ' ' && *p != '=' && *p != '!' && *p != '\n' && ni < 255) name[ni++] = *p++;
            name[ni] = '\0';
            const char *val = mac_get(name);
            int truthy = (val && val[0] != '\0' && strcmp(val, "0") != 0);

            int nest = 1; int skip = !truthy;
            while (nest > 0 && *pos < ctx->count) {
                Token nt = ctx->tokens[(*pos)++];
                if (nt.type == TOK_PP_IFDEF || nt.type == TOK_PP_IFNDEF || nt.type == TOK_PP_IF) nest++;
                else if (nt.type == TOK_PP_ENDIF) { nest--; if (nest == 0) break; }
                else if ((nt.type == TOK_PP_ELIF || nt.type == TOK_PP_ELSE) && nest == 1) {
                    if (nt.type == TOK_PP_ELSE) skip = false;
                    else skip = true;
                    continue;
                }
                if (nest == 1 && !skip) {
                    if (nt.type == TOK_TEXT) { expand_line(nt.text, nt.len, out); fputc('\n', out); }
                    else if (nt.type == TOK_PP_DEFINE) {
                        const char *dp = nt.text; while (*dp == ' ' || *dp == '\t') dp++;
                        char dn[256]; int dni = 0;
                        while (*dp && *dp != ' ' && *dp != '\t' && *dp != '\n' && dni < 255) dn[dni++] = *dp++;
                        dn[dni] = '\0';
                        while (*dp == ' ' || *dp == '\t') dp++;
                        mac_set(dn, dp);
                        fprintf(out, "#define %s %s\n", dn, dp);
                    }
                }
            }
            break;
        }

        case TOK_PP_ERROR:
            fprintf(stderr, "gcl:%d: #error %.*s\n", t.line, t.len, t.text);
            return -1;

        case TOK_PP_PRAGMA: {
            const char *p = t.text;
            if (strncmp(p, "message", 7) == 0) {
                p += 7; while (*p == ' ' || *p == '\t') p++;
                if (*p == '(') p++;
                if (*p == '"') p++;
                char msg[512]; int mi = 0;
                while (*p && *p != '"' && *p != ')' && *p != '\n' && mi < 510) msg[mi++] = *p++;
                msg[mi] = '\0';
                fprintf(stderr, "note: %s\n", msg);
            }
            break;
        }

        case TOK_PP_LINE:
            fprintf(out, "#line %.*s\n", t.len, t.text);
            break;

        case TOK_PP_ELIF:
        case TOK_PP_ELSE:
        case TOK_PP_ENDIF:
            /* standalone — ignore */
            break;

        default: break;
        }
    }
    return 0;
}

int directive_process(TokenCtx *ctx, FILE *out) {
    int pos = 0;
    fprintf(out, "/* Generated by GCL v0.1 */\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");
    return process_ctx(ctx, out, &pos, 0);
}
