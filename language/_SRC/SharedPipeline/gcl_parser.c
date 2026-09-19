/*
 * gcl_parser.c — GCL AST parser.
 *
 * Supported syntax (simple_doc.md):
 *   type variable = expr;  printf("{} {}", a, b);  if/else, while, for, switch,
 *   struct/enum/typedef, function decl, return, break, continue,
 *   member access (obj.field), call (fn(args)), binary ops.
 */

/* strdup() is POSIX.1-2008, not C99 — see the note in gcl_lexer.c. Without an
 * explicit feature-test macro the implicit declaration truncates the returned
 * pointer on LP64 and every duplicated name/type is silently corrupted. Asking
 * for POSIX here makes the unit self-sufficient; the guard yields to a
 * command-line -D_POSIX_C_SOURCE=200809L. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "gcl_parser.h"
#include "gcl_lexer.h"
#include "gcl_diag.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    GclToken *toks;
    int count;
    int pos;
    int err;
    char *msg;            /* legacy flat message (the FIRST error) */
    GclDiagList *diags;   /* rich sink; may be NULL */
} Parser;

static GclToken *peek(Parser *p) { return &p->toks[p->pos]; }
static GclToken *advance(Parser *p) {
    GclToken *t = &p->toks[p->pos];
    if (t->type != TOK_EOF) p->pos++;
    return t;
}
static int check(Parser *p, GclTokenType t) { return peek(p)->type == t; }
static int match(Parser *p, GclTokenType t) {
    if (check(p, t)) { advance(p); return 1; }
    return 0;
}

/* ---------- diagnostics ---------- */

static GclSpan span_of(const GclToken *t) {
    GclSpan s;
    s.line = t ? t->line : 0;
    s.col  = t ? t->col : 0;
    s.len  = t ? (int)t->len : 0;
    return s;
}

static GclSpan span_make(int line, int col, int len) {
    GclSpan s;
    s.line = line;
    s.col = col;
    s.len = len;
    return s;
}

/* Report the FIRST syntax error. `p->err` stays sticky on purpose: the whole
   parser unwinds on it (`if (p->err) break;`), so recording more than one would
   produce a cascade of guesses instead of one accurate statement. */
static GclDiag *parse_error(Parser *p, GclDiagCode code, GclSpan span,
                           const char *fmt, ...) {
    if (p->err) return NULL;
    p->err = 1;

    char stack[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);

    /* The legacy flat message, in the historical shape. */
    if (!p->msg) {
        char leg[640];
        snprintf(leg, sizeof(leg), "%s at %d:%d", stack, span.line,
                 span.col > 0 ? span.col : 1);
        p->msg = strdup(leg);
    }
    if (!p->diags) return NULL;
    return gcl_diag_add(p->diags, code, GCL_SEV_FATAL, span, "%s", stack);
}

/* Shim for the sites that only have a sentence to offer. */
static void set_err(Parser *p, const char *m) {
    parse_error(p, GCL_E_PARSE_UNEXPECTED_TOKEN, span_of(peek(p)), "%s", m);
}

/* Find bracket tokens that were opened but never closed, scanning the tokens
   before `pos`. `out` receives their indices, most recent first. This is what
   lets "expected '}'" point back at the brace that is STILL OPEN instead of
   dropping the user at the end of the file with no explanation. */
static int unclosed_openers(Parser *p, GclTokenType open, GclTokenType close,
                            int pos, int *out, int max) {
    int stack[64];
    int n = 0;
    for (int i = 0; i < pos && i < p->count; i++) {
        if (p->toks[i].type == open) {
            if (n < 64) stack[n++] = i;
        } else if (p->toks[i].type == close) {
            if (n > 0) n--;
        }
    }
    int found = 0;
    for (int k = 0; k < n && found < max; k++) out[found++] = stack[n - 1 - k];
    return found;
}

/* Human name of the thing being called: `printf`, `Raylib.DrawText`. */
static void callee_name(const GclExpr *e, char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    if (!e) { snprintf(buf, cap, "the call"); return; }
    if (e->kind == AST_EXPR_VAR && e->name) {
        snprintf(buf, cap, "%s", e->name);
        return;
    }
    if (e->kind == AST_EXPR_MEMBER && e->member_name) {
        char base[160];
        callee_name(e->left, base, sizeof(base));
        if (base[0] && strcmp(base, "the call") != 0)
            snprintf(buf, cap, "%s.%s", base, e->member_name);
        else
            snprintf(buf, cap, "%s", e->member_name);
        return;
    }
    snprintf(buf, cap, "the call");
}

/* Report a missing closer, pointing back at the opener that is STILL OPEN.
   `unclosed_openers` (above) was written for exactly this and then never
   called, so a block left open reported a bare "expected '}'" anchored at EOF
   and dropped the reader at the end of the file with no explanation - the
   note beside the helper described a behaviour the parser did not have.
   The diagnostic's SPAN is the OPENER's, so every consumer that reads the span
   (and the legacy "at L:C" line) points at the brace the user forgot.

   `code` is the IDENTITY of the failure and the caller picks the specific one
   from the shared vocabulary: gcl_diag.h reserves 2006 for "a block is never
   closed" and 2018 for "a bracket is never closed", and both were unused while
   every missing closer was reported as the generic 2019 "unexpected token".
   The code is only reported when an opener was actually FOUND; with nothing
   left open the sentence is a plain expectation and keeps the old identity. */
static void err_expect_close(Parser *p, const char *what, GclDiagCode code,
                             GclTokenType open, GclTokenType close) {
    int idx[8];
    int n = unclosed_openers(p, open, close, p->pos, idx, 8);
    if (n > 0) {
        GclToken *o = &p->toks[idx[0]];
        parse_error(p, code, span_make(o->line, o->col, (int)o->len),
                    "%s (the one opened at %d:%d is never closed)", what, o->line, o->col);
        return;
    }
    set_err(p, what);
}

/* ---------- AST source positions ---------- */

/* Stamp an expression with the token it started at. The runtime reads these so
   failures such as "name 'scor' is not defined" can point at a line. */
static void stamp_expr(GclExpr *e, const GclToken *t) {
    if (!e || !t) return;
    e->line = t->line;
    e->col = t->col;
    e->len = (int)t->len;
}

static void stamp_expr_from(GclExpr *dst, const GclExpr *src) {
    if (!dst || !src) return;
    dst->line = src->line;
    dst->col = src->col;
    dst->len = src->len;
}

static void stamp_stmt(GclStmt *s, const GclToken *t) {
    if (!s || !t) return;
    s->line = t->line;
    s->col = t->col;
    s->len = (int)t->len;
}
static char *tok_str(GclToken *t) {
    if (!t) return NULL;
    /* Remove the opening/closing quote characters from a string token */
    size_t start = 0;
    size_t end = t->len;
    if (t->len >= 2 && (t->lexeme[0] == '"' || t->lexeme[0] == '\'') &&
        t->lexeme[t->len - 1] == t->lexeme[0]) {
        start = 1;
        end = t->len - 1;
    }
    size_t n = end - start;
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, t->lexeme + start, n);
    s[n] = '\0';
    return s;
}

static GclExpr *new_expr(GclExprKind k) {
    GclExpr *e = (GclExpr *)calloc(1, sizeof(GclExpr));
    if (e) e->kind = k;
    return e;
}

/* ---------- Expression (Pratt / precedence climbing) ---------- */

static GclExpr *parse_expr(Parser *p, int min_prec);
/* The join for a multi-word builtin type name ("long long", "unsigned int").
   It is defined with the statement helpers far below, but parse_primary needs
   it: a multi-word type is legal in EXPRESSION position as a `sizeof`
   argument, and the two places must agree on ONE spelling. */
static char *parse_type_name(Parser *p);
/* B10: prefix ++/-- operandının bağımsız kopyası için gerekli (aşağıda tanımlı). */
static GclExpr *clone_expr(const GclExpr *e);

/* ---------- Qualified type names (Module.Type) ----------

   Native module types carry the module as a qualifier and GCL accepts both
   spellings: bare (`Rectangle r;`) and qualified (`Raylib.Rectangle r;`).
   The lexer emits `Raylib` `.` `Rectangle` as three separate tokens, so a
   dotted type name never arrives as one token and the parser has to stitch the
   segments back together.

   qualified_type_span() is pure lookahead (the parser position is untouched):
   it matches IDENT ('.' IDENT)+ and reports the token index just past the last
   segment. Requiring at least one dot is what keeps the existing single-token
   paths authoritative - a plain `Point p;` still takes the IDENT IDENT route. */
static int qualified_type_span(Parser *p, int *end_out) {
    int i = p->pos;
    if (i >= p->count || p->toks[i].type != TOK_IDENT) return 0;   /* module names are plain idents */
    i++;
    int dots = 0;
    while (i + 1 < p->count && p->toks[i].type == TOK_DOT) {
        GclTokenType seg = p->toks[i + 1].type;
        if (seg != TOK_IDENT && seg != TOK_TYPE_NAME && seg != TOK_KEYWORD) break;
        i += 2;
        dots++;
    }
    if (!dots) return 0;
    if (end_out) *end_out = i;
    return 1;
}

/* Consume a qualified type name and return it as "Raylib.Rectangle" (heap). */
static char *parse_qualified_type_name(Parser *p) {
    int end = p->pos;
    if (!qualified_type_span(p, &end)) return NULL;
    char buf[256] = "";
    for (int i = p->pos; i < end; i++) {
        if (p->toks[i].type == TOK_DOT) {
            strncat(buf, ".", sizeof(buf) - strlen(buf) - 1);
            continue;
        }
        char *part = tok_str(&p->toks[i]);
        if (!part) continue;
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
        free(part);
    }
    p->pos = end;
    return strdup(buf);
}

/* Does a declarator follow a qualified type name?
   `Raylib.Rectangle Player = ...` (variable), `Raylib.Rectangle make() { ... }`
   (function), `Raylib.Rectangle rects[4];` (array). A member access such as
   `Player.rec = ...` has `=` directly after the dotted chain and is rejected,
   so a dotted chain is only consumed when it really introduces a declaration. */
static int qualified_decl_follows(Parser *p) {
    int end = 0;
    if (!qualified_type_span(p, &end)) return 0;
    if (end >= p->count || p->toks[end].type != TOK_IDENT) return 0;
    if (end + 1 >= p->count) return 0;
    switch (p->toks[end + 1].type) {
        case TOK_ASSIGN:
        case TOK_SEMI:
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_COMMA:
            return 1;
        default:
            return 0;
    }
}

/* B29 — 'A' karakter literalinin sayısal kodunu çöz. Lexeme tek tırnaklıdır:
   'A' (3 bayt) veya kaçışlı '\n' (4 bayt). Lexer yalnızca tek karakterli
   literalleri geçirir; burada kaçış dizisi karakter koduna çevrilir. */
