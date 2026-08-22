#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "../SharedPipeline/Ir/gcl_ir.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Core interpreter data structures and functions */

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
} VarSlot;

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
    const GclIrProgram *current_prog;
    int         call_stack[INTERP_CALL_MAX];
    int         call_depth;
    int         in_call;
    char        elem_buf[256];
    char        string_pool[64][256];
    int         string_pool_count;
} InterpState;

/* Stack operations */
void push(InterpState *st, Val v) {
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

/* Value constructors */
Val val_int(int64_t v) { Val r = {VAL_INT, v, 0, NULL}; return r; }
Val val_float(double v) { Val r = {VAL_FLOAT, 0, v, NULL}; return r; }
Val val_string(const char *s) { Val r = {VAL_STRING, 0, 0, s}; return r; }
Val val_null(void) { Val r = {VAL_NULL, 0, 0, NULL}; return r; }

/* String pooling */
const char *pool_string(InterpState *st, const char *s) {
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

/* UTF-8 character count */
int64_t utf8_strlen(const char *s) {
    if (!s) return 0;
    int64_t count = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) count++;
    }
    return count;
}

/* Value conversion */
int64_t val_to_int(Val v) {
    switch (v.kind) {
    case VAL_INT: return v.i_val;
    case VAL_FLOAT: return (int64_t)v.f_val;
    case VAL_STRING: return v.s_val ? utf8_strlen(v.s_val) : 0;
    case VAL_NULL: return 0;
    }
    return 0;
}

int val_truthy(Val v) {
    switch (v.kind) {
    case VAL_NULL: return 0;
    case VAL_INT: return v.i_val != 0;
    case VAL_FLOAT: return v.f_val != 0.0;
    case VAL_STRING: return v.s_val != NULL && v.s_val[0] != '\0';
    }
    return 0;
}

/* Variable storage */
VarSlot *find_var(InterpState *st, const char *name) {
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
        st->var_count++;
    }
    if (val.kind == VAL_STRING && val.s_val) {
        val.s_val = pool_string(st, val.s_val);
    }
    slot->value = val;
}

Val load_var(InterpState *st, const char *name) {
    VarSlot *slot = find_var(st, name);
    if (slot) return slot->value;
    return val_null();
}
