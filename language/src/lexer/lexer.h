#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stddef.h>

typedef struct {
    const char *src;     /* kaynağın başlangıcı (satır gösterimi için) */
    const char *pos;
    int line, col;
    Token cur;
    const char *src_name;
    int error_count;
} Lexer;

Lexer lexer_new(const char *src, const char *name);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
void lexer_skip_ws(Lexer *l);

/* Process escape sequences in string literals.
   Returns processed length. If out is non-NULL, writes processed bytes there. */
int process_escapes(const char *src, size_t src_len, char *out, size_t *out_len);

#endif
