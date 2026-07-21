#include "parser.h"
#include "error.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *strndup_safe(const char *s, size_t len) {
    char *d = malloc(len + 1);
    if (d) { memcpy(d, s, len); d[len] = '\0'; }
    return d;
}

static AstNode *make_node(NodeKind kind, const char *val, size_t len, size_t line, size_t col) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->value = (val && len > 0) ? strndup_safe(val, len) : NULL;
    n->len = len;
    n->line = line;
    n->col = col;
    return n;
}

static Token eat(Parser *p) {
    Token t = p->lexer->current;
    p->lexer->current = lexer_next(p->lexer);
    return t;
}

static Token expect(Parser *p, TokenKind k, const char *err) {
    Token t = p->lexer->current;
    if (t.kind != k) error("E001", t.line, t.col, err);
    return eat(p);
}

static void skip_newlines(Parser *p) {
    while (p->lexer->current.kind == TOK_NEWLINE)
        eat(p);
}

/* #include "file" / <file> */
static AstNode *parse_include(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #include");
    AstNode *n = make_node(NODE_INCLUDE, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

/* #lib "file" / <file> */
static AstNode *parse_lib(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #lib");
    AstNode *n = make_node(NODE_LIB, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

/* #extern "file" / <file> */
static AstNode *parse_extern(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #extern");
    AstNode *n = make_node(NODE_EXTERN, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

/* #define NAME value */
static AstNode *parse_define(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #define");
    AstNode *n = make_node(NODE_DEFINE, name.text, name.len, hash.line, hash.col);

    Token val = p->lexer->current;
    if (val.kind == TOK_IDENT || val.kind == TOK_STRING || val.kind == TOK_NUMBER) {
        eat(p);
        n->left = make_node(val.kind == TOK_STRING ? NODE_STRING :
                            val.kind == TOK_NUMBER ? NODE_NUMBER : NODE_IDENT,
                            val.text, val.len, val.line, val.col);
    }
    return n;
}

/* #undef NAME */
static AstNode *parse_undef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #undef");
    AstNode *n = make_node(NODE_UNDEF, name.text, name.len, hash.line, hash.col);
    return n;
}

/* #debug expr expr ... */
static AstNode *parse_debug(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_DEBUG, NULL, 0, hash.line, hash.col);
    AstNode *tail = NULL;

    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        Token t = p->lexer->current;
        AstNode *arg = NULL;

        if (t.kind == TOK_STRING) {
            eat(p);
            arg = make_node(NODE_STRING, t.text, t.len, t.line, t.col);
        } else if (t.kind == TOK_NUMBER) {
            eat(p);
            arg = make_node(NODE_NUMBER, t.text, t.len, t.line, t.col);
        } else if (t.kind == TOK_IDENT) {
            Token first = eat(p);
            if (p->lexer->current.kind == TOK_DOT) {
                eat(p);
                Token second = p->lexer->current;
                if (second.kind == TOK_IDENT) {
                    eat(p);
                    arg = make_node(NODE_IDENT, second.text, second.len, second.line, second.col);
                } else if (second.kind == TOK_NUMBER) {
                    eat(p);
                    arg = make_node(NODE_NUMBER, second.text, second.len, second.line, second.col);
                }
            } else {
                arg = make_node(NODE_IDENT, first.text, first.len, first.line, first.col);
            }
        } else {
            eat(p);
            continue;
        }

        if (arg) {
            if (!n->left) { n->left = arg; tail = arg; }
            else { tail->next = arg; tail = arg; }
        }
    }
    return n;
}

/* extern "c" { #define ... #define ... } */
static AstNode *parse_extern_c(Parser *p) {
    Token ext = eat(p);
    Token c = p->lexer->current;
    if (c.kind == TOK_STRING) eat(p);

    expect(p, TOK_LBRACE, "expected '{' after extern \"c\"");
    AstNode *block = make_node(NODE_EXTERN_C_BLOCK, NULL, 0, ext.line, ext.col);
    AstNode *tail = NULL;

    skip_newlines(p);

    while (p->lexer->current.kind != TOK_RBRACE && p->lexer->current.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->lexer->current.kind == TOK_RBRACE) break;

        AstNode *stmt = NULL;
        if (p->lexer->current.kind == TOK_HASH_DEFINE)
            stmt = parse_define(p);
        else {
            eat(p);
            continue;
        }
        if (!block->left) { block->left = stmt; tail = stmt; }
        else { tail->next = stmt; tail = stmt; }
    }

    if (p->lexer->current.kind == TOK_RBRACE) eat(p);
    return block;
}

/* #pragma once */
static AstNode *parse_pragma(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_PRAGMA, NULL, 0, hash.line, hash.col);
    Token arg = p->lexer->current;
    if (arg.kind == TOK_IDENT) {
        eat(p);
        n->left = make_node(NODE_IDENT, arg.text, arg.len, arg.line, arg.col);
    }
    return n;
}

/* #error msg */
static AstNode *parse_error(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_ERROR, NULL, 0, hash.line, hash.col);
    char msg[1024] = "";
    int first = 1;
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        Token t = p->lexer->current;
        if (!first) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
        first = 0;
        strncat(msg, t.text, t.len < 128 ? t.len : 128);
        eat(p);
    }
    n->left = make_node(NODE_STRING, msg, strlen(msg), hash.line, hash.col);
    return n;
}

