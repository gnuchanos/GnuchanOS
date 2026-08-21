#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gcl_parser.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Check file existence relative to source file directory */
static int file_exists_relative(const char *filepath, const char *filename) {
    char path[4096];
    size_t dir_len = 0;
    if (filepath) {
        const char *last_sep = filepath;
        for (const char *p = filepath; *p; p++) {
            if (*p == '/' || *p == '\\') last_sep = p + 1;
        }
        dir_len = (size_t)(last_sep - filepath);
    }
    if (dir_len + strlen(filename) + 1 >= sizeof(path)) return 0;
    if (dir_len > 0) memcpy(path, filepath, dir_len);
    strcpy(path + dir_len, filename);
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

/* ── helpers ─────────────────────────────────────── */

static void parser_advance(GclParser *p) {
    p->current = gcl_lexer_next(&p->lex);
}

static bool parser_check(GclParser *p, GclTokenKind kind) {
    return p->current.kind == kind;
}

static bool parser_match(GclParser *p, GclTokenKind kind) {
    if (p->current.kind == kind) {
        parser_advance(p);
        return true;
    }
    return false;
}

static void parser_expect(GclParser *p, GclTokenKind kind) {
    if (p->current.kind == kind) {
        parser_advance(p);
        return;
    }
    gcl_diag_add(p->diag, GCL_DIAG_ERROR, p->current.line, p->current.col, p->filepath, "expected '%s', got '%s'", gcl_token_kind_name(kind), gcl_token_kind_name(p->current.kind));
}

static const char *tok_str(GclParser *p, GclToken *t) {
    return gcl_intern(p->intern, t->start, t->length);
}

/* Part 6: operator alias lookup — returns the real op text ("==", "&&", ...)
 * when the current IDENT token matches a typedef'd operator alias, else NULL.
 * Non-IDENT tokens return their own text (caller filters by precedence). */
static const char *parser_cur_binop(GclParser *p) {
    if (p->current.kind == TOK_IDENT) {
        const char *name = tok_str(p, &p->current);
        for (int i = 0; i < p->op_alias_count; i++) {
            if (strcmp(p->op_alias_name[i], name) == 0) {
                return p->op_alias_op[i];
            }
        }
        return NULL;
    }
    return tok_str(p, &p->current);
}

/* Part 6: typedef type alias lookup */
static const GclTypeAlias *parser_find_type_alias(GclParser *p, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < p->type_alias_count; i++) {
        if (strcmp(p->type_aliases[i].name, name) == 0) return &p->type_aliases[i];
    }
    return NULL;
}

/* Part 7: register an enum constant */
static void parser_add_enum(GclParser *p, const char *name, int64_t value) {
    if (!name || p->enum_count >= GCL_ENUM_MAX) return;
    size_t nl = strlen(name);
    if (nl >= 128) nl = 127;
    memcpy(p->enum_names[p->enum_count], name, nl);
    p->enum_names[p->enum_count][nl] = '\0';
    p->enum_values[p->enum_count] = value;
    p->enum_count++;
}

/* Reads an enum body: { A, B, C } and registers the constants */
static void parse_enum_body(GclParser *p, int64_t *next_val) {
    parser_advance(p); /* skip '{' */
    int64_t val = *next_val;
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_IDENT)) {
            parser_add_enum(p, tok_str(p, &p->current), val);
            val++;
            parser_advance(p);
        } else if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
        } else {
            parser_advance(p);
        }
    }
    if (parser_check(p, TOK_RBRACE)) parser_advance(p);
    *next_val = val;
}

/* ── forward declarations ────────────────────────── */

static GclAstNode *parse_stmt(GclParser *p);
static GclAstNode *parse_expr(GclParser *p);
static GclAstNode *parse_assign_expr(GclParser *p);
static GclAstNode *parse_or_expr(GclParser *p);
static GclAstNode *parse_and_expr(GclParser *p);
static GclAstNode *parse_bit_or_expr(GclParser *p);
static GclAstNode *parse_bit_xor_expr(GclParser *p);
static GclAstNode *parse_bit_and_expr(GclParser *p);
static GclAstNode *parse_equality_expr(GclParser *p);
static GclAstNode *parse_relational_expr(GclParser *p);
static GclAstNode *parse_shift_expr(GclParser *p);
static GclAstNode *parse_additive_expr(GclParser *p);
static GclAstNode *parse_multiplicative_expr(GclParser *p);
static GclAstNode *parse_unary_expr(GclParser *p);
static GclAstNode *parse_postfix_expr(GclParser *p);
static GclAstNode *parse_primary_expr(GclParser *p);
static GclAstNode *parse_block(GclParser *p);
static GclAstNode *parse_switch_stmt(GclParser *p);
static GclAstNode *parse_do_while_stmt(GclParser *p);
static GclAstNode *parse_class_decl(GclParser *p);

/* ── type check helpers ──────────────────────────── */

static bool is_type_token(GclTokenKind k) {
    switch (k) {
    case TOK_KW_INT: case TOK_KW_INT8: case TOK_KW_INT16: case TOK_KW_INT32:
    case TOK_KW_INT64: case TOK_KW_INT128:
    case TOK_KW_UINT8: case TOK_KW_UINT16: case TOK_KW_UINT32:
    case TOK_KW_UINT64: case TOK_KW_UINT128:
    case TOK_KW_FLOAT: case TOK_KW_FLOAT16: case TOK_KW_FLOAT32:
    case TOK_KW_FLOAT64: case TOK_KW_FLOAT128:
    case TOK_KW_DOUBLE: case TOK_KW_LONG: case TOK_KW_SHORT: case TOK_KW_UNSIGNED:
    case TOK_KW_CHAR: case TOK_KW_GCCHAR: case TOK_KW_BOOL:
    case TOK_KW_VOID:
    case TOK_KW_STRUCT: case TOK_KW_ENUM:
        return true;
    default:
        return false;
    }
}

/* True when the current token is a builtin type or a typedef'd alias */
static bool is_type_or_alias(GclParser *p) {
    if (is_type_token(p->current.kind)) return true;
    if (p->current.kind == TOK_IDENT) {
        return parser_find_type_alias(p, tok_str(p, &p->current)) != NULL;
    }
    return false;
}

/* ── macro table ─────────────────────────────────── */

static GclMacro *parser_find_macro(GclParser *p, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < p->macro_count; i++) {
        if (strcmp(p->macros[i].name, name) == 0) return &p->macros[i];
    }
    return NULL;
}

static void parser_define(GclParser *p, const char *name, const char *value) {
    if (!name || name[0] == '\0') return;
    if (p->macro_count < GCL_MACRO_MAX) {
        GclMacro *m = &p->macros[p->macro_count++];
        size_t nl = strlen(name);
        if (nl >= sizeof m->name) nl = sizeof m->name - 1;
        memcpy(m->name, name, nl);
        m->name[nl] = '\0';
        if (value) {
            size_t vl = strlen(value);
            if (vl >= sizeof m->value) vl = sizeof m->value - 1;
            memcpy(m->value, value, vl);
            m->value[vl] = '\0';
        } else {
            m->value[0] = '\0';
        }
    }
}

static void parser_undef(GclParser *p, const char *name) {
    for (int i = 0; i < p->macro_count; i++) {
        if (p->macros[i].name[0] && strcmp(p->macros[i].name, name) == 0) {
            for (int j = i; j < p->macro_count - 1; j++) {
                p->macros[j] = p->macros[j + 1];
            }
            p->macro_count--;
            return;
        }
    }
}

static int parser_is_defined(GclParser *p, const char *name) {
    return parser_find_macro(p, name) != NULL;
}

/* ── typedef struct/enum parsing ─────────────────── */

