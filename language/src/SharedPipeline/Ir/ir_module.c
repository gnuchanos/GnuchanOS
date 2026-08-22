
/* === From: gcl_ir_core.c === */
#include <stdio.h>
#include <string.h>
#include "gcl_ir.h"

void gcl_ir_init(GclIrProgram *prog, GclArena *arena, GclDiagBag *diag) {
    memset(prog->instrs, 0, sizeof(prog->instrs));
    prog->count = 0;
    prog->arena = arena;
    prog->diag = diag;
}

int gcl_ir_emit(GclIrProgram *prog, GclIrOp op, int64_t i_val, double f_val,
                const char *s_val, int arg_count) {
    if (prog->count >= GCL_IR_MAX) {
        gcl_diag_add(prog->diag, GCL_DIAG_ERROR, 0, 0, "<ir>",
                     "IR instruction limit reached (%d)", GCL_IR_MAX);
        return -1;
    }
    GclIrInstr *instr = &prog->instrs[prog->count];
    instr->op = op;
    instr->i_val = i_val;
    instr->f_val = f_val;
    instr->s_val = s_val;
    instr->arg_count = arg_count;
    return prog->count++;
}

const char *gcl_ir_op_name(GclIrOp op) {
    switch (op) {
    case IR_NOP:          return "NOP";
    case IR_PUSH_INT:     return "PUSH_INT";
    case IR_PUSH_FLOAT:   return "PUSH_FLOAT";
    case IR_PUSH_STRING:  return "PUSH_STRING";
    case IR_PUSH_NULL:    return "PUSH_NULL";
    case IR_POP:          return "POP";
    case IR_ADD:          return "ADD";
    case IR_SUB:          return "SUB";
    case IR_MUL:          return "MUL";
    case IR_DIV:          return "DIV";
    case IR_MOD:          return "MOD";
    case IR_PTR_ADD:      return "PTR_ADD";
    case IR_PTR_SUB:      return "PTR_SUB";
    case IR_NEG:          return "NEG";
    case IR_NOT:          return "NOT";
    case IR_BITNOT:       return "BITNOT";
    case IR_EQ:           return "EQ";
    case IR_NEQ:          return "NEQ";
    case IR_LT:           return "LT";
    case IR_GT:           return "GT";
    case IR_LTE:          return "LTE";
    case IR_GTE:          return "GTE";
    case IR_AND:          return "AND";
    case IR_OR:           return "OR";
    case IR_BITAND:       return "BITAND";
    case IR_BITOR:        return "BITOR";
    case IR_BITXOR:       return "BITXOR";
    case IR_SHL:          return "SHL";
    case IR_SHR:          return "SHR";
    case IR_LOAD:         return "LOAD";
    case IR_STORE:        return "STORE";
    case IR_LOAD_GLOBAL:  return "LOAD_GLOBAL";
    case IR_STORE_GLOBAL: return "STORE_GLOBAL";
    case IR_STRUCT_COPY:  return "STRUCT_COPY";
    case IR_CALL:         return "CALL";
    case IR_RET:          return "RET";
    case IR_JMP:          return "JMP";
    case IR_JZ:           return "JZ";
    case IR_JNZ:          return "JNZ";
    case IR_LABEL:        return "LABEL";
    case IR_MEMBER:       return "MEMBER";
    case IR_MEMBER_ASSIGN: return "MEMBER_ASSIGN";
    case IR_INDEX:        return "INDEX";
    case IR_INDEX_ASSIGN: return "INDEX_ASSIGN";
    case IR_SCANF: return "SCANF";
    case IR_FREE:  return "FREE";
    case IR_GCMALLOC: return "GCMALLOC";
    case IR_PRINT:        return "PRINT";
    case IR_HALT:         return "HALT";
    }
    return "???";
}


/* === From: gcl_ir_dump.c === */
#include <stdio.h>
#include "gcl_ir.h"

void gcl_ir_dump(const GclIrProgram *prog) {
    printf("--- IR Dump (%d instructions) ---\n", prog->count);
    for (int i = 0; i < prog->count; i++) {
        const GclIrInstr *ins = &prog->instrs[i];
        printf("  [%04d] %-14s", i, gcl_ir_op_name(ins->op));
        switch (ins->op) {
        case IR_PUSH_INT:
            printf(" %lld", (long long)ins->i_val);
            break;
        case IR_PUSH_FLOAT:
            printf(" %f", ins->f_val);
            break;
        case IR_PUSH_STRING:
            if (ins->s_val) printf(" \"%s\"", ins->s_val);
            break;
        case IR_LOAD:
        case IR_STORE:
        case IR_LOAD_GLOBAL:
        case IR_STORE_GLOBAL:
        case IR_LABEL:
        case IR_MEMBER:
            if (ins->s_val) printf(" %s", ins->s_val);
            break;
        case IR_CALL:
            if (ins->s_val) printf(" %s", ins->s_val);
            printf(" (argc=%d)", ins->arg_count);
            break;
        case IR_JMP:
        case IR_JZ:
        case IR_JNZ:
            printf(" -> %lld", (long long)ins->i_val);
            break;
        case IR_INDEX_ASSIGN:
            if (ins->s_val) printf(" %s", ins->s_val);
            break;
        default:
            break;
        }
        printf("\n");
    }
    printf("--- End IR ---\n");
}


