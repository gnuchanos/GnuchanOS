/*
 * gcl_parser.c — GCL AST parser.
 *
 * Desteklenen sözdizimi (simple_doc.md):
 *   type variable = expr;  printf("{} {}", a, b);  if/else, while, for, switch,
 *   struct/enum/typedef, function decl, return, break, continue,
 *   member access (obj.field), call (fn(args)), binary ops.
 */

#include "gcl_parser.h"
#include "gcl_lexer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    GclToken *toks;
    int count;
    int pos;
    int err;
    char *msg;
} Parser;

static GclToken *peek(Parser *p) { return &p->toks[p->pos]; }
static GclToken *advance(Parser *p) {
    GclToken *t = &p->toks[p->pos];
    if (t->type != TOK_EOF) p->pos++;
    return t;
}
static int check(Parser *p, GclTokenType t) { return peek(p)->type == t; }
static int match(Parser *p, GclTokenType t) {
    if (check(p, t)) { advance(p); return 1; }
    return 0;
}
static void set_err(Parser *p, const char *m) {
    if (p->err) return;
    char b[256];
    snprintf(b, sizeof(b), "%s at %d:%d", m, peek(p)->line, peek(p)->col);
    p->msg = strdup(b);
    p->err = 1;
}
static char *tok_str(GclToken *t) {
    if (!t) return NULL;
    /* String token'ında ilk/son tırnak karakterlerini kaldır */
    size_t start = 0;
    size_t end = t->len;
    if (t->len >= 2 && (t->lexeme[0] == '"' || t->lexeme[0] == '\'') &&
        t->lexeme[t->len - 1] == t->lexeme[0]) {
        start = 1;
        end = t->len - 1;
    }
    size_t n = end - start;
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, t->lexeme + start, n);
    s[n] = '\0';
    return s;
}

static GclExpr *new_expr(GclExprKind k) {
    GclExpr *e = (GclExpr *)calloc(1, sizeof(GclExpr));
    if (e) e->kind = k;
    return e;
}

/* ---------- Expression (Pratt / precedence climbing) ---------- */

static GclExpr *parse_expr(Parser *p, int min_prec);