static GclAstNode *parse_typedef_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'typedef' */
    GclAstNode *node = gcl_ast_new(p->arena, AST_TYPEDEF_DECL, line, col);

    if (parser_check(p, TOK_KW_STRUCT)) {
        /* typedef struct Name { ... } Alias; */
        parser_advance(p); /* skip 'struct' */
        if (parser_check(p, TOK_IDENT)) {
            node->str_value = tok_str(p, &p->current);
            parser_advance(p);
        }
        if (parser_check(p, TOK_LBRACE)) {
            parser_advance(p);
            while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                while (!parser_check(p, TOK_SEMICOLON) && !parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                    parser_advance(p);
                }
                if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
            }
            if (parser_check(p, TOK_RBRACE)) parser_advance(p);
        }
        if (parser_check(p, TOK_IDENT)) {
            node->type_name = tok_str(p, &p->current);
            parser_advance(p);
        }
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
    } else if (parser_check(p, TOK_KW_ENUM)) {
        parser_advance(p);
        if (parser_check(p, TOK_IDENT)) {
            node->str_value = tok_str(p, &p->current);
            parser_advance(p);
        }
        if (parser_check(p, TOK_LBRACE)) {
            int64_t ev = 0;
            parse_enum_body(p, &ev);
        }
        if (parser_check(p, TOK_IDENT)) {
            node->type_name = tok_str(p, &p->current);
            parser_advance(p);
        }
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
    } else {
        char toks[16][64];
        int toks_count = 0;
        while (!parser_check(p, TOK_SEMICOLON) && !parser_check(p, TOK_EOF) && toks_count < 16) {
            size_t l = p->current.length < 63 ? p->current.length : 63;
            memcpy(toks[toks_count], p->current.start, l);
            toks[toks_count][l] = '\0';
            toks_count++;
            parser_advance(p);
        }
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
        if (toks_count >= 2) {
            const char *alias = gcl_intern(p->intern, toks[toks_count-1], strlen(toks[toks_count-1]));
            const char *first = toks[0];
            if (strcmp(first,"==")==0 || strcmp(first,"!=")==0 || strcmp(first,"&&")==0 ||
                strcmp(first,"||")==0 || strcmp(first,"!")==0 || strcmp(first,"&")==0 ||
                strcmp(first,"|")==0 || strcmp(first,"^")==0 || strcmp(first,"~")==0 ||
                strcmp(first,"<<")==0 || strcmp(first,">>")==0) {
                if (p->op_alias_count < GCL_ALIAS_MAX) {
                    snprintf(p->op_alias_name[p->op_alias_count], 128, "%s", alias);
                    /* op buffer is 16 bytes; truncate safely (max 15 chars + NUL)
                     * instead of letting GCC warn about truncation. */
                    {
                        size_t olen = strlen(first);
                        if (olen >= sizeof p->op_alias_op[0])
                            olen = sizeof p->op_alias_op[0] - 1;
                        memcpy(p->op_alias_op[p->op_alias_count], first, olen);
                        p->op_alias_op[p->op_alias_count][olen] = '\0';
                    }
                    p->op_alias_count++;
                }
            } else {
                if (p->type_alias_count < GCL_ALIAS_MAX) {
                    char tbuf[256];
                    size_t tn = 0;
                    int ptr = 0;
                    for (int i = 0; i < toks_count - 1; i++) {
                        if (strcmp(toks[i], "*") == 0) { ptr = 1; continue; }
                        if (tn > 0 && tn + 1 < sizeof(tbuf)) tbuf[tn++] = ' ';
                        size_t tl = strlen(toks[i]);
                        if (tn + tl < sizeof(tbuf)) {
                            memcpy(tbuf + tn, toks[i], tl);
                            tn += tl;
                        }
                    }
                    tbuf[tn] = '\0';
                    snprintf(p->type_aliases[p->type_alias_count].name, 128, "%s", alias);
                    /* type buffer is 128 bytes; truncate safely instead of
                     * letting GCC warn about truncation. */
                    {
                        const char *tsrc = tn ? tbuf : "int";
                        size_t tlen = strlen(tsrc);
                        if (tlen >= sizeof p->type_aliases[0].type)
                            tlen = sizeof p->type_aliases[0].type - 1;
                        memcpy(p->type_aliases[p->type_alias_count].type, tsrc, tlen);
                        p->type_aliases[p->type_alias_count].type[tlen] = '\0';
                    }
                    p->type_aliases[p->type_alias_count].ptr = ptr;
                    p->type_alias_count++;
                }
            }
        }
    }
    return node;
}

static GclAstNode *parse_struct_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'struct' */
    GclAstNode *node = gcl_ast_new(p->arena, AST_STRUCT_DECL, line, col);
    if (parser_check(p, TOK_IDENT)) {
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    if (parser_check(p, TOK_LBRACE)) {
        parser_advance(p);
        while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
            while (!parser_check(p, TOK_SEMICOLON) && !parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                parser_advance(p);
            }
            if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
        }
        if (parser_check(p, TOK_RBRACE)) parser_advance(p);
    }
    if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
    return node;
}

/* ── preprocessor parsing ────────────────────────── */

static GclAstNode *parse_pp_path(GclParser *p) {
    GclAstNode *node = NULL;
    if (parser_check(p, TOK_STRING_LIT)) {
        node = gcl_ast_new(p->arena, AST_STRING_LIT, p->current.line, p->current.col);
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    } else if (parser_check(p, TOK_LT)) {
        parser_advance(p);
        const char *start = p->current.start;
        int line = p->current.line, col = p->current.col;
        while (!parser_check(p, TOK_GT) && !parser_check(p, TOK_EOF)) {
            parser_advance(p);
        }
        size_t len = (size_t)(p->current.start - start);
        node = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
        node->str_value = gcl_intern(p->intern, start, len);
        if (parser_check(p, TOK_GT)) parser_advance(p);
    }
    return node;
}

static GclAstNode *parse_pp_include(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_INCLUDE, line, col);
    GclAstNode *path = parse_pp_path(p);
    if (path) gcl_ast_add_child(node, path);
    return node;
}

static GclAstNode *parse_pp_extern(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_EXTERN, line, col);
    GclAstNode *path = parse_pp_path(p);
    if (path) gcl_ast_add_child(node, path);
    while (parser_check(p, TOK_PP_REGISTER)) {
        int rline = p->current.line, rcol = p->current.col;
        parser_advance(p);
        GclAstNode *reg = gcl_ast_new(p->arena, AST_PP_REGISTER, rline, rcol);
        const char *start = p->current.start;
        while (!parser_check(p, TOK_SEMICOLON) && !parser_check(p, TOK_EOF)) {
            parser_advance(p);
        }
        size_t len = (size_t)(p->current.start - start);
        reg->str_value = gcl_intern(p->intern, start, len);
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
        gcl_ast_add_child(node, reg);
    }
    return node;
}

static void skip_pp_block(GclParser *p) {
    int depth = 0;
    while (!parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_PP_IF) || parser_check(p, TOK_PP_IFDEF) || parser_check(p, TOK_PP_IFNDEF)) {
            depth++;
            parser_advance(p);
        } else if (parser_check(p, TOK_PP_ENDIF)) {
            if (depth == 0) break;
            depth--;
            parser_advance(p);
        } else if ((parser_check(p, TOK_PP_ELSE) || parser_check(p, TOK_PP_ELIF_PP)) && depth == 0) {
            break;
        } else {
            parser_advance(p);
        }
    }
}

