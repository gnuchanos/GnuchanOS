/*
 * gcl_runner.c — GCL AST yürütücü (SimpleRunner'a ait).
 *
 * simple_doc.md'deki örnekleri çalıştırır:
 *   variable decl, printf("{}", ...), if/else, while, for, break/continue,
 *   aritmetik/karşılaştırma/boolean op'lar, scanf (basit), Math.*, Stdio.*,
 *   struct/enum/typedef, kullanıcı fonksiyon tanımı ve çağrısı.
 */

#include "gcl_runner.h"
#include "gcl_parser.h"
#include "gcl_error.h"
#include "gcl_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

/* -debug bayrağı: gcl -debug -run ... ile etkinleşir (gcl_main.c set eder).
   Normal çalıştırmada runtime debug mesajları GÖRÜNMEZ. */
int gcl_debug = 0;

/* Tek debug kapısı: runtime debug/izleme çıktısının TAMAMI buradan geçer.
   `gcl -debug -run ...` verilmedikçe hiçbir debug satırı yazılmaz. */
static void gcl_debugf(const char *fmt, ...) {
    if (!gcl_debug) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access _access
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

/* ---------- Değişken ortamı ---------- */

typedef struct Var {
    char *name;
    double num;
    char *str;          /* string ise */
    int is_string;
    char *decl_type;    /* bildirim tipi (gcChar, char, int, ...) */
    int array_size;     /* 0=array değil, -1=char[] boş, N=char[N] */
    int is_pointer;     /* char *ptr */
    int is_const;       /* 1=const değişken (atama yapılamaz) */
    int is_global;      /* 1=global değişken (fonksiyon çıkışında kalır) */
    /* int arr[] = {...} sayısal dizi desteği */
    double *arr_vals;
    int arr_count;
    /* char array of strings: e.g. char texts[3][20] */
    char **arr_strs;
    /* array of struct members (for struct T arr[N]) */
    struct GclStructValue **arr_members;
    struct GclStructValue *members; /* struct değişken ise */
    struct Var *next;
} Var;

typedef struct TypedefDef {
    char *alias;
    char *base;
    struct TypedefDef *next;
} TypedefDef;

typedef struct StructDef {
    char *name;
    char **member_names;
    char **member_types;
    int member_count;
    struct StructDef *next;
} StructDef;

typedef struct FuncDef {
    char *name;
    char **params;
    int param_count;    
    GclStmt *body;
    struct FuncDef *next;
} FuncDef;

/* #lib <Ad> ile yüklenen GCL kütüphanesi (Ad.gclib) */
/* #lib/.gclib support removed — LibModule type eliminated */

/* Yüklenmiş native modül */
typedef struct NativeModule {
    char *name;
#ifdef _WIN32
    HMODULE handle;
#else
    void *handle;
#endif
    const GclNativeEntry *entries;
    int entry_count;
    struct NativeModule *next;
} NativeModule;

typedef struct GclEnv {
    Var *vars;
    TypedefDef *typedefs;
    StructDef *structs;
    FuncDef *funcs;
    NativeModule *modules;
    /* lib bundles removed */
    GclExternDll extern_dlls[16];
    int extern_dll_count;
    GclExternReg extern_regs[64];
    int extern_reg_count;
    char base_dir[4096];
    /* main(int argc, char *argv[]) desteği */
    int arg_count;
    char **arg_vals;
} GclEnv;

/* ön bildirimler */
static void free_struct_members(GclStructValue *m);
static GclStructValue *clone_struct_members(GclStructValue *m);
static StructDef *find_struct(GclEnv *env, const char *name);
static double truncate_to_declared_type(double val, const char *decl_type);

static Var *env_find(GclEnv *env, const char *name) {
    for (Var *v = env->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) return v;
    }
    return NULL;
}

/* For-init gibi blok-scope değişkenini env'den kaldır (scope sonu) */
static void env_remove_var(GclEnv *env, const char *name) {
    if (!env || !name) return;
    Var **pp = &env->vars;
    while (*pp) {
        Var *v = *pp;
        if (strcmp(v->name, name) == 0) {
            *pp = v->next;
            if (v->name) free(v->name);
            if (v->str) free(v->str);
            if (v->decl_type) free(v->decl_type);
            if (v->members) free_struct_members(v->members);
            if (v->arr_vals) free(v->arr_vals);
            if (v->arr_strs) {
                for (int i = 0; i < v->arr_count; i++) if (v->arr_strs[i]) free(v->arr_strs[i]);
                free(v->arr_strs);
            }
            if (v->arr_members) {
                for (int i = 0; i < v->arr_count; i++) if (v->arr_members[i]) free_struct_members(v->arr_members[i]);
                free(v->arr_members);
            }
            free(v);
            return;
        }
        pp = &v->next;
    }
}

static void env_set_num(GclEnv *env, const char *name, double val) {
    Var *v = env_find(env, name);
    if (!v) {
        v = (Var *)calloc(1, sizeof(Var));
        if (!v) return;
        v->name = strdup(name);
        v->next = env->vars;
        env->vars = v;
    }
    /* int8/uint8/.../float32 gibi tiplerde gerçek taşma/clamp semantiği */
    v->num = truncate_to_declared_type(val, v->decl_type);
    v->is_string = 0;
    v->array_size = 0;
    v->is_pointer = 0;
    if (v->str) { free(v->str); v->str = NULL; }
}

static void env_set_str(GclEnv *env, const char *name, const char *val) {
    Var *v = env_find(env, name);
    if (!v) {
        v = (Var *)calloc(1, sizeof(Var));
        if (!v) return;
        v->name = strdup(name);
        v->next = env->vars;
        env->vars = v;
    }
    if (v->str) free(v->str);
    v->str = strdup(val ? val : "");
    v->is_string = 1;
    v->num = 0;
}

static double env_get_num(GclEnv *env, const char *name) {
    Var *v = env_find(env, name);
    if (!v) return 0.0;
    if (v->is_string && v->str) return atof(v->str);
    return v->num;
}

/* Gerçek int/uint/float tipleri: bildirilen decl_type'a göre değeri
   taşma/fazlalık sınırına clamp eder. Runner önceden her şeyi double
   tutuyordu; bu, int8/int16/.../float32 ve bool için doğru semantik verir. */
static double truncate_to_declared_type(double val, const char *decl_type) {
    if (!decl_type || !decl_type[0]) return val;
    /* bool → 0/1 */
    if (strcmp(decl_type, "bool") == 0) return val != 0.0 ? 1.0 : 0.0;
    /* işaretli tamsayılar */
    if (strcmp(decl_type, "int8") == 0) return (double)(signed char)((long long)val);
    if (strcmp(decl_type, "int16") == 0) return (double)(short)((long long)val);
    if (strcmp(decl_type, "int32") == 0) return (double)(int)((long long)val);
    if (strcmp(decl_type, "int64") == 0) return (double)(long long)val;
    if (strcmp(decl_type, "int128") == 0) return (double)(long long)val;
    /* işaretsiz tamsayılar */
    if (strcmp(decl_type, "uint8") == 0) return (double)(unsigned char)((long long)val);
    if (strcmp(decl_type, "uint16") == 0) return (double)(unsigned short)((long long)val);
    if (strcmp(decl_type, "uint32") == 0) return (double)(unsigned int)((long long)val);
    if (strcmp(decl_type, "uint64") == 0) return (double)(unsigned long long)((long long)val);
    if (strcmp(decl_type, "uint128") == 0) return (double)(unsigned long long)((long long)val);
    /* float tipleri */
    if (strcmp(decl_type, "float") == 0 || strcmp(decl_type, "float32") == 0 || strcmp(decl_type, "float16") == 0)
        return (double)(float)val;
    if (strcmp(decl_type, "float64") == 0 || strcmp(decl_type, "double") == 0) return val;
    return val;
}

/* Struct member listesini yapılandır (recursive nested struct) */
static void build_members_recursive(GclEnv *env, GclStructValue **head, StructDef *def) {
    if (!env || !head || !def) return;
    *head = NULL;
    for (int i = def->member_count - 1; i >= 0; i--) {
        GclStructValue *m = (GclStructValue *)calloc(1, sizeof(GclStructValue));
        if (!m) continue;
        m->name = strdup(def->member_names[i]);
        m->num = 0.0;
        m->is_string = 0;
        m->members = NULL;
        /* üyenin bildirim tipi — int8/float32 gibi tiplerde clamp için */
        if (def->member_types && def->member_types[i])
            m->decl_type = strdup(def->member_types[i]);
        /* member tipi başka struct ise recursive oluştur */
        if (def->member_types && def->member_types[i]) {
            StructDef *nsd = find_struct(env, def->member_types[i]);
            if (nsd) build_members_recursive(env, &m->members, nsd);
        }
        m->next = *head;
        *head = m;
    }
}

/* Struct değişken oluştur */
static void env_set_struct(GclEnv *env, const char *name, StructDef *def) {
    Var *v = env_find(env, name);
    if (!v) {
        v = (Var *)calloc(1, sizeof(Var));
        if (!v) return;
        v->name = strdup(name);
        v->next = env->vars;
        env->vars = v;
    }
    v->is_string = 0;
    if (v->str) { free(v->str); v->str = NULL; }
    if (v->members) free_struct_members(v->members);
    v->members = NULL;
    build_members_recursive(env, &v->members, def);
}

/* Struct member değeri oku */
static double env_get_member_num(GclEnv *env, const char *var_name, const char *member) {
    Var *v = env_find(env, var_name);
    if (!v || !v->members) return 0.0;
    for (GclStructValue *m = v->members; m; m = m->next) {
        if (strcmp(m->name, member) == 0) return m->num;
    }
    return 0.0;
}

static const char *env_get_member_str(GclEnv *env, const char *var_name, const char *member) {
    Var *v = env_find(env, var_name);
    if (!v || !v->members) return NULL;
    for (GclStructValue *m = v->members; m; m = m->next) {
        if (strcmp(m->name, member) == 0) return m->is_string && m->str ? m->str : NULL;
    }
    return NULL;
}

/* Struct member değer yaz — member tipine göre clamp uygula */
static void env_set_member_num(GclEnv *env, const char *var_name, const char *member, double val) {
    Var *v = env_find(env, var_name);
    if (!v || !v->members) return;
    for (GclStructValue *m = v->members; m; m = m->next) {
        if (strcmp(m->name, member) == 0) {
            m->num = truncate_to_declared_type(val, m->decl_type);
            m->is_string = 0;
            return;
        }
    }
}

/* Struct member string değer yaz */
static void env_set_member_str(GclEnv *env, const char *var_name, const char *member, const char *val) {
    Var *v = env_find(env, var_name);
    if (!v || !v->members) return;
    for (GclStructValue *m = v->members; m; m = m->next) {
        if (strcmp(m->name, member) == 0) {
            if (m->str) free(m->str);
            m->str = strdup(val ? val : "");
            m->is_string = 1;
            m->num = 0;
            return;
        }
    }
}

/* Struct member listesini temizle (recursive nested) */
static void free_struct_members(GclStructValue *m) {
    while (m) {
        GclStructValue *nx = m->next;
        if (m->name) free(m->name);
        if (m->str) free(m->str);
        if (m->decl_type) free(m->decl_type);
        if (m->members) free_struct_members(m->members);
        free(m);
        m = nx;
    }
}

/* Struct değerini kopyala (aynı listeden derin kopya, recursive nested) */
static GclStructValue *clone_struct_members(GclStructValue *m) {
    GclStructValue *head = NULL, *tail = NULL;
    while (m) {
        GclStructValue *nm = (GclStructValue *)calloc(1, sizeof(GclStructValue));
        if (!nm) break;
        if (m->name) nm->name = strdup(m->name);
        if (m->decl_type) nm->decl_type = strdup(m->decl_type);
        if (m->is_string && m->str) {
            nm->str = strdup(m->str);
            nm->is_string = 1;
        } else {
            nm->num = m->num;
            nm->is_string = 0;
        }
        if (m->members) nm->members = clone_struct_members(m->members);
        if (!head) head = nm; else tail->next = nm;
        tail = nm;
        m = m->next;
    }
    return head;
}

/* typedef arar */
static const char *find_typedef_base(GclEnv *env, const char *alias) {
    for (TypedefDef *td = env->typedefs; td; td = td->next) {
        if (strcmp(td->alias, alias) == 0) return td->base;
    }
    return NULL;
}