static int char_literal_code(const char *lex) {
    if (!lex || lex[0] != '\'') return 0;
    if (lex[1] == '\\' && lex[2]) {
        switch (lex[2]) {
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case '0': return 0;
            case 'a': return '\a';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'v': return '\v';
            case '?': return '?';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            default: return (unsigned char)lex[2];
        }
    }
    return (unsigned char)lex[1];
}

/* B11 — `(unsigned int)x` gibi çok sözcüklü yerleşik tip adlarını tek bir
   tip dizesine indirger. `first`..`last` token dizisinde bitişiktir. */
static char *cast_type_name(GclToken *first, GclToken *last) {
    char buf[64];
    size_t used = 0;
    for (GclToken *t = first; ; t++) {
        if (used > 0 && used + 1 < sizeof(buf)) buf[used++] = ' ';
        size_t room = sizeof(buf) - used - 1;
        size_t n = t->len < room ? t->len : room;
        memcpy(buf + used, t->lexeme, n);
        used += n;
        if (t == last) break;
    }
    buf[used] = '\0';
    return strdup(buf);
}

static GclExpr *parse_primary(Parser *p) {
    GclToken *t = peek(p);
    /* B10 — prefix ++i / --i. Eskiden YALNIZCA postfix tanınıyordu; `int j = ++i;`
       veya `while (++m < 3)` "expected expression" hatası veriyordu. Prefix
       İFADENİN DEĞERİ yeni değerdir (C), bu yüzden atama düğümü döndürülür. */
    if (t->type == TOK_PLUSPLUS || t->type == TOK_MINUSMINUS) {
        GclTokenType op = t->type;
        advance(p);
        GclExpr *target = parse_primary(p);
        if (!target || p->err) return target;
        /* B9: prefix'in DEĞERİ yeni değerdir → AST_EXPR_PREINC (runtime
           lvalue'yu yazar ve yeni değeri döndürür). */
        GclExpr *e = new_expr(AST_EXPR_PREINC);
        if (e) {
            e->left = target;
            e->op = (op == TOK_PLUSPLUS) ? OP_ADD : OP_SUB;
        }
        /* `++i++` gibi zincirleri engellemek için burada durulur */
        return e;
    }
    if (t->type == TOK_NUMBER) {
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_FLOAT);
        if (e) { e->num = t->num; stamp_expr(e, t); }
        /* keep original token lexeme for large integer printing */
        if (e) e->str = tok_str(t);
        if (t->len >= 2 && t->lexeme[0] == '0' && (t->lexeme[1] == 'x' || t->lexeme[1] == 'X')) {
            e->num = (double)strtoll(t->lexeme, NULL, 16);
        }
        return e;
    }
    if (t->type == TOK_STRING) {
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_STRING);
        if (e) { e->str = tok_str(t); stamp_expr(e, t); }
        return e;
    }
    /* B29: 'A' bir SAYIdir (65) — C'de de `'A'`in tipi int'tir. Eskiden
       TOK_STRING ile ayni yola dusuyordu ve aritmetikte 0, `c == 'A'`
       karsilastirmasinda yanlis sonuc veriyordu.
       TUR 22 KARARI (bkz. language/tests/gcsf/stdio/char_rendering.gcsf):
       `'A'` bir int SABITIDIR, bir `char` DEGISKENI degildir. printf'te
       sayisal (65) basilir; `char c = 'A'` ise karakter ('A') basar. Bu
       ayrim C'nin tip sisteminin ta kendisidir, tutarsizlik degil. */
    if (t->type == TOK_CHAR) {
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_FLOAT);
        if (e) { e->num = (double)char_literal_code(t->lexeme); stamp_expr(e, t); }
        return e;
    }
    /* Unary operator alias check: NOT, BIT_XOR (~) */
    if (t->type == TOK_IDENT) {
        char *alias_name = tok_str(t);
        if (alias_name && (strcmp(alias_name, "NOT") == 0 || strcmp(alias_name, "BIT_XOR") == 0)) {
            advance(p);
            /* 9 is one above the highest binary precedence (8), so the operand
               stays a primary: `NOT a AND b` is (NOT a) AND b. */
            GclExpr *right = parse_expr(p, 9);
            GclExpr *e = new_expr(AST_EXPR_UNOP);
            if (e) {
                e->left = NULL;
                e->right = right;
                e->op = OP_BITNOT;  /* ~ — bitwise NOT or BIT_XOR */
            }
            free(alias_name);
            return e;
        }
        free(alias_name);
    }
    if (t->type == TOK_IDENT || t->type == TOK_KEYWORD || t->type == TOK_TYPE_NAME) {
        if (t->type == TOK_KEYWORD && t->len == 4 && strncmp(t->lexeme, "true", 4) == 0) { advance(p); GclExpr *e = new_expr(AST_EXPR_FLOAT); if (e) e->num = 1; return e; }
        if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "false", 5) == 0) { advance(p); GclExpr *e = new_expr(AST_EXPR_FLOAT); if (e) e->num = 0; return e; }
        /* A multi-word builtin type name in EXPRESSION position. The only place
           it is legal is a `sizeof` argument — `sizeof(long long)`,
           `sizeof(unsigned int)`, `sizeof(long double)` — but the lexer emits
           one token per word, so consuming a single token left the rest of the
           type unread and the call reported "expected ')'". The runtime has
           ALWAYS understood the joined spelling: decl_type_size() lists
           "long long", "long int", "unsigned int", "long double", ... It was
           only the parser that could not spell it, which forced `sizeof(int64)`
           to be the one way to ask for a 64-bit width. Joining here reuses
           parse_type_name() so a declaration and a sizeof argument cannot
           disagree about how the type is spelled. */
        char *name;
        if (t->type == TOK_TYPE_NAME && p->pos + 1 < p->count &&
            p->toks[p->pos + 1].type == TOK_TYPE_NAME) {
            name = parse_type_name(p);
        } else {
            name = tok_str(t);
            advance(p);
        }
        GclExpr *e = new_expr(AST_EXPR_VAR);
        if (e) e->name = name;
        /* chained member/array access: support x.y, x[i], and combinations like x[i].y */
        for (;;) {
            if (match(p, TOK_DOT)) {
        GclToken *mt = peek(p);
                if (mt->type != TOK_IDENT && mt->type != TOK_KEYWORD) {
                    GclToken *dot = &p->toks[p->pos - 1];
                    char b[512];
                    snprintf(b, sizeof(b),
                             "expected member name after '.' at %d:%d (got '%.*s' — write e.g. Module.member)",
                             dot->line, dot->col, (int)mt->len, mt->lexeme);
                    if (!p->err) { p->msg = strdup(b); p->err = 1; }
                    free(name);
                    return e;
                }
                advance(p);
                GclExpr *m = new_expr(AST_EXPR_MEMBER);
                if (m) {
                    m->left = e;
                    m->member_name = tok_str(mt);
                }
                e = m;
                continue;
            }
            if (match(p, TOK_LBRACKET)) {
                GclExpr *idx = parse_expr(p, 0);
                match(p, TOK_RBRACKET);
                if (p->err) break;
                GclExpr *arr = new_expr(AST_EXPR_ARRAY);
                if (arr) {
                    arr->left = e;
                    arr->right = idx;
                }
                e = arr;
                continue;
            }
            break;
        }
        /* call x(...) */
        if (match(p, TOK_LPAREN)) {
            GclExpr *call = new_expr(AST_EXPR_CALL);
            if (call) call->left = e;
            int cap = 0;
            if (!check(p, TOK_RPAREN)) {
                for (;;) {
                    GclExpr *arg = parse_expr(p, 0);
                    if (p->err) break;
                    if (call->arg_count >= cap) {
                        cap = cap ? cap * 2 : 4;
                        call->args = (GclExpr **)realloc(call->args, (size_t)cap * sizeof(GclExpr *));
                    }
                    call->args[call->arg_count++] = arg;
                    if (!match(p, TOK_COMMA)) break;
                }
            }
            if (!match(p, TOK_RPAREN)) {
                /* Name the callee - "expected ')' in call to 'printf'" says
                   WHICH call is unclosed, which a bare "expected ')'" cannot.
                   callee_name() walks the callee expression (call->left — see
                   the TRAP note in gcl_parser.h), so a qualified call reports
                   the prefix the user typed: "Stdio.printf". */
                char cn[160];
                callee_name(call ? call->left : NULL, cn, sizeof(cn));
                char buf[256];
                snprintf(buf, sizeof(buf), "expected ')' in call to '%s'", cn);
                err_expect_close(p, buf, GCL_E_PARSE_EXPECTED_CLOSE,
                                 TOK_LPAREN, TOK_RPAREN);
            }
            e = call;
        }
        /* postfix ++ / -- — B9: DEĞERİ ESKİ değerdir (C semantiği).
           Eskiden `e = (e + 1)` atamasına indirgeniyordu, bu yüzden postfix de
           YENİ değeri döndürüyordu ve `int j = i++;` j = 6 (C'de 5) veriyordu.
           Tek sahiplikli AST_EXPR_POSTINC düğümü hem double-free riskini
           (eski var_clone hilesi) hem de yanlış değeri ortadan kaldırır. */
        if (check(p, TOK_PLUSPLUS) || check(p, TOK_MINUSMINUS)) {
            GclTokenType op = peek(p)->type;
            advance(p);
            GclExpr *inc = new_expr(AST_EXPR_POSTINC);
            if (inc) {
                inc->left = e;
                inc->op = (op == TOK_PLUSPLUS) ? OP_ADD : OP_SUB;
            }
            e = inc;
        }
        return e;
    }
    if (t->type == TOK_LBRACE) {
        /* struct init list:
           { "John", 20 }                  → positional
           { .testo = 30, .name = "BETA" } → designated (by member name)
           {}                              → empty (default) */
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_INIT_LIST);
        if (!e) return NULL;
        int cap = 0;
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            GclExpr *item;
            if (check(p, TOK_DOT)) {
                /* designated initializer: .member = value */
                advance(p);
                GclToken *mt = peek(p);
                if (mt->type != TOK_IDENT && mt->type != TOK_KEYWORD) {
                    GclToken *dot = &p->toks[p->pos - 1];
                    char b[512];
                    snprintf(b, sizeof(b),
                             "expected member name after '.' at %d:%d (got '%.*s' — write e.g. .testo = 30)",
                             dot->line, dot->col, (int)mt->len, mt->lexeme);
                    if (!p->err) { p->msg = strdup(b); p->err = 1; }
                    item = NULL;
                    break;
                }
                char *mname = tok_str(mt);
                advance(p);
                GclExpr *target = new_expr(AST_EXPR_VAR);
                if (target) target->name = strdup(mname ? mname : "");
                free(mname);
                match(p, TOK_ASSIGN);
                GclExpr *val = parse_expr(p, 0);
                item = new_expr(AST_EXPR_ASSIGN);
                if (item) { item->left = target; item->right = val; }
            } else {
                /* pozisyonel: { 20, 30.5, "world", "ALPHA" } */
                item = parse_expr(p, 0);
            }
            if (p->err) break;
            if (e->arg_count >= cap) {
                cap = cap ? cap * 2 : 4;
                e->args = (GclExpr **)realloc(e->args, (size_t)cap * sizeof(GclExpr *));
            }
            e->args[e->arg_count++] = item;
            if (!match(p, TOK_COMMA)) break;
        }
        match(p, TOK_RBRACE);
        return e;
    }
    if (t->type == TOK_LPAREN) {
        advance(p);
        int save = p->pos;
        /* Qualified compound literal: (Raylib.Rectangle){ 400, 300, 200, 150 }.
           The unqualified form below cannot see this one because a dotted type
           name is three tokens; without this branch `(Raylib.Rectangle)` was
           parsed as a member access and the `{` that followed was left over,
           which surfaced as a confusing "expected expression" on the comma
           inside the initializer. */
        int qend = 0;
        if (qualified_type_span(p, &qend)) {
            p->pos = qend;
            if (match(p, TOK_RPAREN) && check(p, TOK_LBRACE))
                return parse_primary(p);   /* { ... } → AST_EXPR_INIT_LIST */
            p->pos = save;
        }
        /* Compound literal cast: (Vector3){ 10, 20, 30 } → parse as an init list.
           C-like syntax: if type name + ')' + '{' is seen, it is a compound literal. */
        if ((peek(p)->type == TOK_TYPE_NAME || peek(p)->type == TOK_IDENT)) {
            GclToken *ty_first = peek(p);
            int builtin_type = (ty_first->type == TOK_TYPE_NAME);
            GclToken *ty_last = ty_first;
            advance(p); /* type */
            /* `unsigned int`, `long long` — yerleşik tip sözcükleri bitişik gelir. */
            if (builtin_type) {
                while (peek(p)->type == TOK_TYPE_NAME) ty_last = advance(p);
            }
            if (match(p, TOK_RPAREN)) {
                if (check(p, TOK_LBRACE)) {
                    GclExpr *init = parse_primary(p); /* { ... } → AST_EXPR_INIT_LIST */
                    return init;
                }
                /* B11 — gerçek C-tipi cast: `(int)7.5`. Yalnızca YERLEŞİK tip
                   adları için (TOK_TYPE_NAME). Sıradan bir değişken adı da
                   TOK_IDENT'tır ve `(x) + 1` bir cast DEĞİLDİR — bu yüzden
                   IDENT'ler burada cast sayılmaz ve aşağıdaki normal parantez
                   yoluna düşer. */
                if (builtin_type) {
                    char *type_name = cast_type_name(ty_first, ty_last);
                    /* Operand bir tekli (unary) ifade gibi bağlanır:
                       `(int)7.5 / 2` → ((int)7.5) / 2 — C ile aynı. */
                    GclExpr *operand = parse_expr(p, 9);
                    GclExpr *e = new_expr(AST_EXPR_CAST);
                    if (e) {
                        e->left = operand;
                        e->str = type_name;
                        stamp_expr(e, ty_first);
                    } else {
                        free(type_name);
                    }
                    return e;
                }
                /* sadece bir cast değil — normal parantez ifadesi olabilir */
            }
        }
        p->pos = save;
        GclExpr *e = parse_expr(p, 0);
        if (!match(p, TOK_RPAREN))
            err_expect_close(p, "expected ')'", GCL_E_PARSE_EXPECTED_CLOSE,
                             TOK_LPAREN, TOK_RPAREN);
        return e;
    }
    if (t->type == TOK_MINUS || t->type == TOK_NOT || t->type == TOK_PLUS ||
        t->type == TOK_TILDE || t->type == TOK_AND) {
        GclTokenType op = t->type;
        advance(p);
        /* 9 is one above the highest binary precedence (8). With the old floor
           the operand swallowed every following binary operator:
               -3 + 5   parsed as  -(3 + 5)  = -8
               !a && b  parsed as  !(a && b)
           The second one silently turned every `while (!WindowShouldClose() && ...)`
           game loop into an infinite loop. */
        GclExpr *right = parse_expr(p, 9);
        GclExpr *e = new_expr(AST_EXPR_UNOP);
        if (e) {
            e->left = NULL;
            e->right = right;
            if (op == TOK_MINUS) e->op = OP_SUB;
            else if (op == TOK_PLUS) e->op = OP_ADD;
            else if (op == TOK_NOT) e->op = OP_NOT;    /* ! — boolean NOT */
            else if (op == TOK_AND) e->op = OP_BITAND;  /* & — address-of */
            else e->op = OP_BITNOT;                     /* ~ — bitwise NOT */
        }
        return e;
    }
    set_err(p, "expected expression");
    return NULL;
}