/* === From: gcl_ir_gen.c === */
#include <stdio.h>
#include <string.h>
#include "gcl_ir.h"

/* IR generation from AST — walks the tree and emits instructions */

#define GCL_LOOP_PATCH_MAX 256

static void gen_expr(GclIrProgram *prog, const GclAstNode *node);
static void gen_stmt(GclIrProgram *prog, const GclAstNode *node);

/* Loop context: patching targets for break/continue. */
typedef struct {
    int break_patches[GCL_LOOP_PATCH_MAX];
    int break_count;
    int continue_patches[GCL_LOOP_PATCH_MAX];
    int continue_count;
    int continue_target;
    int is_switch;
} GclLoopCtx;

static GclLoopCtx g_loop_stack[64];
static int g_loop_depth = 0;

static void loop_ctx_enter(int continue_target, int is_switch) {
    if (g_loop_depth < 64) {
        GclLoopCtx *ctx = &g_loop_stack[g_loop_depth];
        memset(ctx, 0, sizeof(*ctx));
        ctx->continue_target = continue_target;
        ctx->is_switch = is_switch;
        g_loop_depth++;
    }
}

static void loop_ctx_exit(GclIrProgram *prog) {
    if (g_loop_depth <= 0) return;
    GclLoopCtx *ctx = &g_loop_stack[--g_loop_depth];
    int cont_target = ctx->continue_target;
    if (cont_target < 0) cont_target = prog->count;
    for (int i = 0; i < ctx->continue_count; i++) {
        prog->instrs[ctx->continue_patches[i]].i_val = cont_target;
    }
    for (int i = 0; i < ctx->break_count; i++) {
        prog->instrs[ctx->break_patches[i]].i_val = prog->count;
    }
}

static void emit_break_patch(GclIrProgram *prog) {
    if (g_loop_depth > 0) {
        GclLoopCtx *ctx = &g_loop_stack[g_loop_depth - 1];
        int idx = gcl_ir_emit(prog, IR_JMP, 0, 0, NULL, 0);
        if (ctx->break_count < GCL_LOOP_PATCH_MAX) {
            ctx->break_patches[ctx->break_count++] = idx;
        }
    }
}

static void emit_continue_patch(GclIrProgram *prog) {
    if (g_loop_depth > 0) {
        GclLoopCtx *ctx = &g_loop_stack[g_loop_depth - 1];
        int idx = gcl_ir_emit(prog, IR_JMP, 0, 0, NULL, 0);
        if (ctx->continue_count < GCL_LOOP_PATCH_MAX) {
            ctx->continue_patches[ctx->continue_count++] = idx;
        }
    }
}

/* Copies a string into the IR arena so generated literals survive the build */
static const char *ir_strdup(GclIrProgram *prog, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *mem = (char *)gcl_arena_alloc(prog->arena, n + 1);
    memcpy(mem, s, n + 1);
    return mem;
}

/* True when name refers to a registered class (constructor call). */
static int ir_is_class(GclIrProgram *prog, const char *name) {
    if (!name) return 0;
    for (int i = 0; i < prog->class_count; i++) {
        if (strcmp(prog->class_names[i], name) == 0) return 1;
    }
    return 0;
}

/* Builds an encoded "obj\x01member" key for IR_MEMBER_ASSIGN. */
static const char *ir_member_key(GclIrProgram *prog, const char *obj, const char *member) {
    if (!obj || !member) return NULL;
    size_t on = strlen(obj), mn = strlen(member);
    char *mem = (char *)gcl_arena_alloc(prog->arena, on + mn + 2);
    memcpy(mem, obj, on);
    mem[on] = '\x01';
    memcpy(mem + on + 1, member, mn);
    mem[on + mn + 1] = '\0';
    return mem;
}

/* Registers a static array (name, rows, cols) in the IR program. */
static void ir_register_array(GclIrProgram *prog, const char *name, int rows, int cols) {
    if (!name) return;
    for (int i = 0; i < prog->array_count; i++) {
        if (prog->arrays[i].name && strcmp(prog->arrays[i].name, name) == 0) {
            prog->arrays[i].rows = rows;
            prog->arrays[i].cols = cols;
            return;
        }
    }
    if (prog->array_count >= GCL_IR_ARR_MAX) return;
    prog->arrays[prog->array_count].name = name;
    prog->arrays[prog->array_count].rows = rows;
    prog->arrays[prog->array_count].cols = cols;
    prog->array_count++;
}

static int ir_array_cols(GclIrProgram *prog, const char *name) {
    if (!name) return 0;
    for (int i = 0; i < prog->array_count; i++) {
        if (prog->arrays[i].name && strcmp(prog->arrays[i].name, name) == 0) {
            return prog->arrays[i].cols;
        }
    }
    return 0;
}

/* Part 5: gcMalloc/malloc(...) caller detection with literal reserve/extra.
 * Returns 1 when node is AST_CALL_EXPR targeting gcMalloc/malloc.
 * Now also evaluates simple expressions: reserve=sizeof(type), reserve=N*M, etc. */
