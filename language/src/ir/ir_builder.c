#include "ir_builder.h"
#include "../type/type.h"
#include "../lexer/lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================= */
/*  Builder helpers                                                    */
/* ================================================================= */

static IROperand build_expr(IRBuilder *b, AstNode *node);

IRBuilder *ir_builder_new(IRModule *mod) {
    IRBuilder *b = (IRBuilder*)calloc(1, sizeof(IRBuilder));
    if (!b) return NULL;
    b->module = mod;
    b->next_temp = 0;
    b->next_label = 0;
    b->next_local = 0;
    b->string_count = 0;
    b->string_cap = 0;
    b->strings = NULL;
    return b;
}

void ir_builder_free(IRBuilder *b) {
    if (!b) return;
    for (int i = 0; i < b->string_count; i++)
        free((void*)b->strings[i]);
    free(b->strings);
    free(b);
}

/* Add a string to the builder's string table and return its index */
int ir_builder_add_string(IRBuilder *b, const char *str) {
    if (!b || !str) return -1;
    if (b->string_count >= b->string_cap) {
        int new_cap = b->string_cap ? b->string_cap * 2 : 16;
        b->strings = (const char**)realloc(b->strings, new_cap * sizeof(const char*));
        b->string_cap = new_cap;
    }
    /* Make a copy - need to process escape sequences */
    size_t len;
    process_escapes(str, strlen(str), NULL, &len);
    char *processed = (char*)malloc(len + 1);
    if (processed) {
        process_escapes(str, strlen(str), processed, NULL);
        processed[len] = '\0';
    }
    b->strings[b->string_count] = processed ? processed : strdup(str);
    return b->string_count++;
}

/* ================================================================= */
/*  Block / Emit helpers                                              */
/* ================================================================= */

IRBlock *ir_builder_new_block(IRBuilder *b, const char *label) {
    IRBlock *block = ir_block_new(b->next_label++, label);
    if (!b->ir_func->blocks) {
        b->ir_func->blocks = block;
    } else {
        IRBlock *last = b->ir_func->blocks;
        while (last->next) last = last->next;
        last->next = block;
    }
    b->ir_func->block_count++;
    return block;
}

IRInsn *ir_builder_emit(IRBuilder *b, IROpcode op,
                         IROperand dst, IROperand src1, IROperand src2) {
    IRInsn *insn = ir_insn_new(op, dst, src1, src2);
    if (!insn) return NULL;
    insn->insn_id = b->module->next_insn_id++;
    insn->src_file = b->src_file;
    insn->src_line = b->src_line;
    insn->src_col = b->src_col;
    insn->func_name = b->func_name;
    ir_block_add_insn(b->current_block, insn);
    return insn;
}

int ir_builder_new_temp(IRBuilder *b, GclType *type) {
    (void)type;
    return b->next_temp++;
}

int ir_builder_new_local(IRBuilder *b, GclType *type) {
    (void)type;
    return b->next_local++;
}

void ir_builder_set_location(IRBuilder *b, int line, int col) {
    b->src_line = line;
    b->src_col = col;
}

/* ================================================================= */
/*  Expression builder (AST → IR operands)                            */
/* ================================================================= */

