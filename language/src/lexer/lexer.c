#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static Token make(Lexer *l, TokenType type) {
    Token t = {type, l->cur.start, (size_t)(l->pos - l->cur.start), l->cur.line, l->cur.col};
    return t;
}

static int next_char(Lexer *l) {
    if (!*l->pos) return EOF;
    int c = (unsigned char)*l->pos++;
    if (c == '\r') {
        if (*l->pos == '\n') l->pos++;
        l->line++; l->col = 1;
    } else if (c == '\n') {
        l->line++; l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static int peek_char(Lexer *l) {
    if (!*l->pos) return EOF;
    return (unsigned char)*l->pos;
}

Lexer lexer_new(const char *src, const char *name) {
    Lexer l;
    l.src = src;
    l.pos = src;
    l.line = 1; l.col = 1;
    l.cur = (Token){TOKEN_EOF, src, 0, 1, 1};
    l.src_name = name;
    l.error_count = 0;
    return l;
}

/* Process escape sequences in a string buffer.
   Converts \n → 0x0A, \t → 0x09, \r → 0x0D, \\ → \\, \" → ",
   \0 → 0x00, \xHH → hex byte.
   Writes processed bytes to 'out' and returns processed length via out_len.
   out can be NULL to compute length only.
*/
int process_escapes(const char *src, size_t src_len, char *out, size_t *out_len) {
    size_t w = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '\\' && i + 1 < src_len) {
            char c = src[++i];
            char val = 0;
            switch (c) {
            case 'n':  val = '\n'; break;
            case 't':  val = '\t'; break;
            case 'r':  val = '\r'; break;
            case '0':  val = '\0'; break;
            case '\\': val = '\\'; break;
            case '"':  val = '"';  break;
            case '\'': val = '\''; break;
            case 'a':  val = '\a'; break;
            case 'b':  val = '\b'; break;
            case 'v':  val = '\v'; break;
            case 'f':  val = '\f'; break;
            case 'x': {
                if (i + 2 < src_len) {
                    char hex[3] = {src[i+1], src[i+2], 0};
                    val = (char)strtol(hex, NULL, 16);
                    i += 2;
                }
                break;
            }
            default:
                /* Unknown escape — pass through (e.g. \% → % for printf) */
                if (out) out[w] = c;
                w++;
                continue;
            }
            if (out) out[w] = val;
            w++;
        } else {
            if (out) out[w] = src[i];
            w++;
        }
    }
    if (out_len) *out_len = w;
    return (int)w;
}