/* struct tanımı arar */
static StructDef *find_struct(GclEnv *env, const char *name) {
    for (StructDef *sd = env->structs; sd; sd = sd->next) {
        if (strcmp(sd->name, name) == 0) return sd;
    }
    return NULL;
}

/* fonksiyon arar */
static FuncDef *find_func(GclEnv *env, const char *name) {
    for (FuncDef *fd = env->funcs; fd; fd = fd->next) {
        if (strcmp(fd->name, name) == 0) return fd;
    }
    return NULL;
}

/* ---------- Runner ---------- */

typedef struct {
    GclEnv *env;
    int return_flag;
    double return_value;
    int break_flag;
    int continue_flag;
    int error;
    GclStructValue *return_struct;  /* struct dönüş değeri */
    GclExpr *return_init_list;      /* struct literal dönüşü (return { ... }) */
} Runner;

/* eval_expr ön bildirimi */
static double eval_expr(GclExpr *e, Runner *r);
/* Nested struct init için ön bildirim — fill_members_from_init_list recursive */
static void fill_members_from_init_list(Runner *r, GclStructValue *members, GclExpr *init);

/* Struct member'ına değer yaz — string ise string, sayı ise number */
static void set_member_value(GclStructValue *m, GclExpr *item, Runner *r) {
    if (!m || !item) return;
    /* Nested struct init: Vector3 position = { 10, 20, 30 } → alt üyelere recursive uygula */
    if (item->kind == AST_EXPR_INIT_LIST && m->members) {
        fill_members_from_init_list(r, m->members, item);
        return;
    }
    if (item->kind == AST_EXPR_STRING) {
        if (m->str) free(m->str);
        m->str = strdup(item->str ? item->str : "");
        m->is_string = 1;
        m->num = 0;
    } else if (item->kind == AST_EXPR_VAR) {
        Var *sv = env_find(r->env, item->name);
        if (sv && sv->is_string && sv->str) {
            if (m->str) free(m->str);
            m->str = strdup(sv->str);
            m->is_string = 1;
            m->num = 0;
        } else {
            m->num = truncate_to_declared_type(sv ? sv->num : eval_expr(item, r), m->decl_type);
            m->is_string = 0;
        }
    } else {
        m->num = truncate_to_declared_type(eval_expr(item, r), m->decl_type);
        m->is_string = 0;
    }
}

/* Struct init listesini member listesine uygula (pozisyonel + designated) */
static void fill_members_from_init_list(Runner *r, GclStructValue *members, GclExpr *init) {
    if (!members || !init || init->kind != AST_EXPR_INIT_LIST) return;
    GclStructValue *m = members;
    for (int i = 0; i < init->arg_count; i++) {
        GclExpr *item = init->args[i];
        if (!item) continue;
        if (item->kind == AST_EXPR_ASSIGN && item->left) {
            /* designated: .member = value */
            if (item->left->kind == AST_EXPR_MEMBER && item->left->left) {
                const char *mname = item->left->member_name;
                for (GclStructValue *ms = members; ms; ms = ms->next) {
                    if (ms->name && strcmp(ms->name, mname) == 0) {
                        set_member_value(ms, item->right, r);
                        break;
                    }
                }
            } else if (item->left->kind == AST_EXPR_VAR) {
                /* .member = value → AST_EXPR_VAR + member_name bazı durumlarda */
                const char *mname = item->left->name;
                for (GclStructValue *ms = members; ms; ms = ms->next) {
                    if (ms->name && strcmp(ms->name, mname) == 0) {
                        set_member_value(ms, item->right, r);
                        break;
                    }
                }
            }
        } else if (m) {
            /* pozisyonel: { val1, val2, ... } */
            set_member_value(m, item, r);
            m = m->next;
        }
    }
}

/* Struct init doldur — VAR kopya, CALL dönüş, INIT_LIST (pozisyonel + designated) */
static void fill_struct_init(Runner *r, const char *var_name, GclExpr *init) {
    Var *v = env_find(r->env, var_name);
    if (!v || !v->members || !init) return;

    if (init->kind == AST_EXPR_INIT_LIST) {
        fill_members_from_init_list(r, v->members, init);
        return;
    }

    /* init bir değişken: aynı struct'tan kopyala (Hello f = e;) */
    if (init->kind == AST_EXPR_VAR) {
        Var *sv = env_find(r->env, init->name);
        if (sv && sv->members) {
            free_struct_members(v->members);
            v->members = clone_struct_members(sv->members);
        }
        return;
    }

    /* init bir fonksiyon çağrısı: return { ... } veya return struct */
    if (init->kind == AST_EXPR_CALL) {
        /* call_expr eval edildiğinde return_struct/return_init_list dolu olur */
        double dummy = eval_expr(init, r);
        (void)dummy;
        if (r->return_struct) {
            free_struct_members(v->members);
            v->members = clone_struct_members(r->return_struct);
            free_struct_members(r->return_struct);
            r->return_struct = NULL;
        } else if (r->return_init_list) {
            fill_members_from_init_list(r, v->members, r->return_init_list);
            r->return_init_list = NULL;
        }
        return;
    }
}

/* ---------- GCL lib modülleri (#lib <Ad>) ---------- */

/* eval_expr ve call_user_func ön bildirimi */
static double eval_expr(GclExpr *e, Runner *r);
static double call_user_func(Runner *r, FuncDef *fd, GclExpr **args, int arg_count);

/* AST_EXPR_MEMBER zincirini çöz: p.addr.city -> son GclStructValue */
static GclStructValue *resolve_member_chain(GclExpr *e, Runner *r) {
    if (!e) return NULL;
    if (e->kind == AST_EXPR_VAR) {
        Var *v = env_find(r->env, e->name);
        return v ? v->members : NULL;
    }
    /* if this is an array access that selects a struct element, return that element's members */
    if (e->kind == AST_EXPR_ARRAY) {
        /* left should resolve to a var or member chain, right is index */
        if (e->left && e->left->kind == AST_EXPR_VAR) {
            Var *v = env_find(r->env, e->left->name);
            if (v && v->arr_members && e->right) {
                double idx = eval_expr(e->right, r);
                int i = (int)idx;
                if (i >= 0 && i < v->arr_count) return v->arr_members[i];
            }
        }
    }
    if (e->kind == AST_EXPR_MEMBER) {
        GclStructValue *parent = resolve_member_chain(e->left, r);
        if (!parent) return NULL;
        /* Nested zincir: e->left MEMBER ise parent son member'dır,
           onun members listesinde aranmalı. e->left VAR ise parent zaten
           kök struct'ın üye listesidir. */
        GclStructValue *search_from = parent;
        if (e->left && e->left->kind == AST_EXPR_MEMBER) {
            search_from = parent->members;
            if (!search_from) return NULL;
        }
        for (GclStructValue *m = search_from; m; m = m->next) {
            if (m->name && strcmp(m->name, e->member_name) == 0) return m;
        }
    }
    return NULL;
}

/* Struct üyelerini native modüle recursive düzleştir: nested struct (Vector3 vb.)
   içindeki alt float üyeleri de sırayla açılır. Camera3D.position → 3 arg. */
static void flatten_struct_members(GclStructValue *m, const char **argv,
                                   char (*numbuf)[64], char (*chbuf)[2], int *ac, int cap) {
    for (; m && *ac < cap; m = m->next) {
        if (m->members) {
            flatten_struct_members(m->members, argv, numbuf, chbuf, ac, cap);
        } else if (m->is_string && m->str) {
            argv[*ac] = m->str;
            (*ac)++;
        } else {
            int idx = *ac;
            snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", m->num);
            argv[idx] = numbuf[idx];
            (*ac)++;
        }
    }
}

/* #lib support removed */

/* Modül bul */
static NativeModule *native_find(GclEnv *env, const char *name) {
    for (NativeModule *m = env->modules; m; m = m->next) {
        if (strcmp(m->name, name) == 0) return m;
    }
    return NULL;
}

/* scanf(name) veya scanf("format", var...) — güvenli input, env'e yazar */
static double do_scanf(Runner *r, GclExpr **args, int arg_count) {
    if (arg_count < 1 || !args[0]) return 0.0;
    GclExpr *var = NULL;
    if (args[0]->kind == AST_EXPR_VAR) {
        /* scanf(name) — tek argüman: değişkene oku */
        var = args[0];
    } else if (args[0]->kind == AST_EXPR_STRING && arg_count >= 2 &&
               args[1] && args[1]->kind == AST_EXPR_VAR) {
        /* scanf("format", var) — format + hedef değişken */
        var = args[1];
    } else {
        return 0.0;
    }
    Var *v = env_find(r->env, var->name);
    if (!v) return 0.0;
    /* vanilla char — tek karakter */
    if (v->decl_type && strcmp(v->decl_type, "char") == 0 && !v->is_pointer && v->array_size == 0) {
        char c;
        if (scanf("%c", &c) == 1) {
            env_set_num(r->env, var->name, (double)c);
            int ch; while ((ch = getchar()) != '\n' && ch != EOF);
        }
        return 1.0;
    }
    /* gcChar / char* / char[N] — fgets */
    if (v->is_string || (v->decl_type && strcmp(v->decl_type, "gcChar") == 0) ||
        v->is_pointer || v->array_size >= 1) {
        int limit = (v->array_size >= 1) ? v->array_size : 1024;
        char buf[1024];
        if (fgets(buf, sizeof(buf), stdin)) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            if ((int)len >= limit) { buf[limit-1] = '\0'; }
            env_set_str(r->env, var->name, buf);
        }
        return 1.0;
    }
    double n;
    if (scanf("%lf", &n) == 1) env_set_num(r->env, var->name, n);
    return 1.0;
}

static NativeModule *native_load(GclEnv *env, const char *name);

