#ifndef GCL_PARSE_DIRECTIVE_H
#define GCL_PARSE_DIRECTIVE_H

#include "types.h"
#include "parser.h"

/* Helper: strndup + make_node (shared with parser.c) */
char    *strndup_safe(const char *s, size_t len);
AstNode *make_node(NodeKind kind, const char *val, size_t len, size_t line, size_t col);
Token    eat(Parser *p);
void     skip_newlines(Parser *p);

/* Directive node constructors */
AstNode *parse_include(Parser *p);
AstNode *parse_lib(Parser *p);
AstNode *parse_extern(Parser *p);
AstNode *parse_define(Parser *p);
AstNode *parse_undef(Parser *p);
AstNode *parse_debug(Parser *p);
AstNode *parse_extern_c(Parser *p);
AstNode *parse_pragma(Parser *p);
AstNode *parse_error(Parser *p);
AstNode *parse_message(Parser *p);
AstNode *parse_ifdef(Parser *p);
AstNode *parse_ifndef(Parser *p);
AstNode *parse_if(Parser *p);
AstNode *parse_elif(Parser *p);
AstNode *parse_else(Parser *p);
AstNode *parse_endif(Parser *p);

/* Raw line capture (fallback for non-directive content) */
AstNode *parse_raw_line(Parser *p);

/* Check if an identifier name matches a bare directive keyword */
int bare_directive_dispatch(const char *s, size_t len);

#endif
