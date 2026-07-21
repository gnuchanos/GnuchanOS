/* token.h — GCL preprocessor tokens only */
#ifndef GCL_TOKEN_H
#define GCL_TOKEN_H
#include "gcl.h"

typedef enum {
    TOK_EOF,
    TOK_TEXT,       /* passthrough line */
    TOK_PP_INCLUDE, TOK_PP_LIB, TOK_PP_EXTERN,
    TOK_PP_DEFINE, TOK_PP_UNDEF,
    TOK_PP_IFDEF, TOK_PP_IFNDEF, TOK_PP_IF, TOK_PP_ELIF,
    TOK_PP_ELSE, TOK_PP_ENDIF,
    TOK_PP_ERROR, TOK_PP_PRAGMA, TOK_PP_LINE,
} TokenType;

typedef struct {
    TokenType type;
    const char *text;   /* raw line content */
    int len;
    int line;
} Token;

typedef struct {
    Token *tokens;
    int    count;
    int    cap;
} TokenCtx;

const char *tok_name(TokenType t);
#endif