static int binop_prec(GclTokenType t) {
    switch (t) {
        case TOK_OR: case TOK_PIPEPIPE: return 1;
        case TOK_XOR: return 2;
        case TOK_AND: case TOK_AMPAMP: return 3;
        case TOK_EQ: case TOK_NE: return 4;
        case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE: return 5;
        case TOK_SHL: case TOK_SHR: return 6;
        case TOK_PLUS: case TOK_MINUS: return 7;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 8;
        default: return 0;
    }
}

static GclBinOp to_binop(GclTokenType t) {
    switch (t) {
        case TOK_PLUS: return OP_ADD;
        case TOK_MINUS: return OP_SUB;
        case TOK_STAR: return OP_MUL;
        case TOK_SLASH: return OP_DIV;
        case TOK_PERCENT: return OP_MOD;
        case TOK_EQ: return OP_EQ;
        case TOK_NE: return OP_NE;
        case TOK_LT: return OP_LT;
        case TOK_GT: return OP_GT;
        case TOK_LE: return OP_LE;
        case TOK_GE: return OP_GE;
        case TOK_AMPAMP: return OP_AND;    /* && — boolean AND */
        case TOK_AND: return OP_BITAND;    /* &  — bitwise AND */
        case TOK_PIPEPIPE: return OP_OR;   /* || — boolean OR */
        case TOK_OR: return OP_BITOR;      /* |  — bitwise OR */
        case TOK_XOR: return OP_BITXOR;    /* ^  — bitwise XOR */
        case TOK_SHL: return OP_SHL;
        case TOK_SHR: return OP_SHR;
        default: return OP_ASSIGN;
    }
}

/* Operator token'ını string'e dönüştür — typedef && AND; desteklemek için */
static char *operator_to_string(GclTokenType t) {
    switch (t) {
        case TOK_PLUS: return strdup("+");
        case TOK_MINUS: return strdup("-");
        case TOK_STAR: return strdup("*");
        case TOK_SLASH: return strdup("/");
        case TOK_PERCENT: return strdup("%");
        case TOK_EQ: return strdup("==");
        case TOK_NE: return strdup("!=");
        case TOK_LT: return strdup("<");
        case TOK_GT: return strdup(">");
        case TOK_LE: return strdup("<=");
        case TOK_GE: return strdup(">=");
        case TOK_AMPAMP: return strdup("&&");
        case TOK_AND: return strdup("&");
        case TOK_PIPEPIPE: return strdup("||");
        case TOK_OR: return strdup("|");
        case TOK_XOR: return strdup("^");
        case TOK_SHL: return strdup("<<");
        case TOK_SHR: return strdup(">>");
        case TOK_NOT: return strdup("!");
        case TOK_TILDE: return strdup("~");
        default: return NULL;
    }
}

/* Convert an operator alias identifier to its operator token
   Example: "AND" -> TOK_AMPAMP, "OR" -> TOK_PIPEPIPE, "XOR" -> TOK_XOR */
static GclTokenType resolve_operator_alias(const char *name) {
    if (!name) return TOK_EOF;
    if (strcmp(name, "AND") == 0) return TOK_AMPAMP;      /* && */
    if (strcmp(name, "OR") == 0) return TOK_PIPEPIPE;     /* || */
    if (strcmp(name, "XOR") == 0) return TOK_XOR;         /* ^ */
    if (strcmp(name, "NOT") == 0) return TOK_NOT;         /* ! */
    if (strcmp(name, "BIT_AND") == 0) return TOK_AND;     /* & */
    if (strcmp(name, "BIT_OR") == 0) return TOK_OR;       /* | */
    if (strcmp(name, "BIT_XOR") == 0) return TOK_XOR;     /* ^ */
    if (strcmp(name, "LEFT_SHIFT") == 0) return TOK_SHL;  /* << */
    if (strcmp(name, "RIGHT_SHIFT") == 0) return TOK_SHR; /* >> */
    return TOK_EOF;  /* Not an operator alias */
}

/* B15 — `typedef <op> NAME;` ile tanımlanan ÖZEL operatör takma adları.
   resolve_operator_alias() yalnızca sabit yerleşik adları tanır; `typedef && ANDOP;`
   bu listeyi genişletmiyordu, bu yüzden `a ANDOP b` ifadesi `a`'da kesiliyor ve
   HİÇBİR hata verilmeden yanlış sonuç (10) dönüyordu.
   GCL önce tüm dosyayı ayrıştırdığı için adlar, kullanıldıkları yerde hazırdır. */
#define GCL_MAX_OP_ALIASES 32
static char        *g_op_alias_names[GCL_MAX_OP_ALIASES];
static GclTokenType g_op_alias_ops[GCL_MAX_OP_ALIASES];
static int          g_op_alias_count = 0;

static void op_alias_reset(void) {
    for (int i = 0; i < g_op_alias_count; i++) {
        free(g_op_alias_names[i]);
        g_op_alias_names[i] = NULL;
    }
    g_op_alias_count = 0;
}

static void op_alias_add(const char *name, GclTokenType op) {
    if (!name || !name[0] || g_op_alias_count >= GCL_MAX_OP_ALIASES) return;
    g_op_alias_names[g_op_alias_count] = strdup(name);
    g_op_alias_ops[g_op_alias_count] = op;
    g_op_alias_count++;
}

static GclTokenType op_alias_lookup(const char *name) {
    if (!name) return TOK_EOF;
    for (int i = 0; i < g_op_alias_count; i++) {
        if (g_op_alias_names[i] && strcmp(g_op_alias_names[i], name) == 0)
            return g_op_alias_ops[i];
    }
    return TOK_EOF;
}

