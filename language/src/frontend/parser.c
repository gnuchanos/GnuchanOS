#include "parser.h"
#include "error.h"
#include "parse_directive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- core parse statement ---------- */

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

/* ---------- public API ---------- */

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