static GclExpr *parse_primary(Parser *p) {
    GclToken *t = peek(p);
    if (t->type == TOK_NUMBER) {
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_FLOAT);
        if (e) e->num = t->num;
        /* keep original token lexeme for large integer printing */
        if (e) e->str = tok_str(t);
        if (t->len >= 2 && t->lexeme[0] == '0' && (t->lexeme[1] == 'x' || t->lexeme[1] == 'X')) {
            e->num = (double)strtoll(t->lexeme, NULL, 16);
        }
        return e;
    }
    if (t->type == TOK_STRING) {
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_STRING);
        if (e) e->str = tok_str(t);
        return e;
    }
    /* Unary operator alias kontrolü: NOT, BIT_XOR (~) */
    if (t->type == TOK_IDENT) {
        char *alias_name = tok_str(t);
        if (alias_name && (strcmp(alias_name, "NOT") == 0 || strcmp(alias_name, "BIT_XOR") == 0)) {
            advance(p);
            GclExpr *right = parse_expr(p, 12);
            GclExpr *e = new_expr(AST_EXPR_UNOP);
            if (e) {
                e->left = NULL;
                e->right = right;
                e->op = OP_BITNOT;  /* ~ — bitwise NOT veya BIT_XOR */
            }
            free(alias_name);
            return e;
        }
        free(alias_name);
    }
    if (t->type == TOK_IDENT || t->type == TOK_KEYWORD || t->type == TOK_TYPE_NAME) {
        if (t->type == TOK_KEYWORD && t->len == 4 && strncmp(t->lexeme, "true", 4) == 0) { advance(p); GclExpr *e = new_expr(AST_EXPR_FLOAT); if (e) e->num = 1; return e; }
        if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "false", 5) == 0) { advance(p); GclExpr *e = new_expr(AST_EXPR_FLOAT); if (e) e->num = 0; return e; }
        char *name = tok_str(t);
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_VAR);
        if (e) e->name = name;
        /* chained member/array access: support x.y, x[i], and combinations like x[i].y */
        for (;;) {
            if (match(p, TOK_DOT)) {
                GclToken *mt = peek(p);
                if (mt->type != TOK_IDENT && mt->type != TOK_KEYWORD) {
                    GclToken *dot = &p->toks[p->pos - 1];
                    char b[512];
                    snprintf(b, sizeof(b),
                             "expected member name after '.' at %d:%d (got '%.*s' — write e.g. Module.member)",
                             dot->line, dot->col, (int)mt->len, mt->lexeme);
                    if (!p->err) { p->msg = strdup(b); p->err = 1; }
                    free(name);
                    return e;
                }
                advance(p);
                GclExpr *m = new_expr(AST_EXPR_MEMBER);
                if (m) {
                    m->left = e;
                    m->member_name = tok_str(mt);
                }
                e = m;
                continue;
            }
            if (match(p, TOK_LBRACKET)) {
                GclExpr *idx = parse_expr(p, 0);
                match(p, TOK_RBRACKET);
                if (p->err) break;
                GclExpr *arr = new_expr(AST_EXPR_ARRAY);
                if (arr) {
                    arr->left = e;
                    arr->right = idx;
                }
                e = arr;
                continue;
            }
            break;
        }
        /* call x(...) */
        if (match(p, TOK_LPAREN)) {
            GclExpr *call = new_expr(AST_EXPR_CALL);
            if (call) call->left = e;
            int cap = 0;
            if (!check(p, TOK_RPAREN)) {
                for (;;) {
                    GclExpr *arg = parse_expr(p, 0);
                    if (p->err) break;
                    if (call->arg_count >= cap) {
                        cap = cap ? cap * 2 : 4;
                        call->args = (GclExpr **)realloc(call->args, (size_t)cap * sizeof(GclExpr *));
                    }
                    call->args[call->arg_count++] = arg;
                    if (!match(p, TOK_COMMA)) break;
                }
            }
            if (!match(p, TOK_RPAREN)) { set_err(p, "expected ')'"); }
            e = call;
        }
        /* postfix ++ / -- */
        if (check(p, TOK_PLUSPLUS) || check(p, TOK_MINUSMINUS)) {
            GclTokenType op = peek(p)->type;
            advance(p);
            GclExpr *bin = new_expr(AST_EXPR_BINOP);
            if (bin) {
                /* KRİTİK: bin->left ve as->left AYNI pointer olamaz.
                   Eski kod `bin->left = e; as->left = e;` yapıyordu — free_expr
                   aynı Var'ı İKİ KEZ free ediyordu (for döngüsündeki i++ →
                   segfault / double-free). bin->left için KOPYA var oluştur. */
                GclExpr *var_clone = new_expr(AST_EXPR_VAR);
                if (var_clone) {
                    var_clone->name = (e->kind == AST_EXPR_VAR) ? strdup(e->name ? e->name : "") : (char *)"";
                    if (e->kind != AST_EXPR_VAR) { free(var_clone->name); var_clone->name = strdup(""); }
                    bin->left = var_clone;
                }
                GclExpr *one = new_expr(AST_EXPR_FLOAT);
                if (one) one->num = 1;
                bin->right = one;
                bin->op = (op == TOK_PLUSPLUS) ? OP_ADD : OP_SUB;
            }
            GclExpr *as = new_expr(AST_EXPR_ASSIGN);
            if (as) { as->left = e; as->right = bin; }
            e = as;
        }
        return e;
    }
    if (t->type == TOK_LBRACE) {
        /* struct init listesi:
           { "John", 20 }                  → pozisyonel
           { .testo = 30, .name = "BETA" } → designated (member adı ile)
           {}                              → boş (default) */
        advance(p);
        GclExpr *e = new_expr(AST_EXPR_INIT_LIST);
        if (!e) return NULL;
        int cap = 0;
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            GclExpr *item;
            if (check(p, TOK_DOT)) {
                /* designated initializer: .member = value */
                advance(p);
                GclToken *mt = peek(p);
                if (mt->type != TOK_IDENT && mt->type != TOK_KEYWORD) {
                    GclToken *dot = &p->toks[p->pos - 1];
                    char b[512];
                    snprintf(b, sizeof(b),
                             "expected member name after '.' at %d:%d (got '%.*s' — write e.g. .testo = 30)",
                             dot->line, dot->col, (int)mt->len, mt->lexeme);
                    if (!p->err) { p->msg = strdup(b); p->err = 1; }
                    item = NULL;
                    break;
                }
                char *mname = tok_str(mt);
                advance(p);
                GclExpr *target = new_expr(AST_EXPR_VAR);
                if (target) target->name = strdup(mname ? mname : "");
                free(mname);
                match(p, TOK_ASSIGN);
                GclExpr *val = parse_expr(p, 0);
                item = new_expr(AST_EXPR_ASSIGN);
                if (item) { item->left = target; item->right = val; }
            } else {
                /* pozisyonel: { 20, 30.5, "world", "ALPHA" } */
                item = parse_expr(p, 0);
            }
            if (p->err) break;
            if (e->arg_count >= cap) {
                cap = cap ? cap * 2 : 4;
                e->args = (GclExpr **)realloc(e->args, (size_t)cap * sizeof(GclExpr *));
            }
            e->args[e->arg_count++] = item;
            if (!match(p, TOK_COMMA)) break;
        }
        match(p, TOK_RBRACE);
        return e;
    }
    if (t->type == TOK_LPAREN) {
        advance(p);
        int save = p->pos;
        /* Compound literal cast: (Vector3){ 10, 20, 30 } → init list olarak parse et.
           C'ye yakın sözdizimi: type adı + ')' + '{' görülürse compound literal. */
        if ((peek(p)->type == TOK_TYPE_NAME || peek(p)->type == TOK_IDENT)) {
            advance(p); /* type */
            if (match(p, TOK_RPAREN)) {
                if (check(p, TOK_LBRACE)) {
                    GclExpr *init = parse_primary(p); /* { ... } → AST_EXPR_INIT_LIST */
                    return init;
                }
                /* sadece bir cast değil — normal parantez ifadesi olabilir */
            }
        }
        p->pos = save;
        GclExpr *e = parse_expr(p, 0);
        if (!match(p, TOK_RPAREN)) set_err(p, "expected ')'");
        return e;
    }
    if (t->type == TOK_MINUS || t->type == TOK_NOT || t->type == TOK_PLUS ||
        t->type == TOK_TILDE || t->type == TOK_AND) {
        GclTokenType op = t->type;
        advance(p);
        GclExpr *right = parse_expr(p, 12);
        GclExpr *e = new_expr(AST_EXPR_UNOP);
        if (e) {
            e->left = NULL;
            e->right = right;
            if (op == TOK_MINUS) e->op = OP_SUB;
            else if (op == TOK_PLUS) e->op = OP_ADD;
            else if (op == TOK_NOT) e->op = OP_NOT;    /* ! — boolean NOT */
            else if (op == TOK_AND) e->op = OP_BITAND;  /* & — address-of */
            else e->op = OP_BITNOT;                     /* ~ — bitwise NOT */
        }
        return e;
    }
    set_err(p, "expected expression");
    return NULL;
}

static int binop_prec(GclTokenType t) {
    switch (t) {
        case TOK_OR: case TOK_PIPEPIPE: return 1;
        case TOK_XOR: return 2;
        case TOK_AND: case TOK_AMPAMP: return 3;
        case TOK_EQ: case TOK_NE: return 4;
        case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE: return 5;
        case TOK_SHL: case TOK_SHR: return 6;
        case TOK_PLUS: case TOK_MINUS: return 7;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 8;
        default: return 0;
    }
}

static GclBinOp to_binop(GclTokenType t) {
    switch (t) {
        case TOK_PLUS: return OP_ADD;
        case TOK_MINUS: return OP_SUB;
        case TOK_STAR: return OP_MUL;
        case TOK_SLASH: return OP_DIV;
        case TOK_PERCENT: return OP_MOD;
        case TOK_EQ: return OP_EQ;
        case TOK_NE: return OP_NE;
        case TOK_LT: return OP_LT;
        case TOK_GT: return OP_GT;
        case TOK_LE: return OP_LE;
        case TOK_GE: return OP_GE;
        case TOK_AMPAMP: return OP_AND;    /* && — boolean AND */
        case TOK_AND: return OP_BITAND;    /* &  — bitwise AND */
        case TOK_PIPEPIPE: return OP_OR;   /* || — boolean OR */
        case TOK_OR: return OP_BITOR;      /* |  — bitwise OR */
        case TOK_XOR: return OP_BITXOR;    /* ^  — bitwise XOR */
        case TOK_SHL: return OP_SHL;
        case TOK_SHR: return OP_SHR;
        default: return OP_ASSIGN;
    }
}