/* Modül fonksiyonunu çağır: Modul.member(args) */
static double call_native_member(const char *module_name, const char *member,
                                 GclExpr **args, int arg_count, Runner *r) {
    NativeModule *mod = native_find(r->env, module_name);
    if (!mod) {
        /* Yerleşik modül ise (Math/Stdio/Embed) tembel yükle — bare printf/scanf için */
        mod = native_load(r->env, module_name);
    }
    if (!mod) {
        fprintf(stderr, "Runtime error: unknown module '%s'\n", module_name);
        return 0.0;
    }
    /* Stdio.scanf — gerçek input okuma, env'e yaz (bare scanf ile aynı) */
    if (strcmp(module_name, "Stdio") == 0 && strcmp(member, "scanf") == 0) {
        return do_scanf(r, args, arg_count);
    }
    for (int i = 0; i < mod->entry_count; i++) {
        if (strcmp(mod->entries[i].name, member) == 0) {
            /* GclExpr argümanlarını string dizisine çevir.
               STRUCT DEĞİŞKEN argümanı: üyeleri bildirim sırasıyla genişletilir.
               Örn. Camera3D camera → BeginMode3D(camera) → 11 arg (position.x,y,z,
               target.x,y,z, up.x,y,z, fovy, projection). Bu, GCL typedef struct
               sisteminin native modüllere gerçek köprüsüdür. */
            const char *argv[256];
            char numbuf[256][64];
            char chbuf[256][2];
            int ac = 0;
            for (int j = 0; j < arg_count && ac < 255; j++) {
                GclExpr *a = args[j];
                if (!a) { argv[ac++] = ""; continue; }
                /* Address-of: &camera → alttaki struct üyelerini düzleştir (UpdateCamera(&camera, ...)) */
                if (a->kind == AST_EXPR_UNOP && a->op == OP_BITAND && a->right) {
                    GclExpr *inner = a->right;
                    if (inner->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, inner->name);
                        if (v && v->members) {
                            flatten_struct_members(v->members, argv, numbuf, chbuf, &ac, 255);
                            continue;
                        }
                    }
                }
                if (a->kind == AST_EXPR_VAR) {
                    Var *v = env_find(r->env, a->name);
                    /* Struct değişken → üyeleri bildirim sırasıyla genişlet */
                    if (v && v->members) {
                        flatten_struct_members(v->members, argv, numbuf, chbuf, &ac, 255);
                        continue;
                    }
                    if (v && v->is_string) {
                        argv[ac++] = v->str ? v->str : "";
                    } else if (v && v->decl_type && (strcmp(v->decl_type, "uint64") == 0 || strcmp(v->decl_type, "uint128") == 0) && v->str) {
                        argv[ac++] = v->str;
                    } else if (v && v->decl_type && strcmp(v->decl_type, "char") == 0 &&
                               v->array_size == 0 && !v->is_pointer && !v->is_string) {
                        /* vanilla char -> tek karakter */
                        int idx = ac;
                        chbuf[idx][0] = (char)(int)v->num;
                        chbuf[idx][1] = '\0';
                        argv[ac++] = chbuf[idx];
                    } else {
                        int idx = ac;
                        snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", v ? v->num : 0.0);
                        argv[ac++] = numbuf[idx];
                    }
                } else if (a->kind == AST_EXPR_ASSIGN && a->right) {
                    /* named/pozisyonel atama argümanı: sağ tarafın değerini kullan */
                    GclExpr *right = a->right;
                    if (right->kind == AST_EXPR_STRING) {
                        argv[ac++] = right->str ? right->str : "";
                    } else if (right->kind == AST_EXPR_VAR) {
                        Var *rv = env_find(r->env, right->name);
                        if (rv && rv->is_string) {
                            argv[ac++] = rv->str ? rv->str : "";
                        } else {
                            int idx = ac;
                            snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", rv ? rv->num : 0.0);
                            argv[ac++] = numbuf[idx];
                        }
                    } else {
                        int idx = ac;
                        snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", eval_expr(right, r));
                        argv[ac++] = numbuf[idx];
                    }
                    continue;
                } else if (a->kind == AST_EXPR_STRING) {
                    /* String argüman: InitWindow(..., "GCL 3D Project") gibi başlıklar
                       doğru iletilsin. Eski kod bunu eval_expr ile sayıya çevirip
                       "0" gönderiyordu (pencer başlığı "0" oluyordu). */
                    argv[ac++] = a->str ? a->str : "";
                } else if (a->kind == AST_EXPR_ARRAY) {
                    /* argv[i] — string olarak geç; ayrıca normal dizi arr[i] desteği. */
                    if (a->left && a->left->kind == AST_EXPR_VAR &&
                        strcmp(a->left->name, "argv") == 0 && r->env->arg_vals) {
                        int idx = (int)eval_expr(a->right, r);
                        if (idx >= 0 && idx < r->env->arg_count && r->env->arg_vals[idx])
                            argv[ac++] = r->env->arg_vals[idx];
                        else
                            argv[ac++] = "";
                    } else if (a->left && a->left->kind == AST_EXPR_VAR) {
                        Var *arr = env_find(r->env, a->left->name);
                        int idx = (int)eval_expr(a->right, r);
                        if (arr && idx >= 0) {
                            int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                            if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                            if (arr->arr_strs && idx < arr->arr_count) {
                                argv[ac++] = arr->arr_strs[idx] ? arr->arr_strs[idx] : "";
                            } else if (arr->arr_vals && idx < arr->arr_count) {
                                if (arr->decl_type && strcmp(arr->decl_type, "char") == 0) {
                                    int ci = ac;
                                    chbuf[ci][0] = (char)(int)arr->arr_vals[idx];
                                    chbuf[ci][1] = '\0';
                                    argv[ac++] = chbuf[ci];
                                } else {
                                    int idx2 = ac;
                                    snprintf(numbuf[idx2], sizeof(numbuf[idx2]), "%.17g", arr->arr_vals[idx]);
                                    argv[ac++] = numbuf[idx2];
                                }
                            } else if (arr->is_string && arr->str && idx < bounds) {
                                int ci = ac;
                                chbuf[ci][0] = arr->str[idx];
                                chbuf[ci][1] = '\0';
                                argv[ac++] = chbuf[ci];
                            } else {
                                int idx2 = ac;
                                snprintf(numbuf[idx2], sizeof(numbuf[idx2]), "%.17g", eval_expr(a, r));
                                argv[ac++] = numbuf[idx2];
                            }
                        } else {
                            int idx2 = ac;
                            snprintf(numbuf[idx2], sizeof(numbuf[idx2]), "%.17g", eval_expr(a, r));
                            argv[ac++] = numbuf[idx2];
                        }
                    } else {
                        int idx2 = ac;
                        snprintf(numbuf[idx2], sizeof(numbuf[idx2]), "%.17g", eval_expr(a, r));
                        argv[ac++] = numbuf[idx2];
                    }
                    continue;
                } else if (a->kind == AST_EXPR_MEMBER) {
                    /* Native modül sabiti: Raylib.WHITE gibi (parantezsiz). */
                    int native_resolved = 0;
                    if (a->left && a->left->kind == AST_EXPR_VAR) {
                        NativeModule *nm = native_find(r->env, a->left->name);
                        if (nm && a->member_name) {
                            for (int k = 0; k < nm->entry_count; k++) {
                                if (strcmp(nm->entries[k].name, a->member_name) == 0) {
                                    int idx = ac;
                                    double nv = nm->entries[k].fn(0, NULL);
                                    snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", nv);
                                    argv[ac++] = numbuf[idx];
                                    native_resolved = 1;
                                    break;
                                }
                            }
                        }
                    }
                    if (!native_resolved) {
                        /* struct member fallback (nested destekli) */
                        GclStructValue *mv = resolve_member_chain(a, r);
                        if (mv && mv->is_string && mv->str) {
                            argv[ac++] = mv->str;
                        } else if (mv) {
                            int idx = ac;
                            snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", mv->num);
                            argv[ac++] = numbuf[idx];
                        } else {
                            int idx = ac;
                            snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", eval_expr(a, r));
                            argv[ac++] = numbuf[idx];
                        }
                    }
                    continue;
                } else {
                    /* Diğer tüm argüman türleri (number, binop, vs.) */
                    int idx = ac;
                    snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", eval_expr(a, r));
                    argv[ac++] = numbuf[idx];
                }
            }
            return mod->entries[i].fn(ac, argv);
        }
    }
    fprintf(stderr, "Runtime error: unknown member '%s.%s'\n", module_name, member);
    return 0.0;
}

/* Aşağıdaki eski kod bloğu kaldırıldı (yukarıda yeni genişletilmiş mantık var) */
#if 0
    for (int i = 0; i < mod->entry_count; i++) {
        if (strcmp(mod->entries[i].name, member) == 0) {
            /* GclExpr argümanlarını string dizisine çevir */
            const char *argv[32];
            char numbuf[32][64];
            char chbuf[32][2];
            int ac = arg_count < 32 ? arg_count : 32;
            for (int j = 0; j < ac; j++) {
                GclExpr *a = args[j];
                if (!a) { argv[j] = ""; continue; }
                if (a->kind == AST_EXPR_STRING) {
                    argv[j] = a->str ? a->str : "";
                } else if (a->kind == AST_EXPR_VAR) {
                    Var *v = env_find(r->env, a->name);
                    if (v && v->is_string) argv[j] = v->str ? v->str : "";
                    else if (v && v->decl_type && (strcmp(v->decl_type, "uint64") == 0 || strcmp(v->decl_type, "uint128") == 0) && v->str) {
                        argv[j] = v->str;
                    }
                    else if (v && v->decl_type && strcmp(v->decl_type, "char") == 0 &&
                             v->array_size == 0 && !v->is_pointer && !v->is_string) {
                        /* vanilla char -> tek karakter */
                        chbuf[j][0] = (char)(int)v->num;
                        chbuf[j][1] = '\0';
                        argv[j] = chbuf[j];
                    } else { snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", v ? v->num : 0.0); argv[j] = numbuf[j]; }
                } else if (a->kind == AST_EXPR_ASSIGN && a->right) {
                    /* named argument: file="file", text="", value=..., type="python" */
                    GclExpr *val = a->right;
                    if (val->kind == AST_EXPR_STRING) argv[j] = val->str ? val->str : "";
                    else if (val->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, val->name);
                        if (v && v->is_string) argv[j] = v->str ? v->str : "";
                        else { snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", v ? v->num : 0.0); argv[j] = numbuf[j]; }
                    } else {
                        snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(val, r));
                        argv[j] = numbuf[j];
                    }
                } else if (a->kind == AST_EXPR_MEMBER) {
                    /* Native modül sabiti: Raylib.WHITE gibi (parantezsiz).
                       Önce bu modülün üyesi olarak ara — struct member değil! */
                    int native_resolved = 0;
                    if (a->left && a->left->kind == AST_EXPR_VAR) {
                        NativeModule *nm = native_find(r->env, a->left->name);
                        if (nm && a->member_name) {
                            for (int k = 0; k < nm->entry_count; k++) {
                                if (strcmp(nm->entries[k].name, a->member_name) == 0) {
                                    double nv = nm->entries[k].fn(0, NULL);
                                    snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", nv);
                                    argv[j] = numbuf[j];
                                    native_resolved = 1;
                                    break;
                                }
                            }
                        }
                    }
                    if (!native_resolved) {
                        /* struct member fallback (nested destekli) */
                        GclStructValue *mv = resolve_member_chain(a, r);
                        if (mv && mv->is_string && mv->str) {
                            argv[j] = mv->str;
                        } else if (mv) {
                            snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", mv->num);
                            argv[j] = numbuf[j];
                        } else {
                            snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(a, r));
                            argv[j] = numbuf[j];
                        }
                    }
                } else if (a->kind == AST_EXPR_ARRAY) {
                    /* argv[i] — string olarak geç */
                    if (a->left && a->left->kind == AST_EXPR_VAR &&
                        strcmp(a->left->name, "argv") == 0 && r->env->arg_vals) {
                        int idx = (int)eval_expr(a->right, r);
                        if (idx >= 0 && idx < r->env->arg_count && r->env->arg_vals[idx]) {
                            argv[j] = r->env->arg_vals[idx];
                        } else {
                            argv[j] = "";
                        }
                    } else if (a->left && a->left->kind == AST_EXPR_VAR) {
                        /* Regular array: arr[i] → element değerini getir */
                        Var *arr = env_find(r->env, a->left->name);
                        int idx = (int)eval_expr(a->right, r);
                        if (arr && idx >= 0) {
                            int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                            if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                            if (arr->arr_strs && idx < arr->arr_count) {
                                argv[j] = arr->arr_strs[idx];
                            } else if (arr->arr_vals && idx < arr->arr_count) {
                                /* If declared as char, present as single-char string */
                                if (arr->decl_type && strcmp(arr->decl_type, "char") == 0) {
                                    chbuf[j][0] = (char)(int)arr->arr_vals[idx];
                                    chbuf[j][1] = '\0';
                                    argv[j] = chbuf[j];
                                } else {
                                                /* For unsigned 64/128 types, if arr_strs is available prefer it. */
                                                if (arr->decl_type && (strcmp(arr->decl_type, "uint64") == 0 || strcmp(arr->decl_type, "uint128") == 0) && arr->arr_strs && idx < arr->arr_count) {
                                                    argv[j] = arr->arr_strs[idx];
                                                } else {
                                                    snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", arr->arr_vals[idx]);
                                                    argv[j] = numbuf[j];
                                                }
                                }
                            } else if (arr->is_string && arr->str) {
                                /* string var with index: return single-character string if in bounds */
                                if (idx >= 0 && idx < bounds && arr->str[idx]) {
                                    chbuf[j][0] = arr->str[idx];
                                    chbuf[j][1] = '\0';
                                    argv[j] = chbuf[j];
                                } else {
                                    argv[j] = "";
                                }
                            } else {
                                snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(a, r));
                                argv[j] = numbuf[j];
                            }
                        } else {
                            snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(a, r));
                            argv[j] = numbuf[j];
                        }
                    } else {
                        snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(a, r));
                        argv[j] = numbuf[j];
                    }
                } else {
                    snprintf(numbuf[j], sizeof(numbuf[j]), "%.17g", eval_expr(a, r));
                    argv[j] = numbuf[j];
                }
            }
            return mod->entries[i].fn(ac, argv);
        }
    }
    fprintf(stderr, "Runtime error: unknown member '%s.%s'\n", module_name, member);
    return 0.0;
#endif

/* Modül yükle: #native <Ad> — exe'nin Library/, base_dir/Library, cwd/Library, PATH sırasıyla */
static NativeModule *native_load(GclEnv *env, const char *name) {
    if (native_find(env, name)) return native_find(env, name);
    char path[4096];
    const char *dll_ext = "dll";
#ifdef _WIN32
    dll_ext = "dll";
#else
    dll_ext = "so";
#endif
    NativeModule *m = (NativeModule *)calloc(1, sizeof(NativeModule));
    if (!m) return NULL;
    m->name = strdup(name);

    /* exe'nin bulunduğu dizini bul (Library/ onun altında) */
    char exe_dir[4096] = "";
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_dir, (DWORD)sizeof(exe_dir));
    char *bs = strrchr(exe_dir, '\\');
    if (bs) *bs = '\0';
    else { char *sl = strrchr(exe_dir, '/'); if (sl) *sl = '\0'; }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (n > 0) { exe_dir[n] = '\0'; char *sl = strrchr(exe_dir, '/'); if (sl) *sl = '\0'; }
    }