static IROperand build_expr(IRBuilder *b, AstNode *node) {
    if (!node) return ir_none();
    ir_builder_set_location(b, node->line, node->col);

    switch (node->type) {

    case AST_LITERAL_INT:
        return ir_const_int(node->data.int_val);

    case AST_LITERAL_FLOAT:
        return ir_const_float(node->data.float_val);

    case AST_STRING: {
        /* String literal: store in string table and return reference */
        int idx = ir_builder_add_string(b, node->data.str_val);
        /* Return a temp pointing to the string index (will become pool offset) */
        char tmpbuf[32];
        snprintf(tmpbuf, sizeof(tmpbuf), "__str_%d", idx);
        return ir_const_string(b->strings[idx]);
    }

    case AST_IDENTIFIER: {
        GclType *t = type_int();
        IROperand op = ir_local(0, t);
        op.data.name = node->data.id;
        return op;
    }

    case AST_FUNC_CALL: {
        /* First, evaluate the function name */
        const char *fname = node->data.call.name;
        int acount = node->data.call.acount;
        
        /* Push a string constant if first arg is a string literal */
        if (strcmp(fname, "printf") == 0 || strcmp(fname, "puts") == 0) {
            /* For printf: push all args first, then emit IR_PRINTF */
            for (int i = 0; i < acount; i++) {
                IROperand arg = build_expr(b, node->data.call.args[i]);
                /* If it's a string, emit push with the const_string operand */
                if (arg.kind == IR_CONST_STRING) {
                    ir_builder_emit(b, IR_PUSH, arg, ir_none(), ir_none());
                } else if (arg.kind == IR_CONST_INT) {
                    ir_builder_emit(b, IR_PUSH, arg, ir_none(), ir_none());
                } else if (arg.kind == IR_CONST_FLOAT) {
                    ir_builder_emit(b, IR_PUSH, arg, ir_none(), ir_none());
                } else if (arg.kind == IR_TEMP || arg.kind == IR_LOCAL) {
                    ir_builder_emit(b, IR_PUSH, arg, ir_none(), ir_none());
                } else {
                    /* For complex expressions, store to a temp first */
                    int temp = ir_builder_new_temp(b, type_int());
                    IROperand td = ir_temp(temp, type_int());
                    /* evaluate + push */
                    IROperand val = build_expr(b, node->data.call.args[i]);
                    ir_builder_emit(b, IR_PUSH, val, ir_none(), ir_none());
                }
            }
            
            /* Format string is the first argument */
            IROperand fmt_op = ir_none();
            if (acount > 0 && node->data.call.args[0]->type == AST_STRING) {
                int idx = ir_builder_add_string(b, node->data.call.args[0]->data.str_val);
                fmt_op = ir_const_string(b->strings[idx]);
            }
            
            IROperand arg_count_op = ir_const_int(acount);
            ir_builder_emit(b, IR_PRINTF, fmt_op, arg_count_op, ir_none());
            int temp = ir_builder_new_temp(b, type_int());
            return ir_temp(temp, type_int());
        }
        
        /* Regular function call: evaluate all args for side effects */
        for (int i = 0; i < acount && i < 16; i++)
            build_expr(b, node->data.call.args[i]);
        int temp = ir_builder_new_temp(b, type_int());
        return ir_temp(temp, type_int());
    }

    case AST_BINARY_OP: {
        IROperand left = build_expr(b, node->data.bin.left);
        IROperand right = build_expr(b, node->data.bin.right);
        int op_token = node->data.bin.op;
        IROpcode opcode;
        switch (op_token) {
        case TOKEN_PLUS:   opcode = IR_ADD; break;
        case TOKEN_MINUS:  opcode = IR_SUB; break;
        case TOKEN_STAR:   opcode = IR_MUL; break;
        case TOKEN_SLASH:  opcode = IR_DIV; break;
        case TOKEN_PERCENT:opcode = IR_MOD; break;
        case TOKEN_AND:    opcode = IR_AND; break;
        case TOKEN_PIPE:   opcode = IR_OR;  break;
        case TOKEN_CARET:  opcode = IR_XOR; break;
        case TOKEN_LSHIFT: opcode = IR_SHL; break;
        case TOKEN_RSHIFT: opcode = IR_SHR; break;
        case TOKEN_EQEQ:   opcode = IR_EQ;  break;
        case TOKEN_BANGEQ: opcode = IR_NE;  break;
        case TOKEN_LT:     opcode = IR_LT;  break;
        case TOKEN_GT:     opcode = IR_GT;  break;
        case TOKEN_LE:     opcode = IR_LE;  break;
        case TOKEN_GE:     opcode = IR_GE;  break;
        default:           opcode = IR_ADD; break;
        }
        GclType *lt = left.type ? left.type : type_int();
        GclType *rt = right.type ? right.type : type_int();
        GclType *res_type = type_int();
        if (type_is_float(lt) || type_is_float(rt))
            res_type = type_double();
        else if ((lt->kind == TYPE_LONG || lt->kind == TYPE_LONG_LONG ||
                  rt->kind == TYPE_LONG || rt->kind == TYPE_LONG_LONG))
            res_type = type_long();
        int temp = ir_builder_new_temp(b, res_type);
        IROperand dst = ir_temp(temp, res_type);
        left.type = lt;
        right.type = rt;
        ir_builder_emit(b, opcode, dst, left, right);
        return dst;
    }

    case AST_UNARY_OP: {
        IROperand operand = build_expr(b, node->data.un.operand);
        int op_token = node->data.un.op;
        IROpcode opcode;
        if (op_token == '-') opcode = IR_NEG;
        else if (op_token == '!') opcode = IR_NOT;
        else if (op_token == '~') opcode = IR_BNOT;
        else return operand;
        GclType *t = operand.type ? operand.type : type_int();
        int temp = ir_builder_new_temp(b, t);
        IROperand dst = ir_temp(temp, t);
        ir_builder_emit(b, opcode, dst, operand, ir_none());
        return dst;
    }

    default:
        return ir_const_int(0);
    }
}

/* ================================================================= */
/*  Statement builder                                                  */
/* ================================================================= */