/* Operator token'ını string'e dönüştür — typedef && AND; desteklemek için */
static char *operator_to_string(GclTokenType t) {
    switch (t) {
        case TOK_PLUS: return strdup("+");
        case TOK_MINUS: return strdup("-");
        case TOK_STAR: return strdup("*");
        case TOK_SLASH: return strdup("/");
        case TOK_PERCENT: return strdup("%");
        case TOK_EQ: return strdup("==");
        case TOK_NE: return strdup("!=");
        case TOK_LT: return strdup("<");
        case TOK_GT: return strdup(">");
        case TOK_LE: return strdup("<=");
        case TOK_GE: return strdup(">=");
        case TOK_AMPAMP: return strdup("&&");
        case TOK_AND: return strdup("&");
        case TOK_PIPEPIPE: return strdup("||");
        case TOK_OR: return strdup("|");
        case TOK_XOR: return strdup("^");
        case TOK_SHL: return strdup("<<");
        case TOK_SHR: return strdup(">>");
        case TOK_NOT: return strdup("!");
        case TOK_TILDE: return strdup("~");
        default: return NULL;
    }
}

/* Operator alias identifier'ını operator token'ına dönüştür
   Örnek: "AND" -> TOK_AMPAMP, "OR" -> TOK_PIPEPIPE, "XOR" -> TOK_XOR */
static GclTokenType resolve_operator_alias(const char *name) {
    if (!name) return TOK_EOF;
    if (strcmp(name, "AND") == 0) return TOK_AMPAMP;      /* && */
    if (strcmp(name, "OR") == 0) return TOK_PIPEPIPE;     /* || */
    if (strcmp(name, "XOR") == 0) return TOK_XOR;         /* ^ */
    if (strcmp(name, "NOT") == 0) return TOK_NOT;         /* ! */
    if (strcmp(name, "BIT_AND") == 0) return TOK_AND;     /* & */
    if (strcmp(name, "BIT_OR") == 0) return TOK_OR;       /* | */
    if (strcmp(name, "BIT_XOR") == 0) return TOK_XOR;     /* ^ */
    if (strcmp(name, "LEFT_SHIFT") == 0) return TOK_SHL;  /* << */
    if (strcmp(name, "RIGHT_SHIFT") == 0) return TOK_SHR; /* >> */
    return TOK_EOF;  /* İşleç alias değil */
}

/* binary expr: parse_primary + binary operatörler */
static GclExpr *parse_expr_binary(Parser *p, int min_prec) {
    GclExpr *left = parse_primary(p);
    if (!left || p->err) return left;
    for (;;) {
        GclToken *t = peek(p);
        int prec = binop_prec(t->type);
        
        /* Operator alias kontrolü: IDENT token'ı operator alias olabilir mi? */
        GclTokenType resolved_op = TOK_EOF;
        if (t->type == TOK_IDENT && prec == 0) {
            char *alias_name = tok_str(t);
            resolved_op = resolve_operator_alias(alias_name);
            if (resolved_op != TOK_EOF) {
                prec = binop_prec(resolved_op);
                t = &(GclToken){.type = resolved_op, .lexeme = "", .len = 0, .line = 0, .col = 0};
            }
        }
        
        if (prec < min_prec) break;
        advance(p);
        GclBinOp bop = to_binop(t->type);
        GclExpr *right = parse_expr_binary(p, prec + 1);
        GclExpr *e = new_expr(AST_EXPR_BINOP);
        if (e) { e->left = left; e->right = right; e->op = bop; }
        left = e;
        if (p->err) break;
    }
    return left;
}

/* assignment: a = expr, a += b, ... */
static GclExpr *parse_assign(Parser *p) {
    GclExpr *left = parse_expr_binary(p, 1);
    if (!left || p->err) return left;
    if (match(p, TOK_ASSIGN)) {
        GclExpr *right = parse_assign(p);
        GclExpr *e = new_expr(AST_EXPR_ASSIGN);
        if (e) { e->left = left; e->right = right; }
        return e;
    }
    if (check(p, TOK_PLUSEQ) || check(p, TOK_MINUSEQ) || check(p, TOK_STAREQ) || check(p, TOK_SLASHEQ)) {
        GclTokenType op = peek(p)->type;
        advance(p);
        GclExpr *right = parse_assign(p);
        GclExpr *e = new_expr(AST_EXPR_ASSIGN);
        GclBinOp bop = op == TOK_PLUSEQ ? OP_ADD : (op == TOK_MINUSEQ ? OP_SUB : (op == TOK_STAREQ ? OP_MUL : OP_DIV));
        if (e) {
            GclExpr *bin = new_expr(AST_EXPR_BINOP);
            if (bin) { bin->left = clone_expr(left); bin->right = right; bin->op = bop; }
            e->left = left;
            e->right = bin;
        }
        return e;
    }
    return left;
}

static GclExpr *parse_expr(Parser *p, int min_prec) {
    return parse_assign(p);
}

/* ---------- Statements ---------- */

static GclStmt *parse_stmt(Parser *p);

/* Çok kelimeli tip adı: "long int", "unsigned int", "long double", "long long int" ... */
static char *parse_type_name(Parser *p) {
    if (peek(p)->type != TOK_TYPE_NAME) return NULL;
    char buf[256] = "";
    while (peek(p)->type == TOK_TYPE_NAME) {
        char *part = tok_str(peek(p));
        if (!part) break;
        if (buf[0]) strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
        free(part);
        advance(p);
    }
    return strdup(buf);
}

/* switch case body'sini topla: sonraki case/default/}'e kadar */
static GclStmt *parse_case_body(Parser *p) {
    GclStmt *blk = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (!blk) return NULL;
    blk->kind = STMT_BLOCK;
    int cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (check(p, TOK_KEYWORD) &&
            ((peek(p)->len == 4 && strncmp(peek(p)->lexeme, "case", 4) == 0) ||
             (peek(p)->len == 7 && strncmp(peek(p)->lexeme, "default", 7) == 0))) break;
        GclStmt *st = parse_stmt(p);
        if (!st) break;
        if (blk->u.block.count >= cap) {
            cap = cap ? cap * 2 : 8;
            blk->u.block.stmts = (GclStmt **)realloc(blk->u.block.stmts, (size_t)cap * sizeof(GclStmt *));
        }
        blk->u.block.stmts[blk->u.block.count++] = st;
    }
    return blk;
}

