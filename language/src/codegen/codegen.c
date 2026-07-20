#include "codegen.h"
#include "ast.h"
#include "tokens.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// GCL Codegen — AST to C transpiler

static void emit_indent(GclCodegen *cg) {
    for (int i = 0; i < cg->indent_level; i++) fprintf(cg->out, "    ");
}
static void emit_type_str(GclCodegen *cg, GclType *type);
static void emit_expr(GclCodegen *cg, GclAstNode *node);
static void emit_stmt(GclCodegen *cg, GclAstNode *node);
static void emit_decl(GclCodegen *cg, GclAstNode *node);

// Emit type to C string (just the base type, no array brackets)
static void emit_type_str(GclCodegen *cg, GclType *type) {
    if (!type) { fprintf(cg->out, "int"); return; }
    if (type->is_const) fprintf(cg->out, "const ");
    if (type->is_static) fprintf(cg->out, "static ");
    switch (type->category) {
    case TYPE_PRIMITIVE: {
        const char *n = type->name;
        if (!n) { fprintf(cg->out, "int"); return; }
        if (strcmp(n, "int") == 0) {
            if (type->is_unsigned) fprintf(cg->out, "unsigned int");
            else fprintf(cg->out, "int");
        } else if (strcmp(n, "char") == 0) fprintf(cg->out, "char");
        else if (strcmp(n, "short") == 0) {
            if (type->is_unsigned) fprintf(cg->out, "unsigned short");
            else fprintf(cg->out, "short");
        } else if (strcmp(n, "long") == 0) {
            if (type->is_unsigned) fprintf(cg->out, "unsigned long");
            else fprintf(cg->out, "long");
        } else if (strcmp(n, "long long") == 0) {
            if (type->is_unsigned) fprintf(cg->out, "unsigned long long");
            else fprintf(cg->out, "long long");
        } else if (strcmp(n, "float") == 0) {
            fprintf(cg->out, "float");
        } else if (strcmp(n, "double") == 0) {
            fprintf(cg->out, "double");
        } else if (strcmp(n, "long double") == 0) {
            fprintf(cg->out, "long double");
        } else if (strcmp(n, "void") == 0) fprintf(cg->out, "void");
        else if (strcmp(n, "bool") == 0) fprintf(cg->out, "int");
        else if (strcmp(n, "int8") == 0 || strcmp(n, "int8_t") == 0) fprintf(cg->out, "int8_t");
        else if (strcmp(n, "int16") == 0 || strcmp(n, "int16_t") == 0) fprintf(cg->out, "int16_t");
        else if (strcmp(n, "int32") == 0 || strcmp(n, "int32_t") == 0) fprintf(cg->out, "int32_t");
        else if (strcmp(n, "int64") == 0 || strcmp(n, "int64_t") == 0) fprintf(cg->out, "int64_t");
        else if (strcmp(n, "int128") == 0) fprintf(cg->out, "__int128");
        else if (strcmp(n, "uint8") == 0 || strcmp(n, "uint8_t") == 0) fprintf(cg->out, "uint8_t");
        else if (strcmp(n, "uint16") == 0 || strcmp(n, "uint16_t") == 0) fprintf(cg->out, "uint16_t");
        else if (strcmp(n, "uint32") == 0 || strcmp(n, "uint32_t") == 0) fprintf(cg->out, "uint32_t");
        else if (strcmp(n, "uint64") == 0 || strcmp(n, "uint64_t") == 0) fprintf(cg->out, "uint64_t");
        else fprintf(cg->out, "%s", n);
        break;
    }
    case TYPE_POINTER:
        emit_type_str(cg, type->base_type);
        fprintf(cg->out, "*");
        break;
    case TYPE_ARRAY:
        // For param types: char *argv[] → array(base=pointer(char))
        // We need to emit "char *" then append "[]" after the name
        emit_type_str(cg, type->base_type);
        break;
    case TYPE_STRUCT:
        fprintf(cg->out, "struct %s", type->name ? type->name : "");
        break;
    case TYPE_ENUM:
        fprintf(cg->out, "enum %s", type->name ? type->name : "");
        break;
    case TYPE_UNION:
        fprintf(cg->out, "union %s", type->name ? type->name : "");
        break;
    default:
        fprintf(cg->out, "int");
        break;
    }
}

