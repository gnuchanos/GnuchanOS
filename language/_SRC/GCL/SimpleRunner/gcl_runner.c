/*
 * gcl_runner.c — GCL AST executor (part of SimpleRunner).
 *
 * Runs the examples from simple_doc.md:
 *   variable decl, printf("{}", ...), if/else, while, for, break/continue,
 *   arithmetic/comparison/boolean ops, scanf (simple), Math.*, Stdio.*,
 *   struct/enum/typedef, user function definition and calls.
 */

#include "gcl_runner.h"
#include "gcl_parser.h"
#include "gcl_error.h"
#include "gcl_module.h"
/* Native modul struct alan duzeni (Vector2/Rectangle/...) — tamamlama motoruyla
   AYNI tablo (SharedPipeline). `Raylib.Rectangle r;` bildirimini kurmak icin
   gerekir; tablo olmadan alan okumalari sessizce 0 donerdi. */
#include "gcl_native_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>   /* LLONG_MAX / LLONG_MIN - the finite-narrowing bounds in gcl_to_ll */

/* -debug flag: enabled by `gcl -debug -run ...` (set by gcl_main.c).
   During a normal run runtime debug messages are NOT shown. */
int gcl_debug = 0;

/* Single debug gate: ALL runtime debug/trace output goes through here.
   Unless `gcl -debug -run ...` is given, no debug line is printed. */
static void gcl_debugf(const char *fmt, ...) {
    if (!gcl_debug) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* ---------- Runtime hata kanali (B17) ----------

   Butun runtime hatalari TEK noktadan gecer: mesaj stderr'e yazilir ve hata
   SAYACI artar. gcl_run_program bu sayaca bakip exit code'u ayarlar.

   Neden global sayac? call_user_func / exec_block ic cagrilari Runner'i
   `memcpy` ile KOPYALAR (bkz. `Runner sub`). Kopyaya yazilan bir BAYRAK
   cagirana geri DONMEZ; bu yuzden bayrak yerine global sayac kullanilir —
   boylece hata en distaki gcl_run_program'a ulasir. Runner'da boyle bir alan
   YOKTUR: bir zamanlar bir `error` alani vardi ve tek bir yerde yazilip hic
   OKUNMUYORDU; TURN 41'de kaldirildi.

   Eskiden her hata yalnizca `fprintf` yapiyordu: program **0** ile
   cikiyordu, CI/IDE hatayi goremiyordu. */
int gcl_runtime_errors = 0;

/* ---------- try/catch hata kanali ----------

   Butun runtime hatalari TEK noktadan gecer (runtime_errorf → error_capture).
   Iki mod vardir:

   * `try` DISINDA (g_try_depth == 0): eski TOLERANSLI davranis aynen korunur —
     mesaj hemen stderr'e yazilir, sayac artar ve program AKMAYA devam eder.
     Altin testler bu sozlesmeye dayanir: `errors/runtime_diagnostics.gcsf`
     "division by zero"dan SONRA `div 0` basmaya devam eder ve surec 1 ile cikar.
   * `try` ICINDE (g_try_depth > 0): mesaj SAKLANIR, ekrana YAZILMAZ ve sayac
     ARTMAZ — yakalanan bir hatanin gorunur izi kalmamalidir. `g_unwinding = 1`
     ile govde bloklari/ donguler hizli cikisa zorlanir; STMT_TRY yakalar.

   Bayraklar GLOBAL'dir: call_user_func ve exec_block Runner'i `memcpy` ile
   KOPYALAR (yukaridaki gerekce), kopyaya yazilan alan cagirana geri donmez.

   TURN 50 - TEK SOZLUK. Eskiden bu ceviri birimi gcl_diag.h'yi include ETMEZ
   ve kodlari `GCL_RT_CODE_*` adiyla ELLE kopyalardi; yorumu da "degerler
   gcl_diag.h'deki GCL3xxx ile AYNIdir" diyordu. Iki sozlugu elle senkron tutmak
   zorunlulugu somut bir hata uretmisti: GCL_RT_CODE_THROW = 3014, gcl_diag.h'de
   HIC YOKTU - yorumlayici, sozlugun tanimadigi bir kod uretiyor ve `-explain`
   onu "diagnostic" diye cevapliyordu. Kodlar artik gercek basliktan gelir
   (yukaridaki include); THROW/LOOP_CONTROL/CALL_DEPTH oraya eklendi. */

#define GCL_ERR_MSG_MAX 512

static char       g_err_msg[GCL_ERR_MSG_MAX];
static GclDiagCode g_err_code;
static int  g_err_line;
static int  g_err_col;
static int  g_err_len;       /* span uzunlugu: alti cizilen karakter sayisi */
static int  g_err_valid;     /* yakalanmayi bekleyen bir hata var mi? */
static int  g_err_is_throw;  /* hata acik `throw` ile mi uretildi? */
static int  g_unwinding;     /* 1 = govdeler hizli cikisa zorlanir */
static int  g_try_depth;     /* kac `try` govdesi icindeyiz? */
/* Bir sonraki raporlamaya eklenecek "= help:" satiri (bkz. runtime_errorf_hint).
   Yalnizca hata URETILIRKEN ile RAPORLANANA kadar yasar; `error_capture`
   tuketir, bu yuzden baska bir hataya sizamaz. */
static const char *g_err_hint;

/* ---------- runtime tanilama HAVUZU (TURN 50) ----------

   Eskiden runtime kendi satirini BURADA bicimlendirip basardi:
   "Runtime error: <mesaj>". Bu seklin kodu yok, spani yok, kaynak cercevesi
   yok - ve ayni hata bir cagri yerinde makine-okunur (kod geciren cagrilar),
   digerinde duz yaziydi (mesaj metnine "(GCL3008)" gomen cagrilar). IDE ise
   yalnizca lexer/parser tanilamalarini okuyabildigi icin runtime hatasini HIC
   okuyamiyordu.

   Artik runtime da lexer/parser gibi bir URETICIDIR: hata bu listeye kodu ve
   spaniyla eklenir, bicimlendirmeyi cagiran yapar
   (gcl_diag_list_print_mapped). Tek sozluk, tek renderer, tek "mesaji ve cikis
   kodunu kim uretiyor" yeri. Liste `main` disinda global'tir cunku hatayi
   URETEN yer ile RAPORLAYAN yer farklidir (bkz. error_escalate). */
static GclDiagList g_rt_diags;
static int g_rt_diags_ready;      /* gcl_diag_list_init cagrildi mi? */
static const char *g_rt_src;      /* fallback kaynak (preprocessed buffer) */
static size_t g_rt_src_len;
static const GclSourceMap *g_rt_map;

static GclDiagList *rt_diags(void) {
    if (!g_rt_diags_ready) {
        gcl_diag_list_init(&g_rt_diags, NULL);
        g_rt_diags_ready = 1;
    }
    return &g_rt_diags;
}

void gcl_runtime_set_source(const char *file, const char *src, size_t len,
                            const GclSourceMap *map) {
    g_rt_src = src;
    g_rt_src_len = len;
    g_rt_map = map;
    rt_diags()->file = file;    /* borrowed default; bkz. gcl_runner.h */
}

const GclDiagList *gcl_runtime_diags(void) { return rt_diags(); }

/* ---------- span helpers ----------

   "span, yapinin BASLADIGI token'dir" - parser'daki stamp_expr()/stamp_stmt()
   ile ayni kural. AST'te karsiligi OLMAYAN bir hata (modul yukleme dongusu,
   bellek yetmezligi yolu) sifir span alir: renderer o zaman line:col'suz bir
   baslik basar, ki "nerede?" sorusuna UYDURMA bir konum vermekten iyidir.
   `len` sifir olsa da renderer `^` isaretini basar (tek karakter). */
static GclSpan span_none(void) {
    GclSpan sp;
    sp.line = 0; sp.col = 0; sp.len = 0;
    return sp;
}
static GclSpan span_of_expr(const GclExpr *e) {
    GclSpan sp;
    sp.line = e ? e->line : 0;
    sp.col  = e ? e->col  : 0;
    sp.len  = e ? e->len  : 0;
    return sp;
}
static GclSpan span_of_stmt(const GclStmt *s) {
    GclSpan sp;
    sp.line = s ? s->line : 0;
    sp.col  = s ? s->col  : 0;
    sp.len  = s ? s->len  : 0;
    return sp;
}

/* ---------- the ONE place a runtime failure becomes a diagnostic ----------

   TURN 50 removed the message-text code sniffing that used to live here
   (`code_from_message`). It existed for ONE reason: about half the call sites
   passed a code as a number while the other half wrote it INTO the sentence
   ("division by zero (GCL3008)"), so the error object had to read the prose
   back out to answer `e.code`. Now every call site passes its code and its
   span, the messages are plain sentences, and the code reaches the user
   through the renderer instead of through a parenthesised suffix - so the
   same failure can no longer be machine-readable at one site and prose at
   another, and there is no second table to keep in sync. */
static void runtime_report_pending(void) {
    GclSpan span;
    span.line = g_err_line;
    span.col  = g_err_col;
    span.len  = g_err_len;
    gcl_runtime_errors++;
    GclDiag *diag = gcl_diag_add(rt_diags(), g_err_code, GCL_SEV_ERROR, span,
                                 "%s", g_err_msg);
    /* The explanation the message cannot carry: the printed line stays the
       pinned "Runtime error: <message>" form (golden tests match it as a
       substring), while the IDE gets the "= help:" line from the renderer. */
    if (diag && g_err_hint) gcl_diag_hint(diag, "%s", g_err_hint);
    g_err_hint = NULL;
    /* The diag list above is what the IDE reads; the LINE BELOW is what the
       user reads. Both belong here, at the single reporting point, because the
       legacy contract (17 golden tests) is that a runtime failure outside a
       `try` is written to stderr IMMEDIATELY and the program keeps going.
       The wording is pinned by errors/runtime_diagnostics.gcsf and friends:
       "Runtime error: <message>", with the GCL3xxx code travelling inside the
       message text at the call sites that carry one. */
    fputs("Runtime error: ", stderr);
    fputs(g_err_msg, stderr);
    if (g_err_line > 0) fprintf(stderr, " at %d:%d", g_err_line, g_err_col);
    fputc('\n', stderr);
}

/* Hatayi kaydet. Raporlama karari modu belirler (bkz. yukaridaki aciklama).
   `span`: hatanin kaynak konumu (yoksa span_none()), `code`: GCL3xxx kodu. */
static void error_capture(const char *msg, GclDiagCode code, GclSpan span,
                          int is_throw) {
    snprintf(g_err_msg, sizeof(g_err_msg), "%s", msg ? msg : "error");
    if (code == GCL_E_NONE) code = GCL_E_SEM_RUNTIME;
    g_err_code = code;
    g_err_line = span.line;
    g_err_col  = span.col;
    g_err_len  = span.len;
    g_err_valid = 1;
    g_err_is_throw = is_throw;
    if (g_try_depth > 0) { g_unwinding = 1; g_err_hint = NULL; return; }   /* try yakalayabilir */
    runtime_report_pending();
    /* Acik `throw` yakalanmadiysa program DURUR (Python gibi); siradan runtime
       hatasi toleransli modda akisa devam eder (legacy sozlesme). */
    if (is_throw) g_unwinding = 1;
}

/* Spansiz varyant: konum bilinmiyorsa (modul yukleme dongusu, bellek
   yetmezligi) cagiran span_none() gecirir ve renderer line:col'suz bir baslik
   basar. Kodu GCL_E_SEM_RUNTIME'dir: "genel runtime hatasi". */
static void runtime_errorf(GclSpan span, const char *fmt, ...) {
    char msg[GCL_ERR_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    error_capture(msg, GCL_E_SEM_RUNTIME, span, 0);
}

/* Ipucu tasiyan varyant: mesaj kisa kalir (altin testler
   "Runtime error: <mesaj>" satirini birebir sabitler), aciklama ise
   tanilamanin "= help:" satirina gider - IDE onu okur, kullanicinin gordugu
   duz satir degismez. */
static void runtime_errorf_hint(GclSpan span, const char *hint, const char *fmt, ...) {
    char msg[GCL_ERR_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    g_err_hint = hint;
    error_capture(msg, GCL_E_SEM_RUNTIME, span, 0);
}

/* Kodlu varyant: ayni kod HEM `e.code`a HEM basilacak tanilamaya gider. */
static void runtime_errorcf(GclDiagCode code, GclSpan span, const char *fmt, ...) {
    char msg[GCL_ERR_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    error_capture(msg, code, span, 0);
}

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access _access
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

/* ---------- Variable environment ---------- */

typedef struct Var {
    char *name;
    double num;
    char *str;          /* string ise */
    int is_string;
    char *decl_type;    /* declared type (gcChar, char, int, ...) */
    int array_size;     /* 0=not an array, -1=empty char[], N=char[N] */
    int is_pointer;     /* char *ptr */
    int is_const;       /* 1=const variable (cannot be assigned) */
    int is_global;      /* 1=global variable (survives function exit) */
    /* int arr[] = {...} numeric array support */
    double *arr_vals;
    int arr_count;
    /* char array of strings: e.g. char texts[3][20] */
    char **arr_strs;
    /* array of struct members (for struct T arr[N]) */
    struct GclStructValue **arr_members;
    struct GclStructValue *members; /* if this is a struct variable */
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
    /* TURN 26 — ONE array of GclParam {name, type}, copied from the AST.
       Previously the runner kept a SECOND pair of parallel arrays (`params` +
       `param_types`); two structures holding the same fact is what let them
       drift, and the struct-parameter path left the type slot uninitialized. */
    GclParam *params;
    int param_count;
    GclStmt *body;
    /* Donus tipi. Bir degerin turu fonksiyon sinirindan gecerken kaybolmasin
       diye tasinir: `char pick()`in sonucu karakter, `gcChar greet()`in sonucu
       metin olarak yazdirilir. Parametre tipi artik `params[i].type` alanidir. */
    char *return_type;
    struct FuncDef *next;
} FuncDef;

/* GCL library loaded via #lib <Name> (Name.gclib) */
/* #lib/.gclib support removed — LibModule type eliminated */

/* A loaded native module */
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
    /* main(int argc, char *argv[]) support */
    int arg_count;
    char **arg_vals;
} GclEnv;

/* forward declarations */
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

/* Remove a block-scoped variable (like a for-init) from env (end of scope) */
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

/* Blok cikisi: bir bloga girmeden ONCEKI liste basini (`mark`) alir ve mark'in
   ONUNDE kalan (yani blok ICINDE olusan) degiskenleri siler. Boylece dongu
   govdesinde tanimlanan degiskenler dongu bitince yasar (C blok-scope).
   `global` isaretli degiskenler korunur. `mark` NULL ise env tamamen bloktu. */
static void env_scope_exit(GclEnv *env, Var *mark) {
    if (!env) return;
    Var **pp = &env->vars;
    while (*pp && *pp != mark) {
        Var *v = *pp;
        if (v->is_global) { pp = &v->next; continue; }
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
    /* real overflow/clamp semantics for types like int8/uint8/.../float32 */
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

/* Narrow `val` to a long long with a DEFINED result for inf/nan, and with the
   DECLARED type's own bounds wherever the C conversion is undefined.

   Two regimes, and the line between them is exactly where C stops defining the
   answer (TURN 43):

   1. -2^63 <= val < 2^63 -- `(long long)val` is DEFINED, so GCL follows C and
      nothing here interferes: the caller's cast performs the C conversion, so
      `int8 = 200` is -56, `uint8 = -1` is 255, `uint64 = -1` is 2^64-1.
   2. |val| >= 2^63 -- `(long long)val` is UNDEFINED BEHAVIOUR (on x86-64
      cvttsd2si yields INT64_MIN, so a POSITIVE overflow came back NEGATIVE).
      TURN 31 clamped here, but only to `long long`, and the declared type's
      cast then took the LOW BITS of that clamp: the sign-flip TURN 31 removed
      at int64 survived one level down -- `char x = 1e30` printed -1 and
      `int x = 1e30` printed -1 -- while `uint64 x = -1e30` printed
      +9.2233720368547758e+18 instead of 0. The clamp now uses the DECLARED
      TYPE's own bounds, so a value too large for its type saturates to that
      type's ceiling: `int8 = 1e30` is 127, `int = 1e30` is 2147483647,
      `uint64 = -1e30` is 0.

   Non-finite (inf/nan) narrows to 0: the GCL3008 diagnostic is what reports the
   error, the stored value only has to be predictable. Float/double targets are
   NOT affected -- they keep IEEE inf/nan (see the float branches).

   2^63 is written as a double ON PURPOSE: it is exactly representable, while
   the integer constant LLONG_MAX (2^63-1) would round back UP to 2^63 as a
   double and make the comparison useless. `val < -2^63` (not <=) because -2^63
   itself IS representable in long long and belongs to regime 1. */
#define GCL_LL_TWO63      9223372036854775808.0    /*  2^63 exact, as a double */
#define GCL_LL_NEG_TWO63 (-9223372036854775808.0)  /* -2^63 exact, as a double */

/* The declared integer type's own floor and ceiling, carried as the long long
   BIT PATTERN that saturates to it. Unsigned 64-bit types are why the ceiling
   is a pattern and not a value: the largest uint64 has no positive long long
   spelling, so its ceiling is -1 (all ones) and the caller's `(unsigned long
   long)` cast turns it into 2^64-1 -- exactly as it does for a real value. */
typedef struct {
    long long lo;
    long long hi;
} GclIntBounds;

/* 1 when `decl_type` is an integer type this runner narrows; `out` then holds
   its bounds. A type that is NOT an integer (float, double, gcChar, a struct, a
   typedef alias) answers 0 and is narrowed by its own branch instead. */
static int int_type_bounds(const char *decl_type, GclIntBounds *out) {
    if (!decl_type || !decl_type[0]) return 0;
    /* signed -- as wide as the C type the matching branch down below casts to */
    if (strcmp(decl_type, "int8") == 0)  { out->lo = -128L;   out->hi = 127L;   return 1; }
    if (strcmp(decl_type, "int16") == 0) { out->lo = -32768L; out->hi = 32767L; return 1; }
    if (strcmp(decl_type, "char") == 0)  { out->lo = -128L;   out->hi = 127L;   return 1; }
    if (strcmp(decl_type, "int64") == 0 || strcmp(decl_type, "int128") == 0 ||
        strcmp(decl_type, "long long") == 0 || strcmp(decl_type, "long long int") == 0) {
        out->lo = LLONG_MIN; out->hi = LLONG_MAX; return 1;
    }
    if (strcmp(decl_type, "int32") == 0 || strcmp(decl_type, "int") == 0 ||
        strcmp(decl_type, "long") == 0 || strcmp(decl_type, "long int") == 0 ||
        strcmp(decl_type, "unsigned") == 0 || strcmp(decl_type, "unsigned int") == 0 ||
        strcmp(decl_type, "unsigned long") == 0) {
        /* The C basics AND their unsigned spellings. Every one of them is
           narrowed through the SAME `(int)` cast below, so `unsigned int` is
           32-bit here. Giving `unsigned` a real unsigned range is a SEPARATE
           decision and cannot be made in this branch alone: regime 1 follows C,
           and C says `(unsigned)-1` is 2^32-1, which the DEFINED path would
           have to change too. Left as it is, on purpose. */
        out->lo = -2147483648L; out->hi = 2147483647L; return 1;
    }
    if (strcmp(decl_type, "short") == 0 || strcmp(decl_type, "short int") == 0) {
        out->lo = -32768L; out->hi = 32767L; return 1;
    }
    /* unsigned */
    if (strcmp(decl_type, "uint8") == 0)  { out->lo = 0L; out->hi = 255L;        return 1; }
    if (strcmp(decl_type, "uint16") == 0) { out->lo = 0L; out->hi = 65535L;      return 1; }
    if (strcmp(decl_type, "uint32") == 0) { out->lo = 0L; out->hi = 4294967295L; return 1; }
    if (strcmp(decl_type, "uint64") == 0 || strcmp(decl_type, "uint128") == 0) {
        out->lo = 0L;
        out->hi = -1L;   /* all ones -- 2^64-1 through the (unsigned long long) cast */
        return 1;
    }
    return 0;
}

static long long gcl_to_ll(double val, const char *decl_type) {
    if (!isfinite(val)) return 0;
    if (val >= GCL_LL_NEG_TWO63 && val < GCL_LL_TWO63)
        return (long long)val;                    /* regime 1: C is defined */
    GclIntBounds b;
    if (!int_type_bounds(decl_type, &b))
        return val > 0 ? LLONG_MAX : LLONG_MIN;   /* not an integer type here */
    return val > 0 ? b.hi : b.lo;                 /* regime 2: the declared type's bound */
}

/* TURN 34 - may the exact-lexeme mirror be kept for this literal?

   `uint64`/`uint128` keep a mirror of the literal's EXACT text so that a value
   beyond 2^53 can print its digits instead of a double's rounded ones (see the
   declaration path below). Two gates decide whether keeping it is truthful.

   1. The text must BE an integer spelling. A lexeme carrying a point, an
      exponent or a suffix (`1e18`, `10.0`, `9007199254740993u`) is not one:
      keeping it would print `1e18` for a `uint64` that stores
      1000000000000000000 - the "one value, two texts" wart this gate exists to
      prevent. Exponent literals only reach here at all because the lexer now
      accepts them (TURN 33).

   2. The written DIGITS must fit the type. The old gate compared the parsed
      double against 2^63, and a double cannot answer this question: both
      18446744073709551615 and 18446744073709551616 parse to the SAME double
      2^64, yet only the first is a `uint64`; and 12345678901234567890 - a
      perfectly good `uint64` - was dropped and printed as 1.2345678901234567e+19
      even though it WAS stored exactly. So the comparison is a decimal
      digit-string compare against the type's own ceiling.

   `uint64` and `uint128` share that ceiling: the interpreter stores both through
   the SAME 64-bit gate (`truncate_to_declared_type` casts both to
   `unsigned long long`; the "128" in the name is aspirational - see
   simple_doc.md), so a uint128 literal above 2^64-1 is saturated just like a
   uint64 one and must not keep its mirror either. Negative values cannot reach
   this function: the lexer only produces digits, and a leading '-' is a
   separate unary operator. */
#define GCL_UINT64_MAX_DIGITS "18446744073709551615"

static int unsigned_mirror_digits_fit(const char *lex, const char *decl_type) {
    if (!lex || !lex[0] || !decl_type) return 0;
    if (strcmp(decl_type, "uint64") != 0 && strcmp(decl_type, "uint128") != 0) return 0;
    /* (1) an integer spelling: decimal digits only */
    for (const char *q = lex; *q; q++) {
        if (*q < '0' || *q > '9') return 0;
    }
    /* (2) the value fits: compare digit strings; leading zeros are not significant */
    const char *p = lex;
    while (*p == '0') p++;
    if (!*p) return 1;                       /* the value 0 */
    const char *maxd = GCL_UINT64_MAX_DIGITS;
    size_t len = strlen(p), maxlen = strlen(maxd);
    if (len != maxlen) return len < maxlen;
    return strcmp(p, maxd) <= 0;
}

/* THE single narrowing choke point: it is named in exactly ONE forward
   declaration and every storage path in this file reaches it (declaration,
   plain assignment, compound assignment, compound-literal leaves, struct
   members, array elements, cast). A value is a double; this gives each declared
   type its own width on top of it.

   WIDTHS, stated here because three of the spellings are WIDER-NAMING THAN THE
   STORAGE (TURN 43, documented in simple_doc.md):
     int8/int16/int32/int64    REAL - 1/2/4/8 bytes, C conversion at regime 1
     uint8/uint16/uint32/uint64 REAL
     int128 / uint128          ALIASES of int64 / uint64. There is no 128-bit
                               arithmetic in the interpreter - the stored value
                               is an IEEE double, which is already only exact to
                               2^53 - so a "128-bit" type would hold exactly the
                               same bits as its 64-bit neighbour. They are
                               accepted as spellings and narrowed identically.
     float16 / float32         ALIASES of `float` (4 bytes)
     float64 / double          the stored form itself
     float128                  ALIAS of `double`
     bool                      0/1
   `int`/`long`/`short`/`char` and their unsigned spellings are C's basics; each
   casts to the C type named in its branch (see int_type_bounds for the note on
   `unsigned` being narrowed through `int`). */
static double truncate_to_declared_type(double val, const char *decl_type) {
    if (!decl_type || !decl_type[0]) return val;
    /* bool → 0/1 */
    if (strcmp(decl_type, "bool") == 0) return val != 0.0 ? 1.0 : 0.0;
    /* signed integers. gcl_to_ll takes the type so that a value outside long
       long saturates to THIS type's ceiling instead of taking the low bits of
       a long long clamp. */
    if (strcmp(decl_type, "int8") == 0) return (double)(signed char)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "int16") == 0) return (double)(short)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "int32") == 0) return (double)(int)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "int64") == 0) return (double)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "int128") == 0) return (double)gcl_to_ll(val, decl_type);
    /* unsigned integers */
    if (strcmp(decl_type, "uint8") == 0) return (double)(unsigned char)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "uint16") == 0) return (double)(unsigned short)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "uint32") == 0) return (double)(unsigned int)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "uint64") == 0) return (double)(unsigned long long)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "uint128") == 0) return (double)(unsigned long long)gcl_to_ll(val, decl_type);
    /* float tipleri — float16/float32 collapse to a 4-byte float, and
       float128 collapses to the 8-byte double that IS the storage. The
       branches name them so the widths are stated in the code rather than
       falling out of the default `return val`. */
    if (strcmp(decl_type, "float") == 0 || strcmp(decl_type, "float32") == 0 || strcmp(decl_type, "float16") == 0)
        return (double)(float)val;
    if (strcmp(decl_type, "float64") == 0 || strcmp(decl_type, "double") == 0 ||
        strcmp(decl_type, "float128") == 0)
        return val;
    /* B3 — C'nin TEMEL tam sayı tipleri. Eskiden yalnızca int8..int128 /
       uint8..uint128 tanınıyordu; `int`/`long`/`short`/`char` listede yoktu ve
       değer double olarak saklanıyordu: `int c = 7 / 2;` → 3.5 (C'de 3). */
    if (strcmp(decl_type, "int") == 0 || strcmp(decl_type, "long") == 0 ||
        strcmp(decl_type, "long int") == 0 || strcmp(decl_type, "unsigned") == 0 ||
        strcmp(decl_type, "unsigned int") == 0 || strcmp(decl_type, "unsigned long") == 0)
        return (double)(int)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "short") == 0 || strcmp(decl_type, "short int") == 0)
        return (double)(short)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "char") == 0) return (double)(signed char)gcl_to_ll(val, decl_type);
    if (strcmp(decl_type, "long long") == 0 || strcmp(decl_type, "long long int") == 0)
        return (double)gcl_to_ll(val, decl_type);
    return val;
}

