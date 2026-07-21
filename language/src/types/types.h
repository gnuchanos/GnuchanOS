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
    TOK_HASH_INCLUDE,
    TOK_HASH_LIB,
    TOK_HASH_EXTERN,
    TOK_HASH_DEBUG,
    TOK_HASH_DEFINE,
    TOK_EXTERN_C_OPEN,
    TOK_EXTERN_C_CLOSE,
    TOK_DOT,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_NEWLINE,
    TOK_COMMENT,
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
    NODE_IDENT,
    NODE_STRING,
    NODE_NUMBER,
    NODE_BINARY,
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
