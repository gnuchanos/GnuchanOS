#include "parser.h"
#include "../lexer/lexer.h"
#include "../lexer/token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================= */
/*  Parser — recursive descent, minimal GCL subset                    */
/*  Tüm token erişimi: p->lexer.cur = şu anki token                   */
/*  lexer_next() ilerletir. ASLA lexer_peek kullanma.                */
/* ================================================================= */

static AstNode *parse_stmt(Parser *p);
static AstNode *parse_expr(Parser *p);

/* current token'i kontrol et, eşleşirse yut ve ilerle */
static int accept(Parser *p, TokenType type) {
    if (p->lexer.cur.type == type) {
        p->lexer.cur = lexer_next(&p->lexer);
        return 1;
    }
    return 0;
}

/* belirtilen satırın içeriğini src'den bulup yazdır */
static void print_source_line(const char *src, int target_line, FILE *out) {
    int line = 1;
    const char *p = src;
    const char *line_start = src;

    while (*p && line < target_line) {
        if (*p == '\n') { line++; p++; line_start = p; }
        else if (*p == '\r') { if (p[1] == '\n') p++; line++; p++; line_start = p; }
        else p++;
    }
    while (*p && *p != '\n' && *p != '\r') {
        fputc(*p, out);
        p++;
    }
    fputc('\n', out);
}

/* current token eşleşmezse hata — kaynak satır + ^ göster */
static int expect(Parser *p, TokenType type) {
    if (accept(p, type)) return 1;
    Token cur = p->lexer.cur;
    fprintf(stderr, "%s:%d:%d: error: expected '%s', found '%s'\n",
            p->lexer.src_name, cur.line, cur.col,
            token_name(type), token_name(cur.type));
    /* kaynak satırı göster */
    if (p->lexer.src) {
        fprintf(stderr, "  ");
        print_source_line(p->lexer.src, cur.line, stderr);
        fprintf(stderr, "  ");
        for (int i = 1; i < cur.col; i++) fputc(' ', stderr);
        fprintf(stderr, "^\n");
    }
    p->error_count++;
    return 0;
}

/* ================================================================= */
/*  Type parsing (built-in type isimleri + multi-word)               */
/* ================================================================= */
static int is_type_token(TokenType t) {
    return t == TOKEN_KW_CHAR || t == TOKEN_KW_SHORT ||
           t == TOKEN_KW_INT  || t == TOKEN_KW_LONG ||
           t == TOKEN_KW_FLOAT|| t == TOKEN_KW_DOUBLE ||
           t == TOKEN_KW_VOID ||
           t == TOKEN_KW_INT8 || t == TOKEN_KW_INT16 ||
           t == TOKEN_KW_INT32 || t == TOKEN_KW_INT64 ||
           t == TOKEN_KW_INT128 ||
           t == TOKEN_KW_UNSIGNED ||
           t == TOKEN_KW_UINT8 || t == TOKEN_KW_UINT16 ||
           t == TOKEN_KW_UINT32 || t == TOKEN_KW_UINT64 ||
           t == TOKEN_KW_BOOL ||
           t == TOKEN_KW_SIZE_T || t == TOKEN_KW_SSIZE_T ||
           t == TOKEN_KW_INTPTR_T || t == TOKEN_KW_UINTPTR_T;
}

