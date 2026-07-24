#ifndef GCL_TYPES_H
#define GCL_TYPES_H

#include <stddef.h>

typedef struct {
    const char *filename;
    const char *source;
    size_t      line;
    size_t      col;
    size_t      pos;
} SourceLoc;

typedef enum {
    TOK_EOF,
    TOK_IDENT,
    TOK_STRING,
    TOK_NUMBER,
    TOK_SLASH,
    TOK_COMMA,
    TOK_HASH_INCLUDE,
    TOK_HASH_LIB,
    TOK_HASH_EXTERN,
    TOK_HASH_DEBUG,
    TOK_HASH_DEFINE,
    TOK_HASH_UNDEF,
    TOK_HASH_PRAGMA,
    TOK_HASH_IFDEF,
    TOK_HASH_IFNDEF,
    TOK_HASH_IF,
    TOK_HASH_ELIF,
    TOK_HASH_ELSE,
    TOK_HASH_ENDIF,
    TOK_HASH_ERROR,
    TOK_HASH_MESSAGE,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_LE,
    TOK_GT,
    TOK_GE,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_AMPERSAND,
    TOK_PIPE,
    TOK_PERCENT,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_EXTERN_C_OPEN,
    TOK_DOT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_BRACE_OPEN,
    TOK_BRACE_CLOSE,
    TOK_NEWLINE,
    TOK_HASH_FOR,
    TOK_HASH_ENDFOR,
} TokenKind;

typedef struct {
    TokenKind   kind;
    const char *text;
    size_t      len;
    size_t      line;
    size_t      col;
} Token;

typedef enum {
    NODE_PROGRAM,
    NODE_INCLUDE,
    NODE_LIB,
    NODE_EXTERN,
    NODE_DEBUG,
    NODE_DEFINE,
    NODE_EXTERN_C_BLOCK,
    NODE_PRAGMA,
    NODE_UNDEF,
    NODE_IFDEF,
    NODE_IFNDEF,
    NODE_IF,
    NODE_ELIF,
    NODE_ELSE,
    NODE_ENDIF,
    NODE_ERROR,
    NODE_MESSAGE,
    NODE_IDENT,
    NODE_STRING,
    NODE_NUMBER,
    NODE_BINARY,
    NODE_RAW,
    NODE_FOR,
    NODE_ENDFOR,
} NodeKind;

typedef struct AstNode {
    NodeKind         kind;
    const char      *value;
    size_t           len;
    struct AstNode  *left;
    struct AstNode  *right;
    struct AstNode  *next;
    size_t           line;
    size_t           col;
} AstNode;

#endif
