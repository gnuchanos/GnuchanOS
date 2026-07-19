#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include "gcl.h"

typedef struct {
    TokenType type;
    char      text[256];
    int       ival;
    SourceLoc loc;
} Token;

typedef struct {
    const char *pos, *start, *filename;
    int line, col;
    Token cur, prev;
    DebugFlags debug;
    char text[256];
} Lexer;

void  lexer_init(Lexer *l, const char *src, const char *fname, DebugFlags d);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
Token lexer_advance(Lexer *l);
int   lexer_match(Lexer *l, TokenType t);
void  lexer_dump(Lexer *l);
const char *token_name(TokenType t);

#endif
