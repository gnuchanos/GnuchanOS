#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

int main() {
    const char *src = "int main() { return 0; }";
    GclLexer lexer;
    lexer_init(&lexer, src);

    printf("Lexing: %s\n\n", src);
    int count = 0;
    for (;;) {
        GclToken tok = lexer_next_token(&lexer);
        if (tok.type == TOK_EOF) { printf("%d: EOF\n", count); break; }
        printf("%d: %s (%.*s)\n", count, token_name(tok.type), tok.length, tok.lexeme);
        count++;
        if (count > 50) { printf("INFINITE LOOP DETECTED\n"); break; }
    }
    return 0;
}