/* The width, in bytes, of one stored value of a declared type -- the number
   `sizeof(T)` reports (TURN 43).

   It is derived from the SAME fact truncate_to_declared_type narrows with, and
   that is the whole point of having it here: the runtime value is a double, and
   each sized type keeps the C width of the cast it narrows through. So the
   "128-bit" names report 8 and `float16` reports 4 -- see the WIDTHS note above
   truncate_to_declared_type.

   One table, so `sizeof(uint128)` cannot disagree with what a `uint128`
   declaration actually stores. The list used to live inside the sizeof branch
   of eval_expr and had drifted in exactly the two places you would expect: the
   three "128-bit" names claimed 16 bytes while the interpreter stores 8, and
   `float16` was missing altogether so it fell through to the double default.

   Returns 0 for a name that is not a sized type (gcChar, a struct, a module
   type, a typo); the caller keeps its own default. */
static int decl_type_size(const char *t) {
    if (!t || !t[0]) return 0;
    if (strcmp(t, "bool") == 0 || strcmp(t, "char") == 0 ||
        strcmp(t, "int8") == 0 || strcmp(t, "uint8") == 0) return 1;
    if (strcmp(t, "short") == 0 || strcmp(t, "short int") == 0 ||
        strcmp(t, "int16") == 0 || strcmp(t, "uint16") == 0) return 2;
    /* `unsigned`/`unsigned long` are narrowed through `(int)` (see
       int_type_bounds), so they are 4 here for the same reason. */
    if (strcmp(t, "int") == 0 || strcmp(t, "int32") == 0 || strcmp(t, "uint32") == 0 ||
        strcmp(t, "long") == 0 || strcmp(t, "long int") == 0 ||
        strcmp(t, "unsigned") == 0 || strcmp(t, "unsigned int") == 0 ||
        strcmp(t, "unsigned long") == 0 ||
        strcmp(t, "float") == 0 || strcmp(t, "float32") == 0 ||
        strcmp(t, "float16") == 0) return 4;
    /* int128/uint128/float128 are ALIASES of the 8-byte types they narrow
       through, so they report 8. `long double` has no narrowing branch at all
       (it is not one of the lexer's sized type names), so a value declared that
       way is simply the stored double -- also 8. */
    if (strcmp(t, "long long") == 0 || strcmp(t, "long long int") == 0 ||
        strcmp(t, "int64") == 0 || strcmp(t, "uint64") == 0 ||
        strcmp(t, "int128") == 0 || strcmp(t, "uint128") == 0 ||
        strcmp(t, "double") == 0 || strcmp(t, "float64") == 0 ||
        strcmp(t, "float128") == 0 || strcmp(t, "long double") == 0) return 8;
    return 0;
}

/* Build the struct member list (recursive nested struct) */
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
        /* the member's declared type — used for clamping types like int8/float32 */
        if (def->member_types && def->member_types[i])
            m->decl_type = strdup(def->member_types[i]);
        /* if the member type is another struct, build it recursively */
        if (def->member_types && def->member_types[i]) {
            StructDef *nsd = find_struct(env, def->member_types[i]);
            if (nsd) build_members_recursive(env, &m->members, nsd);
        }
        m->next = *head;
        *head = m;
    }
}

/* Create a struct variable */
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

/* ---------- Native (raylib) struct degiskenleri ----------

   Raylib/Raygui struct'lari GCL'de bir DEGERDIR ve iki yazim gecerlidir:
       Rectangle r;          // duz
       Raylib.Rectangle r;   // modul oneki ile (bkz. complete/complete_type.c)
   Alan listesi gcl_native_types.c'de tek kaynaktan gelir. Bu yol olmadan
   `r.x` SESSIZCE 0 donuyordu: hata yok, yanlis deger var. */

/* Tipi native struct'a coz; modul oneki (`Raylib.Rectangle`) soyulur. */
static const GclNativeStruct *lookup_native_struct(const char *type) {
    if (!type || !type[0]) return NULL;
    const GclNativeStruct *ns = gcl_native_struct(type);
    if (ns) return ns;
    const char *dot = strrchr(type, '.');
    return dot ? gcl_native_struct(dot + 1) : NULL;
}

/* Native struct alanlarini kur (ic ice struct'lar ozyinelemeli). */
static void build_native_members(GclStructValue **head, const GclNativeStruct *ns) {
    if (!head || !ns) return;
    *head = NULL;
    for (int i = ns->field_count - 1; i >= 0; i--) {
        GclStructValue *m = (GclStructValue *)calloc(1, sizeof(GclStructValue));
        if (!m) continue;
        m->name = strdup(ns->fields[i].name ? ns->fields[i].name : "");
        m->num = 0.0;
        m->is_string = 0;
        m->members = NULL;
        if (ns->fields[i].type) {
            m->decl_type = strdup(ns->fields[i].type);
            /* Alan baska bir native struct ise (Vector3 position) icini de kur;
               yoksa `camera.position.x` sessizce 0 donerdi. */
            const GclNativeStruct *sub = gcl_native_struct(ns->fields[i].type);
            if (sub) build_native_members(&m->members, sub);
        }
        m->next = *head;
        *head = m;
    }
}

/* Native struct degiskeni yarat (varsa uzerine yaz). */
static void env_set_native_struct(GclEnv *env, const char *name, const GclNativeStruct *ns) {
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
    build_native_members(&v->members, ns);
    if (v->decl_type) free(v->decl_type);
    v->decl_type = strdup(ns->type ? ns->type : "");
}

/* Read a struct member value */
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

/* Write a struct member value — clamp according to the member type */
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

/* Write a struct member string value */
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

/* Copy a struct value (deep copy from the same list, recursive nested) */
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

/* Look up a typedef */
static const char *find_typedef_base(GclEnv *env, const char *alias) {
    for (TypedefDef *td = env->typedefs; td; td = td->next) {
        if (strcmp(td->alias, alias) == 0) return td->base;
    }
    return NULL;
}

/* Look up a struct definition */
static StructDef *find_struct(GclEnv *env, const char *name) {
    for (StructDef *sd = env->structs; sd; sd = sd->next) {
        if (strcmp(sd->name, name) == 0) return sd;
    }
    return NULL;
}

/* Look up a function */
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
    /* B31 — `break`/`continue` yalnizca bir dongu/switch ICINDE gecerlidir.
       Sayaclar olmadan dongu DISINDAKI bir `break`, exec_block'un dongusunu
       durdurup blogu SESSIZCE bitiriyordu: hata yok, exit 0, kullaniciya
       hicbir sey soylenmiyordu (`bug_33_breakout`).
       INCE NOKTA: call_user_func ve gcl_run_program Runner'i `memcpy` ile
       KOPYALAR, bu yuzden `sub` icinde bu sayaclar SIFIRLANIR — aksi halde
       dongu icinden cagrilan bir fonksiyondaki `break` "gecerli" sayilirdi. */
    int loop_depth;       /* kac dongu icinde? (`continue` icin) */
    int breakable_depth;  /* kac dongu/switch icinde? (`break` icin) */
    GclStructValue *return_struct;  /* struct return value */
    GclExpr *return_init_list;      /* struct literal return { ... } */
    /* Metin donduren fonksiyonlar icin kanal (gcChar / char* / char[N]).
       `return_value` yalnizca bir double tasir; bu kanal olmadan bir
       fonksiyonun dondurdugu METIN kayboluyordu ve `printf("{}", greet())`
       0 yaziyordu. Sahibi bu Runner'dir; tuketiciler yalnizca OKUR. */
    char *return_str;
} Runner;

/* ---------- Host API: modüllerden GCL'e GERİ ÇAĞRI (callback köprüsü) ----------

   Modüllere `const char **argv` gider; bu yüzden bir GCL fonksiyonu C'ye
   geçirilemiyordu ve C işaretçisi alan raylib üyeleri (SetTraceLogCallback,
   SetLoadFileDataCallback, SetAudioStreamCallback, ...) no-op stub'tı. Bu
   blok, modülün ÇAĞRILACAK GCL fonksiyonunun ADINI alıp gerçekten
   çağırabilmesini sağlar (bkz. include/gcl_module.h).

   İŞ PARÇACICI GÜVENLİĞİ: call_gcl YORUMLAYICININ ORTAMINA girer, bu yüzden
   YALNIZCA ANA iş parçacığından çağrılabilir. Ses callback'i ayrı bir iş
   parçacığında koşar; modül orada ASLA call_gcl çağırmaz, örnekleri halka
   tampona yazar, üretimi gcl_modules_tick() (ana iş parçacığı) üzerinden
   bu fonksiyonla yapar. */

static Runner        *g_active_runner = NULL;
static GclHostApi     g_host_api;

/* B2 — özyineleme derinliği koruması. Kullanıcı fonksiyon çağrıları NATIVE C
   yığını üzerinde koşar (call_user_func → exec_block → eval_expr →
   call_user_func); sınır olmadan derin özyineleme STATUS_STACK_OVERFLOW ile
   ÇÖKÜYORDU (exit 0xC00000FD, mesajsız). Sınırda kontrollü hata verilir. */
#ifdef _WIN32
/* gcl.exe 64 MB yiginla baglanir (makefile.py: -Wl,--stack,67108864);
   kare basina ~2 KB ile 8192 cagri ~16 MB yapar ve rahat sigar. */
#define GCL_MAX_CALL_DEPTH_DEFAULT 8192
#else
/* POSIX'te ana is parcacigi yigini ulimit'ten gelir (tipik 8 MB). Kare
   basina ~2 KB ile 2048 guvenli mesafededir; daha derini icin derleme
   bayragi/ulimit buyutulup GCL_MAX_CALL_DEPTH yukseltilmelidir. */
#define GCL_MAX_CALL_DEPTH_DEFAULT 2048
#endif
static int g_call_depth = 0;
static int g_max_call_depth = 0;   /* 0 = henuz okunmadi */

/* B2 — cagri derinligi siniri. ONEMLI: bu sinir NATIVE C yiginina baglidir,
   bu yuzden gcl.exe buyuk yiginla baglanir (makefile.py:
   -Wl,--stack,67108864 → 64 MB). Kare basina ~2 KB ile bu ~32.000 cagri
   demektir; varsayilan 8192 guvenli mesafede kalir. Daha derin (ya da daha
   sig) bir sinir isteyen `GCL_MAX_CALL_DEPTH` ortam degiskenini kullanabilir. */
static int max_call_depth(void) {
    if (g_max_call_depth > 0) return g_max_call_depth;
    int v = GCL_MAX_CALL_DEPTH_DEFAULT;
    const char *env = getenv("GCL_MAX_CALL_DEPTH");
    if (env && env[0]) {
        int n = atoi(env);
        if (n > 0) v = n;
    }
    g_max_call_depth = v;
    return v;
}

/* B34 - loop iteration limit. A runaway `while (1)` used to abort with a bare
   `return -1`, which a `try` could NEVER catch: the error was unrecoverable by
   design. The guard now reports through the normal error channel (code
   GCL3009), so `try/catch` can handle it, while an UNCAUGHT runaway loop still
   stops the process with a non-zero exit code - exactly the old behaviour.

   The limit is configurable because a long-running program is not a bug: a
   raylib game loop legitimately runs for hours. `GCL_MAX_LOOP_ITERATIONS`
   overrides the default. The counter is per loop ENTRY, not global, so N
   sequential loops may each run the full budget. */
#define GCL_MAX_LOOP_ITERATIONS_DEFAULT 1000000
static int g_max_loop_iterations = 0;   /* 0 = not read yet */
static int max_loop_iterations(void) {
    if (g_max_loop_iterations > 0) return g_max_loop_iterations;
    int v = GCL_MAX_LOOP_ITERATIONS_DEFAULT;
    const char *env = getenv("GCL_MAX_LOOP_ITERATIONS");
    if (env && env[0]) {
        int n = atoi(env);
        if (n > 0) v = n;
    }
    g_max_loop_iterations = v;
    return v;
}