static GclAstNode *parse_pp_if(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    GclTokenKind pp_kind = p->current.kind;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_IF, line, col);

    int condition = 0;
    if (pp_kind == TOK_PP_IFDEF || pp_kind == TOK_PP_IFNDEF) {
        char macro_name[128] = {0};
        if (parser_check(p, TOK_IDENT)) {
            size_t nl = p->current.length;
            if (nl >= sizeof macro_name) nl = sizeof macro_name - 1;
            memcpy(macro_name, p->current.start, nl);
            macro_name[nl] = '\0';
            parser_advance(p);
        }
        int defined = (macro_name[0] != '\0') ? parser_is_defined(p, macro_name) : 0;
        condition = (pp_kind == TOK_PP_IFDEF) ? defined : !defined;
    } else if (parser_check(p, TOK_IDENT) && p->current.length == 5 &&
        memcmp(p->current.start, "exist", 5) == 0) {
        parser_advance(p);
        char filename[512] = {0};
        if (parser_check(p, TOK_STRING_LIT)) {
            if (p->current.length >= 2) {
                size_t len = p->current.length - 2;
                if (len >= sizeof(filename)) len = sizeof(filename) - 1;
                memcpy(filename, p->current.start + 1, len);
                filename[len] = '\0';
            }
            parser_advance(p);
        } else if (parser_check(p, TOK_LT)) {
            parser_advance(p);
            const char *start = p->current.start;
            while (!parser_check(p, TOK_GT) && !parser_check(p, TOK_EOF)) {
                parser_advance(p);
            }
            size_t len = (size_t)(p->current.start - start);
            if (len >= sizeof(filename)) len = sizeof(filename) - 1;
            memcpy(filename, start, len);
            filename[len] = '\0';
            if (parser_check(p, TOK_GT)) parser_advance(p);
        }
        condition = file_exists_relative(p->filepath, filename);
    } else if (parser_check(p, TOK_IDENT)) {
        /* Part 1: platform kosullari — gcl_doc.md:
         *   #if gnuLinux / gnu_linux / gnu / linux / windows
         * "windows" -> 1, digerleri (Linux tabanli) -> 0.
         * Bilinmeyen ad -> "exist" olmadiysa ve isimli makro uyumu yoksa 0. */
        const char *plat = tok_str(p, &p->current);
        condition = strcmp(plat, "windows") == 0;
        parser_advance(p);
    } else if (parser_check(p, TOK_STRING_LIT) || parser_check(p, TOK_CHAR_LIT)) {
        /* #if "text" — dolu string dogru, bos yanlis */
        GclToken t = p->current;
        condition = t.length > 2;
        parser_advance(p);
    } else {
        /* #if (makro) veya #if 1 / #if 0 veya opsiyonel parantez */
        GclMacro *m = NULL;
        int cond_set = 0;
        if (parser_check(p, TOK_LPAREN)) parser_advance(p);
        if (parser_check(p, TOK_IDENT)) {
            const char *name = tok_str(p, &p->current);
            m = parser_find_macro(p, name);
            if (m) { condition = m->value[0] != '\0'; cond_set = 1; }
            parser_advance(p);
        } else if (parser_check(p, TOK_INT_LIT)) {
            /* sayi: 0 yanlis, sifir olmayan dogru */
            char buf[64];
            size_t l = p->current.length < 63 ? p->current.length : 63;
            memcpy(buf, p->current.start, l); buf[l] = '\0';
            int64_t v = atoll(buf);
            condition = v != 0;
            cond_set = 1;
            parser_advance(p);
        }
        if (parser_check(p, TOK_RPAREN)) parser_advance(p);
        while (!parser_check(p, TOK_EOF) && !parser_check(p, TOK_PP_ELSE) &&
               !parser_check(p, TOK_PP_ENDIF) && !parser_check(p, TOK_PP_ELIF_PP) &&
               !parser_check(p, TOK_PP_EXTERN) && !parser_check(p, TOK_PP_INCLUDE) &&
               !parser_check(p, TOK_PP_ERROR) &&
               !parser_check(p, TOK_PP_WARNING) && !parser_check(p, TOK_PP_REGISTER)) {
            parser_advance(p);
        }
        if (!cond_set) condition = 1;
    }

    (void)pp_kind;

    if (condition) {
        while (!parser_check(p, TOK_EOF) && !parser_check(p, TOK_PP_ELSE) &&
               !parser_check(p, TOK_PP_ENDIF) && !parser_check(p, TOK_PP_ELIF_PP)) {
            GclAstNode *child = parse_stmt(p);
            if (child) gcl_ast_add_child(node, child);
        }
        if (parser_check(p, TOK_PP_ELSE)) {
            parser_advance(p);
            skip_pp_block(p);
        }
    } else {
        skip_pp_block(p);
        if (parser_check(p, TOK_PP_ELSE)) {
            parser_advance(p);
            while (!parser_check(p, TOK_EOF) && !parser_check(p, TOK_PP_ENDIF)) {
                GclAstNode *child = parse_stmt(p);
                if (child) gcl_ast_add_child(node, child);
            }
        }
    }
    if (parser_check(p, TOK_PP_ENDIF)) parser_advance(p);
    return node;
}

static GclAstNode *parse_pp_error(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_ERROR, line, col);
    if (parser_check(p, TOK_STRING_LIT)) {
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    return node;
}

static GclAstNode *parse_pp_warning(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_WARNING, line, col);
    if (parser_check(p, TOK_STRING_LIT)) {
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    return node;
}

/* Part 1: #debug "message" → AST_PP_DEBUG (blue console output) */
static GclAstNode *parse_pp_debug(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_PP_DEBUG, line, col);
    if (parser_check(p, TOK_STRING_LIT)) {
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    return node;
}

/* ── expression parsing (precedence climbing) ────── */

static GclAstNode *parse_primary_expr(GclParser *p) {
    GclToken t = p->current;
    int line = t.line, col = t.col;

    switch (t.kind) {
    case TOK_INT_LIT: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_INT_LIT, line, col);
        char buf[64];
        size_t l = t.length < 63 ? t.length : 63;
        memcpy(buf, t.start, l); buf[l] = '\0';
        n->int_value = atoll(buf);
        n->str_value = tok_str(p, &t);
        parser_advance(p);
        return n;
    }
    case TOK_FLOAT_LIT: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_FLOAT_LIT, line, col);
        char buf[64];
        size_t l = t.length < 63 ? t.length : 63;
        memcpy(buf, t.start, l); buf[l] = '\0';
        n->float_value = atof(buf);
        n->str_value = tok_str(p, &t);
        parser_advance(p);
        return n;
    }
    case TOK_STRING_LIT: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
        n->str_value = tok_str(p, &t);
        parser_advance(p);
        return n;
    }
    case TOK_CHAR_LIT: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_CHAR_LIT, line, col);
        n->str_value = tok_str(p, &t);
        parser_advance(p);
        return n;
    }
    case TOK_KW_NULL: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_NULL_LIT, line, col);
        n->str_value = "null";
        parser_advance(p);
        return n;
    }
    case TOK_KW_BOOL: {
        GclAstNode *n = gcl_ast_new(p->arena, AST_BOOL_LIT, line, col);
        n->str_value = tok_str(p, &t);
        n->int_value = (t.length == 4 && memcmp(t.start, "true", 4) == 0) ? 1 : 0;
        parser_advance(p);
        return n;
    }
    case TOK_IDENT: {
        const char *id = tok_str(p, &t);
        for (int ei = 0; ei < p->enum_count; ei++) {
            if (strcmp(p->enum_names[ei], id) == 0) {
                GclAstNode *n = gcl_ast_new(p->arena, AST_INT_LIT, line, col);
                n->int_value = p->enum_values[ei];
                n->str_value = gcl_intern(p->intern, p->enum_names[ei], strlen(p->enum_names[ei]));
                parser_advance(p);
                return n;
            }
        }
        GclMacro *macro = parser_find_macro(p, id);
        if (macro && macro->value[0]) {
            fprintf(stderr, "[parser_debug] Found macro '%s' with value '%s'\n", id, macro->value);
            const char *mv = macro->value;
            char *endp = NULL;
            long long v = strtoll(mv, &endp, 10);
            fprintf(stderr, "[parser_debug] Parsed as: %lld (endp='%s')\n", v, endp ? endp : "NULL");
            if (endp != mv && *endp == '\0') {
                GclAstNode *n = gcl_ast_new(p->arena, AST_INT_LIT, line, col);
                n->int_value = v;
                n->str_value = gcl_intern(p->intern, mv, strlen(mv));
                fprintf(stderr, "[parser_debug] Created INT_LIT with value %lld\n", v);
                parser_advance(p);
                return n;
            }
            if (mv[0] == '"') {
                GclAstNode *n = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
                n->str_value = gcl_intern(p->intern, mv, strlen(mv));
                parser_advance(p);
                return n;
            }
        } else {
            fprintf(stderr, "[parser_debug] Identifier '%s' - macro=%p\n", id, macro);
        }
        GclAstNode *n = gcl_ast_new(p->arena, AST_IDENT_EXPR, line, col);
        n->str_value = id;
        parser_advance(p);
        return n;
    }
    case TOK_LPAREN: {
        parser_advance(p);
        GclAstNode *n = parse_expr(p);
        parser_expect(p, TOK_RPAREN);
        return n;
    }
    default:
        break;
    }

    if (t.kind == TOK_KW_PRINTF || t.kind == TOK_KW_SCANF ||
        t.kind == TOK_KW_MALLOC || t.kind == TOK_KW_FREE ||
        t.kind == TOK_KW_GCMALLOC || t.kind == TOK_KW_SIZEOF) {
        GclAstNode *n = gcl_ast_new(p->arena, AST_IDENT_EXPR, line, col);
        n->str_value = tok_str(p, &t);
        parser_advance(p);
        return n;
    }

    gcl_diag_add(p->diag, GCL_DIAG_ERROR, line, col, p->filepath,
                 "unexpected token '%s' in expression", gcl_token_kind_name(t.kind));
    parser_advance(p);
    return gcl_ast_new(p->arena, AST_NULL_LIT, line, col);
}

