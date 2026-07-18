#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"
#include "ast.h"

typedef struct {
    Lexer lexer;
    int error_count;
} Parser;

Parser parser_new(const char *src, const char *name);
AstNode *parser_parse(Parser *p);

#endif