#endif

    int found = 0;
    /* Linux: Embed.so/gcl.so vb. libpython3.14.so.1.0'a DT_NEEDED bağımlılığı taşır.
       LD_LIBRARY_PATH'ı setenv ile değiştirmek dinamik linker başladıktan sonra
       etkisizdir. Bu yüzden libpython'u önce manuel dlopen ile yükle —
       böylece Embed.so açılırken bağımlılık bellekte çözülmüş olur. */
#ifdef __linux__
    {
        char py_lib[4096];
        snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.14.so.1.0", exe_dir);
        if (access(py_lib, 0) == 0) {
            void *h = dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
            if (!h) {
                /* alternatif: libpython3.so */
                snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.so", exe_dir);
                if (access(py_lib, 0) == 0) dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
            }
        }
    }
#endif
    /* 1) exe_dir/Library/name.dll */
    if (exe_dir[0]) {
        snprintf(path, sizeof(path), "%s/Library/%s.%s", exe_dir, name, dll_ext);
        if (access(path, 0) == 0) found = 1;
    }
    /* 2) base_dir/Library/name.dll */
    if (!found && env->base_dir[0]) {
        snprintf(path, sizeof(path), "%s/Library/%s.%s", env->base_dir, name, dll_ext);
        if (access(path, 0) == 0) found = 1;
    }
    /* 3) cwd/Library/name.dll */
    if (!found) {
        snprintf(path, sizeof(path), "Library/%s.%s", name, dll_ext);
        if (access(path, 0) == 0) found = 1;
    }
    /* 4) name.dll (PATH) */
    if (!found) {
        snprintf(path, sizeof(path), "%s.%s", name, dll_ext);
        if (access(path, 0) != 0) { free(m->name); free(m); return NULL; }
        found = 1;
    }
    if (!found) { free(m->name); free(m); return NULL; }

#ifdef _WIN32
    m->handle = LoadLibraryA(path);
    if (!m->handle) { free(m->name); free(m); return NULL; }
#else
    m->handle = dlopen(path, RTLD_LAZY);
    if (!m->handle) { free(m->name); free(m); return NULL; }
#endif
    /* Export adı: gcl_<name>_get_functions — küçük harfe çevir */
    char export_name[256];
    snprintf(export_name, sizeof(export_name), "gcl_%s_get_functions", name);
    for (char *p = export_name; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p += ('a' - 'A');
    }
#ifdef _WIN32
    GclModuleGetFunctions gf = NULL;
    {
        void *sym2 = (void *)GetProcAddress(m->handle, export_name);
        memcpy(&gf, &sym2, sizeof(gf));
    }
#else
    GclModuleGetFunctions gf = (GclModuleGetFunctions)dlsym(m->handle, export_name);
#endif
    if (!gf) { free(m->name); free(m); return NULL; }
    m->entries = gf(&m->entry_count);
    m->next = env->modules;
    env->modules = m;
    return m;
}

/* ---------- #extern DLL + #register fonksiyonlar ---------- */

/* Extern DLL bul */
static GclExternDll *extern_dll_find(GclEnv *env, const char *dll_name) {
    for (int i = 0; i < env->extern_dll_count; i++) {
        if (strcmp(env->extern_dlls[i].dll_name, dll_name) == 0) return &env->extern_dlls[i];
    }
    return NULL;
}

/* Extern DLL yükle — exe_dir, base_dir, cwd, sırasıyla aranır */
static GclExternDll *extern_dll_load(GclEnv *env, const char *name) {
    GclExternDll *d = extern_dll_find(env, name);
    if (d) return d;
    if (env->extern_dll_count >= (int)(sizeof(env->extern_dlls) / sizeof(env->extern_dlls[0]))) return NULL;

    char path[4096];
    char exe_dir[4096] = "";
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_dir, (DWORD)sizeof(exe_dir));
    char *bs = strrchr(exe_dir, '\\');
    if (bs) *bs = '\0';
    else { char *sl = strrchr(exe_dir, '/'); if (sl) *sl = '\0'; }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
        if (n > 0) { exe_dir[n] = '\0'; char *sl = strrchr(exe_dir, '/'); if (sl) *sl = '\0'; }
    }
#endif

    int found = 0;
    if (exe_dir[0]) { snprintf(path, sizeof(path), "%s/%s", exe_dir, name); if (access(path, 0) == 0) found = 1; }
    if (!found && env->base_dir[0]) { snprintf(path, sizeof(path), "%s/%s", env->base_dir, name); if (access(path, 0) == 0) found = 1; }
    if (!found) { snprintf(path, sizeof(path), "%s", name); if (access(path, 0) != 0) return NULL; }
    if (!found) return NULL;

    GclExternDll *new_d = &env->extern_dlls[env->extern_dll_count];
    memset(new_d, 0, sizeof(*new_d));
    new_d->dll_name = strdup(name);
#ifdef _WIN32
    new_d->handle = (void *)LoadLibraryA(path);
#else
    new_d->handle = dlopen(path, RTLD_LAZY);
#endif
    if (!new_d->handle) { free(new_d->dll_name); memset(new_d, 0, sizeof(*new_d)); return NULL; }
    env->extern_dll_count++;
    return new_d;
}

/* "ret|func|params" kayıt dizesini parse et ve GclExternReg'e yaz */
static GclExternType extern_parse_type(const char *s) {
    if (!s) return GCL_EXT_VOID;
    /* Parametre adı dahil: "int width" → INT, "const char *title" → STRING,
       "char *buf" → STRING, "double x" → DOUBLE, "float f" → INT. */
    if (strstr(s, "char")) return GCL_EXT_STRING;
    if (strstr(s, "int") || strstr(s, "float") || strstr(s, "long")) return GCL_EXT_INT;
    if (strstr(s, "double")) return GCL_EXT_DOUBLE;
    return GCL_EXT_VOID;
}

static void extern_register_parse(GclEnv *env, const char *reg) {
    if (!reg || env->extern_reg_count >= (int)(sizeof(env->extern_regs) / sizeof(env->extern_regs[0]))) return;
    /* format: ret|func|params */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", reg);
    char *p1 = strchr(buf, '|');
    if (!p1) return;
    *p1 = '\0';
    char *p2 = strchr(p1 + 1, '|');
    if (!p2) return;
    *p2 = '\0';

    GclExternReg *er = &env->extern_regs[env->extern_reg_count];
    memset(er, 0, sizeof(*er));

    er->dll_name = strdup("");  /* şimdilik DLL adı register'dan bağımsız — tüm extern DLL'lerde aranır */
    er->func_name = strdup(p1 + 1);
    er->ret = extern_parse_type(buf);

    /* params: "int width, int height, const char *title" */
    char *params = p2 + 1;
    er->param_count = 0;
    char *save = NULL;
    char *tok = strtok_r(params, ",", &save);
    while (tok && er->param_count < 8) {
        while (*tok == ' ' || *tok == '\t') tok++;
        /* sondaki boşlukları ve ')' ';' karakterlerini temizle (son token'da ");" kalır) */
        char *e = tok + strlen(tok) - 1;
        while (e >= tok && (*e == ' ' || *e == '\t' || *e == ')' || *e == ';')) *e-- = '\0';
        /* parametre adını ayır: "const char *title" → tip "const char *" */
        er->params[er->param_count++] = extern_parse_type(tok);
        tok = strtok_r(NULL, ",", &save);
    }
    env->extern_reg_count++;
}

/* Extern fonksiyon bul: isim eşleşirse fn_ptr'ı döndür */
static void *extern_func_find(GclEnv *env, const char *func_name) {
    for (int i = 0; i < env->extern_reg_count; i++) {
        GclExternReg *er = &env->extern_regs[i];
        if (strcmp(er->func_name, func_name) == 0) {
            if (er->fn_ptr) return er->fn_ptr;
            /* fn_ptr yoksa tüm DLL'lerde ara */
            for (int j = 0; j < env->extern_dll_count; j++) {
                if (!env->extern_dlls[j].handle) continue;
#ifdef _WIN32
                void *sym = (void *)GetProcAddress((HMODULE)env->extern_dlls[j].handle, func_name);
#else
                void *sym = dlsym(env->extern_dlls[j].handle, func_name);
#endif
                if (sym) {
                    er->fn_ptr = sym;
                    return sym;
                }
            }
        }
    }
    return NULL;
}

/* Extern C fonksiyonunu çağır — tip dönüşümleri yaparak */
static double call_extern_func(GclEnv *env, const char *func_name, GclExpr **args, int arg_count, Runner *r) {
    GclExternReg *er = NULL;
    for (int i = 0; i < env->extern_reg_count; i++) {
        if (strcmp(env->extern_regs[i].func_name, func_name) == 0) { er = &env->extern_regs[i]; break; }
    }
    if (!er) {
        fprintf(stderr, "Runtime error: unknown function '%s'\n", func_name);
        return 0.0;
    }
    void *fp = er->fn_ptr;
    if (!fp) fp = extern_func_find(env, func_name);
    if (!fp) {
        fprintf(stderr, "Runtime error: cannot resolve external function '%s'\n", func_name);
        return 0.0;
    }

    /* GCL değerlerini C tiplerine dönüştür ve çağır */
    int n = arg_count < er->param_count ? arg_count : er->param_count;

    /* Basit: en fazla 8 int/double/char* destekle */
    long long a_ll[8];
    double a_dd[8];
    char *a_ss[8];
    for (int i = 0; i < n; i++) {
        GclExpr *a = args[i];
        if (!a) { a_ll[i] = 0; a_dd[i] = 0; a_ss[i] = NULL; continue; }
        switch (er->params[i]) {
            case GCL_EXT_INT:
                a_ll[i] = (long long)eval_expr(a, r);
                break;
            case GCL_EXT_DOUBLE:
                a_dd[i] = eval_expr(a, r);
                break;
            case GCL_EXT_STRING:
                if (a->kind == AST_EXPR_STRING) a_ss[i] = a->str ? a->str : (char *)"";
                else if (a->kind == AST_EXPR_VAR) {
                    Var *v = env_find(env, a->name);
                    a_ss[i] = (v && v->is_string && v->str) ? v->str : (char *)"";
                } else a_ss[i] = (char *)"";
                break;
            default:
                a_ll[i] = 0; a_dd[i] = 0; a_ss[i] = NULL;
        }
    }

    /* ret tipine göre çağır — 0(n) fonksiyonlar:
       Basit ABI: ilk 4 arg register (x86-64 Windows: rcx, rdx, r8, r9) */
    switch (er->ret) {
        case GCL_EXT_VOID: {
            /* void fonksiyonlar: f(a0, a1, ...) — sadece int/double/char* */
            /* x86-64 System V / Windows'da küçük fonksiyonlar için ortak:
               en basit yol: int argümanlar için int, double için double, char* için char* — union yok */
            /* Burada sadece ilk 4 argümanı basitça kullanıyoruz */
            /* f(int,int,char*) örneği: InitWindow(800,600,"title") */
            /* Fonksiyon imzasına göre tek tek call yapmak yerine, tüm tipleri ayrı çağırmak zor.
               Pratik: int ve string için en yaygın kombinasyon:
               - (int,int) → 2 int
               - (int,int,char*) → 2 int + 1 string
               Burada tüm kombinasyonları desteklemek için switch-case — ama çok fazla.
               En basit: ints, doubles, strings dizilerini hazırla ve imzasına göre çağır. */
            /* Sınırlı destek: ilk 4 param int/double/string karışımı — ayrı ayrı */
            /* Aşağıda N parametreli void fonksiyonlar için 8'e kadar kombinasyon:
               Ancak C'de aynı fonksiyon farklı tiplerle çağrılamaz — imza sabittir.
               Bu yüzden imzadaki tipleri kullanarak doğrudan çağırıyoruz. */
            /* Pratik yaklaşım: int,double,string olarak ayrı ayrı 3 varyant */
            switch (er->param_count) {
                case 0: ((void (*)(void))fp)(); break;
                case 1:
                    switch (er->params[0]) {
                        case GCL_EXT_INT: ((void (*)(long long))fp)(a_ll[0]); break;
                        case GCL_EXT_DOUBLE: ((void (*)(double))fp)(a_dd[0]); break;
                        case GCL_EXT_STRING: ((void (*)(char *))fp)(a_ss[0]); break;
                        default: break;
                    }
                    break;
                case 2:
                    if (er->params[0] == GCL_EXT_INT && er->params[1] == GCL_EXT_INT)
                        ((void (*)(long long, long long))fp)(a_ll[0], a_ll[1]);
                    break;
                case 3:
                    if (er->params[0] == GCL_EXT_INT && er->params[1] == GCL_EXT_INT && er->params[2] == GCL_EXT_STRING)
                        ((void (*)(long long, long long, char *))fp)(a_ll[0], a_ll[1], a_ss[2]);
                    break;
                default: break;
            }
            return 0.0;
        }
        case GCL_EXT_INT: {
            if (er->param_count == 0) return (double)((long long (*)(void))fp)();
            if (er->param_count == 1 && er->params[0] == GCL_EXT_INT)
                return (double)((long long (*)(long long))fp)(a_ll[0]);
            return 0.0;
        }
        case GCL_EXT_DOUBLE: {
            if (er->param_count == 1 && er->params[0] == GCL_EXT_DOUBLE)
                return ((double (*)(double))fp)(a_dd[0]);
            return 0.0;
        }
        default: return 0.0;
    }
}

