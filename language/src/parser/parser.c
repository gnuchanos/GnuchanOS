#include "parser.h"
#include "tokens.h"
#include "ast.h"
#include "errors.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ── Helper: safe strdup (C99 compatible) ──────────────────
static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len + 1);
    return d;
}

// ── Helper: bounded strdup from a token's lexeme ──────────
// Lexeme pointers are into a source buffer with no \0 at boundaries,
// so strlen would copy the rest of the file. Use tok.length instead.
static char *token_dup(GclToken *tok) {
    if (!tok || !tok->lexeme || tok->length <= 0) return strdup_safe("");
    char *d = (char *)malloc(tok->length + 1);
    if (!d) return NULL;
    memcpy(d, tok->lexeme, tok->length);
    d[tok->length] = '\0';
    return d;
}

// ============================================================
// GCL Parser — Recursive Descent Implementation
// ============================================================

// ── Forward declarations ──────────────────────────────────
static GclAstNode *parse_declaration(GclParser *p);
static GclAstNode *parse_statement(GclParser *p);
static GclAstNode *parse_expression(GclParser *p);
static GclType    *parse_type(GclParser *p);
static GclAstNode *parse_block(GclParser *p);

// ── Helpers ────────────────────────────────────────────────
static void parser_advance(GclParser *p) {
    p->previous = p->current;
    p->current = lexer_next_token(&p->lexer);

    // Skip comment/preprocessor tokens in stream
    while (p->current.type == TOK_GCL_COMMENT ||
           p->current.type == TOK_GCL_COMMENT_BLOCK ||
           p->current.type == TOK_GCL_COMMENT_CPP) {
        p->current = lexer_next_token(&p->lexer);
    }
}

static int parser_check(GclParser *p, GclTokenType type) {
    return p->current.type == type;
}

static int parser_match(GclParser *p, GclTokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

static int parser_match_any(GclParser *p, int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        GclTokenType t = va_arg(args, GclTokenType);
        if (parser_check(p, t)) {
            parser_advance(p);
            va_end(args);
            return 1;
        }
    }
    va_end(args);
    return 0;
}

static GclToken parser_consume(GclParser *p, GclTokenType type, const char *msg) {
    if (parser_check(p, type)) {
        GclToken tok = p->current;
        parser_advance(p);
        return tok;
    }
    // Error
    GclSourceLoc loc = {p->filename, p->current.line, p->current.col};
    error_syntax(E003, loc, msg, NULL, NULL);
    p->had_error = 1;
    p->panic_mode = 1;
    return p->current;
}

static int parser_peek_is_type(GclParser *p) {
    switch (p->current.type) {
    case TOK_INT: case TOK_CHAR: case TOK_SHORT: case TOK_LONG:
    case TOK_FLOAT: case TOK_DOUBLE: case TOK_VOID: case TOK_BOOL:
    case TOK_SIGNED: case TOK_UNSIGNED:
    case TOK_INT8: case TOK_INT16: case TOK_INT32: case TOK_INT64: case TOK_INT128:
    case TOK_UINT8: case TOK_UINT16: case TOK_UINT32: case TOK_UINT64:
    case TOK_STRUCT: case TOK_ENUM: case TOK_UNION: case TOK_TYPEDEF:
    case TOK_CONST:
        return 1;
    default:
        return 0;
    }
}

static void parser_sync(GclParser *p) {
    p->panic_mode = 0;
    while (p->current.type != TOK_EOF) {
        if (p->previous.type == TOK_SEMICOLON) return;
        switch (p->current.type) {
        case TOK_SEMICOLON:
            parser_advance(p);
            return;
        case TOK_INT: case TOK_CHAR: case TOK_VOID: case TOK_STRUCT:
        case TOK_ENUM: case TOK_TYPEDEF:
        case TOK_IF: case TOK_FOR: case TOK_WHILE: case TOK_RETURN:
        case TOK_RPAREN: case TOK_RBRACE: /* stop at end-of-scope tokens */
            parser_advance(p);
            return;
        default:
            break;
        }
        parser_advance(p);
    }
}

// ── Type parsing ──────────────────────────────────────────

static int is_type_token(GclTokenType t) {
    switch (t) {
    case TOK_INT: case TOK_CHAR: case TOK_SHORT: case TOK_LONG:
    case TOK_FLOAT: case TOK_DOUBLE: case TOK_VOID: case TOK_BOOL:
    case TOK_SIGNED: case TOK_UNSIGNED:
    case TOK_INT8: case TOK_INT16: case TOK_INT32: case TOK_INT64: case TOK_INT128:
    case TOK_UINT8: case TOK_UINT16: case TOK_UINT32: case TOK_UINT64:
    case TOK_STRUCT: case TOK_ENUM: case TOK_UNION:
        return 1;
    default:
        return 0;
    }
}