static int64_t eval_const_expr(const GclAstNode *node) {
    if (!node) return 0;
    switch (node->kind) {
    case AST_INT_LIT: return node->int_value;
    case AST_SIZEOF_EXPR: {
        /* sizeof(type) — use same logic as gen_expr */
        const char *type_name = node->str_value ? node->str_value : "int";
        int64_t size = 8;
        if (strncmp(type_name, "int8", 4) == 0) size = 1;
        else if (strncmp(type_name, "int16", 5) == 0) size = 2;
        else if (strncmp(type_name, "int32", 5) == 0) size = 4;
        else if (strncmp(type_name, "int64", 5) == 0) size = 8;
        else if (strncmp(type_name, "char", 4) == 0) size = 1;
        else if (strncmp(type_name, "gcChar", 6) == 0) size = 1;
        else size = 8;
        return size;
    }
    case AST_BINARY_EXPR: {
        int64_t left = eval_const_expr(node->children[0]);
        int64_t right = eval_const_expr(node->children[1]);
        if (!node->str_value) return 0;
        if (strcmp(node->str_value, "*") == 0) return left * right;
        else if (strcmp(node->str_value, "/") == 0) return right ? left / right : 0;
        else if (strcmp(node->str_value, "+") == 0) return left + right;
        else if (strcmp(node->str_value, "-") == 0) return left - right;
        return 0;
    }
    default: return 0;
    }
}

static int ir_parse_malloc_call(const GclAstNode *node, int64_t *reserve, int64_t *extra) {
    if (!node || node->kind != AST_CALL_EXPR) return 0;
    const GclAstNode *callee = node->children[0];
    if (!callee || callee->kind != AST_IDENT_EXPR || !callee->str_value) return 0;
    if (strcmp(callee->str_value, "gcMalloc") != 0 &&
        strcmp(callee->str_value, "malloc") != 0) return 0;
    *reserve = 0;
    *extra = 0;
    for (int i = 1; i < node->child_count; i++) {
        const GclAstNode *a = node->children[i];
        if (!a || a->kind != AST_ASSIGN_EXPR || a->child_count < 2) continue;
        const GclAstNode *an = a->children[0];
        const GclAstNode *av = a->children[1];
        if (!an || an->kind != AST_IDENT_EXPR || !an->str_value) continue;
        /* Part 5 extended: now also accept expressions (sizeof, multiplication, etc) */
        int64_t val = eval_const_expr(av);
        if (strcmp(an->str_value, "reserve") == 0) *reserve = val;
        else if (strcmp(an->str_value, "extra") == 0) *extra = val;
    }
    return 1;
}

