#ifndef GCL_AST_H
#define GCL_AST_H

#include "gcl.h"

typedef struct Node {
    NodeType type;
    char  name[256];
    int   ival;
    int   size;         /* array size for var_decl */
    SourceLoc loc;
    TokenType op;       /* operator for N_BINARY, N_UNARY, N_POSTFIX, N_ASSIGN */
    struct Node *left;  /* or target */
    struct Node *right; /* or value/index */
    struct Node *cond;  /* ternary */
    struct Node *next;
} Node;

Node *node_alloc(NodeType t, SourceLoc loc);
Node *node_int(int val, SourceLoc loc);
Node *node_char(int val, SourceLoc loc);
Node *node_ident(const char *name, SourceLoc loc);
Node *node_binary(TokenType op, Node *l, Node *r, SourceLoc loc);
Node *node_unary(TokenType op, Node *o, SourceLoc loc);
Node *node_assign(TokenType op, Node *t, Node *v, SourceLoc loc);
void  ast_dump(Node *n, int depth);

#endif