/* ---------- Expression eval ---------- */

static double eval_expr(GclExpr *e, Runner *r);

/* exec_block/exec_stmt ön bildirimi (call_user_func bunları kullanır) */
static int exec_stmt(GclStmt *s, Runner *r);
static int exec_block(GclStmt *blk, Runner *r);

/* Kullanıcı fonksiyonunu çağır */
static double call_user_func(Runner *r, FuncDef *fd, GclExpr **args, int arg_count) {
    /* Fonksiyon çağrısından ÖNCE var listesinin başını kaydet —
       içeride yeni oluşturulan local (global olmayan) değişkenleri
       fonksiyon çıkışında temizlemek için. */
    Var *orig_head = r->env->vars;
    /* Parametreleri env'e bağla — geçici: eski değerleri sakla ve geri yükle.
       env_set_num, mevcut string değişkenin str alanını free ettiği için
       restore'da dangling pointer okumamak adına str kopyasını sakla. */
    Var *saved[32];
    char *saved_str[32] = {0};
    int saved_is_string[32] = {0};
    int saved_count = 0;
    for (int i = 0; i < fd->param_count && i < arg_count; i++) {
        Var *existing = env_find(r->env, fd->params[i]);
        if (existing && saved_count < 32) {
            /* saved_count 32'yi aştığında push edilmez — taşma önlenir */
            saved[saved_count] = existing;
            saved_is_string[saved_count] = existing->is_string;
            saved_str[saved_count] = existing->str ? strdup(existing->str) : NULL;
            saved_count++;
        }
        /* String argümanlar env_set_num ile bağlanırsa AST_EXPR_STRING 0.0
           döndürür ve string değer kaybolur. String ise env_set_str kullan. */
        GclExpr *arg = args[i];
        if (arg && arg->kind == AST_EXPR_STRING) {
            env_set_str(r->env, fd->params[i], arg->str ? arg->str : "");
        } else if (arg && arg->kind == AST_EXPR_VAR) {
            Var *av = env_find(r->env, arg->name);
            if (av && av->is_string) {
                env_set_str(r->env, fd->params[i], av->str ? av->str : "");
            } else {
                env_set_num(r->env, fd->params[i], eval_expr(arg, r));
            }
        } else {
            env_set_num(r->env, fd->params[i], arg ? eval_expr(arg, r) : 0.0);
        }
    }
    Runner sub;
    memcpy(&sub, r, sizeof(sub));
    sub.return_flag = 0;
    sub.return_value = 0;
    sub.break_flag = 0;
    sub.continue_flag = 0;
    sub.return_struct = NULL;
    sub.return_init_list = NULL;
    if (fd->body && fd->body->kind == STMT_BLOCK) exec_block(fd->body, &sub);
    else if (fd->body) exec_stmt(fd->body, &sub);
    double ret = sub.return_value;
    if (sub.return_struct) {
        if (r->return_struct) free_struct_members(r->return_struct);
        r->return_struct = clone_struct_members(sub.return_struct);
        free_struct_members(sub.return_struct);
    }
    if (sub.return_init_list) {
        r->return_init_list = sub.return_init_list;
    }
    /* eski değerleri geri yükle — str kopyası saved_str'den (use-after-free önlemi) */
    for (int i = 0; i < fd->param_count && i < arg_count; i++) {
        Var *v = env_find(r->env, fd->params[i]);
        if (v) {
            for (int j = 0; j < saved_count; j++) {
                if (saved[j] == v) {
                    v->num = saved[j]->num;
                    v->is_string = saved_is_string[j];
                    if (v->str) free(v->str);
                    v->str = saved_str[j] ? strdup(saved_str[j]) : NULL;
                    free(saved_str[j]);
                    saved_str[j] = NULL;
                    break;
                }
            }
        }
    }
    /* Fonksiyon içinde oluşturulan LOCAL (global olmayan) değişkenleri temizle.
       Global değişkenler (call_user_func öncesinde de varsa) korunur. */
    {
        Var **pp = &r->env->vars;
        while (*pp) {
            Var *v = *pp;
            int in_orig = 0;
            for (Var *o = orig_head; o; o = o->next) {
                if (o == v) { in_orig = 1; break; }
            }
            if (!in_orig && !v->is_global) {
                *pp = v->next;
                if (v->name) free(v->name);
                if (v->str) free(v->str);
                if (v->decl_type) free(v->decl_type);
                if (v->members) free_struct_members(v->members);
                free(v);
            } else {
                pp = &v->next;
            }
        }
    }
    return ret;
}

