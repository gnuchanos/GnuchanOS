#include "parser.h"
#include "error.h"
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
            /* check for namespace.member pattern: strip prefix */
            Token first = eat(p);
            if (p->lexer->current.kind == TOK_DOT) {
                eat(p); /* dot */
                Token second = p->lexer->current;
                if (second.kind == TOK_IDENT) {
                    eat(p);
                    arg = make_node(NODE_IDENT, second.text, second.len, second.line, second.col);
                } else if (second.kind == TOK_NUMBER) {
                    eat(p);
                    arg = make_node(NODE_NUMBER, second.text, second.len, second.line, second.col);
                }
                /* else: malformed — skip */
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

    /* eat "c" */
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
        case TOK_EXTERN_C_OPEN:return parse_extern_c(p);
        case TOK_NEWLINE:      eat(p); return parse_stmt(p);
        default:
            eat(p);
            return parse_stmt(p);
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