// Emit array brackets for a type chain (used for both params and var decls)
static void emit_array_brackets(GclCodegen *cg, GclType *type) {
    // The type chain is built inner-to-outer by the parser
    // e.g., char names[3][32] → array(array(char, 3), 32)
    // Walking the chain gives [32][3], we need [3][32]
    // So we collect sizes into a stack-like array and emit in reverse.
    if (!type) return;
    int sizes[32];
    int count = 0;
    GclType *arr = type;
    while (arr && arr->category == TYPE_ARRAY && count < 32) {
        sizes[count++] = arr->array_size;
        arr = arr->base_type;
    }
    // Emit in reverse (outermost-to-innermost → C declaration order)
    // In C, only the outermost dimension can be unspecified.
    // Inner unspecified dimensions get a default size (32 for strings).
    for (int i = count - 1; i >= 0; i--) {
        int sz = sizes[i];
        if (sz > 0) {
            fprintf(cg->out, "[%d]", sz);
        } else if (i == count - 1) {
            // Outermost dimension — can be auto-sized
            fprintf(cg->out, "[]");
        } else {
            // Inner dimension — must have a size in C
            fprintf(cg->out, "[32]");
        }
    }
}

static const char *token_to_c_op(GclTokenType op) {
    switch (op) {
    case TOK_PLUS: return "+"; case TOK_MINUS: return "-"; case TOK_STAR: return "*";
    case TOK_SLASH: return "/"; case TOK_PERCENT: return "%"; case TOK_AMPERSAND: return "&";
    case TOK_PIPE: return "|"; case TOK_CARET: return "^"; case TOK_TILDE: return "~";
    case TOK_LSHIFT: return "<<"; case TOK_RSHIFT: return ">>";
    case TOK_AND: return "&&"; case TOK_OR: return "||"; case TOK_BANG: return "!";
    case TOK_GT: return ">"; case TOK_LT: return "<"; case TOK_GE: return ">="; case TOK_LE: return "<=";
    case TOK_EQ: return "=="; case TOK_NE: return "!="; case TOK_ASSIGN: return "=";
    case TOK_PLUS_EQ: return "+="; case TOK_MINUS_EQ: return "-="; case TOK_STAR_EQ: return "*=";
    case TOK_SLASH_EQ: return "/="; case TOK_PERCENT_EQ: return "%="; case TOK_AMP_EQ: return "&=";
    case TOK_PIPE_EQ: return "|="; case TOK_CARET_EQ: return "^=";
    case TOK_LSHIFT_EQ: return "<<="; case TOK_RSHIFT_EQ: return ">>=";
    case TOK_INC: return "++"; case TOK_DEC: return "--";
    default: return "?";
    }
}

