#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "../SharedPipeline/Ir/gcl_ir.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Stack-based IR interpreter */

#define INTERP_STACK_MAX 1024
#define INTERP_VARS_MAX  512
#define INTERP_DLL_MAX   16
#define INTERP_EXTERN_MAX 128
#define INTERP_CALL_MAX  256

typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING
} ValKind;

typedef struct {
    ValKind     kind;
    int64_t     i_val;
    double      f_val;
    const char *s_val;
} Val;

typedef struct {
    const char *name;
    Val         value;
    int         is_const;  /* 1 if variable is const, 0 otherwise */
} VarSlot;

/* DLL function registry */
typedef struct {
    const char *name;
    void       *proc;
} ExternFunc;

typedef struct {
#ifdef _WIN32
    HMODULE handle;
#else
    void   *handle;
#endif
    char    path[512];
} DllEntry;

typedef struct {
    Val         stack[INTERP_STACK_MAX];
    int         sp;
    VarSlot     vars[INTERP_VARS_MAX];
    int         var_count;
    int         pc;
    int         halted;
    DllEntry    dlls[INTERP_DLL_MAX];
    int         dll_count;
    ExternFunc  externs[INTERP_EXTERN_MAX];
    int         extern_count;
    const char *source_dir;
    /* User-defined GCL function call support */
    const GclIrProgram *current_prog;
    int         call_stack[INTERP_CALL_MAX];
    int         call_depth;
    int         in_call;
    /* Scratch buffer for dynamically produced values (char elements, struct members, tuple elems) */
    char        elem_buf[256];
    /* String pool: copies of produced strings so stored values survive */
    char        string_pool[64][256];
    int         string_pool_count;
} InterpState;

static void push(InterpState *st, Val v) {
    if (st->sp >= INTERP_STACK_MAX) {
        fprintf(stderr, "gcl: interp: stack overflow\n");
        st->halted = 1;
        return;
    }
    st->stack[st->sp++] = v;
}

Val pop(InterpState *st) {
    if (st->sp <= 0) {
        Val v = {VAL_NULL, 0, 0, NULL};
        return v;
    }
    return st->stack[--st->sp];
}

Val val_int(int64_t v) { Val r = {VAL_INT, v, 0, NULL}; return r; }
Val val_float(double v) { Val r = {VAL_FLOAT, 0, v, NULL}; return r; }
Val val_string(const char *s) { Val r = {VAL_STRING, 0, 0, s}; return r; }
static Val val_null(void) { Val r = {VAL_NULL, 0, 0, NULL}; return r; }

/* Copies a produced string into the stable string pool (bounded ring-ish).
 * If the pool is full, the last slot is reused. */
static const char *pool_string(InterpState *st, const char *s) {
    if (!s) return NULL;
    size_t sl = strlen(s);
    if (sl >= 256) sl = 255;
    int slot = st->string_pool_count;
    if (slot >= 64) slot = 63;
    else st->string_pool_count++;
    memcpy(st->string_pool[slot], s, sl);
    st->string_pool[slot][sl] = '\0';
    return st->string_pool[slot];
}

/* Count UTF-8 characters (not bytes) */
static int64_t utf8_strlen(const char *s) {
    if (!s) return 0;
    int64_t count = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        /* Count bytes that are not UTF-8 continuation bytes (10xxxxxx) */
        if ((*p & 0xC0) != 0x80) count++;
    }
    return count;
}

int64_t val_to_int(Val v) {
    switch (v.kind) {
    case VAL_INT: return v.i_val;
    case VAL_FLOAT: return (int64_t)v.f_val;
    case VAL_STRING: return v.s_val ? utf8_strlen(v.s_val) : 0;  /* UTF-8 character count */
    case VAL_NULL: return 0;
    }
    return 0;
}

static int val_truthy(Val v) {
    switch (v.kind) {
    case VAL_NULL: return 0;
    case VAL_INT: return v.i_val != 0;
    case VAL_FLOAT: return v.f_val != 0.0;
    case VAL_STRING: return v.s_val != NULL && v.s_val[0] != '\0';
    }
    return 0;
}

static VarSlot *find_var(InterpState *st, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < st->var_count; i++) {
        if (st->vars[i].name && strcmp(st->vars[i].name, name) == 0) {
            return &st->vars[i];
        }
    }
    return NULL;
}

void store_var(InterpState *st, const char *name, Val val) {
    VarSlot *slot = find_var(st, name);
    if (!slot) {
        if (st->var_count >= INTERP_VARS_MAX) {
            fprintf(stderr, "gcl: interp: too many variables\n");
            return;
        }
        slot = &st->vars[st->var_count];
        st->vars[st->var_count].name = name;
        st->vars[st->var_count].is_const = 0;  /* New variables not const by default */
        st->var_count++;
    } else if (slot->is_const) {
        /* Const variable - prevent modification */
        fprintf(stderr, "gcl: error: cannot assign to const variable '%s'\n", name);
        return;
    }
    /* Strings that point into the scratch buffer must be copied to the pool */
    if (val.kind == VAL_STRING && val.s_val) {
        val.s_val = pool_string(st, val.s_val);
    }
    slot->value = val;
    
    /* Part 8: Struct initialization - when "S;field=value;..." is stored,
     * also store individual fields as Name.field = value for transparency */
    if (val.kind == VAL_STRING && val.s_val && strlen(val.s_val) > 2 &&
        val.s_val[0] == 'S' && val.s_val[1] == ';') {
        const char *p = val.s_val + 2;
        const char *end = val.s_val + strlen(val.s_val);
        char field_name[512];
        while (p < end) {
            const char *eq = strchr(p, '=');
            if (!eq || eq >= end) break;
            size_t fname_len = (size_t)(eq - p);
            if (fname_len > 0 && fname_len < 256) {
                memcpy(field_name, p, fname_len);
                field_name[fname_len] = '\0';
                const char *val_start = eq + 1;
                const char *val_end = strchr(val_start, ';');
                if (!val_end || val_end > end) val_end = end;
                size_t vlen = (size_t)(val_end - val_start);
                
                /* Store field value - crude but functional */
                if (vlen > 0 && vlen < 256) {
                    char fbuf[512];
                    snprintf(fbuf, sizeof(fbuf), "%s.%s", name, field_name);
                    char vbuf[256];
                    memcpy(vbuf, val_start, vlen);
                    vbuf[vlen] = '\0';
                    
                    Val fval;
                    if (strchr(vbuf, '.') != NULL) {
                        fval = val_float(atof(vbuf));
                    } else {
                        fval = val_int(strtoll(vbuf, NULL, 10));
                    }
                    store_var(st, fbuf, fval);
                }
            }
            const char *next = strchr(eq, ';');
            if (!next || next >= end) break;
            p = next + 1;
        }
    }
}

static Val load_var(InterpState *st, const char *name) {
    VarSlot *slot = find_var(st, name);
    if (slot) return slot->value;
    return val_null();
}

/* Parses an unsigned decimal integer at s (up to e); advances s past digits. */
static const char *interp_parse_num(const char *s, const char *e, int64_t *out) {
    int64_t v = 0;
    int digit = 0;
    while (s < e && *s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); digit = 1; s++; }
    *out = digit ? v : 0;
    return s;
}

/* DLL Loading */