static GclStmt *parse_block(Parser *p) {
    if (!match(p, TOK_LBRACE)) { set_err(p, "expected '{'"); return NULL; }
    GclStmt *blk = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (!blk) return NULL;
    blk->kind = STMT_BLOCK;
    int cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (p->msg) break;
        GclStmt *s = parse_stmt(p);
        if (!s) break;
        if (blk->u.block.count >= cap) {
            cap = cap ? cap * 2 : 8;
            blk->u.block.stmts = (GclStmt **)realloc(blk->u.block.stmts, (size_t)cap * sizeof(GclStmt *));
        }
        blk->u.block.stmts[blk->u.block.count++] = s;
    }
    if (match(p, TOK_RBRACE)) return blk;
    set_err(p, "expected '}'");
    return blk;
}

static GclStmt *new_stmt(GclStmtKind k) {
    GclStmt *s = (GclStmt *)calloc(1, sizeof(GclStmt));
    if (s) s->kind = k;
    return s;
}

static GclStmt *parse_stmt(Parser *p) {
    GclToken *t = peek(p);

    /* preprocessor satırı: atla */
    if (t->type == TOK_PREPROC) {
        advance(p);
        return (GclStmt *)calloc(1, sizeof(GclStmt)); /* empty */
    }

    /* modifier: const/public/private/global/local/inline — tüket ve devam et.
       global: değişken kalıcı olur (fonksiyon dışına taşmaz),
       local: değişken yerel olur (çağrı sonrası silinir),
       inline: no-op, const: atanamaz. */
    int mod_const = 0;
    int mod_global = 0;
    int mod_local = 0;
    while (t->type == TOK_KEYWORD &&
           (strncmp(t->lexeme, "const", 5) == 0 ||
            strncmp(t->lexeme, "public", 6) == 0 ||
            strncmp(t->lexeme, "private", 7) == 0 ||
            strncmp(t->lexeme, "global", 6) == 0 ||
            strncmp(t->lexeme, "local", 5) == 0 ||
            strncmp(t->lexeme, "inline", 6) == 0)) {
        if (t->len == 5 && strncmp(t->lexeme, "const", 5) == 0) mod_const = 1;
        if (t->len == 6 && strncmp(t->lexeme, "global", 6) == 0) mod_global = 1;
        if (t->len == 5 && strncmp(t->lexeme, "local", 5) == 0) mod_local = 1;
        advance(p);
        t = peek(p);
    }

    /* Typeless global/local listesi: "global g2, g_count;" veya "local local_x;"
       — tip belirtilmez, sadece isimler. global: dışarıda tanımlı/kalıcı değişkene
       bağlanır (yoksa 0 ile oluşturulur); local: çağrı sonrası silinen yerel. */
    if ((mod_global || mod_local) && t->type == TOK_IDENT) {
        GclStmt *head = NULL, *tail = NULL;
        while (t->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                s->u.var_decl.name = tok_str(t);
                s->u.var_decl.type_name = NULL;
                s->u.var_decl.is_global = mod_global;
                s->u.var_decl.is_const = mod_const;
                s->u.var_decl.array_size = 0;
                s->u.var_decl.is_pointer = 0;
            }
            if (!head) head = s; else tail->next = s;
            tail = s;
            advance(p);
            t = peek(p);
            if (match(p, TOK_COMMA)) { t = peek(p); continue; }
            break;
        }
        match(p, TOK_SEMI);
        if (head) return head;
    }

    /* block */
    if (t->type == TOK_LBRACE) return parse_block(p);

    /* if */
    if (t->type == TOK_KEYWORD && t->len == 2 && strncmp(t->lexeme, "if", 2) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_IF);
        if (!match(p, TOK_LPAREN)) set_err(p, "expected '(' after if");
        else s->u.if_.cond = parse_expr(p, 0);
        match(p, TOK_RPAREN);
        if (p->err) return s;
        s->u.if_.then = parse_stmt(p);
        if (check(p, TOK_KEYWORD) && peek(p)->len == 4 && strncmp(peek(p)->lexeme, "else", 4) == 0) {
            advance(p);
            s->u.if_.else_ = parse_stmt(p);
        }
        return s;
    }
    /* while */
    if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "while", 5) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_WHILE);
        if (!match(p, TOK_LPAREN)) set_err(p, "expected '(' after while");
        else s->u.while_.cond = parse_expr(p, 0);
        match(p, TOK_RPAREN);
        s->u.while_.body = parse_stmt(p);
        return s;
    }
    /* for */
    if (t->type == TOK_KEYWORD && t->len == 3 && strncmp(t->lexeme, "for", 3) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_FOR);
        if (match(p, TOK_LPAREN)) {
            /* init: TYPE NAME = expr | expr */
            if (peek(p)->type == TOK_TYPE_NAME) {
                char *type_name = parse_type_name(p);
                GclToken *nt = peek(p);
                char *vname = nt->type == TOK_IDENT ? tok_str(nt) : NULL;
                if (nt->type == TOK_IDENT) advance(p);
                if (match(p, TOK_ASSIGN)) {
                    GclExpr *init = parse_expr(p, 0);
                    if (vname) {
                        GclExpr *var_e = new_expr(AST_EXPR_VAR);
                        GclExpr *assign_e = new_expr(AST_EXPR_ASSIGN);
                        if (var_e) var_e->name = strdup(vname);
                        if (assign_e) { assign_e->left = var_e; assign_e->right = init; }
                        s->u.for_.var = assign_e ? assign_e : init;
                    } else {
                        s->u.for_.var = init;
                    }
                    free(type_name);
                    free(vname);
                } else {
                    free(type_name);
                    free(vname);
                }
            } else {
                s->u.for_.var = parse_expr(p, 0);
            }
            match(p, TOK_SEMI);
            if (!check(p, TOK_SEMI)) s->u.for_.cond = parse_expr(p, 0);
            match(p, TOK_SEMI);
            if (!check(p, TOK_RPAREN)) s->u.for_.inc = parse_expr(p, 0);
            match(p, TOK_RPAREN);
        }
        s->u.for_.body = parse_stmt(p);
        return s;
    }
    /* switch */
    if (t->type == TOK_KEYWORD && t->len == 6 && strncmp(t->lexeme, "switch", 6) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_SWITCH);
        if (match(p, TOK_LPAREN)) {
            if (peek(p)->type == TOK_IDENT || peek(p)->type == TOK_KEYWORD) {
                s->u.switch_.name = tok_str(peek(p));
                advance(p);
            }
        }
        match(p, TOK_RPAREN);
        match(p, TOK_LBRACE);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (check(p, TOK_KEYWORD) && peek(p)->len == 4 && strncmp(peek(p)->lexeme, "case", 4) == 0) {
                advance(p);
                GclExpr *val = parse_expr(p, 0);
                match(p, TOK_COLON);
                GclStmt *body = parse_case_body(p);
                s->u.switch_.case_vals = (GclExpr **)realloc(s->u.switch_.case_vals, (size_t)(s->u.switch_.case_count + 1) * sizeof(GclExpr *));
                s->u.switch_.cases = (GclStmt **)realloc(s->u.switch_.cases, (size_t)(s->u.switch_.case_count + 1) * sizeof(GclStmt *));
                s->u.switch_.case_vals[s->u.switch_.case_count] = val;
                s->u.switch_.cases[s->u.switch_.case_count] = body;
                s->u.switch_.case_count++;
            } else if (check(p, TOK_KEYWORD) && peek(p)->len == 7 && strncmp(peek(p)->lexeme, "default", 7) == 0) {
                advance(p);
                match(p, TOK_COLON);
                s->u.switch_.default_case = parse_case_body(p);
            } else {
                GclStmt *cs = parse_stmt(p);
                free(cs);
                if (check(p, TOK_SEMI)) advance(p);
            }
        }
        match(p, TOK_RBRACE);
        return s;
    }
    /* return */
    if (t->type == TOK_KEYWORD && t->len == 6 && strncmp(t->lexeme, "return", 6) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_RETURN);
        if (!check(p, TOK_SEMI) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            s->u.ret.expr = parse_expr(p, 0);
        }
        match(p, TOK_SEMI);
        return s;
    }
    /* break/continue */
    if (t->type == TOK_KEYWORD && t->len == 5 && strncmp(t->lexeme, "break", 5) == 0) {
        advance(p); GclStmt *s = new_stmt(STMT_BREAK); match(p, TOK_SEMI); return s;
    }
    if (t->type == TOK_KEYWORD && t->len == 8 && strncmp(t->lexeme, "continue", 8) == 0) {
        advance(p); GclStmt *s = new_stmt(STMT_CONTINUE); match(p, TOK_SEMI); return s;
    }
    /* typedef int number;  veya  typedef struct { ... } Name;  veya  typedef enum { ... } Name;
       veya operator alias: typedef && AND; typedef || OR; typedef << LEFT_SHIFT; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "typedef", 7) == 0) {
        advance(p);
        GclStmt *s = new_stmt(STMT_TYPEDEF);
        if (!s) return NULL;
        
        /* Operator alias kontrolü: typedef && AND; typedef || OR; vb. */
        if (peek(p)->type == TOK_PLUS || peek(p)->type == TOK_MINUS || peek(p)->type == TOK_STAR ||
            peek(p)->type == TOK_SLASH || peek(p)->type == TOK_PERCENT || peek(p)->type == TOK_EQ ||
            peek(p)->type == TOK_NE || peek(p)->type == TOK_LT || peek(p)->type == TOK_GT ||
            peek(p)->type == TOK_LE || peek(p)->type == TOK_GE || peek(p)->type == TOK_AMPAMP ||
            peek(p)->type == TOK_AND || peek(p)->type == TOK_PIPEPIPE || peek(p)->type == TOK_OR ||
            peek(p)->type == TOK_XOR || peek(p)->type == TOK_SHL || peek(p)->type == TOK_SHR ||
            peek(p)->type == TOK_NOT || peek(p)->type == TOK_TILDE) {
            GclTokenType op_type = peek(p)->type;
            advance(p);
            s->u.typedef_info.base_type = operator_to_string(op_type);
            /* alias adı */
            if (peek(p)->type == TOK_IDENT) {
                s->u.typedef_info.alias_name = tok_str(peek(p));
                advance(p);
            }
            match(p, TOK_SEMI);
            return s;
        }
        
        if (peek(p)->type == TOK_TYPE_NAME) {
            s->u.typedef_info.base_type = parse_type_name(p);
        } else if (peek(p)->type == TOK_KEYWORD && strncmp(peek(p)->lexeme, "struct", 6) == 0) {
            advance(p);
            char *sname = NULL;
            if (peek(p)->type == TOK_IDENT) { sname = tok_str(peek(p)); advance(p); }
            if (!match(p, TOK_LBRACE)) { /* değilse: typedef struct Name; — basit, geç */ }
            else {
                /* gövdeyi gerçek parse et: member'ları topla */
                GclStmt *sd = new_stmt(STMT_STRUCT_DECL);
                if (sd) {
                    int cap = 0;
                    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                        /* Member tipi: bilinen tip (TOK_TYPE_NAME) VEYA başka struct adı (TOK_IDENT) */
                        GclToken *mtype_tok = peek(p);
                        char *mtype = NULL;
                        if (mtype_tok->type == TOK_TYPE_NAME) {
                            mtype = parse_type_name(p);
                        } else if (mtype_tok->type == TOK_IDENT) {
                            mtype = tok_str(mtype_tok);
                            advance(p);
                        } else {
                            /* Geçersiz token — sonsuz döngüyü önle, atla */
                            advance(p);
                            continue;
                        }
                        if (peek(p)->type == TOK_IDENT) {
                            if (sd->u.struct_info.member_count >= cap) {
                                cap = cap ? cap * 2 : 4;
                                sd->u.struct_info.member_names = (char **)realloc(sd->u.struct_info.member_names, (size_t)cap * sizeof(char *));
                                sd->u.struct_info.member_types = (char **)realloc(sd->u.struct_info.member_types, (size_t)cap * sizeof(char *));
                            }
                            sd->u.struct_info.member_names[sd->u.struct_info.member_count] = tok_str(peek(p));
                            sd->u.struct_info.member_types[sd->u.struct_info.member_count] = mtype;
                            sd->u.struct_info.member_count++;
                            advance(p);
                            if (match(p, TOK_LBRACKET)) {
                                while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                                match(p, TOK_RBRACKET);
                            }
                        } else {
                            free(mtype);
                        }
                        match(p, TOK_SEMI);
                    }
                    match(p, TOK_RBRACE);
                }
                /* struct adı: inline blokta typedef alias adı olacak */
                if (sname) free(sname);
                /* alias adı daha sonra okunacak — sname'i sonraki IDENT olarak kullan */
                if (peek(p)->type == TOK_IDENT) {
                    s->u.typedef_info.alias_name = tok_str(peek(p));
                    if (sd) sd->u.struct_info.struct_name = strdup(s->u.typedef_info.alias_name);
                    advance(p);
                }
                char base[128];
                snprintf(base, sizeof(base), "struct %s", s->u.typedef_info.alias_name ? s->u.typedef_info.alias_name : "");
                s->u.typedef_info.base_type = strdup(base);
                s->next = sd;
            }
            match(p, TOK_SEMI);
            return s;
        } else if (peek(p)->type == TOK_KEYWORD && strncmp(peek(p)->lexeme, "enum", 4) == 0) {
            advance(p);
            char *ename = NULL;
            if (peek(p)->type == TOK_IDENT) { ename = tok_str(peek(p)); advance(p); }
            GclStmt *en = NULL;
            if (match(p, TOK_LBRACE)) {
                en = new_stmt(STMT_ENUM);
                if (en) {
                    int cap = 0;
                    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                        if (peek(p)->type == TOK_IDENT || peek(p)->type == TOK_KEYWORD) {
                            if (en->u.enum_info.const_count >= cap) {
                                cap = cap ? cap * 2 : 4;
                                en->u.enum_info.const_names = (char **)realloc(en->u.enum_info.const_names, (size_t)cap * sizeof(char *));
                            }
                            en->u.enum_info.const_names[en->u.enum_info.const_count++] = tok_str(peek(p));
                            advance(p);
                        }
                        if (!match(p, TOK_COMMA)) break;
                    }
                    match(p, TOK_RBRACE);
                }
            } else if (ename) {
                en = NULL; /* enum Name; kullanımı — tek başına */
            }
            if (peek(p)->type == TOK_IDENT) {
                s->u.typedef_info.alias_name = tok_str(peek(p));
                if (en) en->u.enum_info.enum_name = strdup(s->u.typedef_info.alias_name);
                advance(p);
            }
            s->u.typedef_info.base_type = strdup("enum");
            if (en) s->next = en;
            if (ename) free(ename);
            match(p, TOK_SEMI);
            return s;
        }
        /* alias adı */
        if (peek(p)->type == TOK_IDENT) {
            s->u.typedef_info.alias_name = tok_str(peek(p));
            advance(p);
        }
        match(p, TOK_SEMI);
        return s;
    }
    /* enum Day { MONDAY, TUESDAY }; veya enum { ... } var; veya enum Day today; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "enum", 4) == 0) {
        advance(p);
        /* enum adı olabilir: enum Day ... */
        char *enum_name = NULL;
        if (peek(p)->type == TOK_IDENT) {
            enum_name = tok_str(peek(p));
            advance(p);
        }
        /* enum tanımı: enum Day { ... }; */
        if (match(p, TOK_LBRACE)) {
            GclStmt *s = new_stmt(STMT_ENUM);
            if (s) {
                if (enum_name) s->u.enum_info.enum_name = strdup(enum_name);
                int cap = 0;
                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    if (peek(p)->type == TOK_IDENT || peek(p)->type == TOK_KEYWORD) {
                        if (s->u.enum_info.const_count >= cap) {
                            cap = cap ? cap * 2 : 4;
                            s->u.enum_info.const_names = (char **)realloc(s->u.enum_info.const_names, (size_t)cap * sizeof(char *));
                        }
                        s->u.enum_info.const_names[s->u.enum_info.const_count++] = tok_str(peek(p));
                        advance(p);
                    }
                    if (!match(p, TOK_COMMA)) break;
                }
                match(p, TOK_RBRACE);
            }
            free(enum_name);
            match(p, TOK_SEMI);
            return s;
        }
        /* enum Day today; → var_decl */
        if (enum_name && peek(p)->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                char type_str[128];
                snprintf(type_str, sizeof(type_str), "enum %s", enum_name);
                s->u.var_decl.type_name = strdup(type_str);
                s->u.var_decl.name = tok_str(peek(p));
                advance(p);
                if (match(p, TOK_ASSIGN)) {
                    s->u.var_decl.init = parse_expr(p, 0);
                }
                match(p, TOK_SEMI);
            }
            free(enum_name);
            return s;
        }
        free(enum_name);
        match(p, TOK_SEMI);
        return (GclStmt *)calloc(1, sizeof(GclStmt));
    }
    /* struct Student { char name[20]; int age; };  veya  struct Student s1; */
    if (t->type == TOK_KEYWORD && strncmp(t->lexeme, "struct", 6) == 0) {
        advance(p);
        char *struct_type = NULL;
        if (peek(p)->type == TOK_IDENT) {
            struct_type = tok_str(peek(p));
            advance(p);
        }
        if (match(p, TOK_LBRACE)) {
            /* struct tanımı */
            GclStmt *s = new_stmt(STMT_STRUCT_DECL);
            if (s) {
                if (struct_type) {
                    s->u.struct_info.struct_name = strdup(struct_type);
                }
                int cap = 0;
                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    /* Member tipi: bilinen tip (TOK_TYPE_NAME) VEYA başka struct adı (TOK_IDENT) */
                    GclToken *mtype_tok = peek(p);
                    char *mtype = NULL;
                    if (mtype_tok->type == TOK_TYPE_NAME) {
                        mtype = parse_type_name(p);
                    } else if (mtype_tok->type == TOK_IDENT) {
                        mtype = tok_str(mtype_tok);
                        advance(p);
                    } else {
                        /* Geçersiz token — sonsuz döngüyü önle, atla */
                        advance(p);
                        continue;
                    }
                    if (peek(p)->type == TOK_IDENT) {
                        if (s->u.struct_info.member_count >= cap) {
                            cap = cap ? cap * 2 : 4;
                            s->u.struct_info.member_names = (char **)realloc(s->u.struct_info.member_names, (size_t)cap * sizeof(char *));
                            s->u.struct_info.member_types = (char **)realloc(s->u.struct_info.member_types, (size_t)cap * sizeof(char *));
                        }
                        s->u.struct_info.member_names[s->u.struct_info.member_count] = tok_str(peek(p));
                        s->u.struct_info.member_types[s->u.struct_info.member_count] = mtype;
                        s->u.struct_info.member_count++;
                        advance(p);
                        if (match(p, TOK_LBRACKET)) {
                            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                            match(p, TOK_RBRACKET);
                        }
                    } else {
                        free(mtype);
                    }
                    match(p, TOK_SEMI);
                }
                match(p, TOK_RBRACE);
            }
            match(p, TOK_SEMI);
            free(struct_type);
            return s;
        }
        /* değilse: struct {type} name; → var_decl
           Support arrays: struct T name[...]; and optional initializer. */
        if (struct_type && peek(p)->type == TOK_IDENT) {
            GclStmt *s = new_stmt(STMT_VAR_DECL);
            if (s) {
                char type_str[128];
                snprintf(type_str, sizeof(type_str), "struct %s", struct_type);
                s->u.var_decl.type_name = strdup(type_str);
                s->u.var_decl.array_size = 0;
                s->u.var_decl.array_inner = 0;
                s->u.var_decl.is_pointer = 0;
                s->u.var_decl.is_const = 0;
                s->u.var_decl.is_global = 0;
                s->u.var_decl.name = tok_str(peek(p));
                advance(p);
                /* array dimensions (optional) */
                if (match(p, TOK_LBRACKET)) {
                    if (check(p, TOK_NUMBER)) {
                        s->u.var_decl.array_size = (int)peek(p)->num;
                        advance(p);
                    } else if (check(p, TOK_RBRACKET)) {
                        s->u.var_decl.array_size = -1;
                    } else {
                        set_err(p, "expected array size");
                    }
                    match(p, TOK_RBRACKET);
                    if (match(p, TOK_LBRACKET)) {
                        if (check(p, TOK_NUMBER)) {
                            s->u.var_decl.array_inner = (int)peek(p)->num;
                            advance(p);
                        } else if (check(p, TOK_RBRACKET)) {
                            s->u.var_decl.array_inner = -1;
                        } else {
                            set_err(p, "expected array size");
                        }
                        match(p, TOK_RBRACKET);
                    }
                }
                if (match(p, TOK_ASSIGN)) {
                    s->u.var_decl.init = parse_expr(p, 0);
                }
                match(p, TOK_SEMI);
            }
            free(struct_type);
            return s;
        }
        free(struct_type);
        match(p, TOK_SEMI);
        return (GclStmt *)calloc(1, sizeof(GclStmt));
    }

    /* function decl: TYPE name(params) { ... } */
    if ((t->type == TOK_TYPE_NAME || t->type == TOK_IDENT) &&
        p->pos + 2 < p->count) {
        GclToken *nxt1 = &p->toks[p->pos + 1];
        GclToken *nxt2 = &p->toks[p->pos + 2];
        int is_func = (nxt1->type == TOK_IDENT && nxt2->type == TOK_LPAREN);
        if (t->type == TOK_TYPE_NAME) {
            int scan = p->pos;
            while (scan < p->count && p->toks[scan].type == TOK_TYPE_NAME) scan++;
            if (scan + 1 < p->count && p->toks[scan].type == TOK_IDENT &&
                p->toks[scan + 1].type == TOK_LPAREN) is_func = 1;
        }
        if (is_func) {
            /* TYPE name( */
            char *rtype = (t->type == TOK_TYPE_NAME) ? parse_type_name(p) : tok_str(t);
            if (t->type != TOK_TYPE_NAME) advance(p);
            GclStmt *s = new_stmt(STMT_FUNC_DECL);
            if (s) {
                s->u.func_decl.name = tok_str(peek(p));
                advance(p);
                if (match(p, TOK_LPAREN)) {
                    int cap = 0;
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        /* param: TYPE [*...] NAME [array]  veya  TYPE */
                        if (peek(p)->type == TOK_TYPE_NAME) {
                            char *ptype = parse_type_name(p);
                            (void)ptype; free(ptype);
                            /* pointer yıldızları: char *argv, char **argv */
                            while (check(p, TOK_STAR)) advance(p);
                            if (peek(p)->type == TOK_IDENT) {
                                if (s->u.func_decl.param_count >= cap) {
                                    cap = cap ? cap * 2 : 4;
                                    s->u.func_decl.params = (char **)realloc(s->u.func_decl.params, (size_t)cap * sizeof(char *));
                                }
                                s->u.func_decl.params[s->u.func_decl.param_count++] = tok_str(peek(p));
                                advance(p);
                                /* array boyutu: int arr[10] veya char *argv[] */
                                if (match(p, TOK_LBRACKET)) {
                                    while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) advance(p);
                                    match(p, TOK_RBRACKET);
                                }
                            }
                        }
                        if (!match(p, TOK_COMMA)) break;
                    }
                    match(p, TOK_RPAREN);
                }
                s->u.func_decl.body = parse_stmt(p); /* { ... } */
                /* type_name'ı sakla — body'de kullan */
                /* Basit: init olarak yok, tip bilgisi kaybolmaz */
                free(rtype);
            }
            return s;
        }
    }

    /* var decl: TYPE NAME = expr;  veya  char name[32];  char buf[];  char *ptr; */
    if (t->type == TOK_TYPE_NAME ||
        (t->type == TOK_IDENT && p->pos + 1 < p->count && p->toks[p->pos+1].type == TOK_IDENT)) {
        /* IDENT + IDENT = typedef alias + değişken adı (Student s1;) */
        char *type_name = (t->type == TOK_TYPE_NAME) ? parse_type_name(p) : tok_str(t);
        if (t->type != TOK_TYPE_NAME) advance(p);
        GclStmt *s = new_stmt(STMT_VAR_DECL);
        if (s) {
            s->u.var_decl.type_name = type_name;
            s->u.var_decl.array_size = 0;
            s->u.var_decl.is_pointer = 0;
            s->u.var_decl.is_const = mod_const;
            s->u.var_decl.is_global = mod_global;
            /* char *ptr — pointer */
            while (check(p, TOK_STAR)) { advance(p); s->u.var_decl.is_pointer = 1; }
            GclToken *name_t = peek(p);
            if (name_t->type == TOK_IDENT) {
                advance(p);
                s->u.var_decl.name = tok_str(name_t);
            } else {
                set_err(p, "expected variable name");
            }
            /* char name[N] veya char name[] (aynı zamanda çok boyutlu: [N][M]) */
            s->u.var_decl.array_inner = 0;
            if (match(p, TOK_LBRACKET)) {
                if (check(p, TOK_NUMBER)) {
                    s->u.var_decl.array_size = (int)peek(p)->num;
                    advance(p);
                } else if (check(p, TOK_RBRACKET)) {
                    s->u.var_decl.array_size = -1; /* boş [] */
                } else {
                    set_err(p, "expected array size");
                }
                match(p, TOK_RBRACKET);
                /* support second dimension for declarations like char a[3][20] */
                if (check(p, TOK_LBRACKET)) {
                    /* parse inner dimension but keep as array_inner */
                    if (match(p, TOK_LBRACKET)) {
                        if (check(p, TOK_NUMBER)) {
                            s->u.var_decl.array_inner = (int)peek(p)->num;
                            advance(p);
                        } else if (check(p, TOK_RBRACKET)) {
                            s->u.var_decl.array_inner = -1; /* empty inner */
                        } else {
                            set_err(p, "expected array size");
                        }
                        match(p, TOK_RBRACKET);
                    }
                }
            }
            if (match(p, TOK_ASSIGN)) {
                s->u.var_decl.init = parse_expr(p, 0);
            }
            match(p, TOK_SEMI);
        }
        return s;
    }

    /* expression statement */
    GclExpr *e = parse_expr(p, 0);
    if (p->err) return NULL;
    GclStmt *s = new_stmt(STMT_EXPR);
    if (s) s->u.expr = e;
    match(p, TOK_SEMI);
    return s;
}