static double eval_expr(GclExpr *e, Runner *r) {
    if (!e) return 0.0;
    switch (e->kind) {
        case AST_EXPR_FLOAT: return e->num;
        case AST_EXPR_STRING: return 0.0;
        case AST_EXPR_VAR: {
            Var *v = env_find(r->env, e->name);
            if (!v) {
                fprintf(stderr, "Runtime error: undefined variable '%s'\n", e->name);
                return 0.0;
            }
            return env_get_num(r->env, e->name);
        }
        case AST_EXPR_BINOP: {
            double l = eval_expr(e->left, r);
            double rr = eval_expr(e->right, r);
            switch (e->op) {
                case OP_ADD: return l + rr;
                case OP_SUB: return l - rr;
                case OP_MUL: return l * rr;
                case OP_DIV: return rr != 0 ? l / rr : 0.0;
                case OP_MOD: return rr != 0 ? fmod(l, rr) : 0.0;
                case OP_EQ: return l == rr;
                case OP_NE: return l != rr;
                case OP_LT: return l < rr;
                case OP_GT: return l > rr;
                case OP_LE: return l <= rr;
                case OP_GE: return l >= rr;
                case OP_AND: return (l != 0) && (rr != 0);
                case OP_OR: return (l != 0) || (rr != 0);
                case OP_XOR: return (l != 0) ^ (rr != 0);
                case OP_SHL: return (double)((long long)l << (int)rr);
                case OP_SHR: return (double)((long long)l >> (int)rr);
                case OP_BITAND: return (double)((long long)l & (long long)rr);
                case OP_BITOR: return (double)((long long)l | (long long)rr);
                case OP_BITXOR: return (double)((long long)l ^ (long long)rr);
                default: return 0.0;
            }
        }
        case AST_EXPR_UNOP: {
            double v = eval_expr(e->right, r);
            if (e->op == OP_SUB) return -v;
            if (e->op == OP_NOT) return (v == 0.0);   /* ! */
            if (e->op == OP_BITNOT) return (double)(~(long long)v); /* ~ */
            return v;
        }
        case AST_EXPR_ASSIGN: {
            double v = eval_expr(e->right, r);
            if (e->left && e->left->kind == AST_EXPR_VAR) {
                Var *tv = env_find(r->env, e->left->name);
                if (tv && tv->is_const) {
                    fprintf(stderr, "Runtime error: cannot assign to const '%s'\n", e->left->name);
                    r->error = 1;
                    return 0.0;
                }
                /* struct atama: student1 = getStudent(); */
                if (r->return_struct) {
                    Var *dst = env_find(r->env, e->left->name);
                    if (dst && dst->members) {
                        free_struct_members(dst->members);
                        dst->members = clone_struct_members(r->return_struct);
                        free_struct_members(r->return_struct);
                        r->return_struct = NULL;
                        return 0.0;
                    }
                }
                if (r->return_init_list) {
                    Var *dst = env_find(r->env, e->left->name);
                    if (dst && dst->members) {
                        fill_members_from_init_list(r, dst->members, r->return_init_list);
                        r->return_init_list = NULL;
                    }
                }
                /* String atama: gcChar/char* var — string değeri yaz */
                if (e->right->kind == AST_EXPR_STRING) {
                    env_set_str(r->env, e->left->name, e->right->str);
                } else if (e->right->kind == AST_EXPR_VAR) {
                    Var *rsv = env_find(r->env, e->right->name);
                    if (rsv && rsv->is_string) {
                        env_set_str(r->env, e->left->name, rsv->str ? rsv->str : "");
                    } else {
                        env_set_num(r->env, e->left->name, v);
                    }
                } else {
                    env_set_num(r->env, e->left->name, v);
                }
            } else if (e->left && e->left->kind == AST_EXPR_MEMBER) {
                /* nested destekli member atama: p.addr.city = "Ankara" */
                GclStructValue *mv = resolve_member_chain(e->left, r);
                if (mv) {
                    /* Nested struct atama: camera.position = (Vector3){...} → compound literal */
                    if (e->right->kind == AST_EXPR_INIT_LIST && mv->members) {
                        fill_members_from_init_list(r, mv->members, e->right);
                    } else if (e->right->kind == AST_EXPR_STRING) {
                        if (mv->str) free(mv->str);
                        mv->str = strdup(e->right->str ? e->right->str : "");
                        mv->is_string = 1;
                        mv->num = 0;
                    } else if (e->right->kind == AST_EXPR_VAR) {
                        Var *rsv = env_find(r->env, e->right->name);
                        if (rsv && rsv->is_string) {
                            if (mv->str) free(mv->str);
                            mv->str = strdup(rsv->str ? rsv->str : "");
                            mv->is_string = 1;
                            mv->num = 0;
                        } else {
                            mv->num = truncate_to_declared_type(v, mv->decl_type);
                            mv->is_string = 0;
                        }
                    } else {
                        mv->num = truncate_to_declared_type(v, mv->decl_type);
                        mv->is_string = 0;
                    }
                }
            } else if (e->left && e->left->kind == AST_EXPR_ARRAY) {
                /* dizi eleman ataması: a[0] = 5; — daha önce no-op'du */
                GclExpr *arr_expr = e->left;
                GclExpr *arr_base = arr_expr->left;
                GclExpr *idx_expr = arr_expr->right;
                if (arr_base && arr_base->kind == AST_EXPR_VAR && idx_expr) {
                    Var *arr = env_find(r->env, arr_base->name);
                    int idx = (int)eval_expr(idx_expr, r);
                    if (arr && idx >= 0) {
                        int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                        if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                        if (arr->arr_vals && idx < arr->arr_count) {
                            /* String değer verilirse arr_strs'e, sayı verilirse arr_vals'e yaz */
                            if (e->right->kind == AST_EXPR_STRING) {
                                if (!arr->arr_strs) {
                                    arr->arr_strs = (char **)calloc((size_t)arr->arr_count + 1, sizeof(char *));
                                }
                                if (arr->arr_strs[idx]) free(arr->arr_strs[idx]);
                                arr->arr_strs[idx] = strdup(e->right->str ? e->right->str : "");
                            } else {
                                double nv = eval_expr(e->right, r);
                                /* elemanı da bildirilen tipin sınırına kırp */
                                arr->arr_vals[idx] = truncate_to_declared_type(nv, arr->decl_type);
                            }
                        } else if (arr->arr_strs && idx < arr->arr_count) {
                            if (arr->arr_strs[idx]) free(arr->arr_strs[idx]);
                            arr->arr_strs[idx] = strdup(e->right->kind == AST_EXPR_STRING ?
                                                        (e->right->str ? e->right->str : "") :
                                                        "");
                        } else if (arr->is_string && arr->str && idx < bounds) {
                            /* char[N] string: tek karakter ata — SADECE karakteri yaz,
                               null-terminator EKLEME. strdup'lanmış tamponu taşırmasın
                               ve mevcut string'i ("Ali") "B" gibi kısaltmasın. */
                            if (e->right->kind == AST_EXPR_STRING && e->right->str && e->right->str[0]) {
                                arr->str[idx] = e->right->str[0];
                            } else {
                                arr->str[idx] = (char)(int)eval_expr(e->right, r);
                            }
                        }
                    }
                }
            }
            return v;
        }
        case AST_EXPR_CALL: {
            GclExpr *callee = e->left;
            if (callee && callee->kind == AST_EXPR_VAR) {
                const char *fn = callee->name;
                /* Bare printf("{}", ...) — simple_doc.md giriş dilinde Stdio. öneki yok */
                if (strcmp(fn, "printf") == 0) {
                    return call_native_member("Stdio", "printf", e->args, e->arg_count, r);
                }
                /* Bare scanf(name) / scanf("format", var) — simple_doc.md giriş dili */
                if (strcmp(fn, "scanf") == 0) {
                    return do_scanf(r, e->args, e->arg_count);
                }
                /* strlen(s) — string/char* uzunluğu */
                if (strcmp(fn, "strlen") == 0 && e->arg_count > 0) {
                    GclExpr *arg = e->args[0];
                    if (arg->kind == AST_EXPR_STRING) return (double)strlen(arg->str ? arg->str : "");
                    if (arg->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, arg->name);
                        if (v && v->is_string && v->str) return (double)strlen(v->str);
                    }
                    return 0.0;
                }
                /* sizeof(expr) — boyut (string: uzunluk, sayı: 8, dizi: eleman sayısı) */
                if (strcmp(fn, "sizeof") == 0 && e->arg_count > 0) {
                    GclExpr *arg = e->args[0];
                    /* sizeof(argv) — tüm argv pointer dizisi */
                    if (arg->kind == AST_EXPR_VAR && strcmp(arg->name, "argv") == 0)
                        return (double)(r->env->arg_count * (int)sizeof(char *));
                    /* sizeof(argv[0]) — tek pointer */
                    if (arg->kind == AST_EXPR_ARRAY &&
                        arg->left && arg->left->kind == AST_EXPR_VAR &&
                        strcmp(arg->left->name, "argv") == 0)
                        return (double)sizeof(char *);
                    /* sizeof(dizi_adı) — dizi eleman sayısı (simple_doc.md: sizeof()/sizeof()) */
                    if (arg->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, arg->name);
                        if (v && !v->is_string) {
                            if (v->array_size >= 1) return (double)v->array_size;
                            if (v->arr_count > 0) return (double)v->arr_count;
                        }
                    }
                    /* sizeof(dizi_adı[0]) — tek eleman */
                    if (arg->kind == AST_EXPR_ARRAY &&
                        arg->left && arg->left->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, arg->left->name);
                        if (v && !v->is_string && (v->array_size >= 1 || v->arr_count > 0))
                            return 1.0;
                    }
                    if (arg->kind == AST_EXPR_STRING) return (double)(strlen(arg->str ? arg->str : "") + 1);
                    if (arg->kind == AST_EXPR_VAR) {
                        Var *v = env_find(r->env, arg->name);
                        if (v && v->is_string && v->str) return (double)(strlen(v->str) + 1);
                        if (!v) {
                            /* sizeof(tip_adı) — bilinen tip boyutları */
                            const char *tn = arg->name;
                            if (strcmp(tn, "char")==0 || strcmp(tn,"int8")==0 || strcmp(tn,"uint8")==0) return 1.0;
                            if (strcmp(tn, "short")==0 || strcmp(tn,"int16")==0 || strcmp(tn,"uint16")==0) return 2.0;
                            if (strcmp(tn, "int")==0 || strcmp(tn,"long")==0 || strcmp(tn,"int32")==0 ||
                                strcmp(tn,"uint32")==0 || strcmp(tn,"float")==0 || strcmp(tn,"float32")==0) return 4.0;
                            if (strcmp(tn, "long long")==0 || strcmp(tn,"int64")==0 || strcmp(tn,"uint64")==0 ||
                                strcmp(tn,"double")==0 || strcmp(tn,"float64")==0) return 8.0;
                            if (strcmp(tn, "long double")==0 || strcmp(tn,"int128")==0 ||
                                strcmp(tn,"uint128")==0 || strcmp(tn,"float128")==0) return 16.0;
                        }
                    }
                    return 8.0; /* double varsayımı */
                }
                /* kullanıcı fonksiyonu */
                FuncDef *fd = find_func(r->env, fn);
                if (fd) {
                    return call_user_func(r, fd, e->args, e->arg_count);
                }
                /* #extern + #register ile kayıtlı C fonksiyonu */
                return call_extern_func(r->env, fn, e->args, e->arg_count, r);
            }
            /* modül member çağrısı: Math.fn(...), Stdio.fn(...), Embed.fn(...) */
            if (callee && callee->kind == AST_EXPR_MEMBER) {
                GclExpr *base = callee->left;
                if (base && base->kind == AST_EXPR_VAR) {
                    /* #lib/.gclib feature removed — always dispatch to native modules */
                    return call_native_member(base->name, callee->member_name, e->args, e->arg_count, r);
                }
            }
            return 0.0;
        }
        case AST_EXPR_MEMBER: {
            /* Native modül sabiti: Raylib.RED, Raylib.RAYWHITE gibi (parantezsiz) */
            if (e->left && e->left->kind == AST_EXPR_VAR && native_find(r->env, e->left->name)) {
                return call_native_member(e->left->name, e->member_name, NULL, 0, r);
            }
            /* Struct member oku (nested destekli) */
            GclStructValue *mv = resolve_member_chain(e, r);
            if (mv) {
                if (mv->is_string && mv->str) return atof(mv->str);
                return mv->num;
            }
            return 0.0;
        }
        case AST_EXPR_ARRAY: {
            int idx = (int)eval_expr(e->right, r);
            if (e->left && e->left->kind == AST_EXPR_VAR &&
                strcmp(e->left->name, "argv") == 0 && r->env->arg_vals) {
                /* argv[i] — sayısal bağlamda string ise 0, index döndür */
                if (idx >= 0 && idx < r->env->arg_count && r->env->arg_vals[idx])
                    return atof(r->env->arg_vals[idx]);
            } else if (e->left && e->left->kind == AST_EXPR_VAR) {
                /* Normal sayısal dizi: arr[i] → eleman değerini döndür.
                   Daha önce yalnızca argv destekleniyordu; int8 a[2]={...} gibi
                   diziler expression bağlamında hep 0 dönüyordu. */
                Var *arr = env_find(r->env, e->left->name);
                if (arr && idx >= 0) {
                    int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                    if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                    if (arr->arr_vals && idx < arr->arr_count)
                        return arr->arr_vals[idx];
                    if (arr->arr_strs && idx < arr->arr_count)
                        return atof(arr->arr_strs[idx] ? arr->arr_strs[idx] : "0");
                    if (arr->is_string && arr->str && idx < bounds)
                        return (double)(unsigned char)arr->str[idx];
                }
            }
            return 0.0;
        }
        default:
            return 0.0;
    }
}

/* ---------- Statements ---------- */

static int exec_stmt(GclStmt *s, Runner *r);

static int exec_block(GclStmt *blk, Runner *r) {
    if (!blk || blk->kind != STMT_BLOCK) return 0;
    for (int i = 0; i < blk->u.block.count && !r->return_flag && !r->break_flag && !r->continue_flag; i++) {
        if (exec_stmt(blk->u.block.stmts[i], r) != 0) return -1;
    }
    return 0;
}