Token lexer_next(Lexer *l) {
    lexer_skip_ws(l);
    l->cur.start = l->pos;
    l->cur.line  = l->line;
    l->cur.col   = l->col;

    /* Handle preprocessor directives: #include, #lib, #extern */
    if (peek_char(l) == '#') {
        const char *start = l->pos;
        next_char(l); /* consume # */
        while (peek_char(l) == ' ' || peek_char(l) == '\t') next_char(l);
        const char *kw_start = l->pos;
        while (isalpha(peek_char(l))) next_char(l);
        size_t kw_len = (size_t)(l->pos - kw_start);

        /* Skip whitespace before filename */
        while (peek_char(l) == ' ' || peek_char(l) == '\t') next_char(l);

        if (kw_len == 7 && memcmp(kw_start, "include", 7) == 0) {
            /* <file> or "file" */
            if (peek_char(l) == '<' || peek_char(l) == '"') {
                char delim = peek_char(l);
                next_char(l);
                const char *fn_start = l->pos;
                while (peek_char(l) != delim && peek_char(l) != EOF && peek_char(l) != '\n') next_char(l);
                size_t fn_len = (size_t)(l->pos - fn_start);
                Token t = {TOKEN_PREPROC_INCLUDE, fn_start, fn_len, l->line, l->col - (int)(l->pos - start)};
                if (peek_char(l) == delim) next_char(l);
                return l->cur = t;
            }
        }
        if (kw_len == 3 && memcmp(kw_start, "lib", 3) == 0) {
            while (peek_char(l) != '\n' && peek_char(l) != EOF) next_char(l);
            size_t fn_len = (size_t)(l->pos - l->cur.start);
            Token t = {TOKEN_PREPROC_LIB, l->cur.start, fn_len, l->line, l->col};
            return l->cur = t;
        }
        if (kw_len == 6 && memcmp(kw_start, "extern", 6) == 0) {
            while (peek_char(l) != '\n' && peek_char(l) != EOF) next_char(l);
            size_t fn_len = (size_t)(l->pos - l->cur.start);
            Token t = {TOKEN_PREPROC_EXTERN, l->cur.start, fn_len, l->line, l->col};
            return l->cur = t;
        }
    }

    int c = peek_char(l);
    if (c == EOF) return l->cur = make(l, TOKEN_EOF);

    /* identifier / keyword */
    if (isalpha(c) || c == '_') {
        const char *start = l->pos;
        while (isalnum(peek_char(l)) || peek_char(l) == '_') next_char(l);
        size_t len = (size_t)(l->pos - start);
        TokenType kw = token_keyword(start, len);
        Token t = {kw, start, len, l->line, l->col - (int)len};
        return l->cur = t;
    }

    /* number */
    if (isdigit(c)) {
        const char *start = l->pos;
        int is_float = 0;
        while (isdigit(peek_char(l))) next_char(l);
        /* Handle hex (0x...) */
        if (!is_float && start[0] == '0' && (peek_char(l) == 'x' || peek_char(l) == 'X')) {
            next_char(l);
            while (isxdigit(peek_char(l))) next_char(l);
        } else if (peek_char(l) == '.') {
            is_float = 1; next_char(l);
            while (isdigit(peek_char(l))) next_char(l);
        }
        /* Handle float suffix f / F / l / L */
        if (is_float && (peek_char(l) == 'f' || peek_char(l) == 'F' || peek_char(l) == 'l' || peek_char(l) == 'L'))
            next_char(l);
        /* Handle integer suffixes: u/U, l/L, ll/LL, ul/UL, ull/ULL, lu/LU, llu/LLU */
        if (!is_float) {
            int c1 = peek_char(l);
            if (c1 == 'u' || c1 == 'U') {
                next_char(l);
                int c2 = peek_char(l);
                if (c2 == 'l' || c2 == 'L') {
                    next_char(l);
                    int c3 = peek_char(l);
                    if ((c2 == 'l' || c2 == 'L') && (c3 == 'l' || c3 == 'L'))
                        next_char(l);
                }
            } else if (c1 == 'l' || c1 == 'L') {
                next_char(l);
                int c2 = peek_char(l);
                if (c2 == 'l' || c2 == 'L') {
                    next_char(l);
                    /* ll + optional u/U */
                    if (peek_char(l) == 'u' || peek_char(l) == 'U')
                        next_char(l);
                }
                /* single l + optional u/U */
                else if (c2 == 'u' || c2 == 'U')
                    next_char(l);
            }
        }
        size_t len = (size_t)(l->pos - start);
        Token t = {is_float ? TOKEN_NUMBER_FLOAT : TOKEN_NUMBER_INT,
                   start, len, l->line, l->col - (int)len};
        return l->cur = t;
    }

    /* string literal — with escape processing */
    if (c == '"') {
        next_char(l);
        const char *start = l->pos;
        while (peek_char(l) != '"' && peek_char(l) != EOF) {
            if (peek_char(l) == '\\') next_char(l);
            next_char(l);
        }
        size_t raw_len = (size_t)(l->pos - start);
        if (peek_char(l) == '"') next_char(l);
        Token t = {TOKEN_STRING, start, raw_len, l->line, l->col - (int)raw_len - 1};
        return l->cur = t;
    }

    /* char literal */
    if (c == '\'') {
        next_char(l);
        const char *start = l->pos;
        if (peek_char(l) == '\\') next_char(l);
        if (peek_char(l) != EOF) next_char(l);
        size_t len = (size_t)(l->pos - start);
        if (peek_char(l) == '\'') next_char(l);
        Token t = {TOKEN_CHAR_LIT, start, len, l->line, l->col - (int)len - 1};
        return l->cur = t;
    }

    /* operators / punctuation */
    next_char(l);
    #define OP2(a,b,ta,tb) if(c==a&&peek_char(l)==b){next_char(l);return l->cur=make(l,tb);} return l->cur=make(l,ta);
    #define OP1(a,ta) return l->cur=make(l,ta);
    switch (c) {
    case '+': OP2('+','+',TOKEN_PLUS,TOKEN_PLUSPLUS)
              OP2('+','=',TOKEN_PLUS,TOKEN_PLUSEQ)
    case '-': OP2('-','-',TOKEN_MINUS,TOKEN_MINUSMINUS)
              OP2('-','=',TOKEN_MINUS,TOKEN_MINUSEQ)
              OP2('-','>',TOKEN_MINUS,TOKEN_ARROW)
    case '*': OP2('*','=',TOKEN_STAR,TOKEN_STAREQ)
    case '/': OP2('/','=',TOKEN_SLASH,TOKEN_SLASHEQ)
    case '%': OP2('%','=',TOKEN_PERCENT,TOKEN_PERCENTEQ)
    case '=': OP2('=','=',TOKEN_EQ,TOKEN_EQEQ)
    case '!': OP2('!','=',TOKEN_BANG,TOKEN_BANGEQ)
    case '<': OP2('<','<',TOKEN_LT,TOKEN_LSHIFT)
              OP2('<','=',TOKEN_LT,TOKEN_LE)
    case '>': OP2('>','>',TOKEN_GT,TOKEN_RSHIFT)
              OP2('>','=',TOKEN_GT,TOKEN_GE)
    case '&': OP2('&','&',TOKEN_AND,TOKEN_ANDAND)
              OP2('&','=',TOKEN_AND,TOKEN_ANDEQ)
    case '|': OP2('|','|',TOKEN_PIPE,TOKEN_OROR)
              OP2('|','=',TOKEN_PIPE,TOKEN_PIPEEQ)
    case '^': OP2('^','=',TOKEN_CARET,TOKEN_CARETEQ)
    case '~': OP1('~',TOKEN_TILDE)
    case '.': OP1('.',TOKEN_DOT)
    case ',': OP1(',',TOKEN_COMMA)
    case ';': OP1(';',TOKEN_SEMI)
    case ':': OP1(':',TOKEN_COLON)
    case '?': OP1('?',TOKEN_QUESTION)
    case '(': OP1('(',TOKEN_LPAREN)
    case ')': OP1(')',TOKEN_RPAREN)
    case '[': OP1('[',TOKEN_LBRACKET)
    case ']': OP1(']',TOKEN_RBRACKET)
    case '{': OP1('{',TOKEN_LBRACE)
    case '}': OP1('}',TOKEN_RBRACE)
    case '@': OP1('@',TOKEN_AT)
    default:
        fprintf(stderr, "%s:%d:%d: unexpected character '%c'\n",
                l->src_name, l->line, l->col, c);
        l->error_count++;
        return l->cur = make(l, TOKEN_EOF);
    }
}

Token lexer_peek(Lexer *l) { return l->cur; }

void lexer_skip_ws(Lexer *l) {
    for (;;) {
        int c = peek_char(l);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { next_char(l); continue; }
        if (c == EOF) break;
        if (c == '/' && l->pos[1] == '/') { while (peek_char(l) != '\n' && peek_char(l) != EOF) next_char(l); continue; }
        if (c == '/' && l->pos[1] == '*') { next_char(l); next_char(l);
            while (!(peek_char(l) == '*' && l->pos[1] == '/') && peek_char(l) != EOF) next_char(l);
            if (peek_char(l) == '*') { next_char(l); next_char(l); } continue; }
        break;
    }
}
