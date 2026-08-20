#include <string.h>
#include "gcl_lexer.h"

void gcl_lexer_init(GclLexer *lex, const char *source, GclArena *arena, GclStringIntern *intern) {
    lex->source = source;
    lex->length = source ? strlen(source) : 0;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
    lex->arena = arena;
    lex->intern = intern;
}