/* binary expr: parse_primary + binary operators */
static GclExpr *parse_expr_binary(Parser *p, int min_prec) {
    GclExpr *left = parse_primary(p);
    if (!left || p->err) return left;
    for (;;) {
        GclToken *t = peek(p);
        int prec = binop_prec(t->type);
        
        /* Operator alias check: can this IDENT token be an operator alias? */
        GclTokenType resolved_op = TOK_EOF;
        if (t->type == TOK_IDENT && prec == 0) {
            char *alias_name = tok_str(t);
            resolved_op = resolve_operator_alias(alias_name);
            /* B15: özel adlar (`typedef && ANDOP;`) sabit listede yoktur. */
            if (resolved_op == TOK_EOF) resolved_op = op_alias_lookup(alias_name);
            /* tok_str() HEAP tahsis eder; eskiden bu deger HIC serbest
               birakilmiyordu → "ifade IDENT" konumunun her denemesinde sizinti
               (sicak yol: her ikili operator arasindan sonra calisir).
               resolve_operator_alias yalnizca strcmp yapar, bu yuzden burada
               hemen serbest birakmak guvenli. */
            free(alias_name);
            if (resolved_op != TOK_EOF) {
                prec = binop_prec(resolved_op);
                t = &(GclToken){.type = resolved_op, .lexeme = "", .len = 0, .line = 0, .col = 0};
            }
        }
        
        if (prec < min_prec) break;
        advance(p);
        GclBinOp bop = to_binop(t->type);
        GclExpr *right = parse_expr_binary(p, prec + 1);
        GclExpr *e = new_expr(AST_EXPR_BINOP);
        if (e) { e->left = left; e->right = right; e->op = bop; }
        left = e;
        if (p->err) break;
    }
    return left;
}

/* Deep copy of an expression tree.
   When parse_assign rewrites `a += b` to `a = (a + b)`, the left-hand side needs
   an INDEPENDENT copy: if e->left (the assignment target) and bin->left
   (the left operand of the sum) are the same pointer, gcl_program_free will
   release the same Var node twice (double-free / segfault). */
static GclExpr *clone_expr(const GclExpr *e) {
    if (!e) return NULL;
    GclExpr *c = new_expr(e->kind);
    if (!c) return NULL;
    /* Carry the source position across. It used to be dropped, and the
       position matters BECAUSE this copy is what a rewritten compound
       assignment is built from: `a += b` becomes `a = (a + b)` with a CLONE on
       the left, so a runtime error raised while evaluating that clone reported
       "at 0:0" instead of the line the user wrote. stamp_expr_from() existed
       for this and was never called. */
    stamp_expr_from(c, e);
    c->op = e->op;
    c->num = e->num;
    c->str = e->str ? strdup(e->str) : NULL;
    c->name = e->name ? strdup(e->name) : NULL;
    c->member_name = e->member_name ? strdup(e->member_name) : NULL;
    c->left = clone_expr(e->left);
    c->right = clone_expr(e->right);
    if (e->arg_count > 0 && e->args) {
        c->args = (GclExpr **)calloc((size_t)e->arg_count, sizeof(GclExpr *));
        if (c->args) {
            for (int i = 0; i < e->arg_count; i++) {
                c->args[i] = clone_expr(e->args[i]);
            }
            c->arg_count = e->arg_count;
        }
    }
    return c;
}

/* assignment: a = expr, a += b, ... */
static GclExpr *parse_assign(Parser *p) {
    GclExpr *left = parse_expr_binary(p, 1);
    if (!left || p->err) return left;
    if (match(p, TOK_ASSIGN)) {
        GclExpr *right = parse_assign(p);
        GclExpr *e = new_expr(AST_EXPR_ASSIGN);
        if (e) { e->left = left; e->right = right; }
        return e;
    }
    /* Compound assignment: `a op= b` is rewritten to `a = (a op b)`.
       The set is the C set the language documents: += -= *= /= %= &= |= ^=.
       Each new operator needs its own lexer token; a missing one made the
       statement fall through and parse as two unrelated expressions. */
    if (check(p, TOK_PLUSEQ) || check(p, TOK_MINUSEQ) || check(p, TOK_STAREQ) || check(p, TOK_SLASHEQ) ||
        check(p, TOK_PERCENTEQ) || check(p, TOK_ANDEQ) || check(p, TOK_OREQ) || check(p, TOK_XOREQ)) {
        GclTokenType op = peek(p)->type;
        advance(p);
        GclExpr *right = parse_assign(p);
        GclExpr *e = new_expr(AST_EXPR_ASSIGN);
        GclBinOp bop =
            op == TOK_PLUSEQ    ? OP_ADD :
            op == TOK_MINUSEQ   ? OP_SUB :
            op == TOK_STAREQ    ? OP_MUL :
            op == TOK_SLASHEQ   ? OP_DIV :
            op == TOK_PERCENTEQ ? OP_MOD :
            /* `&=` / `|=` / `^=` are the BITWISE operators: the parser's
               OP_AND / OP_OR are the logical `&&` / `||` (see to_binop), so
               mapping to them would have turned `a &= b` into a boolean AND. */
            op == TOK_ANDEQ     ? OP_BITAND :
            op == TOK_OREQ      ? OP_BITOR : OP_BITXOR;
        if (e) {
            GclExpr *bin = new_expr(AST_EXPR_BINOP);
            if (bin) { bin->left = clone_expr(left); bin->right = right; bin->op = bop; }
            e->left = left;
            e->right = bin;
        }
        return e;
    }
    return left;
}

/* min_prec is honoured. It used to be ignored (`return parse_assign(p)`), so a
   unary operand was parsed with the LOWEST precedence floor and swallowed the
   rest of the expression -- see the two call sites in parse_primary(). */
static GclExpr *parse_expr(Parser *p, int min_prec) {
    if (min_prec <= 1) return parse_assign(p);   /* top level: allow `a = b`, `a += b` */
    return parse_expr_binary(p, min_prec);
}

/* ---------- Statements ---------- */

static GclStmt *parse_stmt_raw(Parser *p);
/* Serbest birakma ileri bildirimi: switch govdesinde ayristirilip ATILAN
   ifadeler icin gerekir (tanim dosyanin sonunda). */
static void free_stmt(GclStmt *s);

/* B31 / tanilama — her durum (statement) kendi kaynak konumunu TASISIN.
   `stamp_stmt` bu dosyada tanimliydi ama HICBIR yerde cagrilmiyordu; bu yuzden
   dongu disindaki `break` gibi runtime hatalari "at 0:0" (konumsuz) cikiyordu
   ve `s->line`/`s->col` okuyan her yol 0 goruyordu.
   Tek noktadan damgalayan sarmalayici (wrapper): ic ayristirici artik
   parse_stmt_raw'dir ve tum ozyinelemeli cagrilar bu sarmalayicidan gecer. */
static GclStmt *parse_stmt(Parser *p) {
    GclToken *start = peek(p);
    GclStmt *s = parse_stmt_raw(p);
    if (s && start) stamp_stmt(s, start);
    return s;
}

/* Multi-word type name: "long int", "unsigned int", "long double", "long long int" ... */
static char *parse_type_name(Parser *p) {
    if (peek(p)->type != TOK_TYPE_NAME) return NULL;
    char buf[256] = "";
    while (peek(p)->type == TOK_TYPE_NAME) {
        char *part = tok_str(peek(p));
        if (!part) break;
        if (buf[0]) strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
        free(part);
        advance(p);
    }
    return strdup(buf);
}

/* collect a switch case body: until the next case/default/} */
static GclStmt *parse_case_body(Parser *p) {
    GclStmt *blk = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (!blk) return NULL;
    blk->kind = STMT_BLOCK;
    int cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (check(p, TOK_KEYWORD) &&
            ((peek(p)->len == 4 && strncmp(peek(p)->lexeme, "case", 4) == 0) ||
             (peek(p)->len == 7 && strncmp(peek(p)->lexeme, "default", 7) == 0))) break;
        GclStmt *st = parse_stmt(p);
        if (!st) break;
        if (blk->u.block.count >= cap) {
            cap = cap ? cap * 2 : 8;
            blk->u.block.stmts = (GclStmt **)realloc(blk->u.block.stmts, (size_t)cap * sizeof(GclStmt *));
        }
        blk->u.block.stmts[blk->u.block.count++] = st;
    }
    return blk;
}

static GclStmt *parse_block(Parser *p) {
    if (!match(p, TOK_LBRACE)) { set_err(p, "expected '{'"); return NULL; }
    GclStmt *blk = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (!blk) return NULL;
    blk->kind = STMT_BLOCK;
    int cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (p->msg) break;
        GclStmt *s = parse_stmt(p);
        if (!s) break;
        if (blk->u.block.count >= cap) {
            cap = cap ? cap * 2 : 8;
            blk->u.block.stmts = (GclStmt **)realloc(blk->u.block.stmts, (size_t)cap * sizeof(GclStmt *));
        }
        blk->u.block.stmts[blk->u.block.count++] = s;
    }
    if (match(p, TOK_RBRACE)) return blk;
    err_expect_close(p, "expected '}'", GCL_E_PARSE_UNCLOSED_BLOCK,
                     TOK_LBRACE, TOK_RBRACE);
    return blk;
}

static GclStmt *new_stmt(GclStmtKind k) {
    GclStmt *s = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (s) s->kind = k;
    return s;
}

/* Bir token TOK_KEYWORD mi ve TAM OLARAK `word` mu? Lexeme NUL ile bitmez
   (uzunlugu ayri tutulur), bu yuzden hem uzunluk hem icerik karsilastirilir.
   Dosyadaki eski kullanimlar `t->len == 4 && strncmp(...)` seklinde elle
   yazilmisti; yeni hata-yakalama sozcukleri icin tek bir yardimci kullanilir. */
static int kw_eq(const GclToken *t, const char *word) {
    if (!t || t->type != TOK_KEYWORD || !word) return 0;
    size_t n = strlen(word);
    return t->len == n && strncmp(t->lexeme, word, n) == 0;
}

/* TURN 26 — function parameters live in ONE array of GclParam {name, type}.
   `func_decl_params_reserve` is the ONLY place that grows the array and
   `func_decl_param_add` is the ONLY place that appends an entry, so the two
   fields are always written together and every freshly grown slot is zeroed.

   Before this, parameters were TWO parallel arrays (`char **params` and
   `char **param_types`). The struct-parameter branch wrote only the name, and
   the type array was grown with `realloc`, whose new tail is NOT zeroed — so a
   struct parameter that landed past a growth boundary left an UNINITIALIZED
   pointer in the type table, which the runner later `strdup`'d (gcl_runner.c)
   and `free`'d (env_cleanup). That is undefined behaviour that happens to be
   benign on a fresh heap and a heap-corruption bug on a churned one. */
static int func_decl_params_reserve(GclFuncDecl *fd, int *cap, int want) {
    if (want <= *cap) return 1;
    int new_cap = *cap > 0 ? *cap : 4;
    while (new_cap < want) new_cap *= 2;
    /* Temporary pointer: on failure the old array (and *cap) stay valid. */
    GclParam *grown = (GclParam *)realloc(fd->params, (size_t)new_cap * sizeof(GclParam));
    if (!grown) return 0;
    for (int i = *cap; i < new_cap; i++) { grown[i].name = NULL; grown[i].type = NULL; }
    fd->params = grown;
    *cap = new_cap;
    return 1;
}

/* Append one parameter. On failure returns 0 and owns neither `name` nor
   `type` (the caller frees them and reports the error). */