static void emit_expr(GclCodegen *cg, GclAstNode *node) {
    if (!node) { fprintf(cg->out, "/*null*/"); return; }
    switch (node->type) {
    case AST_INT_LITERAL:
        fprintf(cg->out, "%lld", node->data.int_lit.int_val);
        break;
    case AST_FLOAT_LITERAL:
        if (node->data.float_lit.is_long_double)
            fprintf(cg->out, "%.16gL", node->data.float_lit.float_val);
        else
            fprintf(cg->out, "%.16g", node->data.float_lit.float_val);
        break;
    case AST_CHAR_LITERAL: {
        int c = node->data.char_lit.code;
        switch (c) {
        case '\n': fprintf(cg->out, "'\\n'"); break;
        case '\t': fprintf(cg->out, "'\\t'"); break;
        case '\r': fprintf(cg->out, "'\\r'"); break;
        case '\\': fprintf(cg->out, "'\\\\'"); break;
        case '\'': fprintf(cg->out, "'\\''"); break;
        case '\0': fprintf(cg->out, "'\\0'"); break;
        default:
            if (c >= 32 && c < 127) fprintf(cg->out, "'%c'", c);
            else fprintf(cg->out, "0x%x", c);
            break;
        }
        break;
    }
    case AST_STRING_LITERAL: {
        fprintf(cg->out, "\"");
        const char *s = node->data.string_lit.str;
        if (s) {
            for (const char *c = s; *c; c++) {
                switch (*c) {
                case '\n': fprintf(cg->out, "\\n"); break;
                case '\t': fprintf(cg->out, "\\t"); break;
                case '\r': fprintf(cg->out, "\\r"); break;
                case '\\': fprintf(cg->out, "\\\\"); break;
                case '"':  fprintf(cg->out, "\\\""); break;
                default:   fputc(*c, cg->out); break;
                }
            }
        }
        fprintf(cg->out, "\"");
        break;
    }
    case AST_BOOL_LITERAL:
        fprintf(cg->out, "%d", node->data.int_lit.int_val ? 1 : 0);
        break;
    case AST_IDENTIFIER:
        fprintf(cg->out, "%s", node->data.ident.name);
        break;
    case AST_BINARY:
        fprintf(cg->out, "(");
        emit_expr(cg, node->data.binary.lhs);
        fprintf(cg->out, " %s ", token_to_c_op(node->data.binary.op));
        emit_expr(cg, node->data.binary.rhs);
        fprintf(cg->out, ")");
        break;
    case AST_UNARY: {
        GclTokenType op_type = node->data.unary.op;
        if (op_type == TOK_AMPERSAND) {
            fprintf(cg->out, "&");
            emit_expr(cg, node->data.unary.operand);
        } else if (op_type == TOK_STAR) {
            fprintf(cg->out, "(*");
            emit_expr(cg, node->data.unary.operand);
            fprintf(cg->out, ")");
        } else if (op_type == TOK_INC) {
            fprintf(cg->out, "++");
            emit_expr(cg, node->data.unary.operand);
        } else if (op_type == TOK_DEC) {
            fprintf(cg->out, "--");
            emit_expr(cg, node->data.unary.operand);
        } else {
            fprintf(cg->out, "%s", token_to_c_op(op_type));
            emit_expr(cg, node->data.unary.operand);
        }
        break;
    }
    case AST_TERNARY:
        fprintf(cg->out, "(");
        emit_expr(cg, node->data.ternary.cond);
        fprintf(cg->out, " ? ");
        emit_expr(cg, node->data.ternary.true_expr);
        fprintf(cg->out, " : ");
        emit_expr(cg, node->data.ternary.false_expr);
        fprintf(cg->out, ")");
        break;
    case AST_CAST:
        fprintf(cg->out, "((");
        emit_type_str(cg, node->data.cast.target_type);
        fprintf(cg->out, ")");
        emit_expr(cg, node->data.cast.expr);
        fprintf(cg->out, ")");
        break;
    case AST_SIZEOF:
        fprintf(cg->out, "sizeof(");
        if (node->data.sizeof_expr.target_type)
            emit_type_str(cg, node->data.sizeof_expr.target_type);
        else
            emit_expr(cg, node->data.sizeof_expr.expr);
        fprintf(cg->out, ")");
        break;
    case AST_CALL:
        emit_expr(cg, node->data.call.callee);
        fprintf(cg->out, "(");
        if (node->data.call.args) {
            for (GclAstNode *a = node->data.call.args->head; a; a = a->next) {
                if (a != node->data.call.args->head) fprintf(cg->out, ", ");
                emit_expr(cg, a);
            }
        }
        fprintf(cg->out, ")");
        break;
    case AST_ASSIGN:
        emit_expr(cg, node->data.assign.lhs);
        fprintf(cg->out, " %s ", token_to_c_op(node->data.assign.op));
        emit_expr(cg, node->data.assign.rhs);
        break;
    case AST_SUBSCRIPT:
        emit_expr(cg, node->data.subscript.base);
        fprintf(cg->out, "[");
        emit_expr(cg, node->data.subscript.index);
        fprintf(cg->out, "]");
        break;
    case AST_MEMBER:
        emit_expr(cg, node->data.member.base);
        fprintf(cg->out, ".%s", node->data.member.member);
        break;
    case AST_MEMBER_PTR:
        emit_expr(cg, node->data.member.base);
        fprintf(cg->out, "->%s", node->data.member.member);
        break;
    case AST_ARRAY_INIT:
        fprintf(cg->out, "{ ");
        if (node->data.array_init.elements) {
            for (GclAstNode *e = node->data.array_init.elements->head; e; e = e->next) {
                if (e != node->data.array_init.elements->head) fprintf(cg->out, ", ");
                emit_expr(cg, e);
            }
        }
        fprintf(cg->out, " }");
        break;
    default:
        fprintf(cg->out, "/*unhandled_expr*/");
        break;
    }
}

