/*
 * gcl_lexer.h — GCL token types.
 */

#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
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

/* Splits the source into tokens (the token list owns them; lexemes point into
   the source). Success: token list, error: NULL. */
GclTokenList *gcl_lex(const char *src, size_t len, char **error_msg);
void gcl_token_free(GclTokenList *list);

#endif /* GCL_LEXER_H */
