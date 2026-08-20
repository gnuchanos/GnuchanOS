/*
 * gcl_lexer.h — GCL Lexer interface
 */

#ifndef GCL_LEXER_H
#define GCL_LEXER_H

#include "../Common/gcl_common.h"
#include "../Common/gcl_token.h"

typedef struct {
    const char      *source;
    size_t           length;
    size_t           pos;
    int              line;
    int              col;
    GclArena        *arena;
    GclStringIntern *intern;
} GclLexer;

void     gcl_lexer_init(GclLexer *lex, const char *source, GclArena *arena, GclStringIntern *intern);
GclToken gcl_lexer_next(GclLexer *lex);
GclToken gcl_lexer_peek(GclLexer *lex);

#endif /* GCL_LEXER_H */