static int exec_stmt(GclStmt *s, Runner *r) {
    if (!s) return 0;
    switch (s->kind) {
        case STMT_EXPR:
            eval_expr(s->u.expr, r);
            return 0;
        case STMT_VAR_DECL: {
            const char *type = s->u.var_decl.type_name;
            const char *name = s->u.var_decl.name;
            /* Typeless bildirim: "global g2, g_count;" veya "local local_x;".
               global → mevcut değişkene bağlan (yoksa 0 ile oluştur + global işaretle);
               local  → yerel değişken oluştur (çağrı sonrası silinir). */
            if (!type && name) {
                Var *v = env_find(r->env, name);
                if (s->u.var_decl.is_global) {
                    if (v) {
                        v->is_global = 1;
                        v->is_const = s->u.var_decl.is_const;
                    } else {
                        env_set_num(r->env, name, 0.0);
                        v = env_find(r->env, name);
                        if (v) { v->is_global = 1; v->is_const = s->u.var_decl.is_const; }
                    }
                } else {
                    env_set_num(r->env, name, 0.0);
                }
                return 0;
            }
            /* Global değişken zaten tanımlıysa → yeniden oluşturma, mevcut değeri koru.
               Global tanımları dışarıda yapılır; fonksiyon içindeki `global` kullanımı
               mevcut global'e bağlanır, değeri sıfırlamaz. */
            if (s->u.var_decl.is_global && name) {
                Var *gv = env_find(r->env, name);
                if (gv) {
                    gv->is_global = 1;
                    gv->is_const = s->u.var_decl.is_const;
                    return 0;
                }
            }
            /* struct tipi mi? */
            if (type && strncmp(type, "struct ", 7) == 0) {
                StructDef *sd = find_struct(r->env, type + 7);
                if (sd) {
                    /* single struct variable */
                    if (!(s->u.var_decl.array_size >= 1)) {
                        env_set_struct(r->env, name, sd);
                        if (s->u.var_decl.init) fill_struct_init(r, name, s->u.var_decl.init);
                        return 0;
                    }
                    /* array of struct */
                    int cnt = s->u.var_decl.array_size;
                    if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) cnt = s->u.var_decl.init->arg_count;
                    env_set_num(r->env, name, 0.0);
                    Var *vv = env_find(r->env, name);
                    if (vv) {
                        vv->arr_count = cnt;
                        vv->arr_members = (GclStructValue **)calloc((size_t)cnt, sizeof(GclStructValue *));
                        for (int ai = 0; ai < cnt; ai++) {
                            build_members_recursive(r->env, &vv->arr_members[ai], sd);
                        }
                        if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                            GclExpr *init = s->u.var_decl.init;
                            for (int ai = 0; ai < init->arg_count && ai < cnt; ai++) {
                                GclExpr *item = init->args[ai];
                                if (!item) continue;
                                if (item->kind == AST_EXPR_INIT_LIST) {
                                    fill_members_from_init_list(r, vv->arr_members[ai], item);
                                } else if (item->kind == AST_EXPR_VAR) {
                                    Var *sv = env_find(r->env, item->name);
                                    if (sv && sv->members) {
                                        if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                                        vv->arr_members[ai] = clone_struct_members(sv->members);
                                    }
                                } else if (item->kind == AST_EXPR_CALL) {
                                    /* evaluate call that returns struct or init_list */
                                    double dummy = eval_expr(item, r);
                                    (void)dummy;
                                    if (r->return_struct) {
                                        if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                                        vv->arr_members[ai] = clone_struct_members(r->return_struct);
                                        free_struct_members(r->return_struct);
                                        r->return_struct = NULL;
                                    } else if (r->return_init_list) {
                                        fill_members_from_init_list(r, vv->arr_members[ai], r->return_init_list);
                                        r->return_init_list = NULL;
                                    }
                                }
                            }
                        }
                    }
                    return 0;
                }
            }
            /* bare struct tipi: Test t; */
            if (type) {
                StructDef *sd2 = find_struct(r->env, type);
                if (sd2) {
                    /* single struct var */
                    if (!(s->u.var_decl.array_size >= 1)) {
                        env_set_struct(r->env, name, sd2);
                        if (s->u.var_decl.init) fill_struct_init(r, name, s->u.var_decl.init);
                        return 0;
                    }
                    /* array of struct */
                    int cnt = s->u.var_decl.array_size;
                    if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) cnt = s->u.var_decl.init->arg_count;
                    env_set_num(r->env, name, 0.0);
                    Var *vv = env_find(r->env, name);
                    if (vv) {
                        vv->arr_count = cnt;
                        vv->arr_members = (GclStructValue **)calloc((size_t)cnt, sizeof(GclStructValue *));
                        for (int ai = 0; ai < cnt; ai++) build_members_recursive(r->env, &vv->arr_members[ai], sd2);
                        if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                            GclExpr *init = s->u.var_decl.init;
                            for (int ai = 0; ai < init->arg_count && ai < cnt; ai++) {
                                GclExpr *item = init->args[ai];
                                if (!item) continue;
                                if (item->kind == AST_EXPR_INIT_LIST) fill_members_from_init_list(r, vv->arr_members[ai], item);
                                else if (item->kind == AST_EXPR_VAR) {
                                    Var *sv = env_find(r->env, item->name);
                                    if (sv && sv->members) {
                                        if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                                        vv->arr_members[ai] = clone_struct_members(sv->members);
                                    }
                                } else if (item->kind == AST_EXPR_CALL) {
                                    double dummy = eval_expr(item, r);
                                    (void)dummy;
                                    if (r->return_struct) {
                                        if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                                        vv->arr_members[ai] = clone_struct_members(r->return_struct);
                                        free_struct_members(r->return_struct);
                                        r->return_struct = NULL;
                                    } else if (r->return_init_list) {
                                        fill_members_from_init_list(r, vv->arr_members[ai], r->return_init_list);
                                        r->return_init_list = NULL;
                                    }
                                }
                            }
                        }
                    }
                    return 0;
                }
            }
            /* typedef alias mı? */
            const char *base = type ? find_typedef_base(r->env, type) : NULL;
            if (base && strncmp(base, "struct ", 7) == 0) {
                StructDef *sd = find_struct(r->env, base + 7);
                if (sd) {
                    env_set_struct(r->env, name, sd);
                    if (s->u.var_decl.init) fill_struct_init(r, name, s->u.var_decl.init);
                    return 0;
                }
            }
            /* char[] boş bildirim — yalnızca initializer yoksa hata ver
               (ör. `char name[] = "Hello";` veya `char a[] = {'A','B'};` desteklenir) */
            if (type && strcmp(type, "char") == 0 && s->u.var_decl.array_size == -1 &&
                !(s->u.var_decl.init && (s->u.var_decl.init->kind == AST_EXPR_STRING || s->u.var_decl.init->kind == AST_EXPR_INIT_LIST))) {
                fprintf(stderr, "Runtime error: 'char %s[]' boş boyut belirtilemez. 'char %s[32]' gibi bir boyut verin.\n", name, name);
                return -1;
            }
            /* normal sayı/string */
            double v = s->u.var_decl.init ? eval_expr(s->u.var_decl.init, r) : 0.0;
            /* tip bilgisi */
            int is_char_arr = (s->u.var_decl.array_size >= 1 || s->u.var_decl.is_pointer || s->u.var_decl.array_size == -1);
            if (type && strcmp(type, "gcChar") == 0) {
                /* gcChar → UTF-8 string */
                if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_STRING) {
                    env_set_str(r->env, name, s->u.var_decl.init->str);
                } else {
                    env_set_str(r->env, name, "");
                }
                Var *vv = env_find(r->env, name);
                if (vv) {
                    if (vv->decl_type) free(vv->decl_type);
                    vv->decl_type = strdup(type);
                    vv->array_size = s->u.var_decl.array_size;
                    vv->is_pointer = s->u.var_decl.is_pointer;
                    vv->is_const = s->u.var_decl.is_const;
                    vv->is_global = s->u.var_decl.is_global;
                }
            } else if (type && strcmp(type, "char") == 0) {
                if (is_char_arr) {
                    /* char* / char[N] → string OR initializer list of chars
                       Also support char a[N][M] with initializer list of strings. */
                    if (s->u.var_decl.array_inner > 0) {
                        /* multidimensional char array (array of fixed-size strings) */
                        if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                            GclExpr *init = s->u.var_decl.init;
                            int cnt = init->arg_count;
                            env_set_num(r->env, name, 0.0);
                            Var *vv = env_find(r->env, name);
                            if (vv) {
                                vv->arr_count = cnt;
                                if (vv->arr_strs) { for (int ii = 0; vv->arr_strs[ii]; ii++) free(vv->arr_strs[ii]); free(vv->arr_strs); vv->arr_strs = NULL; }
                                /* if all entries are strings, copy them into arr_strs */
                                int all_strings = 1;
                                for (int ai = 0; ai < cnt; ai++) { if (!init->args[ai] || init->args[ai]->kind != AST_EXPR_STRING) { all_strings = 0; break; } }
                                if (all_strings) {
                                    vv->arr_strs = (char **)calloc((size_t)cnt + 1, sizeof(char *));
                                    for (int ai = 0; ai < cnt; ai++) vv->arr_strs[ai] = strdup(init->args[ai]->str ? init->args[ai]->str : "");
                                    vv->arr_strs[cnt] = NULL;
                                } else {
                                    /* fallback: store as numeric char codes */
                                    if (vv->arr_vals) free(vv->arr_vals);
                                    vv->arr_vals = (double *)calloc((size_t)cnt, sizeof(double));
                                    for (int ai = 0; ai < cnt; ai++) {
                                        GclExpr *it = init->args[ai];
                                        double val = 0.0;
                                        if (it) {
                                            if (it->kind == AST_EXPR_STRING && it->str && it->str[0]) val = (double)(unsigned char)it->str[0];
                                            else val = eval_expr(it, r);
                                        }
                                        vv->arr_vals[ai] = val;
                                    }
                                }
                            }
                        } else {
                            env_set_str(r->env, name, "");
                        }
                    } else {
                        /* single-dimension char array */
                        if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_STRING) {
                            env_set_str(r->env, name, s->u.var_decl.init->str);
                        } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                            /* Populate numeric arr_vals with char codes from initializer list */
                            GclExpr *init = s->u.var_decl.init;
                            int cnt = init->arg_count;
                            env_set_num(r->env, name, 0.0);
                            Var *vv = env_find(r->env, name);
                            if (vv) {
                                vv->arr_count = cnt;
                                if (vv->arr_vals) free(vv->arr_vals);
                                if (cnt > 0) {
                                    vv->arr_vals = (double *)calloc((size_t)cnt, sizeof(double));
                                    for (int ai = 0; ai < cnt; ai++) {
                                        GclExpr *it = init->args[ai];
                                        double val = 0.0;
                                        if (it) {
                                            if (it->kind == AST_EXPR_STRING && it->str && it->str[0]) val = (double)(unsigned char)it->str[0];
                                            else val = eval_expr(it, r);
                                        }
                                        vv->arr_vals[ai] = val;
                                    }
                                }
                            }
                        } else {
                            env_set_str(r->env, name, "");
                        }
                    }
                } else {
                    /* vanilla char → tek karakter (C gibi) */
                    if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_STRING &&
                        s->u.var_decl.init->str && s->u.var_decl.init->str[0]) {
                        env_set_num(r->env, name, (double)s->u.var_decl.init->str[0]);
                    } else {
                        env_set_num(r->env, name, v);
                    }
                }
                Var *vv = env_find(r->env, name);
                if (vv) {
                    if (vv->decl_type) free(vv->decl_type);
                    vv->decl_type = strdup(type);
                    vv->array_size = s->u.var_decl.array_size;
                    vv->is_pointer = s->u.var_decl.is_pointer;
                    vv->is_const = s->u.var_decl.is_const;
                    vv->is_global = s->u.var_decl.is_global;
                    if (!vv->is_string && vv->decl_type)
                        vv->num = truncate_to_declared_type(vv->num, vv->decl_type);
                }
            } else {
                if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_STRING) {
                    env_set_str(r->env, name, s->u.var_decl.init->str);
                } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_CALL) {
                    env_set_num(r->env, name, v);
                } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                    /* Array initializer: allocate arr_vals or arr_strs depending on contents */
                    GclExpr *init = s->u.var_decl.init;
                    int cnt = init->arg_count;
                    env_set_num(r->env, name, 0.0);
                    Var *vv = env_find(r->env, name);
                    if (vv) {
                        vv->arr_count = cnt;
                        if (vv->arr_vals) free(vv->arr_vals);
                        if (vv->arr_strs) { for (int ii = 0; vv->arr_strs[ii]; ii++) free(vv->arr_strs[ii]); free(vv->arr_strs); vv->arr_strs = NULL; }
                                if (cnt > 0) {
                                    /* If all items are string literals, keep as arr_strs. If declared type
                                       is uint64/uint128 and items are numeric floats with lexeme, preserve
                                       the lexeme strings to avoid double->scientific formatting. */
                                    int all_strings = 1;
                                    int all_num_with_lex = 1;
                                    for (int ai = 0; ai < cnt; ai++) {
                                        if (!init->args[ai] || init->args[ai]->kind != AST_EXPR_STRING) all_strings = 0;
                                        if (!init->args[ai] || init->args[ai]->kind != AST_EXPR_FLOAT || !init->args[ai]->str) all_num_with_lex = 0;
                                    }
                                    /* effective declared type: resolve typedef alias if any */
                                    const char *eff_type = type;
                                    const char *tdbase = type ? find_typedef_base(r->env, type) : NULL;
                                    if (!eff_type && tdbase) eff_type = tdbase;
                                    if (!eff_type && tdbase) eff_type = tdbase;
                                    if (all_strings) {
                                        vv->arr_strs = (char **)calloc((size_t)cnt + 1, sizeof(char *));
                                        for (int ai = 0; ai < cnt; ai++) vv->arr_strs[ai] = strdup(init->args[ai]->str ? init->args[ai]->str : "");
                                        vv->arr_strs[cnt] = NULL;
                                        } else if ((eff_type && (strcmp(eff_type, "uint64") == 0 || strcmp(eff_type, "uint128") == 0)) && all_num_with_lex) {
                                            /* preserve original numeric lexemes for unsigned 64/128 arrays */
                                            vv->arr_strs = (char **)calloc((size_t)cnt + 1, sizeof(char *));
                                            for (int ai = 0; ai < cnt; ai++) vv->arr_strs[ai] = strdup(init->args[ai]->str ? init->args[ai]->str : "0");
                                            vv->arr_strs[cnt] = NULL;
                                        } else {
                                            vv->arr_vals = (double *)calloc((size_t)cnt, sizeof(double));
                                            for (int ai = 0; ai < cnt; ai++) {
                                                GclExpr *it = init->args[ai];
                                                double val = 0.0;
                                                if (it) {
                                                    if (it->kind == AST_EXPR_STRING) val = atof(it->str ? it->str : "0");
                                                    else val = eval_expr(it, r);
                                                }
                                                /* Dizi elemanını da bildirilen tipin sınırına kırp —
                                                   scalar atama (env_set_num) ile tutarlılık. */
                                                vv->arr_vals[ai] = truncate_to_declared_type(val, eff_type);
                                            }
                                        }
                                }
                    }
                } else {
                    env_set_num(r->env, name, v);
                }
                Var *vv = env_find(r->env, name);
                /* Preserve exact literal string for large unsigned types so printing
                   shows the original value instead of a rounded double. */
                if (vv && type && (strcmp(type, "uint64") == 0 || strcmp(type, "uint128") == 0) &&
                    s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_FLOAT && s->u.var_decl.init->str) {
                    if (vv->str) free(vv->str);
                    vv->str = strdup(s->u.var_decl.init->str);
                }
                if (vv && type) {
                    if (vv->decl_type) free(vv->decl_type);
                    vv->decl_type = strdup(type);
                    vv->array_size = s->u.var_decl.array_size;
                    vv->is_const = s->u.var_decl.is_const;
                    vv->is_global = s->u.var_decl.is_global;
                    if (!vv->is_string && vv->decl_type)
                        vv->num = truncate_to_declared_type(vv->num, vv->decl_type);
                }
            }
            return 0;
        }
        case STMT_IF: {
            double cond = s->u.if_.cond ? eval_expr(s->u.if_.cond, r) : 0.0;
            if (cond) {
                if (s->u.if_.then && s->u.if_.then->kind == STMT_BLOCK) return exec_block(s->u.if_.then, r);
                if (s->u.if_.then) return exec_stmt(s->u.if_.then, r);
            } else if (s->u.if_.else_) {
                if (s->u.if_.else_->kind == STMT_BLOCK) return exec_block(s->u.if_.else_, r);
                return exec_stmt(s->u.if_.else_, r);
            }
            return 0;
        }
        case STMT_WHILE: {
            int guard = 0;
            while (!r->break_flag && !r->return_flag) {
                double cond = s->u.while_.cond ? eval_expr(s->u.while_.cond, r) : 0.0;
                if (!cond) break;
                if (++guard > 1000000) { fprintf(stderr, "Runtime error: possible infinite loop\n"); return -1; }
                /* continue: sonraki iterasyona geç (continue_flag'i temizle) */
                r->continue_flag = 0;
                if (s->u.while_.body->kind == STMT_BLOCK) exec_block(s->u.while_.body, r);
                else exec_stmt(s->u.while_.body, r);
                if (r->continue_flag) r->continue_flag = 0;
            }
            r->break_flag = 0; r->continue_flag = 0;
            return 0;
        }
        case STMT_FOR: {
            /* for-init değişkeni ("int i = 0" gibi) C'de blok-scope'tur:
               döngü bittiğinde env'den kaldırılır. */
            const char *for_var_name = NULL;
            if (s->u.for_.var) {
                if (s->u.for_.var->kind == AST_EXPR_ASSIGN && s->u.for_.var->left &&
                    s->u.for_.var->left->kind == AST_EXPR_VAR) {
                    for_var_name = s->u.for_.var->left->name;
                } else if (s->u.for_.var->kind == AST_EXPR_VAR) {
                    for_var_name = s->u.for_.var->name;
                }
            }
            /* Aynı isimli değişken döngüden ÖNCE var mı? (for (i = 0; ...) dış değişken) */
            Var *for_existing = for_var_name ? env_find(r->env, for_var_name) : NULL;
            if (s->u.for_.var) eval_expr(s->u.for_.var, r);
            int guard = 0;
            while (!r->break_flag && !r->return_flag) {
                if (s->u.for_.cond) {
                    double cond = eval_expr(s->u.for_.cond, r);
                    if (!cond) break;
                }
                if (++guard > 1000000) { fprintf(stderr, "Runtime error: possible infinite loop\n"); return -1; }
                /* continue: inc'e atla (continue_flag'i temizle) */
                r->continue_flag = 0;
                if (s->u.for_.body->kind == STMT_BLOCK) exec_block(s->u.for_.body, r);
                else exec_stmt(s->u.for_.body, r);
                if (r->continue_flag) r->continue_flag = 0;
                if (s->u.for_.inc) eval_expr(s->u.for_.inc, r);
            }
            r->break_flag = 0; r->continue_flag = 0;
            /* for-init'te YENİ oluşturulan değişkeni kaldır (dış değişkeni değil) */
            if (for_var_name && !for_existing) env_remove_var(r->env, for_var_name);
            return 0;
        }
        case STMT_RETURN:
            r->return_flag = 1;
            if (s->u.ret.expr && s->u.ret.expr->kind == AST_EXPR_VAR) {
                Var *rv = env_find(r->env, s->u.ret.expr->name);
                if (rv && rv->members) {
                    if (r->return_struct) free_struct_members(r->return_struct);
                    r->return_struct = clone_struct_members(rv->members);
                    r->return_value = 0.0;
                    return 0;
                }
            }
            if (s->u.ret.expr && s->u.ret.expr->kind == AST_EXPR_INIT_LIST) {
                /* return { ... } struct literal — return_init_list'e sakla */
                r->return_init_list = s->u.ret.expr;
                r->return_value = 0.0;
                return 0;
            }
            r->return_value = s->u.ret.expr ? eval_expr(s->u.ret.expr, r) : 0.0;
            return 0;
        case STMT_BREAK:
            r->break_flag = 1;
            return 0;
        case STMT_CONTINUE:
            r->continue_flag = 1;
            return 0;
        case STMT_SWITCH: {
            double val = s->u.switch_.name ? env_get_num(r->env, s->u.switch_.name) : 0.0;
            int matched = 0;
            for (int i = 0; i < s->u.switch_.case_count; i++) {
                if (!matched && s->u.switch_.case_vals[i]) {
                    double cv = eval_expr(s->u.switch_.case_vals[i], r);
                    if (val == cv) matched = 1;
                }
                if (matched) {
                    if (s->u.switch_.cases[i]) exec_stmt(s->u.switch_.cases[i], r);
                    if (r->break_flag) { r->break_flag = 0; break; }
                    if (r->return_flag) return 0;
                }
            }
            if (!matched && s->u.switch_.default_case) {
                exec_stmt(s->u.switch_.default_case, r);
                if (r->break_flag) r->break_flag = 0;
            }
            return 0;
        }
        case STMT_BLOCK:
            return exec_block(s, r);
        case STMT_TYPEDEF: {
            if (s->u.typedef_info.alias_name && s->u.typedef_info.base_type) {
                TypedefDef *td = (TypedefDef *)calloc(1, sizeof(TypedefDef));
                if (td) {
                    td->alias = strdup(s->u.typedef_info.alias_name);
                    td->base = strdup(s->u.typedef_info.base_type);
                    td->next = r->env->typedefs;
                    r->env->typedefs = td;
                }
            }
            return 0;
        }
        case STMT_ENUM: {
            for (int i = 0; i < s->u.enum_info.const_count; i++) {
                env_set_num(r->env, s->u.enum_info.const_names[i], (double)i);
            }
            return 0;
        }
        case STMT_STRUCT_DECL: {
            if (s->u.struct_info.struct_name) {
                StructDef *sd = (StructDef *)calloc(1, sizeof(StructDef));
                if (sd) {
                    sd->name = strdup(s->u.struct_info.struct_name);
                    /* Runnable kendi kopyasÄ±nÄ± alÄ±r — AST deep-free'si bu alanlarÄ± da
                       free ettiÄŸinden, aynÄ± pointer'Ä± paylaÅŸmak dangling'e yol aÃ§ar. */
                    sd->member_count = s->u.struct_info.member_count;
                    if (sd->member_count > 0) {
                        sd->member_names = (char **)calloc((size_t)sd->member_count, sizeof(char *));
                        sd->member_types = (char **)calloc((size_t)sd->member_count, sizeof(char *));
                        for (int mi = 0; mi < sd->member_count; mi++) {
                            sd->member_names[mi] = strdup(s->u.struct_info.member_names[mi]);
                            sd->member_types[mi] = strdup(s->u.struct_info.member_types[mi]);
                        }
                    }
                    sd->next = r->env->structs;
                    r->env->structs = sd;
                }
            }
            return 0;
        }
        case STMT_FUNC_DECL: {
            if (s->u.func_decl.name) {
                FuncDef *fd = (FuncDef *)calloc(1, sizeof(FuncDef));
                if (fd) {
                    fd->name = strdup(s->u.func_decl.name);
                    fd->param_count = s->u.func_decl.param_count;
                    if (fd->param_count > 0) {
                        fd->params = (char **)calloc((size_t)fd->param_count, sizeof(char *));
                        for (int pi = 0; pi < fd->param_count; pi++) {
                            fd->params[pi] = strdup(s->u.func_decl.params[pi]);
                        }
                    }
                    fd->body = s->u.func_decl.body; /* body AST'ye ait — deep-free AST'de */
                    fd->next = r->env->funcs;
                    r->env->funcs = fd;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

/* ---------- env temizliÄŸi (memory leak) ---------- */
static void env_cleanup(GclEnv *env) {
    if (!env) return;
    for (Var *v = env->vars; v;) {
        Var *nx = v->next;
        if (gcl_debug) fprintf(stderr, "DEBUG cleanup var='%s' members=%p arr_members=%p arr_vals=%p arr_strs=%p arr_count=%d\n",
                v->name?v->name:"?", (void*)v->members, (void*)v->arr_members,
                (void*)v->arr_vals, (void*)v->arr_strs, v->arr_count);
        if (v->name) free(v->name);
        if (v->str) free(v->str);
        if (v->decl_type) free(v->decl_type);
        if (v->members) { if (gcl_debug) fprintf(stderr, "DEBUG free members var='%s' ptr=%p\n", v->name?v->name:"?", (void*)v->members); free_struct_members(v->members); }
        if (v->arr_vals) free(v->arr_vals);
    if (v->arr_strs) {
            for (int i = 0; i < v->arr_count; i++) if (v->arr_strs[i]) free(v->arr_strs[i]);
            free(v->arr_strs);
        }
        if (v->arr_members) {
            for (int i = 0; i < v->arr_count; i++) {
                if (gcl_debug) fprintf(stderr, "DEBUG free arr_members[%d] var='%s' ptr=%p\n", i, v->name?v->name:"?", v->arr_members[i]?(void*)v->arr_members[i]:NULL);
                if (v->arr_members[i]) free_struct_members(v->arr_members[i]);
            }
            free(v->arr_members);
        }
        free(v);
        v = nx;
    }
    for (TypedefDef *td = env->typedefs; td;) { TypedefDef *nx = td->next; free(td->alias); free(td->base); free(td); td = nx; }
    for (StructDef *sd = env->structs; sd;) {
        StructDef *nx = sd->next;
        free(sd->name);
        if (sd->member_names) { for (int mi = 0; mi < sd->member_count; mi++) free(sd->member_names[mi]); free(sd->member_names); }
        if (sd->member_types) { for (int mi = 0; mi < sd->member_count; mi++) free(sd->member_types[mi]); free(sd->member_types); }
        free(sd);
        sd = nx;
    }
    for (FuncDef *fd = env->funcs; fd;) {
        FuncDef *nx = fd->next;
        free(fd->name);
        if (fd->params) { for (int pi = 0; pi < fd->param_count; pi++) free(fd->params[pi]); free(fd->params); }
        free(fd);
        fd = nx;
    }
    /* lib bundles removed; nothing to free here */
    for (NativeModule *nm = env->modules; nm;) { NativeModule *nx = nm->next; free(nm->name); free(nm); nm = nx; }
    for (int i = 0; i < env->extern_dll_count; i++) free(env->extern_dlls[i].dll_name);
    for (int i = 0; i < env->extern_reg_count; i++) { free(env->extern_regs[i].dll_name); free(env->extern_regs[i].func_name); }
}

int gcl_run_program(GclProgram *prog,
                    const char *base_dir,
                    const char **native_modules,
                    int native_count,
                    const char **extern_dlls,
                    int extern_dll_count,
                    const char **extern_regs,
                    int extern_reg_count,
                    int argc,
                    char **argv) {
    if (!prog) return -1;
    if (gcl_debug) fprintf(stderr, "gcl_run_program: start\n");
    GclEnv env = { 0 };
    Runner r = { 0 };
    r.env = &env;
    if (base_dir) snprintf(env.base_dir, sizeof(env.base_dir), "%s", base_dir);

    /* #native ile bildirilen modülleri yükle (Library/ dizininden) */
    for (int i = 0; i < native_count; i++) {
        if (native_modules[i]) native_load(&env, native_modules[i]);
    }
    /* NOTE: #lib/.gclib support removed — do not load GCL library bundles here. */

    /* #extern DLL'leri yükle */
    for (int i = 0; i < extern_dll_count; i++) {
        if (extern_dlls[i]) extern_dll_load(&env, extern_dlls[i]);
    }
    /* #register kayıtlarını parse et */
    for (int i = 0; i < extern_reg_count; i++) {
        if (extern_regs[i]) extern_register_parse(&env, extern_regs[i]);
    }

    /* Önce fonksiyon ve typedef/enum/struct tanımlarını topla,
       sonra gövdeyi çalıştır — çünkü bildirimden önce çağrılabilir. */
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (s && (s->kind == STMT_FUNC_DECL || s->kind == STMT_TYPEDEF ||
                  s->kind == STMT_ENUM || s->kind == STMT_STRUCT_DECL)) {
            exec_stmt(s, &r);
        }
    }
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (s && (s->kind == STMT_FUNC_DECL || s->kind == STMT_TYPEDEF ||
                  s->kind == STMT_ENUM || s->kind == STMT_STRUCT_DECL)) {
            continue;
        }
        if (exec_stmt(s, &r) != 0) {
            fprintf(stderr, "Runtime error: top-level statement %d (kind=%d) returned error\n", i, (int)(s ? s->kind : (GclStmtKind)-1));
            env_cleanup(&env);
            fprintf(stderr, "gcl_run_program: returning -1 due to top-level stmt\n");
            return -1;
        }
        if (r.return_flag) break;
    }

    /* main() varsa otomatik çağır — argc/argv parametrelerini bağla */
    env.arg_count = argc;
    env.arg_vals = argv;
    FuncDef *main_fd = find_func(&env, "main");
    if (main_fd) {
        Runner sub;
        memcpy(&sub, &r, sizeof(sub));
        sub.return_flag = 0;
        sub.return_value = 0;
        sub.break_flag = 0;
        sub.continue_flag = 0;
        /* argc parametresi */
        if (main_fd->param_count > 0) env_set_num(&env, main_fd->params[0], (double)argc);
        if (main_fd->body && main_fd->body->kind == STMT_BLOCK) exec_block(main_fd->body, &sub);
        else if (main_fd->body) exec_stmt(main_fd->body, &sub);
    }

    env_cleanup(&env);
    if (gcl_debug) fprintf(stderr, "gcl_run_program: return 0\n");
    return 0;
}