static int func_decl_param_add(GclFuncDecl *fd, int *cap, char *name, char *type) {
    if (!func_decl_params_reserve(fd, cap, fd->param_count + 1)) return 0;
    /* The two fields are NOT interchangeable: `name` is the parameter's
       identifier, `type` is its declared type (NULL when the type is a
       struct/typedef/native name that the caller cannot resolve). Writing them
       the other way round made every parameter's name and type swap places, so
       a struct parameter (which passes type = NULL) ended up with a NULL NAME —
       the "half-written parameter" the ptest case catches. */
    fd->params[fd->param_count].name = name;
    fd->params[fd->param_count].type = type;
    fd->param_count++;
    return 1;
}

static GclStmt *parse_stmt_raw(Parser *p) {
    GclToken *t = peek(p);

    /* preprocessor line: skip */
    if (t->type == TOK_PREPROC) {
        advance(p);
        return (GclStmt *)calloc(1, sizeof(GclStmt)); /* empty */
    }

    /* modifier: const/public/private/global/local/inline — consume and continue.
       global: the variable persists (does not get hoisted out of the function),
       local: the variable is local (deleted after the call),
       inline: no-op, const: not assignable. */
    int mod_const = 0;
    int mod_global = 0;
    int mod_local = 0;
    while (t->type == TOK_KEYWORD &&
           (strncmp(t->lexeme, "const", 5) == 0 ||
            strncmp(t->lexeme, "public", 6) == 0 ||
            strncmp(t->lexeme, "private", 7) == 0 ||
            strncmp(t->lexeme, "global", 6) == 0 ||
            strncmp(t->lexeme, "local", 5) == 0 ||
            strncmp(t->lexeme, "inline", 6) == 0)) {
        if (t->len == 5 && strncmp(t->lexeme, "const", 5) == 0) mod_const = 1;
        if (t->len == 6 && strncmp(t->lexeme, "global", 6) == 0) mod_global = 1;
        if (t->len == 5 && strncmp(t->lexeme, "local", 5) == 0) mod_local = 1;
        advance(p);
        t = peek(p);
    }

    /* Typeless global/local list: "global g2, g_count;" or "local local_x;"
       — no type is specified, only names. global: binds to an externally defined/persistent variable
       (or creates it as 0 if absent); local: a local variable removed after the call. */
    if ((mod_global || mod_local) && t->type == TOK_IDENT) {
        GclStmt *head = NULL, *tail = NULL;
        while (t->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                s->u.var_decl.name = tok_str(t);
                s->u.var_decl.type_name = NULL;
                s->u.var_decl.is_global = mod_global;
                s->u.var_decl.is_const = mod_const;
                s->u.var_decl.array_size = 0;
                s->u.var_decl.is_pointer = 0;
            }
            if (!head) head = s; else tail->next = s;
            tail = s;
            advance(p);
            t = peek(p);
            if (match(p, TOK_COMMA)) { t = peek(p); continue; }
            break;
        }
        match(p, TOK_SEMI);
        if (head) return head;
    }

    /* block */
    if (t->type == TOK_LBRACE) return parse_block(p);

    /* if */
    if (t->type == TOK_KEYWORD && t->len == 2 && strncmp(t->lexeme, "if", 2) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_IF);
        if (!match(p, TOK_LPAREN)) set_err(p, "expected '(' after if");
        else s->u.if_.cond = parse_expr(p, 0);
        match(p, TOK_RPAREN);
        if (p->err) return s;
        s->u.if_.then = parse_stmt(p);
        if (check(p, TOK_KEYWORD) && peek(p)->len == 4 && strncmp(peek(p)->lexeme, "else", 4) == 0) {
            advance(p);
            s->u.if_.else_ = parse_stmt(p);
        }
        return s;
    }
    /* while */
    if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "while", 5) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_WHILE);
        if (!match(p, TOK_LPAREN)) set_err(p, "expected '(' after while");
        else s->u.while_.cond = parse_expr(p, 0);
        match(p, TOK_RPAREN);
        s->u.while_.body = parse_stmt(p);
        return s;
    }
    /* for */
    if (t->type == TOK_KEYWORD && t->len == 3 && strncmp(t->lexeme, "for", 3) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_FOR);
        if (match(p, TOK_LPAREN)) {
            /* init: TYPE NAME = expr | expr */
            if (peek(p)->type == TOK_TYPE_NAME) {
                char *type_name = parse_type_name(p);
                GclToken *nt = peek(p);
                char *vname = nt->type == TOK_IDENT ? tok_str(nt) : NULL;
                if (nt->type == TOK_IDENT) advance(p);
                if (match(p, TOK_ASSIGN)) {
                    GclExpr *init = parse_expr(p, 0);
                    if (vname) {
                        GclExpr *var_e = new_expr(AST_EXPR_VAR);
                        GclExpr *assign_e = new_expr(AST_EXPR_ASSIGN);
                        if (var_e) var_e->name = strdup(vname);
                        if (assign_e) { assign_e->left = var_e; assign_e->right = init; }
                        s->u.for_.var = assign_e ? assign_e : init;
                    } else {
                        s->u.for_.var = init;
                    }
                    free(type_name);
                    free(vname);
                } else {
                    free(type_name);
                    free(vname);
                }
            } else {
                s->u.for_.var = parse_expr(p, 0);
            }
            match(p, TOK_SEMI);
            if (!check(p, TOK_SEMI)) s->u.for_.cond = parse_expr(p, 0);
            match(p, TOK_SEMI);
            if (!check(p, TOK_RPAREN)) s->u.for_.inc = parse_expr(p, 0);
            match(p, TOK_RPAREN);
        }
        s->u.for_.body = parse_stmt(p);
        return s;
    }
    /* switch */
    if (t->type == TOK_KEYWORD && t->len == 6 && strncmp(t->lexeme, "switch", 6) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_SWITCH);
        if (match(p, TOK_LPAREN)) {
            /* Parse the FULL condition expression — `switch (v)`, `switch (v + 1)`
               and `switch (f(x))` are all valid. The old code consumed a single
               IDENT and left the rest of the expression in the token stream, so
               the case-collection loop called parse_stmt() on tokens it could
               never consume and the parser spun forever (hard hang, B1). */
            s->u.switch_.cond = parse_expr(p, 0);
            if (s->u.switch_.cond && s->u.switch_.cond->kind == AST_EXPR_VAR &&
                s->u.switch_.cond->name) {
                s->u.switch_.name = strdup(s->u.switch_.cond->name);
            }
        }
        if (!match(p, TOK_RPAREN)) set_err(p, "expected ')' after switch condition");
        match(p, TOK_LBRACE);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->err) {
            int __switch_before = p->pos;
            if (check(p, TOK_KEYWORD) && peek(p)->len == 4 && strncmp(peek(p)->lexeme, "case", 4) == 0) {
                advance(p);
                GclExpr *val = parse_expr(p, 0);
                match(p, TOK_COLON);
                GclStmt *body = parse_case_body(p);
                s->u.switch_.case_vals = (GclExpr **)realloc(s->u.switch_.case_vals, (size_t)(s->u.switch_.case_count + 1) * sizeof(GclExpr *));
                s->u.switch_.cases = (GclStmt **)realloc(s->u.switch_.cases, (size_t)(s->u.switch_.case_count + 1) * sizeof(GclStmt *));
                s->u.switch_.case_vals[s->u.switch_.case_count] = val;
                s->u.switch_.cases[s->u.switch_.case_count] = body;
                s->u.switch_.case_count++;
            } else if (check(p, TOK_KEYWORD) && peek(p)->len == 7 && strncmp(peek(p)->lexeme, "default", 7) == 0) {
                advance(p);
                match(p, TOK_COLON);
                s->u.switch_.default_case = parse_case_body(p);
            } else {
                /* switch {} icinde case/default DISINDA kalan ifade: ayristirilir
                   ve atilir. Eski `free(cs)` YALNIZ dugumu birakiyordu; ifade
                   agacinin ve zincirin tum alt tahsisleri (isimler, arguman
                   listeleri) siziyordu. */
                GclStmt *cs = parse_stmt(p);
                free_stmt(cs);
                if (check(p, TOK_SEMI)) advance(p);
            }
            /* Progress guarantee: if this round consumed no token, stop with an
               error rather than spinning forever (the old code could hang). */
            if (p->pos == __switch_before) {
                set_err(p, "unexpected token in switch body");
                break;
            }
        }
        match(p, TOK_RBRACE);
        return s;
    }
    /* try { ... } catch (err) { ... } finally { ... }
       En az bir `catch` ya da `finally` dali ZORUNLUDUR: yalniz `try { }`
       yazan bir kullanici hatayi yutar ve hicbir sey ogrenmezdi. */
    if (kw_eq(t, "try")) {
        advance(p);
        GclStmt *s = new_stmt(STMT_TRY);
        if (!check(p, TOK_LBRACE)) {
            set_err(p, "expected '{' after try");
            return s;
        }
        s->u.try_.body = parse_block(p);
        if (kw_eq(peek(p), "catch")) {
            advance(p);
            /* `catch (err)` — hata nesnesini adlandirir. `catch { }` de gecerli. */
            if (match(p, TOK_LPAREN)) {
                if (peek(p)->type == TOK_IDENT) {
                    s->u.try_.err_name = tok_str(peek(p));
                    advance(p);
                } else {
                    set_err(p, "expected an error variable name in 'catch (...)'");
                }
                if (!match(p, TOK_RPAREN)) set_err(p, "expected ')' after the catch binding");
            }
            if (check(p, TOK_LBRACE)) s->u.try_.catch_body = parse_block(p);
            else s->u.try_.catch_body = parse_stmt(p);
        }
        if (kw_eq(peek(p), "finally")) {
            advance(p);
            if (check(p, TOK_LBRACE)) s->u.try_.finally_body = parse_block(p);
            else s->u.try_.finally_body = parse_stmt(p);
        }
        if (!s->u.try_.catch_body && !s->u.try_.finally_body)
            set_err(p, "'try' requires a 'catch' or a 'finally' clause");
        return s;
    }
    /* throw <expr>;  /  raise <expr>;  (raise = throw takma adi) */
    if (kw_eq(t, "throw") || kw_eq(t, "raise")) {
        advance(p);
        GclStmt *s = new_stmt(STMT_THROW);
        if (!check(p, TOK_SEMI) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF))
            s->u.throw_.expr = parse_expr(p, 0);
        if (!match(p, TOK_SEMI)) set_err(p, "expected ';' after the thrown value");
        return s;
    }
    /* defer <stmt>;  — govde blogu (ya da tek dongu iterasyonu) kapanirken
       LIFO sirayla calisir. Govde tek bir ifade ya da bloga alinmis bir
       eylem olabilir: `defer Stdio.closeFile(f);` / `defer { ... }`. */
    if (kw_eq(t, "defer")) {
        advance(p);
        GclStmt *s = new_stmt(STMT_DEFER);
        if (!s) return NULL;
        if (check(p, TOK_LBRACE)) s->u.defer_.stmt = parse_block(p);
        else s->u.defer_.stmt = parse_stmt(p);
        if (!s->u.defer_.stmt)
            set_err(p, "expected a statement after 'defer'");
        return s;
    }
    /* return */
    if (t->type == TOK_KEYWORD && t->len == 6 && strncmp(t->lexeme, "return", 6) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_RETURN);
        if (!check(p, TOK_SEMI) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            s->u.ret.expr = parse_expr(p, 0);
        }
        match(p, TOK_SEMI);
        return s;
    }
    /* break/continue */
    if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "break", 5) == 0) {
        advance(p); GclStmt *s = new_stmt(STMT_BREAK); match(p, TOK_SEMI); return s;
    }
    if (t->type == TOK_KEYWORD && t->len == 8 && strncmp(t->lexeme, "continue", 8) == 0) {
        advance(p); GclStmt *s = new_stmt(STMT_CONTINUE); match(p, TOK_SEMI); return s;
    }
    /* typedef int number;  or  typedef struct { ... } Name;  or  typedef enum { ... } Name;
       or operator alias: typedef && AND; typedef || OR; typedef << LEFT_SHIFT; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "typedef", 7) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_TYPEDEF);
        if (!s) return NULL;
        
        /* Operator alias kontrolü: typedef && AND; typedef || OR; vb. */
        if (peek(p)->type == TOK_PLUS || peek(p)->type == TOK_MINUS || peek(p)->type == TOK_STAR ||
            peek(p)->type == TOK_SLASH || peek(p)->type == TOK_PERCENT || peek(p)->type == TOK_EQ ||
            peek(p)->type == TOK_NE || peek(p)->type == TOK_LT || peek(p)->type == TOK_GT ||
            peek(p)->type == TOK_LE || peek(p)->type == TOK_GE || peek(p)->type == TOK_AMPAMP ||
            peek(p)->type == TOK_AND || peek(p)->type == TOK_PIPEPIPE || peek(p)->type == TOK_OR ||
            peek(p)->type == TOK_XOR || peek(p)->type == TOK_SHL || peek(p)->type == TOK_SHR ||
            peek(p)->type == TOK_NOT || peek(p)->type == TOK_TILDE) {
            GclTokenType op_type = peek(p)->type;
            advance(p);
            s->u.typedef_info.base_type = operator_to_string(op_type);
            /* alias name */
            if (peek(p)->type == TOK_IDENT) {
                s->u.typedef_info.alias_name = tok_str(peek(p));
                advance(p);
            }
            /* B15: adı ayrıştırma tablosuna kaydet — sonraki kullanımlar
               `a ANDOP b` ifadesini operatör olarak çözebilsin. */
            op_alias_add(s->u.typedef_info.alias_name, op_type);
            match(p, TOK_SEMI);
            return s;
        }
        
        if (peek(p)->type == TOK_TYPE_NAME) {
            s->u.typedef_info.base_type = parse_type_name(p);
        } else if (peek(p)->type == TOK_KEYWORD && strncmp(peek(p)->lexeme, "struct", 6) == 0) {
            advance(p);
            char *sname = NULL;
            if (peek(p)->type == TOK_IDENT) { sname = tok_str(peek(p)); advance(p); }
            if (!match(p, TOK_LBRACE)) { /* otherwise: typedef struct Name; — simple pass-through */ }
            else {
                /* parse the body actually: collect members */
                GclStmt *sd = new_stmt(STMT_STRUCT_DECL);
                if (sd) {
                    int cap = 0;
                    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                        /* Member type: known type (TOK_TYPE_NAME) OR another struct name (TOK_IDENT) */
                        GclToken *mtype_tok = peek(p);
                        char *mtype = NULL;
                        if (mtype_tok->type == TOK_TYPE_NAME) {
                            mtype = parse_type_name(p);
                        } else if (qualified_type_span(p, NULL)) {
                            /* native struct member: Raylib.Vector2 position; */
                            mtype = parse_qualified_type_name(p);
                        } else if (mtype_tok->type == TOK_IDENT) {
                            mtype = tok_str(mtype_tok);
                            advance(p);
                        } else {
                            /* Invalid token — prevent an infinite loop, skip */
                            advance(p);
                            continue;
                        }
                        if (peek(p)->type == TOK_IDENT) {
                            if (sd->u.struct_info.member_count >= cap) {
                                cap = cap ? cap * 2 : 4;
                                sd->u.struct_info.member_names = (char **)realloc(sd->u.struct_info.member_names, (size_t)cap * sizeof(char *));
                                sd->u.struct_info.member_types = (char **)realloc(sd->u.struct_info.member_types, (size_t)cap * sizeof(char *));
                            }
                            sd->u.struct_info.member_names[sd->u.struct_info.member_count] = tok_str(peek(p));
                            sd->u.struct_info.member_types[sd->u.struct_info.member_count] = mtype;
                            sd->u.struct_info.member_count++;
                            advance(p);
                            if (match(p, TOK_LBRACKET)) {
                                while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                                match(p, TOK_RBRACKET);
                            }
                        } else {
                            free(mtype);
                        }
                        match(p, TOK_SEMI);
                    }
                    match(p, TOK_RBRACE);
                }
                /* struct name: in an inline block, this becomes the typedef alias name */
                if (sname) free(sname);
                /* alias name will be read next — use sname as the next IDENT */
                if (peek(p)->type == TOK_IDENT) {
                    s->u.typedef_info.alias_name = tok_str(peek(p));
                    if (sd) sd->u.struct_info.struct_name = strdup(s->u.typedef_info.alias_name);
                    advance(p);
                }
                char base[128];
                snprintf(base, sizeof(base), "struct %s", s->u.typedef_info.alias_name ? s->u.typedef_info.alias_name : "");
                s->u.typedef_info.base_type = strdup(base);
                s->next = sd;
            }
            match(p, TOK_SEMI);
            return s;
        } else if (peek(p)->type == TOK_KEYWORD && strncmp(peek(p)->lexeme, "enum", 4) == 0) {
            advance(p);
            char *ename = NULL;
            if (peek(p)->type == TOK_IDENT) { ename = tok_str(peek(p)); advance(p); }
            GclStmt *en = NULL;
            if (match(p, TOK_LBRACE)) {
                en = new_stmt(STMT_ENUM);
                if (en) {
                    int cap = 0;
                    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                        if (peek(p)->type == TOK_IDENT || peek(p)->type == TOK_KEYWORD) {
                            if (en->u.enum_info.const_count >= cap) {
                                cap = cap ? cap * 2 : 4;
                                en->u.enum_info.const_names = (char **)realloc(en->u.enum_info.const_names, (size_t)cap * sizeof(char *));
                            }
                            en->u.enum_info.const_names[en->u.enum_info.const_count++] = tok_str(peek(p));
                            advance(p);
                        }
                        if (!match(p, TOK_COMMA)) break;
                    }
                    match(p, TOK_RBRACE);
                }
            } else if (ename) {
                en = NULL; /* enum Name; kullanımı — tek başına */
            }
            if (peek(p)->type == TOK_IDENT) {
                s->u.typedef_info.alias_name = tok_str(peek(p));
                if (en) en->u.enum_info.enum_name = strdup(s->u.typedef_info.alias_name);
                advance(p);
            }
            s->u.typedef_info.base_type = strdup("enum");
            if (en) s->next = en;
            if (ename) free(ename);
            match(p, TOK_SEMI);
            return s;
        }
        /* alias adı */
        if (peek(p)->type == TOK_IDENT) {
            s->u.typedef_info.alias_name = tok_str(peek(p));
            advance(p);
        }
        match(p, TOK_SEMI);
        return s;
    }
    /* enum Day { MONDAY, TUESDAY }; or enum { ... } var; or enum Day today; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "enum", 4) == 0) {
        advance(p);
        /* enum name may exist: enum Day ... */
        char *enum_name = NULL;
        if (peek(p)->type == TOK_IDENT) {
            enum_name = tok_str(peek(p));
            advance(p);
        }
        /* enum definition: enum Day { ... }; */
        if (match(p, TOK_LBRACE)) {
            GclStmt *s = new_stmt(STMT_ENUM);
            if (s) {
                if (enum_name) s->u.enum_info.enum_name = strdup(enum_name);
                int cap = 0;
                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    if (peek(p)->type == TOK_IDENT || peek(p)->type == TOK_KEYWORD) {
                        if (s->u.enum_info.const_count >= cap) {
                            cap = cap ? cap * 2 : 4;
                            s->u.enum_info.const_names = (char **)realloc(s->u.enum_info.const_names, (size_t)cap * sizeof(char *));
                        }
                        s->u.enum_info.const_names[s->u.enum_info.const_count++] = tok_str(peek(p));
                        advance(p);
                    }
                    if (!match(p, TOK_COMMA)) break;
                }
                match(p, TOK_RBRACE);
            }
            free(enum_name);
            match(p, TOK_SEMI);
            return s;
        }
        /* enum Day today; → var_decl */
        if (enum_name && peek(p)->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                char type_str[128];
                snprintf(type_str, sizeof(type_str), "enum %s", enum_name);
                s->u.var_decl.type_name = strdup(type_str);
                s->u.var_decl.name = tok_str(peek(p));
                advance(p);
                if (match(p, TOK_ASSIGN)) {
                    s->u.var_decl.init = parse_expr(p, 0);
                }
                match(p, TOK_SEMI);
            }
            free(enum_name);
            return s;
        }
        free(enum_name);
        match(p, TOK_SEMI);
        return (GclStmt *)calloc(1, sizeof(GclStmt));
    }
    /* struct Student { char name[20]; int age; };  or  struct Student s1; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "struct", 6) == 0) {
        advance(p);
        char *struct_type = NULL;
        if (peek(p)->type == TOK_IDENT) {
            struct_type = tok_str(peek(p));
            advance(p);
        }
        if (match(p, TOK_LBRACE)) {
            /* struct definition */
            GclStmt *s = new_stmt(STMT_STRUCT_DECL);
            if (s) {
                if (struct_type) {
                    s->u.struct_info.struct_name = strdup(struct_type);
                }
                int cap = 0;
                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    /* Member tipi: bilinen tip (TOK_TYPE_NAME) VEYA başka struct adı (TOK_IDENT) */
                    GclToken *mtype_tok = peek(p);
                    char *mtype = NULL;
                    if (mtype_tok->type == TOK_TYPE_NAME) {
                        mtype = parse_type_name(p);
                    } else if (qualified_type_span(p, NULL)) {
                        /* native struct member: Raylib.Vector2 position; */
                        mtype = parse_qualified_type_name(p);
                    } else if (mtype_tok->type == TOK_IDENT) {
                        mtype = tok_str(mtype_tok);
                        advance(p);
                    } else {
                        /* Geçersiz token — sonsuz döngüyü önle, atla */
                        advance(p);
                        continue;
                    }
                    if (peek(p)->type == TOK_IDENT) {
                        if (s->u.struct_info.member_count >= cap) {
                            cap = cap ? cap * 2 : 4;
                            s->u.struct_info.member_names = (char **)realloc(s->u.struct_info.member_names, (size_t)cap * sizeof(char *));
                            s->u.struct_info.member_types = (char **)realloc(s->u.struct_info.member_types, (size_t)cap * sizeof(char *));
                        }
                        s->u.struct_info.member_names[s->u.struct_info.member_count] = tok_str(peek(p));
                        s->u.struct_info.member_types[s->u.struct_info.member_count] = mtype;
                        s->u.struct_info.member_count++;
                        advance(p);
                        if (match(p, TOK_LBRACKET)) {
                            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                            match(p, TOK_RBRACKET);
                        }
                    } else {
                        free(mtype);
                    }
                    match(p, TOK_SEMI);
                }
                match(p, TOK_RBRACE);
            }
            match(p, TOK_SEMI);
            free(struct_type);
            return s;
        }
        /* değilse: struct {type} name; → var_decl
           Support arrays: struct T name[...]; and optional initializer. */
        if (struct_type && peek(p)->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                char type_str[128];
                snprintf(type_str, sizeof(type_str), "struct %s", struct_type);
                s->u.var_decl.type_name = strdup(type_str);
                s->u.var_decl.array_size = 0;
                s->u.var_decl.array_inner = 0;
                s->u.var_decl.is_pointer = 0;
                s->u.var_decl.is_const = 0;
                s->u.var_decl.is_global = 0;
                s->u.var_decl.name = tok_str(peek(p));
                advance(p);
                /* array dimensions (optional) */
                if (match(p, TOK_LBRACKET)) {
                    if (check(p, TOK_NUMBER)) {
                        s->u.var_decl.array_size = (int)peek(p)->num;
                        advance(p);
                    } else if (check(p, TOK_RBRACKET)) {
                        s->u.var_decl.array_size = -1;
                    } else {
                        set_err(p, "expected array size");
                    }
                    match(p, TOK_RBRACKET);
                    if (match(p, TOK_LBRACKET)) {
                        if (check(p, TOK_NUMBER)) {
                            s->u.var_decl.array_inner = (int)peek(p)->num;
                            advance(p);
                        } else if (check(p, TOK_RBRACKET)) {
                            s->u.var_decl.array_inner = -1;
                        } else {
                            set_err(p, "expected array size");
                        }
                        match(p, TOK_RBRACKET);
                    }
                }
                if (match(p, TOK_ASSIGN)) {
                    s->u.var_decl.init = parse_expr(p, 0);
                }
                match(p, TOK_SEMI);
            }
            free(struct_type);
            return s;
        }
        free(struct_type);
        match(p, TOK_SEMI);
        return (GclStmt *)calloc(1, sizeof(GclStmt));
    }

    /* function decl: TYPE name(params) { ... } */
    if ((t->type == TOK_TYPE_NAME || t->type == TOK_IDENT) &&
        p->pos + 2 < p->count) {
        GclToken *nxt1 = &p->toks[p->pos + 1];
        GclToken *nxt2 = &p->toks[p->pos + 2];
        int is_func = (nxt1->type == TOK_IDENT && nxt2->type == TOK_LPAREN);
        /* `Raylib.Rectangle make() { ... }` — qualified return type */
        int qual_ret = 0;
        if (!is_func && qualified_type_span(p, &qual_ret) &&
            qual_ret + 1 < p->count &&
            p->toks[qual_ret].type == TOK_IDENT &&
            p->toks[qual_ret + 1].type == TOK_LPAREN) {
            is_func = 1;
        }
        if (t->type == TOK_TYPE_NAME) {
            int scan = p->pos;
            while (scan < p->count && p->toks[scan].type == TOK_TYPE_NAME) scan++;
            if (scan + 1 < p->count && p->toks[scan].type == TOK_IDENT &&
                p->toks[scan + 1].type == TOK_LPAREN) is_func = 1;
        }
        if (is_func) {
            /* TYPE name( */
            char *rtype = qual_ret ? parse_qualified_type_name(p)
                                   : ((t->type == TOK_TYPE_NAME) ? parse_type_name(p) : tok_str(t));
            if (!qual_ret && t->type != TOK_TYPE_NAME) advance(p);
            GclStmt *s = new_stmt(STMT_FUNC_DECL);
            if (s) {
                s->u.func_decl.name = tok_str(peek(p));
                advance(p);
                if (match(p, TOK_LPAREN)) {
                    int cap = 0;
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        /* param: [modul.]TYPE [*...] NAME [array]  veya  TYPE
                           B14: struct/typedef adi TOK_IDENT'tir; eskiden yalnizca
                           TOK_TYPE_NAME kabul edildigi icin `int getId(P v)`
                           "expected expression" veriyordu. */
                        int param_is_struct_type = 0;
                        int qend14 = 0;
                        if (qualified_type_span(p, &qend14) && qend14 < p->count &&
                            p->toks[qend14].type == TOK_IDENT) {
                            p->pos = qend14;              /* Raylib.Rectangle v */
                            param_is_struct_type = 1;
                        } else if (peek(p)->type == TOK_IDENT && p->pos + 1 < p->count &&
                                   (p->toks[p->pos + 1].type == TOK_IDENT ||
                                    p->toks[p->pos + 1].type == TOK_STAR)) {
                            advance(p);                    /* Player v / Player *v */
                            param_is_struct_type = 1;
                        }
                        if (param_is_struct_type) {
                            while (check(p, TOK_STAR)) advance(p);
                            if (peek(p)->type == TOK_IDENT) {
                                /* Struct/typedef/native type: the name is a single
                                   token and the declared type stays NULL (the
                                   runner only uses it to decide whether a value
                                   prints as a character). Name and type are
                                   written by the ONE helper that also grows the
                                   array — a half-written parameter is impossible.
                                   Before TURN 26 this branch wrote only `params`
                                   and left `param_types[i]` uninitialized. */
                                char *pname = tok_str(peek(p));
                                if (!func_decl_param_add(&s->u.func_decl, &cap, pname, NULL)) {
                                    free(pname);
                                    set_err(p, "out of memory while parsing the parameter list");
                                    break;
                                }
                                advance(p);
                                if (match(p, TOK_LBRACKET)) {
                                    while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                                    match(p, TOK_RBRACKET);
                                }
                            }
                        } else if (peek(p)->type == TOK_TYPE_NAME) {
                            /* Keep the declared type: the runner needs it so
                               `void show(char x)` prints `x` as a character
                               instead of its numeric code. Ownership of `ptype`
                               moves into the parameter entry. */
                            char *ptype = parse_type_name(p);
                            /* pointer stars: char *argv, char **argv */
                            while (check(p, TOK_STAR)) advance(p);
                            if (peek(p)->type == TOK_IDENT) {
                                /* Name AND type go in through the same call, so
                                   the two can never drift apart. */
                                char *pname = tok_str(peek(p));
                                if (!func_decl_param_add(&s->u.func_decl, &cap, pname, ptype)) {
                                    free(pname);
                                    free(ptype);
                                    set_err(p, "out of memory while parsing the parameter list");
                                    break;
                                }
                                advance(p);
                                /* array size: int arr[10] or char *argv[] */
                                if (match(p, TOK_LBRACKET)) {
                                    while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                                    match(p, TOK_RBRACKET);
                                }
                            } else {
                                /* A type with no parameter name after it: the
                                   type string has no owner now, so release it
                                   here (the old code leaked it). */
                                free(ptype);
                            }
                        }
                        if (!match(p, TOK_COMMA)) break;
                    }
                    match(p, TOK_RPAREN);
                }
                s->u.func_decl.body = parse_stmt(p); /* { ... } */
                /* Donus tipini SAKLA. Eskiden burada `free(rtype)` vardi ve tip
                   bilgisi kayboluyordu; runner bir cagrinin turunu bilemedigi
                   icin `gcChar f()` sonucu metin olarak yazdirilamiyordu.
                   Sahiplik AST'ye gecer (gcl_program_free serbest birakir). */
                s->u.func_decl.return_type = rtype;
            }
            return s;
        }
    }

    /* var decl: TYPE NAME = expr;  veya  char name[32];  char buf[];  char *ptr;
       Qualified native type counts as one: `Raylib.Rectangle Player = ...;`
       declares the same variable as `Rectangle Player = ...;`. */
    int qual_decl = qualified_decl_follows(p);
    if (t->type == TOK_TYPE_NAME || qual_decl ||
        (t->type == TOK_IDENT && p->pos + 1 < p->count && p->toks[p->pos+1].type == TOK_IDENT)) {
        /* IDENT + IDENT = typedef alias + değişken adı (Student s1;) */
        char *type_name;
        if (qual_decl) {
            type_name = parse_qualified_type_name(p);
        } else {
            type_name = (t->type == TOK_TYPE_NAME) ? parse_type_name(p) : tok_str(t);
            if (t->type != TOK_TYPE_NAME) advance(p);
        }
        GclStmt *s = new_stmt(STMT_VAR_DECL);
        if (s) {
            s->u.var_decl.type_name = type_name;
            s->u.var_decl.array_size = 0;
            s->u.var_decl.is_pointer = 0;
            s->u.var_decl.is_const = mod_const;
            s->u.var_decl.is_global = mod_global;
            /* char *ptr — pointer */
            while (check(p, TOK_STAR)) { advance(p); s->u.var_decl.is_pointer = 1; }
            GclToken *name_t = peek(p);
            if (name_t->type == TOK_IDENT) {
                advance(p);
                s->u.var_decl.name = tok_str(name_t);
            } else {
                set_err(p, "expected variable name");
            }
            /* char name[N] veya char name[] (aynı zamanda çok boyutlu: [N][M]) */
            s->u.var_decl.array_inner = 0;
            if (match(p, TOK_LBRACKET)) {
                if (check(p, TOK_NUMBER)) {
                    s->u.var_decl.array_size = (int)peek(p)->num;
                    advance(p);
                } else if (check(p, TOK_RBRACKET)) {
                    s->u.var_decl.array_size = -1; /* boş [] */
                } else {
                    set_err(p, "expected array size");
                }
                match(p, TOK_RBRACKET);
                /* support second dimension for declarations like char a[3][20] */
                if (check(p, TOK_LBRACKET)) {
                    /* parse inner dimension but keep as array_inner */
                    if (match(p, TOK_LBRACKET)) {
                        if (check(p, TOK_NUMBER)) {
                            s->u.var_decl.array_inner = (int)peek(p)->num;
                            advance(p);
                        } else if (check(p, TOK_RBRACKET)) {
                            s->u.var_decl.array_inner = -1; /* empty inner */
                        } else {
                            set_err(p, "expected array size");
                        }
                        match(p, TOK_RBRACKET);
                    }
                }
            }
            if (match(p, TOK_ASSIGN)) {
                s->u.var_decl.init = parse_expr(p, 0);
            }
            /* B16: başlatıcıdan sonra `;` ZORUNLU. Eskiden `match(TOK_SEMI)`
               sonucu yok sayılıyordu; `int x = 5 6;` sessizce `x = 5` oluyordu
               (aynı mekanizma B11 cast'ini ve B15'i de gizliyordu). */
            if (!match(p, TOK_SEMI) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                set_err(p, "expected ';' after variable declaration");
            }
        }
        return s;
    }

    /* expression statement */
    GclExpr *e = parse_expr(p, 0);
    if (p->err) return NULL;
    GclStmt *s = new_stmt(STMT_EXPR);
    if (s) s->u.expr = e;
    /* B16: ifade deyiminden sonra da `;` beklenir. */
    if (!match(p, TOK_SEMI) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        set_err(p, "expected ';' after expression");
    }
    return s;
}