static void build_stmt(IRBuilder *b, AstNode *node) {
    if (!node) return;
    ir_builder_set_location(b, node->line, node->col);

    switch (node->type) {

    case AST_BLOCK:
        for (int i = 0; i < node->data.block.count; i++)
            build_stmt(b, node->data.block.stmts[i]);
        break;

    case AST_VAR_DECL: {
        GclType *t = type_int();
        if (node->data.var.type)
            t = ast_to_type(node->data.var.type);
        int slot = ir_builder_new_local(b, t);
        IROperand local = ir_local(slot, t);
        ir_builder_emit(b, IR_ALLOCA, local, ir_none(), ir_none());
        if (node->data.var.init) {
            IROperand val = build_expr(b, node->data.var.init);
            ir_builder_emit(b, IR_STORE, local, val, ir_none());
        }
        break;
    }

    case AST_RETURN: {
        IROperand val = ir_none();
        if (node->data.ret.val)
            val = build_expr(b, node->data.ret.val);
        ir_builder_emit(b, IR_RET, val, ir_none(), ir_none());
        break;
    }

    case AST_IF: {
        IROperand cond = build_expr(b, node->data.if_stmt.cond);
        int else_label = b->next_label++;
        int end_label = b->next_label++;
        ir_builder_emit(b, IR_BR, cond, ir_label(end_label), ir_label(else_label));
        IRBlock *then_block = ir_builder_new_block(b, "if.then");
        b->current_block = then_block;
        build_stmt(b, node->data.if_stmt.then);
        ir_builder_emit(b, IR_JMP, ir_label(end_label), ir_none(), ir_none());
        IRBlock *else_block = ir_builder_new_block(b, "if.else");
        b->current_block = else_block;
        if (node->data.if_stmt.els)
            build_stmt(b, node->data.if_stmt.els);
        ir_builder_emit(b, IR_JMP, ir_label(end_label), ir_none(), ir_none());
        IRBlock *end_block = ir_builder_new_block(b, "if.end");
        b->current_block = end_block;
        break;
    }

    case AST_WHILE: {
        int cond_label = b->next_label++;
        int body_label = b->next_label++;
        int end_label = b->next_label++;
        IRBlock *cond_block = ir_builder_new_block(b, "while.cond");
        b->current_block = cond_block;
        IROperand cond = build_expr(b, node->data.while_stmt.cond);
        ir_builder_emit(b, IR_BR, cond, ir_label(body_label), ir_label(end_label));
        IRBlock *body_block = ir_builder_new_block(b, "while.body");
        b->current_block = body_block;
        build_stmt(b, node->data.while_stmt.body);
        ir_builder_emit(b, IR_JMP, ir_label(cond_label), ir_none(), ir_none());
        IRBlock *end_block = ir_builder_new_block(b, "while.end");
        b->current_block = end_block;
        break;
    }

    case AST_FOR: {
        int cond_label = b->next_label++;
        int body_label = b->next_label++;
        int step_label = b->next_label++;
        int end_label = b->next_label++;
        if (node->data.for_stmt.init)
            build_stmt(b, node->data.for_stmt.init);
        IRBlock *cond_block = ir_builder_new_block(b, "for.cond");
        b->current_block = cond_block;
        IROperand cond = ir_const_int(1);
        if (node->data.for_stmt.cond)
            cond = build_expr(b, node->data.for_stmt.cond);
        ir_builder_emit(b, IR_BR, cond, ir_label(body_label), ir_label(end_label));
        IRBlock *body_block = ir_builder_new_block(b, "for.body");
        b->current_block = body_block;
        build_stmt(b, node->data.for_stmt.body);
        ir_builder_emit(b, IR_JMP, ir_label(step_label), ir_none(), ir_none());
        IRBlock *step_block = ir_builder_new_block(b, "for.step");
        b->current_block = step_block;
        if (node->data.for_stmt.step)
            build_expr(b, node->data.for_stmt.step);
        ir_builder_emit(b, IR_JMP, ir_label(cond_label), ir_none(), ir_none());
        IRBlock *end_block = ir_builder_new_block(b, "for.end");
        b->current_block = end_block;
        break;
    }

    default:
        build_expr(b, node);
        break;
    }
}

/* ================================================================= */
/*  Function builder                                                   */
/* ================================================================= */

IRFunction *ir_build_function(IRBuilder *b, AstNode *func_node) {
    if (!func_node || func_node->type != AST_FUNCTION_DEF) return NULL;

    b->func_name = func_node->data.func.name;
    b->next_temp = 0;
    b->next_label = 0;
    b->next_local = 0;

    GclType *ret_type = type_int();
    if (func_node->data.func.type)
        ret_type = ast_to_type(func_node->data.func.type);

    IRFunction *f = ir_func_new(func_node->data.func.name, ret_type,
                                 NULL, func_node->data.func.pcount);
    b->ir_func = f;

    IRBlock *entry = ir_builder_new_block(b, "entry");
    b->current_block = entry;
    f->entry_block = entry;

    if (func_node->data.func.body)
        build_stmt(b, func_node->data.func.body);

    f->local_count = b->next_local;
    f->temp_count = b->next_temp;

    return f;
}

/* ================================================================= */
/*  Program builder                                                    */
/* ================================================================= */

void ir_build_program(IRBuilder *b, AstNode *prog) {
    if (!prog || prog->type != AST_PROGRAM) return;
    for (int i = 0; i < prog->data.program.count; i++) {
        AstNode *node = prog->data.program.stmts[i];
        if (node->type == AST_FUNCTION_DEF) {
            IRFunction *f = ir_build_function(b, node);
            if (f) {
                f->next = b->module->functions;
                b->module->functions = f;
                b->module->func_count++;
            }
        }
    }
}