static void emit_stmt(GclCodegen *cg, GclAstNode *node) {
    if (!node) return;
    switch (node->type) {
    case AST_COMPOUND:
        if (node->data.compound.statements) {
            for (GclAstNode *s = node->data.compound.statements->head; s; s = s->next)
                emit_decl(cg, s);
        }
        break;
    case AST_IF:
        emit_indent(cg);
        fprintf(cg->out, "if (");
        emit_expr(cg, node->data.if_stmt.cond);
        fprintf(cg->out, ")");
        if (node->data.if_stmt.then_branch) {
            if (node->data.if_stmt.then_branch->type == AST_COMPOUND) {
                fprintf(cg->out, " {\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.if_stmt.then_branch);
                cg->indent_level--;
                emit_indent(cg);
                fprintf(cg->out, "}");
            } else {
                fprintf(cg->out, "\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.if_stmt.then_branch);
                cg->indent_level--;
            }
        } else {
            fprintf(cg->out, " {}");
        }
        if (node->data.if_stmt.else_branch) {
            if (node->data.if_stmt.then_branch && node->data.if_stmt.then_branch->type == AST_COMPOUND)
                fprintf(cg->out, " ");
            fprintf(cg->out, "else");
            if (node->data.if_stmt.else_branch->type == AST_COMPOUND) {
                fprintf(cg->out, " {\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.if_stmt.else_branch);
                cg->indent_level--;
                emit_indent(cg);
                fprintf(cg->out, "}");
            } else if (node->data.if_stmt.else_branch->type == AST_IF) {
                fprintf(cg->out, " ");
                emit_stmt(cg, node->data.if_stmt.else_branch);
            } else {
                fprintf(cg->out, "\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.if_stmt.else_branch);
                cg->indent_level--;
            }
        }
        fprintf(cg->out, "\n");
        break;
    case AST_FOR:
        emit_indent(cg);
        fprintf(cg->out, "for (");
        if (node->data.for_stmt.init) {
            if (node->data.for_stmt.init->type == AST_VAR_DECL) {
                GclAstNode *init = node->data.for_stmt.init;
                emit_type_str(cg, init->data.var_decl.var_type);
                fprintf(cg->out, " %s", init->data.var_decl.name);
                if (init->data.var_decl.init) {
                    fprintf(cg->out, " = ");
                    emit_expr(cg, init->data.var_decl.init);
                }
            } else {
                emit_expr(cg, node->data.for_stmt.init);
            }
        }
        fprintf(cg->out, "; ");
        if (node->data.for_stmt.cond) emit_expr(cg, node->data.for_stmt.cond);
        fprintf(cg->out, "; ");
        if (node->data.for_stmt.incr) emit_expr(cg, node->data.for_stmt.incr);
        fprintf(cg->out, ")");
        if (node->data.for_stmt.body) {
            if (node->data.for_stmt.body->type == AST_COMPOUND) {
                fprintf(cg->out, " {\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.for_stmt.body);
                cg->indent_level--;
                emit_indent(cg);
                fprintf(cg->out, "}\n");
            } else {
                fprintf(cg->out, "\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.for_stmt.body);
                cg->indent_level--;
            }
        } else {
            fprintf(cg->out, ";\n");
        }
        break;
    case AST_WHILE:
        emit_indent(cg);
        fprintf(cg->out, "while (");
        emit_expr(cg, node->data.while_stmt.cond);
        fprintf(cg->out, ")");
        if (node->data.while_stmt.body) {
            if (node->data.while_stmt.body->type == AST_COMPOUND) {
                fprintf(cg->out, " {\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.while_stmt.body);
                cg->indent_level--;
                emit_indent(cg);
                fprintf(cg->out, "}\n");
            } else {
                fprintf(cg->out, "\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.while_stmt.body);
                cg->indent_level--;
            }
        } else {
            fprintf(cg->out, ";\n");
        }
        break;
    case AST_DO_WHILE:
        emit_indent(cg);
        fprintf(cg->out, "do");
        if (node->data.do_while.body) {
            if (node->data.do_while.body->type == AST_COMPOUND) {
                fprintf(cg->out, " {\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.do_while.body);
                cg->indent_level--;
                emit_indent(cg);
                fprintf(cg->out, "}");
            } else {
                fprintf(cg->out, "\n");
                cg->indent_level++;
                emit_stmt(cg, node->data.do_while.body);
                cg->indent_level--;
                emit_indent(cg);
            }
        }
        fprintf(cg->out, " while (");
        emit_expr(cg, node->data.do_while.cond);
        fprintf(cg->out, ");\n");
        break;
    case AST_SWITCH:
        emit_indent(cg);
        fprintf(cg->out, "switch (");
        emit_expr(cg, node->data.switch_stmt.expr);
        fprintf(cg->out, ") {\n");
        cg->indent_level++;
        if (node->data.switch_stmt.body)
            emit_stmt(cg, node->data.switch_stmt.body);
        cg->indent_level--;
        emit_indent(cg);
        fprintf(cg->out, "}\n");
        break;
    case AST_CASE:
        emit_indent(cg);
        if (node->data.case_stmt.value) {
            fprintf(cg->out, "case ");
            emit_expr(cg, node->data.case_stmt.value);
        } else {
            fprintf(cg->out, "default");
        }
        fprintf(cg->out, ":\n");
        if (node->data.case_stmt.stmt) {
            cg->indent_level++;
            emit_stmt(cg, node->data.case_stmt.stmt);
            cg->indent_level--;
        }
        break;
    case AST_RETURN:
        emit_indent(cg);
        fprintf(cg->out, "return");
        if (node->data.return_stmt.expr) {
            fprintf(cg->out, " ");
            emit_expr(cg, node->data.return_stmt.expr);
        }
        fprintf(cg->out, ";\n");
        break;
    case AST_BREAK:
        emit_indent(cg);
        fprintf(cg->out, "break;\n");
        break;
    case AST_CONTINUE:
        emit_indent(cg);
        fprintf(cg->out, "continue;\n");
        break;
    case AST_EXPR_STMT:
        emit_indent(cg);
        fprintf(cg->out, ";\n");
        break;
    default:
        emit_indent(cg);
        emit_expr(cg, node);
        fprintf(cg->out, ";\n");
        break;
    }
}

static void emit_decl(GclCodegen *cg, GclAstNode *node) {
    if (!node) return;
    switch (node->type) {
    case AST_PREP_INCLUDE:
    case AST_PREP_EXTERN:
    case AST_PREP_LIB: {
        const char *path = node->data.prep_include.path;
        if (!path) break;
        int is_system = node->data.prep_include.is_system;
        if (node->type == AST_PREP_LIB)
            fprintf(cg->out, "#include <%s.h>\n", path);
        else if (is_system)
            fprintf(cg->out, "#include <%s>\n", path);
        else
            fprintf(cg->out, "#include \"%s\"\n", path);
        break;
    }
    case AST_PREP_DEFINE:
        fprintf(cg->out, "#define %s", node->data.prep_define.name);
        if (node->data.prep_define.value)
            fprintf(cg->out, " %s", node->data.prep_define.value);
        fprintf(cg->out, "\n");
        break;
    case AST_PREP_UNDEF:
        fprintf(cg->out, "#undef %s\n", node->data.prep_undef.name);
        break;
    case AST_PREP_IFDEF:
    case AST_PREP_IFNDEF:
    case AST_PREP_IF:
    case AST_PREP_ELIF:
    case AST_PREP_ELSE:
    case AST_PREP_ENDIF:
    case AST_PREP_ERROR:
    case AST_PREP_PRAGMA:
    case AST_PREP_LINE:
        // Pass through as-is for now
        break;
    case AST_GCL_COMMENT:
    case AST_GCL_COMMENT_BLOCK:
    case AST_GCL_COMMENT_CPP:
        break; // strip GCL comments
    case AST_VAR_DECL: {
        emit_indent(cg);
        GclType *vt = node->data.var_decl.var_type;
        if (!vt) { fprintf(cg->out, "int %s;\n", node->data.var_decl.name); break; }
        emit_type_str(cg, vt);
        fprintf(cg->out, " %s", node->data.var_decl.name);
        // Emit array brackets if the type is an array
        if (vt->category == TYPE_ARRAY)
            emit_array_brackets(cg, vt);
        if (node->data.var_decl.init) {
            fprintf(cg->out, " = ");
            // Convert standalone char literals to strings for char[][] and char*[] initializers
            GclAstNode *init = node->data.var_decl.init;
            int is_char_array = (vt->category == TYPE_ARRAY && vt->base_type &&
                vt->base_type->category == TYPE_ARRAY && vt->base_type->base_type &&
                vt->base_type->base_type->category == TYPE_PRIMITIVE &&
                vt->base_type->base_type->name && strcmp(vt->base_type->base_type->name, "char") == 0);
            int is_char_ptr_array = (vt->category == TYPE_ARRAY && vt->base_type &&
                vt->base_type->category == TYPE_POINTER && vt->base_type->base_type &&
                vt->base_type->base_type->category == TYPE_PRIMITIVE &&
                vt->base_type->base_type->name && strcmp(vt->base_type->base_type->name, "char") == 0);
            if (init->type == AST_ARRAY_INIT && (is_char_array || is_char_ptr_array)) {
                if (init->data.array_init.elements) {
                    for (GclAstNode *e = init->data.array_init.elements->head; e; e = e->next) {
                        if (e->type == AST_CHAR_LITERAL) {
                            // Convert to string: 'A' → "A"
                            char buf[4] = {0};
                            int c = e->data.char_lit.code;
                            if (c >= 32 && c < 127 && c != '"' && c != '\\') {
                                buf[0] = (char)c; buf[1] = '\0';
                            } else {
                                buf[0] = '?'; buf[1] = '\0';
                            }
                            // Transform this node into a string literal
                            e->type = AST_STRING_LITERAL;
                            e->data.string_lit.str = strdup(buf);
                        }
                    }
                }
            }
            emit_expr(cg, init);
        }
        fprintf(cg->out, ";\n");
        break;
    }
    case AST_FUNC_DEF: {
        GclType *ret = node->data.func.return_type;
        emit_type_str(cg, ret);
        fprintf(cg->out, " %s(", node->data.func.name);
        if (node->data.func.params) {
            for (GclAstNode *p = node->data.func.params->head; p; p = p->next) {
                if (p != node->data.func.params->head) fprintf(cg->out, ", ");
                GclType *pt = p->data.param.param_type;
                emit_type_str(cg, pt);
                if (p->data.param.name && strlen(p->data.param.name) > 0) {
                    fprintf(cg->out, " %s", p->data.param.name);
                    // Append array brackets after param name
                    if (pt && pt->category == TYPE_ARRAY)
                        emit_array_brackets(cg, pt);
                }
            }
        }
        if (node->data.func.is_variadic) {
            if (node->data.func.params && node->data.func.params->count > 0)
                fprintf(cg->out, ", ...");
            else
                fprintf(cg->out, "...");
        }
        if (node->data.func.body) {
            fprintf(cg->out, ") {\n");
            cg->indent_level++;
            emit_stmt(cg, node->data.func.body);
            cg->indent_level--;
            fprintf(cg->out, "}\n");
        } else {
            fprintf(cg->out, ");\n");
        }
        break;
    }
    case AST_FUNC_DECL: {
        emit_type_str(cg, node->data.func.return_type);
        fprintf(cg->out, " %s(", node->data.func.name);
        if (node->data.func.params) {
            for (GclAstNode *p = node->data.func.params->head; p; p = p->next) {
                if (p != node->data.func.params->head) fprintf(cg->out, ", ");
                emit_type_str(cg, p->data.param.param_type);
            }
        }
        fprintf(cg->out, ");\n");
        break;
    }
    case AST_STRUCT_DECL:
        fprintf(cg->out, "struct %s {\n", node->data.struct_decl.name ? node->data.struct_decl.name : "");
        cg->indent_level++;
        if (node->data.struct_decl.members) {
            for (GclAstNode *m = node->data.struct_decl.members->head; m; m = m->next) {
                emit_indent(cg);
                emit_type_str(cg, m->data.var_decl.var_type);
                fprintf(cg->out, " %s;\n", m->data.var_decl.name);
            }
        }
        cg->indent_level--;
        fprintf(cg->out, "};\n");
        break;
    case AST_ENUM_DECL:
        fprintf(cg->out, "enum %s {\n", node->data.struct_decl.name ? node->data.struct_decl.name : "");
        cg->indent_level++;
        if (node->data.struct_decl.members) {
            int val = 0;
            for (GclAstNode *m = node->data.struct_decl.members->head; m; m = m->next) {
                emit_indent(cg);
                fprintf(cg->out, "%s", m->data.enum_member.name);
                if (m->data.enum_member.value) {
                    fprintf(cg->out, " = ");
                    emit_expr(cg, m->data.enum_member.value);
                } else {
                    fprintf(cg->out, " = %d", val);
                }
                fprintf(cg->out, ",\n");
                val++;
            }
        }
        cg->indent_level--;
        fprintf(cg->out, "};\n");
        break;
    case AST_TYPEDEF_DECL:
        fprintf(cg->out, "typedef ");
        emit_type_str(cg, node->data.typedef_decl.original_type);
        fprintf(cg->out, " %s;\n", node->data.typedef_decl.alias);
        break;
    default:
        emit_stmt(cg, node);
        break;
    }
}

// Recursive header scanner
static void scan_node_for_headers(GclAstNode *node, int *ns, int *nl, int *nstr, int *nb) {
    if (!node) return;
    if (node->type == AST_VAR_DECL && node->data.var_decl.var_type) {
        const char *n = node->data.var_decl.var_type->name;
        if (n) {
            if (strncmp(n, "int", 3) == 0 || strncmp(n, "uint", 4) == 0) *ns = 1;
            if (strcmp(n, "bool") == 0) *nb = 1;
        }
    }
    if (node->type == AST_CALL && node->data.call.callee &&
        node->data.call.callee->type == AST_IDENTIFIER && node->data.call.callee->data.ident.name) {
        const char *fn = node->data.call.callee->data.ident.name;
        if (strcmp(fn, "malloc") == 0 || strcmp(fn, "calloc") == 0 ||
            strcmp(fn, "realloc") == 0 || strcmp(fn, "free") == 0) *nl = 1;
        if (strcmp(fn, "strlen") == 0) *nstr = 1;
    }
    if (node->type == AST_SIZEOF) *nl = 1;
    if (node->type == AST_VAR_DECL && node->data.var_decl.init)
        scan_node_for_headers(node->data.var_decl.init, ns, nl, nstr, nb);
    if (node->type == AST_COMPOUND && node->data.compound.statements)
        for (GclAstNode *s = node->data.compound.statements->head; s; s = s->next)
            scan_node_for_headers(s, ns, nl, nstr, nb);
    if (node->type == AST_FUNC_DEF && node->data.func.body)
        scan_node_for_headers(node->data.func.body, ns, nl, nstr, nb);
    if (node->type == AST_FOR) {
        if (node->data.for_stmt.init) scan_node_for_headers(node->data.for_stmt.init, ns, nl, nstr, nb);
        if (node->data.for_stmt.body) scan_node_for_headers(node->data.for_stmt.body, ns, nl, nstr, nb);
    }
    if (node->type == AST_IF) {
        if (node->data.if_stmt.then_branch) scan_node_for_headers(node->data.if_stmt.then_branch, ns, nl, nstr, nb);
        if (node->data.if_stmt.else_branch) scan_node_for_headers(node->data.if_stmt.else_branch, ns, nl, nstr, nb);
    }
    if (node->type == AST_WHILE && node->data.while_stmt.body)
        scan_node_for_headers(node->data.while_stmt.body, ns, nl, nstr, nb);
    if (node->type == AST_DO_WHILE && node->data.do_while.body)
        scan_node_for_headers(node->data.do_while.body, ns, nl, nstr, nb);
    if (node->type == AST_ASSIGN) {
        if (node->data.assign.lhs) scan_node_for_headers(node->data.assign.lhs, ns, nl, nstr, nb);
        if (node->data.assign.rhs) scan_node_for_headers(node->data.assign.rhs, ns, nl, nstr, nb);
    }
    if (node->type == AST_BINARY) {
        if (node->data.binary.lhs) scan_node_for_headers(node->data.binary.lhs, ns, nl, nstr, nb);
        if (node->data.binary.rhs) scan_node_for_headers(node->data.binary.rhs, ns, nl, nstr, nb);
    }
    if (node->type == AST_UNARY && node->data.unary.operand)
        scan_node_for_headers(node->data.unary.operand, ns, nl, nstr, nb);
    if (node->type == AST_SUBSCRIPT) {
        if (node->data.subscript.base) scan_node_for_headers(node->data.subscript.base, ns, nl, nstr, nb);
        if (node->data.subscript.index) scan_node_for_headers(node->data.subscript.index, ns, nl, nstr, nb);
    }
    if (node->type == AST_ARRAY_INIT && node->data.array_init.elements)
        for (GclAstNode *e = node->data.array_init.elements->head; e; e = e->next)
            scan_node_for_headers(e, ns, nl, nstr, nb);
}

void codegen_generate(GclCodegen *cg, GclAstNode *program) {
    if (!program) return;
    int ns = 0, nl = 0, nstr = 0, nb = 0;
    for (GclAstNode *node = program->next; node; node = node->next)
        scan_node_for_headers(node, &ns, &nl, &nstr, &nb);

    if (ns) fprintf(cg->out, "#include <stdint.h>\n");
    if (nl) fprintf(cg->out, "#include <stdlib.h>\n");
    if (nstr) fprintf(cg->out, "#include <string.h>\n");
    if (nb) fprintf(cg->out, "#include <stdbool.h>\n");
    fprintf(cg->out, "\n");

    for (GclAstNode *node = program->next; node; node = node->next)
        emit_decl(cg, node);
}

void codegen_init(GclCodegen *cg, FILE *out) {
    cg->out = out;
    cg->indent_level = 0;
    cg->need_semicolon = 0;
}

void codegen_generate_to_file(GclAstNode *program, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("codegen: fopen"); return; }
    GclCodegen cg;
    codegen_init(&cg, f);
    codegen_generate(&cg, program);
    fclose(f);
}