/* ONE parse loop, TWO front doors.

   `gcl_parse_diag` used to be a DECLARATION ONLY: gcl_parser.h promised a
   rich-diagnostic parser (codes, real spans, notes) and nothing in the tree
   defined or called it, while this function hard-set `p.diags = NULL` — so
   every diagnostic this file builds was discarded the moment it was built and
   parse_error()'s `gcl_diag_add` never ran. The sink is a PARAMETER now, so
   the two entry points share this single loop and cannot drift apart. */
static GclProgram *parse_tokens(GclTokenList *tokens, GclDiagList *diags,
                                char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!tokens) return NULL;
    GclProgram *prog = (GclProgram *)calloc(1, sizeof(GclProgram));
    if (!prog) return NULL;

    /* B15: özel operatör takma adları her ayrıştırmada sıfırlanır. */
    op_alias_reset();

    Parser p;
    p.toks = tokens->tokens;
    p.count = tokens->count;
    p.pos = 0;
    p.err = 0;
    p.msg = NULL;
    /* `diags` alanı burada HİÇ başlatılmıyordu; parse_error() `if (!p->diags)`
       ile okuduğu için her söz dizimi hatası ilklendirilmemiş bir pointer
       üzerinden dallanıyordu (UB — şansımıza NULL çıkıyordu). Artık çağıranın
       seçtiği sink: NULL ise yalnızca eski düz mesaj üretilir. */
    p.diags = diags;

    int cap = 0;
    while (!check(&p, TOK_EOF) && !p.err) {
        GclStmt *s = parse_stmt(&p);
        if (!s) break;
        /* s->next zinciri: typedef struct/enum içinde ek lenmiş stmt'ler */
        GclStmt *chain = s;
        while (chain) {
            GclStmt *nx = chain->next;
            chain->next = NULL;
            if (chain->kind == STMT_EXPR && chain->u.expr == NULL) { free(chain); chain = nx; continue; }
            if (prog->count >= cap) {
                cap = cap ? cap * 2 : 16;
                prog->stmts = (GclStmt **)realloc(prog->stmts, (size_t)cap * sizeof(GclStmt *));
            }
            prog->stmts[prog->count++] = chain;
            chain = nx;
        }
    }

    if (p.err) {
        if (error_msg) *error_msg = p.msg;
        else free(p.msg);
        gcl_program_free(prog);
        return NULL;
    }
    return prog;
}