/* call_user_func aşağıda tanımlı; host köprüsü onu burada çağırdığı için
   önden bildirilir (C99 örtük bildirime izin vermez). */
/* TURN 50 - `call_span` is the span of the CALL EXPRESSION, passed down so
   that "unknown function", "wrong number of arguments" and "call depth" point
   at the call the user wrote instead of at nothing. A callback coming FROM a
   native module has no call site in GCL source, so host_call_gcl passes
   span_none(). */
static double call_user_func(Runner *r, FuncDef *fd, GclExpr **args,
                             int arg_count, GclSpan call_span);

static int host_call_gcl(void *host, const char *fn_name, int argc,
                         const GclHostArg *argv, double *ret) {
    (void)host;
    if (ret) *ret = 0.0;
    if (!fn_name || !g_active_runner) return -1;
    FuncDef *fd = find_func(g_active_runner->env, fn_name);
    if (!fd) return -1;

    /* Argümanları AST düğümlerine çevir: call_user_func bağlamayı normal bir
       çağrıdaki gibi yapar (AST_EXPR_STRING → metin parametresi). Düğümler
       YIĞIN üzerindedir; call_user_func bunları yalnızca okur. */
    GclExpr  nodes[16];
    GclExpr *ptrs[16];
    int n = argc;
    if (n < 0) n = 0;
    if (n > 16) n = 16;
    if (n > fd->param_count) n = fd->param_count;   /* fazlası bağlanamaz */
    for (int i = 0; i < n; i++) {
        memset(&nodes[i], 0, sizeof(nodes[i]));
        if (argv[i].is_string) {
            nodes[i].kind = AST_EXPR_STRING;
            nodes[i].str  = (char *)(argv[i].str ? argv[i].str : "");
        } else {
            nodes[i].kind = AST_EXPR_FLOAT;
            nodes[i].num  = argv[i].num;
        }
        ptrs[i] = &nodes[i];
    }
    double v = call_user_func(g_active_runner, fd, ptrs, n, span_none());
    if (ret) *ret = v;
    return 0;
}

/* Tick: modüllerin ERTELENMİŞ işlerini (ses örneği üretimi gibi) ana iş
   parçacığında çalıştırır. Tick içinde native bir çağrı olursa özyinelemeyi
   keser — aksi hâlde tick → GCL → tick kilitlenirdi. */
#define GCL_MAX_TICK_MODULES 16
static GclModuleTickFn g_tick_fns[GCL_MAX_TICK_MODULES];
static int             g_tick_count = 0;

static void gcl_modules_tick(void) {
    static int in_tick = 0;
    if (in_tick) return;
    in_tick = 1;
    for (int i = 0; i < g_tick_count; i++)
        if (g_tick_fns[i]) g_tick_fns[i](&g_host_api);
    in_tick = 0;
}

/* eval_expr forward declaration */
static double eval_expr(GclExpr *e, Runner *r);
/* PLAYER.Camera gibi bir uyeden kopyalanan native struct degiskenleri canli
   kalir: her native modul cagrisindan once bu yardimci onlari tazeler. */
static void refresh_native_aliases(GclEnv *env);

/* Modul sahipli struct tipleri (FPS/Terrain): alanlar modulun LastSlot
   kanalindan gelir. Tanim asagida; call_native_member kullanir. */
typedef struct {
    const char *type;
    const char *module;
    int         count;
} GclNativeSlotMap;
static const GclNativeStruct *lookup_native_struct(const char *type);
static int collect_var_leaves(GclEnv *env, const char *var_name,
                              GclStructValue **leaves, int cap);
static const GclNativeSlotMap *native_slot_map_for_type(const char *type);
static int store_slots_into_var(GclEnv *env, const char *var_name,
                                const char *module, int count);
static void write_back_struct_args(Runner *r, const char *module,
                                   GclExpr **args, int arg_count);
static double call_native_struct_method(GclSpan span, const char *var_name,
                                        const char *member, Runner *r, int *handled);
/* Forward declaration for nested struct init — fill_members_from_init_list is recursive */
static void fill_members_from_init_list(Runner *r, GclStructValue *members, GclExpr *init);

/* Write a value to a struct member — string if it is a string, number if it is a number */
static void set_member_value(GclStructValue *m, GclExpr *item, Runner *r) {
    if (!m || !item) return;
    /* Nested struct init: Vector3 position = { 10, 20, 30 } → apply recursively to sub-members */
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

/* Apply a struct init list to the member list (positional + designated) */
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
                /* .member = value → AST_EXPR_VAR + member_name in some cases */
                const char *mname = item->left->name;
                for (GclStructValue *ms = members; ms; ms = ms->next) {
                    if (ms->name && strcmp(ms->name, mname) == 0) {
                        set_member_value(ms, item->right, r);
                        break;
                    }
                }
            }
        } else if (m) {
            /* positional: { val1, val2, ... } */
            set_member_value(m, item, r);
            m = m->next;
        }
    }
}

