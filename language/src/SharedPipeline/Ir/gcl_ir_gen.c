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
 * Returns 1 when node is AST_CALL_EXPR targeting gcMalloc/malloc. */
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
        if (!av || av->kind != AST_INT_LIT) continue;
        if (strcmp(an->str_value, "reserve") == 0) *reserve = av->int_value;
        else if (strcmp(an->str_value, "extra") == 0) *extra = av->int_value;
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
                gcl_ir_emit(prog, IR_LOAD, 0, 0, target->str_value, 0);
                gcl_ir_emit(prog, IR_PUSH_INT, 1, 0, NULL, 0);
                gcl_ir_emit(prog, strcmp(node->str_value, "++") == 0 ? IR_ADD : IR_SUB, 0, 0, NULL, 0);
                gcl_ir_emit(prog, IR_STORE, 0, 0, target->str_value, 0);
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
            gcl_ir_emit(prog, IR_STORE, 0, 0, target, 0);
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
        if (argc == 2 && callee_name && strcmp(callee_name, "scanf") == 0) {
            const GclAstNode *fmt = node->children[1];
            const char *fmt_text = fmt ? fmt->str_value : NULL;
            int kind = -1;
            if (fmt_text) {
                for (const char *f = fmt_text; *f; f++) {
                    if (*f != '%') continue;
                    if (f[1] == 's') { kind = 0; break; }
                    if (f[1] == 'd' || f[1] == 'i' || f[1] == 'u') { kind = 1; break; }
                    if (f[1] == 'f' || f[1] == 'F') { kind = 2; break; }
                    if (f[1] == 'c') { kind = 3; break; }
                }
            }
            const GclAstNode *target = argc >= 2 ? node->children[2] : NULL;
            const char *tname = NULL;
            if (target && target->kind == AST_IDENT_EXPR) tname = target->str_value;
            if (kind >= 0 && tname) {
                gcl_ir_emit(prog, IR_SCANF, kind, 0, tname, 0);
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
        /* Part 14/15: class — register class name, emit constructor label
         * that pushes an empty instance "S;", compile methods as
         * "<Class>_<Method>" global functions. */
        if (node->str_value && prog->class_count < GCL_IR_CLASS_MAX) {
            const char *cls = ir_strdup(prog, node->str_value);
            int known = 0;
            for (int i = 0; i < prog->class_count; i++) {
                if (strcmp(prog->class_names[i], cls) == 0) { known = 1; break; }
            }
            if (!known) prog->class_names[prog->class_count++] = cls;
            gcl_ir_emit(prog, IR_LABEL, 0, 0, ir_strdup(prog, cls), 0);
            gcl_ir_emit(prog, IR_PUSH_STRING, 0, 0, ir_strdup(prog, "S;"), 0);
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
