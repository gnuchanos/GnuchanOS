#include "lexer.h"
#include "error.h"
#include <string.h>
#include <ctype.h>

static int is_eof(Lexer *l) { return l->source[l->pos] == '\0'; }

static char peek(Lexer *l, int n) { return l->source[l->pos + n]; }

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else { l->col++; }
    return c;
}

static void skip_ws(Lexer *l) {
    while (!is_eof(l) && (l->source[l->pos] == ' ' || l->source[l->pos] == '\t' || l->source[l->pos] == '\r'))
        advance(l);
}

static Token make_tok(Lexer *l, TokenKind k, const char *s, size_t len) {
    Token t = { .kind = k, .text = s, .len = len, .line = l->line, .col = l->col };
    return t;
}

void lexer_init(Lexer *l, const char *source, const char *filename) {
    l->source = source;
    l->filename = filename;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->current = lexer_next(l);
}

Token lexer_peek(Lexer *l) { return l->current; }

Token lexer_next(Lexer *l) {
    while (!is_eof(l)) {
        skip_ws(l);
        if (is_eof(l)) break;
        char c = l->source[l->pos];

        /* newline — emit TOK_NEWLINE, skip blank */
        if (c == '\n') {
            advance(l);
            return make_tok(l, TOK_NEWLINE, "\n", 1);
        }

        /* // comment */
        if (c == '/' && peek(l, 1) == '/') {
            while (!is_eof(l) && l->source[l->pos] != '\n') advance(l);
            continue;
        }

        /* / * comment */
        if (c == '/' && peek(l, 1) == '*') {
            advance(l); advance(l);
            while (!is_eof(l)) {
                if (l->source[l->pos] == '*' && peek(l, 1) == '/') { advance(l); advance(l); break; }
                advance(l);
            }
            continue;
        }

        /* #| block comment */
        if (c == '#' && peek(l, 1) == '|') {
            advance(l); advance(l);
            while (!is_eof(l)) {
                if (l->source[l->pos] == '|' && peek(l, 1) == '#') { advance(l); advance(l); break; }
                advance(l);
            }
            continue;
        }

        /* # single-line / keyword */
        if (c == '#') {
            if (strncmp(l->source + l->pos, "#include", 8) == 0 && !isalnum((unsigned char)peek(l, 8)) && peek(l, 8) != '_') {
                advance(l);
                for (int i = 0; i < 7 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_INCLUDE, "#include", 8);
            }
            if (strncmp(l->source + l->pos, "#lib", 4) == 0 && !isalnum((unsigned char)peek(l, 4)) && peek(l, 4) != '_') {
                advance(l);
                for (int i = 0; i < 3 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_LIB, "#lib", 4);
            }
            if (strncmp(l->source + l->pos, "#extern", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_EXTERN, "#extern", 7);
            }
            if (strncmp(l->source + l->pos, "#debug", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_DEBUG, "#debug", 6);
            }
            if (strncmp(l->source + l->pos, "#define", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_DEFINE, "#define", 7);
            }
            /* plain # comment */
            while (!is_eof(l) && l->source[l->pos] != '\n') advance(l);
            continue;
        }

        /* string "..." */
        if (c == '"') {
            const char *start = l->source + l->pos;
            advance(l);
            while (!is_eof(l) && l->source[l->pos] != '"') advance(l);
            if (!is_eof(l)) advance(l);
            const char *end = l->source + l->pos;
            return make_tok(l, TOK_STRING, start, end - start);
        }

        /* angle-bracket path <...> (like C #include <file>) */
        if (c == '<') {
            const char *start = l->source + l->pos;
            advance(l);
            while (!is_eof(l) && l->source[l->pos] != '>') advance(l);
            if (!is_eof(l)) advance(l);
            const char *end = l->source + l->pos;
            return make_tok(l, TOK_STRING, start, end - start);
        }

        /* number: integer or float (e.g. 42, 3.14159, .5) */
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)peek(l, 1)))) {
            const char *start = l->source + l->pos;
            /* integer part */
            if (c == '.') {
                advance(l); /* leading dot */
            } else {
                while (isdigit((unsigned char)l->source[l->pos])) advance(l);
            }
            /* fractional part */
            if (l->source[l->pos] == '.' && isdigit((unsigned char)peek(l, 1))) {
                advance(l); /* dot */
                while (isdigit((unsigned char)l->source[l->pos])) advance(l);
            }
            return make_tok(l, TOK_NUMBER, start, l->source + l->pos - start);
        }

        /* identifier */
        if (isalpha((unsigned char)c) || c == '_') {
            const char *start = l->source + l->pos;
            while (isalnum((unsigned char)l->source[l->pos]) || l->source[l->pos] == '_') advance(l);
            size_t len = l->source + l->pos - start;

            if (len == 6 && strncmp(start, "extern", 6) == 0) {
                return make_tok(l, TOK_EXTERN_C_OPEN, start, len);
            }

            return make_tok(l, TOK_IDENT, start, len);
        }

        /* dot */
        if (c == '.') {
            advance(l);
            return make_tok(l, TOK_DOT, ".", 1);
        }

        /* { } */
        if (c == '{') { advance(l); return make_tok(l, TOK_LBRACE, "{", 1); }
        if (c == '}') { advance(l); return make_tok(l, TOK_RBRACE, "}", 1); }

        /* unknown — skip */
        advance(l);
    }
    return make_tok(l, TOK_EOF, "", 0);
}