static GclAstNode *parse_postfix_expr(GclParser *p) {
    GclAstNode *left = parse_primary_expr(p);
    for (;;) {
        if (parser_check(p, TOK_LPAREN)) {
            int line = p->current.line, col = p->current.col;
            parser_advance(p);
            GclAstNode *call = gcl_ast_new(p->arena, AST_CALL_EXPR, line, col);
            gcl_ast_add_child(call, left);
            while (!parser_check(p, TOK_RPAREN) && !parser_check(p, TOK_EOF)) {
                GclAstNode *arg = parse_assign_expr(p);
                gcl_ast_add_child(call, arg);
                if (!parser_check(p, TOK_RPAREN)) parser_expect(p, TOK_COMMA);
            }
            parser_expect(p, TOK_RPAREN);
            left = call;
        } else if (parser_check(p, TOK_DOT) || parser_check(p, TOK_ARROW)) {
            int line = p->current.line, col = p->current.col;
            GclAstNode *member = gcl_ast_new(p->arena, AST_MEMBER_EXPR, line, col);
            member->str_value = tok_str(p, &p->current);
            parser_advance(p);
            gcl_ast_add_child(member, left);
            GclAstNode *field = gcl_ast_new(p->arena, AST_IDENT_EXPR, p->current.line, p->current.col);
            field->str_value = tok_str(p, &p->current);
            parser_advance(p);
            gcl_ast_add_child(member, field);
            left = member;
        } else if (parser_check(p, TOK_LBRACKET)) {
            int line = p->current.line, col = p->current.col;
            parser_advance(p);
            GclAstNode *idx = gcl_ast_new(p->arena, AST_INDEX_EXPR, line, col);
            gcl_ast_add_child(idx, left);
            gcl_ast_add_child(idx, parse_expr(p));
            parser_expect(p, TOK_RBRACKET);
            left = idx;
        } else if (parser_check(p, TOK_PLUSPLUS) || parser_check(p, TOK_MINUSMINUS)) {
            int line = p->current.line, col = p->current.col;
            GclAstNode *un = gcl_ast_new(p->arena, AST_UNARY_EXPR, line, col);
            un->str_value = tok_str(p, &p->current);
            gcl_ast_add_child(un, left);
            parser_advance(p);
            left = un;
        } else {
            break;
        }
    }
    return left;
}

static GclAstNode *parse_unary_expr(GclParser *p) {
    const char *uop = NULL;
    if (parser_check(p, TOK_BANG) || parser_check(p, TOK_MINUS) ||
        parser_check(p, TOK_TILDE) || parser_check(p, TOK_AMP) ||
        parser_check(p, TOK_STAR) || parser_check(p, TOK_PLUSPLUS) ||
        parser_check(p, TOK_MINUSMINUS)) {
        uop = tok_str(p, &p->current);
    } else if (p->current.kind == TOK_IDENT) {
        const char *name = tok_str(p, &p->current);
        for (int i = 0; i < p->op_alias_count; i++) {
            if (strcmp(p->op_alias_name[i], name) == 0 && strcmp(p->op_alias_op[i], "!") == 0) {
                uop = p->op_alias_op[i];
                break;
            }
        }
    }
    if (uop) {
        int line = p->current.line, col = p->current.col;
        GclAstNode *un = gcl_ast_new(p->arena, AST_UNARY_EXPR, line, col);
        un->str_value = uop;
        parser_advance(p);
        gcl_ast_add_child(un, parse_unary_expr(p));
        return un;
    }
    return parse_postfix_expr(p);
}

static GclAstNode *parse_multiplicative_expr(GclParser *p) {
    GclAstNode *left = parse_unary_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"*")!=0 && strcmp(op,"/")!=0 && strcmp(op,"%")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_unary_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_additive_expr(GclParser *p) {
    GclAstNode *left = parse_multiplicative_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"+")!=0 && strcmp(op,"-")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_multiplicative_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_shift_expr(GclParser *p) {
    GclAstNode *left = parse_additive_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"<<")!=0 && strcmp(op,">>")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_additive_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_relational_expr(GclParser *p) {
    GclAstNode *left = parse_shift_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"<")!=0 && strcmp(op,">")!=0 &&
            strcmp(op,"<=")!=0 && strcmp(op,">=")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_shift_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_equality_expr(GclParser *p) {
    GclAstNode *left = parse_relational_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"==")!=0 && strcmp(op,"!=")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_relational_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_bit_and_expr(GclParser *p) {
    GclAstNode *left = parse_equality_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"&")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_equality_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_bit_xor_expr(GclParser *p) {
    GclAstNode *left = parse_bit_and_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"^")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_bit_and_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_bit_or_expr(GclParser *p) {
    GclAstNode *left = parse_bit_xor_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"|")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_bit_xor_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_and_expr(GclParser *p) {
    GclAstNode *left = parse_bit_or_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"&&")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_bit_or_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_or_expr(GclParser *p) {
    GclAstNode *left = parse_and_expr(p);
    for (;;) {
        const char *op = parser_cur_binop(p);
        if (!op) break;
        if (strcmp(op,"||")!=0) break;
        int line = p->current.line, col = p->current.col;
        parser_advance(p);
        GclAstNode *bin = gcl_ast_new(p->arena, AST_BINARY_EXPR, line, col);
        bin->str_value = op;
        gcl_ast_add_child(bin, left);
        gcl_ast_add_child(bin, parse_and_expr(p));
        left = bin;
    }
    return left;
}

static GclAstNode *parse_assign_expr(GclParser *p) {
    GclAstNode *left = parse_or_expr(p);
    if (parser_check(p, TOK_EQ) || parser_check(p, TOK_PLUS_EQ) ||
        parser_check(p, TOK_MINUS_EQ) || parser_check(p, TOK_STAR_EQ) ||
        parser_check(p, TOK_SLASH_EQ)) {
        int line = p->current.line, col = p->current.col;
        const char *op = tok_str(p, &p->current);
        parser_advance(p);
        GclAstNode *assign = gcl_ast_new(p->arena, AST_ASSIGN_EXPR, line, col);
        assign->str_value = op;
        gcl_ast_add_child(assign, left);
        gcl_ast_add_child(assign, parse_assign_expr(p));
        return assign;
    }
    return left;
}

static GclAstNode *parse_expr(GclParser *p) {
    return parse_assign_expr(p);
}

/* ── statement parsing ───────────────────────────── */

static GclAstNode *parse_block(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_expect(p, TOK_LBRACE);
    GclAstNode *block = gcl_ast_new(p->arena, AST_BLOCK, line, col);
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        GclAstNode *s = parse_stmt(p);
        if (s) gcl_ast_add_child(block, s);
    }
    parser_expect(p, TOK_RBRACE);
    return block;
}