static void gen_expr(GclIrProgram *prog, const GclAstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case AST_INT_LIT:
        gcl_ir_emit(prog, IR_PUSH_INT, node->int_value, 0, NULL, 0);
        break;
    case AST_FLOAT_LIT:
        gcl_ir_emit(prog, IR_PUSH_FLOAT, 0, node->float_value, NULL, 0);
        break;
    case AST_STRING_LIT:
        gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, node->str_value, 0);
        break;
    case AST_CHAR_LIT:
        gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, node->str_value, 0);
        break;
    case AST_NULL_LIT:
        gcl_ir_emit(prog, IR_PUSH_NULL, 0, 0, NULL, 0);
        break;
    case AST_BOOL_LIT:
        gcl_ir_emit(prog, IR_PUSH_INT, node->int_value, 0, NULL, 0);
        break;
    case AST_IDENT_EXPR:
        gcl_ir_emit(prog, IR_LOAD, 0, 0, node->str_value, 0);
        break;
    case AST_BINARY_EXPR: {
        gen_expr(prog, node->children[0]);
        gen_expr(prog, node->children[1]);
        if (!node->str_value) break;
        if (strcmp(node->str_value, "+") == 0) gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "-") == 0) gcl_ir_emit(prog, IR_SUB, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "*") == 0) gcl_ir_emit(prog, IR_MUL, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "/") == 0) gcl_ir_emit(prog, IR_DIV, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "%") == 0) gcl_ir_emit(prog, IR_MOD, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "==") == 0) gcl_ir_emit(prog, IR_EQ, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "!=") == 0) gcl_ir_emit(prog, IR_NEQ, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "<") == 0) gcl_ir_emit(prog, IR_LT, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, ">") == 0) gcl_ir_emit(prog, IR_GT, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "<=") == 0) gcl_ir_emit(prog, IR_LTE, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, ">=") == 0) gcl_ir_emit(prog, IR_GTE, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "&&") == 0) gcl_ir_emit(prog, IR_AND, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "||") == 0) gcl_ir_emit(prog, IR_OR, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "&") == 0) gcl_ir_emit(prog, IR_BITAND, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "|") == 0) gcl_ir_emit(prog, IR_BITOR, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "^") == 0) gcl_ir_emit(prog, IR_BITXOR, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, "<<") == 0) gcl_ir_emit(prog, IR_SHL, 0, 0, NULL, 0);
        else if (strcmp(node->str_value, ">>") == 0) gcl_ir_emit(prog, IR_SHR, 0, 0, NULL, 0);
        break;
    }
    case AST_UNARY_EXPR:
        if (node->str_value &&
            (strcmp(node->str_value, "++") == 0 || strcmp(node->str_value, "--") == 0)) {
            const GclAstNode *target = node->children[0];
            if (target && target->kind == AST_IDENT_EXPR && target->str_value) {
                bool is_post = node->is_pointer;  /* is_pointer flag marks POST-inc/dec */
                
                if (is_post) {
                    /* Post-increment: push old value first */
                    gcl_ir_emit(prog, IR_LOAD, 0, 0, target->str_value, 0);
                }
                /* Load, add/sub 1, store */
                gcl_ir_emit(prog, IR_LOAD, 0, 0, target->str_value, 0);
                gcl_ir_emit(prog, IR_PUSH_INT, 1, 0, NULL, 0);
                gcl_ir_emit(prog, strcmp(node->str_value, "++") == 0 ? IR_ADD : IR_SUB, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_STORE, 0, 0, target->str_value, 0);
                
                if (!is_post) {
                    /* Pre-increment: push new value after storing */
                    gcl_ir_emit(prog, IR_LOAD, 0, 0, target->str_value, 0);
                }
            }
            break;
        }
        gen_expr(prog, node->children[0]);
        if (node->str_value) {
            if (strcmp(node->str_value, "-") == 0) gcl_ir_emit(prog, IR_NEG, 0, 0, NULL, 0);
            else if (strcmp(node->str_value, "!") == 0) gcl_ir_emit(prog, IR_NOT, 0, 0, NULL, 0);
            else if (strcmp(node->str_value, "~") == 0) gcl_ir_emit(prog, IR_BITNOT, 0, 0, NULL, 0);
        }
        break;
    case AST_CAST_EXPR:
        /* Type casting: (type) expr - just evaluate the operand, cast is implicit at runtime */
        if (node->child_count > 0) {
            gen_expr(prog, node->children[0]);
        }
        break;
    case AST_ASSIGN_EXPR: {
        const GclAstNode *lhs = node->children[0];
        /* Part 5: List = gcMalloc(reserve=N, extra=M); → IR_GCMALLOC */
        if (node->str_value && strcmp(node->str_value, "=") == 0 &&
            lhs && lhs->kind == AST_IDENT_EXPR && lhs->str_value &&
            node->child_count >= 2) {
            int64_t reserve = 0, extra = 0;
            if (ir_parse_malloc_call(node->children[1], &reserve, &extra)) {
                gcl_ir_emit(prog, IR_GCMALLOC, reserve, 0,
                            ir_strdup(prog, lhs->str_value), (int)extra);
                break;
            }
        }
        /* Part 14: element assignment a[i] = v / grid[i][j] = v. */
        const GclAstNode *idx_base = NULL;
        const GclAstNode *row_idx = NULL;
        const GclAstNode *col_idx = NULL;
        if (node->str_value && strcmp(node->str_value, "=") == 0 &&
            lhs && lhs->kind == AST_INDEX_EXPR && lhs->children[0]) {
            if (lhs->children[0]->kind == AST_IDENT_EXPR) {
                idx_base = lhs->children[0];
                row_idx = lhs->child_count > 1 ? lhs->children[1] : NULL;
            } else if (lhs->children[0]->kind == AST_INDEX_EXPR &&
                       lhs->children[0]->children[0] &&
                       lhs->children[0]->children[0]->kind == AST_IDENT_EXPR) {
                idx_base = lhs->children[0]->children[0];
                row_idx = lhs->children[0]->child_count > 1 ? lhs->children[0]->children[1] : NULL;
                col_idx = lhs->child_count > 1 ? lhs->children[1] : NULL;
            }
        }
        if (idx_base && idx_base->str_value) {
            const char *arr_name = idx_base->str_value;
            int cols = ir_array_cols(prog, arr_name);
            if (cols > 0 && row_idx && col_idx) {
                gen_expr(prog, node->children[1]);
                gcl_ir_emit(prog, IR_LOAD, 0, 0, arr_name, 0);
                gen_expr(prog, row_idx);
                gcl_ir_emit(prog, IR_PUSH_INT, cols, 0, NULL, 0);
                gcl_ir_emit(prog, IR_MUL, 0, 0, NULL, 0);
                gen_expr(prog, col_idx);
                gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_INDEX_ASSIGN, 0, 0, arr_name, 0);
                break;
            }
            gen_expr(prog, node->children[1]);
            gcl_ir_emit(prog, IR_LOAD, 0, 0, arr_name, 0);
            if (row_idx) gen_expr(prog, row_idx);
            gcl_ir_emit(prog, IR_INDEX_ASSIGN, 0, 0, arr_name, 0);
            break;
        }
        /* Part 14: instance field write — obj.field = v. */
        if (node->str_value && strcmp(node->str_value, "=") == 0 &&
            lhs && lhs->kind == AST_MEMBER_EXPR && lhs->child_count >= 2 &&
            lhs->children[0] && lhs->children[0]->kind == AST_IDENT_EXPR &&
            lhs->children[0]->str_value && lhs->children[1] && lhs->children[1]->str_value) {
            gen_expr(prog, node->children[1]);
            gcl_ir_emit(prog, IR_LOAD, 0, 0, lhs->children[0]->str_value, 0);
            gcl_ir_emit(prog, IR_MEMBER_ASSIGN, 0, 0,
                        ir_member_key(prog, lhs->children[0]->str_value,
                                      lhs->children[1]->str_value), 0);
            break;
        }
        const char *target = (node->children[0] && node->children[0]->str_value)
                             ? node->children[0]->str_value : NULL;
        if (node->str_value && strcmp(node->str_value, "=") != 0 && target) {
            if (strcmp(node->str_value, "+=") == 0 || strcmp(node->str_value, "-=") == 0 ||
                strcmp(node->str_value, "*=") == 0 || strcmp(node->str_value, "/=") == 0) {
                gcl_ir_emit(prog, IR_LOAD, 0, 0, target, 0);
                gen_expr(prog, node->children[1]);
                const char *op = node->str_value;
                if (strcmp(op, "+=") == 0) gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);
                else if (strcmp(op, "-=") == 0) gcl_ir_emit(prog, IR_SUB, 0, 0, NULL, 0);
                else if (strcmp(op, "*=") == 0) gcl_ir_emit(prog, IR_MUL, 0, 0, NULL, 0);
                else gcl_ir_emit(prog, IR_DIV, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_STORE, 0, 0, target, 0);
                break;
            }
        }
        gen_expr(prog, node->children[1]);
        if (target) {
            /* Check if RHS is struct initialization (starts with 'S;') */
            const GclAstNode *rhs = node->children[1];
            int is_struct_init = (rhs && rhs->kind == AST_STRING_LIT && rhs->str_value &&
                                  strlen(rhs->str_value) > 2 && rhs->str_value[0] == 'S' && 
                                  rhs->str_value[1] == ';');
            
            if (is_struct_init) {
                /* Struct assignment: emit IR_STRUCT_COPY instead of simple STORE */
                gcl_ir_emit(prog, IR_STRUCT_COPY, 0, 0, target, 0);
            } else {
                gcl_ir_emit(prog, IR_STORE, 0, 0, target, 0);
            }
        }
        break;
    }
    case AST_CALL_EXPR: {
        int argc = node->child_count - 1;
        const char *callee_name = NULL;
        if (node->children[0]) {
            if (node->children[0]->kind == AST_IDENT_EXPR) {
                callee_name = node->children[0]->str_value;
            } else if (node->children[0]->kind == AST_MEMBER_EXPR) {
                GclAstNode *mem = node->children[0];
                if (mem->child_count >= 2 && mem->children[1] &&
                    mem->children[1]->kind == AST_IDENT_EXPR) {
                    callee_name = mem->children[1]->str_value;
                } else {
                    callee_name = mem->str_value;
                }
            }
        }
        /* Part 4/5: buffer.free() → IR_FREE. */
        if (argc == 0 && node->children[0] &&
            node->children[0]->kind == AST_MEMBER_EXPR) {
            GclAstNode *mem = node->children[0];
            if (mem->child_count >= 2 && mem->children[0] &&
                mem->children[0]->kind == AST_IDENT_EXPR &&
                mem->children[0]->str_value &&
                mem->children[1] && mem->children[1]->kind == AST_IDENT_EXPR &&
                mem->children[1]->str_value &&
                strcmp(mem->children[1]->str_value, "free") == 0) {
                gcl_ir_emit(prog, IR_FREE, 0, 0, mem->children[0]->str_value, 0);
                break;
            }
        }
        /* Part 5: free(target);  — function-call form of var.free(). */
        if (argc == 1 && callee_name && strcmp(callee_name, "free") == 0 &&
            node->children[1] && node->children[1]->kind == AST_IDENT_EXPR &&
            node->children[1]->str_value) {
            gcl_ir_emit(prog, IR_FREE, 0, 0, node->children[1]->str_value, 0);
            break;
        }
        /* Part 3: scanf("%type", target); → IR_SCANF */
        if (callee_name && strcmp(callee_name, "scanf") == 0 && argc >= 2) {
            const GclAstNode *fmt = node->children[1];
            const char *fmt_text = fmt ? fmt->str_value : NULL;
            
            if (fmt_text) {
                int var_idx = 2;
                
                for (const char *f = fmt_text; *f && var_idx < node->child_count; f++) {
                    if (*f != '%') continue;
                    if (!f[1] || f[1] == '%') {
                        f++;
                        continue;
                    }
                    
                    int kind = -1;
                    int width = 0;
                    int skip_assign = 0;
                    
                    f++;
                    
                    /* Skip assignment suppression */
                    if (*f == '*') {
                        skip_assign = 1;
                        f++;
                    }
                    
                    /* Parse field width */
                    while (*f >= '0' && *f <= '9') {
                        width = width * 10 + (*f - '0');
                        f++;
                    }
                    
                    /* Parse format specifier */
                    if (*f == 's') kind = 0;
                    else if (*f == 'd' || *f == 'i') kind = 1;
                    else if (*f == 'f' || *f == 'F') kind = 2;
                    else if (*f == 'c') kind = 3;
                    else if (*f == 'o') kind = 9;
                    else if (*f == 'x' || *f == 'X' || *f == 'p') kind = 10;
                    else if (*f == 'h') {
                        f++;
                        if (*f == 'd') kind = 4;
                        else if (*f == 'u') kind = 11;
                        f--;
                    }
                    else if (*f == 'l') {
                        f++;
                        if (*f == 'd') kind = 6;
                        else if (*f == 'u') kind = 12;
                        else if (*f == 'l') {
                            f++;
                            if (*f == 'd') kind = 7;
                            else if (*f == 'u') kind = 8;
                            f--;
                        }
                        else if (*f == 'f') kind = 2;
                        f--;
                    }
                    else if (*f == 'L' && f[1] == 'f') {
                        kind = 2;
                        f++;
                    }
                    else if (*f == '[') {
                        kind = 13;
                        f++;
                        while (*f && *f != ']') f++;
                    }
                    
                    if (kind >= 0 && !skip_assign && var_idx < node->child_count) {
                        const GclAstNode *target = node->children[var_idx];
                        const char *tname = NULL;
                        
                        /* Handle both identifier and &identifier */
                        if (target && target->kind == AST_IDENT_EXPR && target->str_value) {
                            tname = target->str_value;
                        } else if (target && target->kind == AST_UNARY_EXPR && target->child_count > 0 &&
                                   target->children[0] && target->children[0]->kind == AST_IDENT_EXPR) {
                            tname = target->children[0]->str_value;
                        }
                        
                        if (tname) {
                            gcl_ir_emit(prog, IR_SCANF, kind, width, tname, 0);
                            var_idx++;
                        }
                    } else if (skip_assign) {
                        gcl_ir_emit(prog, IR_SCANF, kind, width, "", 0);
                    }
                }
                break;
            }
        }
        if (ir_is_class(prog, callee_name)) {
            /* Part 14: constructor call FATHER() / CHILD(...) */
            gcl_ir_emit(prog, IR_CALL, 0, 0, callee_name, 0);
        } else {
            for (int i = 1; i < node->child_count; i++) {
                gen_expr(prog, node->children[i]);
            }
            gcl_ir_emit(prog, IR_CALL, 0, 0, callee_name, argc);
        }
        break;
    }
    case AST_MEMBER_EXPR:
        gen_expr(prog, node->children[0]);
        if (node->child_count > 1 && node->children[1]) {
            gcl_ir_emit(prog, IR_MEMBER, 0, 0, node->children[1]->str_value, 0);
        }
        break;
    case AST_INDEX_EXPR: {
        const GclAstNode *base = node->children[0];
        if (base && base->kind == AST_INDEX_EXPR && base->children[0] &&
            base->children[0]->kind == AST_IDENT_EXPR) {
            /* Part 14 2D read: grid[i][j] → LOAD grid, i*M, +j, INDEX */
            const char *arr_name = base->children[0]->str_value;
            int cols = ir_array_cols(prog, arr_name);
            if (cols > 0) {
                gcl_ir_emit(prog, IR_LOAD, 0, 0, arr_name, 0);
                gen_expr(prog, base->children[1]);
                gcl_ir_emit(prog, IR_PUSH_INT, cols, 0, NULL, 0);
                gcl_ir_emit(prog, IR_MUL, 0, 0, NULL, 0);
                if (node->child_count > 1) gen_expr(prog, node->children[1]);
                gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_INDEX, 0, 0, NULL, 0);
                break;
            }
        }
        gen_expr(prog, node->children[0]);
        if (node->child_count > 1) gen_expr(prog, node->children[1]);
        gcl_ir_emit(prog, IR_INDEX, 0, 0, NULL, 0);
        break;
    }
    case AST_SIZEOF_EXPR: {
        /* sizeof(type) → push the size in bytes.
         * Basic sizes: int/int32/int64 = 8, int8 = 1, int16 = 2, 
         * float/float64 = 8, float32 = 4, char/gcChar = 1, void* = 8.
         * Pointer modifier (*) scales by 8 per level: int = 8, int* = 8, int** = 8 */
        const char *type_name = node->str_value ? node->str_value : "int";
        int64_t size = 8;  /* default: word size */
        
        if (strncmp(type_name, "int8", 4) == 0) size = 1;
        else if (strncmp(type_name, "int16", 5) == 0) size = 2;
        else if (strncmp(type_name, "int32", 5) == 0) size = 4;
        else if (strncmp(type_name, "int64", 5) == 0) size = 8;
        else if (strncmp(type_name, "uint8", 5) == 0) size = 1;
        else if (strncmp(type_name, "uint16", 6) == 0) size = 2;
        else if (strncmp(type_name, "uint32", 6) == 0) size = 4;
        else if (strncmp(type_name, "uint64", 6) == 0) size = 8;
        else if (strncmp(type_name, "float32", 7) == 0) size = 4;
        else if (strncmp(type_name, "float64", 7) == 0) size = 8;
        else if (strncmp(type_name, "float", 5) == 0) size = 8;
        else if (strncmp(type_name, "double", 6) == 0) size = 8;
        else if (strncmp(type_name, "char", 4) == 0) size = 1;
        else if (strncmp(type_name, "gcChar", 6) == 0) size = 1;
        else if (strcmp(type_name, "void") == 0) size = 1;
        else size = 8;  /* default: int or pointer */
        
        gcl_ir_emit(prog, IR_PUSH_INT, size, 0, NULL, 0);
        break;
    }
    default:
        break;
    }
}

