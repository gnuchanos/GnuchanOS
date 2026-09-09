/*
 * gcl_lexer.c — GCL tokenizer.
 *
 * simple_doc.md'deki sözdizimini destekler:
 *   type variable = 30;  int8/float16/gcl tipleri, printf("{}", ...), scanf(),
 *   #new/#include, if/else/switch/for/while, struct/enum/typedef, &&/||, ++, +=, ...
 */

#include "gcl_lexer.h"

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
    char *err;
} Lexer;

/* GCL keyword listesi (simple_doc.md) */
static const char *g_keywords[] = {
    "if","else","while","for","switch","case","default","break","continue",
    "return","struct","enum","typedef","const","public","private",
    "global","local","inline","sizeof","strlen",
    "true","false","null",
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

static void add_err(Lexer *lx, const char *msg) {
    if (lx->err) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s at %d:%d", msg, lx->line, lx->col);
    lx->err = strdup(buf);
}

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

GclTokenList *gcl_lex(const char *src, size_t len, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!src) return NULL;

    GclTokenList *list = (GclTokenList *)calloc(1, sizeof(GclTokenList));
    if (!list) return NULL;

    Lexer lx;
    lx.src = src;
    lx.len = len;
    lx.pos = 0;
    lx.line = 1;
    lx.col = 1;
    lx.err = NULL;

    while (lx.pos < lx.len) {
        char c = src[lx.pos];

        /* whitespace */
        if (isspace((unsigned char)c)) {
            if (c == '\n') { lx.line++; lx.col = 1; }
            else lx.col++;
            lx.pos++;
            continue;
        }

        /* comments: // ve yıldız-blok */
        if (c == '/' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '/') {
            while (lx.pos < lx.len && src[lx.pos] != '\n') { lx.pos++; lx.col++; }
            continue;
        }
        if (c == '/' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '*') {
            lx.pos += 2; lx.col += 2;
            while (lx.pos + 1 < lx.len && !(src[lx.pos] == '*' && src[lx.pos + 1] == '/')) {
                if (src[lx.pos] == '\n') { lx.line++; lx.col = 1; }
                else lx.col++;
                lx.pos++;
            }
            if (lx.pos + 1 < lx.len) { lx.pos += 2; lx.col += 2; }
            continue;
        }

        /* string "..." or '...' */
        if (c == '"' || c == '\'') {
            char quote = c;
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            lx.pos++; lx.col++;
            while (lx.pos < lx.len && src[lx.pos] != quote) {
                if (src[lx.pos] == '\\' && lx.pos + 1 < lx.len) { lx.pos += 2; lx.col += 2; continue; }
                if (src[lx.pos] == '\n') { lx.line++; lx.col = 1; }
                else lx.col++;
                lx.pos++;
            }
            if (lx.pos >= lx.len) { add_err(&lx, "unterminated string"); break; }
            lx.pos++; lx.col++;
            push_token(list, TOK_STRING, start, (size_t)(src + lx.pos - start), sline, scol, 0);
            continue;
        }

        /* number */
        if (isdigit((unsigned char)c)) {
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            /* 0x/0X öneki: arkasında geçerli hex digit varsa hex sayı, yoksa
               yine tek parça olarak tüketilir — "0x" ayrı "0"+"x" token'ı olmaz. */
            int is_hex = 0;
            int has_hex_prefix = (c == '0' && lx.pos + 1 < lx.len &&
                                  (src[lx.pos + 1] == 'x' || src[lx.pos + 1] == 'X'));
            if (has_hex_prefix && lx.pos + 2 < lx.len &&
                isxdigit((unsigned char)src[lx.pos + 2])) {
                is_hex = 1;
            }
            if (has_hex_prefix) {
                /* "0x"/"0X" önekini atla; hex sayısıysa rakamlarını tüket */
                lx.pos += 2; lx.col += 2;
                while (is_hex && lx.pos < lx.len && isxdigit((unsigned char)src[lx.pos])) {
                    lx.pos++; lx.col++;
                }
            } else {
                while (lx.pos < lx.len && (isdigit((unsigned char)src[lx.pos]) || src[lx.pos] == '.')) {
                    lx.pos++; lx.col++;
                }
            }
            /* suffix: L, LL, U, F */
            while (lx.pos < lx.len && (src[lx.pos] == 'l' || src[lx.pos] == 'L' ||
                   src[lx.pos] == 'u' || src[lx.pos] == 'U' || src[lx.pos] == 'f' || src[lx.pos] == 'F')) {
                lx.pos++; lx.col++;
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

        /* GCL özel çok satırlı yorum: #| ... |# — içerideki her şey atlanır.
           C yorumlarından (slash-yıldız ... yıldız-slash) farklı, GCL'nin kendi yorum biçimi. */
        if (c == '#' && lx.pos + 1 < lx.len && src[lx.pos + 1] == '|') {
            const char *start = src + lx.pos;
            int sline = lx.line, scol = lx.col;
            lx.pos += 2; lx.col += 2;
            while (lx.pos + 1 < lx.len && !(src[lx.pos] == '|' && src[lx.pos + 1] == '#')) {
                if (src[lx.pos] == '\n') { lx.line++; lx.col = 1; }
                else lx.col++;
                lx.pos++;
            }
            if (lx.pos + 1 < lx.len) { lx.pos += 2; lx.col += 2; }
            (void)start; (void)sline; (void)scol;
            continue;
        }

        /* preprocessor: #define, #include, #if, ... — satır sonuna kadar */
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
                else { push_token(list, TOK_AND, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '|':
                if (lx.pos + 1 < lx.len && src[lx.pos+1] == '|') { push_token(list, TOK_PIPEPIPE, src+lx.pos, 2, lx.line, lx.col, 0); lx.pos += 2; lx.col += 2; }
                else { push_token(list, TOK_OR, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; }
                break;
            case '^': push_token(list, TOK_XOR, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
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
            case '%': push_token(list, TOK_PERCENT, src+lx.pos, 1, lx.line, lx.col, 0); lx.pos++; lx.col++; break;
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
            default:
                add_err(&lx, "unexpected character");
                lx.pos++;
                break;
        }
    }

    push_token(list, TOK_EOF, src + lx.len, 0, lx.line, lx.col, 0);

    if (lx.err) {
        if (error_msg) *error_msg = lx.err;
        else free(lx.err);
        gcl_token_free(list);
        return NULL;
    }
    return list;
}

void gcl_token_free(GclTokenList *list) {
    if (!list) return;
    free(list->tokens);
    free(list);
}
