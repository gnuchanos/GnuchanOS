#include "lexer.h"
#include "error.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

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
    l->last_was_directive = 0;
    l->in_condition_context = 0;
    l->current = lexer_next(l);
}

Token lexer_peek(Lexer *l) { return l->current; }

Token lexer_next(Lexer *l) {
    while (!is_eof(l)) {
        skip_ws(l);
        if (is_eof(l)) break;
        char c = l->source[l->pos];

        /* newline — reset all context flags, emit TOK_NEWLINE */
        if (c == '\n') {
            advance(l);
            l->last_was_directive = 0;
            l->in_condition_context = 0;
            return make_tok(l, TOK_NEWLINE, "\n", 1);
        }

        /* condition context: slash as separator, // and block comments to skip */
        if (l->in_condition_context) {
            if (c == ',') { advance(l); return make_tok(l, TOK_COMMA, ",", 1); }
            if (c == '/' && peek(l, 1) == '/') {
                advance(l); advance(l);
                while (!is_eof(l) && l->source[l->pos] != '\n') advance(l);
                continue;
            }
            if (c == '/' && peek(l, 1) == '*') {
                advance(l); advance(l);
                while (!is_eof(l)) {
                    if (l->source[l->pos] == '*' && peek(l, 1) == '/') { advance(l); advance(l); break; }
                    advance(l);
                }
                continue;
            }
            if (c == '/') { advance(l); return make_tok(l, TOK_SLASH, "/", 1); }
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
                l->last_was_directive = 1;
                return make_tok(l, TOK_HASH_INCLUDE, "#include", 8);
            }
            if (strncmp(l->source + l->pos, "#ifndef", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_IFNDEF, "#ifndef", 7);
            }
            if (strncmp(l->source + l->pos, "#ifdef", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_IFDEF, "#ifdef", 6);
            }
            if (strncmp(l->source + l->pos, "#define", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_DEFINE, "#define", 7);
            }
            if (strncmp(l->source + l->pos, "#pragma", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_PRAGMA, "#pragma", 7);
            }
            if (strncmp(l->source + l->pos, "#extern", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                l->last_was_directive = 1;
                return make_tok(l, TOK_HASH_EXTERN, "#extern", 7);
            }
            if (strncmp(l->source + l->pos, "#debug", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_DEBUG, "#debug", 6);
            }
            if (strncmp(l->source + l->pos, "#endif", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_ENDIF, "#endif", 6);
            }
            if (strncmp(l->source + l->pos, "#error", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_ERROR, "#error", 6);
            }
            if (strncmp(l->source + l->pos, "#undef", 6) == 0 && !isalnum((unsigned char)peek(l, 6)) && peek(l, 6) != '_') {
                advance(l);
                for (int i = 0; i < 5 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_UNDEF, "#undef", 6);
            }
            if (strncmp(l->source + l->pos, "#elif", 5) == 0 && !isalnum((unsigned char)peek(l, 5)) && peek(l, 5) != '_') {
                advance(l);
                for (int i = 0; i < 4 && !is_eof(l); i++) advance(l);
                l->in_condition_context = 1;
                return make_tok(l, TOK_HASH_ELIF, "#elif", 5);
            }
            if (strncmp(l->source + l->pos, "#else", 5) == 0 && !isalnum((unsigned char)peek(l, 5)) && peek(l, 5) != '_') {
                advance(l);
                for (int i = 0; i < 4 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_ELSE, "#else", 5);
            }
            if (strncmp(l->source + l->pos, "#lib", 4) == 0 && !isalnum((unsigned char)peek(l, 4)) && peek(l, 4) != '_') {
                advance(l);
                for (int i = 0; i < 3 && !is_eof(l); i++) advance(l);
                l->last_was_directive = 1;
                return make_tok(l, TOK_HASH_LIB, "#lib", 4);
            }
            if (strncmp(l->source + l->pos, "#if", 3) == 0 && !isalnum((unsigned char)peek(l, 3)) && peek(l, 3) != '_') {
                advance(l);
                for (int i = 0; i < 2 && !is_eof(l); i++) advance(l);
                l->in_condition_context = 1;
                return make_tok(l, TOK_HASH_IF, "#if", 3);
            }
            if (strncmp(l->source + l->pos, "#message", 8) == 0 && !isalnum((unsigned char)peek(l, 8)) && peek(l, 8) != '_') {
                advance(l);
                for (int i = 0; i < 7 && !is_eof(l); i++) advance(l);
                return make_tok(l, TOK_HASH_MESSAGE, "#message", 8);
            }
            if (strncmp(l->source + l->pos, "#for", 4) == 0 && !isalnum((unsigned char)peek(l, 4)) && peek(l, 4) != '_') {
                advance(l);
                for (int i = 0; i < 3 && !is_eof(l); i++) advance(l);
                l->in_condition_context = 0;
                return make_tok(l, TOK_HASH_FOR, "#for", 4);
            }
            if (strncmp(l->source + l->pos, "#endfor", 7) == 0 && !isalnum((unsigned char)peek(l, 7)) && peek(l, 7) != '_') {
                advance(l);
                for (int i = 0; i < 6 && !is_eof(l); i++) advance(l);
                l->in_condition_context = 0;
                return make_tok(l, TOK_HASH_ENDFOR, "#endfor", 7);
            }
            /* unknown # directive — skip line (parser will error) */
            while (!is_eof(l) && l->source[l->pos] != '\n') advance(l);
            continue;
        }

        /* string "..." — handles escape sequences like \" */
        if (c == '"') {
            const char *start = l->source + l->pos;
            advance(l);
            while (!is_eof(l) && l->source[l->pos] != '"') {
                if (l->source[l->pos] == '\\' && !is_eof(l)) {
                    advance(l);
                }
                advance(l);
            }
            if (!is_eof(l)) advance(l);
            const char *end = l->source + l->pos;
            l->last_was_directive = 0;
            return make_tok(l, TOK_STRING, start, end - start);
        }

        /* angle-bracket path <...> — only after #include, #lib, #extern */
        if (c == '<') {
            if (l->last_was_directive) {
                const char *start = l->source + l->pos;
                advance(l);
                while (!is_eof(l) && l->source[l->pos] != '>') advance(l);
                if (!is_eof(l)) advance(l);
                const char *end = l->source + l->pos;
                l->last_was_directive = 0;
                return make_tok(l, TOK_STRING, start, end - start);
            }
            advance(l);
            return make_tok(l, TOK_LT, "<", 1);
        }

        /* comparison operators */
        if (c == '!' && peek(l, 1) == '=') { advance(l); advance(l); return make_tok(l, TOK_NE, "!=", 2); }
        if (c == '=' && peek(l, 1) == '=') { advance(l); advance(l); return make_tok(l, TOK_EQ, "==", 2); }
        if (c == '<' && peek(l, 1) == '=') { advance(l); advance(l); return make_tok(l, TOK_LE, "<=", 2); }
        if (c == '>' && peek(l, 1) == '=') { advance(l); advance(l); return make_tok(l, TOK_GE, ">=", 2); }
        if (c == '>') { advance(l); return make_tok(l, TOK_GT, ">", 1); }

        /* number: integer or float (e.g. 42, 3.14159, .5) */
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)peek(l, 1)))) {
            const char *start = l->source + l->pos;
            if (c == '.') {
                advance(l);
            } else {
                while (isdigit((unsigned char)l->source[l->pos])) advance(l);
            }
            if (l->source[l->pos] == '.' && isdigit((unsigned char)peek(l, 1))) {
                advance(l);
                while (isdigit((unsigned char)l->source[l->pos])) advance(l);
            }
            l->last_was_directive = 0;
            return make_tok(l, TOK_NUMBER, start, l->source + l->pos - start);
        }

        /* identifier (plus compound keyword in condition context) */
        if (isalpha((unsigned char)c) || c == '_') {
            const char *start = l->source + l->pos;
            while (isalnum((unsigned char)l->source[l->pos]) || l->source[l->pos] == '_') advance(l);

            /* In condition context: detect compound keywords like "gnu/linux" */
            if (l->in_condition_context) {
                const char *p = l->source + l->pos;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '/') {
                    p++;
                    while (*p == ' ' || *p == '\t') p++;
                    if (isalpha((unsigned char)*p) || *p == '_') {
                        /* yes — compound keyword: consume whitespace, /, whitespace, second ident */
                        while (l->source[l->pos] == ' ' || l->source[l->pos] == '\t') advance(l);
                        advance(l); /* consume / */
                        while (l->source[l->pos] == ' ' || l->source[l->pos] == '\t') advance(l);
                        while (isalnum((unsigned char)l->source[l->pos]) || l->source[l->pos] == '_') advance(l);
                    }
                }
            }

            size_t len = l->source + l->pos - start;

            /* Check for extern "c" { pattern — peek at source ahead */
            if (len == 6 && strncmp(start, "extern", 6) == 0) {
                const char *p = l->source + l->pos;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '"' && (*(p+1) == 'c' || *(p+1) == 'C') && *(p+2) == '"') {
                    p += 3;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p == '{') {
                        return make_tok(l, TOK_EXTERN_C_OPEN, start, len);
                    }
                }
            }

            l->last_was_directive = 0;
            return make_tok(l, TOK_IDENT, start, len);
        }

        /* ( ) for defined() and message() */
        if (c == '(') { advance(l); return make_tok(l, TOK_LPAREN, "(", 1); }
        if (c == ')') { advance(l); return make_tok(l, TOK_RPAREN, ")", 1); }

        /* dot */
        if (c == '.') {
            advance(l);
            return make_tok(l, TOK_DOT, ".", 1);
        }

        /* { } */
        if (c == '{') { advance(l); return make_tok(l, TOK_BRACE_OPEN, "{", 1); }
        if (c == '}') { advance(l); return make_tok(l, TOK_BRACE_CLOSE, "}", 1); }

        /* comma (also handled in condition context above, but needed globally for define values) */
        if (c == ',') { advance(l); return make_tok(l, TOK_COMMA, ",", 1); }

        /* = assignment (single =, not == which is TOK_EQ) */
        if (c == '=') { advance(l); return make_tok(l, TOK_ASSIGN, "=", 1); }

        /* + */
        if (c == '+') { advance(l); return make_tok(l, TOK_PLUS, "+", 1); }

        /* - */
        if (c == '-') { advance(l); return make_tok(l, TOK_MINUS, "-", 1); }

        /* * */
        if (c == '*') { advance(l); return make_tok(l, TOK_STAR, "*", 1); }

        /* ; */
        if (c == ';') { advance(l); return make_tok(l, TOK_SEMICOLON, ";", 1); }

        /* : */
        if (c == ':') { advance(l); return make_tok(l, TOK_COLON, ":", 1); }

        /* & */
        if (c == '&') { advance(l); return make_tok(l, TOK_AMPERSAND, "&", 1); }

        /* | */
        if (c == '|') { advance(l); return make_tok(l, TOK_PIPE, "|", 1); }

        /* % */
        if (c == '%') { advance(l); return make_tok(l, TOK_PERCENT, "%", 1); }

        /* [ */
        if (c == '[') { advance(l); return make_tok(l, TOK_LBRACKET, "[", 1); }

        /* ] */
        if (c == ']') { advance(l); return make_tok(l, TOK_RBRACKET, "]", 1); }

        /* unknown — skip but warn in debug mode */
        advance(l);
    }
    return make_tok(l, TOK_EOF, "", 0);
}