static AstNode *parse_type(Parser *p) {
    if (!is_type_token(p->lexer.cur.type))
        return NULL;

    Token first = p->lexer.cur;
    p->lexer.cur = lexer_next(&p->lexer);

    /* Handle "unsigned" + type pattern */
    if (first.type == TOKEN_KW_UNSIGNED) {
        if (is_type_token(p->lexer.cur.type)) {
            Token second = p->lexer.cur;
            p->lexer.cur = lexer_next(&p->lexer);

            /* "unsigned long long" */
            if (second.type == TOKEN_KW_LONG && p->lexer.cur.type == TOKEN_KW_LONG) {
                p->lexer.cur = lexer_next(&p->lexer);
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned long long");
                return n;
            }
            /* "unsigned long" or "unsigned long double" */
            if (second.type == TOKEN_KW_LONG) {
                /* "unsigned long double" */
                if (p->lexer.cur.type == TOKEN_KW_DOUBLE) {
                    p->lexer.cur = lexer_next(&p->lexer);
                    AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                    if (n) n->data.id = strdup("unsigned long double");
                    return n;
                }
                /* "unsigned long" */
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned long");
                return n;
            }
            /* "unsigned float" */
            if (second.type == TOKEN_KW_FLOAT) {
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned float");
                return n;
            }
            /* "unsigned double" */
            if (second.type == TOKEN_KW_DOUBLE) {
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned double");
                return n;
            }
            /* "unsigned char" */
            if (second.type == TOKEN_KW_CHAR) {
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned char");
                return n;
            }
            /* "unsigned short" */
            if (second.type == TOKEN_KW_SHORT) {
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned short");
                return n;
            }
            /* "unsigned int" */
            if (second.type == TOKEN_KW_INT) {
                AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
                if (n) n->data.id = strdup("unsigned int");
                return n;
            }
            /* unsigned + unresolved keyword -> "unsigned int" fallback */
            AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
            if (n) n->data.id = strdup("unsigned int");
            return n;
        }
        /* Bare "unsigned" = "unsigned int" */
        AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
        if (n) n->data.id = strdup("unsigned int");
        return n;
    }

    /* Handle multi-word types: "long long", "long double" */
    if (first.type == TOKEN_KW_LONG && is_type_token(p->lexer.cur.type)) {
        Token second = p->lexer.cur;
        if (second.type == TOKEN_KW_LONG || second.type == TOKEN_KW_DOUBLE) {
            p->lexer.cur = lexer_next(&p->lexer);
            const char *combined = (second.type == TOKEN_KW_LONG) ? "long long" : "long double";
            AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
            if (n) n->data.id = strdup(combined);
            return n;
        }
        AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
        if (n) {
            char *s = (char*)malloc(first.len + 1);
            if (s) { memcpy(s, first.start, first.len); s[first.len] = '\0'; n->data.id = s; }
        }
        return n;
    }

    /* Simple single-word type */
    AstNode *n = ast_alloc(AST_IDENTIFIER, first.line, first.col);
    if (n) {
        char *s = (char*)malloc(first.len + 1);
        if (s) { memcpy(s, first.start, first.len); s[first.len] = '\0'; n->data.id = s; }
    }
    return n;
}

/* ================================================================= */
/*  Block: { stmt stmt ... }                                         */
/* ================================================================= */
static AstNode *parse_block(Parser *p) {
    if (!expect(p, TOKEN_LBRACE)) return ast_alloc(AST_BLOCK, 0, 0);
    AstNode *block = ast_alloc(AST_BLOCK, p->lexer.cur.line, p->lexer.cur.col);
    int cap = 8;
    block->data.block.stmts = malloc(cap * sizeof(AstNode*));
    block->data.block.count = 0;
    while (p->lexer.cur.type != TOKEN_RBRACE && p->lexer.cur.type != TOKEN_EOF) {
        if (block->data.block.count >= cap) {
            cap *= 2;
            block->data.block.stmts = realloc(block->data.block.stmts, cap * sizeof(AstNode*));
        }
        block->data.block.stmts[block->data.block.count++] = parse_stmt(p);
    }
    p->lexer.cur = lexer_next(&p->lexer); /* consume '}' */
    return block;
}

