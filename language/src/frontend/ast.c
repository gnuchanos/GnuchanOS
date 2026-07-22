#include "parse_directive.h"
#include <stdlib.h>

void ast_free(AstNode *n) {
    if (!n) return;
    ast_free(n->left);
    ast_free(n->right);
    ast_free(n->next);
    if (n->value) free((void*)n->value);
    free(n);
}