static GclType *parse_base_type(GclParser *p) {
    int is_unsigned = 0;

    if (parser_match(p, TOK_UNSIGNED)) {
        is_unsigned = 1;
    } else if (parser_match(p, TOK_SIGNED)) {
        is_unsigned = 0;
    }

    GclTokenType t = p->current.type;

    // Check for struct/enum/union
    if (t == TOK_STRUCT) {
        parser_advance(p);
        // Optional name
        char *name = NULL;
        if (parser_check(p, TOK_IDENTIFIER)) {
            name = token_dup(&p->current);
            parser_advance(p);
        }
        // Optional body
        GclAstList *members = NULL;
        if (parser_match(p, TOK_LBRACE)) {
            members = ast_list_create();
            while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                // struct member = type name ;
                GclType *mtype = parse_type(p);
                char *mname = token_dup(&p->current);
                (void)mname; /* consumed below via token */
                parser_consume(p, TOK_IDENTIFIER, "expected member name");
                parser_consume(p, TOK_SEMICOLON, "expected ';' after struct member");
                GclAstNode *mdecl = ast_var_decl(mtype, mname, NULL,
                                                  p->previous.line, p->previous.col);
                ast_list_append(members, mdecl);
                free(mname);
            }
            parser_consume(p, TOK_RBRACE, "expected '}' after struct body");
        }
        GclType *stype = type_struct(name);
        free(name);
        return stype;
    }

    if (t == TOK_ENUM) {
        parser_advance(p);
        char *name = NULL;
        if (parser_check(p, TOK_IDENTIFIER)) {
            name = token_dup(&p->current);
            parser_advance(p);
        }
        // Optional body
        if (parser_match(p, TOK_LBRACE)) {
            while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                char *mname = token_dup(&p->current);
                (void)mname;
                parser_consume(p, TOK_IDENTIFIER, "expected enum member name");
                free(mname);
                // Optional = value
                GclAstNode *val = NULL;
                if (parser_match(p, TOK_ASSIGN)) {
                    val = parse_expression(p);
                    (void)val;
                }
                if (parser_check(p, TOK_COMMA)) parser_advance(p);
            }
            parser_consume(p, TOK_RBRACE, "expected '}' after enum body");
            free(NULL); // stub — full enum parse deferred
        }
        GclType *etype = type_enum(name);
        free(name);
        return etype;
    }

    if (t == TOK_UNION) {
        parser_advance(p);
        // stub
        GclType *utype = type_union("");
        return utype;
    }

    // Primitive / fixed-width types
    if (t == TOK_INT) {
        parser_advance(p);
        if (is_unsigned) return type_unsigned_int();
        return type_int();
    }
    if (t == TOK_CHAR) {
        parser_advance(p);
        return type_char();
    }
    if (t == TOK_SHORT) {
        parser_advance(p);
        // could be short int
        if (parser_match(p, TOK_INT)) {}
        return type_short();
    }
    if (t == TOK_LONG) {
        parser_advance(p);
        if (parser_match(p, TOK_LONG)) {
            return type_long_long();
        }
        if (parser_match(p, TOK_INT)) {}
        if (parser_match(p, TOK_DOUBLE)) {
            return type_long_double();
        }
        return type_long();
    }
    if (t == TOK_FLOAT) {
        parser_advance(p);
        if (is_unsigned) return type_unsigned_float();
        return type_float();
    }
    if (t == TOK_DOUBLE) {
        parser_advance(p);
        if (is_unsigned) return type_unsigned_double();
        return type_double();
    }
    if (t == TOK_VOID) {
        parser_advance(p);
        return type_void();
    }
    if (t == TOK_BOOL) {
        parser_advance(p);
        return type_bool();
    }
    if (t == TOK_INT8)   { parser_advance(p); return type_int8(); }
    if (t == TOK_INT16)  { parser_advance(p); return type_int16(); }
    if (t == TOK_INT32)  { parser_advance(p); return type_int32(); }
    if (t == TOK_INT64)  { parser_advance(p); return type_int64(); }
    if (t == TOK_INT128) { parser_advance(p); return type_int128(); }
    if (t == TOK_UINT8)  { parser_advance(p); return type_uint8(); }
    if (t == TOK_UINT16) { parser_advance(p); return type_uint16(); }
    if (t == TOK_UINT32) { parser_advance(p); return type_uint32(); }
    if (t == TOK_UINT64) { parser_advance(p); return type_uint64(); }

    // typedef'd name
    if (t == TOK_IDENTIFIER) {
        // For now, treat any unknown identifier as a typedef alias
        // Proper symbol table integration later
        parser_advance(p);
        return type_primitive("int", 0); // placeholder
    }

    return type_int(); // fallback
}

static GclType *parse_type(GclParser *p) {
    GclType *base = parse_base_type(p);

    // Pointer: *
    while (parser_match(p, TOK_STAR)) {
        base = type_pointer(base);
    }

    return base;
}

// ── Expression parsing (Pratt parser style by precedence) ─

