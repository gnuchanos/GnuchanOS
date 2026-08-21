#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gcl_lexer.h"

/* ── helpers ─────────────────────────────────────── */

static char peek(GclLexer *lex) {
    if (lex->pos >= lex->length) return '\0';
    return lex->source[lex->pos];
}

static char peek_next(GclLexer *lex) {
    if (lex->pos + 1 >= lex->length) return '\0';
    return lex->source[lex->pos + 1];
}

static char advance(GclLexer *lex) {
    char c = lex->source[lex->pos];
    lex->pos++;
    if (c == '\n') { lex->line++; lex->col = 1; }
    else { lex->col++; }
    return c;
}

static void skip_whitespace(GclLexer *lex) {
    while (lex->pos < lex->length) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
        } else {
            break;
        }
    }
}

static void skip_line_comment(GclLexer *lex) {
    /* #// style comment — skip to end of line */
    while (lex->pos < lex->length && peek(lex) != '\n') {
        advance(lex);
    }
}

static void skip_block_comment(GclLexer *lex) {
    /* #| ... |# */
    while (lex->pos < lex->length) {
        if (peek(lex) == '|' && peek_next(lex) == '#') {
            advance(lex);
            advance(lex);
            return;
        }
        advance(lex);
    }
}

static GclToken make_token(GclLexer *lex, GclTokenKind kind, const char *start, size_t len, int line, int col) {
    (void)lex;
    GclToken t;
    gcl_token_init(&t, kind, start, len, line, col);
    return t;
}

/* ── keyword lookup ──────────────────────────────── */

typedef struct { const char *name; GclTokenKind kind; } KwEntry;

static const KwEntry keywords[] = {
    {"int",       TOK_KW_INT},
    {"int8",      TOK_KW_INT8},
    {"int16",     TOK_KW_INT16},
    {"int32",     TOK_KW_INT32},
    {"int64",     TOK_KW_INT64},
    {"int128",    TOK_KW_INT128},
    {"uint8",     TOK_KW_UINT8},
    {"uint16",    TOK_KW_UINT16},
    {"uint32",    TOK_KW_UINT32},
    {"uint64",    TOK_KW_UINT64},
    {"uint128",   TOK_KW_UINT128},
    {"float",     TOK_KW_FLOAT},
    {"float16",   TOK_KW_FLOAT16},
    {"float32",   TOK_KW_FLOAT32},
    {"float64",   TOK_KW_FLOAT64},
    {"float128",  TOK_KW_FLOAT128},
    {"double",    TOK_KW_DOUBLE},
    {"long",      TOK_KW_LONG},
    {"short",     TOK_KW_SHORT},
    {"unsigned",  TOK_KW_UNSIGNED},
    {"char",      TOK_KW_CHAR},
    {"gcChar",    TOK_KW_GCCHAR},
    {"bool",      TOK_KW_BOOL},
    {"void",      TOK_KW_VOID},
    {"null",      TOK_KW_NULL},
    {"NULL",      TOK_KW_NULL},
    {"const",     TOK_KW_CONST},
    {"inline",    TOK_KW_INLINE},
    {"global",    TOK_KW_GLOBAL},
    {"public",    TOK_KW_PUBLIC},
    {"private",   TOK_KW_PRIVATE},
    {"if",        TOK_KW_IF},
    {"else",      TOK_KW_ELSE},
    {"elif",      TOK_KW_ELIF},
    {"for",       TOK_KW_FOR},
    {"while",     TOK_KW_WHILE},
    {"do",        TOK_KW_DO},
    {"switch",    TOK_KW_SWITCH},
    {"case",      TOK_KW_CASE},
    {"default",   TOK_KW_DEFAULT},
    {"break",     TOK_KW_BREAK},
    {"continue",  TOK_KW_CONTINUE},
    {"return",    TOK_KW_RETURN},
    {"struct",    TOK_KW_STRUCT},
    {"enum",      TOK_KW_ENUM},
    {"typedef",   TOK_KW_TYPEDEF},
    {"class",     TOK_KW_CLASS},
    {"tuple",     TOK_KW_TUPLE},
    {"dict",      TOK_KW_DICT},
    {"sizeof",    TOK_KW_SIZEOF},
    {"malloc",    TOK_KW_MALLOC},
    {"free",      TOK_KW_FREE},
    {"gcMalloc",  TOK_KW_GCMALLOC},
    {"printf",    TOK_KW_PRINTF},
    {"scanf",     TOK_KW_SCANF},
    {"extern",    TOK_KW_EXTERN},
    {"true",      TOK_KW_BOOL},
    {"false",     TOK_KW_BOOL},
    {NULL, TOK_UNKNOWN}
};

static GclTokenKind lookup_keyword(const char *str, size_t len) {
    for (int i = 0; keywords[i].name != NULL; i++) {
        if (strlen(keywords[i].name) == len &&
            memcmp(keywords[i].name, str, len) == 0) {
            return keywords[i].kind;
        }
    }
    return TOK_IDENT;
}

/* ── preprocessor directive lookup ───────────────── */

typedef struct { const char *name; GclTokenKind kind; } PpEntry;