/* #message "msg" or message("msg") */
static AstNode *parse_message(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_MESSAGE, NULL, 0, hash.line, hash.col);
    char msg[1024] = "";
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        Token t = p->lexer->current;
        /* skip function-call syntax: ( ) , */
        if (t.kind == TOK_LBRACE && t.len == 1 && t.text[0] == '(') { eat(p); continue; }
        if (t.kind == TOK_RBRACE && t.len == 1 && t.text[0] == ')') { eat(p); continue; }
        if (t.kind == TOK_COMMA) { eat(p); continue; }
        if (t.kind == TOK_STRING) {
            const char *s = t.text;
            size_t slen = t.len;
            if (slen >= 2 && s[0] == '"') { s++; slen -= 2; }
            strncat(msg, s, slen < 128 ? slen : 128);
        } else {
            strncat(msg, t.text, t.len < 64 ? t.len : 64);
        }
        eat(p);
    }
    n->left = make_node(NODE_STRING, msg, strlen(msg), hash.line, hash.col);
    return n;
}

/* #ifdef NAME */
static AstNode *parse_ifdef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #ifdef");
    AstNode *n = make_node(NODE_IFDEF, name.text, name.len, hash.line, hash.col);
    return n;
}

/* #ifndef NAME */
static AstNode *parse_ifndef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #ifndef");
    AstNode *n = make_node(NODE_IFNDEF, name.text, name.len, hash.line, hash.col);
    return n;
}

/* #if expr — simplified: #if NAME, #if NAME OP VALUE, #if defined(NAME) */
static AstNode *parse_if(Parser *p) {
    Token hash = eat(p);
    lexer_set_condition(1);
    AstNode *n = make_node(NODE_IF, NULL, 0, hash.line, hash.col);

    Token t = p->lexer->current;
    if (t.kind == TOK_IDENT && t.len == 7 && strncmp(t.text, "defined", 7) == 0) {
        eat(p);
        if (p->lexer->current.kind == TOK_LBRACE) eat(p);
        Token name = expect(p, TOK_IDENT, "expected identifier in defined()");
        if (p->lexer->current.kind == TOK_RBRACE) eat(p);
        n->value = strndup_safe(name.text, name.len);
        n->left = make_node(NODE_IDENT, "defined", 7, t.line, t.col);
        return n;
    }

    if (t.kind == TOK_IDENT) {
        eat(p);
        n->value = strndup_safe(t.text, t.len);

        Token op = p->lexer->current;
        if (op.kind == TOK_EQ || op.kind == TOK_NE ||
            op.kind == TOK_LT || op.kind == TOK_LE ||
            op.kind == TOK_GT || op.kind == TOK_GE) {
            eat(p);
            n->left = make_node(NODE_IDENT, op.text, op.len, op.line, op.col);
            Token val = p->lexer->current;
            if (val.kind == TOK_NUMBER || val.kind == TOK_STRING || val.kind == TOK_IDENT) {
                eat(p);
                n->right = make_node(val.kind == TOK_STRING ? NODE_STRING :
                                     val.kind == TOK_NUMBER ? NODE_NUMBER : NODE_IDENT,
                                     val.text, val.len, val.line, val.col);
            }
        } else {
            AstNode *tail = NULL;
            while (p->lexer->current.kind == TOK_SLASH || p->lexer->current.kind == TOK_COMMA) {
                eat(p);
                Token alt = p->lexer->current;
                if (alt.kind != TOK_IDENT) break;
                eat(p);
                AstNode *alt_node = make_node(NODE_IDENT, alt.text, alt.len, alt.line, alt.col);
                if (!n->right) {
                    n->right = alt_node;
                    tail = alt_node;
                } else {
                    tail->next = alt_node;
                    tail = alt_node;
                }
            }
        }
        return n;
    }

    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) eat(p);
    return n;
}

