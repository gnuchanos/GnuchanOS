#include "codegen.h"
#include "../type/type.h"
#include <string.h>

static int is_expr_stmt(AstType t) {
    return t == AST_FUNC_CALL || t == AST_BINARY_OP || t == AST_UNARY_OP ||
           t == AST_IDENTIFIER || t == AST_LITERAL_INT ||
           t == AST_LITERAL_FLOAT || t == AST_STRING;
}

static void emit_node(AstNode *n, FILE *out, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) fputc(' ', out);
    switch (n->type) {
    case AST_PROGRAM:
        for (int i = 0; i < n->data.program.count; i++)
            emit_node(n->data.program.stmts[i], out, 0);
        break;
    case AST_FUNCTION_DEF: {
        const char *cname = n->data.func.type ? type_c_name(ast_to_type(n->data.func.type)) : "int";
        fprintf(out, "%s %s(", cname, n->data.func.name);
        for (int i = 0; i < n->data.func.pcount; i++) {
            if (i > 0) fprintf(out, ", ");
            emit_node(n->data.func.params[i]->data.var.type, out, 0);
            fprintf(out, " %s", n->data.func.params[i]->data.var.name);
        }
        fprintf(out, ")\n");
        emit_node(n->data.func.body, out, 0);
        break;
    }
    case AST_BLOCK:
        fprintf(out, "{\n");
        for (int i = 0; i < n->data.block.count; i++) {
            emit_node(n->data.block.stmts[i], out, depth + 1);
            if (is_expr_stmt(n->data.block.stmts[i]->type))
                fprintf(out, ";\n");
        }
        for (int i = 0; i < depth; i++) fputc(' ', out);
        fprintf(out, "}\n");
        break;
    case AST_VAR_DECL: {
        const char *cname = n->data.var.type ? type_c_name(ast_to_type(n->data.var.type)) : "int";
        /* If char var initialized with string literal, emit as array */
        int is_char_array = 0;
        if (n->data.var.init && n->data.var.init->type == AST_STRING &&
            cname && strcmp(cname, "char") == 0) {
            is_char_array = 1;
        }
        if (is_char_array)
            fprintf(out, "%s %s[]", cname, n->data.var.name);
        else
            fprintf(out, "%s %s", cname, n->data.var.name);
        if (n->data.var.init) {
            fprintf(out, " = ");
            emit_node(n->data.var.init, out, 0);
        }
        fprintf(out, ";\n");
        break;
    }
    case AST_RETURN:
        fprintf(out, "return ");
        emit_node(n->data.ret.val, out, 0);
        fprintf(out, ";\n");
        break;
    case AST_IF:
        fprintf(out, "if (");
        emit_node(n->data.if_stmt.cond, out, 0);
        fprintf(out, ")\n");
        emit_node(n->data.if_stmt.then, out, depth);
        if (n->data.if_stmt.els) {
            for (int i = 0; i < depth; i++) fputc(' ', out);
            fprintf(out, "else\n");
            emit_node(n->data.if_stmt.els, out, depth);
        }
        break;
    case AST_WHILE:
        fprintf(out, "while (");
        emit_node(n->data.while_stmt.cond, out, 0);
        fprintf(out, ")\n");
        emit_node(n->data.while_stmt.body, out, depth);
        break;
    case AST_FOR:
        fprintf(out, "for (");
        emit_node(n->data.for_stmt.init, out, 0);
        fprintf(out, "; ");
        emit_node(n->data.for_stmt.cond, out, 0);
        fprintf(out, "; ");
        emit_node(n->data.for_stmt.step, out, 0);
        fprintf(out, ")\n");
        emit_node(n->data.for_stmt.body, out, depth);
        break;
    case AST_BINARY_OP: {
        fprintf(out, "(");
        emit_node(n->data.bin.left, out, 0);
        int op = n->data.bin.op;
        const char *op_str = "?";
        if (op == TOKEN_PLUS) op_str = "+";
        else if (op == TOKEN_MINUS) op_str = "-";
        else if (op == TOKEN_STAR) op_str = "*";
        else if (op == TOKEN_SLASH) op_str = "/";
        else if (op == TOKEN_PERCENT) op_str = "%";
        else if (op == TOKEN_EQEQ) op_str = "==";
        else if (op == TOKEN_BANGEQ) op_str = "!=";
        else if (op == TOKEN_LT) op_str = "<";
        else if (op == TOKEN_GT) op_str = ">";
        else if (op == TOKEN_LE) op_str = "<=";
        else if (op == TOKEN_GE) op_str = ">=";
        else if (op == TOKEN_ANDAND) op_str = "&&";
        else if (op == TOKEN_OROR) op_str = "||";
        else if (op == TOKEN_AND) op_str = "&";
        else if (op == TOKEN_OR) op_str = "|";
        else if (op == TOKEN_CARET) op_str = "^";
        else if (op == TOKEN_LSHIFT) op_str = "<<";
        else if (op == TOKEN_RSHIFT) op_str = ">>";
        else if (op == TOKEN_EQ) op_str = "=";
        fprintf(out, " %s ", op_str);
        emit_node(n->data.bin.right, out, 0);
        fprintf(out, ")");
        break;
    }
    case AST_UNARY_OP: {
        int op = n->data.un.op;
        if (op == TOKEN_MINUS) fprintf(out, "(-");
        else if (op == TOKEN_BANG) fprintf(out, "(!");
        else if (op == TOKEN_TILDE) fprintf(out, "(~");
        else fprintf(out, "(");
        emit_node(n->data.un.operand, out, 0);
        fprintf(out, ")");
        break;
    }
    case AST_LITERAL_INT:
        fprintf(out, "%ld", n->data.int_val);
        break;
    case AST_LITERAL_FLOAT:
        fprintf(out, "%f", n->data.float_val);
        break;
    case AST_STRING:
        fprintf(out, "\"%.*s\"", (int)n->data.str_val[0], n->data.str_val);
        break;
    case AST_IDENTIFIER:
        if (n->data.id)
            fprintf(out, "%s", n->data.id);
        else
            fprintf(out, "int");
        break;
    case AST_FUNC_CALL:
        fprintf(out, "%s(", n->data.call.name);
        for (int i = 0; i < n->data.call.acount; i++) {
            if (i > 0) fprintf(out, ", ");
            emit_node(n->data.call.args[i], out, 0);
        }
        fprintf(out, ")");
        break;
    default:
        fprintf(out, "/* unknown node %d */", n->type);
        break;
    }
}

void codegen_emit(AstNode *ast, FILE *out) {
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <stdint.h>\n");
    fprintf(out, "#include <stdbool.h>\n\n");
    emit_node(ast, out, 0);
}
