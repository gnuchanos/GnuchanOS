/*
 * gcl_lexer.c — GCL tokenizer.
 *
 * Supports the syntax in simple_doc.md:
 *   type variable = 30;  int8/float16/gcl types, printf("{}", ...), scanf(),
 *   #new/#include, if/else/switch/for/while, struct/enum/typedef, &&/||, ++, +=, ...
 */

/*
 * strdup() is POSIX.1-2008, not C99. Under -std=c99 glibc declares it only when
 * a feature-test macro asks for POSIX; otherwise the compiler falls back to an
 * implicit declaration whose int return TRUNCATES the pointer on LP64 — every
 * duplicated string is silently corrupted (the headless parser test segfaulted
 * exactly this way when it was compiled without the build's -D flag).
 * Requesting POSIX here keeps this translation unit self-sufficient whatever
 * flags the build passes; a command-line -D_POSIX_C_SOURCE=200809L still wins
 * because the guard below then skips the define.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "gcl_lexer.h"
#include "gcl_diag.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int col;
    GclDiagList *diags;   /* rich-diagnostic sink; may be NULL */
    int failed;           /* the first error stops the scan (no cascade noise) */
} Lexer;

/* ---------- token type names ---------- */

const char *gcl_token_type_name(GclTokenType t) {
    switch (t) {
        case TOK_EOF:        return "end of file";
        case TOK_IDENT:      return "identifier";
        case TOK_NUMBER:     return "number";
        case TOK_STRING:     return "string";
        case TOK_CHAR:       return "character";
        case TOK_KEYWORD:    return "keyword";
        case TOK_INT:        return "integer";
        case TOK_FLOAT:      return "float";
        case TOK_TYPE_NAME:  return "type name";
        case TOK_LPAREN:     return "(";
        case TOK_RPAREN:     return ")";
        case TOK_LBRACE:     return "{";
        case TOK_RBRACE:     return "}";
        case TOK_LBRACKET:   return "[";
        case TOK_RBRACKET:   return "]";
        case TOK_COMMA:      return ", (comma)";
        case TOK_SEMI:       return "; (semicolon)";
        case TOK_DOT:        return ". (dot)";
        case TOK_COLON:      return ": (colon)";
        case TOK_QUESTION:   return "? (question mark)";
        case TOK_PLUS:       return "+ (plus)";
        case TOK_MINUS:      return "- (minus)";
        case TOK_STAR:       return "* (star)";
        case TOK_SLASH:      return "/ (slash)";
        case TOK_PERCENT:    return "% (percent)";
        case TOK_PLUSPLUS:   return "operator '++'";
        case TOK_MINUSMINUS: return "operator '--'";
        case TOK_PLUSEQ:     return "operator '+='";
        case TOK_MINUSEQ:    return "operator '-='";
        case TOK_STAREQ:     return "operator '*='";
        case TOK_SLASHEQ:    return "operator '/='";
        case TOK_PERCENTEQ:  return "operator '%='";
        case TOK_ANDEQ:      return "operator '&='";
        case TOK_OREQ:       return "operator '|='";
        case TOK_XOREQ:      return "operator '^='";
        case TOK_ASSIGN:     return "'='";
        case TOK_EQ:         return "operator '=='";
        case TOK_NE:         return "operator '!='";
        case TOK_LT:         return "operator '<'";
        case TOK_GT:         return "operator '>'";
        case TOK_LE:         return "operator '<='";
        case TOK_GE:         return "operator '>='";
        case TOK_AND:        return "operator '&'";
        case TOK_OR:         return "operator '|'";
        case TOK_XOR:        return "operator '^'";
        case TOK_NOT:        return "operator '!'";
        case TOK_TILDE:      return "operator '~'";
        case TOK_SHL:        return "operator '<<'";
        case TOK_SHR:        return "operator '>>'";
        case TOK_AMPAMP:     return "operator '&&'";
        case TOK_PIPEPIPE:   return "operator '||'";
        case TOK_HASH:       return "operator '#'";
        case TOK_AT:         return "operator '@'";
        case TOK_PREPROC:    return "preprocessor line";
        default:             return "token";
    }
}

/* ---------- diagnostics ---------- */

static GclSpan span_at(int line, int col, int len) {
    GclSpan s;
    s.line = line;
    s.col = col;
    s.len = len;
    return s;
}

/* Report the FIRST lexical error and stop the scan. Reporting cascades after a
   lexer error produces noise, not information. Returns the new diagnostic (or
   NULL when the sink is absent / already failed). */