/* #elif expr — same as #if but reuses NODE_ELIF */
static AstNode *parse_elif(Parser *p) {
    Token hash = eat(p);
    lexer_set_condition(1);
    AstNode *n = make_node(NODE_ELIF, NULL, 0, hash.line, hash.col);

    Token t = p->lexer->current;
    if (t.kind == TOK_IDENT && t.len == 7 && strncmp(t.text, "defined", 7) == 0) {
        eat(p);
        if (p->lexer->current.kind == TOK_LBRACE) eat(p);
        Token name = expect(p, TOK_IDENT, "expected identifier in defined()");
        if (p->lexer->current.kind == TOK_RBRACE) eat(p);
        n->value = strndup_safe(name.text, name.len);
        n->left = make_node(NODE_IDENT, "defined", 7, t.line, t.col);
        return n;
    }

    if (t.kind == TOK_IDENT) {
        eat(p);
        n->value = strndup_safe(t.text, t.len);
        Token op = p->lexer->current;
        if (op.kind == TOK_EQ || op.kind == TOK_NE ||
            op.kind == TOK_LT || op.kind == TOK_LE ||
            op.kind == TOK_GT || op.kind == TOK_GE) {
            eat(p);
            n->left = make_node(NODE_IDENT, op.text, op.len, op.line, op.col);
            Token val = p->lexer->current;
            if (val.kind == TOK_NUMBER || val.kind == TOK_STRING || val.kind == TOK_IDENT) {
                eat(p);
                n->right = make_node(val.kind == TOK_STRING ? NODE_STRING :
                                     val.kind == TOK_NUMBER ? NODE_NUMBER : NODE_IDENT,
                                     val.text, val.len, val.line, val.col);
            }
        } else {
            AstNode *tail = NULL;
            while (p->lexer->current.kind == TOK_SLASH || p->lexer->current.kind == TOK_COMMA) {
                eat(p);
                Token alt = p->lexer->current;
                if (alt.kind != TOK_IDENT) break;
                eat(p);
                AstNode *alt_node = make_node(NODE_IDENT, alt.text, alt.len, alt.line, alt.col);
                if (!n->right) {
                    n->right = alt_node;
                    tail = alt_node;
                } else {
                    tail->next = alt_node;
                    tail = alt_node;
                }
            }
        }
        return n;
    }

    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) eat(p);
    return n;
}

/* #else */
static AstNode *parse_else(Parser *p) {
    Token hash = eat(p);
    return make_node(NODE_ELSE, NULL, 0, hash.line, hash.col);
}

/* #endif */
static AstNode *parse_endif(Parser *p) {
    Token hash = eat(p);
    return make_node(NODE_ENDIF, NULL, 0, hash.line, hash.col);
}

/* recognize bare directive keywords (without #) by length+content */
static int bare_directive_dispatch(const char *s, size_t len) {
    /* sort by length desc to avoid prefix mismatches */
    if (len == 7 && strncmp(s, "message", 7) == 0) return 1;
    if (len == 7 && strncmp(s, "include", 7) == 0) return 1;
    if (len == 6 && strncmp(s, "ifndef", 6) == 0) return 1;
    if (len == 6 && strncmp(s, "define", 6) == 0) return 1;
    if (len == 6 && strncmp(s, "extern", 6) == 0) return 1;
    if (len == 6 && strncmp(s, "pragma", 6) == 0) return 1;
    if (len == 5 && strncmp(s, "ifdef", 5) == 0) return 1;
    if (len == 5 && strncmp(s, "endif", 5) == 0) return 1;
    if (len == 5 && strncmp(s, "undef", 5) == 0) return 1;
    if (len == 5 && strncmp(s, "debug", 5) == 0) return 1;
    if (len == 5 && strncmp(s, "error", 5) == 0) return 1;
    if (len == 4 && strncmp(s, "elif", 4) == 0) return 1;
    if (len == 4 && strncmp(s, "else", 4) == 0) return 1;
    if (len == 3 && strncmp(s, "lib", 3) == 0) return 1;
    if (len == 2 && strncmp(s, "if", 2) == 0) return 1;
    return 0;
}