GclProgram *gcl_parse(GclTokenList *tokens, char **error_msg) {
    if (error_msg) *error_msg = NULL;
    if (!tokens) return NULL;
    GclProgram *prog = (GclProgram *)calloc(1, sizeof(GclProgram));
    if (!prog) return NULL;

    Parser p;
    p.toks = tokens->tokens;
    p.count = tokens->count;
    p.pos = 0;
    p.err = 0;
    p.msg = NULL;

    int cap = 0;
    while (!check(&p, TOK_EOF) && !p.err) {
        GclStmt *s = parse_stmt(&p);
        if (!s) break;
        /* s->next zinciri: typedef struct/enum içinde ek lenmiş stmt'ler */
        GclStmt *chain = s;
        while (chain) {
            GclStmt *nx = chain->next;
            chain->next = NULL;
            if (chain->kind == STMT_EXPR && chain->u.expr == NULL) { free(chain); chain = nx; continue; }
            if (prog->count >= cap) {
                cap = cap ? cap * 2 : 16;
                prog->stmts = (GclStmt **)realloc(prog->stmts, (size_t)cap * sizeof(GclStmt *));
            }
            prog->stmts[prog->count++] = chain;
            chain = nx;
        }
    }

    if (p.err) {
        if (error_msg) *error_msg = p.msg;
        else free(p.msg);
        gcl_program_free(prog);
        return NULL;
    }
    return prog;
}

