/* AST: dugum olusturma ve yazdirma */
#include "gcl.h"
#include "ast.h"

Node *node_alloc(NodeType t, SourceLoc loc) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) return NULL;
    n->type = t; n->loc = loc;
    return n;
}

Node *node_int(int val, SourceLoc loc) {
    Node *n = node_alloc(N_INT_LIT, loc);
    if (n) n->ival = val;
    return n;
}

Node *node_char(int val, SourceLoc loc) {
    Node *n = node_alloc(N_CHAR_LIT, loc);
    if (n) n->ival = val;
    return n;
}

Node *node_ident(const char *name, SourceLoc loc) {
    Node *n = node_alloc(N_IDENT, loc);
    if (n) { strncpy(n->name, name, 255); n->name[255] = 0; }
    return n;
}

Node *node_binary(TokenType op, Node *l, Node *r, SourceLoc loc) {
    Node *n = node_alloc(N_BINARY, loc);
    if (n) { n->op = op; n->left = l; n->right = r; }
    return n;
}

Node *node_unary(TokenType op, Node *o, SourceLoc loc) {
    Node *n = node_alloc(N_UNARY, loc);
    if (n) { n->op = op; n->left = o; }
    return n;
}

Node *node_assign(TokenType op, Node *t, Node *v, SourceLoc loc) {
    Node *n = node_alloc(N_ASSIGN, loc);
    if (n) { n->op = op; n->left = t; n->right = v; }
    return n;
}

static void indent(int d) { for (int i = 0; i < d; i++) printf("  "); }

static const char *op_str(TokenType t) {
    switch (t) {
    case T_PLUS: return "+"; case T_MINUS: return "-";
    case T_STAR: return "*"; case T_SLASH: return "/";
    case T_PERCENT: return "%"; case T_AMPERSAND: return "&";
    case T_PIPE: return "|"; case T_CARET: return "^";
    case T_TILDE: return "~"; case T_LSHIFT: return "<<";
    case T_RSHIFT: return ">>";
    case T_EQ: return "=="; case T_NE: return "!=";
    case T_LT: return "<"; case T_GT: return ">";
    case T_LE: return "<="; case T_GE: return ">=";
    case T_AND: return "&&"; case T_OR: return "||";
    case T_NOT: return "!"; case T_INC: return "++";
    case T_DEC: return "--";
    case T_ASSIGN: return "="; case T_ADD_ASSIGN: return "+=";
    case T_SUB_ASSIGN: return "-="; case T_MUL_ASSIGN: return "*=";
    case T_DIV_ASSIGN: return "/="; case T_MOD_ASSIGN: return "%=";
    case T_AND_ASSIGN: return "&="; case T_OR_ASSIGN: return "|=";
    case T_XOR_ASSIGN: return "^="; case T_LSHIFT_ASSIGN: return "<<=";
    case T_RSHIFT_ASSIGN: return ">>=";
    default: return "?";
    }
}

void ast_dump(Node *n, int depth) {
    while (n) {
        indent(depth);
        switch (n->type) {
        case N_PROGRAM:    printf("PROGRAM\n"); ast_dump(n->left, depth+1); break;
        case N_VAR_DECL:   printf("VAR_DECL: int %s = %d\n", n->name, n->ival); break;
        case N_EXPR_STMT:  printf("EXPR_STMT\n"); ast_dump(n->left, depth+1); break;
        case N_INT_LIT:    printf("INT(%d)\n", n->ival); break;
        case N_CHAR_LIT:   printf("CHAR(%d)\n", n->ival); break;
        case N_IDENT:      printf("IDENT(%s)\n", n->name); break;
        case N_BINARY:     printf("BIN(%s)\n", op_str(n->op)); ast_dump(n->left, depth+1); ast_dump(n->right, depth+1); break;
        case N_UNARY:      printf("UNARY(%s)\n", op_str(n->op)); ast_dump(n->left, depth+1); break;
        case N_POSTFIX:    printf("POST(%s)\n", op_str(n->op)); ast_dump(n->left, depth+1); break;
        case N_ASSIGN:     printf("ASSIGN(%s)\n", op_str(n->op)); ast_dump(n->left, depth+1); ast_dump(n->right, depth+1); break;
        case N_TERNARY:    printf("TERNARY\n"); break;
        default:           printf("NODE(%d)\n", n->type); break;
        }
        n = n->next;
    }
}