static void *interp_load_dll(InterpState *st, const char *dll_name) {
    for (int i = 0; i < st->dll_count; i++) {
        if (strcmp(st->dlls[i].path, dll_name) == 0) {
#ifdef _WIN32
            return (void *)st->dlls[i].handle;
#else
            return st->dlls[i].handle;
#endif
        }
    }
    if (st->dll_count >= INTERP_DLL_MAX) return NULL;

    char full_path[4096];
    if (st->source_dir && st->source_dir[0]) {
        snprintf(full_path, sizeof(full_path), "%s%s", st->source_dir, dll_name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", dll_name);
    }

#ifdef _WIN32
    HMODULE h = LoadLibraryA(full_path);
    if (!h) {
        h = LoadLibraryA(dll_name);
    }
    if (!h) {
        fprintf(stderr, "gcl: interp: cannot load DLL: %s\n", full_path);
        return NULL;
    }
    st->dlls[st->dll_count].handle = h;
#else
    void *h = dlopen(full_path, RTLD_NOW);
    if (!h) h = dlopen(dll_name, RTLD_NOW);
    if (!h) {
        fprintf(stderr, "gcl: interp: cannot load .so: %s\n", full_path);
        return NULL;
    }
    st->dlls[st->dll_count].handle = h;
#endif
    size_t nlen = strlen(dll_name);
    if (nlen >= sizeof(st->dlls[st->dll_count].path))
        nlen = sizeof(st->dlls[st->dll_count].path) - 1;
    memcpy(st->dlls[st->dll_count].path, dll_name, nlen);
    st->dlls[st->dll_count].path[nlen] = '\0';
    st->dll_count++;
    return (void *)h;
}

static void *interp_get_proc(InterpState *st, const char *func_name) {
    for (int i = 0; i < st->extern_count; i++) {
        if (st->externs[i].name && strcmp(st->externs[i].name, func_name) == 0) {
            return st->externs[i].proc;
        }
    }
    for (int i = 0; i < st->dll_count; i++) {
#ifdef _WIN32
        FARPROC proc = GetProcAddress(st->dlls[i].handle, func_name);
#else
        void *proc = dlsym(st->dlls[i].handle, func_name);
#endif
        if (proc) {
            if (st->extern_count < INTERP_EXTERN_MAX) {
                st->externs[st->extern_count].name = func_name;
                st->externs[st->extern_count].proc = (void *)proc;
                st->extern_count++;
            }
            return (void *)proc;
        }
    }
    return NULL;
}

static void interp_unload_dlls(InterpState *st) {
    for (int i = 0; i < st->dll_count; i++) {
#ifdef _WIN32
        if (st->dlls[i].handle) FreeLibrary(st->dlls[i].handle);
#else
        if (st->dlls[i].handle) dlclose(st->dlls[i].handle);
#endif
    }
    st->dll_count = 0;
    st->extern_count = 0;
}

/* Extract unquoted string value */

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

/* Extern FFI call (simple: up to 6 int/ptr args) */

typedef int64_t (*ffi_void_fn)(void);
typedef int64_t (*ffi_1_fn)(int64_t);
typedef int64_t (*ffi_2_fn)(int64_t, int64_t);
typedef int64_t (*ffi_3_fn)(int64_t, int64_t, int64_t);
typedef int64_t (*ffi_4_fn)(int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_5_fn)(int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*ffi_6_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static Val call_extern_ffi(InterpState *st, void *proc, int argc) {
    int64_t a[6] = {0};
    char strbuf[6][512];

    Val args[6];
    int n = argc < 6 ? argc : 6;
    for (int i = n - 1; i >= 0; i--) {
        args[i] = pop(st);
    }

    for (int i = 0; i < n; i++) {
        if (args[i].kind == VAL_STRING) {
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

/* User-defined GCL function call */
static int call_user_function(InterpState *st, const char *name, int argc) {
    (void)argc;
    if (!name || !st->current_prog) return 0;
    const GclIrProgram *prog = st->current_prog;
    for (int i = 0; i < prog->count; i++) {
        if (prog->instrs[i].op == IR_LABEL &&
            prog->instrs[i].s_val && strcmp(prog->instrs[i].s_val, name) == 0) {
            if (st->call_depth < INTERP_CALL_MAX) {
                st->call_stack[st->call_depth++] = st->pc;
                st->in_call = 1;
                st->pc = i + 1;
                return 1;
            }
            fprintf(stderr, "gcl: interp: call depth exceeded for %s\n", name);
            break;
        }
    }
    /* Part 15: class method fallback — instance.method() where the IR
     * registered the method as "<Class>_<method>". Try to find any label
     * ending with "_<name>". */
    if (name && name[0]) {
        size_t nl = strlen(name);
        for (int i = 0; i < prog->count; i++) {
            const GclIrInstr *li = &prog->instrs[i];
            if (li->op != IR_LABEL || !li->s_val) continue;
            size_t ll = strlen(li->s_val);
            if (ll > nl + 1 && li->s_val[ll - nl - 1] == '_' &&
                strcmp(li->s_val + ll - nl, name) == 0) {
                if (st->call_depth < INTERP_CALL_MAX) {
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

static Val call_function(InterpState *st, const char *name, int argc) {
    if (!name) return val_null();

    if (strcmp(name, "printf") == 0) {
        extern void interp_printf_module(void *st, int argc);
        interp_printf_module(st, argc);
        return val_null();
    }
    
    /* String functions */
    if (strcmp(name, "strlen") == 0) {
        Val s = pop(st);
        const char *str = (s.kind == VAL_STRING) ? s.s_val : "";
        if (str && str[0] == '"') {
            /* Remove quotes */
            size_t len = strlen(str);
            if (len >= 2) return val_int((int64_t)(len - 2));
        }
        return val_int((int64_t)strlen(str));
    }
    
    if (strcmp(name, "strcmp") == 0) {
        Val b = pop(st), a = pop(st);
        const char *sa = (a.kind == VAL_STRING) ? a.s_val : "";
        const char *sb = (b.kind == VAL_STRING) ? b.s_val : "";
        return val_int((int64_t)strcmp(sa, sb));
    }
    
    if (strcmp(name, "strcpy") == 0) {
        Val src = pop(st), dst = pop(st);
        const char *s = (src.kind == VAL_STRING) ? src.s_val : "";
        if (dst.kind == VAL_STRING) {
            char buf[512];
            size_t slen = strlen(s);
            if (slen < sizeof(buf) - 3) {
                snprintf(buf, sizeof(buf), "\"%s\"", s);
                store_var(st, (const char*)dst.s_val, val_string(pool_string(st, buf)));
            }
        }
        return val_string(s);
    }
    
    if (strcmp(name, "strcat") == 0) {
        Val b = pop(st), a = pop(st);
        const char *sa = (a.kind == VAL_STRING) ? a.s_val : "";
        const char *sb = (b.kind == VAL_STRING) ? b.s_val : "";
        char buf[512];
        snprintf(buf, sizeof(buf), "%s%s", sa, sb);
        return val_string(pool_string(st, buf));
    }

    /* Memory functions */
    if (strcmp(name, "malloc") == 0 || strcmp(name, "gcMalloc") == 0) {
        Val size = pop(st);
        int64_t sz = val_to_int(size);
        if (sz <= 0) sz = 1;
        char buf[512];
        snprintf(buf, sizeof(buf), "\"PTR:%lld\"", (long long)sz);
        return val_string(pool_string(st, buf));
    }
    
    if (strcmp(name, "calloc") == 0) {
        Val size = pop(st);
        Val count = pop(st);
        int64_t total = val_to_int(count) * val_to_int(size);
        if (total <= 0) total = 1;
        char buf[512];
        snprintf(buf, sizeof(buf), "\"PTR:%lld\"", (long long)total);
        return val_string(pool_string(st, buf));
    }
    
    if (strcmp(name, "realloc") == 0) {
        Val newsize = pop(st);
        pop(st);  /* discard old ptr */
        int64_t sz = val_to_int(newsize);
        if (sz <= 0) sz = 1;
        char buf[512];
        snprintf(buf, sizeof(buf), "\"PTR:%lld\"", (long long)sz);
        return val_string(pool_string(st, buf));
    }
    
    if (strcmp(name, "memcpy") == 0 || strcmp(name, "memset") == 0 || 
        strcmp(name, "memmove") == 0 || strcmp(name, "memcmp") == 0) {
        /* Dummy implementations - just consume arguments */
        for (int i = 0; i < argc; i++) pop(st);
        return val_int(0);
    }
    
    /* Math functions */
    if (strcmp(name, "abs") == 0) {
        Val v = pop(st);
        int64_t val = val_to_int(v);
        return val_int(val < 0 ? -val : val);
    }
    
    if (strcmp(name, "sqrt") == 0) {
        Val v = pop(st);
        double d = (v.kind == VAL_FLOAT) ? v.f_val : (double)val_to_int(v);
        return val_float(d < 0 ? 0.0 : sqrt(d));
    }
    
    if (strcmp(name, "sin") == 0) {
        Val v = pop(st);
        double d = (v.kind == VAL_FLOAT) ? v.f_val : (double)val_to_int(v);
        return val_float(sin(d));
    }
    
    if (strcmp(name, "cos") == 0) {
        Val v = pop(st);
        double d = (v.kind == VAL_FLOAT) ? v.f_val : (double)val_to_int(v);
        return val_float(cos(d));
    }
    
    if (strcmp(name, "pow") == 0) {
        Val exp = pop(st), base = pop(st);
        double b = (base.kind == VAL_FLOAT) ? base.f_val : (double)val_to_int(base);
        double e = (exp.kind == VAL_FLOAT) ? exp.f_val : (double)val_to_int(exp);
        return val_float(pow(b, e));
    }
    
    if (strcmp(name, "sprintf") == 0 || strcmp(name, "snprintf") == 0) {
        /* Simplified: just format and return string */
        Val fmt = pop(st);
        const char *f = (fmt.kind == VAL_STRING) ? fmt.s_val : "";
        char buf[512];
        snprintf(buf, sizeof(buf), "\"%s\"", f);
        return val_string(pool_string(st, buf));
    }
    
    /* Overflow detection helpers */
    if (strcmp(name, "_check_add_overflow") == 0) {
        /* Check if a + b overflows */
        Val b = pop(st), a = pop(st);
        int64_t av = val_to_int(a);
        int64_t bv = val_to_int(b);
        if ((bv > 0 && av > INT64_MAX - bv) || (bv < 0 && av < INT64_MIN - bv)) {
            fprintf(stdout, "\x1b[31mgcl: error: integer overflow in addition\x1b[0m\n");
            return val_int(1);
        }
        return val_int(0);
    }
    
    if (strcmp(name, "_check_mul_overflow") == 0) {
        /* Check if a * b overflows */
        Val b = pop(st), a = pop(st);
        int64_t av = val_to_int(a);
        int64_t bv = val_to_int(b);
        if (av != 0 && bv != 0) {
            if ((av > 0 && bv > 0 && av > INT64_MAX / bv) ||
                (av < 0 && bv < 0 && av < INT64_MAX / bv) ||
                (av > 0 && bv < 0 && bv < INT64_MIN / av) ||
                (av < 0 && bv > 0 && av < INT64_MIN / bv)) {
                fprintf(stdout, "\x1b[31mgcl: error: integer overflow in multiplication\x1b[0m\n");
                return val_int(1);
            }
        }
        return val_int(0);
    }
    
    if (strcmp(name, "sizeof") == 0) {
        /* sizeof always returns 8 (generic size) */
        return val_int(8);
    }

    /* File I/O functions */
    if (strcmp(name, "fopen") == 0) {
        extern void interp_fopen(void *st, const char *filename, const char *mode);
        Val mode = pop(st), filename = pop(st);
        const char *fn = (filename.kind == VAL_STRING) ? filename.s_val : "";
        const char *md = (mode.kind == VAL_STRING) ? mode.s_val : "r";
        interp_fopen(st, fn, md);
        return val_null();
    }
    
    if (strcmp(name, "fclose") == 0) {
        extern void interp_fclose(void *st, int64_t handle);
        Val handle = pop(st);
        interp_fclose(st, val_to_int(handle));
        return val_null();
    }
    
    if (strcmp(name, "fprintf") == 0) {
        extern void interp_fprintf(void *st, int64_t handle, const char *format, int argc);
        Val fmt = pop(st), handle = pop(st);
        const char *f = (fmt.kind == VAL_STRING) ? fmt.s_val : "";
        interp_fprintf(st, val_to_int(handle), f, argc - 2);
        return val_null();
    }
    
    if (strcmp(name, "fscanf") == 0) {
        extern void interp_fscanf(void *st, int64_t handle, const char *format);
        Val fmt = pop(st), handle = pop(st);
        const char *f = (fmt.kind == VAL_STRING) ? fmt.s_val : "";
        interp_fscanf(st, val_to_int(handle), f);
        return val_null();
    }
    
    if (strcmp(name, "fread") == 0) {
        extern int64_t interp_fread(void *st, int64_t handle, int64_t size, int64_t count);
        Val count = pop(st), size = pop(st), handle = pop(st);
        return val_int(interp_fread(st, val_to_int(handle), val_to_int(size), val_to_int(count)));
    }
    
    if (strcmp(name, "fwrite") == 0) {
        extern int64_t interp_fwrite(void *st, int64_t handle, const char *data, int64_t size);
        Val size = pop(st), data = pop(st), handle = pop(st);
        const char *d = (data.kind == VAL_STRING) ? data.s_val : "";
        return val_int(interp_fwrite(st, val_to_int(handle), d, val_to_int(size)));
    }
    
    if (strcmp(name, "fgets") == 0) {
        extern void interp_fgets(void *st, const char *var_name, int64_t handle, int64_t max_len);
        Val maxlen = pop(st), handle = pop(st), varname = pop(st);
        const char *vn = (varname.kind == VAL_STRING) ? varname.s_val : "";
        interp_fgets(st, vn, val_to_int(handle), val_to_int(maxlen));
        return val_null();
    }
    
    if (strcmp(name, "fputs") == 0) {
        extern int64_t interp_fputs(void *st, const char *text, int64_t handle);
        Val handle = pop(st), text = pop(st);
        const char *t = (text.kind == VAL_STRING) ? text.s_val : "";
        return val_int(interp_fputs(st, t, val_to_int(handle)));
    }
    
    if (strcmp(name, "fseek") == 0) {
        extern int64_t interp_fseek(void *st, int64_t handle, int64_t offset, int64_t whence);
        Val whence = pop(st), offset = pop(st), handle = pop(st);
        return val_int(interp_fseek(st, val_to_int(handle), val_to_int(offset), val_to_int(whence)));
    }
    
    if (strcmp(name, "ftell") == 0) {
        extern int64_t interp_ftell(void *st, int64_t handle);
        Val handle = pop(st);
        return val_int(interp_ftell(st, val_to_int(handle)));
    }

    void *proc = interp_get_proc(st, name);
    if (proc) {
        return call_extern_ffi(st, proc, argc);
    }

    for (int i = 0; i < argc; i++) pop(st);
    return val_null();
}

static void interp_process_extern(InterpState *st, const GclIrProgram *prog) {
    (void)prog;
    (void)st;
}

/* Resolve a struct/dict member from an encoded string "S;name=value;..." / "D;name=value;...".
 * Returns 1 and pushes the member value, or 0 if not found/not a container. */
static int struct_member_value(InterpState *st, const char *encoded, const char *member) {
    if (!encoded || !member) return 0;
    size_t elen = strlen(encoded);
    if (elen < 2 || (encoded[0] != 'S' && encoded[0] != 'D') || encoded[1] != ';') return 0;
    const char *p = encoded + 2;
    const char *end = encoded + elen;
    while (p < end) {
        const char *eq = strchr(p, '=');
        if (!eq || eq >= end) break;
        size_t name_len = (size_t)(eq - p);
        if (name_len == strlen(member) && memcmp(p, member, name_len) == 0) {
            const char *val_start = eq + 1;
            const char *val_end = strchr(val_start, ';');
            if (!val_end || val_end > end) val_end = end;
            size_t vlen = (size_t)(val_end - val_start);
            if (vlen == 0) { push(st, val_null()); return 1; }
            /* copy the exact value into the scratch buffer so later
             * members/components cannot corrupt it */
            size_t cp = vlen < 255 ? vlen : 255;
            memcpy(st->elem_buf, val_start, cp);
            st->elem_buf[cp] = '\0';
            const char *clean = st->elem_buf;
            if (vlen >= 2 && val_start[0] == '"') {
                push(st, val_string(clean));
                return 1;
            }
            if (vlen >= 2 && val_start[0] == '\'') {
                if (vlen >= 3) {
                    st->elem_buf[0] = '\'';
                    st->elem_buf[1] = val_start[1];
                    st->elem_buf[2] = '\'';
                    st->elem_buf[3] = '\0';
                    push(st, val_string(st->elem_buf));
                    return 1;
                }
                push(st, val_null());
                return 1;
            }
            if (strchr(clean, '.') != NULL) {
                push(st, val_float(atof(clean)));
            } else {
                push(st, val_int(strtoll(clean, NULL, 10)));
            }
            return 1;
        }
        const char *next = strchr(eq, ';');
        if (!next || next >= end) break;
        p = next + 1;
    }
    return 0;
}

/* Index into a string-backed container:
 * - "T;elem;elem;..."  → tuple  (element types auto-detected)
 * - "I;1;2;3"          → int array
 * - char array "abc"   → char element as 'x' string
 * Pushes the resolved value (or null). */
static void interp_index_value(InterpState *st, const char *s, int64_t i) {
    if (!s) { push(st, val_null()); return; }
    size_t slen = strlen(s);
    const char *sp = s;
    if (slen >= 2 && s[0] == '"' && s[slen-1] == '"') { sp = s + 1; slen -= 2; }

    /* Part 14 2D array encoded as "B;rows;cols;v0;v1;..." (row-major flat) */
    if (slen >= 2 && sp[0] == 'B' && sp[1] == ';') {
        const char *q = sp + 2;
        const char *end = sp + slen;
        while (q < end && *q != ';') q++;   /* skip rows */
        if (q < end) q++;
        while (q < end && *q != ';') q++;   /* skip cols */
        if (q < end) q++;
        int64_t cur = 0;
        int64_t v = 0;
        int digit = 0;
        for (const char *r = q; r <= end; r++) {
            if (r < end && *r >= '0' && *r <= '9') {
                v = v * 10 + (*r - '0');
                digit = 1;
            } else {
                if (digit) {
                    if (cur == i) break;
                    cur++;
                    v = 0;
                    digit = 0;
                }
                if (r >= end) break;
            }
        }
        push(st, val_int(v));
        return;
    }

    /* Part 5: growable list "M;cap;extra;count;v0;v1;..." */
    if (slen >= 2 && sp[0] == 'M' && sp[1] == ';') {
        const char *q = sp + 2;
        const char *end = sp + slen;
        /* parse header: cap;extra;count; (count is the element count) */
        int64_t list_cap = 0, list_extra = 0, list_count = 0;
        q = interp_parse_num(q, end, &list_cap);
        if (q < end && *q == ';') q++;
        q = interp_parse_num(q, end, &list_extra);
        if (q < end && *q == ';') q++;
        q = interp_parse_num(q, end, &list_count);
        if (q < end && *q == ';') q++;
        /* out-of-bounds read → return 0 (uninitialized element) */
        if (i < 0 || i >= list_count) {
            push(st, val_int(0));
            return;
        }
        int64_t cur = 0;
        for (;;) {
            const char *sep = strchr(q, ';');
            const char *elem_end = sep ? sep : end;
            if (cur == i) {
                size_t el = (size_t)(elem_end - q);
                char num[64];
                size_t cp = el < 63 ? el : 63;
                memcpy(num, q, cp);
                num[cp] = '\0';
                push(st, val_int(strtoll(num, NULL, 10)));
                return;
            }
            cur++;
            if (!sep) break;
            q = sep + 1;
        }
        push(st, val_int(0));
        return;
    }

    /* int array encoded as "I;1;2;3" */
    if (slen >= 2 && sp[0] == 'I' && sp[1] == ';') {
        const char *p2 = sp + 2;
        const char *end = sp + slen;
        int64_t cur = 0;
        int digit = 0;
        int eidx = 0;
        int64_t v = 0;
        for (const char *q = p2; q <= end; q++) {
            if (q < end && *q >= '0' && *q <= '9') {
                cur = cur * 10 + (*q - '0');
                digit = 1;
            } else {
                if (digit) {
                    if (eidx == i) { v = cur; break; }
                    eidx++;
                    cur = 0;
                    digit = 0;
                }
                if (q >= end) break;
            }
        }
        push(st, val_int(v));
        return;
    }

    /* tuple encoded as "T;elem;elem;..." */
    if (slen >= 2 && sp[0] == 'T' && sp[1] == ';') {
        const char *q = sp + 2;
        const char *end = sp + slen;
        int64_t cur_idx = 0;
        for (;;) {
            const char *sep = strchr(q, ';');
            const char *elem_end = sep ? sep : end;
            if (cur_idx == i) {
                size_t el = (size_t)(elem_end - q);
                if (el >= 255) el = 254;
                memcpy(st->elem_buf, q, el);
                st->elem_buf[el] = '\0';
                const char *clean = st->elem_buf;
                if (el >= 2 && q[0] == '"') { push(st, val_string(clean)); return; }
                if (el >= 3 && q[0] == '\'') {
                    st->elem_buf[0] = '\'';
                    st->elem_buf[1] = q[1];
                    st->elem_buf[2] = '\'';
                    st->elem_buf[3] = '\0';
                    push(st, val_string(st->elem_buf));
                    return;
                }
                if (strchr(clean, '.') != NULL) {
                    push(st, val_float(atof(clean)));
                } else if (el > 0 && (clean[0] == '-' || (clean[0] >= '0' && clean[0] <= '9'))) {
                    push(st, val_int(strtoll(clean, NULL, 10)));
                } else {
                    push(st, val_string(clean));
                }
                return;
            }
            cur_idx++;
            if (!sep) break;
            q = sep + 1;
        }
        push(st, val_null());
        return;
    }

    /* char array */
    if (slen >= 2 && sp[0] == '\'' && sp[slen-1] == '\'') { sp = sp + 1; slen -= 2; }
    if (i >= 0 && i < (int64_t)slen) {
        st->elem_buf[0] = '\'';
        st->elem_buf[1] = sp[i];
        st->elem_buf[2] = '\'';
        st->elem_buf[3] = '\0';
        push(st, val_string(st->elem_buf));
        return;
    }
    push(st, val_null());
}

/* Part 5: gcMalloc/malloc — growable int list.
 * Format: "M;cap;extra;count;v0;v1;..." (cap=current capacity, extra=grow step).
 * plain malloc: extra=0 → fixed capacity (no auto-grow). */
static void interp_gcmalloc(InterpState *st, const char *name, int64_t reserve, int64_t extra) {
    if (!name) return;
    if (reserve <= 0) reserve = 1;
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "\"M;%lld;%lld;0", (long long)reserve, (long long)extra);
    if (n < (int)sizeof(buf) - 2) { buf[n++] = '"'; buf[n] = '\0'; }
    store_var(st, pool_string(st, name), val_string(pool_string(st, buf)));
}

/* Part 4/5: variable.free() — marks the var as null. Freeing an already
 * null var emits a double-free #warning (orange). */
static void interp_free(InterpState *st, const char *name) {
    if (!name) return;
    VarSlot *slot = find_var(st, name);
    if (!slot) {
        fprintf(stdout, "\x1b[33mgcl: warning: free() on undeclared variable '%s'\x1b[0m\n", name);
        return;
    }
    if (slot->value.kind == VAL_NULL) {
        fprintf(stdout, "\x1b[33mgcl: warning: double free of '%s'\x1b[0m\n", name);
        return;
    }
    slot->value = val_null();
}

/* Part 3: scanf — delegate to scanf_module.c */
static void interp_scanf(InterpState *st, const char *name, int64_t kind) {
    extern void interp_scanf_module(void *st, const char *name, int64_t kind);
    interp_scanf_module(st, name, kind);
}

/* Part 14: instance field write — obj.field = v.
 * IR key: "obj\x01member". Stack: [obj, value] (obj pushed first).
 * Rebuilds the instance "S;field=value;..." and stores it back into obj. */
static void interp_member_assign(InterpState *st, const char *key) {
    if (!key) { pop(st); pop(st); return; }
    const char *sep = strchr(key, '\x01');
    const char *member = sep ? sep + 1 : key;
    size_t obj_len = sep ? (size_t)(sep - key) : 0;
    if (obj_len == 0 || !member[0]) { pop(st); pop(st); return; }
    char obj_name[128];
    size_t ol = obj_len < 127 ? obj_len : 127;
    memcpy(obj_name, key, ol);
    obj_name[ol] = '\0';

    /* IR order: value pushed first, then obj (LOAD obj). Top of stack is obj. */
    Val obj = pop(st);
    Val value = pop(st);

    const char *base = (obj.kind == VAL_STRING && obj.s_val) ? obj.s_val : "S;";
    size_t blen = strlen(base);
    const char *sp = base;
    if (blen >= 2 && sp[0] == '"' && sp[blen-1] == '"') { sp = sp + 1; blen -= 2; }
    /* validate container start; fall back to empty for non-containers */
    const char *body = sp;
    size_t clen = blen;
    if (clen >= 2 && (sp[0] == 'S' || sp[0] == 'D') && sp[1] == ';') {
        body = sp + 2;
        clen = blen - 2;
    } else {
        body = sp;
        /* plain string (e.g. "" or a literal) — rebuild from empty */
        clen = 0;
    }

    /* value serialization: keep quotes for strings so struct_member_value
     * can re-read them; bare ints/floats stay raw. */
    char vbuf[512];
    if (value.kind == VAL_STRING && value.s_val) {
        snprintf(vbuf, sizeof(vbuf), "%s", value.s_val);
    } else if (value.kind == VAL_FLOAT) {
        snprintf(vbuf, sizeof(vbuf), "%g", value.f_val);
    } else if (value.kind == VAL_INT) {
        snprintf(vbuf, sizeof(vbuf), "%lld", (long long)value.i_val);
    } else {
        vbuf[0] = '\0';
    }

    /* rebuild: keep existing members except the one being replaced */
    char out[1024];
    size_t n = 0;
    if (n + 2 < sizeof(out)) { out[n++] = 'S'; out[n++] = ';'; }
    const char *p = body;
    const char *end = body + clen;
    int replaced = 0;
    size_t mlen = strlen(member);
    while (p < end) {
        const char *eq = strchr(p, '=');
        if (!eq || eq >= end) break;
        size_t name_len = (size_t)(eq - p);
        const char *val_end = strchr(eq + 1, ';');
        if (!val_end || val_end > end) val_end = end;
        int is_target = (name_len == mlen && memcmp(p, member, name_len) == 0);
        if (!is_target) {
            size_t piece = (size_t)(val_end - p);
            if (n + piece + 1 < sizeof(out)) {
                memcpy(out + n, p, piece);
                n += piece;
                if (n < sizeof(out)) out[n++] = ';';
            }
        } else {
            replaced = 1;
            size_t needed = mlen + 1 + strlen(vbuf);
            if (n + needed + 1 < sizeof(out)) {
                memcpy(out + n, member, mlen);
                n += mlen;
                out[n++] = '=';
                memcpy(out + n, vbuf, strlen(vbuf));
                n += strlen(vbuf);
                if (n < sizeof(out)) out[n++] = ';';
            }
        }
        if (val_end >= end) break;
        p = val_end + 1;
    }
    if (!replaced && vbuf[0]) {
        size_t needed = mlen + 1 + strlen(vbuf);
        if (n + needed + 1 < sizeof(out)) {
            memcpy(out + n, member, mlen);
            n += mlen;
            out[n++] = '=';
            memcpy(out + n, vbuf, strlen(vbuf));
            n += strlen(vbuf);
            if (n < sizeof(out)) out[n++] = ';';
        } else if (n + mlen + 1 < sizeof(out)) {
            memcpy(out + n, member, mlen);
            n += mlen;
            out[n++] = '=';
            if (n < sizeof(out)) out[n++] = ';';
        }
    }
    if (n > 0 && out[n-1] == ';') n--;
    out[n] = '\0';
    /* obj_name is a stack buffer — copy it into the stable pool so the
     * stored var name survives the return. */
    store_var(st, pool_string(st, obj_name), val_string(pool_string(st, out)));
    push(st, value);
}

/* Part 14: write element at flat index back into a string-backed array.
 * Stack:  value, array_string, flat_index → (store), pops both array+index. */
static void interp_assign_index(InterpState *st, const char *name, int argc) {
    (void)argc;
    Val idx = pop(st);
    Val arr = pop(st);
    Val val = pop(st);
    if (arr.kind != VAL_STRING || !arr.s_val || !name) {
        push(st, val);
        return;
    }
    size_t slen = strlen(arr.s_val);
    const char *sp = arr.s_val;
    if (slen >= 2 && sp[0] == '"' && sp[slen-1] == '"') { sp = sp + 1; slen -= 2; }

    /* Part 5: growable list write "M;cap;extra;count;v0;v1;..." — appends
     * at target == count (auto-grow by extra with a #warning), replaces an
     * existing slot otherwise. Fixed malloc (extra==0) cannot grow. */
    if (slen >= 2 && sp[0] == 'M' && sp[1] == ';') {
        const char *end = sp + slen;
        int64_t cap = 0, extra = 0, count = 0;
        const char *q = sp + 2;
        q = interp_parse_num(q, end, &cap);
        if (q < end && *q == ';') q++;
        q = interp_parse_num(q, end, &extra);
        if (q < end && *q == ';') q++;
        q = interp_parse_num(q, end, &count);
        if (q < end && *q == ';') q++;

        int64_t target = val_to_int(idx);
        if (target < 0 || target > count) {
            /* bounds violation → no write */
            push(st, val);
            return;
        }
        if (target == count) {
            /* append */
            if (count >= cap) {
                if (extra <= 0) {
                    fprintf(stdout, "\x1b[33mgcl: warning: malloc array '%s' is full (%lld)\x1b[0m\n",
                            name, (long long)cap);
                    push(st, val);
                    return;
                }
                fprintf(stdout, "\x1b[33mgcl: warning: gcMalloc '%s' full (%lld), auto-grow +%lld\x1b[0m\n",
                        name, (long long)cap, (long long)extra);
                cap += extra;
            }
            count++;
        }

        char out[1024];
        size_t n = 0;
        int wn = snprintf(out + n, sizeof(out) - n, "M;%lld;%lld;%lld;",
                          (long long)cap, (long long)extra, (long long)count);
        if (wn > 0) n += (size_t)wn;

        /* copy existing elements; the slot that matches `target` is replaced
         * with the new value (append when target == old count). The last
         * element is NOT followed by ';' after a malloc write, so use `sep`
         * only to advance; never stop before emitting the target. */
        int64_t cur = 0;
        const char *r = q;
        int old_count = count;
        if (target == count) old_count = count - 1; /* appended slot: not in old data */
        while (cur < old_count && n + 2 < sizeof(out)) {
            const char *sep = strchr(r, ';');
            const char *elem_end = sep ? sep : end;
            if (cur == target) {
                int written = snprintf(out + n, sizeof(out) - n, "%lld;", (long long)val_to_int(val));
                if (written > 0) n += (size_t)written;
            } else if (elem_end > r) {
                size_t piece = (size_t)(elem_end - r);
                if (n + piece + 1 < sizeof(out)) {
                    memcpy(out + n, r, piece);
                    n += piece;
                    if (n < sizeof(out)) out[n++] = ';';
                }
            }
            cur++;
            if (!sep) break;
            r = sep + 1;
        }
        /* append case: new value goes after all old elements */
        if (target == count - 1) {
            int written = snprintf(out + n, sizeof(out) - n, "%lld;", (long long)val_to_int(val));
            if (written > 0) n += (size_t)written;
        }
        if (n > 0 && out[n-1] == ';') n--; /* trailing ; trimmed (read parse-tolerant) */
        out[n] = '\0';
        store_var(st, name, val_string(pool_string(st, out)));
        push(st, val);
        return;
    }

    if (slen < 2 || sp[0] != 'B' || sp[1] != ';') {
        push(st, val);
        return;
    }
    /* parse rows;cols then values up to flat index */
    const char *end = sp + slen;
    const char *q = sp + 2;
    while (q < end && *q != ';') q++;
    if (q < end) q++;
    while (q < end && *q != ';') q++;
    if (q < end) q++;
    int64_t target = val_to_int(idx);
    int64_t cur = 0;
    /* rebuild the encoded string with element @ target replaced */
    char out[1024];
    size_t n = 0;
    n += snprintf(out + n, sizeof(out) - n, "B;");
    const char *hd = sp + 2;
    size_t hd_len = (size_t)(q - hd);
    if (n + hd_len < sizeof(out)) { memcpy(out + n, hd, hd_len); n += hd_len; }
    if (n < sizeof(out)) out[n++] = ';';
    const char *r = q;
    int digit = 0;
    for (const char *p = r; p <= end; p++) {
        if (p < end && *p >= '0' && *p <= '9') { digit = 1; }
        if (p < end && digit && (p[1] < '0' || p[1] > '9')) {
            if (cur == target) {
                int written = snprintf(out + n, sizeof(out) - n, "%lld;", (long long)val_to_int(val));
                if (written > 0) n += (size_t)written;
                digit = 0;
                cur++;
            } else {
                const char *tok_start = p + 1 - 1;
                while (tok_start > r + 1 && tok_start[-1] >= '0' && tok_start[-1] <= '9') tok_start--;
                size_t tl = (size_t)((p + 1) - tok_start);
                if (n + tl + 1 < sizeof(out)) {
                    memcpy(out + n, tok_start, tl);
                    n += tl;
                    if (n < sizeof(out)) out[n++] = ';';
                }
                digit = 0;
                cur++;
            }
        }
    }
    if (n > 0 && out[n-1] == ';') n--;
    out[n] = '\0';
    store_var(st, name, val_string(pool_string(st, out)));
    push(st, val);
}

int gcl_interp_run(const GclIrProgram *prog) {
    InterpState st;
    memset(&st, 0, sizeof(st));

    st.source_dir = NULL;
    st.current_prog = prog;

    interp_process_extern(&st, prog);

    int main_pc = -1;
    for (int i = 0; i < prog->count; i++) {
        if (prog->instrs[i].op == IR_LABEL && prog->instrs[i].s_val &&
            strcmp(prog->instrs[i].s_val, "main") == 0) {
            main_pc = i + 1;
            break;
        }
    }
    if (main_pc < 0) main_pc = 0;

    st.pc = 0;
    while (st.pc < prog->count && prog->instrs[st.pc].op != IR_LABEL && !st.halted) {
        const GclIrInstr *g = &prog->instrs[st.pc];
        st.pc++;
        switch (g->op) {
        case IR_PUSH_INT:    push(&st, val_int(g->i_val)); break;
        case IR_PUSH_FLOAT:  push(&st, val_float(g->f_val)); break;
        case IR_PUSH_STRING: push(&st, val_string(g->s_val)); break;
        case IR_PUSH_NULL:   push(&st, val_null()); break;
        case IR_STORE: { Val v = pop(&st); store_var(&st, g->s_val, v); break; }
        case IR_STORE_GLOBAL: { Val v = pop(&st); store_var(&st, g->s_val, v); break; }
        default:
            break;
        }
    }
    st.pc = main_pc;

    while (st.pc < prog->count && !st.halted) {
        const GclIrInstr *ins = &prog->instrs[st.pc];
        st.pc++;

        switch (ins->op) {
        case IR_NOP: break;
        case IR_PUSH_INT: push(&st, val_int(ins->i_val)); break;
        case IR_PUSH_FLOAT: push(&st, val_float(ins->f_val)); break;
        case IR_PUSH_STRING: push(&st, val_string(ins->s_val)); break;
        case IR_PUSH_NULL: push(&st, val_null()); break;
        case IR_POP: pop(&st); break;
        case IR_ADD: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) + val_to_int(b))); break; }
        case IR_SUB: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) - val_to_int(b))); break; }
        case IR_MUL: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) * val_to_int(b))); break; }
        case IR_DIV: { Val b = pop(&st); Val a = pop(&st); int64_t bv = val_to_int(b); push(&st, val_int(bv ? val_to_int(a)/bv : 0)); break; }
        case IR_MOD: { Val b = pop(&st); Val a = pop(&st); int64_t bv = val_to_int(b); push(&st, val_int(bv ? val_to_int(a)%bv : 0)); break; }
        case IR_PTR_ADD: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) + val_to_int(b))); break; }
        case IR_PTR_SUB: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) - val_to_int(b))); break; }
        case IR_NEG: { Val a = pop(&st); push(&st, val_int(-val_to_int(a))); break; }
        case IR_NOT: { Val a = pop(&st); push(&st, val_int(!val_truthy(a))); break; }
        case IR_BITNOT: { Val a = pop(&st); push(&st, val_int(~val_to_int(a))); break; }
        case IR_EQ: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) == val_to_int(b))); break; }
        case IR_NEQ: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) != val_to_int(b))); break; }
        case IR_LT: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) < val_to_int(b))); break; }
        case IR_GT: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) > val_to_int(b))); break; }
        case IR_LTE: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) <= val_to_int(b))); break; }
        case IR_GTE: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) >= val_to_int(b))); break; }
        case IR_AND: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_truthy(a) && val_truthy(b))); break; }
        case IR_OR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_truthy(a) || val_truthy(b))); break; }
        case IR_BITAND: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) & val_to_int(b))); break; }
        case IR_BITOR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) | val_to_int(b))); break; }
        case IR_BITXOR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) ^ val_to_int(b))); break; }
        case IR_SHL: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) << val_to_int(b))); break; }
        case IR_SHR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) >> val_to_int(b))); break; }
        case IR_LOAD: push(&st, load_var(&st, ins->s_val)); break;
        case IR_STORE: { Val v = pop(&st); store_var(&st, ins->s_val, v); break; }
        case IR_LOAD_GLOBAL: push(&st, load_var(&st, ins->s_val)); break;
        case IR_STORE_GLOBAL: { Val v = pop(&st); store_var(&st, ins->s_val, v); break; }
        case IR_CALL: {
            if (call_user_function(&st, ins->s_val, ins->arg_count)) {
                break;
            }
            Val result = call_function(&st, ins->s_val, ins->arg_count);
            push(&st, result);
            break;
        }
        case IR_RET: {
            Val retval = pop(&st);
            if (st.call_depth > 0) {
                int ret_pc = st.call_stack[--st.call_depth];
                st.pc = ret_pc;
                push(&st, retval);
            } else {
                st.halted = 1;
            }
            break;
        }
        case IR_JMP: st.pc = (int)ins->i_val; break;
        case IR_JZ: { Val c = pop(&st); if (!val_truthy(c)) st.pc = (int)ins->i_val; break; }
        case IR_JNZ: { Val c = pop(&st); if (val_truthy(c)) st.pc = (int)ins->i_val; break; }
        case IR_LABEL: break;
        case IR_MEMBER: {
            Val base = pop(&st);
            if (base.kind == VAL_STRING && base.s_val &&
                struct_member_value(&st, base.s_val, ins->s_val ? ins->s_val : "")) {
                break;
            }
            /* Part 18: parentheseless class member access — FATHER.Call
             * calls the underlying "<Class>_<Method>" function; its return
             * value is pushed by the IR_RET of that function. */
            const char *member = ins->s_val;
            int called = 0;
            if (member && member[0] && st.current_prog) {
                size_t nl = strlen(member);
                const GclIrProgram *prog = st.current_prog;
                for (int i = 0; i < prog->count; i++) {
                    const GclIrInstr *li = &prog->instrs[i];
                    if (li->op != IR_LABEL || !li->s_val) continue;
                    size_t ll = strlen(li->s_val);
                    if (ll > nl + 1 && li->s_val[ll - nl - 1] == '_' &&
                        strcmp(li->s_val + ll - nl, member) == 0) {
                        if (st.call_depth < INTERP_CALL_MAX) {
                            st.call_stack[st.call_depth++] = st.pc;
                            st.in_call = 1;
                            st.pc = i + 1;
                            called = 1;
                        }
                        break;
                    }
                }
            }
            if (!called) {
                push(&st, val_null());
            }
            break;
        }
        case IR_INDEX: {
            Val idx = pop(&st);
            Val base = pop(&st);
            if (base.kind == VAL_STRING && base.s_val) {
                interp_index_value(&st, base.s_val, val_to_int(idx));
                break;
            }
            push(&st, val_null());
            break;
        }
        case IR_INDEX_ASSIGN: {
            interp_assign_index(&st, ins->s_val, ins->arg_count);
            break;
        }
        case IR_MEMBER_ASSIGN: {
            interp_member_assign(&st, ins->s_val);
            break;
        }
        case IR_SCANF: {
            interp_scanf(&st, ins->s_val, ins->i_val);
            break;
        }
        case IR_FREE: {
            interp_free(&st, ins->s_val);
            break;
        }
        case IR_GCMALLOC: {
            interp_gcmalloc(&st, ins->s_val, ins->i_val, ins->arg_count);
            break;
        }
        case IR_STRUCT_COPY: break;
        case IR_PRINT: { Val v = pop(&st); if (v.kind == VAL_STRING && v.s_val) printf("%s\n", v.s_val); else if (v.kind == VAL_INT) printf("%lld\n", (long long)v.i_val); else printf("null\n"); break; }
        case IR_HALT: st.halted = 1; break;
        }
    }

    interp_unload_dlls(&st);
    return 0;
}