/* Part 10: switch (expr) { case label: stmts... default: stmts... }
 * Children: [0] = switch expr, [1..] = case clauses (AST_CASE_CLAUSE).
 * A default clause has str_value "default". */
static GclAstNode *parse_switch_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'switch' */
    GclAstNode *node = gcl_ast_new(p->arena, AST_SWITCH_STMT, line, col);
    parser_expect(p, TOK_LPAREN);
    gcl_ast_add_child(node, parse_expr(p));
    parser_expect(p, TOK_RPAREN);
    parser_expect(p, TOK_LBRACE);
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_KW_CASE)) {
            int cl = p->current.line, cc = p->current.col;
            parser_advance(p);
            GclAstNode *clause = gcl_ast_new(p->arena, AST_CASE_CLAUSE, cl, cc);
            gcl_ast_add_child(clause, parse_expr(p));
            parser_expect(p, TOK_COLON);
            while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF) &&
                   !parser_check(p, TOK_KW_CASE) && !parser_check(p, TOK_KW_DEFAULT)) {
                GclAstNode *s = parse_stmt(p);
                if (s) gcl_ast_add_child(clause, s);
            }
            gcl_ast_add_child(node, clause);
        } else if (parser_check(p, TOK_KW_DEFAULT)) {
            int dl = p->current.line, dc = p->current.col;
            parser_advance(p);
            GclAstNode *clause = gcl_ast_new(p->arena, AST_CASE_CLAUSE, dl, dc);
            clause->str_value = "default";
            parser_expect(p, TOK_COLON);
            while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF) &&
                   !parser_check(p, TOK_KW_CASE) && !parser_check(p, TOK_KW_DEFAULT)) {
                GclAstNode *s = parse_stmt(p);
                if (s) gcl_ast_add_child(clause, s);
            }
            gcl_ast_add_child(node, clause);
        } else {
            parser_advance(p);
        }
    }
    parser_expect(p, TOK_RBRACE);
    return node;
}

/* Part 12: do { ... } while (cond);
 * Reuses AST_WHILE_STMT with str_value "do" — the IR generator emits
 * body-first layout when str_value == "do". */
static GclAstNode *parse_do_while_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'do' */
    GclAstNode *node = gcl_ast_new(p->arena, AST_WHILE_STMT, line, col);
    node->str_value = "do";
    gcl_ast_add_child(node, parse_block(p));   /* children[0] = body */
    /* while (cond) */
    parser_expect(p, TOK_KW_WHILE);
    parser_expect(p, TOK_LPAREN);
    gcl_ast_add_child(node, parse_expr(p));    /* children[1] = condition */
    parser_expect(p, TOK_RPAREN);
    parser_expect(p, TOK_SEMICOLON);
    return node;
}

/* Part 15: class NAME() | class NAME(PARENT) { members }
 * Members: methods (void call() { ... }) and fields (type var;).
 * Methods are registered as global functions "<Class>_<Method>" so the IR
 * can emit labels for them; `@return`-prefixed lines are skipped. */
static GclAstNode *parse_class_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'class' */
    GclAstNode *node = gcl_ast_new(p->arena, AST_CLASS_DECL, line, col);
    if (parser_check(p, TOK_IDENT)) {
        node->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    const char *class_name = node->str_value ? node->str_value : "";

    /* optional ( PARENT ) constructor signature */
    if (parser_check(p, TOK_LPAREN)) {
        parser_advance(p);
        if (parser_check(p, TOK_IDENT)) {
            node->type_name = tok_str(p, &p->current); /* parent */
            parser_advance(p);
        }
        parser_expect(p, TOK_RPAREN);
    }

    parser_expect(p, TOK_LBRACE);
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        /* skip @return lines (GCL class return annotation) — line based,
         * so the following member declaration is not swallowed */
        if (parser_check(p, TOK_AT)) {
            int at_line = p->current.line;
            while (p->current.line == at_line && !parser_check(p, TOK_EOF) &&
                   !parser_check(p, TOK_RBRACE)) {
                parser_advance(p);
            }
            continue;
        }
        GclAstNode *member = parse_stmt(p);
        if (!member) {
            if (!parser_check(p, TOK_RBRACE)) parser_advance(p);
            continue;
        }
        if (member->kind == AST_FUNC_DECL && member->str_value) {
            /* register as global function "<Class>_<Method>" */
            char buf[256];
            snprintf(buf, sizeof(buf), "%s_%s", class_name, member->str_value);
            member->str_value = gcl_intern(p->intern, buf, strlen(buf));
        }
        gcl_ast_add_child(node, member);
    }
    parser_expect(p, TOK_RBRACE);
    return node;
}

/* Part 9: tuple initializer: ('a', "half life 3", 10, 300.2000, 33)
 * Encoded as "T;elem;elem;..." (raw token texts, ;-separated). */
static GclAstNode *parse_tuple_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'tuple' */
    GclAstNode *decl = gcl_ast_new(p->arena, AST_VAR_DECL, line, col);
    decl->type_name = "tuple";
    if (parser_check(p, TOK_IDENT)) {
        decl->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    parser_expect(p, TOK_EQ);
    char buf[1024];
    size_t n = 0;
    if (n + 2 < sizeof(buf)) { buf[n++] = 'T'; buf[n++] = ';'; }
    parser_expect(p, TOK_LPAREN);
    while (!parser_check(p, TOK_RPAREN) && !parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_COMMA)) { parser_advance(p); continue; }
        GclToken t = p->current;
        size_t l = t.length < 200 ? t.length : 200;
        if (n + l + 1 < sizeof(buf)) {
            memcpy(buf + n, t.start, l);
            n += l;
            buf[n++] = ';';
        }
        parser_advance(p);
    }
    parser_expect(p, TOK_RPAREN);
    parser_expect(p, TOK_SEMICOLON);
    GclAstNode *str = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
    str->str_value = gcl_intern(p->intern, buf, n);
    gcl_ast_add_child(decl, str);
    return decl;
}

/* Part 9: dict initializer: { gcChar name : "gnuchanos", int8 scale: 30 }
 * Encoded as "D;name=value;..." — same member format as struct "S;...". */
static GclAstNode *parse_dict_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip 'dict' */
    GclAstNode *decl = gcl_ast_new(p->arena, AST_VAR_DECL, line, col);
    decl->type_name = "dict";
    if (parser_check(p, TOK_IDENT)) {
        decl->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }
    parser_expect(p, TOK_EQ);
    char buf[1024];
    size_t n = 0;
    int first = 1;
    parser_expect(p, TOK_LBRACE);
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_COMMA)) { parser_advance(p); continue; }
        /* skip field type tokens until the field name (IDENT followed by ':') */
        for (;;) {
            if (parser_check(p, TOK_RBRACE) || parser_check(p, TOK_COMMA) || parser_check(p, TOK_EOF)) break;
            if (parser_check(p, TOK_IDENT)) {
                GclToken nxt = gcl_lexer_peek(&p->lex);
                if (nxt.kind == TOK_COLON) break; /* this IDENT is the field name */
            }
            parser_advance(p);
        }
        if (!parser_check(p, TOK_IDENT)) break;
        {
            const char *fname = tok_str(p, &p->current);
            parser_advance(p);
            /* optional array brackets */
            if (parser_check(p, TOK_LBRACKET)) {
                parser_advance(p);
                if (parser_check(p, TOK_RBRACKET)) parser_advance(p);
            }
            if (first) {
                if (n + 2 < sizeof(buf)) { buf[n++] = 'D'; buf[n++] = ';'; }
                first = 0;
            }
            size_t fl = strlen(fname);
            if (n + fl + 2 < sizeof(buf)) {
                memcpy(buf + n, fname, fl);
                n += fl;
                buf[n++] = '=';
            }
            if (parser_check(p, TOK_COLON)) parser_advance(p);
            if (parser_check(p, TOK_LBRACE)) {
                /* nested { ... } value — copy contents verbatim */
                char sb[600];
                size_t sn = 0;
                parser_advance(p);
                if (sn + 1 < sizeof(sb)) sb[sn++] = '{';
                while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
                    if (parser_check(p, TOK_COMMA)) { parser_advance(p); continue; }
                    GclToken t = p->current;
                    size_t l = t.length < 200 ? t.length : 200;
                    if (sn + l + 1 < sizeof(sb)) {
                        memcpy(sb + sn, t.start, l);
                        sn += l;
                        if (sn + 1 < sizeof(sb)) sb[sn++] = ',';
                    }
                    parser_advance(p);
                }
                if (parser_check(p, TOK_RBRACE)) parser_advance(p);
                if (sn > 0 && sb[sn-1] == ',') sn--;
                if (sn + 1 < sizeof(sb)) sb[sn++] = '}';
                sb[sn] = '\0';
                if (n + sn + 1 < sizeof(buf)) {
                    memcpy(buf + n, sb, sn);
                    n += sn;
                    buf[n++] = ';';
                }
            } else if (parser_check(p, TOK_STRING_LIT) || parser_check(p, TOK_CHAR_LIT) ||
                       parser_check(p, TOK_INT_LIT) || parser_check(p, TOK_FLOAT_LIT)) {
                GclToken t = p->current;
                size_t l = t.length < 200 ? t.length : 200;
                if (n + l + 1 < sizeof(buf)) {
                    memcpy(buf + n, t.start, l);
                    n += l;
                    buf[n++] = ';';
                }
                parser_advance(p);
            } else {
                if (n + 1 < sizeof(buf)) buf[n++] = ';';
                parser_advance(p);
            }
        }
    }
    parser_expect(p, TOK_RBRACE);
    parser_expect(p, TOK_SEMICOLON);
    GclAstNode *str = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
    str->str_value = gcl_intern(p->intern, buf, n);
    gcl_ast_add_child(decl, str);
    return decl;
}

