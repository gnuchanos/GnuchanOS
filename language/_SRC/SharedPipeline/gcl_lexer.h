/*
 * gcl_lexer.h — GCL token types.
 */

#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include <stddef.h>
#include <stdint.h>

/* The diagnostics sink lives in gcl_diag.h. Declaring the TAG here (at file
   scope) lets this header mention the type without depending on it, and keeps
   the two modules free to include each other in either order. */
struct GclDiagList;

typedef enum {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,      /* 'x' — B29: karakter literali (sayısal kod), string DEĞİL */
    TOK_KEYWORD,
    TOK_INT,
    TOK_FLOAT,
    TOK_TYPE_NAME,

    /* Punctuation */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_SEMI, TOK_DOT,
    TOK_COLON, TOK_QUESTION,
    
    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_PLUSPLUS, TOK_MINUSMINUS,
    TOK_PLUSEQ, TOK_MINUSEQ, TOK_STAREQ, TOK_SLASHEQ,
    TOK_PERCENTEQ, TOK_ANDEQ, TOK_OREQ, TOK_XOREQ,
    TOK_ASSIGN,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR, TOK_XOR, TOK_NOT, TOK_TILDE, TOK_SHL, TOK_SHR,
    TOK_AMPAMP, TOK_PIPEPIPE,
    TOK_HASH,
    TOK_AT,

    /* Preproc */
    TOK_PREPROC,
} GclTokenType;

typedef struct {
    GclTokenType type;
    const char *lexeme;   /* points into the source (strlen kept separately) */
    size_t len;
    int line;
    int col;
    double num;
} GclToken;

typedef struct {
    GclToken *tokens;
    int count;
    int cap;
} GclTokenList;

/* Human-readable name of a token type, used inside diagnostics:
   "identifier", "number", "keyword", "'{'", "end of file", ... */
const char *gcl_token_type_name(GclTokenType t);

/* Splits the source into tokens (the token list owns them; lexemes point into
   the source). Success: token list, error: NULL.
   Deprecated: returns a single flattened message. Use gcl_lex_diag(). */
GclTokenList *gcl_lex(const char *src, size_t len, char **error_msg);

/* Diagnostics-aware lexer. On failure returns NULL and appends rich
   diagnostics (stable code, span, note, hint) to `diags`; `diags` may be NULL.
   `struct GclDiagList` is defined in gcl_diag.h — forward-declared here so
   this header stays free of the diagnostics dependency. */
GclTokenList *gcl_lex_diag(const char *src, size_t len, struct GclDiagList *diags);

void gcl_token_free(GclTokenList *list);

#endif /* GCL_LEXER_H */
