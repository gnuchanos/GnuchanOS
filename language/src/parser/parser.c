/* Parser: recursive descent, operator precedence */
#include "gcl.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"

void parser_init(Parser *p, Lexer *lx, const char *src, DebugFlags d) {
    p->lexer = lx; p->source = src; p->debug = d; p->errors = 0;
}

static void error(Parser *p, int line, int col, const char *msg) {
    fprintf(stderr, "error[E001]: %s\n --> %s:%d:%d\n",
            msg, p->lexer->filename, line, col);
    p->errors++;
}

static Node *parse_expr(Parser *p);

static Token peek(Parser *p) { return lexer_peek(p->lexer); }
static Token advance(Parser *p) { return lexer_advance(p->lexer); }
static int match(Parser *p, TokenType t) { return lexer_match(p->lexer, t); }
static int check(Parser *p, TokenType t) { return peek(p).type == t; }

/* operator precedence */
static int prec(TokenType t) {
    switch (t) {
    case T_QUESTION: case T_COLON: return 1;
    case T_OR: return 2;
    case T_AND: return 3;
    case T_PIPE: return 4;
    case T_CARET: return 5;
    case T_AMPERSAND: return 6;
    case T_EQ: case T_NE: return 7;
    case T_LT: case T_GT: case T_LE: case T_GE: return 8;
    case T_LSHIFT: case T_RSHIFT: return 9;
    case T_PLUS: case T_MINUS: return 10;
    case T_STAR: case T_SLASH: case T_PERCENT: return 11;
    default: return 0;
    }
}

/* Pratt parser: prefix + infix */
static Node *parse_primary(Parser *p) {
    Token t = peek(p);
    SourceLoc loc = t.loc;
    if (t.type == T_NUMBER) { advance(p); return node_int(t.ival, loc); }
    if (t.type == T_CHAR)   { advance(p); return node_char(t.ival, loc); }
    if (t.type == T_INT)    { advance(p); return NULL; } /* handled by decl */
    if (t.type == T_IDENT) {
        advance(p);
        if (check(p, T_LPAREN)) {
            /* function call */
            advance(p);
            Node *callee = node_ident(t.text, loc);
            (void)callee; /* simplified */
            while (!check(p, T_RPAREN) && !check(p, T_EOF)) {
                if (!check(p, T_RPAREN)) advance(p);
                if (check(p, T_COMMA)) advance(p);
            }
            if (check(p, T_RPAREN)) advance(p);
            return node_int(0, loc); /* placeholder */
        }
        return node_ident(t.text, loc);
    }
    if (t.type == T_LPAREN) {
        advance(p); Node *e = parse_expr(p);
        if (check(p, T_RPAREN)) advance(p);
        return e;
    }
    /* prefix operators */
    if (t.type == T_PLUS || t.type == T_MINUS || t.type == T_NOT ||
        t.type == T_TILDE || t.type == T_INC || t.type == T_DEC) {
        advance(p);
        TokenType op = t.type;
        /* -5: negative number literal */
        if (op == T_MINUS && check(p, T_NUMBER)) {
            t = advance(p);
            return node_int(-t.ival, loc);
        }
        if (op == T_PLUS && check(p, T_NUMBER)) {
            t = advance(p);
            return node_int(t.ival, loc);
        }
        return node_unary(op, parse_primary(p), loc);
    }
    if (t.type == T_SIZEOF) {
        advance(p);
        if (check(p, T_LPAREN)) advance(p);
        if (!check(p, T_RPAREN) && !check(p, T_EOF)) advance(p);
        if (check(p, T_RPAREN)) advance(p);
        return node_int(4, loc); /* sizeof(int) = 4 */
    }
    return NULL;
}

static Node *parse_expr(Parser *p) {
    Node *left = parse_primary(p);
    if (!left) return NULL;
    while (1) {
        TokenType op = peek(p).type;
        int p1 = prec(op);
        if (p1 == 0) break;
        /* compound assignment */
        if (op == T_ASSIGN || op == T_ADD_ASSIGN || op == T_SUB_ASSIGN ||
            op == T_MUL_ASSIGN || op == T_DIV_ASSIGN || op == T_MOD_ASSIGN ||
            op == T_AND_ASSIGN || op == T_OR_ASSIGN || op == T_XOR_ASSIGN ||
            op == T_LSHIFT_ASSIGN || op == T_RSHIFT_ASSIGN) {
            advance(p);
            Node *val = parse_expr(p);
            left = node_assign(op, left, val, peek(p).loc);
            break;
        }
        /* ternary */
        if (op == T_QUESTION) {
            advance(p);
            Node *then = parse_expr(p);
            if (check(p, T_COLON)) advance(p);
            Node *el = parse_expr(p);
            Node *t = node_alloc(N_TERNARY, peek(p).loc);
            if (t) { t->cond = left; t->left = then; t->right = el; }
            left = t;
            break;
        }
        /* binary */
        if (check(p, T_LPAREN)) break; /* function call handled in primary */
        advance(p);
        Node *right = parse_primary(p);
        /* handle precedence cascade */
        while (1) {
            TokenType next = peek(p).type;
            int p2 = prec(next);
            if (p2 == 0 || p2 <= p1) break;
            if (next == T_ASSIGN || (next >= T_ADD_ASSIGN && next <= T_RSHIFT_ASSIGN)) break;
            if (next == T_QUESTION) break;
            advance(p);
            right = node_binary(next, right, parse_primary(p), peek(p).loc);
        }
        left = node_binary(op, left, right, peek(p).loc);
    }
    return left;
}

/* parse_assignment: top-level expression entry */
static Node *parse_full_expr(Parser *p) {
    return parse_expr(p);
}

/* parse a declaration: int name = expr; */
static Node *parse_decl(Parser *p) {
    SourceLoc loc = peek(p).loc;
    advance(p); /* consume 'int' */
    if (!check(p, T_IDENT)) {
        error(p, loc.line, loc.col, "expected identifier");
        while (!check(p, T_SEMICOLON) && !check(p, T_EOF)) advance(p);
        if (check(p, T_SEMICOLON)) advance(p);
        return NULL;
    }
    Token name = advance(p);
    Node *n = node_alloc(N_VAR_DECL, name.loc);
    if (n) { strncpy(n->name, name.text, 255); n->name[255] = 0; }
    if (match(p, T_ASSIGN)) {
        Node *val = parse_full_expr(p);
        if (!val) {
            error(p, peek(p).loc.line, peek(p).loc.col, "expected expression");
        } else if (n) {
            n->left = val;
        }
    }
    if (!match(p, T_SEMICOLON))
        error(p, peek(p).loc.line, peek(p).loc.col, "expected ';'");
    return n;
}

Node *parser_parse(Parser *p) {
    Node *head = NULL, *tail = NULL;
    while (!check(p, T_EOF)) {
        Node *n = NULL;
        if (check(p, T_INT)) n = parse_decl(p);
        else {
            error(p, peek(p).loc.line, peek(p).loc.col, "expected declaration");
            while (!check(p, T_SEMICOLON) && !check(p, T_EOF)) advance(p);
            if (check(p, T_SEMICOLON)) advance(p);
        }
        if (n) { if (!head) head = n; else tail->next = n; tail = n; }
    }
    return head;
}

void parser_dump(Node *n, int indent) {
    printf("── AST ──\n");
    ast_dump(n, indent);
}
