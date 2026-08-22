#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include "../SharedPipeline/Ir/gcl_ir.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* InterpState structure definition */
typedef struct {
    void *stack_ptr;  /* unused in this module */
    int sp;
    void *vars_ptr;   /* unused */
    int var_count;
    int pc;
    int halted;
    void *dlls_ptr;   /* unused */
    int dll_count;
    void *externs_ptr;  /* unused */
    int extern_count;
    const char *source_dir;
    void *current_prog;
    int call_stack[256];
    int call_depth;
    int in_call;
    void *elem_buf_ptr;  /* unused */
    void *string_pool_ptr;  /* unused */
    int string_pool_count;
} InterpState;

typedef struct { int kind; int64_t i_val; double f_val; const char *s_val; } Val;

extern Val val_int(int64_t v);
extern Val val_float(double v);
extern Val val_string(const char *s);
extern Val val_null(void);
extern Val pop(InterpState *st);
extern void push(InterpState *st, Val v);
extern int64_t val_to_int(Val v);
extern void store_var(InterpState *st, const char *name, Val val);
extern const char *pool_string(InterpState *st, const char *s);
extern void *interp_get_proc(InterpState *st, const char *func_name);

/* FFI function pointers */
typedef int64_t (*ffi_void_fn)(void);
typedef int64_t (*ffi_1_fn)(int64_t);
typedef int64_t (*ffi_2_fn)(int64_t, int64_t);
typedef int64_t (*ffi_3_fn)(int64_t, int64_t, int64_t);
typedef int64_t (*ffi_4_fn)(int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_5_fn)(int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_6_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static const char *unquote(const char *s, char *buf, size_t bufsz) {
    if (!s) return "";
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len-1] == '"') {
        size_t n = len - 2;
        if (n >= bufsz) n = bufsz - 1;
        memcpy(buf, s + 1, n);
        buf[n] = '\0';
        return buf;
    }
    return s;
}

Val call_extern_ffi(InterpState *st, void *proc, int argc) {
    int64_t a[6] = {0};
    char strbuf[6][512];

    Val args[6];
    int n = argc < 6 ? argc : 6;
    for (int i = n - 1; i >= 0; i--) {
        args[i] = pop(st);
    }

    for (int i = 0; i < n; i++) {
        if (args[i].kind == 3) {  /* VAL_STRING */
            const char *s = unquote(args[i].s_val, strbuf[i], sizeof(strbuf[i]));
            a[i] = (int64_t)(intptr_t)s;
        } else {
            a[i] = val_to_int(args[i]);
        }
    }

    int64_t ret = 0;
    switch (n) {
    case 0: ret = ((ffi_void_fn)proc)(); break;
    case 1: ret = ((ffi_1_fn)proc)(a[0]); break;
    case 2: ret = ((ffi_2_fn)proc)(a[0], a[1]); break;
    case 3: ret = ((ffi_3_fn)proc)(a[0], a[1], a[2]); break;
    case 4: ret = ((ffi_4_fn)proc)(a[0], a[1], a[2], a[3]); break;
    case 5: ret = ((ffi_5_fn)proc)(a[0], a[1], a[2], a[3], a[4]); break;
    case 6: ret = ((ffi_6_fn)proc)(a[0], a[1], a[2], a[3], a[4], a[5]); break;
    }
    return val_int(ret);
}

int call_user_function(InterpState *st, const char *name, int argc) {
    (void)argc;
    if (!name || !st || !st->current_prog) return 0;
    const GclIrProgram *prog = st->current_prog;
    for (int i = 0; i < prog->count; i++) {
        if (prog->instrs[i].op == IR_LABEL &&
            prog->instrs[i].s_val && strcmp(prog->instrs[i].s_val, name) == 0) {
            if (st->call_depth < 256) {
                st->call_stack[st->call_depth++] = st->pc;
                st->in_call = 1;
                st->pc = i + 1;
                return 1;
            }
            fprintf(stderr, "gcl: interp: call depth exceeded for %s\n", name);
            break;
        }
    }
    if (name && name[0]) {
        size_t nl = strlen(name);
        for (int i = 0; i < prog->count; i++) {
            const GclIrInstr *li = &prog->instrs[i];
            if (li->op != IR_LABEL || !li->s_val) continue;
            size_t ll = strlen(li->s_val);
            if (ll > nl + 1 && li->s_val[ll - nl - 1] == '_' &&
                strcmp(li->s_val + ll - nl, name) == 0) {
                if (st->call_depth < 256) {
                    st->call_stack[st->call_depth++] = st->pc;
                    st->in_call = 1;
                    st->pc = i + 1;
                    return 2;
                }
                break;
            }
        }
    }
    return 0;
}

Val call_function(InterpState *st, const char *name, int argc) {
    if (!name) return val_null();

    if (strcmp(name, "printf") == 0) {
        extern void interp_printf_module(void *st, int argc);
        interp_printf_module(st, argc);
        return val_null();
    }
    
    if (strcmp(name, "strlen") == 0) {
        Val s = pop(st);
        const char *str = (s.kind == 3) ? s.s_val : "";
        if (str && str[0] == '"') {
            size_t len = strlen(str);
            if (len >= 2) return val_int((int64_t)(len - 2));
        }
        return val_int((int64_t)strlen(str));
    }
    
    if (strcmp(name, "strcmp") == 0) {
        Val b = pop(st), a = pop(st);
        const char *sa = (a.kind == 3) ? a.s_val : "";
        const char *sb = (b.kind == 3) ? b.s_val : "";
        return val_int((int64_t)strcmp(sa, sb));
    }
    
    if (strcmp(name, "abs") == 0) {
        Val v = pop(st);
        int64_t val = val_to_int(v);
        return val_int(val < 0 ? -val : val);
    }
    
    if (strcmp(name, "sqrt") == 0) {
        Val v = pop(st);
        double d = (v.kind == 2) ? v.f_val : (double)val_to_int(v);
        return val_float(d < 0 ? 0.0 : sqrt(d));
    }
    
    if (strcmp(name, "sin") == 0) {
        Val v = pop(st);
        double d = (v.kind == 2) ? v.f_val : (double)val_to_int(v);
        return val_float(sin(d));
    }
    
    if (strcmp(name, "cos") == 0) {
        Val v = pop(st);
        double d = (v.kind == 2) ? v.f_val : (double)val_to_int(v);
        return val_float(cos(d));
    }
    
    if (strcmp(name, "pow") == 0) {
        Val exp = pop(st), base = pop(st);
        double b = (base.kind == 2) ? base.f_val : (double)val_to_int(base);
        double e = (exp.kind == 2) ? exp.f_val : (double)val_to_int(exp);
        return val_float(pow(b, e));
    }
    
    if (strcmp(name, "sizeof") == 0) {
        return val_int(8);
    }

    void *proc = interp_get_proc(st, name);
    if (proc) {
        return call_extern_ffi(st, proc, argc);
    }

    for (int i = 0; i < argc; i++) pop(st);
    return val_null();
}
