#include "codegen_ast.h"
#include "codegen.h"
#include "colors.h"
#include <stdio.h>

#define V(v) ((v) ? (v) : "(null)")

static void emit_node(AstNode *n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) fprintf(g_codegen_out, "  ");
    switch (n->kind) {
        case NODE_PROGRAM:       fprintf(g_codegen_out, CLR_MAGENTA "PROGRAM" CLR_RESET "\n"); break;
        case NODE_INCLUDE:       fprintf(g_codegen_out, CLR_CYAN "INCLUDE:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_LIB:           fprintf(g_codegen_out, CLR_CYAN "LIB:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_EXTERN:        fprintf(g_codegen_out, CLR_CYAN "EXTERN:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_DEFINE:        fprintf(g_codegen_out, CLR_CYAN "DEFINE:" CLR_RESET " %s = %s%s%s\n",
                                 V(n->value), CLR_MAGENTA, V(n->left ? n->left->value : NULL), CLR_RESET); break;
        case NODE_DEBUG:         fprintf(g_codegen_out, CLR_CYAN "DEBUG:" CLR_RESET "\n"); break;
        case NODE_PRAGMA:        fprintf(g_codegen_out, CLR_CYAN "PRAGMA:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_EXTERN_C_BLOCK:fprintf(g_codegen_out, CLR_YELLOW "EXTERN \"c\" BLOCK:" CLR_RESET "\n"); break;
        case NODE_IFDEF:         fprintf(g_codegen_out, CLR_PURPLE "IFDEF:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_IFNDEF:        fprintf(g_codegen_out, CLR_PURPLE "IFNDEF:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_IF:            fprintf(g_codegen_out, CLR_PURPLE "IF:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_ELIF:          fprintf(g_codegen_out, CLR_PURPLE "ELIF:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_ELSE:          fprintf(g_codegen_out, CLR_PURPLE "ELSE" CLR_RESET "\n"); break;
        case NODE_ENDIF:         fprintf(g_codegen_out, CLR_PURPLE "ENDIF" CLR_RESET "\n"); break;
        case NODE_ERROR:         fprintf(g_codegen_out, CLR_RED "ERROR:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_MESSAGE:       fprintf(g_codegen_out, CLR_YELLOW "MESSAGE:" CLR_RESET " %s\n", V(n->left ? n->left->value : NULL)); break;
        case NODE_IDENT:         fprintf(g_codegen_out, CLR_CYAN "IDENT:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_STRING:        fprintf(g_codegen_out, CLR_DIM "STRING:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_NUMBER:        fprintf(g_codegen_out, CLR_MAGENTA "NUMBER:" CLR_RESET " %s\n", V(n->value)); break;
        case NODE_RAW:           fprintf(g_codegen_out, CLR_DIM "RAW:" CLR_RESET " %.*s", (int)n->len, V(n->value)); break;
        case NODE_UNDEF:         fprintf(g_codegen_out, CLR_RED "UNDEF:" CLR_RESET " %s\n", V(n->value)); break;
        default:                 fprintf(g_codegen_out, CLR_DIM "?" CLR_RESET "\n"); break;
    }
    if (n->left)  emit_node(n->left, depth + 1);
    if (n->right) emit_node(n->right, depth + 1);
    if (n->next)  emit_node(n->next, depth);
}

void codegen_ast_emit(AstNode *prog) {
    emit_node(prog, 0);
}