/* Fill struct init — VAR copy, CALL return, INIT_LIST (positional + designated) */
static void fill_struct_init(Runner *r, const char *var_name, GclExpr *init) {
    Var *v = env_find(r->env, var_name);
    if (!v || !v->members || !init) return;

    if (init->kind == AST_EXPR_INIT_LIST) {
        fill_members_from_init_list(r, v->members, init);
        return;
    }

    /* init is a variable: copy from the same struct (Hello f = e;) */
    if (init->kind == AST_EXPR_VAR) {
        Var *sv = env_find(r->env, init->name);
        if (sv && sv->members) {
            free_struct_members(v->members);
            v->members = clone_struct_members(sv->members);
        }
        return;
    }

    /* init is a function call: return { ... } or return struct */
    if (init->kind == AST_EXPR_CALL) {
        /* when call_expr is evaluated, return_struct/return_init_list gets filled */
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

/* `Raylib.Rectangle r = Raylib.Rectangle(x, y, w, h);`

   Modul cagrisi degeri KENDI durumunda tutar (raylib: g_last_rect); degiskene
   bir sey kopyalamaz. Argumanlar bu yuzden alanlara sirayla yazilir — yoksa
   `r` sifirlarla dolu kalir ve `r.width` sessizce 0 okunurdu. Cagri tipin
   KURUCUSU degilse (baska bir fonksiyon/ad) hicbir sey yapilmaz.

   Donus: alanlar doldurulduysa 1. */
static int fill_native_from_constructor(Runner *r, const char *var_name,
                                        const GclNativeStruct *ns, GclExpr *call) {
    if (!r || !ns || !ns->type || !call || call->kind != AST_EXPR_CALL) return 0;
    GclExpr *callee = call->left;
    if (!callee || callee->kind != AST_EXPR_MEMBER || !callee->member_name) return 0;
    if (strcmp(callee->member_name, ns->type) != 0) return 0;
    Var *v = env_find(r->env, var_name);
    if (!v || !v->members) return 0;
    GclStructValue *m = v->members;
    for (int i = 0; i < call->arg_count && m; i++) {
        if (!call->args[i]) continue;
        set_member_value(m, call->args[i], r);
        m = m->next;
    }
    return 1;
}

/* ---------- GCL lib modules (#lib <Name>) ---------- */

/* Forward declaration of eval_expr and call_user_func */
static double eval_expr(GclExpr *e, Runner *r);
static double call_user_func(Runner *r, FuncDef *fd, GclExpr **args,
                             int arg_count, GclSpan call_span);

/* Resolve an AST_EXPR_MEMBER chain: p.addr.city -> the final GclStructValue */
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
        /* Nested chain: if e->left is a MEMBER, parent is the last member,
           so we must search its members list. If e->left is a VAR, parent is
           already the root struct's member list. */
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

/* B5 - the TEXT an initializer expression denotes, or NULL when it has none.

   `gcChar`/`char[N]`/2D-char declarations used to accept only a string LITERAL;
   every other spelling of the same string silently became "". Three live misses:
   `gcChar c = f();` (the text a call returns travels in Runner.return_str, not in
   the double `v`), `char buf[N] = s;` (a variable) and
   `char names[N][M] = { s, "x" };` (a variable inside the init list).
   One rule for all of them: a string-valued initializer is a literal, a string
   VARIABLE, or a CALL that left its text in return_str. Anything else - a plain
   number, a vanilla `char`, a struct - returns NULL and the caller keeps its
   numeric behaviour. */
static const char *init_text_value(Runner *r, GclExpr *init) {
    if (!r || !init) return NULL;
    if (init->kind == AST_EXPR_STRING) return init->str ? init->str : "";
    if (init->kind == AST_EXPR_VAR) {
        Var *src = env_find(r->env, init->name);
        if (src && src->is_string && src->str) return src->str;
        return NULL;
    }
    /* The caller has already evaluated the call, so the text it returned is in
       the channel - reading the channel is the only way to see it. */
    if (init->kind == AST_EXPR_CALL) return r->return_str;
    return NULL;
}

/* B5 (2D half) - resolve one ROW of a `char a[N][M] = { ... };` initializer.

   The row gate used to accept only AST_EXPR_STRING entries, so every other
   spelling of the same text fell through to the numeric branch:
   `char names[2][16] = { a, "two" };` stored atof("hello")=0 for `a` and the
   single byte 't' for "two", and `names[0]` printed "" while `names[1]` printed
   "t" instead of "two". An entry is TEXT when it is a literal, a string
   VARIABLE, or a CALL - the same three spellings init_text_value() already
   serves for the 1D case.

   Each entry is evaluated EXACTLY ONCE: a call must not run twice, and its text
   lives in Runner.return_str only until the next call refreshes it, so it is
   read immediately after the call that produced it.

   `vals[i]` always receives the entry's numeric value (0 for a text entry).
   Returns 1 when EVERY entry denotes text - the caller then keeps `texts` as
   the row table; otherwise it keeps `vals` and frees `texts`. */
static int collect_text_row(Runner *r, GclExpr *init, int cnt, double *vals, char **texts) {
    int all_text = cnt > 0;
    for (int ai = 0; ai < cnt; ai++) {
        GclExpr *it = init ? init->args[ai] : NULL;
        const char *t = NULL;
        if (it) {
            if (it->kind == AST_EXPR_CALL) {
                vals[ai] = eval_expr(it, r);
                t = r->return_str;   /* set by call_user_func, NULL for a numeric return */
            } else {
                t = init_text_value(r, it);
                vals[ai] = t ? 0.0 : eval_expr(it, r);
            }
        }
        if (t) texts[ai] = strdup(t);
        else all_text = 0;
    }
    return all_text;
}

/* ---------- TEK KURAL (TUR 22): bir `char` degeri KARAKTERDIR ----------

   `printf`in `{}` yer tutucusu bir degeri STATIK TIPINE gore yazar:

       gcChar / char* / char[N]  ->  metin           ("abc")
       char                      ->  TEK KARAKTER    ('A' -> "A")
       diger her sey             ->  sayi            (65)

   Sayisal baglam (int'e atama, aritmetik, karsilastirma, bitsel islemler)
   HER ZAMAN kodu gorur: `int m = c;` -> 65, `c + 1` -> 66. Kuralin tamami
   budur: char degerleri karakter, sayilar sayidir.

   Eskiden bu kural ARGUMAN DONUSTURUCUSUNE dagitilmisti ve yalnizca BAZI
   dugum turlerinde uygulaniyordu — ayni deger ifade bicimine gore iki farkli
   sey basiyordu:

       `char c`     -> 'A'   (VAR dali)     DOGRU
       `char a[i]`  -> 'b'   (ARRAY dali)   DOGRU
       `s[i]`       -> 'b'   (is_string)    DOGRU
       `pick()`     -> 'Z'   (CALL dali)    DOGRU
       `'A'`        -> 65    (int sabiti)   DOGRU  <-- C'de de `'A'` int'tir
       `(char)65`   -> 65    (else dali)    YANLIS
       `p.letter`   -> 65    (MEMBER dali)  YANLIS

   Donus: 1 = ifadenin STATIK tipi `char` (tek karakter), *code = kodu.
          0 = char DEGIL — cagiran kendi yolundan devam eder. */
static int expr_is_char_value(GclExpr *e, Runner *r, int *code) {
    if (!e) return 0;
    /* Not: `'A'` bir int sabitidir (B29/TUR 22), char DEGILDIR. */
    /* Cast: `(char)65` — hedef tip dogrudan karakterdir. */
    if (e->kind == AST_EXPR_CAST && e->str && strcmp(e->str, "char") == 0) {
        *code = (int)eval_expr(e->left, r);
        return 1;
    }
    /* `char c` — skaler, isaretcisiz, dizi olmayan char degiskeni. */
    if (e->kind == AST_EXPR_VAR) {
        Var *v = env_find(r->env, e->name);
        if (v && !v->is_string && !v->is_pointer && v->array_size == 0 &&
            !v->members && v->decl_type && strcmp(v->decl_type, "char") == 0) {
            *code = (int)v->num;
            return 1;
        }
        return 0;
    }
    /* `p.letter` — tipi `char` olan struct alani (metin alani DEGIL). */
    if (e->kind == AST_EXPR_MEMBER) {
        GclStructValue *mv = resolve_member_chain(e, r);
        if (mv && !mv->is_string && !mv->members &&
            mv->decl_type && strcmp(mv->decl_type, "char") == 0) {
            *code = (int)mv->num;
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Flatten struct members recursively for a native module: the sub-float members
   of a nested struct (Vector3 etc.) are also expanded in order. Camera3D.position → 3 args. */
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

/* Find a module */
static NativeModule *native_find(GclEnv *env, const char *name) {
    for (NativeModule *m = env->modules; m; m = m->next) {
        if (strcmp(m->name, name) == 0) return m;
    }
    return NULL;
}

/* scanf(name) or scanf("format", var...) — safe input, writes to env */
static double do_scanf(Runner *r, GclExpr **args, int arg_count) {
    if (arg_count < 1 || !args[0]) return 0.0;
    GclExpr *var = NULL;
    if (args[0]->kind == AST_EXPR_VAR) {
        /* scanf(name) — single argument: read into the variable */
        var = args[0];
    } else if (args[0]->kind == AST_EXPR_STRING && arg_count >= 2 &&
               args[1] && args[1]->kind == AST_EXPR_VAR) {
        /* scanf("format", var) — format + target variable */
        var = args[1];
    } else {
        return 0.0;
    }
    Var *v = env_find(r->env, var->name);
    if (!v) return 0.0;
    /* vanilla char — single character */
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
    /* Numeric read: scanf("%lf") reads the number but the trailing '\n'
       stays in the buffer. The next fgets-based string scanf would read it
       as an EMPTY line ("second scanf is not working"). So after the number,
       consume the rest of the line (including the newline) — this way
       consecutive scanf(int) + scanf(gcChar) works correctly. */
    double n;
    if (scanf("%lf", &n) == 1) env_set_num(r->env, var->name, n);
    { int ch; while ((ch = getchar()) != '\n' && ch != EOF) { } }
    return 1.0;
}

static NativeModule *native_load(GclEnv *env, const char *name);

/* Call a module function: Modul.member(args) */
static double call_native_member(GclSpan call_span, const char *module_name,
                                 const char *member, GclExpr **args,
                                 int arg_count, Runner *r) {
    /* Ertelenmiş callback işlerini (ses üretimi) ana iş parçacığında koştur.
       Modül çağrıları bir oyun döngüsünde her karede olduğu için bu, ses
       tamponunun düzenli beslenmesi için doğal ve yeterli kancadır. */
    gcl_modules_tick();
    /* PLAYER.Camera gibi uyelerden kopyalanan degiskenler, cagri aninda
       kaynagiyla ayni degeri tasimalidir: kopya bir kez alinip donmus olsa da
       her native cagridan once tazelenir. */
    refresh_native_aliases(r->env);
    NativeModule *mod = native_find(r->env, module_name);
    if (!mod) {
        /* If it is a built-in module (Math/Stdio/Embed), load lazily — for bare printf/scanf */
        mod = native_load(r->env, module_name);
    }
    if (!mod) {
        /* `X.method()` where X is a VARIABLE, not a module.
           Saying "unknown module 'CUBE'" here blamed a module that was never
           involved: CUBE is a variable whose type has no slot map, so
           call_native_struct_method() declined and the call fell through to
           this branch (the real fix for that case is the slot-map entry in
           g_native_slot_maps[]). Name the actual mistake so the next such
           failure is diagnosed from the message alone. */
        Var *bv = env_find(r->env, module_name);
        if (bv) {
            runtime_errorcf(GCL_E_SEM_UNKNOWN_MEMBER, call_span,
                            "'%s.%s' is not a module call: '%s' is a variable of type '%s'",
                            module_name, member ? member : "?",
                            module_name,
                            (bv->decl_type && bv->decl_type[0]) ? bv->decl_type : "unknown");
        } else {
            runtime_errorcf(GCL_E_SEM_UNKNOWN_MODULE, call_span,
                            "unknown module '%s'", module_name);
        }
        return 0.0;
    }
    /* Stdio.scanf — real input read, writes to env (same as bare scanf) */
    if (strcmp(module_name, "Stdio") == 0 && strcmp(member, "scanf") == 0) {
        return do_scanf(r, args, arg_count);
    }
    for (int i = 0; i < mod->entry_count; i++) {
        if (strcmp(mod->entries[i].name, member) == 0) {
            /* Convert GclExpr arguments into a string array.
               STRUCT VARIABLE argument: its members are expanded in declaration order.
               E.g. Camera3D camera → BeginMode3D(camera) → 11 args (position.x,y,z,
               target.x,y,z, up.x,y,z, fovy, projection). This is the real bridge from
               the GCL typedef struct system to native modules. */
            const char *argv[256];
            char numbuf[256][64];
            char chbuf[256][2];
            int ac = 0;
            for (int j = 0; j < arg_count && ac < 255; j++) {
                GclExpr *a = args[j];
                if (!a) { argv[ac++] = ""; continue; }
                /* `f(void)`: C'nin BOS parametre listesi. `void` bir degisken
                   degil, yer tutucudur; argüman olarak gecirilmez. Dokumante
                   edilen yazim tam olarak budur:
                       Raylib.EndMode3D(void);
                   Bu satir olmadan yer tutucu "undefined variable 'void'"
                   hatasi uretiyordu. */
                if (a->kind == AST_EXPR_VAR && strcmp(a->name, "void") == 0) continue;
                /* TEK KURAL (bkz. expr_is_char_value): char tipli argüman
                   modüle KARAKTER olarak geçer. `'A'`, `(char)65`, `char c`
                   ve `p.letter` ayni sekilde davranir. */
                {
                    int char_code = 0;
                    if (expr_is_char_value(a, r, &char_code)) {
                        int ci = ac;
                        chbuf[ci][0] = (char)char_code;
                        chbuf[ci][1] = '\0';
                        argv[ac++] = chbuf[ci];
                        continue;
                    }
                }
                /* Address-of: &camera → flatten the underlying struct members (UpdateCamera(&camera, ...)) */
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
                    /* B18: native çağrı argümanında TANIMSIZ değişken eskiden
                       SESSİZCE 0 olarak geçiyordu (eval_expr'in "undefined
                       variable" kontrolü bu yoldan geçmiyordu):
                       printf("{}", undefined_xyz) → "0", uyarı yok. */
                    if (!v) {
                        runtime_errorcf(GCL_E_SEM_UNKNOWN_VAR, span_of_expr(a),
                                        "undefined variable '%s'", a->name);
                        /* B33: inside a `try` body the native call must NOT run at
                           all. Previously this fell through to the `else` branch
                           below, wrote "0" into argv, and `printf("{}", undefined)`
                           printed that "0" BEFORE unwinding started (partial
                           output). In tolerant mode (outside `try`) g_unwinding
                           stays 0, so the legacy behaviour is preserved exactly. */
                        if (g_unwinding) return 0.0;
                    }
                    /* Struct variable → expand its members in declaration order */
                    if (v && v->members) {
                        flatten_struct_members(v->members, argv, numbuf, chbuf, &ac, 255);
                        continue;
                    }
                    if (v && v->is_string) {
                        argv[ac++] = v->str ? v->str : "";
                    } else if (v && v->decl_type && (strcmp(v->decl_type, "uint64") == 0 || strcmp(v->decl_type, "uint128") == 0) && v->str) {
                        argv[ac++] = v->str;
                    } else {
                        /* Not: `char` degiskeni buraya HIC gelmez — dongu
                           basindaki expr_is_char_value() onu zaten KARAKTER
                           olarak alir. Kural TEK yerde yasar. */
                        int idx = ac;
                        snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", v ? v->num : 0.0);
                        argv[ac++] = numbuf[idx];
                    }
                } else if (a->kind == AST_EXPR_ASSIGN && a->right) {
                    /* named/positional assignment argument: use the value on the right side */
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
                    /* String argument: titles like InitWindow(..., "GCL 3D Project")
                       must be passed through correctly. The old code converted this to
                       a number via eval_expr and sent "0" (the window title became "0"). */
                    argv[ac++] = a->str ? a->str : "";
                } else if (a->kind == AST_EXPR_ARRAY) {
                    /* argv[i] — pass as a string; also supports a normal array arr[i]. */
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
                    /* Native module constant: like Raylib.WHITE (without parentheses). */
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
                        /* struct member fallback (nested supported) */
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
                } else if (a->kind == AST_EXPR_CALL) {
                    /* Bir CAGRI sonucu TURUNU korusun. Eskiden bu arguman genel
                       dala dusuyor ve `%.17g` ile sayiya cevriliyordu:
                       `printf("{}", greet())` (gcChar) → 0,
                       `printf("{}", pick())`  (char)  → 90 yaziyordu. */
                    int idx = ac;
                    double cv = eval_expr(a, r);
                    if (r->return_str) {
                        argv[ac++] = r->return_str;   /* Runner'a ait; kopyalanmaz */
                    } else {
                        FuncDef *cfd = a->name ? find_func(r->env, a->name) : NULL;
                        if (cfd && cfd->return_type && strcmp(cfd->return_type, "char") == 0) {
                            chbuf[idx][0] = (char)(int)cv;
                            chbuf[idx][1] = '\0';
                            argv[ac++] = chbuf[idx];
                        } else {
                            snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", cv);
                            argv[ac++] = numbuf[idx];
                        }
                    }
                } else {
                    /* All other argument kinds (number, binop, etc.) */
                    int idx = ac;
                    snprintf(numbuf[idx], sizeof(numbuf[idx]), "%.17g", eval_expr(a, r));
                    argv[ac++] = numbuf[idx];
                }
            }
            /* B33: if an error was raised while converting the arguments
               (`eval_expr`, unknown member, out-of-bounds array, ...) the native
               call is NOT executed at all. Otherwise it would run with
               half-converted arguments and cause a side effect
               (e.g. `printf("{}", undefined + 1)` used to print "1"). */
            if (g_unwinding) return 0.0;
            return mod->entries[i].fn(ac, argv);
        }
    }
    runtime_errorcf(GCL_E_SEM_UNKNOWN_MEMBER, call_span,
                    "unknown member '%s.%s'", module_name, member);
    return 0.0;
}

/* The old code block below was removed (the new extended logic is above) */
#if 0
    for (int i = 0; i < mod->entry_count; i++) {
        if (strcmp(mod->entries[i].name, member) == 0) {
            /* Convert GclExpr arguments into a string array */
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
                    /* Native module constant: like Raylib.WHITE (without parentheses).
                       Search it first as a member of this module — not a struct member! */
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
                    /* argv[i] — pass as a string */
                    if (a->left && a->left->kind == AST_EXPR_VAR &&
                        strcmp(a->left->name, "argv") == 0 && r->env->arg_vals) {
                        int idx = (int)eval_expr(a->right, r);
                        if (idx >= 0 && idx < r->env->arg_count && r->env->arg_vals[idx]) {
                            argv[j] = r->env->arg_vals[idx];
                        } else {
                            argv[j] = "";
                        }
                    } else if (a->left && a->left->kind == AST_EXPR_VAR) {
                        /* Regular array: arr[i] → get the element value */
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
    runtime_errorcf(GCL_RT_CODE_UNKNOWN_MEMBER, "unknown member '%s.%s'", module_name, member);
    return 0.0;
#endif

/* Load a module: #native <Name> — in the order exe/Library, base_dir/Library, cwd/Library, PATH */
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

    /* Find the directory containing the exe (Library/ is under it) */
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
    /* Linux: Embed.so/gcl.so etc. carry a DT_NEEDED dependency on libpython3.14.so.1.0.
       Changing LD_LIBRARY_PATH via setenv is ineffective once the dynamic linker has
       started. So load libpython manually via dlopen first — this way the dependency is
       already resolved in memory when Embed.so opens. */
#ifdef __linux__
    {
        char py_lib[4096];
        snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.14.so.1.0", exe_dir);
        if (access(py_lib, 0) == 0) {
            void *h = dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
            if (!h) {
                /* alternative: libpython3.so */
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
    /* Export name: gcl_<name>_get_functions — convert to lowercase */
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

    /* Host API'yi modüle ver (varsa). İhraç etmeyen modüller eskisi gibi
       çalışır; bu yüzden ABI geriye uyumludur. */
    {
        GclModuleSetHostFn set_host = NULL;
#ifdef _WIN32
        void *sym = (void *)GetProcAddress(m->handle, "gcl_module_set_host");
        memcpy(&set_host, &sym, sizeof(set_host));
#else
        set_host = (GclModuleSetHostFn)dlsym(m->handle, "gcl_module_set_host");
#endif
        if (set_host) set_host(&g_host_api);
    }
    /* Tick ihraç ediyorsa kaydet: ertelenmiş işler ana iş parçacığında
       (her native çağrıdan önce) burada çalıştırılır. */
    {
        GclModuleTickFn tick = NULL;
#ifdef _WIN32
        void *sym = (void *)GetProcAddress(m->handle, "gcl_module_tick");
        memcpy(&tick, &sym, sizeof(tick));
#else
        tick = (GclModuleTickFn)dlsym(m->handle, "gcl_module_tick");
#endif
        if (tick && g_tick_count < GCL_MAX_TICK_MODULES)
            g_tick_fns[g_tick_count++] = tick;
    }

    m->next = env->modules;
    env->modules = m;
    return m;
}

/* ---------- #extern DLL + #register functions ---------- */

/* Find an extern DLL */
static GclExternDll *extern_dll_find(GclEnv *env, const char *dll_name) {
    for (int i = 0; i < env->extern_dll_count; i++) {
        if (strcmp(env->extern_dlls[i].dll_name, dll_name) == 0) return &env->extern_dlls[i];
    }
    return NULL;
}

/* Load an extern DLL — searched in exe_dir, base_dir, cwd, in order */
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

/* Parse the "ret|func|params" registration string and write it into GclExternReg */
static GclExternType extern_parse_type(const char *s) {
    if (!s) return GCL_EXT_VOID;
    /* Including the parameter name: "int width" → INT, "const char *title" → STRING,
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

    er->dll_name = strdup("");  /* for now the DLL name is independent of the register — searched in all extern DLLs */
    er->func_name = strdup(p1 + 1);
    er->ret = extern_parse_type(buf);

    /* params: "int width, int height, const char *title" */
    char *params = p2 + 1;
    er->param_count = 0;
    char *save = NULL;
    char *tok = strtok_r(params, ",", &save);
    while (tok && er->param_count < 8) {
        while (*tok == ' ' || *tok == '\t') tok++;
        /* trim trailing spaces and ')' ';' characters (the last token keeps the closing ");") */
        char *e = tok + strlen(tok) - 1;
        while (e >= tok && (*e == ' ' || *e == '\t' || *e == ')' || *e == ';')) *e-- = '\0';
        /* strip the parameter name: "const char *title" → type "const char *" */
        er->params[er->param_count++] = extern_parse_type(tok);
        tok = strtok_r(NULL, ",", &save);
    }
    env->extern_reg_count++;
}

/* Find an extern function: return fn_ptr if the name matches */
static void *extern_func_find(GclEnv *env, const char *func_name) {
    for (int i = 0; i < env->extern_reg_count; i++) {
        GclExternReg *er = &env->extern_regs[i];
        if (strcmp(er->func_name, func_name) == 0) {
            if (er->fn_ptr) return er->fn_ptr;
            /* if there is no fn_ptr, search all DLLs */
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

/* Call an extern C function — performing the type conversions */
static double call_extern_func(GclEnv *env, GclSpan call_span, const char *func_name,
                               GclExpr **args, int arg_count, Runner *r) {
    GclExternReg *er = NULL;
    for (int i = 0; i < env->extern_reg_count; i++) {
        if (strcmp(env->extern_regs[i].func_name, func_name) == 0) { er = &env->extern_regs[i]; break; }
    }
    if (!er) {
        runtime_errorcf(GCL_E_SEM_UNKNOWN_FUNC, call_span,
                        "unknown function '%s'", func_name);
        return 0.0;
    }
    void *fp = er->fn_ptr;
    if (!fp) fp = extern_func_find(env, func_name);
    if (!fp) {
        runtime_errorcf(GCL_E_SEM_UNKNOWN_FUNC, call_span,
                        "cannot resolve external function '%s'", func_name);
        return 0.0;
    }

    /* Convert GCL values to C types and call */
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

/* Forward declarations for the two body runners. `defers_run_frame` is
   defined before them and needs `run_cleanup_body`; `run_cleanup_body` in
   turn needs `run_nested`. */
static int run_nested(GclStmt *body, Runner *r);
static int run_cleanup_body(GclStmt *body, Runner *r);

/* Kullanıcı fonksiyonunu çağır */
/* B14 — struct argümanını parametreye ÜYE LİSTESİYLE kopyala.
   Eskiden call_user_func yalnızca skaler bağlıyordu; `P v` parametresine
   `a` geçilince üyeler kayboluyor ve `v.id` sessizce 0 dönüyordu. */
static void env_set_members_copy(GclEnv *env, const char *name, Var *src) {
    if (!env || !name) return;
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
    v->num = 0;
    if (v->members) free_struct_members(v->members);
    v->members = (src && src->members) ? clone_struct_members(src->members) : NULL;
    if (v->decl_type) { free(v->decl_type); v->decl_type = NULL; }
    if (src && src->decl_type) v->decl_type = strdup(src->decl_type);
}

/* B6 — dizi argümanını parametreye kopyala (değer semantiği).
   `int last(int arr[3]) { return arr[2]; }` + `last(nums)` eskiden 0
   dönüyordu: dizi argümanı eval_expr ile skalerleşiyor (v->num = 0) ve
   parametrede arr_vals hiç oluşmuyordu. */
static void env_set_array_copy(GclEnv *env, const char *name, Var *src) {
    if (!env || !name || !src) return;
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
    if (v->members) { free_struct_members(v->members); v->members = NULL; }
    if (v->arr_vals) { free(v->arr_vals); v->arr_vals = NULL; }
    if (v->arr_strs) {
        for (int i = 0; i < v->arr_count; i++) if (v->arr_strs[i]) free(v->arr_strs[i]);
        free(v->arr_strs);
        v->arr_strs = NULL;
    }
    if (v->arr_members) {
        for (int i = 0; i < v->arr_count; i++) if (v->arr_members[i]) free_struct_members(v->arr_members[i]);
        free(v->arr_members);
        v->arr_members = NULL;
    }
    v->arr_count = src->arr_count;
    v->array_size = src->array_size;
    if (src->arr_vals && src->arr_count > 0) {
        v->arr_vals = (double *)calloc((size_t)src->arr_count, sizeof(double));
        if (v->arr_vals) memcpy(v->arr_vals, src->arr_vals, (size_t)src->arr_count * sizeof(double));
    }
    if (src->arr_strs && src->arr_count > 0) {
        v->arr_strs = (char **)calloc((size_t)src->arr_count, sizeof(char *));
        if (v->arr_strs) {
            for (int i = 0; i < src->arr_count; i++)
                v->arr_strs[i] = src->arr_strs[i] ? strdup(src->arr_strs[i]) : NULL;
        }
    }
    if (src->arr_members && src->arr_count > 0) {
        v->arr_members = (GclStructValue **)calloc((size_t)src->arr_count, sizeof(GclStructValue *));
        if (v->arr_members) {
            for (int i = 0; i < src->arr_count; i++)
                v->arr_members[i] = src->arr_members[i] ? clone_struct_members(src->arr_members[i]) : NULL;
        }
    }
    if (v->decl_type) { free(v->decl_type); v->decl_type = NULL; }
    if (src->decl_type) v->decl_type = strdup(src->decl_type);
}

/* TUR 25 — parametre kaydetme alani artik SABIT BOYUTLU DEGIL.
   Eskiden `Var *saved[32]` + uc paralel dizi vardi ve `saved_count < 32`
   ile sinirlaniyordu: 32'den fazla parametreli bir fonksiyonda 33. ve
   sonraki parametreler HIC kaydedilmiyor, dolayisiyla geri de
   yuklenmiyordu. Sonuc, TUR 23'te duzeltilen B35 ile AYNI sinifta bir
   hataydi (cagiranin degiskeni kalici olarak eziliyordu), yalnizca keyfi
   bir sinirin otesinde. Kaydetme alani artik `fd->param_count` kadar
   ayrilir; `saved_slot` dizisi her parametrenin hangi slota gittigini
   tutar, boylece geri yukleme ARAMA yapmaz (eski kod her parametre icin
   tum slotlari tarayip O(n^2) idi). */
typedef struct {
    Var   *var;        /* cagiranin Var'i (env_set_* onu YERINDE gunceller) */
    double num;        /* ezilmeden ONCEKI sayisal deger */
    int    is_string;  /* ezilmeden ONCEKI tur */
    char  *str;        /* strdup kopyasi — use-after-free onlemi */
} SavedParam;

static double call_user_func(Runner *r, FuncDef *fd, GclExpr **args,
                             int arg_count, GclSpan call_span) {
    /* B2: derinlik sınırı — aşılırsa çökme yerine kontrollü hata. */
    if (g_call_depth >= max_call_depth()) {
        runtime_errorcf(GCL_E_SEM_CALL_DEPTH, call_span,
                        "call depth limit exceeded (%d) in '%s' "
                        "(set the GCL_MAX_CALL_DEPTH environment variable to raise it)",
                        max_call_depth(), (fd && fd->name) ? fd->name : "?");
        return 0.0;
    }
    /* B26 — argüman sayısı denetimi. Eskiden yalnızca
       `min(param_count, arg_count)` kadar bağlanıyordu: EKSİK argümanda
       parametre env'de tanımsız kalıp daha sonra kafa karıştırıcı bir
       "undefined variable" üretiyor, FAZLA argüman ise tamamen sessizce
       düşüyordu (GCL_E_SEM_BAD_ARGC hiç üretilmiyordu).
       main() bu yoldan DEĞİL (exec_block ile, argv doğrudan) çağrılır. */
    if (fd && arg_count != fd->param_count) {
        runtime_errorcf(GCL_E_SEM_BAD_ARGC, call_span,
                        "'%s' expects %d argument(s), got %d",
                        fd->name ? fd->name : "?", fd->param_count, arg_count);
        return 0.0;
    }
    g_call_depth++;
    /* Fonksiyon çağrısından ÖNCE var listesinin başını kaydet —
       içeride yeni oluşturulan local (global olmayan) değişkenleri
       fonksiyon çıkışında temizlemek için. */
    Var *orig_head = r->env->vars;
    /* Parametreleri env'e bağla — geçici: eski değerleri sakla ve geri yükle.
       env_set_num, mevcut string değişkenin str alanını free ettiği için
       restore'da dangling pointer okumamak adına str kopyasını sakla. */
    /* TUR 23 — ONCEKI DEGERIN KENDISI de saklanmali. Eskiden yalnizca
       POINTER (`saved[j]`) ve string kopyasi tutuluyordu; `v->num` zaten
       `env_set_num` ile EZILMIS oldugu icin geri yukleme dongusundeki
       `v->num = saved[j]->num` bir KENDINE ATAMA idi ve cagiranin degeri
       kalici olarak bozuluyordu:
           int x = 99;  int h(int x) { return x; }  h(7);
           printf("{}", x);   // 99 olmali, eskiden 7 yaziyordu
       C'de parametre DEGER ile gecer; cagiranin degiskeni degismemelidir.
       TUR 25 — alan artik `fd->param_count` kadar ayrilir (eskiden 32 sabit). */
    int save_cap = fd->param_count > 0 ? fd->param_count : 1;
    SavedParam *saved = (SavedParam *)calloc((size_t)save_cap, sizeof(SavedParam));
    int *saved_slot = (int *)malloc((size_t)save_cap * sizeof(int));
    if (!saved || !saved_slot) {
        free(saved);
        free(saved_slot);
        g_call_depth--;
        runtime_errorf(call_span, "out of memory while saving the parameters of '%s'",
                       fd->name ? fd->name : "?");
        return 0.0;
    }
    for (int i = 0; i < save_cap; i++) saved_slot[i] = -1;
    int saved_count = 0;
    /* Onceki cagridan kalan metin sonucunu temizle: argumanlar
       degerlendirilirken return_str tazelenmelidir (bayat deger okunmasin). */
    if (r->return_str) { free(r->return_str); r->return_str = NULL; }
    for (int i = 0; i < fd->param_count && i < arg_count; i++) {
        const char *pname = fd->params[i].name;
        Var *existing = env_find(r->env, pname);
        if (existing) {
            /* Kaydetme alani `fd->param_count` kadar ve bu dongu de
               param_count ile sinirli — tasma artik YAPISAL olarak
               imkansiz (eskiden 32. parametreden sonrasi sessizce
               kaydedilmiyordu). */
            saved_slot[i] = saved_count;
            saved[saved_count].var = existing;
            saved[saved_count].num = existing->num;   /* degerin KENDISI */
            saved[saved_count].is_string = existing->is_string;
            saved[saved_count].str = existing->str ? strdup(existing->str) : NULL;
            saved_count++;
        }
        /* String argümanlar env_set_num ile bağlanırsa AST_EXPR_STRING 0.0
           döndürür ve string değer kaybolur. String ise env_set_str kullan. */
        GclExpr *arg = args[i];
        if (arg && arg->kind == AST_EXPR_STRING) {
            env_set_str(r->env, pname, arg->str ? arg->str : "");
        } else if (arg && arg->kind == AST_EXPR_VAR) {
            Var *av = env_find(r->env, arg->name);
            if (av && av->members) {
                /* B14: struct argümanı — üyeleri kopyala (değer semantiği). */
                env_set_members_copy(r->env, pname, av);
            } else if (av && (av->arr_vals || av->arr_strs || av->arr_members)) {
                /* B6: dizi argümanı — elemanları kopyala. */
                env_set_array_copy(r->env, pname, av);
            } else if (av && av->is_string) {
                env_set_str(r->env, pname, av->str ? av->str : "");
            } else {
                double av_num = eval_expr(arg, r);
                /* Arguman bir CAGRI ise ve metin dondurduyse metni bagla:
                   `f(g())` icinde g()'nin metni kaybolmasin. */
                if (r->return_str) {
                    env_set_str(r->env, pname, r->return_str);
                    free(r->return_str);
                    r->return_str = NULL;
                } else {
                    env_set_num(r->env, pname, av_num);
                }
            }
        } else {
            double av_num = arg ? eval_expr(arg, r) : 0.0;
            if (r->return_str) {
                env_set_str(r->env, pname, r->return_str);
                free(r->return_str);
                r->return_str = NULL;
            } else {
                env_set_num(r->env, pname, av_num);
            }
        }
        /* Parametre TIPINI isaretle: `void show(char x)` icinde `x`'in char
           oldugunu runner ancak boylece bilir. Eskiden tip dusuyordu ve
           `printf("{}", x)` karakter yerine sayisal kodu (81) yaziyordu.
           DIKKAT: yalnizca fonksiyona OZEL (cagiranda olmayan) parametre icin.
           Cagiranin kendi degiskeni varsa onun tipi korunur — geri yukleme
           dongusu `decl_type`i geri koymaz, oraya yazmak cagiranin tipini
           kalici olarak bozardi. */
        if (!existing && i < fd->param_count && fd->params[i].type) {
            Var *pv = env_find(r->env, pname);
            if (pv) {
                if (pv->decl_type) free(pv->decl_type);
                pv->decl_type = strdup(fd->params[i].type);
            }
        }
    }
    Runner sub;
    memcpy(&sub, r, sizeof(sub));
    sub.return_flag = 0;
    sub.return_value = 0;
    sub.break_flag = 0;
    sub.continue_flag = 0;
    /* B31: cagrilan fonksiyon dongu/switch baglamini DEVRALMAZ. */
    sub.loop_depth = 0;
    sub.breakable_depth = 0;
    sub.return_struct = NULL;
    sub.return_init_list = NULL;
    sub.return_str = NULL;
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
    /* Metin sonucu cagirana tasi (cagiranda bayat deger kalmasin). */
    if (r->return_str) { free(r->return_str); r->return_str = NULL; }
    if (sub.return_str) {
        r->return_str = strdup(sub.return_str);
        free(sub.return_str);
        sub.return_str = NULL;
    }
    /* eski değerleri geri yükle — str kopyası saved_str'den (use-after-free önlemi).
       TUR 25: slot indeksi ile dogrudan; eskiden her parametre icin tum
       slotlar taranirdi ve 32'den sonrasi hic bulunamazdi. */
    for (int i = 0; i < fd->param_count; i++) {
        int slot = saved_slot[i];
        if (slot < 0) continue;              /* bu parametre cagiranda yoktu */
        Var *v = env_find(r->env, fd->params[i].name);
        if (!v) continue;
        /* Kayitli DEGER — `saved[slot].var->num` artik ezilmis degerdir. */
        v->num = saved[slot].num;
        v->is_string = saved[slot].is_string;
        if (v->str) free(v->str);
        v->str = saved[slot].str ? strdup(saved[slot].str) : NULL;
    }
    for (int i = 0; i < saved_count; i++) free(saved[i].str);
    free(saved);
    free(saved_slot);
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
                /* B6: parametreye kopyalanan dizi elemanları da serbest
                   bırakılmalı — aksi hâlde her çağrıda sızıntı olur. */
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
            } else {
                pp = &v->next;
            }
        }
    }
    g_call_depth--;
    return ret;
}

/* B7 — bir ifade STRING değeri üretiyor mu? String literali veya string
   DEĞİŞKENİ ise metni döndürür; değilse NULL. `"abc" == "xyz"` eskiden iki
   taraf da sayısal 0'a indiği için DAİMA 1 (true) veriyordu. */
static const char *expr_string_value(GclExpr *e, Runner *r) {
    if (!e) return NULL;
    if (e->kind == AST_EXPR_STRING) return e->str ? e->str : "";
    if (e->kind == AST_EXPR_VAR) {
        Var *v = env_find(r->env, e->name);
        if (v && v->is_string && v->str) return v->str;
    }
    return NULL;
}

static double eval_expr(GclExpr *e, Runner *r) {
    if (!e) return 0.0;
    switch (e->kind) {
        case AST_EXPR_FLOAT: return e->num;
        /* TUR 22 — `'A'` bir `char` DEGERIDIR; sayisal baglamda degeri KODdur
           (65), tipki `int m = c;` gibi. printf'in `{}` yer tutucusunun onu
           KARAKTER basmasi "tek kural"in parcasi; bkz. expr_is_char_value(). */
        case AST_EXPR_STRING: return 0.0;
        case AST_EXPR_VAR: {
            Var *v = env_find(r->env, e->name);
            if (!v) {
        runtime_errorcf(GCL_E_SEM_UNKNOWN_VAR, span_of_expr(e),
                        "undefined variable '%s'", e->name);
                return 0.0;
            }
            return env_get_num(r->env, e->name);
        }
        /* B9 — `i++` ESKİ değeri, `++i` YENİ değeri verir (C). Yeni değer,
           mevcut atama yolları (VAR / MEMBER / ARRAY lvalue) kullanılarak
           yazılır; böylece struct alanı ve dizi elemanı için de çalışır. */
        case AST_EXPR_POSTINC:
        case AST_EXPR_PREINC: {
            double old_value = eval_expr(e->left, r);
            double new_value = (e->op == OP_ADD) ? old_value + 1.0 : old_value - 1.0;
            GclExpr literal;
            memset(&literal, 0, sizeof(literal));
            literal.kind = AST_EXPR_FLOAT;
            literal.num  = new_value;
            GclExpr store;
            memset(&store, 0, sizeof(store));
            store.kind  = AST_EXPR_ASSIGN;
            store.left  = e->left;      /* lvalue: değişken / alan / dizi elemanı */
            store.right = &literal;
            eval_expr(&store, r);       /* yan etki: lvalue = yeni değer */
            return (e->kind == AST_EXPR_POSTINC) ? old_value : new_value;
        }
        /* B11 — C-tipi cast: `(int)7.5`. Operand değerlendirilir, sonra hedef
           tipin aralığına kırpılır (truncate_to_declared_type). Eskiden bu
           sözdizimi hiç desteklenmiyordu → "expected ';'". */
        case AST_EXPR_CAST: {
            double v = eval_expr(e->left, r);
            if (!e->str || !e->str[0]) return v;
            return truncate_to_declared_type(v, e->str);
        }
        case AST_EXPR_BINOP: {
            double l = eval_expr(e->left, r);
            double rr = eval_expr(e->right, r);
            switch (e->op) {
                case OP_ADD: return l + rr;
                case OP_SUB: return l - rr;
                case OP_MUL: return l * rr;
                /* B13 — sıfıra bölme/modül eskiden TAMAMEN SESSİZ 0.0 dönüyordu.
                   Artık C semantiğine uygun sonuç (+/-inf, nan) ve bir tanılama
                   (GCL_E_SEM_DIV_ZERO = GCL3008) üretilir. */
                case OP_DIV:
                    if (rr == 0.0) {
                        runtime_errorcf(GCL_E_SEM_DIV_ZERO, span_of_expr(e),
                                        "division by zero (GCL3008)");
                        if (l == 0.0) return NAN;              /* 0/0 */
                        return (l > 0.0) ? HUGE_VAL : -HUGE_VAL;
                    }
                    return l / rr;
                case OP_MOD:
                    if (rr == 0.0) {
                        runtime_errorcf(GCL_E_SEM_DIV_ZERO, span_of_expr(e),
                                        "modulo by zero (GCL3008)");
                        return NAN;
                    }
                    return fmod(l, rr);
                /* B7: operandlardan biri STRING ise metin karşılaştırması yap. */
                case OP_EQ: case OP_NE: {
                    const char *ls = expr_string_value(e->left, r);
                    const char *rs = expr_string_value(e->right, r);
                    if (ls || rs) {
                        int eq = (ls && rs) ? (strcmp(ls, rs) == 0) : 0;
                        return (double)(e->op == OP_EQ ? eq : !eq);
                    }
                    return (double)(e->op == OP_EQ ? (l == rr) : (l != rr));
                }
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
                    /* Hata GLOBAL sayaca gider (bkz. `gcl_runtime_errors` ve
                       Runner'in `memcpy` kopyasi notu): bu `r` bir KOPYA
                       olabilir ve kopyaya yazilan bir bayrak cagirana
                       DONMEZ — bu yuzden Runner'da boyle bir alan TUTULMAZ.
                       `return` yine de sart: const'un degeri KORUNMALI
                       (errors/try_catch_kinds.gcsf). */
                    runtime_errorcf(GCL_E_SEM_ASSIGN_CONST, span_of_expr(e->left),
                                    "cannot assign to const '%s'", e->left->name);
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
                    if (arr && idx < 0) {
                        /* B12 — negatif index eskiden tamamen sessizdi. */
                        runtime_errorcf(GCL_E_SEM_OUT_OF_BOUNDS, span_of_expr(arr_expr),
                                        "negative array index %d for '%s' [GCL3010]",
                                        idx, arr_base->name);
                    } else if (arr && idx >= 0) {
                        int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                        if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                        /* B12 — aralik disi YAZMA eskiden SESSIZCE ATILIYORDU
                           (ne hata ne uyari vardi). Artik tanilama uretilir. */
                        if (bounds > 0 && idx >= bounds) {
                            runtime_errorcf(GCL_E_SEM_OUT_OF_BOUNDS, span_of_expr(arr_expr),
                                            "array index %d out of bounds for '%s' (size %d) [GCL3010]",
                                            idx, arr_base->name, bounds);
                        } else if (arr->arr_vals && idx < arr->arr_count) {
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
            } else if (e->left) {
                /* B32 — atanamayan hedef (rvalue): `5 = 3`, `f(x) = 1`, `a + b = 2`.
                   Eskiden bu ifade TAMAMEN SESSIZDI: sag taraf hesaplaniyor, atama
                   yapilmiyor, hicbir hata da verilmiyordu (exit 0) — kullanici
                   yazim hatasini asla goremiyordu. */
                const char *what = "an expression";
                switch (e->left->kind) {
                    case AST_EXPR_FLOAT:
                    case AST_EXPR_INT:    what = "a number literal"; break;
                    case AST_EXPR_STRING: what = "a string literal"; break;
                    case AST_EXPR_CALL:   what = "a function call"; break;
                    case AST_EXPR_BINOP:  what = "an arithmetic expression"; break;
                    case AST_EXPR_CAST:   what = "a cast expression"; break;
                    case AST_EXPR_UNOP:   what = "a unary expression"; break;
                    default: break;
                }
                runtime_errorf_hint(span_of_expr(e->left),
                                    "the left side of '=' must be a variable, "
                                    "a struct member or an array element",
                                    "cannot assign to %s", what);
            }
            return v;
        }
        case AST_EXPR_CALL: {
            GclExpr *callee = e->left;
            if (callee && callee->kind == AST_EXPR_VAR) {
                const char *fn = callee->name;
                /* Bare printf("{}", ...) — simple_doc.md giriş dilinde Stdio. öneki yok */
                if (strcmp(fn, "printf") == 0) {
                    return call_native_member(span_of_expr(e), "Stdio", "printf",
                                              e->args, e->arg_count, r);
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
                            /* ONE table, defined beside
                               truncate_to_declared_type, so `sizeof(T)` cannot
                               disagree with what a `T` declaration actually
                               stores (TURN 43). This list used to live here and
                               had drifted in two places: the three "128-bit"
                               names claimed 16 bytes while the interpreter
                               stores 8, and `float16` was missing altogether so
                               it fell through to the 8-byte double default. */
                            int sz = decl_type_size(tn);
                            if (sz > 0) return (double)sz;
                        }
                    }
                    return 8.0; /* double varsayımı */
                }
                /* kullanıcı fonksiyonu */
                FuncDef *fd = find_func(r->env, fn);
                if (fd) {
                    return call_user_func(r, fd, e->args, e->arg_count, span_of_expr(e));
                }
                /* #extern + #register ile kayıtlı C fonksiyonu */
                return call_extern_func(r->env, span_of_expr(e), fn, e->args, e->arg_count, r);
            }
            /* modül member çağrısı: Math.fn(...), Stdio.fn(...), Embed.fn(...) */
            if (callee && callee->kind == AST_EXPR_MEMBER) {
                GclExpr *base = callee->left;
                if (base && base->kind == AST_EXPR_VAR) {
                    /* Modul sahipli struct uzerinde metot cagrisi (PLAYER.Move(),
                       Terrain.Draw()): once LastSlot tasiyici modul denenir.
                       Oyle bir tip degilse normal modul yolu isler. */
                    int handled = 0;
                    double method_result = call_native_struct_method(span_of_expr(e), base->name,
                                                                     callee->member_name, r,
                                                                     &handled);
                    if (handled) return method_result;
                    /* #lib/.gclib feature removed — always dispatch to native modules */
                    double call_result = call_native_member(span_of_expr(e), base->name,
                                                            callee->member_name, e->args,
                                                            e->arg_count, r);
                    /* Struct-var argümanlar yerinde güncellenir:
                       RaylibSimpleCollision.TerrainCollision(PLAYER, Terrain)
                       PLAYER'in konumunu yazar. */
                    write_back_struct_args(r, base->name, e->args, e->arg_count);
                    return call_result;
                }
            }
            return 0.0;
        }
        case AST_EXPR_MEMBER: {
            /* Native modül sabiti: Raylib.RED, Raylib.RAYWHITE gibi (parantezsiz) */
            if (e->left && e->left->kind == AST_EXPR_VAR && native_find(r->env, e->left->name)) {
                return call_native_member(span_of_expr(e), e->left->name,
                                          e->member_name, NULL, 0, r);
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
                if (arr && idx < 0) {
                    /* B12 — negatif index sessizdi (0.0 donerdi). */
                    runtime_errorcf(GCL_E_SEM_OUT_OF_BOUNDS, span_of_expr(e),
                                    "negative array index %d for '%s' [GCL3010]",
                                    idx, e->left->name);
                } else if (arr && idx >= 0) {
                    int bounds = arr->arr_count > 0 ? arr->arr_count : arr->array_size;
                    if (bounds <= 0) bounds = (int)(arr->str ? strlen(arr->str) : 0);
                    /* B12 — aralik disi OKUMA eskiden tamamen sessizdi (0.0).
                       GCL_E_SEM_OUT_OF_BOUNDS tanilamasi uretilsin. */
                    if (bounds > 0 && idx >= bounds) {
                        runtime_errorcf(GCL_E_SEM_OUT_OF_BOUNDS, span_of_expr(e),
                                        "array index %d out of bounds for '%s' (size %d) [GCL3010]",
                                        idx, e->left->name, bounds);
                        return 0.0;
                    }
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

/* ---------- Slot -> struct degiskeni koprusu ----------------------------

   raylib'de struct donduren cagrilar (GetMousePosition, GetWindowScaleDPI,
   MeasureTextEx, GetWorldToScreen, ...) sonucu modulun g_last_* slotuna yazar
   ve 0.0 dondurur: modul ABI'si tek bir double tasir. Modul bu degerleri
   LastV2X, LastRectW, LastCamPosX gibi uyelerle parca parca yayinlar.

   Eskiden `Raylib.Vector2 m = Raylib.GetMousePosition();` SESSIZCE (0,0)
   veriyordu: bildirim baslaticisi bir cagri oldugunda yorumlayici yalnizca
   kurucu bicimini (`Raylib.Vector2(a,b)`) biliyordu; slotu degiskene
   kopyalayan hicbir yol yoktu (hata yok, yanlis deger var). Asagidaki tablo
   her native struct tipinin DUZLESMIS skaler alanlarini sirayla okuyan
   Last* uye adlarini tutar; modul cagrisindan sonra bunlar modulden cagrilip
   degiskenin alanlarina yazilir. Sira, flatten_struct_members ile AYNIDIR. */

typedef struct {
    const char *type;
    const char *const *accessors;
    int count;
} NativeLastMap;

static const char *const last_vec2[]  = {"LastV2X","LastV2Y"};
static const char *const last_vec3[]  = {"LastV3X","LastV3Y","LastV3Z"};
static const char *const last_vec4[]  = {"LastV4X","LastV4Y","LastV4Z","LastV4W"};
static const char *const last_rect[]  = {"LastRectX","LastRectY","LastRectW","LastRectH"};
static const char *const last_cam[]   = {
    "LastCamPosX","LastCamPosY","LastCamPosZ",
    "LastCamTargetX","LastCamTargetY","LastCamTargetZ",
    "LastCamUpX","LastCamUpY","LastCamUpZ",
    "LastCamFovy","LastCamProjection"
};
static const char *const last_cam2d[] = {
    "LastCam2DOffsetX","LastCam2DOffsetY",
    "LastCam2DTargetX","LastCam2DTargetY",
    "LastCam2DRotation","LastCam2DZoom"
};
static const char *const last_ray[]   = {
    "LastRayX","LastRayY","LastRayZ","LastRayDirX","LastRayDirY","LastRayDirZ"
};
static const char *const last_raycol[] = {
    "LastRayColHit","LastRayColDistance",
    "LastRayColPX","LastRayColPY","LastRayColPZ",
    "LastRayColNX","LastRayColNY","LastRayColNZ"
};
static const char *const last_bbox[]  = {
    "LastBoxMinX","LastBoxMinY","LastBoxMinZ","LastBoxMaxX","LastBoxMaxY","LastBoxMaxZ"
};
static const char *const last_quat[]  = {"LastQuatX","LastQuatY","LastQuatZ","LastQuatW"};
static const char *const last_xform[] = {
    "LastTransformTX","LastTransformTY","LastTransformTZ",
    "LastTransformRX","LastTransformRY","LastTransformRZ",
    "LastTransformSX","LastTransformSY","LastTransformSZ"
};
static const char *const last_npatch[] = {
    "LastNPatchX","LastNPatchY","LastNPatchW","LastNPatchH",
    "LastNPatchLeft","LastNPatchTop","LastNPatchRight","LastNPatchBottom","LastNPatchLayout"
};
static const char *const last_glyph[] = {
    "LastGlyphValue","LastGlyphOffsetX","LastGlyphOffsetY","LastGlyphAdvanceX",
    "LastGlyphImgWidth","LastGlyphImgHeight"
};
static const char *const last_aevent[] = {
    "LastAEventFrame","LastAEventType","LastAEventP0","LastAEventP1","LastAEventP2","LastAEventP3"
};
/* Modul sahipli tipler (RaylibShader/RaylibSimpleLight). Bunlarin degerleri
   raylib'in g_last_* yuvalarinda degil, modulun kendi durumundadir; bu yuzden
   erisimciler raylib'in Last* adlari yerine modulun KENDI uye adlaridir
   (RaylibShader: Handle; RaylibSimpleLight: Handle + RotX/RotY/RotZ).
   Sira, gcl_native_types.c'deki alan listesinin SKALER YAPRAK sirasidir. */
static const char *const last_simpleshader[] = { "Handle" };
static const char *const last_sunlight[] = { "Handle", "RotX", "RotY", "RotZ" };
/* RaylibFOG.FOG: mesafe sisi. Raylib'in Last* adlari DEGIL, modulun KENDI
   erisimci adlari kullanilir — degerler g_last_* yuvalarinda degil modulun
   kendi durumundadir (bkz. Modules/gcl_fog.c). Sira,
   gcl_native_types.c'deki `f_fog` alan listesinin SKALER YAPRAK sirasidir ve
   sayi 12'dir; ucu de (tip tablosu, bu tablo, slot map) AYNI olmak zorundadir. */
static const char *const last_fog[] = {
    "Handle", "Color", "Density", "Start", "End", "Mode",
    "Enabled", "Height", "Falloff", "Alpha", "Noise", "HeightFog"
};
/* RaylibSKYBOX.Skybox: ayni sekilde MODUL SAHIPLI bir tip. Erisimci adlari
   modulun KENDI uye adlaridir (bkz. Modules/gcl_skybox.c) ve sira, tipin
   gcl_native_types.c'deki skaler yaprak sirasidir. */
static const char *const last_skybox[] = {
    "Handle", "DailyCycle", "Time", "DayLength",
    "CloudAmount", "CloudSpeed", "StarAmount", "SunSize", "SunBrightness",
    "CloudON"
};
/* RaylibSimpleWater.Water: modul sahipli tip. Alanlarin SKALER YAPRAK sirasi
   gcl_native_types.c'deki `f_water` ile AYNI olmak zorundadir: Color, Handle,
   sonra Position/Rotate/Scale'in her biri AYRI x/y/z yapragi olarak acilir
   (3x3 = 9 yaprak), ardindan su parametreleri. Ic ice alanlarin adlari
   NOKTASIZDIR ("PosX", "Rotate.x" degil) â€” bkz. last_sunlight[] yukarida.
   Toplam 19 yaprak; sayac g_native_slot_maps[] ile de ayni olmalidir.

   Bu tip YALNIZCA SUYU TANIMLAR: yuzme esigi, kapsul olcusu ya da "yuzuyor
   sayilma" karari BURADA YASAMAZ. Boyle bir esik tutmak, ekranin "yuzuyorum"
   dedigi an ile fizigin yuzdugu ani birbirinden ayirirdi; karar tek bir
   yerde, oyuncunun kendi modulunde verilir (bkz. Modules/gcl_raylib_fps.c).
   Modulun WATER_SLOT_COUNT'u ve gcl_native_types.c'deki f_water[] ile
   BIREBIR ayni olmak zorundadir. */
static const char *const last_water[] = {
    "Color", "Handle",
    "PosX", "PosY", "PosZ",
    "RotX", "RotY", "RotZ",
    "ScaleX", "ScaleY", "ScaleZ",
    "Alpha", "Reflection", "Refraction", "Fresnel",
    "WaveStrength", "WaveSpeed", "WaveScale", "Foam"
};

static const NativeLastMap g_native_last_maps[] = {
    { "Vector2",         last_vec2,   (int)(sizeof(last_vec2)   / sizeof(last_vec2[0]))   },
    { "Vector3",         last_vec3,   (int)(sizeof(last_vec3)   / sizeof(last_vec3[0]))   },
    { "Vector4",         last_vec4,   (int)(sizeof(last_vec4)   / sizeof(last_vec4[0]))   },
    { "Rectangle",       last_rect,   (int)(sizeof(last_rect)   / sizeof(last_rect[0]))   },
    { "Camera",          last_cam,    (int)(sizeof(last_cam)    / sizeof(last_cam[0]))    },
    { "Camera3D",        last_cam,    (int)(sizeof(last_cam)    / sizeof(last_cam[0]))    },
    { "Camera2D",        last_cam2d,  (int)(sizeof(last_cam2d)  / sizeof(last_cam2d[0]))  },
    { "Ray",             last_ray,    (int)(sizeof(last_ray)    / sizeof(last_ray[0]))    },
    { "RayCollision",    last_raycol, (int)(sizeof(last_raycol) / sizeof(last_raycol[0])) },
    { "BoundingBox",     last_bbox,   (int)(sizeof(last_bbox)   / sizeof(last_bbox[0]))   },
    { "Quaternion",      last_quat,   (int)(sizeof(last_quat)   / sizeof(last_quat[0]))   },
    { "Transform",       last_xform,  (int)(sizeof(last_xform)  / sizeof(last_xform[0]))  },
    { "NPatchInfo",      last_npatch, (int)(sizeof(last_npatch) / sizeof(last_npatch[0])) },
    { "GlyphInfo",       last_glyph,  (int)(sizeof(last_glyph)  / sizeof(last_glyph[0]))  },
    { "AutomationEvent", last_aevent, (int)(sizeof(last_aevent) / sizeof(last_aevent[0])) },
    { "SimpleShader",    last_simpleshader, (int)(sizeof(last_simpleshader) / sizeof(last_simpleshader[0])) },
    { "SunLight",        last_sunlight,     (int)(sizeof(last_sunlight)     / sizeof(last_sunlight[0]))     },
    { "Skybox",          last_skybox,       (int)(sizeof(last_skybox)       / sizeof(last_skybox[0]))       },
    { "Water",           last_water,        (int)(sizeof(last_water)        / sizeof(last_water[0]))        },
    { "FOG",             last_fog,          (int)(sizeof(last_fog)          / sizeof(last_fog[0]))          },
};

#define GCL_NATIVE_LAST_MAP_COUNT \
    ((int)(sizeof(g_native_last_maps) / sizeof(g_native_last_maps[0])))

static const NativeLastMap *native_last_map(const char *type) {
    if (!type || !type[0]) return NULL;
    for (int i = 0; i < GCL_NATIVE_LAST_MAP_COUNT; i++)
        if (strcmp(g_native_last_maps[i].type, type) == 0) return &g_native_last_maps[i];
    return NULL;
}

/* Struct uye listesindeki skaler yapraklari sirayla topla (ic ice rekursif). */
static void collect_scalar_leaves(GclStructValue *m, GclStructValue **out, int *n, int cap) {
    for (; m && *n < cap; m = m->next) {
        if (m->members) collect_scalar_leaves(m->members, out, n, cap);
        else out[(*n)++] = m;
    }
}

/* Bir modul uyesini sabit cagri olarak cagir (Last* erisimcileri gibi). */
static double native_call_named(GclEnv *env, const char *module, const char *member, double dflt) {
    NativeModule *mod = native_find(env, module);
    if (!mod) mod = native_load(env, module);
    if (!mod || !member) return dflt;
    for (int i = 0; i < mod->entry_count; i++)
        if (strcmp(mod->entries[i].name, member) == 0) return mod->entries[i].fn(0, NULL);
    return dflt;
}

/* Slot dolduran bir native cagridan sonra degiskenin alanlarini doldur.
   Tip icin tablo yoksa hicbir sey yapmaz (0 doner). */
static int fill_native_from_last_slot(Runner *r, const char *var_name,
                                      const char *module, const GclNativeStruct *ns) {
    if (!r || !var_name || !module || !ns || !ns->type) return 0;
    const NativeLastMap *map = native_last_map(ns->type);
    if (!map) return 0;
    Var *v = env_find(r->env, var_name);
    if (!v || !v->members) return 0;
    GclStructValue *leaves[256];
    int n = 0;
    collect_scalar_leaves(v->members, leaves, &n, 256);
    if (n != map->count) return 0;
    for (int i = 0; i < n; i++) {
        double val = native_call_named(r->env, module, map->accessors[i], leaves[i]->num);
        leaves[i]->num = truncate_to_declared_type(val, leaves[i]->decl_type);
        leaves[i]->is_string = 0;
    }
    return 1;
}

/* ---------- Modul sahipli struct tipleri ----------

   RaylibFPS.FPS ve RaylibSimpleMesh.Terrain tiplerinin alanlari raylib'in
   g_last_* yuvalarinda DEGIL, modulun kendi LastSlot kanalindadir: alan
   sirasi i icin `LastSlot(i)` cagrilir. Eslesme (tip, modul) ciftiyle
   yapilir, cunku ayni PLAYER degiskenini iki ayri modul guncelleyebilir
   (RaylibFPS.Move/Look ve RaylibSimpleCollision.TerrainCollision). */
static const GclNativeSlotMap g_native_slot_maps[] = {
    { "FPS",     "RaylibFPS",             27 },
    { "FPS",     "RaylibSimpleCollision", 27 },
    { "Terrain", "RaylibSimpleMesh",       3 },
    /* RaylibSimpleMesh.Mesh, registered under its QUALIFIED name in
       SharedPipeline/gcl_native_types.c because the bare "Mesh" there already
       means raylib's raw mesh struct. The key must be spelled EXACTLY as
       gcl_native_structs[] spells it, because lookup_native_struct() returns
       that struct and native_slot_map_for_type() is asked with its `type`:
       while this entry was missing, `CUBE.Draw()` found no slot map, fell
       through to module dispatch and reported the misleading
       "unknown module 'CUBE'". The count is 12 and NOT Terrain's 3: a Mesh
       also carries Position/Rotate/Scale (slots 3..11) so it can be placed in
       the world, and the number must equal the scalar leaves the type declares
       in SharedPipeline/gcl_native_types.c -- call_native_struct_method()
       refuses the call outright when the two disagree, which is the other half
       of this same failure mode. Both types are LoadObj() registry handles
       drawn by the same fn_draw() in Modules/gcl_SimpleMesh.c. */
    { "RaylibSimpleMesh.Mesh", "RaylibSimpleMesh", 12 },
    { "SunLight",    "RaylibSimpleLight",  4 },
    { "SimpleShader", "RaylibShader",      1 },
    /* RaylibSKYBOX.Skybox: 9 skaler yaprak (Handle, DailyCycle, Time, DayLength,
       CloudAmount, CloudSpeed, StarAmount, SunSize, SunBrightness) — sayi
       gcl_native_types.c'deki f_skybox[] ile AYNI olmak zorundadir, yoksa
       call_native_struct_method() cagriyi tumden reddeder ve `sky.Draw()`
       "unknown module 'sky'" diye raporlanir. */
    { "Skybox",      "RaylibSKYBOX",       10 },
    /* RaylibFOG.FOG: mesafe sisi, 12 skaler yaprak (yukaridaki last_fog ile
       ve gcl_native_types.c'deki f_fog ile AYNI sirada). */
    { "FOG",         "RaylibFOG",          12 },
    /* RaylibSimpleWater.Water: 19 skaler yaprak. Sayi gcl_native_types.c'deki
       f_water[] ve yukaridaki last_water[] ile AYNI olmak zorundadir; yoksa
       call_native_struct_method() cagriyi tumden reddeder ve `water.Draw()`
       "unknown module 'water'" diye raporlanir. Su cizimi ONCE bu modul
       uzerinden yapilir, ardindan KENDI shader'i materyale konur. */
    { "Water",       "RaylibSimpleWater",  19 },
};

#define GCL_NATIVE_SLOT_MAP_COUNT \
    ((int)(sizeof(g_native_slot_maps) / sizeof(g_native_slot_maps[0])))

static const GclNativeSlotMap *native_slot_map_exact(const char *type, const char *module) {
    if (!type || !module) return NULL;
    for (int i = 0; i < GCL_NATIVE_SLOT_MAP_COUNT; i++)
        if (strcmp(g_native_slot_maps[i].type, type) == 0 &&
            strcmp(g_native_slot_maps[i].module, module) == 0)
            return &g_native_slot_maps[i];
    return NULL;
}

/* Tipin sahibi olan modul: PLAYER.Move() hangi modulu cagiracak?

   Arama, lookup_native_struct() ile AYNI kurali izler: once tam ad, sonra
   noktadan sonraki yaprak. Iki arama ayni tip adi icin farkli cevap verdiginde
   tam olarak bu hata doguyordu: gcl_native_types.c tipi NITELIKLI adiyla
   ("RaylibSimpleMesh.Mesh") kaydeder, tablo ise yalnizca "Terrain" tasiyordu;
   `CUBE.Draw()` slot haritasini bulamayinca modul yoluna dusup
   "unknown module 'CUBE'" diyordu. Simdi iki arama ayrismaaz. */
static const GclNativeSlotMap *native_slot_map_for_type(const char *type) {
    const char *leaf;
    if (!type) return NULL;
    for (int i = 0; i < GCL_NATIVE_SLOT_MAP_COUNT; i++)
        if (strcmp(g_native_slot_maps[i].type, type) == 0) return &g_native_slot_maps[i];
    leaf = strrchr(type, '.');
    if (!leaf) return NULL;
    leaf++;
    for (int i = 0; i < GCL_NATIVE_SLOT_MAP_COUNT; i++)
        if (strcmp(g_native_slot_maps[i].type, leaf) == 0) return &g_native_slot_maps[i];
    return NULL;
}

/* Modul uyesini tek bir metin argümaniyla cagir (LastSlot(i) gibi). */
static double native_call_one(GclEnv *env, const char *module, const char *member,
                              const char *arg) {
    NativeModule *mod = native_find(env, module);
    const char *argv[1];
    if (!mod) mod = native_load(env, module);
    if (!mod || !member) return 0.0;
    argv[0] = arg ? arg : "";
    for (int i = 0; i < mod->entry_count; i++)
        if (strcmp(mod->entries[i].name, member) == 0)
            return mod->entries[i].fn(arg ? 1 : 0, argv);
    return 0.0;
}

/* Bir native struct degiskeninin skaler yapraklarina (bildirim sirasinda)
   toplanmis degerleri dondurur. Bos ise 0. */
static int collect_var_leaves(GclEnv *env, const char *var_name,
                              GclStructValue **leaves, int cap) {
    Var *v = env_find(env, var_name);
    int n = 0;
    if (!v || !v->members) return 0;
    collect_scalar_leaves(v->members, leaves, &n, cap);
    return n;
}

/* Modulun LastSlot kanalini degiskene geri yaz. */
static int store_slots_into_var(GclEnv *env, const char *var_name,
                                const char *module, int count) {
    GclStructValue *leaves[128];
    int n = collect_var_leaves(env, var_name, leaves, 128);
    if (n <= 0 || n != count) return 0;
    for (int i = 0; i < n; i++) {
        char index[16];
        snprintf(index, sizeof(index), "%d", i);
        leaves[i]->num = truncate_to_declared_type(
            native_call_one(env, module, "LastSlot", index), leaves[i]->decl_type);
        leaves[i]->is_string = 0;
    }
    return 1;
}

/* Native struct degiskeni uzerinde metot cagrisi (PLAYER.Move()).

   Degiskenin skaler yapraklari bildirim sirasinda argüman olarak verilir;
   cagri dondugunde ayni sira modulun LastSlot kanalindan geri yazilir.
   Tasiyici bir modul/tip degilse `handled` 0 kalir ve cagiran normal modul
   yolunu dener. */
static double call_native_struct_method(GclSpan span, const char *var_name,
                                        const char *member, Runner *r, int *handled) {
    GclExpr        nodes[128];
    GclExpr       *ptrs[128];
    GclStructValue *leaves[128];
    const GclNativeStruct *ns;
    const GclNativeSlotMap *map;
    Var *v = env_find(r->env, var_name);
    int n;

    if (handled) *handled = 0;
    if (!v || !v->decl_type) return 0.0;
    ns = lookup_native_struct(v->decl_type);
    if (!ns) return 0.0;
    map = native_slot_map_for_type(ns->type);
    if (!map) return 0.0;

    n = collect_var_leaves(r->env, var_name, leaves, 128);
    if (n <= 0 || n != map->count) return 0.0;

    for (int i = 0; i < n; i++) {
        memset(&nodes[i], 0, sizeof(nodes[i]));
        if (leaves[i]->is_string && leaves[i]->str) {
            nodes[i].kind = AST_EXPR_STRING;
            nodes[i].str  = leaves[i]->str;
        } else {
            nodes[i].kind = AST_EXPR_FLOAT;
            nodes[i].num  = leaves[i]->num;
        }
        ptrs[i] = &nodes[i];
    }

    double result = call_native_member(span, map->module, member, ptrs, n, r);
    store_slots_into_var(r->env, var_name, map->module, map->count);
    if (handled) *handled = 1;
    return result;
}

/* Bir modul cagrisinin struct-var argümanlarini geri yaz:
   RaylibSimpleCollision.TerrainCollision(PLAYER, Terrain) PLAYER'i gunceller. */
static void write_back_struct_args(Runner *r, const char *module,
                                   GclExpr **args, int arg_count) {
    if (!r || !module || !args) return;
    for (int i = 0; i < arg_count; i++) {
        GclExpr *a = args[i];
        Var *v;
        const GclNativeStruct *ns;
        const GclNativeSlotMap *map;
        if (!a || a->kind != AST_EXPR_VAR) continue;
        v = env_find(r->env, a->name);
        if (!v || !v->decl_type) continue;
        ns = lookup_native_struct(v->decl_type);
        if (!ns) continue;
        map = native_slot_map_exact(ns->type, module);
        if (!map) continue;
        store_slots_into_var(r->env, a->name, module, map->count);
    }
}

/* ---------- Canli kopyalar (PLAYER.Camera gibi) ----------

   `Raylib.Camera3D CurrentCamera = PLAYER.Camera;` bir KOPYADIR, ama oyun
   dongusunde kameranin her karede tazelenmesi gerekir: PLAYER.Move() konumu
   degistirir, cizim ise CurrentCamera'yi okur. Bu yuzden kaynak+yol kaydedilir
   ve her native cagridan once kopya kaynagindan yeniden doldurulur. */
#define GCL_ALIAS_MAX 16
typedef struct {
    char target[96];
    char source[96];
    char member[64];
} GclNativeAlias;

static GclNativeAlias g_alias[GCL_ALIAS_MAX];
static int g_alias_count = 0;

static void register_native_alias(const char *target, const char *source, const char *member) {
    if (!target || !source || !member) return;
    for (int i = 0; i < g_alias_count; i++) {
        if (strcmp(g_alias[i].target, target) == 0) {
            snprintf(g_alias[i].source, sizeof(g_alias[i].source), "%s", source);
            snprintf(g_alias[i].member, sizeof(g_alias[i].member), "%s", member);
            return;
        }
    }
    if (g_alias_count >= GCL_ALIAS_MAX) return;
    snprintf(g_alias[g_alias_count].target, sizeof(g_alias[0].target), "%s", target);
    snprintf(g_alias[g_alias_count].source, sizeof(g_alias[0].source), "%s", source);
    snprintf(g_alias[g_alias_count].member, sizeof(g_alias[0].member), "%s", member);
    g_alias_count++;
}

static void refresh_native_aliases(GclEnv *env) {
    if (!env) return;
    for (int i = 0; i < g_alias_count; i++) {
        Var *target = env_find(env, g_alias[i].target);
        Var *source = env_find(env, g_alias[i].source);
        GclStructValue *member = NULL;
        if (!target || !source || !source->members) continue;
        for (GclStructValue *m = source->members; m; m = m->next) {
            if (m->name && strcmp(m->name, g_alias[i].member) == 0) { member = m; break; }
        }
        if (!member || !member->members) continue;
        if (target->members) free_struct_members(target->members);
        target->members = clone_struct_members(member->members);
    }
}

/* ---------- defer ---------- */

/* `defer <stmt>;` registers a cleanup action for the INNERMOST block (or one
   loop iteration). Actions run in REVERSE order (LIFO) when that frame closes:
   on the normal path, on `break`/`continue`, on `return`, and while an error
   unwinds towards its `try`.

   The stack is FILE SCOPE on purpose: call_user_func() copies the Runner with
   memcpy (see the B31 note in `struct Runner`), so a per-Runner array would be
   duplicated on every call and the copy would then run — or drop — the
   caller's actions. One stack plus a per-frame MARK is correct for any number
   of copied Runners, because a frame only ever truncates back to its own mark. */
typedef struct { GclStmt *stmt; } DeferEntry;
static DeferEntry *g_defers;
static int g_defer_count;
static int g_defer_cap;

static int defer_push(GclStmt *s) {
    if (!s) return 0;
    if (g_defer_count >= g_defer_cap) {
        int cap = g_defer_cap ? g_defer_cap * 2 : 32;
        DeferEntry *grown = (DeferEntry *)realloc(g_defers, (size_t)cap * sizeof(DeferEntry));
        if (!grown) return 0;
        g_defers = grown;
        g_defer_cap = cap;
    }
    g_defers[g_defer_count++].stmt = s;
    return 1;
}

/* Run everything registered since `mark`, newest first, then drop the frame.
   `exec_stmt` is declared just below; the definition comes later in the file
   but the call is fine because the frame only runs at runtime. */
static void defers_run_frame(Runner *r, int mark) {
    while (g_defer_count > mark) {
        GclStmt *s = g_defers[--g_defer_count].stmt;
        int was_unwinding = g_unwinding;
        if (!s) continue;
        /* Run the cleanup in a CLEAN error state. While `g_unwinding` is set,
           call_native_member() refuses to run native functions (the B33 guard
           that stops `printf("{}", undefined)` from emitting a partial "0"),
           and that guard would also silence every cleanup that closes a real
           resource - `defer Stdio.closeFile(f)` must still close `f` while an
           error unwinds. The action is a new, deliberate call, not the tail of
           the expression that failed, so the guard must not apply to it. */
        g_unwinding = 0;
        run_cleanup_body(s, r);
        if (g_unwinding) {
            /* The cleanup itself failed: its error REPLACES the one in flight
               and the remaining actions are skipped (Python's `finally` rule).
               The new error keeps unwinding to the nearest `try`. */
            break;
        }
        /* No new error: restore the state so the original error keeps
           unwinding outwards. */
        g_unwinding = was_unwinding;
    }
    /* Drop anything the cleanup itself registered (e.g. a `defer` inside a
       deferred block): its frame is the one that is ending right now. */
    if (g_defer_count > mark) g_defer_count = mark;
}

/* ---------- Statements ---------- */

static int exec_stmt(GclStmt *s, Runner *r);

static int exec_block(GclStmt *blk, Runner *r) {
    if (!blk || blk->kind != STMT_BLOCK) return 0;
    /* C block scope (B4): remember the variable-list head on entry and drop
       everything created inside on exit. Without this a bare `{ int fresh; }`
       leaked `fresh` past the closing brace, and a block nested in a loop body
       leaked its locals too. `global` variables are preserved by env_scope_exit(). */
    Var *scope_mark = r->env->vars;
    /* `defer` frame of this block: everything registered from here on runs when
       the block closes. */
    int defer_mark = g_defer_count;
    int rc = 0;
    for (int i = 0; i < blk->u.block.count && !r->return_flag && !r->break_flag && !r->continue_flag; i++) {
        if (exec_stmt(blk->u.block.stmts[i], r) != 0) { rc = -1; break; }
        /* try icinde bir hata olustuysa ifade 0 donse bile blok DEVAM ETMEZ:
           denetim try sinirina kadar hizli cikis yapar. */
        if (g_unwinding) { rc = -1; break; }
    }
    /* Deferred actions run BEFORE the block's variables disappear, so a
       deferred call can still use the resource it is closing. This is the only
       exit path check that matters: it runs for the normal end, for
       break/continue, for return (return_flag) and for error unwinding. */
    defers_run_frame(r, defer_mark);
    env_scope_exit(r->env, scope_mark);
    return rc;
}

/* ---------- try/catch yardimcilari ---------- */

/* Bir govdeyi (blok ya da tek durum) calistirir; try/catch/finally ayni
   yoldan gecer. */
static int run_nested(GclStmt *body, Runner *r) {
    if (!body) return 0;
    if (body->kind == STMT_BLOCK) return exec_block(body, r);
    return exec_stmt(body, r);
}

/* Run a body that MUST execute even while `return` / `break` / `continue` is
   unwinding outwards: a `finally` block, a `catch` block, or a deferred
   action.

   exec_block() refuses to run any statement while one of those flags is set -
   that is how a block stops early once control has been transferred out of it.
   A `finally` body is just another block, so it was silently SKIPPED on
   `return` (`try { return 1; } finally { printf("x"); }` printed nothing) and
   the same held for `break` and `continue`. A braced `defer { ... }` action had
   the identical hole, because defers_run_frame() runs the action through
   exec_stmt() -> exec_block().

   The flags are saved, cleared for the duration, and put back afterwards. A
   `return` / `break` / `continue` issued BY the cleanup WINS over the one that
   was in flight (Python's rule for `finally`); that is why the saved flags are
   restored only when the body did not transfer control itself.

   `g_unwinding` is deliberately NOT touched here: both callers (STMT_TRY and
   defers_run_frame) clear it themselves so a cleanup that closes a real
   resource can still run. */
static int run_cleanup_body(GclStmt *body, Runner *r) {
    if (!body) return 0;
    int save_return = r->return_flag;
    int save_break = r->break_flag;
    int save_continue = r->continue_flag;
    r->return_flag = 0;
    r->break_flag = 0;
    r->continue_flag = 0;
    int rc = run_nested(body, r);
    if (!r->return_flag && !r->break_flag && !r->continue_flag) {
        r->return_flag = save_return;
        r->break_flag = save_break;
        r->continue_flag = save_continue;
    }
    return rc;
}

/* Hata nesnesinin bir alanini uretir (GclStructValue zinciri). */
static GclStructValue *mk_member_node(const char *name, double num, const char *str) {
    GclStructValue *m = (GclStructValue *)calloc(1, sizeof(GclStructValue));
    if (!m) return NULL;
    m->name = strdup(name ? name : "");
    m->num = num;
    if (str) { m->str = strdup(str); m->is_string = 1; }
    return m;
}

/* `catch (err)` — hata nesnesini env'e baglar. Alanlar `.uye` erisimiyle
   okunur (resolve_member_chain struct yolunu kullanir):
       err.message (metin), err.code / err.line / err.col (sayi). */
static void bind_error_object(Runner *r, const char *name, const char *msg,
                              int code, int line, int col) {
    if (!r || !name) return;
    env_set_num(r->env, name, 0.0);
    Var *v = env_find(r->env, name);
    if (!v) return;
    if (v->members) { free_struct_members(v->members); v->members = NULL; }
    GclStructValue *head = mk_member_node("message", 0.0, msg ? msg : "");
    GclStructValue *cur = head;
    static const char *field_names[3] = { "code", "line", "col" };
    double field_vals[3] = { (double)code, (double)line, (double)col };
    for (int i = 0; i < 3; i++) {
        GclStructValue *m = mk_member_node(field_names[i], field_vals[i], NULL);
        if (!m) continue;
        if (!cur) { head = m; cur = m; continue; }
        cur->next = m;
        cur = m;
    }
    v->members = head;
}

/* Yakalanmayan hata en distaki try'dan tasiyor: RAPORLA.
   * `throw` ile uretilen hata programi DURDURUR (Python gibi) — g_unwinding
     korunur, bloklar -1 dondurur ve surec 1 ile cikar.
   * siradan runtime hatasi TOLERANSLI moda doner: mesaj basildiktan sonra akis
     try'dan SONRAKI durumdan devam eder (legacy sozlesme; 17 altin test).
   Dis bir try varsa hicbir sey yapilmaz: hata ona kadar tasinir. */
static void error_escalate(void) {
    if (!g_err_valid) return;
    if (g_try_depth > 0) return;
    /* Same single reporting point as the tolerant path: record for the IDE,
       count for the exit code, print for the user. Keeping ONE place that
       formats the line is what stops the two paths from drifting apart. */
    runtime_report_pending();
    g_err_valid = 0;
    if (!g_err_is_throw) g_unwinding = 0;
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
            /* B4 blok scope: her bildirim YENI bir Var yaratir (listenin basina
               eklenir). Boylece ic bloktaki `int x = 10;` distaki `x`'i GOLGELER
               (paylasmaz) ve exec_block cikisinda env_scope_exit onu siler.
               Aksi halde bildirim env_find ile distaki Var'i bulup degerini
               EZIYORDU: `int x = 5; { int x = 10; }` sonrasi x = 10 kaliyordu.
               `global` birlesme yolu (yukarida) bundan muaftir. */
            if (name && !s->u.var_decl.is_global) {
                Var *decl = (Var *)calloc(1, sizeof(Var));
                if (decl) {
                    decl->name = strdup(name);
                    decl->next = r->env->vars;
                    r->env->vars = decl;
                    if (type && type[0]) decl->decl_type = strdup(type);
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
                        /* Dongu icinde tekrar bildirilen struct dizisi: onceki
                           iterasyonun elemanlarini serbest birak, yoksa her
                           karede belleK sizar. */
                        if (vv->arr_members) {
                            for (int ai = 0; ai < vv->arr_count; ai++)
                                if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                            free(vv->arr_members);
                            vv->arr_members = NULL;
                        }
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
            /* Native modül struct tipi: `Raylib.Rectangle r;` ya da düz `Rectangle r;`.
               Kullanıcı tanımlı struct/typedef YUKARIDA ele alınır ve kazanır
               (complete_type.c'deki gölgeleme kuralı); bu yüzden buraya yalnızca
               hiçbir kullanıcı tipi uymadığında gelinir. Alan listesi olmadan
               `r.x` sessizce 0 dönerdi (hata yok, yanlış değer var). */
            if (type && !base) {
                const GclNativeStruct *ns = lookup_native_struct(type);
                if (ns) {
                    /* tek native struct değişkeni */
                    if (!(s->u.var_decl.array_size >= 1)) {
                        env_set_native_struct(r->env, name, ns);
                        if (s->u.var_decl.init) {
                            GclExpr *init = s->u.var_decl.init;
                            if (!fill_native_from_constructor(r, name, ns, init)) {
                                /* Baslatici bir MODUL cagrisi mi (Raylib.GetMousePosition()
                                   gibi)? Oyleyse cagri modulun g_last_* slotunu doldurur;
                                   degeri oradan degiskene kopyala. Boyle bir kopya olmadan
                                   `Raylib.Vector2 m = Raylib.GetMousePosition();` sessizce
                                   (0,0) veriyordu. Cagri bir modul uyesi degilse (kullanici
                                   fonksiyonu) eski fill_struct_init yolu calisir. */
                                const char *init_mod = NULL;
                                if (init->kind == AST_EXPR_CALL && init->left &&
                                    init->left->kind == AST_EXPR_MEMBER && init->left->left &&
                                    init->left->left->kind == AST_EXPR_VAR)
                                    init_mod = init->left->left->name;
                                if (init_mod) {
                                    eval_expr(init, r);   /* modul cagrisi -> slot dolar */

                                    /* IKI ayri read-back kanali vardir:

                                       1. raylib'in KENDI struct'lari (Vector2,
                                          Camera3D, Shader...) icin raylib'in
                                          Last* erisimcileri -- bunlari
                                          fill_native_from_last_slot surer.
                                       2. MODUL SAHIPLI tipler (RaylibSimpleMesh.Mesh,
                                          Terrain, RaylibFPS.FPS) icin modulun kendi
                                          LastSlot kanali -- bu tiplerin Last*
                                          haritasi YOKTUR ve fill_native_... sessizce
                                          0 doner.

                                       Ikinci kanal yok sayilinca `RaylibSimpleMesh.Mesh
                                       CUBE = RaylibSimpleMesh.LoadObj("cube.obj");`
                                       bildiriminde CUBE.Handle 0'da (varsayilan)
                                       kaliyordu. 0, kayit tablosunun ILK yuvasidir --
                                       yani ARAZI. Sonuc: CUBE.Draw() araziyi
                                       ciziyor, kure hic gorunmuyordu. Ikinci
                                       yukleme (arazi zaten yuva 0) yalnizca bu
                                       yuzden "calisiyor" gibi gorunuyordu: kendi
                                       yuvasi ile varsayilan 0 ayni cikti.

                                       Bu yuzden once raylib kanali denenir; o
                                       tipi tanimiyorsa (0 donerse) slot kanalina
                                       dusulur. */
                                    if (!fill_native_from_last_slot(r, name, init_mod, ns)) {
                                        const GclNativeSlotMap *decl_slot_map =
                                            native_slot_map_exact(ns->type, init_mod);
                                        if (decl_slot_map)
                                            store_slots_into_var(r->env, name, init_mod,
                                                                 decl_slot_map->count);
                                    }
                                } else if (init->kind == AST_EXPR_MEMBER && init->left &&
                                           init->left->kind == AST_EXPR_VAR &&
                                           env_find(r->env, init->left->name)) {
                                    /* Baska bir native struct'in uyesinden kopya:
                                       canli bag kurulur, her native cagridan once
                                       kaynagindan tazelenir
                                       (Raylib.Camera3D cam = PLAYER.Camera;). */
                                    register_native_alias(name, init->left->name,
                                                          init->member_name);
                                    refresh_native_aliases(r->env);
                                } else {
                                    fill_struct_init(r, name, init);
                                }
                            }
                        }
                        return 0;
                    }
                    /* native struct dizisi: alanları her eleman için kur */
                    int cnt = s->u.var_decl.array_size;
                    if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST)
                        cnt = s->u.var_decl.init->arg_count;
                    env_set_num(r->env, name, 0.0);
                    Var *vv = env_find(r->env, name);
                    if (vv) {
                        /* Dongu icinde tekrar bildirilen native struct dizisi:
                           onceki elemanlari serbest birak (bellek sizmasin). */
                        if (vv->arr_members) {
                            for (int ai = 0; ai < vv->arr_count; ai++)
                                if (vv->arr_members[ai]) free_struct_members(vv->arr_members[ai]);
                            free(vv->arr_members);
                            vv->arr_members = NULL;
                        }
                        vv->arr_count = cnt;
                        vv->arr_members = (GclStructValue **)calloc((size_t)cnt, sizeof(GclStructValue *));
                        for (int ai = 0; ai < cnt; ai++)
                            build_native_members(&vv->arr_members[ai], ns);
                        if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_INIT_LIST) {
                            GclExpr *init = s->u.var_decl.init;
                            for (int ai = 0; ai < init->arg_count && ai < cnt; ai++) {
                                if (init->args[ai] && init->args[ai]->kind == AST_EXPR_INIT_LIST)
                                    fill_members_from_init_list(r, vv->arr_members[ai], init->args[ai]);
                            }
                        }
                    }
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
                } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_VAR) {
                    /* B5: `gcChar h2 = h;` — kaynak bir STRING DEĞİŞKENİ ise
                       değeri kopyala. Eskiden "" yazılıyordu (sessiz kayıp);
                       yalnızca `h2 = h;` (atama) yolu bunu doğru yapıyordu. */
                    Var *src = env_find(r->env, s->u.var_decl.init->name);
                    env_set_str(r->env, name, (src && src->is_string && src->str) ? src->str : "");
                } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_CALL) {
                    /* B5 (call half): the text a function returns lives in
                       return_str, not in the double `v` - `gcChar g = greet();`
                       stored "". The call was already evaluated by the
                       `double v = eval_expr(...)` above, so the channel holds
                       THIS call's text; a numeric return falls back to the
                       number, the choice the char-array path already makes. */
                    if (r->return_str) env_set_str(r->env, name, r->return_str);
                    else env_set_num(r->env, name, v);
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
                                if (vv->arr_strs) {
                                    for (int ii = 0; ii < cnt && vv->arr_strs[ii]; ii++) free(vv->arr_strs[ii]);
                                    free(vv->arr_strs);
                                    vv->arr_strs = NULL;
                                }
                                if (vv->arr_vals) { free(vv->arr_vals); vv->arr_vals = NULL; }
                                /* B5 - an entry is TEXT when it is a literal, a string
                                   VARIABLE or a CALL (see collect_text_row). The old gate
                                   accepted only a literal, so `{ a, "two" }` fell through to
                                   the numeric branch and printed "" / "t". */
                                double *vals = (double *)calloc((size_t)cnt + 1, sizeof(double));
                                char  **texts = (char **)calloc((size_t)cnt + 1, sizeof(char *));
                                if (!vals || !texts) {
                                    free(vals);
                                    free(texts);
                                } else if (collect_text_row(r, init, cnt, vals, texts)) {
                                    free(vals);
                                    vv->arr_strs = texts;      /* texts[] is the row table */
                                } else {
                                    for (int ai = 0; ai < cnt; ai++) if (texts[ai]) free(texts[ai]);
                                    free(texts);
                                    vv->arr_vals = vals;       /* chars by code */
                                }
                            }
                        } else {
                            env_set_str(r->env, name, "");
                        }
                    } else {
                        /* single-dimension char array. B5 - the initializer may be
                           a string VARIABLE or a CALL too (`char buf[16] = s;`),
                           not only a literal; init_text_value() decides. */
                        const char *csrc = init_text_value(r, s->u.var_decl.init);
                        if (csrc) {
                            /* B24 — `char buf[4] = "abcdefghij";` bildirilen boyutu
                               UYGULAMIYORDU: 10 karakter saklaniyor, ne kirpma ne
                               hata oluyordu (C'de derleme hatasi). Tasi mayi
                               onlemek icin N karaktere KIRP — sessizce buyutmek
                               B22'deki kapasite modelini tutarsiz yapiyordu. */
                            const char *src = csrc;
                            int cap = s->u.var_decl.array_size;
                            if (cap >= 1 && src && (int)strlen(src) > cap) {
                                char *clipped = (char *)calloc((size_t)cap + 1, 1);
                                if (clipped) {
                                    memcpy(clipped, src, (size_t)cap);
                                    env_set_str(r->env, name, clipped);
                                    free(clipped);
                                } else {
                                    env_set_str(r->env, name, src);
                                }
                            } else {
                                env_set_str(r->env, name, src);
                            }
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
                /* B22 — char[N] GERÇEK kapasite. Başlatıcısız `char small[3];`
                   yalnızca strdup("") = 1 bayt ayırıyordu, ama eleman yazımı
                   array_size (3) ile sınırlandığı için `small[1]='B'` tahsisin
                   1 bayt ÖTESİNE yazıyordu (heap taşması). Tamponu en az N+1
                   bayta çıkar; mevcut içerik korunur. */
                {
                    Var *cv = env_find(r->env, name);
                    if (cv && s->u.var_decl.array_size >= 1 && !s->u.var_decl.is_pointer) {
                        size_t need = (size_t)s->u.var_decl.array_size + 1;
                        size_t have = cv->str ? strlen(cv->str) + 1 : 0;
                        if (have < need) {
                            char *nb = (char *)calloc(need, 1);
                            if (nb) {
                                if (cv->str) {
                                    memcpy(nb, cv->str, have > 0 ? have - 1 : 0);
                                    free(cv->str);
                                }
                                cv->str = nb;
                                cv->is_string = 1;
                            }
                        }
                    }
                }
            } else {
                if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_STRING) {
                    env_set_str(r->env, name, s->u.var_decl.init->str);
                } else if (s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_CALL) {
                    /* Fonksiyonun dondurdugu METIN yalnizca return_str'den gelir
                       (`v` bir double'dir, metni tasimaz). Eskiden kosulsuz
                       env_set_num cagriliyordu: `gcChar g = greet();` BOS
                       string veriyordu. */
                    if (r->return_str) env_set_str(r->env, name, r->return_str);
                    else env_set_num(r->env, name, v);
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
                                    /* TURN 32: the same rule as the scalar path - an element whose
                                       written value is not representable in the declared type must
                                       fall through to the NUMERIC branch, otherwise the array would
                                       print digits the element cannot hold. */
                                    if (all_num_with_lex) {
                                        for (int ai = 0; ai < cnt; ai++) {
                                            if (!init->args[ai] ||
                                                !unsigned_mirror_digits_fit(init->args[ai]->str, eff_type)) {
                                                all_num_with_lex = 0;
                                                break;
                                            }
                                        }
                                    }
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
                    s->u.var_decl.init && s->u.var_decl.init->kind == AST_EXPR_FLOAT &&
                    s->u.var_decl.init->str &&
                    unsigned_mirror_digits_fit(s->u.var_decl.init->str, type)) {
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
            /* Dongu govdesi blok kapsamlidir: govdede tanimlanan degiskenler
               dongu bitince env'den silinir (C blok-scope). Disaridaki ve
               `global` degiskenler korunur. */
            Var *scope_mark = r->env->vars;
            int guard = 0;
            r->loop_depth++;
            r->breakable_depth++;
            while (!r->break_flag && !r->return_flag && !g_unwinding) {
                double cond = s->u.while_.cond ? eval_expr(s->u.while_.cond, r) : 0.0;
                if (!cond) break;
                if (++guard > max_loop_iterations()) {
                    runtime_errorcf(GCL_E_SEM_INFINITE_LOOP, span_of_stmt(s),
                                    "possible infinite loop (GCL3009)");
                    /* B34 - inside a `try` the guard is a CATCHABLE error: stop
                       this loop and let the nearest `try` pick the error up off
                       the normal channel. Outside a `try` a runaway loop cannot
                       be recovered from, so the process stops (exit != 0) - the
                       legacy behaviour, unchanged. */
                    if (g_unwinding) break;
                    return -1;
                }
                /* continue: sonraki iterasyona geç (continue_flag'i temizle) */
                r->continue_flag = 0;
                /* One frame per ITERATION: a `defer` in a loop body (braced or
                   not) runs when that iteration ends, not when the loop does. */
                int iter_defer_mark = g_defer_count;
                if (s->u.while_.body->kind == STMT_BLOCK) exec_block(s->u.while_.body, r);
                else exec_stmt(s->u.while_.body, r);
                defers_run_frame(r, iter_defer_mark);
                if (r->continue_flag) r->continue_flag = 0;
                /* try icinde hata olustu: donguden CIK, denetim try sinirina
                   gitsin (aksi halde ayni hata her iterasyonda tekrarlanirdi). */
                if (g_unwinding) break;
            }
            r->break_flag = 0; r->continue_flag = 0;
            r->loop_depth--;
            r->breakable_depth--;
            env_scope_exit(r->env, scope_mark);
            if (g_unwinding) return -1;
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
            /* Govde blok kapsami: for-init'ten SONRA isaretle; boylece init
               degiskeni korunur, govdede tanimlananlar dongu sonunda silinir. */
            Var *scope_mark = r->env->vars;
            int guard = 0;
            r->loop_depth++;
            r->breakable_depth++;
            while (!r->break_flag && !r->return_flag && !g_unwinding) {
                if (s->u.for_.cond) {
                    double cond = eval_expr(s->u.for_.cond, r);
                    if (!cond) break;
                }
                if (++guard > max_loop_iterations()) {
                    runtime_errorcf(GCL_E_SEM_INFINITE_LOOP, span_of_stmt(s),
                                    "possible infinite loop (GCL3009)");
                    /* B34 - see the STMT_WHILE guard above. */
                    if (g_unwinding) break;
                    return -1;
                }
                /* continue: inc'e atla (continue_flag'i temizle) */
                r->continue_flag = 0;
                /* Same per-iteration frame as STMT_WHILE. It is closed BEFORE
                   the increment, so a deferred action observes the state the
                   body left behind (C++ body-scope ordering). */
                int iter_defer_mark = g_defer_count;
                if (s->u.for_.body->kind == STMT_BLOCK) exec_block(s->u.for_.body, r);
                else exec_stmt(s->u.for_.body, r);
                defers_run_frame(r, iter_defer_mark);
                if (r->continue_flag) r->continue_flag = 0;
                if (s->u.for_.inc) eval_expr(s->u.for_.inc, r);
                /* try icinde hata: donguden cik (bkz. STMT_WHILE). */
                if (g_unwinding) break;
            }
            r->break_flag = 0; r->continue_flag = 0;
            r->loop_depth--;
            r->breakable_depth--;
            /* Once GOVDE degiskenlerini sil (env_scope_exit `scope_mark`i
               for-init dugumunde durur). Sonra for-init degiskenini kaldir.
               Sira TERS olursa env_remove_var, scope_mark'in isaret ettigi
               dugumu serbest birakir; env_scope_exit askida kalan isaretciyle
               karsilastirir ve TUM listeyi siler (use-after-free). */
            env_scope_exit(r->env, scope_mark);
            if (for_var_name && !for_existing) env_remove_var(r->env, for_var_name);
            if (g_unwinding) return -1;
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
            /* Metin donduren `return` (gcChar / char*). Runner'da metin kanali
               YOKTU: `eval_expr` bir double dondurur ve metin kayboluyordu —
               `gcChar greet() { return "hi"; }` cagrisi 0 veriyordu. */
            if (s->u.ret.expr) {
                const char *ret_text = NULL;
                if (s->u.ret.expr->kind == AST_EXPR_STRING) {
                    ret_text = s->u.ret.expr->str;
                } else if (s->u.ret.expr->kind == AST_EXPR_VAR) {
                    Var *rv = env_find(r->env, s->u.ret.expr->name);
                    if (rv && rv->is_string) ret_text = rv->str;
                }
                if (ret_text) {
                    if (r->return_str) free(r->return_str);
                    r->return_str = strdup(ret_text);
                    r->return_value = 0.0;
                    return 0;
                }
            }
            r->return_value = s->u.ret.expr ? eval_expr(s->u.ret.expr, r) : 0.0;
            return 0;
        case STMT_BREAK:
            /* B31 — dongu/switch DISINDA `break`: blogu sessizce bitirmek
               yerine hata ver (eskiden hata yok, exit 0 idi). */
            if (r->breakable_depth <= 0) {
                runtime_errorf(span_of_stmt(s), "'break' outside a loop or switch");
                return -1;
            }
            r->break_flag = 1;
            return 0;
        case STMT_CONTINUE:
            /* B31 — dongu DISINDA `continue` ayni sekilde sessizdi. */
            if (r->loop_depth <= 0) {
                runtime_errorf(span_of_stmt(s), "'continue' outside a loop");
                return -1;
            }
            r->continue_flag = 1;
            return 0;
        case STMT_SWITCH: {
            /* B31: switch de `break` icin gecerli bir baglamdir (ama `continue`
               degildir — loop_depth artmaz). */
            r->breakable_depth++;
            /* Prefer the parsed condition expression (supports `switch (v + 1)`,
               `switch (f(x))`); fall back to the legacy plain-variable name. */
            double val = s->u.switch_.cond
                             ? eval_expr(s->u.switch_.cond, r)
                             : (s->u.switch_.name ? env_get_num(r->env, s->u.switch_.name) : 0.0);
            int matched = 0;
            for (int i = 0; i < s->u.switch_.case_count; i++) {
                if (!matched && s->u.switch_.case_vals[i]) {
                    double cv = eval_expr(s->u.switch_.case_vals[i], r);
                    if (val == cv) matched = 1;
                }
                if (matched) {
                    if (s->u.switch_.cases[i]) exec_stmt(s->u.switch_.cases[i], r);
                    if (r->break_flag) { r->break_flag = 0; break; }
                    if (r->return_flag) { r->breakable_depth--; return 0; }
                    /* try icinde hata: switch'ten cik (case govdesi yarida kalir). */
                    if (g_unwinding) { r->breakable_depth--; return -1; }
                }
            }
            if (!matched && s->u.switch_.default_case) {
                exec_stmt(s->u.switch_.default_case, r);
                if (r->break_flag) r->break_flag = 0;
            }
            r->breakable_depth--;
            if (g_unwinding) return -1;
            return 0;
        }
        case STMT_TRY: {
            /* try { body } [catch (err) { ... }] [finally { ... }]
               Hata yakalama akisi:
                 1) try govdesi kosar; ic hata olusursa `g_unwinding` ile erken
                    cikilir ve hata global slotta durur.
                 2) catch dali varsa hata YAKALANIR: hata nesnesi `err` adina
                    baglanir (message/code/line/col) ve catch govdesi kosar.
                 3) finally govdesi HER durumda kosar (hata olsa da, catch olsa
                    da). finally icinde olusan YENI hata oncekini EZER.
                 4) Hala cozulmemis bir hata varsa: `error_escalate()` en distaki
                    try'da raporlar — `throw` programi durdurur, siradan runtime
                    hatasi toleransli moda doner. */
            int pending = 0;
            g_try_depth++;
            run_nested(s->u.try_.body, r);
            g_try_depth--;
            int had_error = g_unwinding ? 1 : 0;

            if (had_error) {
                if (s->u.try_.catch_body) {
                    char msg[GCL_ERR_MSG_MAX];
                    snprintf(msg, sizeof(msg), "%s", g_err_msg);
                    int code = g_err_code, eline = g_err_line, ecol = g_err_col;
                    g_unwinding = 0;
                    g_err_valid = 0;
                    /* catch degiskeni KENDI blok kapsaminda yasar: disari sizmaz
                       ve ayni adli bir dis degiskeni kalici olarak bozmaz. */
                    Var *mark = r->env->vars;
                    if (s->u.try_.err_name)
                        bind_error_object(r, s->u.try_.err_name, msg, code, eline, ecol);
                    g_try_depth++;
                    run_cleanup_body(s->u.try_.catch_body, r);
                    g_try_depth--;
                    env_scope_exit(r->env, mark);
                    pending = g_unwinding;   /* catch icinde YENI hata olustu mu? */
                } else {
                    pending = 1;             /* catch yok: hata disari tasinir */
                }
            }

            if (s->u.try_.finally_body) {
                g_unwinding = 0;
                g_try_depth++;
                run_cleanup_body(s->u.try_.finally_body, r);
                g_try_depth--;
                if (g_unwinding) pending = 1;   /* finally hatasi oncekini ezer */
            }

            g_unwinding = pending;
            if (pending) error_escalate();
            if (g_unwinding) return -1;
            return 0;
        }
        case STMT_THROW: {
            /* throw <expr>; / raise <expr>;
               Metin ifadesi mesaj olur; sayi ifadesi de mesaja yazilir. */
            char msg[GCL_ERR_MSG_MAX];
            GclExpr *v = s->u.throw_.expr;
            if (v) {
                if (v->kind == AST_EXPR_STRING) {
                    snprintf(msg, sizeof(msg), "%s", v->str ? v->str : "");
                } else if (v->kind == AST_EXPR_VAR) {
                    Var *src = env_find(r->env, v->name);
                    if (src && src->is_string && src->str) snprintf(msg, sizeof(msg), "%s", src->str);
                    else snprintf(msg, sizeof(msg), "%.17g", src ? src->num : 0.0);
                } else {
                    snprintf(msg, sizeof(msg), "%.17g", eval_expr(v, r));
                }
            } else {
                snprintf(msg, sizeof(msg), "throw");
            }
            error_capture(msg, GCL_E_SEM_THROW, span_of_stmt(s), 1);
            return -1;   /* yakalanana kadar ya da program durana dek tasinir */
        }
        case STMT_DEFER:
            /* Registration only: the action itself runs when its frame closes
               (block exit or end of the loop iteration). */
            if (!defer_push(s->u.defer_.stmt)) {
                runtime_errorf(span_of_stmt(s), "out of memory while registering 'defer'");
                return -1;
            }
            return 0;
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
                    fd->return_type = s->u.func_decl.return_type ? strdup(s->u.func_decl.return_type) : NULL;
                    if (fd->param_count > 0) {
                        /* TURN 26 — one entry per parameter: name + optional
                           type, copied in the SAME loop. The runner can no
                           longer hold a name without its own type slot. */
                        fd->params = (GclParam *)calloc((size_t)fd->param_count, sizeof(GclParam));
                        for (int pi = 0; pi < fd->param_count; pi++) {
                            const GclParam *src = &s->u.func_decl.params[pi];
                            fd->params[pi].name = src->name ? strdup(src->name) : NULL;
                            fd->params[pi].type = src->type ? strdup(src->type) : NULL;
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
        if (fd->params) {
            /* TURN 26 — each entry owns its name and its optional type. */
            for (int pi = 0; pi < fd->param_count; pi++) {
                free(fd->params[pi].name);
                free(fd->params[pi].type);
            }
            free(fd->params);
        }
        free(fd->return_type);
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
    gcl_runtime_errors = 0;   /* B17: her kosu kendi hata sayaciyla baslar */
    /* try/catch durumu da her kosuda sifirlanir: onceki kosudan kalan bir
       "bekleyen hata" yeni programi etkilememelidir. */
    g_err_msg[0] = '\0';
    g_err_code = 0;
    g_err_line = 0;
    g_err_col = 0;
    g_err_valid = 0;
    g_err_is_throw = 0;
    g_unwinding = 0;
    g_try_depth = 0;
    if (gcl_debug) fprintf(stderr, "gcl_run_program: start\n");
    GclEnv env = { 0 };
    Runner r = { 0 };
    r.env = &env;
    if (base_dir) snprintf(env.base_dir, sizeof(env.base_dir), "%s", base_dir);

    /* Host API'yi YÜKLEMEDEN ÖNCE hazırla: native_load modüllere bu tabloyu
       verir, modüller de callback'leri bu yolla çağırır. */
    g_host_api.abi_version = GCL_HOST_ABI_VERSION;
    g_host_api.call_gcl    = host_call_gcl;
    g_host_api.host        = &env;
    g_active_runner        = &r;
    g_tick_count           = 0;

    /* #native ile bildirilen modülleri yükle (Library/ dizininden) */
    for (int i = 0; i < native_count; i++) {
        if (!native_modules[i]) continue;
        /* B27 — YUKLEME ANI TANILAMASI. Eskiden `native_load` basarisiz olunca
           HICBIR SEY soylenmiyordu: hata ancak modulun bir uyesi CAGRILINCA
           stderr'e dusuyordu ve o ana kadar exit kodu 0 kaliyordu. Bozuk bir
           `#native <X>` bildirimi artik program BASLARKEN bildirilir.

           Neden tanilama burada: `native_load` basarisizlikta yalnizca NULL
           DONER ve hicbir sey basmaz (govdesindeki her hata yolu
           `free(m->name); free(m); return NULL;` seklindedir) — cagiranin
           karar vermesi icin sessizlik kasitli bir tasarimdir, cunku
           `call_native_member` ayni modulu TEMBEL olarak yeniden yuklemeyi
           deneyebilir (Math/Stdio/Embed icin). */
        if (!native_load(&env, native_modules[i])) {
            runtime_errorcf(GCL_E_SEM_UNKNOWN_MODULE, span_none(),
                            "unknown module '%s' (#native) - module could not be loaded",
                           native_modules[i]);
        }
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
    /* Program scope: a file-level `defer` runs when the program finishes.
       Top-level statements run through this plain loop rather than exec_block,
       so this is the frame their registrations belong to. `g_defer_count` is
       reset first so a previous run in the same process cannot leak into this
       one (the IDE and the test harness both call gcl_run_program repeatedly). */
    g_defer_count = 0;
    int top_defer_mark = g_defer_count;
    for (int i = 0; i < prog->count; i++) {
        GclStmt *s = prog->stmts[i];
        if (s && (s->kind == STMT_FUNC_DECL || s->kind == STMT_TYPEDEF ||
                  s->kind == STMT_ENUM || s->kind == STMT_STRUCT_DECL)) {
            continue;
        }
        if (exec_stmt(s, &r) != 0) {
            /* Hata ZATEN raporlandiysa (ya da `throw` ile uretildiyse) ikinci bir
               ic-iz satiri basmak kullaniciyi yaniltir: gercek mesaj stderr'de
               duruyor. Yalnizca ACIKLANAMAYAN bir -1 icin ek satir basilir. */
            if (!g_err_valid && gcl_runtime_errors == 0)
                runtime_errorf(span_of_stmt(s), "top-level statement %d (kind=%d) returned error",
                               i, (int)(s ? s->kind : (GclStmtKind)-1));
            env_cleanup(&env);
            /* DIKKAT: bu satir eskiden `gcl_debug` kapisi OLMADAN basiliyordu.
               `-debug` verilmese bile kullanicinin stderr'ine bir IC iz dusuyordu
               (golden test ciktilarinda da goruluyordu: "gcl_run_program: returning
               -1 due to top-level stmt"). Diger tum debug ciktisi gcl_debugf() ile
               kapili oldugu icin bu satir da ayni kapiya baglandi. */
            if (gcl_debug) fprintf(stderr, "gcl_run_program: returning -1 due to top-level stmt\n");
            return -1;
        }
        if (r.return_flag) break;
    }

    /* main() varsa otomatik çağır — argc/argv parametrelerini bağla */
    env.arg_count = argc;
    env.arg_vals = argv;
    FuncDef *main_fd = find_func(&env, "main");
    int main_rc = 0;
    if (main_fd) {
        Runner sub;
        memcpy(&sub, &r, sizeof(sub));
        sub.return_flag = 0;
        sub.return_value = 0;
        sub.break_flag = 0;
        sub.continue_flag = 0;
        /* B31: main() de dongu baglamini devralmaz. */
        sub.loop_depth = 0;
        sub.breakable_depth = 0;
        /* argc parametresi */
        if (main_fd->param_count > 0) env_set_num(&env, main_fd->params[0].name, (double)argc);
        int rc = 0;
        if (main_fd->body && main_fd->body->kind == STMT_BLOCK) rc = exec_block(main_fd->body, &sub);
        else if (main_fd->body) rc = exec_stmt(main_fd->body, &sub);
        /* B17 — main()'in donus degeri artik YOK SAYILMIYOR: exit code olur.
           exec_stmt/exec_block -1 donerse (guard/erken hata) 1'e eslenir. */
        if (rc != 0) main_rc = 1;
        else if (sub.return_value != 0) main_rc = (int)sub.return_value;
    }

    /* Run program-level deferred actions now: after main(), while the globals
       they may reference are still alive. */
    defers_run_frame(&r, top_defer_mark);

    env_cleanup(&env);
    /* B17 — yakalanmamis runtime hatasi varsa exit code ≠ 0. Eskiden her hata
       stderr'e yazilip program 0 ile cikiyordu; CI/IDE hatayi goremiyordu. */
    if (gcl_runtime_errors > 0) {
        if (gcl_debug) fprintf(stderr, "gcl_run_program: %d runtime error(s)\n", gcl_runtime_errors);
        return 1;
    }
    if (main_rc != 0) {
        if (gcl_debug) fprintf(stderr, "gcl_run_program: main returned %d\n", main_rc);
        return main_rc;
    }
    if (gcl_debug) fprintf(stderr, "gcl_run_program: return 0\n");
    return 0;
}