/* capture a raw line of non-directive C code */
static AstNode *parse_raw_line(Parser *p) {
    size_t line = p->lexer->current.line;
    /* find actual start of this line by scanning backwards from current token */
    const char *tok_start = p->lexer->current.text;
    const char *line_start = tok_start;
    while (line_start > p->lexer->source && line_start[-1] != '\n')
        line_start--;
    /* skip leading whitespace for col tracking */
    size_t col = 1;
    for (const char *c = line_start; c < tok_start; c++) {
        if (*c == '\t') col = ((col + 7) / 8) * 8 + 1;
        else col++;
    }
    const char *start = line_start;
    /* advance to end of line without consuming the newline yet */
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        eat(p);
    }
    /* capture end BEFORE eating the newline so it includes the \n */
    const char *end = p->lexer->source + p->lexer->pos;
    if (p->lexer->current.kind == TOK_NEWLINE) {
        eat(p); /* consume newline, pos advances past it */
    }
    size_t len = end - start;
    if (len == 0) return NULL;
    /* trim trailing whitespace (spaces/tabs) but preserve a trailing newline */
    if (len > 0 && start[len-1] == '\n') {
        len--; /* exclude newline for trimming, will add back */
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
        len = len + 1; /* keep the newline */
    } else {
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
    }
    if (len == 0) return NULL;
    AstNode *n = make_node(NODE_RAW, start, len, line, col);
    return n;
}

/* parse any top-level statement */
static AstNode *parse_stmt(Parser *p) {
    skip_newlines(p);
    if (p->lexer->current.kind == TOK_EOF) return NULL;

    switch (p->lexer->current.kind) {
        case TOK_HASH_INCLUDE: return parse_include(p);
        case TOK_HASH_LIB:     return parse_lib(p);
        case TOK_HASH_EXTERN:  return parse_extern(p);
        case TOK_HASH_DEBUG:   return parse_debug(p);
        case TOK_HASH_DEFINE:  return parse_define(p);
        case TOK_HASH_UNDEF:   return parse_undef(p);
        case TOK_HASH_PRAGMA:  return parse_pragma(p);
        case TOK_HASH_IFDEF:   return parse_ifdef(p);
        case TOK_HASH_IFNDEF:  return parse_ifndef(p);
        case TOK_HASH_IF:      return parse_if(p);
        case TOK_HASH_ELIF:    return parse_elif(p);
        case TOK_HASH_ELSE:    return parse_else(p);
        case TOK_HASH_ENDIF:   return parse_endif(p);
        case TOK_HASH_ERROR:   return parse_error(p);
        case TOK_HASH_MESSAGE: return parse_message(p);
        case TOK_EXTERN_C_OPEN:return parse_extern_c(p);
        case TOK_NEWLINE:      eat(p); return parse_stmt(p);
        case TOK_IDENT:
            /* bare directive keywords (without #) */
            if (bare_directive_dispatch(p->lexer->current.text, p->lexer->current.len)) {
                const char *s = p->lexer->current.text;
                size_t len = p->lexer->current.len;
                if (len == 7 && strncmp(s, "message", 7) == 0) return parse_message(p);
                if (len == 7 && strncmp(s, "include", 7) == 0) return parse_include(p);
                if (len == 6 && strncmp(s, "ifndef", 6) == 0) return parse_ifndef(p);
                if (len == 6 && strncmp(s, "define", 6) == 0) return parse_define(p);
                if (len == 6 && strncmp(s, "pragma", 6) == 0) return parse_pragma(p);
                if (len == 5 && strncmp(s, "ifdef", 5) == 0) return parse_ifdef(p);
                if (len == 5 && strncmp(s, "endif", 5) == 0) return parse_endif(p);
                if (len == 5 && strncmp(s, "undef", 5) == 0) return parse_undef(p);
                if (len == 5 && strncmp(s, "debug", 5) == 0) return parse_debug(p);
                if (len == 5 && strncmp(s, "error", 5) == 0) return parse_error(p);
                if (len == 4 && strncmp(s, "elif", 4) == 0) return parse_elif(p);
                if (len == 4 && strncmp(s, "else", 4) == 0) return parse_else(p);
                if (len == 3 && strncmp(s, "lib", 3) == 0) return parse_lib(p);
                if (len == 2 && strncmp(s, "if", 2) == 0) return parse_if(p);
            }
            /* not a directive keyword — capture whole line as raw C code */
            return parse_raw_line(p);
        default:
            /* any other token — capture whole line as raw C code */
            return parse_raw_line(p);
    }
}

Parser *parser_new(Lexer *l) {
    Parser *p = calloc(1, sizeof(Parser));
    p->lexer = l;
    return p;
}

AstNode *parser_parse(Parser *p) {
    AstNode *prog = make_node(NODE_PROGRAM, NULL, 0, 1, 1);
    AstNode *tail = NULL;

    while (p->lexer->current.kind != TOK_EOF) {
        AstNode *stmt = parse_stmt(p);
        if (!stmt) break;

        if (!prog->left) { prog->left = stmt; tail = stmt; }
        else { tail->next = stmt; tail = stmt; }
    }

    return prog;
}