/* Extended version that accepts source filepath for DLL resolution */
int gcl_interp_run_with_path(const GclIrProgram *prog, const char *filepath) {
    InterpState st;
    memset(&st, 0, sizeof(st));

    static char dir_buf[4096];
    dir_buf[0] = '\0';
    if (filepath) {
        const char *last_sep = filepath;
        for (const char *p = filepath; *p; p++) {
            if (*p == '/' || *p == '\\') last_sep = p + 1;
        }
        size_t dlen = (size_t)(last_sep - filepath);
        if (dlen > 0 && dlen < sizeof(dir_buf)) {
            memcpy(dir_buf, filepath, dlen);
            dir_buf[dlen] = '\0';
        }
    }
    st.source_dir = dir_buf;
    st.current_prog = prog;

    {
        char dll_path[4096];
        snprintf(dll_path, sizeof(dll_path), "%sraylib.dll", dir_buf);
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(dll_path);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            interp_load_dll(&st, "raylib.dll");
        }
#endif
    }

    int main_pc = -1;
    for (int i = 0; i < prog->count; i++) {
        if (prog->instrs[i].op == IR_LABEL && prog->instrs[i].s_val &&
            strcmp(prog->instrs[i].s_val, "main") == 0) {
            main_pc = i + 1;
            break;
        }
    }
    if (main_pc < 0) main_pc = 0;

    st.pc = 0;
    while (st.pc < prog->count && prog->instrs[st.pc].op != IR_LABEL && !st.halted) {
        const GclIrInstr *g = &prog->instrs[st.pc];
        st.pc++;
        switch (g->op) {
        case IR_PUSH_INT:    push(&st, val_int(g->i_val)); break;
        case IR_PUSH_FLOAT:  push(&st, val_float(g->f_val)); break;
        case IR_PUSH_STRING: push(&st, val_string(g->s_val)); break;
        case IR_PUSH_NULL:   push(&st, val_null()); break;
        case IR_STORE: { Val v = pop(&st); store_var(&st, g->s_val, v); break; }
        case IR_STORE_GLOBAL: { Val v = pop(&st); store_var(&st, g->s_val, v); break; }
        default:
            break;
        }
    }
    st.pc = main_pc;

    while (st.pc < prog->count && !st.halted) {
        const GclIrInstr *ins = &prog->instrs[st.pc];
        st.pc++;

        switch (ins->op) {
        case IR_NOP: break;
        case IR_PUSH_INT: push(&st, val_int(ins->i_val)); break;
        case IR_PUSH_FLOAT: push(&st, val_float(ins->f_val)); break;
        case IR_PUSH_STRING: push(&st, val_string(ins->s_val)); break;
        case IR_PUSH_NULL: push(&st, val_null()); break;
        case IR_POP: pop(&st); break;
        case IR_ADD: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) + val_to_int(b))); break; }
        case IR_SUB: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) - val_to_int(b))); break; }
        case IR_MUL: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) * val_to_int(b))); break; }
        case IR_DIV: { Val b = pop(&st); Val a = pop(&st); int64_t bv = val_to_int(b); push(&st, val_int(bv ? val_to_int(a)/bv : 0)); break; }
        case IR_MOD: { Val b = pop(&st); Val a = pop(&st); int64_t bv = val_to_int(b); push(&st, val_int(bv ? val_to_int(a)%bv : 0)); break; }
        case IR_PTR_ADD: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) + val_to_int(b))); break; }
        case IR_PTR_SUB: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) - val_to_int(b))); break; }
        case IR_NEG: { Val a = pop(&st); push(&st, val_int(-val_to_int(a))); break; }
        case IR_NOT: { Val a = pop(&st); push(&st, val_int(!val_truthy(a))); break; }
        case IR_BITNOT: { Val a = pop(&st); push(&st, val_int(~val_to_int(a))); break; }
        case IR_EQ: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) == val_to_int(b))); break; }
        case IR_NEQ: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) != val_to_int(b))); break; }
        case IR_LT: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) < val_to_int(b))); break; }
        case IR_GT: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) > val_to_int(b))); break; }
        case IR_LTE: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) <= val_to_int(b))); break; }
        case IR_GTE: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) >= val_to_int(b))); break; }
        case IR_AND: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_truthy(a) && val_truthy(b))); break; }
        case IR_OR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_truthy(a) || val_truthy(b))); break; }
        case IR_BITAND: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) & val_to_int(b))); break; }
        case IR_BITOR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) | val_to_int(b))); break; }
        case IR_BITXOR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) ^ val_to_int(b))); break; }
        case IR_SHL: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) << val_to_int(b))); break; }
        case IR_SHR: { Val b = pop(&st); Val a = pop(&st); push(&st, val_int(val_to_int(a) >> val_to_int(b))); break; }
        case IR_LOAD: push(&st, load_var(&st, ins->s_val)); break;
        case IR_STORE: { Val v = pop(&st); store_var(&st, ins->s_val, v); break; }
        case IR_LOAD_GLOBAL: push(&st, load_var(&st, ins->s_val)); break;
        case IR_STORE_GLOBAL: { Val v = pop(&st); store_var(&st, ins->s_val, v); break; }
        case IR_CALL: {
            if (call_user_function(&st, ins->s_val, ins->arg_count)) {
                break;
            }
            Val result = call_function(&st, ins->s_val, ins->arg_count);
            push(&st, result);
            break;
        }
        case IR_RET: {
            Val retval = pop(&st);
            if (st.call_depth > 0) {
                int ret_pc = st.call_stack[--st.call_depth];
                st.pc = ret_pc;
                push(&st, retval);
            } else {
                st.halted = 1;
            }
            break;
        }
        case IR_JMP: st.pc = (int)ins->i_val; break;
        case IR_JZ: { Val c = pop(&st); if (!val_truthy(c)) st.pc = (int)ins->i_val; break; }
        case IR_JNZ: { Val c = pop(&st); if (val_truthy(c)) st.pc = (int)ins->i_val; break; }
        case IR_LABEL: break;
        case IR_MEMBER: {
            Val base = pop(&st);
            if (base.kind == VAL_STRING && base.s_val &&
                struct_member_value(&st, base.s_val, ins->s_val ? ins->s_val : "")) {
                break;
            }
            /* Part 18: parentheseless class member access — FATHER.Call
             * calls the underlying "<Class>_<Method>" function; its return
             * value is pushed by the IR_RET of that function. */
            const char *member = ins->s_val;
            int called = 0;
            if (member && member[0] && st.current_prog) {
                size_t nl = strlen(member);
                const GclIrProgram *prog = st.current_prog;
                for (int i = 0; i < prog->count; i++) {
                    const GclIrInstr *li = &prog->instrs[i];
                    if (li->op != IR_LABEL || !li->s_val) continue;
                    size_t ll = strlen(li->s_val);
                    if (ll > nl + 1 && li->s_val[ll - nl - 1] == '_' &&
                        strcmp(li->s_val + ll - nl, member) == 0) {
                        if (st.call_depth < INTERP_CALL_MAX) {
                            st.call_stack[st.call_depth++] = st.pc;
                            st.in_call = 1;
                            st.pc = i + 1;
                            called = 1;
                        }
                        break;
                    }
                }
            }
            if (!called) {
                push(&st, val_null());
            }
            break;
        }
        case IR_INDEX: {
            Val idx = pop(&st);
            Val base = pop(&st);
            if (base.kind == VAL_STRING && base.s_val) {
                interp_index_value(&st, base.s_val, val_to_int(idx));
                break;
            }
            push(&st, val_null());
            break;
        }
        case IR_INDEX_ASSIGN: {
            interp_assign_index(&st, ins->s_val, ins->arg_count);
            break;
        }
        case IR_MEMBER_ASSIGN: {
            interp_member_assign(&st, ins->s_val);
            break;
        }
        case IR_SCANF: {
            interp_scanf(&st, ins->s_val, ins->i_val);
            break;
        }
        case IR_FREE: {
            interp_free(&st, ins->s_val);
            break;
        }
        case IR_GCMALLOC: {
            interp_gcmalloc(&st, ins->s_val, ins->i_val, ins->arg_count);
            break;
        }
        case IR_STRUCT_COPY: break;
        case IR_PRINT: { Val v = pop(&st); if (v.kind == VAL_STRING && v.s_val) printf("%s\n", v.s_val); else if (v.kind == VAL_INT) printf("%lld\n", (long long)v.i_val); else printf("null\n"); break; }
        case IR_HALT: st.halted = 1; break;
        }
    }

    interp_unload_dlls(&st);
    return 0;
}