/* Array/struct initializer:
 * { 'a','b','c' }      → "abc"        (char array, quoted)
 * { 1,2,3 }            → "I;1;2;3"    (int array, quoted)
 * { .f = v, ... }      → S;f=v;...    (struct, raw, no quotes)
 * Mixed → null. */
static GclAstNode *parse_brace_initializer(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p); /* skip '{' */
    char buf[1024];
    size_t n = 0;
    int has_int = 0;
    int has_char = 0;
    int has_struct = 0;
    while (!parser_check(p, TOK_EOF) && !parser_check(p, TOK_RBRACE)) {
        if (parser_check(p, TOK_DOT)) {
            /* designated initializer: .name = value */
            parser_advance(p);
            const char *fname = NULL;
            if (parser_check(p, TOK_IDENT)) {
                fname = tok_str(p, &p->current);
                parser_advance(p);
            }
            if (parser_check(p, TOK_EQ) && fname) {
                parser_advance(p);
                if (!has_struct) {
                    if (n + 2 < sizeof(buf)) { buf[n++] = 'S'; buf[n++] = ';'; }
                    has_struct = 1;
                }
                size_t fl = strlen(fname);
                if (n + fl + 2 < sizeof(buf)) {
                    memcpy(buf + n, fname, fl);
                    n += fl;
                    buf[n++] = '=';
                }
                if (parser_check(p, TOK_STRING_LIT) || parser_check(p, TOK_CHAR_LIT) ||
                    parser_check(p, TOK_INT_LIT) || parser_check(p, TOK_FLOAT_LIT)) {
                    GclToken t = p->current;
                    size_t l = t.length < 200 ? t.length : 200;
                    if (n + l + 1 < sizeof(buf)) {
                        memcpy(buf + n, t.start, l);
                        n += l;
                        buf[n++] = ';';
                    }
                    parser_advance(p);
                } else {
                    if (n + 1 < sizeof(buf)) buf[n++] = ';';
                    parser_advance(p);
                }
            }
        } else if (parser_check(p, TOK_CHAR_LIT)) {
            GclToken t = p->current;
            if (t.length >= 3 && n + 1 < sizeof(buf)) buf[n++] = t.start[1];
            has_char = 1;
            parser_advance(p);
        } else if (parser_check(p, TOK_INT_LIT)) {
            GclToken t = p->current;
            size_t l = t.length < 64 ? t.length : 64;
            if (n + l + 1 < sizeof(buf)) {
                buf[n++] = ';';
                memcpy(buf + n, t.start, l);
                n += l;
            }
            has_int = 1;
            parser_advance(p);
        } else if (parser_check(p, TOK_COMMA)) {
            parser_advance(p);
        } else {
            parser_advance(p);
        }
    }
    if (parser_check(p, TOK_RBRACE)) parser_advance(p);

    if (has_struct) {
        /* struct: raw "S;name=value;..." — no surrounding quotes */
        GclAstNode *node = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
        node->str_value = gcl_intern(p->intern, buf, n);
        return node;
    }
    if (has_int && has_char) return gcl_ast_new(p->arena, AST_NULL_LIT, line, col);
    if (has_int) {
        /* int array: "I;1;2;3" — I marker + ;-separated values */
        char out[1150];
        size_t m = 0;
        out[m++] = '"';
        out[m++] = 'I';
        for (size_t i = 0; i < n && m + 1 < sizeof(out); i++) out[m++] = buf[i];
        out[m++] = '"';
        out[m] = '\0';
        GclAstNode *node = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
        node->str_value = gcl_intern(p->intern, out, m);
        return node;
    }
    if (has_char) {
        /* char array: "abc" */
        char out[600];
        size_t m = 0;
        out[m++] = '"';
        for (size_t i = 0; i < n && m + 1 < sizeof(out); i++) out[m++] = buf[i];
        out[m++] = '"';
        out[m] = '\0';
        GclAstNode *node = gcl_ast_new(p->arena, AST_STRING_LIT, line, col);
        node->str_value = gcl_intern(p->intern, out, m);
        return node;
    }
    return gcl_ast_new(p->arena, AST_NULL_LIT, line, col);
}

/* Parse one variable declaration name. */
static GclAstNode *parse_var_decl_unit(GclParser *p, const char *type_name, int ptr) {
    int line = p->current.line, col = p->current.col;
    GclAstNode *decl = gcl_ast_new(p->arena, AST_VAR_DECL, line, col);
    decl->type_name = type_name;
    decl->is_pointer = ptr;

    if (parser_check(p, TOK_IDENT) || parser_check(p, TOK_KW_PRINTF) || parser_check(p, TOK_KW_SCANF)) {
        decl->str_value = tok_str(p, &p->current);
        parser_advance(p);
    }

    /* Part 14: multi-dimensional array: int grid[2][3]
     * array_dim = first dimension (rows), int_value = second (cols). */
    while (parser_check(p, TOK_LBRACKET)) {
        parser_advance(p);
        int dim = 1;
        if (!parser_check(p, TOK_RBRACKET)) {
            if (parser_check(p, TOK_INT_LIT)) {
                char dimbuf[64];
                size_t dl = p->current.length < 63 ? p->current.length : 63;
                memcpy(dimbuf, p->current.start, dl);
                dimbuf[dl] = '\0';
                dim = (int)atol(dimbuf);
                parser_advance(p);
            } else {
                GclAstNode *size = parse_expr(p);
                gcl_ast_add_child(decl, size);
                parser_expect(p, TOK_RBRACKET);
                break;
            }
        }
        parser_expect(p, TOK_RBRACKET);
        if (decl->array_dim == 0) {
            decl->array_dim = dim;
        } else {
            decl->int_value = dim;
        }
    }

    /* GCL dimension suffix: name[] @N = {...} */
    if (parser_check(p, TOK_AT)) {
        parser_advance(p);
        if (parser_check(p, TOK_INT_LIT)) {
            char buf[64];
            size_t l = p->current.length < 63 ? p->current.length : 63;
            memcpy(buf, p->current.start, l);
            buf[l] = '\0';
            decl->array_dim = (int)atol(buf);
            parser_advance(p);
        }
    }

    if (parser_match(p, TOK_EQ)) {
        if (parser_check(p, TOK_LBRACE)) {
            gcl_ast_add_child(decl, parse_brace_initializer(p));
        } else {
            GclAstNode *init = parse_expr(p);
            gcl_ast_add_child(decl, init);
        }
    }
    return decl;
}