/* ================================================================= */
/*  Function definition: type name(params...) { body }               */
/* ================================================================= */
static AstNode *parse_func(Parser *p) {
    AstNode *type = parse_type(p);
    if (!type) {
        int line = p->lexer.cur.line;
        int col = p->lexer.cur.col;
        fprintf(stderr, "%s:%d:%d: error: expected type at start of function, found '%s'\n",
                p->lexer.src_name, line, col, token_name(p->lexer.cur.type));
        if (p->lexer.src) {
            fprintf(stderr, "  ");
            print_source_line(p->lexer.src, line, stderr);
            fprintf(stderr, "  ");
            for (int i = 1; i < col; i++) fputc(' ', stderr);
            fprintf(stderr, "^\n");
        }
        p->error_count++;
        return NULL;
    }
    Token name_tok = p->lexer.cur;
    if (!expect(p, TOKEN_IDENTIFIER)) return NULL;
    if (!expect(p, TOKEN_LPAREN)) return NULL;

    AstNode *func = ast_alloc(AST_FUNCTION_DEF, name_tok.line, name_tok.col);
    func->data.func.type = type;
    {
        char *s = (char*)malloc(name_tok.len + 1);
        if (s) { memcpy(s, name_tok.start, name_tok.len); s[name_tok.len] = '\0'; func->data.func.name = s; }
    }
    func->data.func.params = NULL;
    func->data.func.pcount = 0;
    int cap = 4;
    func->data.func.params = malloc(cap * sizeof(AstNode*));

    while (p->lexer.cur.type != TOKEN_RPAREN && p->lexer.cur.type != TOKEN_EOF) {
        if (func->data.func.pcount >= cap) {
            cap *= 2;
            func->data.func.params = realloc(func->data.func.params, cap * sizeof(AstNode*));
        }
        AstNode *ptype = parse_type(p);
        if (!ptype) break;
        Token pname = p->lexer.cur;
        if (!expect(p, TOKEN_IDENTIFIER)) break;
        AstNode *pvar = ast_alloc(AST_VAR_DECL, pname.line, pname.col);
        pvar->data.var.type = ptype;
        {
            char *s = (char*)malloc(pname.len + 1);
            if (s) { memcpy(s, pname.start, pname.len); s[pname.len] = '\0'; pvar->data.var.name = s; }
        }
        func->data.func.params[func->data.func.pcount++] = pvar;
        accept(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RPAREN);
    func->data.func.body = parse_block(p);
    return func;
}

/* ================================================================= */
/*  Statement parsing                                                 */
/* ================================================================= */
static AstNode *parse_stmt(Parser *p) {
    Token t = p->lexer.cur;

    /* Variable declaration: type name [= expr] or type name[] [= string] */
    if (is_type_token(t.type)) {
        AstNode *type = parse_type(p);
        Token name_tok = p->lexer.cur;
        if (!expect(p, TOKEN_IDENTIFIER)) return NULL;

        /* Check for array syntax: name[] or name[count] */
        if (p->lexer.cur.type == TOKEN_LBRACKET) {
            p->lexer.cur = lexer_next(&p->lexer);
            if (p->lexer.cur.type != TOKEN_RBRACKET) {
                while (p->lexer.cur.type != TOKEN_RBRACKET && p->lexer.cur.type != TOKEN_SEMI && p->lexer.cur.type != TOKEN_EOF)
                    p->lexer.cur = lexer_next(&p->lexer);
            }
            expect(p, TOKEN_RBRACKET);
        }

        AstNode *var = ast_alloc(AST_VAR_DECL, name_tok.line, name_tok.col);
        var->data.var.type = type;
        {
            char *s = (char*)malloc(name_tok.len + 1);
            if (s) { memcpy(s, name_tok.start, name_tok.len); s[name_tok.len] = '\0'; var->data.var.name = s; }
        }
        if (accept(p, TOKEN_EQ))
            var->data.var.init = parse_expr(p);
        expect(p, TOKEN_SEMI);
        return var;
    }

    /* return expr; */
    if (accept(p, TOKEN_KW_RETURN)) {
        AstNode *ret = ast_alloc(AST_RETURN, t.line, t.col);
        if (p->lexer.cur.type != TOKEN_SEMI)
            ret->data.ret.val = parse_expr(p);
        expect(p, TOKEN_SEMI);
        return ret;
    }

    /* if (cond) stmt [else stmt] */
    if (accept(p, TOKEN_KW_IF)) {
        expect(p, TOKEN_LPAREN);
        AstNode *n = ast_alloc(AST_IF, t.line, t.col);
        n->data.if_stmt.cond = parse_expr(p);
        expect(p, TOKEN_RPAREN);
        n->data.if_stmt.then = parse_stmt(p);
        if (accept(p, TOKEN_KW_ELSE))
            n->data.if_stmt.els = parse_stmt(p);
        return n;
    }

    /* while (cond) stmt */
    if (accept(p, TOKEN_KW_WHILE)) {
        expect(p, TOKEN_LPAREN);
        AstNode *n = ast_alloc(AST_WHILE, t.line, t.col);
        n->data.while_stmt.cond = parse_expr(p);
        expect(p, TOKEN_RPAREN);
        n->data.while_stmt.body = parse_stmt(p);
        return n;
    }

    /* { block } */
    if (t.type == TOKEN_LBRACE)
        return parse_block(p);

    /* expression statement */
    AstNode *e = parse_expr(p);
    expect(p, TOKEN_SEMI);
    return e;
}

/* ================================================================= */
/*  Expression parsing                                                */
/* ================================================================= */
static AstNode *parse_primary(Parser *p) {
    Token t = p->lexer.cur;

    if (t.type == TOKEN_NUMBER_INT) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_LITERAL_INT, t.line, t.col);
        n->data.int_val = strtol(t.start, NULL, 0);
        return n;
    }

    if (t.type == TOKEN_NUMBER_FLOAT) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_LITERAL_FLOAT, t.line, t.col);
        n->data.float_val = strtod(t.start, NULL);
        return n;
    }

    if (t.type == TOKEN_STRING) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_STRING, t.line, t.col);
        {
            char *s = (char*)malloc(t.len + 1);
            if (s) { memcpy(s, t.start, t.len); s[t.len] = '\0'; n->data.str_val = s; }
        }
        return n;
    }

    if (t.type == TOKEN_CHAR_LIT) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_LITERAL_INT, t.line, t.col);
        if (t.len == 1) {
            n->data.int_val = (unsigned char)t.start[0];
        } else if (t.start[0] == '\\' && t.len > 1) {
            switch (t.start[1]) {
            case 'n': n->data.int_val = '\n'; break;
            case 't': n->data.int_val = '\t'; break;
            case 'r': n->data.int_val = '\r'; break;
            case '0': n->data.int_val = '\0'; break;
            case '\\': n->data.int_val = '\\'; break;
            case '\'': n->data.int_val = '\''; break;
            case 'a': n->data.int_val = '\a'; break;
            case 'b': n->data.int_val = '\b'; break;
            case 'x': n->data.int_val = (t.len > 2) ? (int)strtol(t.start+2, NULL, 16) : 0; break;
            default: n->data.int_val = (unsigned char)t.start[1]; break;
            }
        } else {
            n->data.int_val = (unsigned char)t.start[0];
        }
        return n;
    }

    /* true / false keywords */
    if (t.type == TOKEN_KW_TRUE) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_LITERAL_INT, t.line, t.col);
        n->data.int_val = 1;
        return n;
    }
    if (t.type == TOKEN_KW_FALSE) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_LITERAL_INT, t.line, t.col);
        n->data.int_val = 0;
        return n;
    }

    if (t.type == TOKEN_IDENTIFIER) {
        p->lexer.cur = lexer_next(&p->lexer);
        /* Function call if next is '(' */
        if (p->lexer.cur.type == TOKEN_LPAREN) {
            p->lexer.cur = lexer_next(&p->lexer);
            AstNode *n = ast_alloc(AST_FUNC_CALL, t.line, t.col);
            {
                char *s = (char*)malloc(t.len + 1);
                if (s) { memcpy(s, t.start, t.len); s[t.len] = '\0'; n->data.call.name = s; }
            }
            int cap = 4;
            n->data.call.args = malloc(cap * sizeof(AstNode*));
            n->data.call.acount = 0;
            while (p->lexer.cur.type != TOKEN_RPAREN && p->lexer.cur.type != TOKEN_EOF) {
                if (n->data.call.acount >= cap) {
                    cap *= 2;
                    n->data.call.args = realloc(n->data.call.args, cap * sizeof(AstNode*));
                }
                n->data.call.args[n->data.call.acount++] = parse_expr(p);
                accept(p, TOKEN_COMMA);
            }
            expect(p, TOKEN_RPAREN);
            return n;
        }
        AstNode *n = ast_alloc(AST_IDENTIFIER, t.line, t.col);
        {
            char *s = (char*)malloc(t.len + 1);
            if (s) { memcpy(s, t.start, t.len); s[t.len] = '\0'; n->data.id = s; }
        }
        return n;
    }

    if (t.type == TOKEN_LPAREN) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = parse_expr(p);
        expect(p, TOKEN_RPAREN);
        return n;
    }

    /* Unary operators */
    if (t.type == TOKEN_MINUS || t.type == TOKEN_BANG || t.type == TOKEN_TILDE) {
        p->lexer.cur = lexer_next(&p->lexer);
        AstNode *n = ast_alloc(AST_UNARY_OP, t.line, t.col);
        n->data.un.op = t.type;
        n->data.un.operand = parse_primary(p);
        return n;
    }

    return NULL;
}