/* Legacy shape: one flattened message, no sink. Every caller that predates the
   diagnostic vocabulary (gcl_simple_runner.c) still comes through here, which
   is why the CLI's wording — and the golden cases that pin it — is unchanged. */
GclProgram *gcl_parse(GclTokenList *tokens, char **error_msg) {
    return parse_tokens(tokens, NULL, error_msg);
}

/* Rich shape: on failure the list receives the diagnostic with its CODE, its
   SPAN and whatever note/hint the site supplied. No legacy message is written
   (the body frees it), so a caller renders from the list instead. */
GclProgram *gcl_parse_diag(GclTokenList *tokens, struct GclDiagList *diags) {
    return parse_tokens(tokens, (GclDiagList *)diags, NULL);
}

/* ---------- Derinlemesine AST free (memory leak) ---------- */

static void free_expr(GclExpr *e) {
    if (!e) return;
    if (e->left) free_expr(e->left);
    if (e->right) free_expr(e->right);
    if (e->args) {
        for (int i = 0; i < e->arg_count; i++) free_expr(e->args[i]);
        free(e->args);
    }
    if (e->str) free(e->str);
    if (e->name) free(e->name);
    if (e->member_name) free(e->member_name);
    free(e);
}

/* Derleme zamani bekcisi — `GclStmtKind`'in son iki degeri asagidaki switch
   ile BIRLIKTE yazilmistir. ARADA yeni bir tur eklenirse numaralar kayar ve bu
   satir build'i kirar (negatif dizi boyutu); EN SONA eklenirse `default:`
   onu runtime'da yakalar. Ikisi birlikte "sessiz sizinti"yi imkansiz kilar:
   switch'in bir turu kapsamayi unutmasi artik sessiz kalamaz. */
