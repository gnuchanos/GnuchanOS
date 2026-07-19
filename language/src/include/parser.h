#ifndef GCL_PARSER_H
#define GCL_PARSER_H

#include "gcl.h"
#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer      *lexer;
    const char *source;
    DebugFlags  debug;
    int         errors;
} Parser;

void  parser_init(Parser *p, Lexer *lx, const char *src, DebugFlags d);
Node *parser_parse(Parser *p);
void  parser_dump(Node *n, int indent);

#endif
