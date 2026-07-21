/* lexer.c — GCL preprocessor-only lexer */
#include "../include/gcl.h"
#include "../include/token.h"
#include "lexer.h"

const char *tok_name(TokenType t) {
    switch (t) {
        case TOK_EOF: return "EOF";
        case TOK_TEXT: return "TEXT";
        case TOK_PP_INCLUDE: return "#include";
        case TOK_PP_LIB: return "#lib";
        case TOK_PP_EXTERN: return "#extern";
        case TOK_PP_DEFINE: return "#define";
        case TOK_PP_UNDEF: return "#undef";
        case TOK_PP_IFDEF: return "#ifdef";
        case TOK_PP_IFNDEF: return "#ifndef";
        case TOK_PP_IF: return "#if";
        case TOK_PP_ELIF: return "#elif";
        case TOK_PP_ELSE: return "#else";
        case TOK_PP_ENDIF: return "#endif";
        case TOK_PP_ERROR: return "#error";
        case TOK_PP_PRAGMA: return "#pragma";
        case TOK_PP_LINE: return "#line";
        default: return "?";
    }
}

static void add_tok(TokenCtx *c, TokenType type, const char *text, int len, int line) {
    if (c->count >= c->cap) {
        c->cap = c->cap ? c->cap * 2 : 512;
        c->tokens = realloc(c->tokens, c->cap * sizeof(Token));
    }
    char *copy = malloc(len + 1);
    memcpy(copy, text, len); copy[len] = '\0';
    c->tokens[c->count++] = (Token){type, copy, len, line};
}

static int pp_type(const char *s, int len) {
    if (len == 7 && memcmp(s, "include", 7) == 0) return TOK_PP_INCLUDE;
    if (len == 3 && memcmp(s, "lib", 3) == 0)     return TOK_PP_LIB;
    if (len == 6 && memcmp(s, "extern", 6) == 0)  return TOK_PP_EXTERN;
    if (len == 6 && memcmp(s, "define", 6) == 0)  return TOK_PP_DEFINE;
    if (len == 5 && memcmp(s, "undef", 5) == 0)   return TOK_PP_UNDEF;
    if (len == 5 && memcmp(s, "ifdef", 5) == 0)   return TOK_PP_IFDEF;
    if (len == 6 && memcmp(s, "ifndef", 6) == 0)  return TOK_PP_IFNDEF;
    if (len == 2 && memcmp(s, "if", 2) == 0)      return TOK_PP_IF;
    if (len == 4 && memcmp(s, "elif", 4) == 0)    return TOK_PP_ELIF;
    if (len == 4 && memcmp(s, "else", 4) == 0)    return TOK_PP_ELSE;
    if (len == 5 && memcmp(s, "endif", 5) == 0)   return TOK_PP_ENDIF;
    if (len == 5 && memcmp(s, "error", 5) == 0)   return TOK_PP_ERROR;
    if (len == 6 && memcmp(s, "pragma", 6) == 0)  return TOK_PP_PRAGMA;
    if (len == 4 && memcmp(s, "line", 4) == 0)    return TOK_PP_LINE;
    return -1;
}

TokenCtx lexer_run(const char *path) {
    TokenCtx ctx = {0};
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "lexer: cannot open '%s'\n", path); return ctx; }

    char line[8192];
    int lnum = 0;

    while (fgets(line, sizeof(line), f)) {
        lnum++;
        int len = (int)strlen(line);
        /* Trim trailing newline */
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len-1] == '\r') line[--len] = '\0';

        const char *s = line;
        while (*s == ' ' || *s == '\t') s++;

        if (*s == '\0') continue; /* empty line */

        /* Check for preprocessor directive */
        if (*s == '#') {
            s++; while (*s == ' ' || *s == '\t') s++;
            const char *ds = s; int dl = 0;
            while (*s >= 'a' && *s <= 'z') { s++; dl++; }
            int pt = pp_type(ds, dl);
            if (pt >= 0) {
                /* Rest of line after directive name */
                while (*s == ' ' || *s == '\t') s++;
                add_tok(&ctx, pt, s, (int)(line + len - s), lnum);
                continue;
            }
            /* Unknown # — skip (it's a comment per spec) */
            continue;
        }

        /* Regular text line */
        add_tok(&ctx, TOK_TEXT, s, (int)(line + len - s), lnum);
    }

    add_tok(&ctx, TOK_EOF, "", 0, lnum);
    fclose(f);
    return ctx;
}