typedef char gcl_stmt_kind_layout_guard[(STMT_THROW == 15 && STMT_DEFER == 16) ? 1 : -1];

static void free_stmt(GclStmt *s) {
if (!s) return;
    switch (s->kind) {
        case STMT_EXPR:
            free_expr(s->u.expr);
            break;
        case STMT_VAR_DECL:
            free(s->u.var_decl.name);
            free(s->u.var_decl.type_name);
            free_expr(s->u.var_decl.init);
            break;
        case STMT_IF:
            free_expr(s->u.if_.cond);
            free_stmt(s->u.if_.then);
            free_stmt(s->u.if_.else_);
            break;
        case STMT_WHILE:
            free_expr(s->u.while_.cond);
            free_stmt(s->u.while_.body);
            break;
        case STMT_FOR:
            free_expr(s->u.for_.var);
            free_expr(s->u.for_.cond);
            free_expr(s->u.for_.inc);
            free_stmt(s->u.for_.body);
            break;
        case STMT_RETURN:
            free_expr(s->u.ret.expr);
            free(s->u.ret.type_name);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < s->u.block.count; i++) free_stmt(s->u.block.stmts[i]);
            free(s->u.block.stmts);
            break;
        case STMT_FUNC_DECL:
            free(s->u.func_decl.name);
            /* TURN 26 — ONE parameter array, each entry owning its name and its
               (optional) type. The old shape freed TWO parallel arrays, which is
               what let a half-written entry slip through unnoticed. */
            for (int i = 0; i < s->u.func_decl.param_count; i++) {
                free(s->u.func_decl.params[i].name);
                free(s->u.func_decl.params[i].type);
            }
            free(s->u.func_decl.params);
            /* TUR 23 — CIFT SERBEST BIRAKMA (double free) BULUNDU VE DUZELTILDI.
               `return_type` burada IKI KEZ free ediliyordu. Sonuc:
               ICINDE FONKSIYON TANIMI olan HER programda heap bozuluyordu
               (STATUS_HEAP_CORRUPTION = 0xC0000374). Program ciktisini DOGRU
               uretiyor, surec CIKARKEN cokuyordu — bu yuzden belirti, testin
               konusuyla ilgisiz gorunuyordu. Tek satir su dosyalarin TAMAMINI
               kirmizi yapiyordu:
                   language/tests/gcsf/functions/         (params, recursion, arity)
                   language/tests/gcsf/defer/             (frames, braced_exit_paths,
                                                           nested_cleanup, missing_statement)
                   language/tests/gcsf/errors/            (finally_exit_paths,
                                                           try_catch_kinds)
                   language/tests/gcsf/stdio/char_rendering.gcsf
                   language/tests/parser/test_parser.c    (ptest)
               Hicbiri kendi konusuyla ilgili DEGILDI; hepsi yalnizca bir
               fonksiyon tanimliyordu. */
            free(s->u.func_decl.return_type);
            free_stmt(s->u.func_decl.body);
            break;
        case STMT_SWITCH:
            free_expr(s->u.switch_.cond);
            free(s->u.switch_.name);
            for (int i = 0; i < s->u.switch_.case_count; i++) {
                free_expr(s->u.switch_.case_vals[i]);
                free_stmt(s->u.switch_.cases[i]);
            }
            free(s->u.switch_.case_vals);
            free(s->u.switch_.cases);
            free_stmt(s->u.switch_.default_case);
            break;
        case STMT_TYPEDEF:
            free(s->u.typedef_info.alias_name);
            free(s->u.typedef_info.base_type);
            break;
        case STMT_ENUM:
            free(s->u.enum_info.enum_name);
            for (int i = 0; i < s->u.enum_info.const_count; i++) free(s->u.enum_info.const_names[i]);
            free(s->u.enum_info.const_names);
            break;
        case STMT_STRUCT_DECL:
            free(s->u.struct_info.struct_name);
            for (int i = 0; i < s->u.struct_info.member_count; i++) {
                free(s->u.struct_info.member_names[i]);
                free(s->u.struct_info.member_types[i]);
            }
            free(s->u.struct_info.member_names);
            free(s->u.struct_info.member_types);
            break;
        case STMT_DEFER:
            /* `defer` payload'i AST'ye aittir; birakilmazsa her 'defer'
               dugumu icin ifade agacinin tamami sizar. */
            free_stmt(s->u.defer_.stmt);
            break;
        case STMT_TRY:
            free_stmt(s->u.try_.body);
            free(s->u.try_.err_name);
            free_stmt(s->u.try_.catch_body);
            free_stmt(s->u.try_.finally_body);
            break;
        case STMT_THROW:
            free_expr(s->u.throw_.expr);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            /* Govdesi yok: `break_label` HIC tahsis edilmez (findstr
               "break_label" gcl_parser.c -> 0 satir). */
            break;
        default:
            /* Buraya ULASILMAMALIDIR. Bir gun ulasilirsa sessizce sizmasi
               yerine bagirir — sessiz sizinti bu dosyanin en pahali hata
               sinifidir. */
            fprintf(stderr,
                    "internal error: free_stmt does not handle GclStmtKind %d\n",
                    (int)s->kind);
            abort();
    }
    /* parse_stmt `next` ZINCIRI uretir (`typedef struct {...} X;` → typedef +
       struct-decl; `global a, b;` → var_decl listesi). gcl_parse bu zinciri
       PARCALAR: her dugumu prog->stmts'e AYRI girer ve next'i NULL yapar
       (bkz. gcl_parse). Bu yuzden prog->stmts'teki dugumler icin bu satir
       ETKISIZDIR — orada zaten sizinti yoktu.
       Etkili oldugu tek yer: zincirin TAMAMEN atildigi yol. switch govdesinde
       case/default DISINDA kalan ifade `free_stmt(cs)` ile atilir ve `cs` bir
       zincirin basi OLABILIR; devami + tum alt tahsisleri (isimler, ifade
       agaclari) yalnizca bu dolasma sayesinde serbest kalir. Tek dugum
       serbest birakmak zincirin kalanini sizinti olarak birakirdi.
       (Zinciri yalnizca parse_stmt kurar; ic ice ifadeler dizi/isaretci ile
       tutulur ve `next` kullanmaz, bu yuzden burada dolasmak guvenli.) */
    free_stmt(s->next);
    free(s);
}

void gcl_program_free(GclProgram *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (!s) continue;
        free_stmt(s);
    }
    free(prog->stmts);
    free(prog);
}