/* ---------- Derinlemesine AST free (memory leak) ---------- */

static void free_expr(GclExpr *e) {
    if (!e) return;
    if (e->left) free_expr(e->left);
    if (e->right) free_expr(e->right);
    if (e->args) {
        for (int i = 0; i < e->arg_count; i++) free_expr(e->args[i]);
        free(e->args);
    }
    if (e->str) free(e->str);
    if (e->name) free(e->name);
    if (e->member_name) free(e->member_name);
    free(e);
}

static void free_stmt(GclStmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_EXPR:
            free_expr(s->u.expr);
            break;
        case STMT_VAR_DECL:
            free(s->u.var_decl.name);
            free(s->u.var_decl.type_name);
            free_expr(s->u.var_decl.init);
            break;
        case STMT_IF:
            free_expr(s->u.if_.cond);
            free_stmt(s->u.if_.then);
            free_stmt(s->u.if_.else_);
            break;
        case STMT_WHILE:
            free_expr(s->u.while_.cond);
            free_stmt(s->u.while_.body);
            break;
        case STMT_FOR:
            free_expr(s->u.for_.var);
            free_expr(s->u.for_.cond);
            free_expr(s->u.for_.inc);
            free_stmt(s->u.for_.body);
            break;
        case STMT_RETURN:
            free_expr(s->u.ret.expr);
            free(s->u.ret.type_name);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < s->u.block.count; i++) free_stmt(s->u.block.stmts[i]);
            free(s->u.block.stmts);
            break;
        case STMT_FUNC_DECL:
            free(s->u.func_decl.name);
            for (int i = 0; i < s->u.func_decl.param_count; i++) free(s->u.func_decl.params[i]);
            free(s->u.func_decl.params);
            free(s->u.func_decl.return_type);
            free_stmt(s->u.func_decl.body);
            break;
        case STMT_SWITCH:
            free(s->u.switch_.name);
            for (int i = 0; i < s->u.switch_.case_count; i++) {
                free_expr(s->u.switch_.case_vals[i]);
                free_stmt(s->u.switch_.cases[i]);
            }
            free(s->u.switch_.case_vals);
            free(s->u.switch_.cases);
            free_stmt(s->u.switch_.default_case);
            break;
        case STMT_TYPEDEF:
            free(s->u.typedef_info.alias_name);
            free(s->u.typedef_info.base_type);
            break;
        case STMT_ENUM:
            free(s->u.enum_info.enum_name);
            for (int i = 0; i < s->u.enum_info.const_count; i++) free(s->u.enum_info.const_names[i]);
            free(s->u.enum_info.const_names);
            break;
        case STMT_STRUCT_DECL:
            free(s->u.struct_info.struct_name);
            for (int i = 0; i < s->u.struct_info.member_count; i++) {
                free(s->u.struct_info.member_names[i]);
                free(s->u.struct_info.member_types[i]);
            }
            free(s->u.struct_info.member_names);
            free(s->u.struct_info.member_types);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
    }
    free(s);
}

void gcl_program_free(GclProgram *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (!s) continue;
        free_stmt(s);
    }
    free(prog->stmts);
    free(prog);
}