static GclAstNode *parse_primary(GclParser *p) {
    // Literals
    if (parser_match(p, TOK_INT_LITERAL)) {
        return ast_int_literal(p->previous.value.int_val,
                              p->previous.line, p->previous.col);
    }
    if (parser_match(p, TOK_FLOAT_LITERAL)) {
        return ast_float_literal(p->previous.value.float_val, 0,
                                p->previous.line, p->previous.col);
    }
    if (parser_match(p, TOK_CHAR_LITERAL)) {
        return ast_char_literal(p->previous.value.char_val,
                               p->previous.line, p->previous.col);
    }
    if (parser_match(p, TOK_STRING_LITERAL)) {
        // Extract unquoted string
        int len = p->previous.length;
        char *str = (char *)malloc(len + 1);
        int slen = 0;
        const char *s = p->previous.lexeme;
        for (int i = 0; i < len; i++) {
            if (s[i] == '"') continue;
            if (s[i] == '\\' && i + 1 < len) {
                i++;
                switch (s[i]) {
                case 'n':  str[slen++] = '\n'; break;
                case 't':  str[slen++] = '\t'; break;
                case 'r':  str[slen++] = '\r'; break;
                case '0':  str[slen++] = '\0'; break;
                case '\\': str[slen++] = '\\'; break;
                case '\"': str[slen++] = '\"'; break;
                default:   str[slen++] = s[i]; break;
                }
            } else {
                str[slen++] = s[i];
            }
        }
        str[slen] = '\0';
        GclAstNode *node = ast_string_literal(str, p->previous.line, p->previous.col);
        free(str);
        return node;
    }
    if (parser_match(p, TOK_TRUE)) {
        return ast_bool_literal(1, p->previous.line, p->previous.col);
    }
    if (parser_match(p, TOK_FALSE)) {
        return ast_bool_literal(0, p->previous.line, p->previous.col);
    }

    // Identifier (including builtin function keywords; sizeof is NOT here)
    if (parser_match(p, TOK_IDENTIFIER) ||
        parser_match(p, TOK_MALLOC) || parser_match(p, TOK_CALLOC) ||
        parser_match(p, TOK_REALLOC) || parser_match(p, TOK_FREE) ||
        parser_match(p, TOK_STRLEN)) {
        char *name = token_dup(&p->previous);
        GclAstNode *node = ast_identifier(name, p->previous.line, p->previous.col);
        free(name);
        return node;
    }

    // Parenthesized expression
    if (parser_match(p, TOK_LPAREN)) {
        // Could be cast: (type)expr or just (expr)
        if (is_type_token(p->current.type)) {
            GclType *ctype = type_clone(parse_type(p));
            if (parser_match(p, TOK_RPAREN)) {
                GclAstNode *expr = parse_expression(p);
                GclAstNode *node = ast_cast(ctype, expr);
                return node;
            }
            // Not a cast, fall through — but we already consumed type...
            // For simplicity, treat as error recovery
        }
        GclAstNode *expr = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')'");
        return expr;
    }

    // sizeof
    if (parser_match(p, TOK_SIZEOF_BUILTIN)) {
        if (parser_match(p, TOK_LPAREN)) {
            if (is_type_token(p->current.type)) {
                GclType *t = parse_type(p);
                parser_consume(p, TOK_RPAREN, "expected ')' after sizeof(type)");
                return ast_sizeof_type(t, p->previous.line, p->previous.col);
            }
            GclAstNode *expr = parse_expression(p);
            parser_consume(p, TOK_RPAREN, "expected ')'");
            return ast_sizeof_expr(expr);
        }
        GclAstNode *expr = parse_primary(p);
        return ast_sizeof_expr(expr);
    }

    // Error
    GclSourceLoc loc = {p->filename, p->current.line, p->current.col};
    error_syntax(E004, loc, "expected expression", NULL, NULL);
    p->had_error = 1;
    return NULL;
}

static GclAstNode *parse_postfix(GclParser *p) {
    GclAstNode *node = parse_primary(p);

    for (;;) {
        if (parser_match(p, TOK_LBRACKET)) {
            GclAstNode *index = parse_expression(p);
            parser_consume(p, TOK_RBRACKET, "expected ']'");
            node = ast_subscript(node, index);
        } else if (parser_match(p, TOK_LPAREN)) {
            GclAstList *args = ast_list_create();
            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    ast_list_append(args, parse_expression(p));
                } while (parser_match(p, TOK_COMMA));
            }
            parser_consume(p, TOK_RPAREN, "expected ')' after function arguments");
            node = ast_call(node, args);
        } else if (parser_match(p, TOK_DOT)) {
            char *member = token_dup(&p->current);
            parser_consume(p, TOK_IDENTIFIER, "expected member name");
            node = ast_member(node, member);
            free(member);
        } else if (parser_match(p, TOK_ARROW)) {
            char *member = token_dup(&p->current);
            parser_consume(p, TOK_IDENTIFIER, "expected member name");
            GclAstNode *mn = ast_member(node, member);
            free(member);
            node = mn; // AST_MEMBER_PTR handled via codegen
        } else if (parser_match(p, TOK_INC)) {
            node = ast_unary(TOK_INC, node);
        } else if (parser_match(p, TOK_DEC)) {
            node = ast_unary(TOK_DEC, node);
        } else {
            break;
        }
    }
    return node;
}

