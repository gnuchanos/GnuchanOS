#include "codegen_debug.h"
#include "codegen.h"
#include "defines.h"
#include "colors.h"
#include <stdio.h>
#include <string.h>

const char *codegen_resolve(const char *name) {
    if (!name) return "(null)";
    const char *v = defines_get(name);
    return v ? v : name;
}

static void print_val(AstNode *arg) {
    if (!arg) return;
    if (arg->kind == NODE_STRING) {
        size_t len = arg->len;
        const char *s = arg->value;
        if (len >= 2 && s[0] == '"') { s++; len -= 2; }
        fprintf(g_codegen_out, "%s%.*s%s", CLR_DIM, (int)len, s, CLR_RESET);
    } else if (arg->kind == NODE_IDENT) {
        const char *rv = codegen_resolve(arg->value);
        size_t l = strlen(rv);
        if (l >= 2 && rv[0] == '"') { rv++; l -= 2; }
        fprintf(g_codegen_out, "%s%.*s%s", CLR_CYAN, (int)l, rv, CLR_RESET);
    } else if (arg->kind == NODE_NUMBER) {
        fprintf(g_codegen_out, "%s%.*s%s", CLR_MAGENTA, (int)arg->len, arg->value, CLR_RESET);
    }
}

void codegen_debug_emit(AstNode *prog) {
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_DEBUG) {
            AstNode *arg = n->left;
            while (arg) { print_val(arg); arg = arg->next; }
            fprintf(g_codegen_out, "\n");
        }
        n = n->next;
    }
}