/* Parses: type a = 1, b = 2, c = 3;  and  type single = value; */
static GclAstNode *parse_var_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    const char *type_name = tok_str(p, &p->current);
    parser_advance(p);

    int ptr0 = 0;
    const GclTypeAlias *tal = parser_find_type_alias(p, type_name);
    if (tal) {
        type_name = tal->type;
        ptr0 = tal->ptr;
    }

    if ((strcmp(type_name, "struct") == 0 || strcmp(type_name, "enum") == 0) &&
        parser_check(p, TOK_IDENT)) {
        char type_buf[256];
        snprintf(type_buf, sizeof type_buf, "%s %s", type_name, tok_str(p, &p->current));
        parser_advance(p);
        type_name = gcl_intern(p->intern, type_buf, strlen(type_buf));
    }

    int ptr = ptr0;
    while (parser_check(p, TOK_STAR)) { ptr++; parser_advance(p); }

    GclAstNode *first = parse_var_decl_unit(p, type_name, ptr);
    if (!parser_check(p, TOK_COMMA)) {
        parser_expect(p, TOK_SEMICOLON);
        return first;
    }

    GclAstNode *block = gcl_ast_new(p->arena, AST_BLOCK, line, col);
    gcl_ast_add_child(block, first);
    while (parser_check(p, TOK_COMMA)) {
        parser_advance(p);
        GclAstNode *next = parse_var_decl_unit(p, type_name, ptr);
        gcl_ast_add_child(block, next);
    }
    parser_expect(p, TOK_SEMICOLON);
    return block;
}

static GclAstNode *parse_func_decl(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    GclAstNode *func = gcl_ast_new(p->arena, AST_FUNC_DECL, line, col);

    func->type_name = tok_str(p, &p->current);
    parser_advance(p);
    const GclTypeAlias *ftal = parser_find_type_alias(p, func->type_name);
    if (ftal) {
        func->type_name = ftal->type;
        func->is_pointer = ftal->ptr;
    }
    while (parser_check(p, TOK_STAR)) { func->is_pointer++; parser_advance(p); }

    func->str_value = tok_str(p, &p->current);
    parser_advance(p);

    parser_expect(p, TOK_LPAREN);
    while (!parser_check(p, TOK_RPAREN) && !parser_check(p, TOK_EOF)) {
        if (is_type_or_alias(p) || parser_check(p, TOK_IDENT)) {
            int pl = p->current.line, pc = p->current.col;
            GclAstNode *param = gcl_ast_new(p->arena, AST_PARAM, pl, pc);
            param->type_name = tok_str(p, &p->current);
            parser_advance(p);
            const GclTypeAlias *ptal = parser_find_type_alias(p, param->type_name);
            if (ptal) {
                param->type_name = ptal->type;
                param->is_pointer = ptal->ptr;
            }
            while (parser_check(p, TOK_STAR)) { param->is_pointer++; parser_advance(p); }
            if (parser_check(p, TOK_IDENT)) {
                param->str_value = tok_str(p, &p->current);
                parser_advance(p);
            }
            if (parser_check(p, TOK_LBRACKET)) {
                parser_advance(p);
                if (!parser_check(p, TOK_RBRACKET)) parser_advance(p);
                if (parser_check(p, TOK_RBRACKET)) parser_advance(p);
                param->array_dim = 1;
            }
            gcl_ast_add_child(func, param);
        }
        if (!parser_check(p, TOK_RPAREN)) parser_expect(p, TOK_COMMA);
    }
    parser_expect(p, TOK_RPAREN);

    if (parser_check(p, TOK_LBRACE)) {
        GclAstNode *body = parse_block(p);
        gcl_ast_add_child(func, body);
    } else {
        parser_expect(p, TOK_SEMICOLON);
    }
    return func;
}

static GclAstNode *parse_if_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_IF_STMT, line, col);
    parser_expect(p, TOK_LPAREN);
    gcl_ast_add_child(node, parse_expr(p));
    parser_expect(p, TOK_RPAREN);
    gcl_ast_add_child(node, parse_block(p));
    if (parser_check(p, TOK_KW_ELSE) || parser_check(p, TOK_KW_ELIF)) {
        if (parser_check(p, TOK_KW_ELIF)) {
            gcl_ast_add_child(node, parse_if_stmt(p));
        } else {
            parser_advance(p);
            if (parser_check(p, TOK_KW_IF)) {
                gcl_ast_add_child(node, parse_if_stmt(p));
            } else {
                gcl_ast_add_child(node, parse_block(p));
            }
        }
    }
    return node;
}

static GclAstNode *parse_while_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_WHILE_STMT, line, col);
    parser_expect(p, TOK_LPAREN);
    gcl_ast_add_child(node, parse_expr(p));
    parser_expect(p, TOK_RPAREN);
    gcl_ast_add_child(node, parse_block(p));
    return node;
}

static GclAstNode *parse_for_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_FOR_STMT, line, col);
    parser_expect(p, TOK_LPAREN);
    if (!parser_check(p, TOK_SEMICOLON)) {
        gcl_ast_add_child(node, parse_stmt(p));
    } else {
        gcl_ast_add_child(node, gcl_ast_new(p->arena, AST_NULL_LIT, line, col));
        parser_advance(p);
    }
    if (!parser_check(p, TOK_SEMICOLON)) {
        gcl_ast_add_child(node, parse_expr(p));
    } else {
        gcl_ast_add_child(node, gcl_ast_new(p->arena, AST_NULL_LIT, line, col));
    }
    parser_expect(p, TOK_SEMICOLON);
    if (!parser_check(p, TOK_RPAREN)) {
        gcl_ast_add_child(node, parse_expr(p));
    } else {
        gcl_ast_add_child(node, gcl_ast_new(p->arena, AST_NULL_LIT, line, col));
    }
    parser_expect(p, TOK_RPAREN);
    gcl_ast_add_child(node, parse_block(p));
    return node;
}

static GclAstNode *parse_return_stmt(GclParser *p) {
    int line = p->current.line, col = p->current.col;
    parser_advance(p);
    GclAstNode *node = gcl_ast_new(p->arena, AST_RETURN_STMT, line, col);
    if (!parser_check(p, TOK_SEMICOLON)) {
        gcl_ast_add_child(node, parse_expr(p));
    }
    parser_expect(p, TOK_SEMICOLON);
    return node;
}

/* ── is this a declaration? ──────────────────────── */

static bool looks_like_decl(GclParser *p) {
    if (!is_type_or_alias(p)) return false;
    GclLexer saved = p->lex;
    GclToken saved_tok = p->current;
    parser_advance(p);

    bool compound = (saved_tok.kind == TOK_KW_STRUCT || saved_tok.kind == TOK_KW_ENUM);
    if (compound && parser_check(p, TOK_IDENT)) {
        parser_advance(p);
        if (parser_check(p, TOK_LBRACE) || parser_check(p, TOK_SEMICOLON)) {
            p->lex = saved;
            p->current = saved_tok;
            return false;
        }
        p->lex = saved;
        p->current = saved_tok;
        return true;
    }

    while (parser_check(p, TOK_STAR)) parser_advance(p);
    bool result = parser_check(p, TOK_IDENT) || parser_check(p, TOK_KW_PRINTF) || parser_check(p, TOK_KW_SCANF);
    p->lex = saved;
    p->current = saved_tok;
    return result;
}

static bool looks_like_func(GclParser *p) {
    if (!is_type_or_alias(p)) return false;
    GclLexer saved = p->lex;
    GclToken saved_tok = p->current;
    parser_advance(p);
    while (parser_check(p, TOK_STAR)) parser_advance(p);
    bool has_name = parser_check(p, TOK_IDENT);
    if (has_name) parser_advance(p);
    bool has_paren = parser_check(p, TOK_LPAREN);
    p->lex = saved;
    p->current = saved_tok;
    return has_name && has_paren;
}

/* ── top-level statement dispatch ────────────────── */

