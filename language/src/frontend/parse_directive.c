#include "parse_directive.h"
#include "error.h"
#include "defines.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- helpers ---------- */

static Token expect(Parser *p, TokenKind k, const char *err) {
    Token t = p->lexer->current;
    if (t.kind != k) error("E001", t.line, t.col, err);
    return eat(p);
}

char *strndup_safe(const char *s, size_t len) {
    char *d = malloc(len + 1);
    if (d) { memcpy(d, s, len); d[len] = '\0'; }
    return d;
}

AstNode *make_node(NodeKind kind, const char *val, size_t len, size_t line, size_t col) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->value = (val && len > 0) ? strndup_safe(val, len) : NULL;
    n->len = len;
    n->line = line;
    n->col = col;
    return n;
}

Token eat(Parser *p) {
    Token t = p->lexer->current;
    p->lexer->current = lexer_next(p->lexer);
    return t;
}

void skip_newlines(Parser *p) {
    while (p->lexer->current.kind == TOK_NEWLINE)
        eat(p);
}

/* ---------- directive constructors ---------- */

AstNode *parse_include(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #include");
    AstNode *n = make_node(NODE_INCLUDE, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

AstNode *parse_lib(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #lib");
    AstNode *n = make_node(NODE_LIB, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

AstNode *parse_extern(Parser *p) {
    Token hash = eat(p);
    Token file = expect(p, TOK_STRING, "expected string after #extern");
    AstNode *n = make_node(NODE_EXTERN, NULL, 0, hash.line, hash.col);
    n->left = make_node(NODE_STRING, file.text, file.len, file.line, file.col);
    return n;
}

AstNode *parse_define(Parser *p) {
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

AstNode *parse_undef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #undef");
    AstNode *n = make_node(NODE_UNDEF, name.text, name.len, hash.line, hash.col);
    return n;
}

AstNode *parse_debug(Parser *p) {
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
                } else {
                    eat(p);
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

AstNode *parse_extern_c(Parser *p) {
    Token ext = eat(p);
    Token c = p->lexer->current;
    if (c.kind == TOK_STRING) {
        if (c.len < 3 || (c.text[1] != 'c' && c.text[1] != 'C')) {
            error("E004", c.line, c.col, "expected 'extern \"c\"' — only C ABI supported");
        }
        eat(p);
    }

    expect(p, TOK_BRACE_OPEN, "expected '{' after extern \"c\"");
    AstNode *block = make_node(NODE_EXTERN_C_BLOCK, NULL, 0, ext.line, ext.col);
    AstNode *tail = NULL;

    skip_newlines(p);

    while (p->lexer->current.kind != TOK_BRACE_CLOSE && p->lexer->current.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->lexer->current.kind == TOK_BRACE_CLOSE) break;

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

    if (p->lexer->current.kind == TOK_BRACE_CLOSE) eat(p);
    return block;
}

AstNode *parse_pragma(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_PRAGMA, NULL, 0, hash.line, hash.col);
    Token arg = p->lexer->current;
    if (arg.kind == TOK_IDENT) {
        eat(p);
        n->left = make_node(NODE_IDENT, arg.text, arg.len, arg.line, arg.col);
    }
    return n;
}

AstNode *parse_error(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_ERROR, NULL, 0, hash.line, hash.col);
    char msg[1024] = "";
    int first = 1;
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        Token t = p->lexer->current;
        size_t rem = sizeof(msg) - strlen(msg) - 1;
        if (!first) { strncat(msg, " ", rem); rem = sizeof(msg) - strlen(msg) - 1; }
        first = 0;
        size_t copy_len = t.len < rem ? t.len : rem;
        strncat(msg, t.text, copy_len);
        eat(p);
    }
    n->left = make_node(NODE_STRING, msg, strlen(msg), hash.line, hash.col);
    return n;
}

AstNode *parse_message(Parser *p) {
    Token hash = eat(p);
    AstNode *n = make_node(NODE_MESSAGE, NULL, 0, hash.line, hash.col);
    char msg[1024] = "";
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        Token t = p->lexer->current;
        size_t rem = sizeof(msg) - strlen(msg) - 1;
        if (t.kind == TOK_LPAREN) { eat(p); continue; }
        if (t.kind == TOK_RPAREN) { eat(p); continue; }
        if (t.kind == TOK_COMMA) { eat(p); continue; }
        if (t.kind == TOK_STRING) {
            const char *s = t.text;
            size_t slen = t.len;
            if (slen >= 2 && s[0] == '"') { s++; slen -= 2; }
            size_t copy_len = slen < rem ? slen : rem;
            strncat(msg, s, copy_len);
        } else {
            size_t copy_len = t.len < rem ? t.len : rem;
            strncat(msg, t.text, copy_len);
        }
        eat(p);
    }
    n->left = make_node(NODE_STRING, msg, strlen(msg), hash.line, hash.col);
    return n;
}

AstNode *parse_ifdef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #ifdef");
    return make_node(NODE_IFDEF, name.text, name.len, hash.line, hash.col);
}

