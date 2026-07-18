#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AstNode *ast_alloc(AstType type, int line, int col) {
    AstNode *n = (AstNode*)calloc(1, sizeof(AstNode));
    if (!n) return NULL;
    n->type = type;
    n->line = line;
    n->col = col;
    return n;
}

void ast_free(AstNode *node) {
    if (!node) return;
    switch (node->type) {
    case AST_PROGRAM:
    case AST_BLOCK:
        for (int i = 0; i < node->data.block.count; i++)
            ast_free(node->data.block.stmts[i]);
        free(node->data.block.stmts); break;
    case AST_FUNCTION_DEF:
        ast_free(node->data.func.type);
        ast_free(node->data.func.body);
        for (int i = 0; i < node->data.func.pcount; i++)
            ast_free(node->data.func.params[i]);
        free(node->data.func.params); break;
    case AST_VAR_DECL:
        ast_free(node->data.var.type);
        ast_free(node->data.var.init); break;
    case AST_IF:
        ast_free(node->data.if_stmt.cond);
        ast_free(node->data.if_stmt.then);
        ast_free(node->data.if_stmt.els); break;
    case AST_FOR:
        ast_free(node->data.for_stmt.init);
        ast_free(node->data.for_stmt.cond);
        ast_free(node->data.for_stmt.step);
        ast_free(node->data.for_stmt.body); break;
    case AST_WHILE:
        ast_free(node->data.while_stmt.cond);
        ast_free(node->data.while_stmt.body); break;
    case AST_RETURN:
        ast_free(node->data.ret.val); break;
    case AST_BINARY_OP:
        ast_free(node->data.bin.left);
        ast_free(node->data.bin.right); break;
    case AST_UNARY_OP:
        ast_free(node->data.un.operand); break;
    case AST_FUNC_CALL:
        for (int i = 0; i < node->data.call.acount; i++)
            ast_free(node->data.call.args[i]);
        free(node->data.call.args); break;
    default: break;
    }
    free(node);
}

static const char *ast_type_name(AstType t) {
    switch (t) {
    case AST_PROGRAM:      return "Program";
    case AST_FUNCTION_DEF: return "FunctionDef";
    case AST_VAR_DECL:     return "VarDecl";
    case AST_IF:           return "If";
    case AST_FOR:          return "For";
    case AST_WHILE:        return "While";
    case AST_RETURN:       return "Return";
    case AST_BLOCK:        return "Block";
    case AST_BINARY_OP:    return "BinaryOp";
    case AST_UNARY_OP:     return "UnaryOp";
    case AST_FUNC_CALL:    return "FuncCall";
    case AST_LITERAL_INT:  return "Int";
    case AST_LITERAL_FLOAT:return "Float";
    case AST_STRING:       return "String";
    case AST_IDENTIFIER:   return "Ident";
    default:               return "?";
    }
}

static const char *op_symbol(int op) {
    switch (op) {
    case TOKEN_PLUS:   return "+";   case TOKEN_MINUS:  return "-";
    case TOKEN_STAR:   return "*";   case TOKEN_SLASH:  return "/";
    case TOKEN_PERCENT:return "%";   case TOKEN_EQEQ:   return "==";
    case TOKEN_BANGEQ: return "!=";  case TOKEN_LT:     return "<";
    case TOKEN_GT:     return ">";   case TOKEN_LE:     return "<=";
    case TOKEN_GE:     return ">=";  case TOKEN_ANDAND: return "&&";
    case TOKEN_OROR:   return "||";  case TOKEN_AND:    return "&";
    case TOKEN_OR:     return "|";   case TOKEN_CARET:  return "^";
    case TOKEN_LSHIFT: return "<<";  case TOKEN_RSHIFT: return ">>";
    case TOKEN_EQ:     return "=";
    /* Unary (stored as TokenType not raw char) */
    case TOKEN_BANG:   return "!";
    default:  return "?";
    }
}

void ast_dump(AstNode *node, FILE *out, int depth) {
    if (!node) { fprintf(out, "%*s(null)\n", depth*2, ""); return; }
    fprintf(out, "%*s%s", depth*2, "", ast_type_name(node->type));
    if (node->line || node->col)
        fprintf(out, " [%d:%d]", node->line, node->col);
    switch (node->type) {
    case AST_LITERAL_INT:
        fprintf(out, " %ld", node->data.int_val); break;
    case AST_LITERAL_FLOAT:
        fprintf(out, " %g", node->data.float_val); break;
    case AST_STRING:
        fprintf(out, " \"%.*s\"", (int)node->data.str_val[0], node->data.str_val); break;
    case AST_IDENTIFIER:
        if (node->data.id) { fprintf(out, " %s", node->data.id); }
        break;
    case AST_FUNC_CALL:
        fprintf(out, " %s", node->data.call.name ? node->data.call.name : "?"); break;
    case AST_BINARY_OP:
        fprintf(out, " %s", op_symbol(node->data.bin.op)); break;
    case AST_UNARY_OP:
        fprintf(out, " %s", op_symbol(node->data.un.op)); break;
    case AST_VAR_DECL:
        fprintf(out, " %s", node->data.var.name ? node->data.var.name : "?"); break;
    case AST_FUNCTION_DEF:
        fprintf(out, " %s", node->data.func.name ? node->data.func.name : "?"); break;
    default: break;
    }
    fprintf(out, "\n");

    switch (node->type) {
    case AST_PROGRAM:
    case AST_BLOCK:
        for (int i = 0; i < node->data.block.count; i++)
            ast_dump(node->data.block.stmts[i], out, depth + 1);
        break;
    case AST_FUNCTION_DEF:
        ast_dump(node->data.func.type, out, depth + 1);
        fprintf(out, "%*sParams:\n", (depth+1)*2, "");
        for (int i = 0; i < node->data.func.pcount; i++)
            ast_dump(node->data.func.params[i], out, depth + 2);
        ast_dump(node->data.func.body, out, depth + 1);
        break;
    case AST_VAR_DECL:
        ast_dump(node->data.var.type, out, depth + 1);
        if (node->data.var.init)
            ast_dump(node->data.var.init, out, depth + 1);
        break;
    case AST_IF:
        ast_dump(node->data.if_stmt.cond, out, depth + 1);
        ast_dump(node->data.if_stmt.then, out, depth + 1);
        if (node->data.if_stmt.els)
            ast_dump(node->data.if_stmt.els, out, depth + 1);
        break;
    case AST_FOR:
        ast_dump(node->data.for_stmt.init, out, depth + 1);
        ast_dump(node->data.for_stmt.cond, out, depth + 1);
        ast_dump(node->data.for_stmt.step, out, depth + 1);
        ast_dump(node->data.for_stmt.body, out, depth + 1);
        break;
    case AST_WHILE:
        ast_dump(node->data.while_stmt.cond, out, depth + 1);
        ast_dump(node->data.while_stmt.body, out, depth + 1);
        break;
    case AST_RETURN:
        if (node->data.ret.val)
            ast_dump(node->data.ret.val, out, depth + 1);
        break;
    case AST_BINARY_OP:
        ast_dump(node->data.bin.left, out, depth + 1);
        ast_dump(node->data.bin.right, out, depth + 1);
        break;
    case AST_UNARY_OP:
        ast_dump(node->data.un.operand, out, depth + 1);
        break;
    case AST_FUNC_CALL:
        for (int i = 0; i < node->data.call.acount; i++)
            ast_dump(node->data.call.args[i], out, depth + 1);
        break;
    default: break;
    }
}