static const PpEntry pp_directives[] = {
    {"include",  TOK_PP_INCLUDE},
    {"extern",   TOK_PP_EXTERN},
    {"register", TOK_PP_REGISTER},
    {"define",   TOK_PP_DEFINE},
    {"undef",    TOK_PP_UNDEF},
    {"ifdef",    TOK_PP_IFDEF},
    {"ifndef",   TOK_PP_IFNDEF},
    {"if",       TOK_PP_IF},
    {"elif",     TOK_PP_ELIF_PP},
    {"else",     TOK_PP_ELSE},
    {"endif",    TOK_PP_ENDIF},
    {"warning",  TOK_PP_WARNING},
    {"error",    TOK_PP_ERROR},
    {"debug",    TOK_PP_DEBUG},
    {NULL, TOK_UNKNOWN}
};

static GclTokenKind lookup_pp(const char *str, size_t len) {
    for (int i = 0; pp_directives[i].name != NULL; i++) {
        if (strlen(pp_directives[i].name) == len &&
            memcmp(pp_directives[i].name, str, len) == 0) {
            return pp_directives[i].kind;
        }
    }
    return TOK_UNKNOWN;
}

/* ── scan number ─────────────────────────────────── */

static GclToken scan_number(GclLexer *lex) {
    const char *start = lex->source + lex->pos;
    int line = lex->line, col = lex->col;
    int is_float = 0;

    while (lex->pos < lex->length && isdigit((unsigned char)peek(lex))) {
        advance(lex);
    }
    if (peek(lex) == '.' && isdigit((unsigned char)peek_next(lex))) {
        is_float = 1;
        advance(lex); /* '.' */
        while (lex->pos < lex->length && isdigit((unsigned char)peek(lex))) {
            advance(lex);
        }
    }
    size_t len = (size_t)(lex->source + lex->pos - start);
    return make_token(lex, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT,
                      start, len, line, col);
}

/* ── scan string ─────────────────────────────────── */

static GclToken scan_string(GclLexer *lex) {
    int line = lex->line, col = lex->col;
    const char *start = lex->source + lex->pos;
    advance(lex); /* opening " */
    while (lex->pos < lex->length && peek(lex) != '"') {
        if (peek(lex) == '\\') advance(lex); /* escape */
        advance(lex);
    }
    if (peek(lex) == '"') advance(lex); /* closing " */
    size_t len = (size_t)(lex->source + lex->pos - start);
    return make_token(lex, TOK_STRING_LIT, start, len, line, col);
}

/* ── scan char literal ───────────────────────────── */

static GclToken scan_char(GclLexer *lex) {
    int line = lex->line, col = lex->col;
    const char *start = lex->source + lex->pos;
    advance(lex); /* opening ' */
    if (peek(lex) == '\\') advance(lex);
    if (lex->pos < lex->length) advance(lex);
    if (peek(lex) == '\'') advance(lex);
    size_t len = (size_t)(lex->source + lex->pos - start);
    return make_token(lex, TOK_CHAR_LIT, start, len, line, col);
}

/* ── scan identifier/keyword ─────────────────────── */

static GclToken scan_ident(GclLexer *lex) {
    int line = lex->line, col = lex->col;
    const char *start = lex->source + lex->pos;
    while (lex->pos < lex->length) {
        char c = peek(lex);
        if (isalnum((unsigned char)c) || c == '_') {
            advance(lex);
        } else {
            break;
        }
    }
    size_t len = (size_t)(lex->source + lex->pos - start);
    GclTokenKind kind = lookup_keyword(start, len);
    return make_token(lex, kind, start, len, line, col);
}

/* ── scan preprocessor ───────────────────────────── */

static GclToken scan_preprocessor(GclLexer *lex) {
    int line = lex->line, col = lex->col;
    const char *start = lex->source + lex->pos;
    advance(lex); /* '#' */

    /* Check for #// comment */
    if (peek(lex) == '/' && peek_next(lex) == '/') {
        skip_line_comment(lex);
        return gcl_lexer_next(lex);
    }
    /* Check for #| block comment */
    if (peek(lex) == '|') {
        advance(lex);
        skip_block_comment(lex);
        return gcl_lexer_next(lex);
    }

    /* Read directive name */
    const char *dir_start = lex->source + lex->pos;
    while (lex->pos < lex->length && isalpha((unsigned char)peek(lex))) {
        advance(lex);
    }
    size_t dir_len = (size_t)(lex->source + lex->pos - dir_start);
    GclTokenKind kind = lookup_pp(dir_start, dir_len);
    if (kind == TOK_UNKNOWN) {
        /* # followed by a non-directive: single-line comment (# comment) */
        skip_line_comment(lex);
        return gcl_lexer_next(lex);
    }
    size_t total_len = (size_t)(lex->source + lex->pos - start);
    return make_token(lex, kind, start, total_len, line, col);
}

/* ── main scan ───────────────────────────────────── */