static GclDiag *lex_error(Lexer *lx, GclDiagCode code, GclSpan span,
                          const char *fmt, ...) {
    if (lx->failed) return NULL;
    lx->failed = 1;
    if (!lx->diags) return NULL;
    char stack[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    return gcl_diag_add(lx->diags, code, GCL_SEV_FATAL, span, "%s", stack);
}

/* GCL keyword list (simple_doc.md) */
static const char *g_keywords[] = {
    "if","else","while","for","switch","case","default","break","continue",
    "return","struct","enum","typedef","const","public","private",
    "global","local","inline","sizeof","strlen",
    "true","false","null",
    /* Hata yakalama: try/catch/finally/throw (raise = throw takma adi). */
    "try","catch","finally","throw","raise",
    /* Temizlik: `defer <stmt>;` govde blogu kapaninca (LIFO) calisir. */
    "defer",
    NULL
};

static const char *g_type_names[] = {
    "int8","int16","int32","int64","int128",
    "float16","float32","float64","float128",
    "uint8","uint16","uint32","uint64","uint128",
    "char","short","int","long","float","double","unsigned","void","bool",
    "gcChar",
    NULL
};

static int push_token(GclTokenList *list, GclTokenType type, const char *lex, size_t len, int line, int col, double num) {
    if (list->count >= list->cap) {
        int nc = list->cap ? list->cap * 2 : 64;
        GclToken *nt = (GclToken *)realloc(list->tokens, (size_t)nc * sizeof(GclToken));
        if (!nt) return -1;
        list->tokens = nt;
        list->cap = nc;
    }
    GclToken *t = &list->tokens[list->count++];
    t->type = type;
    t->lexeme = lex;
    t->len = len;
    t->line = line;
    t->col = col;
    t->num = num;
    return 0;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* ---------- the scan ---------- */

/* The single implementation. `diags` may be NULL: then no diagnostic is
   recorded and success/failure is signalled only by the return value. */
static GclTokenList *lex_scan(const char *src, size_t len, GclDiagList *diags) {
    if (!src) return NULL;

    GclTokenList *list = (GclTokenList *)calloc(1, sizeof(GclTokenList));
    if (!list) return NULL;

    Lexer lx;
    lx.src = src;
    lx.len = len;
    lx.pos = 0;
    lx.line = 1;
    lx.col = 1;
    lx.diags = diags;
    lx.failed = 0;

    while (lx.pos < lx.len) {
        char c = src[lx.pos];

        /* whitespace */
        if (isspace((unsigned char)c)) {
            if (c == '\n') { lx.line++; lx.col = 1; }
            else lx.col++;
            lx.pos++;
            continue;
        }

        /* comments: // and star-block */
        if (c == '/' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '/') {
            while (lx.pos < lx.len && src[lx.pos] != '\n') { lx.pos++; lx.col++; }
            continue;
        }
        if (c == '/' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '*') {
            int sline = lx.line, scol = lx.col;
            lx.pos += 2; lx.col += 2;
            while (lx.pos + 1 < lx.len && !(src[lx.pos] == '*' && src[lx.pos + 1] == '/')) {
                if (src[lx.pos] == '\n') { lx.line++; lx.col = 1; }
                else lx.col++;
                lx.pos++;
            }
            if (lx.pos + 1 < lx.len) { lx.pos += 2; lx.col += 2; }
            else {
                /* The comment swallowed the rest of the file. Point at the opening
                   delimiter: pointing at the end of the file told the user nothing. */
                GclDiag *d = lex_error(&lx, GCL_E_LEX_UNTERMINATED_COMMENT,
                                       span_at(sline, scol, 2),
                                       "the '/*' block comment is never closed");
                if (d) {
                    gcl_diag_hint(d, "add '*/' to close it; a block comment does not "
                                     "nest, so the first '*/' ends it");
                }
                break;
            }
            continue;
        }

        /* string "..." or character '...' */
        if (c == '"' || c == '\'') {
            char quote = c;
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            int content = 0;                      /* decoded character count */
            int bad_escape_line = 0, bad_escape_col = 0;
            char bad_escape = 0;
            lx.pos++; lx.col++;
            while (lx.pos < lx.len && src[lx.pos] != quote) {
                if (src[lx.pos] == '\\' && lx.pos + 1 < lx.len) {
                    char e = src[lx.pos + 1];
                    if (!bad_escape && !strchr("nrt\\'\"0abfv?", e)) {
                        bad_escape = e;
                        bad_escape_line = lx.line;
                        bad_escape_col = lx.col;
                    }
                    lx.pos += 2; lx.col += 2; content++;
                    continue;
                }
                if (src[lx.pos] == '\n') {
                    /* A literal never continues across a line break. Swallowing the
                       newline is what turned a one-character typo into an
                       "unterminated string" pointing at the END OF THE FILE. */
                    GclDiag *d = lex_error(&lx,
                        quote == '"' ? GCL_E_LEX_UNTERMINATED_STRING
                                     : GCL_E_LEX_UNTERMINATED_CHAR,
                        span_at(sline, scol, 1),
                        "%s is never closed before the end of the line",
                        quote == '"' ? "string literal" : "character literal");
                    if (d) {
                        gcl_diag_hint(d, "add the closing '%c' on this line; a literal "
                                         "cannot continue on the next line", quote);
                    }
                    break;
                }
                lx.col++;
                lx.pos++;
                content++;
            }
            if (lx.failed) break;
            if (lx.pos >= lx.len) {
                GclDiag *d = lex_error(&lx,
                    quote == '"' ? GCL_E_LEX_UNTERMINATED_STRING
                                 : GCL_E_LEX_UNTERMINATED_CHAR,
                    span_at(sline, scol, 1),
                    quote == '"' ? "string literal is never closed"
                                 : "character literal is never closed");
                if (d) gcl_diag_hint(d, "add the closing '%c' to end it", quote);
                break;
            }
            lx.pos++; lx.col++;

            if (bad_escape) {
                GclDiag *d = lex_error(&lx, GCL_E_LEX_BAD_ESCAPE,
                                       span_at(bad_escape_line, bad_escape_col, 2),
                                       "unknown escape sequence '\\%c' in the %s",
                                       bad_escape,
                                       quote == '"' ? "string" : "character literal");
                if (d) {
                    gcl_diag_hint(d, "valid escapes are \\n \\r \\t \\\\ \\' \\\" \\0 "
                                     "\\a \\b \\f \\v \\?");
                }
                break;
            }
            if (quote == '\'') {
                if (content == 0) {
                    GclDiag *d = lex_error(&lx, GCL_E_LEX_EMPTY_CHAR,
                                           span_at(sline, scol, 2),
                                           "empty character literal ''");
                    if (d) gcl_diag_hint(d, "a character literal holds exactly one "
                                            "character, e.g. 'A' or '\\n'");
                    break;
                }
                if (content > 1) {
                    GclDiag *d = lex_error(&lx, GCL_E_LEX_MULTI_CHAR,
                                           span_at(sline, scol, (int)(src + lx.pos - start)),
                                           "character literal holds %d characters, "
                                           "but only one fits", content);
                    if (d) gcl_diag_hint(d, "use double quotes for text");
                    break;
                }
            }
            /* B29: 'A' bir STRING degil, SAYISAL karakter kodudur. Ayri bir
               token uretilir; parser bunu AST_EXPR_FLOAT'a cevirir (65).
               Aksi halde `'A'+1` = 1, `c == 'A'` = 0, `'A'*2` = 0 gibi SESSIZ
               yanlis sonuclar cikiyordu (string olarak islendigi icin). */
            push_token(list, quote == '\'' ? TOK_CHAR : TOK_STRING,
                       start, (size_t)(src + lx.pos - start), sline, scol, 0);
            continue;
        }

        /* number */
        if (isdigit((unsigned char)c)) {
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            int is_hex = 0;
            int dots = 0;
            /* 0x/0X prefix: a hexadecimal literal. */
            int has_hex_prefix = (c == '0' && lx.pos + 1 < lx.len &&
                                  (src[lx.pos + 1] == 'x' || src[lx.pos + 1] == 'X'));
            if (has_hex_prefix) {
                char xch = src[lx.pos + 1];
                is_hex = 1;
                lx.pos += 2; lx.col += 2;
                size_t digits_at = lx.pos;
                while (lx.pos < lx.len && isxdigit((unsigned char)src[lx.pos])) {
                    lx.pos++; lx.col++;
                }
                if (lx.pos == digits_at) {
                    /* "0x" with no digits used to become the number 0 — silently.
                       The user meant a hexadecimal literal and mistyped it. */
                    GclDiag *d = lex_error(&lx, GCL_E_LEX_BAD_NUMBER,
                                           span_at(sline, scol, 2),
                                           "'0%c' starts a hexadecimal literal, but no "
                                           "hexadecimal digit follows", xch);
                    if (d) gcl_diag_hint(d, "write the value in hex, e.g. 0x1F, or drop "
                                            "the '0%c' prefix for a decimal literal", xch);
                    break;
                }
            } else {
                while (lx.pos < lx.len && (isdigit((unsigned char)src[lx.pos]) ||
                                           src[lx.pos] == '.')) {
                    if (src[lx.pos] == '.') dots++;
                    lx.pos++; lx.col++;
                }
                if (dots > 1) {
                    GclDiag *d = lex_error(&lx, GCL_E_LEX_BAD_NUMBER,
                                           span_at(sline, scol, (int)(src + lx.pos - start)),
                                           "numeric literal has %d '.' characters", dots);
                    if (d) gcl_diag_hint(d, "a number takes a single decimal point, "
                                            "e.g. 1.5");
                    break;
                }
                /* Exponent (C99 decimal floating point): 1e30, 1.5E-3, 2e+8.
                   Consumed ONLY when a digit really follows the optional sign, so
                   a token such as `1else` still falls through to the
                   "immediately followed by ..." check below and keeps its own
                   diagnostic instead of being read as a truncated exponent. */
                if (lx.pos < lx.len && (src[lx.pos] == 'e' || src[lx.pos] == 'E')) {
                    size_t exp = lx.pos + 1;
                    if (exp < lx.len && (src[exp] == '+' || src[exp] == '-')) exp++;
                    if (exp < lx.len && isdigit((unsigned char)src[exp])) {
                        lx.col += (int)(exp - lx.pos);   /* the 'e' and any sign */
                        lx.pos = exp;
                        while (lx.pos < lx.len && isdigit((unsigned char)src[lx.pos])) {
                            lx.pos++; lx.col++;
                        }
                    }
                }
            }
            /* suffix: L, LL, U, F */
            while (lx.pos < lx.len && (src[lx.pos] == 'l' || src[lx.pos] == 'L' ||
                   src[lx.pos] == 'u' || src[lx.pos] == 'U' || src[lx.pos] == 'f' || src[lx.pos] == 'F')) {
                lx.pos++; lx.col++;
            }
            /* A letter welded to the digits means an unsupported form ("12abc",
               "0b101", "3xyz"). Without this check the number and the identifier
               silently became TWO tokens and the parser blamed something else.
               A well-formed exponent such as `1e30` was consumed above, so it
               never reaches this branch. */
            if (lx.pos < lx.len && is_ident_start(src[lx.pos])) {
                GclDiag *d = lex_error(&lx, GCL_E_LEX_BAD_NUMBER,
                                       span_at(sline, scol, (int)(src + lx.pos - start)),
                                       "numeric literal is immediately followed by "
                                       "'%c'", src[lx.pos]);
                if (d) gcl_diag_hint(d, "separate the number from the name: a literal may "
                                        "end with a suffix (l, L, u, U, f, F) or carry an "
                                        "exponent, e.g. 1e30");
                break;
            }
            double num = is_hex ? (double)strtoll(start, NULL, 16) : strtod(start, NULL);
            push_token(list, TOK_NUMBER, start, (size_t)(src + lx.pos - start), sline, scol, num);
            continue;
        }

        /* identifier / keyword / type */
        if (is_ident_start(c)) {
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            while (lx.pos < lx.len && is_ident(src[lx.pos])) { lx.pos++; lx.col++; }
            size_t wl = (size_t)(src + lx.pos - start);
            char word[256];
            size_t cp = wl < sizeof(word) - 1 ? wl : sizeof(word) - 1;
            memcpy(word, start, cp); word[cp] = '\0';

            GclTokenType tt = TOK_IDENT;
            for (int i = 0; g_keywords[i]; i++) {
                if (strcmp(word, g_keywords[i]) == 0) { tt = TOK_KEYWORD; break; }
            }
            if (tt == TOK_IDENT) {
                for (int i = 0; g_type_names[i]; i++) {
                    if (strcmp(word, g_type_names[i]) == 0) { tt = TOK_TYPE_NAME; break; }
                }
            }
            push_token(list, tt, start, wl, sline, scol, 0);
            continue;
        }

        /* GCL special multi-line comment: #| ... |# — everything inside is skipped.
           Unlike C comments (slash-star ... star-slash), this is GCL's own comment form. */
        if (c == '#' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '|') {
            int sline = lx.line, scol = lx.col;
            lx.pos += 2; lx.col += 2;
            while (lx.pos + 1 < lx.len && !(src[lx.pos] == '|' && src[lx.pos + 1] == '#')) {
                if (src[lx.pos] == '\n') { lx.line++; lx.col = 1; }
                else lx.col++;
                lx.pos++;
            }
            if (lx.pos + 1 < lx.len) { lx.pos += 2; lx.col += 2; }
            else {
                GclDiag *d = lex_error(&lx, GCL_E_LEX_UNTERMINATED_HASHCOM,
                                       span_at(sline, scol, 2),
                                       "the '#|' block comment is never closed with '|#'");
                if (d) gcl_diag_hint(d, "close it with '|#', or use '//' / slash-star "
                                        "for a regular comment");
                break;
            }
            continue;
        }

        /* preprocessor: #define, #include, #if, ... — up to the end of the line */
        if (c == '#') {
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            while (lx.pos < lx.len && src[lx.pos] != '\n') { lx.pos++; lx.col++; }
            push_token(list, TOK_PREPROC, start, (size_t)(src + lx.pos - start), sline, scol, 0);
            continue;
        }

        /* operators/punctuation */
        switch (c) {
            case '(': push_token(list, TOK_LPAREN, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case ')': push_token(list, TOK_RPAREN, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '{': push_token(list, TOK_LBRACE, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '}': push_token(list, TOK_RBRACE, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '[': push_token(list, TOK_LBRACKET, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case ']': push_token(list, TOK_RBRACKET, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case ',': push_token(list, TOK_COMMA, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case ';': push_token(list, TOK_SEMI, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '.': push_token(list, TOK_DOT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case ':': push_token(list, TOK_COLON, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '?': push_token(list, TOK_QUESTION, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '@': push_token(list, TOK_AT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '&':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '&') { push_token(list, TOK_AMPAMP, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_ANDEQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_AND, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '|':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '|') { push_token(list, TOK_PIPEPIPE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_OREQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_OR, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '^':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_XOREQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_XOR, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '~': push_token(list, TOK_TILDE, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
            case '+':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '+') { push_token(list, TOK_PLUSPLUS, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_PLUSEQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_PLUS, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '-':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '-') { push_token(list, TOK_MINUSMINUS, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_MINUSEQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_MINUS, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '*':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_STAREQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_STAR, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '/':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_SLASHEQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_SLASH, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '%':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_PERCENTEQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_PERCENT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '=':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_EQ, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '>') { push_token(list, TOK_GE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; } /* => greater equal (simple_doc) */
                else { push_token(list, TOK_ASSIGN, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '!':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_NE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_NOT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '<':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_LE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '<') { push_token(list, TOK_SHL, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_LT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '>':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '=') { push_token(list, TOK_GE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else if (lx.pos + 1 < lx.len && src[lx.pos+1] == '>') { push_token(list, TOK_SHR, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_GT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            default: {
                /* Name the character. "unexpected character" alone forced the user
                   to hunt for it; a non-printable byte is shown by its hex value. */
                GclSpan sp = span_at(lx.line, lx.col, 1);
                unsigned char uc = (unsigned char)c;
                GclDiag *d;
                if (uc >= 0x20 && uc < 0x7f)
                    d = lex_error(&lx, GCL_E_LEX_UNEXPECTED_CHAR, sp,
                                  "unexpected character '%c' in the source", c);
                else
                    d = lex_error(&lx, GCL_E_LEX_UNEXPECTED_CHAR, sp,
                                  "unexpected byte 0x%02X in the source", (unsigned)uc);
                if (d) {
                    gcl_diag_hint(d, "GCL has no syntax element for this character; "
                                     "remove it, or escape it inside a string literal");
                }
                lx.pos++;
                break;
            }
        }
    }

    push_token(list, TOK_EOF, src + lx.len, 0, lx.line, lx.col, 0);

    if (lx.failed) {
        gcl_token_free(list);
        return NULL;
    }
    return list;
}

/* ---------- entry points ---------- */

GclTokenList *gcl_lex_diag(const char *src, size_t len, struct GclDiagList *diags) {
    return lex_scan(src, len, (GclDiagList *)diags);
}

/* Deprecated flat-string API. Kept so existing callers (and the headless parser
   test) keep compiling; internally it runs the same scan and flattens the first
   diagnostic into the historical "<message> at <line>:<col>" shape. */
GclTokenList *gcl_lex(const char *src, size_t len, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    GclDiagList dl;
    gcl_diag_list_init(&dl, NULL);
    GclTokenList *list = lex_scan(src, len, &dl);
    if (!list && error_msg) *error_msg = gcl_diag_first_to_legacy_string(&dl);
    gcl_diag_list_free(&dl);
    return list;
}

void gcl_token_free(GclTokenList *list) {
    if (!list) return;
    free(list->tokens);
    free(list);
}