static GclAstNode *parse_unary(GclParser *p) {
    if (parser_match_any(p, 5, TOK_PLUS, TOK_MINUS, TOK_BANG, TOK_TILDE, TOK_STAR)) {
        GclTokenType op = p->previous.type;
        GclAstNode *operand = parse_unary(p);
        return ast_unary(op, operand);
    }
    if (parser_match(p, TOK_INC)) {
        GclAstNode *operand = parse_unary(p);
        return ast_unary(TOK_INC, operand);
    }
    if (parser_match(p, TOK_DEC)) {
        GclAstNode *operand = parse_unary(p);
        return ast_unary(TOK_DEC, operand);
    }
    if (parser_match(p, TOK_AMPERSAND)) {
        GclAstNode *operand = parse_unary(p);
        return ast_unary(TOK_AMPERSAND, operand);
    }
    return parse_postfix(p);
}

static GclAstNode *parse_multiplicative(GclParser *p) {
    GclAstNode *node = parse_unary(p);
    while (parser_match_any(p, 3, TOK_STAR, TOK_SLASH, TOK_PERCENT)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_unary(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_additive(GclParser *p) {
    GclAstNode *node = parse_multiplicative(p);
    while (parser_match_any(p, 2, TOK_PLUS, TOK_MINUS)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_multiplicative(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_shift(GclParser *p) {
    GclAstNode *node = parse_additive(p);
    while (parser_match_any(p, 2, TOK_LSHIFT, TOK_RSHIFT)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_additive(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_relational(GclParser *p) {
    GclAstNode *node = parse_shift(p);
    while (parser_match_any(p, 4, TOK_GT, TOK_LT, TOK_GE, TOK_LE)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_shift(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_equality(GclParser *p) {
    GclAstNode *node = parse_relational(p);
    while (parser_match_any(p, 2, TOK_EQ, TOK_NE)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_relational(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_bitwise_and(GclParser *p) {
    GclAstNode *node = parse_equality(p);
    while (parser_match(p, TOK_AMPERSAND)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_equality(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_bitwise_xor(GclParser *p) {
    GclAstNode *node = parse_bitwise_and(p);
    while (parser_match(p, TOK_CARET)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_bitwise_and(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_bitwise_or(GclParser *p) {
    GclAstNode *node = parse_bitwise_xor(p);
    while (parser_match(p, TOK_PIPE)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_bitwise_xor(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_logical_and(GclParser *p) {
    GclAstNode *node = parse_bitwise_or(p);
    while (parser_match(p, TOK_AND)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_bitwise_or(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_logical_or(GclParser *p) {
    GclAstNode *node = parse_logical_and(p);
    while (parser_match(p, TOK_OR)) {
        GclTokenType op = p->previous.type;
        GclAstNode *rhs = parse_logical_and(p);
        node = ast_binary(op, node, rhs);
    }
    return node;
}

static GclAstNode *parse_ternary(GclParser *p) {
    GclAstNode *node = parse_logical_or(p);
    if (parser_match(p, TOK_QUESTION)) {
        GclAstNode *t = parse_expression(p);
        parser_consume(p, TOK_COLON, "expected ':' in ternary expression");
        GclAstNode *f = parse_ternary(p);
        node = ast_ternary(node, t, f);
    }
    return node;
}

static GclAstNode *parse_assignment(GclParser *p) {
    GclAstNode *node = parse_ternary(p);

    GclTokenType assign_ops[] = {
        TOK_ASSIGN, TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ,
        TOK_PERCENT_EQ, TOK_AMP_EQ, TOK_PIPE_EQ, TOK_CARET_EQ,
        TOK_LSHIFT_EQ, TOK_RSHIFT_EQ
    };

    for (int i = 0; i < 11; i++) {
        if (parser_match(p, assign_ops[i])) {
            GclTokenType op = p->previous.type;
            GclAstNode *rhs = parse_assignment(p);
            node = ast_assign(op, node, rhs);
            break;
        }
    }

    return node;
}

static GclAstNode *parse_expression(GclParser *p) {
    return parse_assignment(p);
}

// ── Array initializer ─────────────────────────────────────
static GclAstNode *parse_array_initializer(GclParser *p) {
    if (!parser_match(p, TOK_LBRACE)) return NULL;

    GclAstList *elements = ast_list_create();

    if (!parser_check(p, TOK_RBRACE)) {
        do {
            if (parser_check(p, TOK_LBRACE)) {
                ast_list_append(elements, parse_array_initializer(p));
            } else {
                ast_list_append(elements, parse_assignment(p));
            }
        } while (parser_match(p, TOK_COMMA));
    }

    parser_consume(p, TOK_RBRACE, "expected '}'");
    return ast_array_init(elements, p->previous.line, p->previous.col);
}

// ── Variable declaration ──────────────────────────────────
static GclAstNode *parse_var_decl(GclParser *p, GclType *type) {
    // type name [ = expr ] {, name [ = expr ]} ;
    GclAstNode *first = NULL;
    GclAstNode *last = NULL;

    do {
        char *name = token_dup(&p->current);
        parser_consume(p, TOK_IDENTIFIER, "expected variable name");

        // Array declaration: name[size] or name[]
        GclType *var_type = type_clone(type);
        if (parser_match(p, TOK_LBRACKET)) {
            int array_size = -1; // unspecified
            if (!parser_check(p, TOK_RBRACKET)) {
                GclAstNode *size_expr = parse_expression(p);
                if (size_expr && size_expr->type == AST_INT_LITERAL) {
                    array_size = (int)size_expr->data.int_lit.int_val;
                }
            }
            parser_consume(p, TOK_RBRACKET, "expected ']'");
            var_type = type_array(var_type, array_size);
        }

        // Initializer
        GclAstNode *init = NULL;
        if (parser_match(p, TOK_ASSIGN)) {
            if (parser_check(p, TOK_LBRACE)) {
                init = parse_array_initializer(p);
            } else if (parser_check(p, TOK_STRING_LITERAL)) {
                init = parse_primary(p);
            } else {
                init = parse_expression(p);
            }
        }

        GclAstNode *decl = ast_var_decl(var_type, name, init,
                                        p->previous.line, p->previous.col);
        free(name);

        if (!first) {
            first = decl;
            last = decl;
        } else {
            last->next = decl;
            last = decl;
        }
    } while (parser_match(p, TOK_COMMA));

    parser_consume(p, TOK_SEMICOLON, "expected ';' after declaration");
    return first;
}

// ── Statement parsing ─────────────────────────────────────
static GclAstNode *parse_statement(GclParser *p) {
    // if
    if (parser_match(p, TOK_IF)) {
        parser_consume(p, TOK_LPAREN, "expected '(' after if");
        GclAstNode *cond = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')'");
        GclAstNode *then_b = parse_statement(p);
        GclAstNode *else_b = NULL;
        if (parser_match(p, TOK_ELSE)) {
            else_b = parse_statement(p);
        }
        return ast_if(cond, then_b, else_b);
    }

    // for
    if (parser_match(p, TOK_FOR)) {
        parser_consume(p, TOK_LPAREN, "expected '(' after for");
        GclAstNode *init = NULL;
        GclAstNode *cond = NULL;
        GclAstNode *incr = NULL;

        if (!parser_check(p, TOK_SEMICOLON)) {
            if (parser_peek_is_type(p)) {
                GclType *t = parse_type(p);
                init = parse_var_decl(p, t);
            } else {
                init = parse_expression(p);
                parser_consume(p, TOK_SEMICOLON, "expected ';'");
            }
        } else {
            parser_advance(p); // ;
        }

        if (!parser_check(p, TOK_SEMICOLON)) {
            cond = parse_expression(p);
        }
        parser_consume(p, TOK_SEMICOLON, "expected ';'");

        if (!parser_check(p, TOK_RPAREN)) {
            incr = parse_expression(p);
        }
        parser_consume(p, TOK_RPAREN, "expected ')'");

        GclAstNode *body = parse_statement(p);
        return ast_for(init, cond, incr, body);
    }

    // while
    if (parser_match(p, TOK_WHILE)) {
        parser_consume(p, TOK_LPAREN, "expected '(' after while");
        GclAstNode *cond = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')'");
        GclAstNode *body = parse_statement(p);
        return ast_while(cond, body);
    }

    // do-while
    if (parser_match(p, TOK_DO)) {
        GclAstNode *body = parse_statement(p);
        parser_consume(p, TOK_WHILE, "expected 'while' after do");
        parser_consume(p, TOK_LPAREN, "expected '('");
        GclAstNode *cond = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')'");
        parser_consume(p, TOK_SEMICOLON, "expected ';' after do-while");
        return ast_do_while(body, cond);
    }

    // switch
    if (parser_match(p, TOK_SWITCH)) {
        parser_consume(p, TOK_LPAREN, "expected '('");
        GclAstNode *expr = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')'");
        parser_consume(p, TOK_LBRACE, "expected '{'");

        GclAstList *cases = ast_list_create();
        while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
            if (parser_match(p, TOK_CASE)) {
                GclAstNode *val = parse_expression(p);
                parser_consume(p, TOK_COLON, "expected ':'");
                GclAstNode *stmt = NULL;
                // Collect statements until next case/default/}
                GclAstList *body_list = ast_list_create();
                while (!parser_check(p, TOK_CASE) && !parser_check(p, TOK_DEFAULT) &&
                       !parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                    ast_list_append(body_list, parse_statement(p));
                }
                if (body_list->count == 1) {
                    stmt = body_list->head;
                } else if (body_list->count > 0) {
                    stmt = ast_compound(body_list);
                }
                ast_list_append(cases, ast_case(val, stmt));
            } else if (parser_match(p, TOK_DEFAULT)) {
                parser_consume(p, TOK_COLON, "expected ':'");
                GclAstList *body_list = ast_list_create();
                while (!parser_check(p, TOK_CASE) && !parser_check(p, TOK_DEFAULT) &&
                       !parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                    ast_list_append(body_list, parse_statement(p));
                }
                GclAstNode *bstmt = body_list->count > 0 ? ast_compound(body_list) : NULL;
                ast_list_append(cases, ast_case(NULL, bstmt));
            } else {
                parser_advance(p); // skip unknown
            }
        }
        parser_consume(p, TOK_RBRACE, "expected '}'");

        GclAstNode *switch_body = ast_compound(cases);
        return ast_switch(expr, switch_body);
    }

    // return
    if (parser_match(p, TOK_RETURN)) {
        GclAstNode *expr = NULL;
        if (!parser_check(p, TOK_SEMICOLON)) {
            expr = parse_expression(p);
        }
        parser_consume(p, TOK_SEMICOLON, "expected ';' after return");
        return ast_return(expr);
    }

    // break
    if (parser_match(p, TOK_BREAK)) {
        parser_consume(p, TOK_SEMICOLON, "expected ';' after break");
        return ast_break(p->previous.line, p->previous.col);
    }

    // continue
    if (parser_match(p, TOK_CONTINUE)) {
        parser_consume(p, TOK_SEMICOLON, "expected ';' after continue");
        return ast_node_create(AST_CONTINUE, p->previous.line, p->previous.col);
    }

    // block
    if (parser_match(p, TOK_LBRACE)) {
        return parse_block(p);
    }

    // Empty statement
    if (parser_match(p, TOK_SEMICOLON)) {
        return ast_node_create(AST_EXPR_STMT, p->previous.line, p->previous.col);
    }

    // Expression statement
    GclAstNode *expr = parse_expression(p);
    parser_consume(p, TOK_SEMICOLON, "expected ';'");
    return expr;
}

// ── Block ─────────────────────────────────────────────────
static GclAstNode *parse_block(GclParser *p) {
    GclAstList *stmts = ast_list_create();
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        ast_list_append(stmts, parse_declaration(p));
    }
    parser_consume(p, TOK_RBRACE, "expected '}'");
    return ast_compound(stmts);
}

// ── Function ──────────────────────────────────────────────
static GclAstNode *parse_function(GclParser *p, GclType *ret_type, const char *name) {
    // ( params ) { body }
    parser_consume(p, TOK_LPAREN, "expected '('");

    GclAstList *params = ast_list_create();
    int is_variadic = 0;

    if (!parser_check(p, TOK_RPAREN)) {
        do {
            if (parser_match(p, TOK_ELLIPSIS)) {
                is_variadic = 1;
                break;
            }
            GclType *ptype = parse_type(p);
            char *pname = NULL;
            if (parser_check(p, TOK_IDENTIFIER)) {
                pname = token_dup(&p->current);
                parser_advance(p);
                // Handle array params: char *argv[] or int arr[10]
                while (parser_match(p, TOK_LBRACKET)) {
                    ptype = type_array(ptype, -1);
                    parser_consume(p, TOK_RBRACKET, "expected ']'");
                }
            }
            GclAstNode *param = ast_func_param(ptype, pname ? pname : "",
                                               p->previous.line, p->previous.col);
            ast_list_append(params, param);
            if (pname) free(pname);
        } while (parser_match(p, TOK_COMMA));
    }

    parser_consume(p, TOK_RPAREN, "expected ')'");

    // Body
    GclAstNode *body = NULL;
    if (parser_match(p, TOK_LBRACE)) {
        body = parse_block(p);
    } else {
        // Function declaration only (no body)
        parser_consume(p, TOK_SEMICOLON, "expected ';' or '{'");
        return ast_func_def(ret_type, name, params, NULL, is_variadic,
                           p->previous.line, p->previous.col);
    }

    return ast_func_def(ret_type, name, params, body, is_variadic,
                       p->previous.line, p->previous.col);
}

// ── Preprocessor parse ────────────────────────────────────
static GclAstNode *parse_preprocessor(GclParser *p) {
    GclToken tok = p->previous;

    switch (tok.type) {
    case TOK_PREP_LIB:
    case TOK_PREP_INCLUDE: {
        // #lib <stdio> → #include <stdio.h>
        // #include <file.gcsf>
        // The token lexeme contains the full line like "#lib <stdio>"
        // Extract path between <...> or "..."
        int is_system = 0;
        char path_buf[256] = {0};
        (void)0; /* pi was here, removed — path extraction uses memcpy */

        // Find the opening < or "
        const char *s = tok.lexeme;
        int len = tok.length;
        const char *open = NULL;
        for (int i = 1; i < len; i++) {
            if (s[i] == '<') { open = &s[i+1]; is_system = 1; break; }
            if (s[i] == '"') { open = &s[i+1]; is_system = 0; break; }
        }
        if (open) {
            const char *close = NULL;
            for (const char *p = open; p < s + len; p++) {
                if ((is_system && *p == '>') || (!is_system && *p == '"')) {
                    close = p;
                    break;
                }
            }
            if (close) {
                int plen = (int)(close - open);
                if (plen > 255) plen = 255;
                memcpy(path_buf, open, plen);
                path_buf[plen] = '\0';
            }
        }

        // If path extraction failed, try consuming < IDENTIFIER > tokens
        if (path_buf[0] == '\0') {
            // Consume < or " if present
            if (parser_match(p, TOK_LT)) is_system = 1;
            else if (parser_match(p, TOK_STRING_LITERAL)) {
                // Already consumed as string
                int slen = p->previous.length;
                if (slen > 255) slen = 255;
                memcpy(path_buf, p->previous.lexeme + 1, slen - 2);
                path_buf[slen - 2] = '\0';
                return ast_prep_include(path_buf, 0, tok.line, tok.col);
            }
            // Read identifier as path
            if (parser_check(p, TOK_IDENTIFIER)) {
                int plen = p->current.length;
                if (plen > 255) plen = 255;
                memcpy(path_buf, p->current.lexeme, plen);
                path_buf[plen] = '\0';
                parser_advance(p);
                // Consume closing > if system include
                if (is_system) parser_match(p, TOK_GT);
            }
        }

        if (path_buf[0] == '\0') {
            // Fallback: empty
            path_buf[0] = '\0';
        }

        GclAstNode *node = ast_prep_include(path_buf, is_system, tok.line, tok.col);
        // Override type for #lib so codegen adds .h suffix
        if (tok.type == TOK_PREP_LIB) node->type = AST_PREP_LIB;
        return node;
    }

    case TOK_PREP_DEFINE: {
        // Parse name and optional value from lexeme
        const char *s = tok.lexeme;
        int len = tok.length;
        char name[128] = {0};
        char value[256] = {0};
        int ni = 0, vi = 0;
        int in_name = 0, in_value = 0;
        for (int i = 0; i < len; i++) {
            if (s[i] == '#' && ni == 0) continue;
            if (!in_name && s[i] != ' ' && s[i] != '\t') {
                in_name = 1;
            }
            if (in_name && !in_value && s[i] == ' ') {
                in_value = 1;
                continue;
            }
            if (in_value) {
                if (vi < 255) value[vi++] = s[i];
            } else if (in_name) {
                if (ni < 127) name[ni++] = s[i];
            }
        }
        name[ni] = '\0';
        value[vi] = '\0';

        // Remove "define" prefix if present
        char *n = name;
        if (strncmp(n, "define", 6) == 0) n += 6;
        while (*n == ' ' || *n == '\t') n++;

        return ast_prep_define(n, vi > 0 ? value : NULL, tok.line, tok.col);
    }

    default:
        // Other preprocessor — pass through as comment for now
        return ast_prep_comment(tok.lexeme, tok.length, tok.line, tok.col);
    }
}

// ── Top-level declaration ─────────────────────────────────
static GclAstNode *parse_declaration(GclParser *p) {
    // Preprocessor: #lib, #include need special cleanup
    if (p->current.type == TOK_PREP_LIB ||
        p->current.type == TOK_PREP_INCLUDE) {
        parser_advance(p); // consume #lib/#include token
        GclAstNode *node = parse_preprocessor(p);
        // Skip any leftover tokens from the preprocessor line
        // (#lib consumes the whole line into one token, so normally none remain)
        // But just in case tokens like < IDENTIFIER > leaked, skip them:
        if (parser_check(p, TOK_LT)) {
            parser_advance(p); // <
            if (parser_check(p, TOK_IDENTIFIER)) {
                parser_advance(p); // stdio
                if (parser_check(p, TOK_GT)) parser_advance(p); // >
            }
        }
        return node;
    }
    if (p->current.type == TOK_PREP_EXTERN ||
        p->current.type == TOK_PREP_DEFINE ||
        p->current.type == TOK_PREP_UNDEF ||
        p->current.type == TOK_PREP_IFDEF ||
        p->current.type == TOK_PREP_IFNDEF ||
        p->current.type == TOK_PREP_IF ||
        p->current.type == TOK_PREP_ELIF ||
        p->current.type == TOK_PREP_ELSE ||
        p->current.type == TOK_PREP_ENDIF ||
        p->current.type == TOK_PREP_ERROR ||
        p->current.type == TOK_PREP_PRAGMA ||
        p->current.type == TOK_PREP_LINE) {
        parser_advance(p);
        return parse_preprocessor(p);
    }

    // typedef
    if (parser_match(p, TOK_TYPEDEF)) {
        GclType *original = parse_type(p);
        char *alias = token_dup(&p->current);
        parser_consume(p, TOK_IDENTIFIER, "expected typedef alias");
        parser_consume(p, TOK_SEMICOLON, "expected ';' after typedef");
        GclAstNode *node = ast_typedef_decl(original, alias,
                                            p->previous.line, p->previous.col);
        free(alias);
        return node;
    }

    // struct/enum/union definitions (without typedef)
    if (p->current.type == TOK_STRUCT || p->current.type == TOK_ENUM || p->current.type == TOK_UNION) {
        GclType *type = parse_type(p);
        // If next is identifier: could be variable declaration using this type
        if (parser_check(p, TOK_IDENTIFIER)) {
            char *name = token_dup(&p->current);
            // Look ahead: if '(' → function, if ';' or '=' or ',' → variable
            // We need to peek
            (void)p->current; /* peeked via parser_check above */
            // Just parse as variable declaration
            free(name);
            // Re-parse: the struct/enum/union was consumed by parse_type, now parse var decl
            return parse_var_decl(p, type);
        }
        if (parser_check(p, TOK_STAR)) {
            // Pointer to struct
            type = type_pointer(type);
            return parse_var_decl(p, type);
        }
        parser_consume(p, TOK_SEMICOLON, "expected ';'");
        return NULL; // standalone struct/enum definition
    }

    // Variable or function declaration
    if (parser_peek_is_type(p)) {
        GclType *type = parse_type(p);

        // Check for additional pointer stars after type
        char *name = NULL;
        if (parser_check(p, TOK_IDENTIFIER)) {
            name = token_dup(&p->current);
        }

        if (name == NULL) {
            // No identifier, just consume semicolon
            parser_consume(p, TOK_SEMICOLON, "expected ';'");
            return NULL;
        }

        // Check if function: look ahead for '('
        parser_advance(p); // consume identifier
        GclAstNode *result = NULL;

        if (parser_check(p, TOK_LPAREN)) {
            // Function
            char *fname = name;
            result = parse_function(p, type, fname);
        } else {
            // Variable declaration (may have array brackets)
            GclType *var_type = type;
            if (parser_match(p, TOK_LBRACKET)) {
                int size = -1;
                if (!parser_check(p, TOK_RBRACKET)) {
                    GclAstNode *sz = parse_expression(p);
                    if (sz && sz->type == AST_INT_LITERAL) size = (int)sz->data.int_lit.int_val;
                }
                parser_consume(p, TOK_RBRACKET, "expected ']'");
                var_type = type_array(var_type, size);
                // If another [, it's multi-dim
                while (parser_match(p, TOK_LBRACKET)) {
                    int sz2 = -1;
                    if (!parser_check(p, TOK_RBRACKET)) {
                        GclAstNode *sz = parse_expression(p);
                        if (sz && sz->type == AST_INT_LITERAL) sz2 = (int)sz->data.int_lit.int_val;
                    }
                    parser_consume(p, TOK_RBRACKET, "expected ']'");
                    var_type = type_array(var_type, sz2);
                }
            }

            // Initializer
            GclAstNode *init = NULL;
            if (parser_match(p, TOK_ASSIGN)) {
                if (parser_check(p, TOK_LBRACE)) {
                    init = parse_array_initializer(p);
                } else if (parser_check(p, TOK_STRING_LITERAL)) {
                    init = parse_primary(p);
                } else {
                    init = parse_expression(p);
                }
            }

            GclAstNode *decl = ast_var_decl(type_clone(var_type), name, init,
                                             p->previous.line, p->previous.col);

            // Handle multi-declaration: type a, b, c;
            GclAstNode *first = decl;
            GclAstNode *last = decl;
            while (parser_match(p, TOK_COMMA)) {
                char *n2 = token_dup(&p->current);
                parser_consume(p, TOK_IDENTIFIER, "expected variable name");
                GclType *t2 = type_clone(type);
                GclAstNode *init2 = NULL;
                if (parser_match(p, TOK_ASSIGN)) {
                    if (parser_check(p, TOK_LBRACE)) {
                        init2 = parse_array_initializer(p);
                    } else {
                        init2 = parse_expression(p);
                    }
                }
                GclAstNode *d2 = ast_var_decl(t2, n2, init2,
                                              p->previous.line, p->previous.col);
                last->next = d2;
                last = d2;
                free(n2);
            }

            parser_consume(p, TOK_SEMICOLON, "expected ';' after declaration");
            result = first;
        }

        free(name);
        return result;
    }

    // Statement
    return parse_statement(p);
}

// ── Parse all ─────────────────────────────────────────────
GclAstNode *parser_parse(GclParser *p) {
    GclAstNode *program = ast_node_create(AST_PROGRAM, 0, 0);
    GclAstList *top_list = ast_list_create();

    parser_advance(p); // load first token

    while (!parser_check(p, TOK_EOF)) {
        GclAstNode *decl = parse_declaration(p);
        if (decl) {
            // Collect ALL nodes from the declaration chain
            GclAstNode *d = decl;
            while (d) {
                GclAstNode *saved_next = d->next;
                d->next = NULL;  // ISOLATE this node
                ast_list_append(top_list, d);
                d = saved_next;
            }
        }
        if (p->panic_mode) parser_sync(p);
    }

    // Wire top_list nodes as program->next chain
    program->next = top_list->head;
    free(top_list); // only free the list struct, not nodes

    return program;
}

// ── Init ──────────────────────────────────────────────────
void parser_init(GclParser *parser, const char *source, const char *filename) {
    lexer_init(&parser->lexer, source);
    parser->current.type = TOK_EOF;
    parser->previous.type = TOK_EOF;
    parser->had_error = 0;
    parser->panic_mode = 0;
    parser->filename = filename;
    parser->dump_tokens = 0;
    parser->dump_ast = 0;
}

int parser_had_error(GclParser *parser) {
    return parser->had_error;
}