static GclAstNode *parse_stmt(GclParser *p) {
    switch (p->current.kind) {
    case TOK_PP_INCLUDE: return parse_pp_include(p);
    case TOK_PP_EXTERN:  return parse_pp_extern(p);
    case TOK_PP_IF:
    case TOK_PP_IFDEF:
    case TOK_PP_IFNDEF:  return parse_pp_if(p);
    case TOK_PP_ERROR:   return parse_pp_error(p);
    case TOK_PP_WARNING: return parse_pp_warning(p);
    case TOK_PP_DEFINE: {
        parser_advance(p);
        char macro_name[128] = {0};
        if (parser_check(p, TOK_IDENT)) {
            size_t nl = p->current.length;
            if (nl >= sizeof macro_name) nl = sizeof macro_name - 1;
            memcpy(macro_name, p->current.start, nl);
            macro_name[nl] = '\0';
            parser_advance(p);
        }
        char macro_value[256] = {0};
        {
            int def_line = p->current.line;
            int depth = 0;
            size_t vlen = 0;
            while (!parser_check(p, TOK_EOF)) {
                if (parser_check(p, TOK_LBRACE)) { depth++; parser_advance(p); continue; }
                if (parser_check(p, TOK_RBRACE)) {
                    if (depth > 0) { depth--; parser_advance(p); continue; }
                    break;
                }
                if (p->current.kind >= TOK_PP_INCLUDE && p->current.kind <= TOK_PP_DEBUG) break;
                if (depth == 0 && p->current.line > def_line) {
                    if (is_type_token(p->current.kind) || 
                        parser_check(p, TOK_KW_PUBLIC) || parser_check(p, TOK_KW_PRIVATE) ||
                        parser_check(p, TOK_KW_IF) ||
                        parser_check(p, TOK_KW_WHILE) || parser_check(p, TOK_KW_FOR) ||
                        parser_check(p, TOK_KW_RETURN) || parser_check(p, TOK_KW_TYPEDEF) ||
                        parser_check(p, TOK_IDENT))
                        break;
                }
                if (vlen + p->current.length + 1 < sizeof macro_value) {
                    memcpy(macro_value + vlen, p->current.start, p->current.length);
                    vlen += p->current.length;
                    macro_value[vlen++] = ' ';
                    macro_value[vlen] = '\0';
                }
                parser_advance(p);
            }
            if (vlen > 0 && macro_value[vlen-1] == ' ') macro_value[vlen-1] = '\0';
        }
        if (macro_name[0] != '\0') parser_define(p, macro_name, macro_value[0] ? macro_value : NULL);
        return NULL;
    }
    case TOK_PP_UNDEF: {
        parser_advance(p);
        char undef_name[128] = {0};
        if (parser_check(p, TOK_IDENT)) {
            size_t nl = p->current.length;
            if (nl >= sizeof undef_name) nl = sizeof undef_name - 1;
            memcpy(undef_name, p->current.start, nl);
            undef_name[nl] = '\0';
            parser_advance(p);
        }
        if (undef_name[0] != '\0') parser_undef(p, undef_name);
        return NULL;
    }
    case TOK_PP_DEBUG:   return parse_pp_debug(p);
    case TOK_PP_ELSE:
    case TOK_PP_ENDIF:
    case TOK_PP_ELIF_PP:
        parser_advance(p);
        while (!parser_check(p, TOK_EOF) &&
               !(p->current.kind >= TOK_PP_INCLUDE && p->current.kind <= TOK_PP_DEBUG) &&
               !is_type_token(p->current.kind) && !parser_check(p, TOK_IDENT) &&
               !parser_check(p, TOK_LBRACE) && !parser_check(p, TOK_KW_IF) &&
               !parser_check(p, TOK_KW_WHILE) && !parser_check(p, TOK_KW_FOR) &&
               !parser_check(p, TOK_KW_RETURN) && !parser_check(p, TOK_KW_TYPEDEF)) {
            parser_advance(p);
        }
        return NULL;

    case TOK_KW_IF:     return parse_if_stmt(p);
    case TOK_KW_SWITCH: return parse_switch_stmt(p);
    case TOK_KW_WHILE:  return parse_while_stmt(p);
    case TOK_KW_DO:     return parse_do_while_stmt(p);
    case TOK_KW_FOR:    return parse_for_stmt(p);
    case TOK_KW_CLASS:  return parse_class_decl(p);
    case TOK_KW_RETURN: return parse_return_stmt(p);
    case TOK_KW_BREAK: {
        int l = p->current.line, c = p->current.col;
        parser_advance(p); parser_expect(p, TOK_SEMICOLON);
        return gcl_ast_new(p->arena, AST_BREAK_STMT, l, c);
    }
    case TOK_KW_CONTINUE: {
        int l = p->current.line, c = p->current.col;
        parser_advance(p); parser_expect(p, TOK_SEMICOLON);
        return gcl_ast_new(p->arena, AST_CONTINUE_STMT, l, c);
    }
    case TOK_LBRACE: return parse_block(p);

    case TOK_KW_PUBLIC:
    case TOK_KW_PRIVATE:
    case TOK_KW_CONST:
        parser_advance(p);
        return parse_stmt(p);

    case TOK_KW_INLINE:
    case TOK_KW_GLOBAL: {
        parser_advance(p);
        if (parser_check(p, TOK_IDENT) || parser_check(p, TOK_KW_PRINTF) ||
            parser_check(p, TOK_KW_SCANF)) {
            int gl = p->current.line, gc = p->current.col;
            GclAstNode *decl = gcl_ast_new(p->arena, AST_VAR_DECL, gl, gc);
            decl->type_name = "global";
            decl->str_value = tok_str(p, &p->current);
            parser_advance(p);
            parser_expect(p, TOK_SEMICOLON);
            return decl;
        }
        return parse_stmt(p);
    }

    case TOK_KW_TUPLE:
        return parse_tuple_decl(p);

    case TOK_KW_DICT:
        return parse_dict_decl(p);

    case TOK_KW_TYPEDEF:
        return parse_typedef_decl(p);

    case TOK_KW_STRUCT:
        if (!looks_like_func(p) && !looks_like_decl(p)) {
            return parse_struct_decl(p);
        }
        break;

    case TOK_KW_ENUM: {
        int l2 = p->current.line, c2 = p->current.col;
        parser_advance(p);
        GclAstNode *en = gcl_ast_new(p->arena, AST_ENUM_DECL, l2, c2);
        if (parser_check(p, TOK_IDENT)) { en->str_value = tok_str(p, &p->current); parser_advance(p); }
        if (parser_check(p, TOK_LBRACE)) {
            int64_t ev = 0;
            parse_enum_body(p, &ev);
        }
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
        return en;
    }

    default:
        break;
    }

    if (looks_like_func(p)) {
        return parse_func_decl(p);
    }
    if (looks_like_decl(p)) {
        return parse_var_decl(p);
    }

    {
        int line = p->current.line, col = p->current.col;
        GclAstNode *expr = parse_expr(p);
        GclAstNode *stmt = gcl_ast_new(p->arena, AST_EXPR_STMT, line, col);
        gcl_ast_add_child(stmt, expr);
        if (parser_check(p, TOK_SEMICOLON)) parser_advance(p);
        return stmt;
    }
}

/* ── public API ──────────────────────────────────── */

void gcl_parser_init(GclParser *p, const char *source, GclArena *arena, GclStringIntern *intern, GclDiagBag *diag, const char *filepath) {
    memset(p, 0, sizeof(*p));
    gcl_lexer_init(&p->lex, source, arena, intern);
    p->arena = arena;
    p->intern = intern;
    p->diag = diag;
    p->filepath = filepath ? filepath : "<input>";
    parser_advance(p);
}

GclAstNode *gcl_parser_parse(GclParser *p) {
    GclAstNode *program = gcl_ast_new(p->arena, AST_PROGRAM, 1, 1);
    while (!parser_check(p, TOK_EOF)) {
        GclAstNode *node = parse_stmt(p);
        if (node) gcl_ast_add_child(program, node);
    }
    return program;
}
