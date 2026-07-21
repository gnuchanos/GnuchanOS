#ifndef GCL_PARSER_H
#define GCL_PARSER_H

#include "types.h"
#include "lexer.h"

typedef struct {
    Lexer    *lexer;
    AstNode  *program;
} Parser;

Parser *parser_new(Lexer *l);
AstNode *parser_parse(Parser *p);

#endif