GclToken gcl_lexer_next(GclLexer *lex) {
    skip_whitespace(lex);

    if (lex->pos >= lex->length) {
        return make_token(lex, TOK_EOF, lex->source + lex->pos, 0, lex->line, lex->col);
    }

    char c = peek(lex);
    int line = lex->line, col = lex->col;
    const char *start = lex->source + lex->pos;

    /* Preprocessor directives and comments */
    if (c == '#') {
        return scan_preprocessor(lex);
    }

    /* Numbers */
    if (isdigit((unsigned char)c)) {
        return scan_number(lex);
    }

    /* Strings */
    if (c == '"') {
        return scan_string(lex);
    }

    /* Char literals */
    if (c == '\'') {
        return scan_char(lex);
    }

    /* Identifiers and keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        return scan_ident(lex);
    }

    /* Two-char operators */
    char n = peek_next(lex);
    switch (c) {
    case '+':
        advance(lex);
        if (peek(lex) == '+') { advance(lex); return make_token(lex, TOK_PLUSPLUS, start, 2, line, col); }
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_PLUS_EQ, start, 2, line, col); }
        return make_token(lex, TOK_PLUS, start, 1, line, col);
    case '-':
        advance(lex);
        if (peek(lex) == '-') { advance(lex); return make_token(lex, TOK_MINUSMINUS, start, 2, line, col); }
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_MINUS_EQ, start, 2, line, col); }
        if (peek(lex) == '>') { advance(lex); return make_token(lex, TOK_ARROW, start, 2, line, col); }
        return make_token(lex, TOK_MINUS, start, 1, line, col);
    case '*':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_STAR_EQ, start, 2, line, col); }
        return make_token(lex, TOK_STAR, start, 1, line, col);
    case '/':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_SLASH_EQ, start, 2, line, col); }
        if (peek(lex) == '/') { /* // line comment */ skip_line_comment(lex); return gcl_lexer_next(lex); }
        return make_token(lex, TOK_SLASH, start, 1, line, col);
    case '%':
        advance(lex);
        return make_token(lex, TOK_PERCENT, start, 1, line, col);
    case '=':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_EQEQ, start, 2, line, col); }
        return make_token(lex, TOK_EQ, start, 1, line, col);
    case '!':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_BANGEQ, start, 2, line, col); }
        return make_token(lex, TOK_BANG, start, 1, line, col);
    case '<':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_LTEQ, start, 2, line, col); }
        if (peek(lex) == '<') { advance(lex); return make_token(lex, TOK_LSHIFT, start, 2, line, col); }
        return make_token(lex, TOK_LT, start, 1, line, col);
    case '>':
        advance(lex);
        if (peek(lex) == '=') { advance(lex); return make_token(lex, TOK_GTEQ, start, 2, line, col); }
        if (peek(lex) == '>') { advance(lex); return make_token(lex, TOK_RSHIFT, start, 2, line, col); }
        return make_token(lex, TOK_GT, start, 1, line, col);
    case '&':
        advance(lex);
        if (peek(lex) == '&') { advance(lex); return make_token(lex, TOK_AMPAMP, start, 2, line, col); }
        return make_token(lex, TOK_AMP, start, 1, line, col);
    case '|':
        advance(lex);
        if (peek(lex) == '|') { advance(lex); return make_token(lex, TOK_PIPEPIPE, start, 2, line, col); }
        return make_token(lex, TOK_PIPE, start, 1, line, col);
    case '^':
        advance(lex);
        return make_token(lex, TOK_CARET, start, 1, line, col);
    case '~':
        advance(lex);
        return make_token(lex, TOK_TILDE, start, 1, line, col);
    case '(':
        advance(lex); return make_token(lex, TOK_LPAREN, start, 1, line, col);
    case ')':
        advance(lex); return make_token(lex, TOK_RPAREN, start, 1, line, col);
    case '{':
        advance(lex); return make_token(lex, TOK_LBRACE, start, 1, line, col);
    case '}':
        advance(lex); return make_token(lex, TOK_RBRACE, start, 1, line, col);
    case '[':
        advance(lex); return make_token(lex, TOK_LBRACKET, start, 1, line, col);
    case ']':
        advance(lex); return make_token(lex, TOK_RBRACKET, start, 1, line, col);
    case ';':
        advance(lex); return make_token(lex, TOK_SEMICOLON, start, 1, line, col);
    case ',':
        advance(lex); return make_token(lex, TOK_COMMA, start, 1, line, col);
    case ':':
        advance(lex); return make_token(lex, TOK_COLON, start, 1, line, col);
    case '.':
        advance(lex); return make_token(lex, TOK_DOT, start, 1, line, col);
    case '@':
        advance(lex); return make_token(lex, TOK_AT, start, 1, line, col);
    default:
        break;
    }

    /* Unknown character */
    advance(lex);
    (void)n;
    return make_token(lex, TOK_UNKNOWN, start, 1, line, col);
}

GclToken gcl_lexer_peek(GclLexer *lex) {
    size_t saved_pos = lex->pos;
    int saved_line = lex->line;
    int saved_col = lex->col;
    GclToken tok = gcl_lexer_next(lex);
    lex->pos = saved_pos;
    lex->line = saved_line;
    lex->col = saved_col;
    return tok;
}
