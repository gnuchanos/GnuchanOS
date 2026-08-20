/*
 * gcl_token.h — Token types for GCL lexer
 */

#ifndef GCL_TOKEN_H
#define GCL_TOKEN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    /* Literals */
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_CHAR_LIT,

    /* Identifiers & keywords */
    TOK_IDENT,
    TOK_KW_INT, TOK_KW_INT8, TOK_KW_INT16, TOK_KW_INT32, TOK_KW_INT64, TOK_KW_INT128,
    TOK_KW_UINT8, TOK_KW_UINT16, TOK_KW_UINT32, TOK_KW_UINT64, TOK_KW_UINT128,
    TOK_KW_FLOAT, TOK_KW_FLOAT16, TOK_KW_FLOAT32, TOK_KW_FLOAT64, TOK_KW_FLOAT128,
    TOK_KW_DOUBLE, TOK_KW_LONG, TOK_KW_SHORT, TOK_KW_UNSIGNED,
    TOK_KW_CHAR, TOK_KW_GCCHAR, TOK_KW_BOOL,
    TOK_KW_VOID, TOK_KW_NULL,
    TOK_KW_CONST, TOK_KW_INLINE, TOK_KW_GLOBAL,
    TOK_KW_PUBLIC, TOK_KW_PRIVATE,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_ELIF,
    TOK_KW_FOR, TOK_KW_WHILE, TOK_KW_DO,
    TOK_KW_SWITCH, TOK_KW_CASE, TOK_KW_DEFAULT,
    TOK_KW_BREAK, TOK_KW_CONTINUE, TOK_KW_RETURN,
    TOK_KW_STRUCT, TOK_KW_ENUM, TOK_KW_TYPEDEF,
    TOK_KW_CLASS, TOK_KW_TUPLE, TOK_KW_DICT,
    TOK_KW_SIZEOF, TOK_KW_MALLOC, TOK_KW_FREE, TOK_KW_GCMALLOC,
    TOK_KW_PRINTF, TOK_KW_SCANF,
    TOK_KW_EXTERN,

    /* Preprocessor */
    TOK_PP_INCLUDE, TOK_PP_LIB, TOK_PP_EXTERN,
    TOK_PP_REGISTER, TOK_PP_DEFINE, TOK_PP_UNDEF,
    TOK_PP_IFDEF, TOK_PP_IFNDEF, TOK_PP_IF, TOK_PP_ELIF_PP, TOK_PP_ELSE, TOK_PP_ENDIF,
    TOK_PP_WARNING, TOK_PP_ERROR, TOK_PP_DEBUG,

    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_PLUSPLUS, TOK_MINUSMINUS,
    TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ,
    TOK_EQ, TOK_EQEQ, TOK_BANGEQ,
    TOK_LT, TOK_GT, TOK_LTEQ, TOK_GTEQ,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_TILDE,
    TOK_AMPAMP, TOK_PIPEPIPE, TOK_BANG,
    TOK_LSHIFT, TOK_RSHIFT,
    TOK_ARROW, TOK_DOT,

    /* Delimiters */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMICOLON, TOK_COMMA, TOK_COLON,
    TOK_AT,

    /* Special */
    TOK_EOF,
    TOK_NEWLINE,
    TOK_UNKNOWN
} GclTokenKind;

typedef struct {
    GclTokenKind  kind;
    const char   *start;
    size_t        length;
    int           line;
    int           col;
} GclToken;

const char *gcl_token_kind_name(GclTokenKind kind);
void        gcl_token_init(GclToken *tok, GclTokenKind kind, const char *start, size_t length, int line, int col);

#endif /* GCL_TOKEN_H */