static void gen_stmt(GclIrProgram *prog, const GclAstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case AST_PROGRAM:
        for (int i = 0; i < node->child_count; i++) {
            gen_stmt(prog, node->children[i]);
        }
        break;
    case AST_FUNC_DECL:
        gcl_ir_emit(prog, IR_LABEL, 0, 0, node->str_value, 0);
        for (int i = node->child_count - 1; i >= 0; i--) {
            const GclAstNode *ch = node->children[i];
            if (ch && ch->kind == AST_PARAM && ch->str_value) {
                gcl_ir_emit(prog, IR_STORE, 0, 0, ch->str_value, 0);
            }
        }
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i] && node->children[i]->kind == AST_BLOCK) {
                gen_stmt(prog, node->children[i]);
            }
        }
        gcl_ir_emit(prog, IR_RET, 0, 0, NULL, 0);
        break;
    case AST_BLOCK:
        for (int i = 0; i < node->child_count; i++) {
            gen_stmt(prog, node->children[i]);
        }
        break;
    case AST_VAR_DECL: {
        int rows = node->array_dim > 0 ? node->array_dim : 0;
        int cols = (node->array_dim > 0 && node->int_value > 0) ? (int)node->int_value : 1;
        if (rows > 0) {
            ir_register_array(prog, node->str_value, rows, cols);
        }
        /* Part 5: T *List = gcMalloc(reserve=N, extra=M); → IR_GCMALLOC */
        if (node->child_count > 0 && node->children[0] && node->str_value) {
            int64_t reserve = 0, extra = 0;
            if (ir_parse_malloc_call(node->children[0], &reserve, &extra)) {
                gcl_ir_emit(prog, IR_GCMALLOC, reserve, 0,
                            ir_strdup(prog, node->str_value), (int)extra);
                break;
            }
            gen_expr(prog, node->children[0]);
            gcl_ir_emit(prog, IR_STORE, 0, 0, node->str_value, 0);
        } else if (rows > 0) {
            /* Part 14: uninitialized static array → zero-filled "B;rows;cols;..." */
            int total = rows * cols;
            char buf[512];
            int n = snprintf(buf, sizeof(buf), "\"B;%d;%d", rows, cols);
            for (int k = 0; k < total && n < (int)sizeof(buf) - 3; k++) {
                n += snprintf(buf + n, (size_t)(sizeof(buf) - (size_t)n), ";0");
            }
            if (n < (int)sizeof(buf) - 2) { buf[n++] = '"'; buf[n] = '\0'; }
            gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, ir_strdup(prog, buf), 0);
            gcl_ir_emit(prog, IR_STORE, 0, 0, node->str_value, 0);
        }
        break;
    }
    case AST_EXPR_STMT:
        if (node->child_count > 0) {
            gen_expr(prog, node->children[0]);
            gcl_ir_emit(prog, IR_POP, 0, 0, NULL, 0);
        }
        break;
    case AST_RETURN_STMT:
        if (node->child_count > 0) {
            gen_expr(prog, node->children[0]);
        } else {
            gcl_ir_emit(prog, IR_PUSH_INT, 0, 0, NULL, 0);
        }
        gcl_ir_emit(prog, IR_RET, 0, 0, NULL, 0);
        break;
    case AST_IF_STMT: {
        if (node->child_count >= 2) {
            gen_expr(prog, node->children[0]);
            int jz_idx = gcl_ir_emit(prog, IR_JZ, 0, 0, NULL, 0);
            gen_stmt(prog, node->children[1]);
            if (node->child_count >= 3) {
                int jmp_idx = gcl_ir_emit(prog, IR_JMP, 0, 0, NULL, 0);
                prog->instrs[jz_idx].i_val = prog->count;
                gen_stmt(prog, node->children[2]);
                prog->instrs[jmp_idx].i_val = prog->count;
            } else {
                prog->instrs[jz_idx].i_val = prog->count;
            }
        }
        break;
    }
    case AST_SWITCH_STMT: {
        if (node->child_count >= 1) {
            gen_expr(prog, node->children[0]);
            gcl_ir_emit(prog, IR_STORE, 0, 0, "__switch_val", 0);
            loop_ctx_enter(-1, 1);
            int case_jumps[64];
            int n_jumps = 0;
            int default_clause = -1;
            for (int c = 1; c < node->child_count; c++) {
                const GclAstNode *clause = node->children[c];
                if (!clause) continue;
                if (clause->kind == AST_CASE_CLAUSE && clause->str_value &&
                    strcmp(clause->str_value, "default") == 0) {
                    default_clause = c;
                    continue;
                }
                if (clause->kind != AST_CASE_CLAUSE) continue;
                gcl_ir_emit(prog, IR_LOAD, 0, 0, "__switch_val", 0);
                if (clause->child_count >= 1) {
                    gen_expr(prog, clause->children[0]);
                }
                gcl_ir_emit(prog, IR_EQ, 0, 0, NULL, 0);
                int jz_idx = gcl_ir_emit(prog, IR_JZ, 0, 0, NULL, 0);
                for (int b = 1; b < clause->child_count; b++) {
                    gen_stmt(prog, clause->children[b]);
                }
                if (n_jumps < 64) {
                    case_jumps[n_jumps++] = gcl_ir_emit(prog, IR_JMP, 0, 0, NULL, 0);
                }
                prog->instrs[jz_idx].i_val = prog->count;
            }
            if (default_clause >= 0) {
                const GclAstNode *clause = node->children[default_clause];
                for (int b = 0; b < clause->child_count; b++) {
                    gen_stmt(prog, clause->children[b]);
                }
            }
            for (int k = 0; k < n_jumps; k++) {
                prog->instrs[case_jumps[k]].i_val = prog->count;
            }
            loop_ctx_exit(prog);
        }
        break;
    }
    case AST_WHILE_STMT: {
        if (node->child_count >= 2) {
            int is_do = (node->str_value && strcmp(node->str_value, "do") == 0);
            int loop_start = prog->count;
            loop_ctx_enter(loop_start, 0);
            if (is_do) {
                gen_stmt(prog, node->children[0]);
                g_loop_stack[g_loop_depth - 1].continue_target = prog->count;
                gen_expr(prog, node->children[1]);
                int jz_idx = gcl_ir_emit(prog, IR_JZ, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_JMP, loop_start, 0, NULL, 0);
                prog->instrs[jz_idx].i_val = prog->count;
            } else {
                gen_expr(prog, node->children[0]);
                int jz_idx = gcl_ir_emit(prog, IR_JZ, 0, 0, NULL, 0);
                gen_stmt(prog, node->children[1]);
                gcl_ir_emit(prog, IR_JMP, loop_start, 0, NULL, 0);
                prog->instrs[jz_idx].i_val = prog->count;
            }
            loop_ctx_exit(prog);
        }
        break;
    }
    case AST_FOR_STMT: {
        if (node->child_count >= 4) {
            gen_stmt(prog, node->children[0]);
            int loop_start = prog->count;
            gen_expr(prog, node->children[1]);
            int jz_idx = gcl_ir_emit(prog, IR_JZ, 0, 0, NULL, 0);
            loop_ctx_enter(-1, 0);
            gen_stmt(prog, node->children[3]);
            g_loop_stack[g_loop_depth - 1].continue_target = prog->count;
            gen_expr(prog, node->children[2]);
            gcl_ir_emit(prog, IR_POP, 0, 0, NULL, 0);
            gcl_ir_emit(prog, IR_JMP, loop_start, 0, NULL, 0);
            prog->instrs[jz_idx].i_val = prog->count;
            loop_ctx_exit(prog);
        }
        break;
    }
    case AST_BREAK_STMT:
        emit_break_patch(prog);
        break;
    case AST_CONTINUE_STMT:
        emit_continue_patch(prog);
        break;
    case AST_CLASS_DECL:
        /* Part 9/9b: class with inheritance & head() constructor.
         * Register class name, emit constructor label:
         * - If has parent: call parent constructor (e.g., "FATHER" → push that instance)
         * - Scan for head() function: if found, it's the constructor, store params in instance "S;name=value;age=value;"
         * - Compile methods as "<Class>_<Method>" global functions. */
        if (node->str_value && prog->class_count < GCL_IR_CLASS_MAX) {
            const char *cls = ir_strdup(prog, node->str_value);
            int known = 0;
            for (int i = 0; i < prog->class_count; i++) {
                if (strcmp(prog->class_names[i], cls) == 0) { known = 1; break; }
            }
            if (!known) prog->class_names[prog->class_count++] = cls;
            
            gcl_ir_emit(prog, IR_LABEL, 0, 0, ir_strdup(prog, cls), 0);
            
            /* Part 9: if parent class exists, call parent constructor */
            if (node->type_name && strcmp(node->type_name, "") != 0) {
                /* call parent: IR_CALL parent, which leaves instance on stack */
                gcl_ir_emit(prog, IR_CALL, 0, 0, ir_strdup(prog, node->type_name), 0);
            } else {
                /* no parent: start with empty instance */
                gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, ir_strdup(prog, "S;"), 0);
            }
            
            /* Part 9b: scan for head() function to collect constructor params */
            GclAstNode *head_func = NULL;
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i] && node->children[i]->kind == AST_FUNC_DECL &&
                    node->children[i]->str_value) {
                    const char *fn = node->children[i]->str_value;
                    /* head() is registered as "<Class>_head", check base name */
                    size_t cls_len = strlen(cls);
                    if (strlen(fn) > cls_len + 1 && strncmp(fn, cls, cls_len) == 0 &&
                        fn[cls_len] == '_' && strcmp(fn + cls_len + 1, "head") == 0) {
                        head_func = node->children[i];
                        break;
                    }
                }
            }
            
            /* If head() found, scan its parameters and store in instance "S;name=value;..." */
            if (head_func && head_func->child_count > 0) {
                /* head_func->children[0..] are AST_PARAM nodes */
                for (int i = 0; i < head_func->child_count; i++) {
                    const GclAstNode *p = head_func->children[i];
                    if (p && p->kind == AST_PARAM && p->str_value) {
                        const char *param_name = p->str_value;
                        /* LOAD param_name (its value was passed in) */
                        gcl_ir_emit(prog, IR_LOAD, 0, 0, ir_strdup(prog, param_name), 0);
                        /* Append to instance: instance + ";" + param_name + "=" + value 
                         * Stack: [instance, value]
                         * Append ";name=" string, then ADD */
                        char append_buf[64];
                        snprintf(append_buf, sizeof(append_buf), ";%s=", param_name);
                        gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, ir_strdup(prog, append_buf), 0);
                        gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);  /* instance + ";name=" */
                        gcl_ir_emit(prog, IR_ADD, 0, 0, NULL, 0);  /* ... + value */
                    }
                }
            }
            
            gcl_ir_emit(prog, IR_RET, 0, 0, NULL, 0);
        }
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i] && node->children[i]->kind == AST_FUNC_DECL) {
                gen_stmt(prog, node->children[i]);
            }
        }
        break;
    case AST_PP_IF:
        for (int i = 0; i < node->child_count; i++) {
            gen_stmt(prog, node->children[i]);
        }
        break;
    case AST_PP_INCLUDE:
    case AST_PP_EXTERN:
    case AST_PP_ERROR:
    case AST_PP_WARNING:
    case AST_PP_DEBUG:
        break;
    default:
        break;
    }
}

void gcl_ir_gen(GclIrProgram *prog, const GclAstNode *ast) {
    g_loop_depth = 0;
    gen_stmt(prog, ast);
    gcl_ir_emit(prog, IR_HALT, 0, 0, NULL, 0);
}


