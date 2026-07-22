#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include "types.h"

typedef struct {
    const char *source;
    const char *filename;
    size_t      pos;
    size_t      line;
    size_t      col;
    Token       current;
    int         last_was_directive;  /* set when #include/#lib/#extern was just emitted */
    int         in_condition_context;/* inside #if/#elif: / and , are separators */
} Lexer;

void lexer_init(Lexer *l, const char *source, const char *filename);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);

#endif