/* Simple expression parser with operator precedence */
static int get_prec(TokenType t) {
    switch (t) {
    case TOKEN_OROR:  return 1;
    case TOKEN_ANDAND: return 2;
    case TOKEN_PIPE:  return 3;
    case TOKEN_CARET: return 4;
    case TOKEN_AND:   return 5;
    case TOKEN_EQEQ:
    case TOKEN_BANGEQ: return 6;
    case TOKEN_LT:
    case TOKEN_GT:
    case TOKEN_LE:
    case TOKEN_GE:    return 7;
    case TOKEN_LSHIFT:
    case TOKEN_RSHIFT: return 8;
    case TOKEN_PLUS:
    case TOKEN_MINUS:  return 9;
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT: return 10;
    default: return -1;
    }
}

static int is_binop(TokenType t) {
    return get_prec(t) != -1;
}

static AstNode *parse_expr_bp(Parser *p, int min_prec) {
    AstNode *left = parse_primary(p);
    if (!left) return NULL;

    while (is_binop(p->lexer.cur.type) && get_prec(p->lexer.cur.type) >= min_prec) {
        int prec = get_prec(p->lexer.cur.type);
        if (p->lexer.cur.type == TOKEN_EQ) {
            AstNode *n = ast_alloc(AST_BINARY_OP, p->lexer.cur.line, p->lexer.cur.col);
            n->data.bin.op = p->lexer.cur.type;
            n->data.bin.left = left;
            p->lexer.cur = lexer_next(&p->lexer);
            n->data.bin.right = parse_expr_bp(p, prec);
            left = n;
            break;
        }
        AstNode *n = ast_alloc(AST_BINARY_OP, p->lexer.cur.line, p->lexer.cur.col);
        n->data.bin.op = p->lexer.cur.type;
        n->data.bin.left = left;
        p->lexer.cur = lexer_next(&p->lexer);
        n->data.bin.right = parse_expr_bp(p, prec + 1);
        left = n;
    }
    return left;
}

static AstNode *parse_expr(Parser *p) {
    return parse_expr_bp(p, 1);
}

/* ================================================================= */
/*  Main API                                                          */
/* ================================================================= */

Parser parser_new(const char *src, const char *name) {
    Parser p;
    p.lexer = lexer_new(src, name);
    p.error_count = 0;
    return p;
}

AstNode *parser_parse(Parser *p) {
    p->lexer.cur = lexer_next(&p->lexer);

    AstNode *prog = ast_alloc(AST_PROGRAM, 1, 1);
    int cap = 8;
    prog->data.program.stmts = malloc(cap * sizeof(AstNode*));
    prog->data.program.count = 0;

    while (p->lexer.cur.type != TOKEN_EOF) {
        if (prog->data.program.count >= cap) {
            cap *= 2;
            prog->data.program.stmts = realloc(prog->data.program.stmts, cap * sizeof(AstNode*));
        }
        AstNode *f = parse_func(p);
        if (f)
            prog->data.program.stmts[prog->data.program.count++] = f;
        else
            break;
    }
    return prog;
}