AstNode *parse_ifndef(Parser *p) {
    Token hash = eat(p);
    Token name = expect(p, TOK_IDENT, "expected identifier after #ifndef");
    return make_node(NODE_IFNDEF, name.text, name.len, hash.line, hash.col);
}

AstNode *parse_if(Parser *p) {
    Token hash = eat(p);
    p->lexer->in_condition_context = 1;
    AstNode *n = make_node(NODE_IF, NULL, 0, hash.line, hash.col);

    Token t = p->lexer->current;
    if (t.kind == TOK_IDENT && t.len == 7 && strncmp(t.text, "defined", 7) == 0) {
        eat(p);
        if (p->lexer->current.kind == TOK_LPAREN) eat(p);
        Token name = expect(p, TOK_IDENT, "expected identifier in defined()");
        if (p->lexer->current.kind == TOK_RPAREN) eat(p);
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

AstNode *parse_elif(Parser *p) {
    Token hash = eat(p);
    p->lexer->in_condition_context = 1;
    AstNode *n = make_node(NODE_ELIF, NULL, 0, hash.line, hash.col);

    Token t = p->lexer->current;
    if (t.kind == TOK_IDENT && t.len == 7 && strncmp(t.text, "defined", 7) == 0) {
        eat(p);
        if (p->lexer->current.kind == TOK_LPAREN) eat(p);
        Token name = expect(p, TOK_IDENT, "expected identifier in defined()");
        if (p->lexer->current.kind == TOK_RPAREN) eat(p);
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

AstNode *parse_else(Parser *p) {
    Token hash = eat(p);
    return make_node(NODE_ELSE, NULL, 0, hash.line, hash.col);
}

AstNode *parse_endif(Parser *p) {
    Token hash = eat(p);
    return make_node(NODE_ENDIF, NULL, 0, hash.line, hash.col);
}

/* ---------- bare directive detection ---------- */

int bare_directive_dispatch(const char *s, size_t len) {
    if (len == 7 && strncmp(s, "message", 7) == 0) return 1;
    if (len == 7 && strncmp(s, "include", 7) == 0) return 1;
    if (len == 6 && strncmp(s, "ifndef", 6) == 0) return 1;
    if (len == 6 && strncmp(s, "define", 6) == 0) return 1;
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

/* ---------- raw line capture ---------- */

AstNode *parse_raw_line(Parser *p) {
    size_t line = p->lexer->current.line;
    const char *tok_start = p->lexer->current.text;
    const char *line_start = tok_start;
    while (line_start > p->lexer->source && line_start[-1] != '\n' && line_start[-1] != '\0')
        line_start--;
    size_t col = 1;
    for (const char *c = line_start; c < tok_start; c++) {
        if (*c == '\t') col = ((col + 7) / 8) * 8 + 1;
        else col++;
    }
    const char *start = line_start;
    while (p->lexer->current.kind != TOK_NEWLINE && p->lexer->current.kind != TOK_EOF) {
        eat(p);
    }
    const char *end = p->lexer->source + p->lexer->pos;
    if (p->lexer->current.kind == TOK_NEWLINE) {
        eat(p);
    }
    size_t len = end - start;
    if (len == 0) return NULL;
    if (len > 0 && start[len-1] == '\n') {
        len--;
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
        len = len + 1;
    } else {
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
    }
    if (len == 0) return NULL;
    return make_node(NODE_RAW, start, len, line, col);
}
