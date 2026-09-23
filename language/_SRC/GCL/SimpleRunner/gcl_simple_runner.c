/*
 * gcl_simple_runner.c — GCL dosyasını preprocess → lexer → parser → runner pipeline'ından geçirir.
 *
 * preprocessor: #define, #include, #if/#ifdef/#ifndef/#else/#endif, #undef
 * native:      #native <Math> direktifleri modül listesine eklenir (Library/Math.dll|.so)
 */

#include "gcl_simple_runner.h"
#include "gcl_lexer.h"
#include "gcl_parser.h"
#include "gcl_runner.h"
#include "gcl_error.h"
#include "gcl_diag.h"
#include "gcl_terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_MACROS 256
#define MAX_MACRO_PARAMS 16
#define MAX_MACRO_ARGS 16
#define MAX_INCLUDES 32
/* B20: #include recursion guard. pp_process() recurses on #include; a file that
   includes itself (or a cyclic include) previously overflowed the native C
   stack (exit 0xC00000FD). Depth is limited here and the offending file is
   reported instead of crashing. */
#define GCL_MAX_INCLUDE_DEPTH 32
static int g_include_depth = 0;

/* Aktif `#error` sayaci. C'de `#error` derlemeyi DURDURUR; bu modul eskiden
   yalnizca kirmizi bir satir basip AKMAYA devam ediyordu (program calisiyordu,
   cikis kodu 0 kaliyordu). Renkli satir IDE icin kalir; bu sayac
   gcl_simple_run_source'in kosuyu iptal etmesini saglar.
   `#include` ozyinelemesi ayni sayaci paylasir, yani include edilen bir
   dosyadaki `#error` de derlemeyi durdurur (C'deki gibi).
   `#if 0` icindeki bir `#error` sayilmaz (aktiflik kontrolu cagirida). */
static int g_pp_error_hits = 0;

/* TURN 24 — Basarisiz `#include` sayaci. Iki durum: dosya bulunamadi ve
   include derinligi asildi. Ikisi de eskiden YALNIZCA stderr'e bir satir
   yaziyordu: program derlenip CALISIYOR ve cikis kodu 0 kaliyordu — yani
   "Error:" yazan bir cikti basarili bir build gibi gorunuyordu (IDE ve CI
   hatayi goremiyordu). C'de bu ikisi FATAL derleme hatasidir. Etkin bir
   `#include` basarisiz olursa derleme durur. `#if 0` icindeki bir include
   artik hic cozumlenmedigi icin (bkz. include dali) bu sayac yalnizca
   gercek hatalari gorur. */
/* TURN 27 — bu sayac ARTIK DOSYA KAPSAMLI DEGIL. `pp_process` hatayi bir
   OUT parametresiyle disariya verir (asagida), boylece `#include` basarisiz
   oldugunda derlemeyi durdurma karari tek cagirana aittir ve unutulmasi
   mumkun degildir. Once `gcl_simple_run_source` hem sifirlamayi hem
   kontrolu hatirlamak zorundaydi; ikinci bir giris noktasi ikisini de
   unutabilirdi. */

/* B28 — `#pragma once`. Eskiden `#include` "yapistir" semantigiydi: ayni
   dosya iki kez dahil edilince icerik ikizleniyordu (bir `typedef struct`
   gibi tekrar-tanimaya hassas icerikte risk). Artik bir dosya `#pragma once`
   iceriyorsa YALNIZCA BIR KEZ dahil edilir — C derleyicilerindeki ile ayni
   opt-in davranis. Basligi olmayan dosyalar C semantiginde kalir (her
   include'da tekrar dahil edilir), cunku bazi dosyalar bilerek birden fazla
   kez dahil edilmek ister. */
/* TURN 25 — kayit defteri artik SABIT BOYUTLU DEGIL.
   Eskiden `char *g_once_files[64]` idi ve `include_once_add` defter
   doldugunda SESSIZCE kaydetmeyi birakiyordu: 65. korumali dosyadan
   sonrasi, ikinci kez dahil edilse bile ATLANMIYORDU — yani guard
   sessizce kayboluyordu. Bu tur sessizligin en kotu bicimidir: kod
   calisir, cikti yanlistir, hicbir uyari yoktur. Liste artik buyur;
   bellek, programdaki FARKLI korumali dosya sayisiyla sinirlidir. */
static char **g_once_files = NULL;
static int    g_once_count = 0;
static int    g_once_cap = 0;

static void include_once_reset(void) {
    for (int i = 0; i < g_once_count; i++) free(g_once_files[i]);
    free(g_once_files);
    g_once_files = NULL;
    g_once_count = 0;
    g_once_cap = 0;
}

static int include_once_seen(const char *path) {
    if (!path) return 0;
    for (int i = 0; i < g_once_count; i++) {
        if (g_once_files[i] && strcmp(g_once_files[i], path) == 0) return 1;
    }
    return 0;
}

static void include_once_add(const char *path) {
    if (!path) return;
    if (g_once_count == g_once_cap) {
        int next = g_once_cap > 0 ? g_once_cap * 2 : 16;
        /* realloc basarisizsa ESKI liste korunur (gecici degiskene alinir);
           OOM'da bile kayitli yollar kaybolmaz. */
        char **grown = (char **)realloc(g_once_files, (size_t)next * sizeof(char *));
        if (!grown) return;
        g_once_files = grown;
        g_once_cap = next;
    }
    g_once_files[g_once_count++] = strdup(path);
}

/* Dosyanin `#pragma once` icerip icermedigi (satir basindaki direktif). */
static int file_has_pragma_once(const char *src) {
    if (!src) return 0;
    const char *p = src;
    while (*p) {
        const char *eol = strchr(p, '\n');
        const char *line = p;
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '#') {
            line++;
            while (*line == ' ' || *line == '\t') line++;
            if (strncmp(line, "pragma", 6) == 0) {
                const char *a = line + 6;
                while (*a == ' ' || *a == '\t') a++;
                if (strncmp(a, "once", 4) == 0) {
                    char after = a[4];
                    if (after == '\0' || after == '\n' || after == '\r' ||
                        after == ' ' || after == '\t')
                        return 1;
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return 0;
}
#define MAX_LINE 4096
#define MAX_NATIVE 32
#define MAX_MODULES 8
#define MAX_EXTERN_DLLS 16
#define MAX_EXTERN_REGS 64

typedef struct {
    char *name;
    char *value;          /* object: değer; function: gövde */
    char *params[MAX_MACRO_PARAMS];
    int param_count;
    int is_function;
} Macro;

/* #if/#elif/#else/#endif koşul çerçevesi — active: bu dal aktif, any_taken: bir dal daha önce alındı */
typedef struct {
    int active;
    int any_taken;
} CondFrame;

typedef struct {
    Macro macros[MAX_MACROS];
    int macro_count;
    char *include_dirs[MAX_INCLUDES];
    int include_dir_count;
} Preproc;

static void pp_init(Preproc *pp, const char *base_dir) {
    memset(pp, 0, sizeof(*pp));
    if (base_dir && base_dir[0]) {
        pp->include_dirs[pp->include_dir_count++] = strdup(base_dir);
        /* Proje standart klasörleri: include/ ve lib/ — simple_doc.md seması */
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s/include", base_dir);
        if (pp->include_dir_count < MAX_INCLUDES)
            pp->include_dirs[pp->include_dir_count++] = strdup(dir);
        snprintf(dir, sizeof(dir), "%s/lib", base_dir);
        if (pp->include_dir_count < MAX_INCLUDES)
            pp->include_dirs[pp->include_dir_count++] = strdup(dir);
    }
}

static void pp_free(Preproc *pp) {
    for (int i = 0; i < pp->macro_count; i++) {
        free(pp->macros[i].name);
        free(pp->macros[i].value);
        for (int j = 0; j < pp->macros[i].param_count; j++) free(pp->macros[i].params[j]);
    }
    for (int i = 0; i < pp->include_dir_count; i++) {
        free(pp->include_dirs[i]);
    }
    pp->macro_count = 0;
    pp->include_dir_count = 0;
}

static char *pp_macro_value(Preproc *pp, const char *name) {
    for (int i = 0; i < pp->macro_count; i++) {
        if (strcmp(pp->macros[i].name, name) == 0) return pp->macros[i].value;
    }
    return NULL;
}

static Macro *pp_find_macro(Preproc *pp, const char *name) {
    for (int i = 0; i < pp->macro_count; i++) {
        if (strcmp(pp->macros[i].name, name) == 0) return &pp->macros[i];
    }
    return NULL;
}

/* Noktali ad: A.B.C. Direktiflerde (define/undef/ifdef/ifndef/if/defined)
   ad HER ZAMAN noktalari ile birlikte okunur. */
static const char *pp_scan_greedy_name(const char *p, char *out, size_t out_sz) {
    size_t n = 0;
    const char *q = p;
    if (!out_sz) return p;
    while ((isalnum((unsigned char)*q) || *q == '_') && n + 1 < out_sz) out[n++] = *q++;
    if (n == 0) { out[0] = '\0'; return p; }
    while (*q == '.') {
        const char *r = q + 1;
        char ext[512];
        size_t e = 0;
        while ((isalnum((unsigned char)*r) || *r == '_') && e + 1 < sizeof(ext)) ext[e++] = *r++;
        if (e == 0) break;
        if (n + 1 + e >= out_sz) break;
        out[n++] = '.';
        memcpy(out + n, ext, e);
        n += e;
        q = r;
    }
    out[n] = '\0';
    return q;
}

/* KOD icindeki ad. Noktali bicim YALNIZCA o adla tanimli bir makro varsa
   kabul edilir; yoksa okuma sade adin sonunda durur ve `PLAYER.Position.x`
   gibi uye zincirleri eskisi gibi tek tek gecirilir. */
static const char *pp_scan_macro_ref(Preproc *pp, const char *p, char *out, size_t out_sz) {
    const char *after = pp_scan_greedy_name(p, out, out_sz);
    if (after != p && pp_macro_value(pp, out)) return after;
    {
        size_t n = 0;
        const char *q = p;
        while ((isalnum((unsigned char)*q) || *q == '_') && n + 1 < out_sz) out[n++] = *q++;
        out[n] = '\0';
        return q;
    }
}

static void pp_define(Preproc *pp, const char *name, const char *value) {
    for (int i = 0; i < pp->macro_count; i++) {
        if (strcmp(pp->macros[i].name, name) == 0) {
            free(pp->macros[i].value);
            pp->macros[i].value = strdup(value ? value : "");
            pp->macros[i].is_function = 0;
            pp->macros[i].param_count = 0;
            return;
        }
    }
    if (pp->macro_count < MAX_MACROS) {
        Macro *m = &pp->macros[pp->macro_count];
        m->name = strdup(name);
        m->value = strdup(value ? value : "1");
        m->is_function = 0;
        m->param_count = 0;
        pp->macro_count++;
    }
}

static void pp_define_function(Preproc *pp, const char *name, char **params, int param_count, const char *body) {
    for (int i = 0; i < pp->macro_count; i++) {
        if (strcmp(pp->macros[i].name, name) == 0) {
            Macro *m = &pp->macros[i];
            free(m->value);
            m->value = strdup(body ? body : "");
            for (int j = 0; j < m->param_count; j++) free(m->params[j]);
            m->param_count = param_count < MAX_MACRO_PARAMS ? param_count : MAX_MACRO_PARAMS;
            for (int j = 0; j < m->param_count; j++) m->params[j] = strdup(params[j]);
            m->is_function = 1;
            return;
        }
    }
    if (pp->macro_count < MAX_MACROS) {
        Macro *m = &pp->macros[pp->macro_count++];
        m->name = strdup(name);
        m->value = strdup(body ? body : "");
        m->is_function = 1;
        m->param_count = param_count < MAX_MACRO_PARAMS ? param_count : MAX_MACRO_PARAMS;
        for (int j = 0; j < m->param_count; j++) m->params[j] = strdup(params[j]);
    }
}

static void pp_undef(Preproc *pp, const char *name) {
    for (int i = 0; i < pp->macro_count; i++) {
        if (strcmp(pp->macros[i].name, name) == 0) {
            free(pp->macros[i].name);
            free(pp->macros[i].value);
            for (int j = 0; j < pp->macros[i].param_count; j++) free(pp->macros[i].params[j]);
            memmove(&pp->macros[i], &pp->macros[i + 1], (size_t)(pp->macro_count - i - 1) * sizeof(Macro));
            pp->macro_count--;
            return;
        }
    }
}

/* Function-like macro expansion: args "a, b" → body'deki param adlarını arg'larla değiştir. */
static char *expand_function_macro_alloc(Preproc *pp, Macro *m, const char *args) {
    (void)pp;
    char *arg_list[MAX_MACRO_ARGS];
    int arg_count = 0;
    const char *p = args;
    char tmp[MAX_LINE * 2];
    size_t ti = 0;
    int depth = 0;
    while (*p) {
        if (*p == '(' || *p == '[' || *p == '{') depth++;
        else if (*p == ')' || *p == ']' || *p == '}') depth--;
        if (*p == ',' && depth == 0) {
            tmp[ti++] = '\0';
            if (arg_count < MAX_MACRO_ARGS) arg_list[arg_count++] = strdup(tmp);
            ti = 0;
            p++;
            continue;
        }
        if (ti < sizeof(tmp) - 1) tmp[ti++] = *p++;
    }
    tmp[ti] = '\0';
    if (arg_count < MAX_MACRO_ARGS) arg_list[arg_count++] = strdup(tmp);

    size_t cap = strlen(m->value) * 4 + 256;
    char *out = (char *)malloc(cap);
    if (!out) { for (int i = 0; i < arg_count; i++) free(arg_list[i]); return NULL; }
    size_t o = 0;
    const char *q = m->value;
    while (*q && o < cap - 1) {
        if (isalpha((unsigned char)*q) || *q == '_') {
            const char *st = q;
            while (isalnum((unsigned char)*q) || *q == '_') q++;
            size_t n = (size_t)(q - st);
            char word[256];
            if (n < sizeof(word)) { memcpy(word, st, n); word[n] = '\0'; }
            else word[0] = '\0';   /* B23: taşan ad → güvenli boş (UB okuma yok) */
            int rep = -1;
            for (int i = 0; i < m->param_count; i++) {
                if (strcmp(m->params[i], word) == 0) { rep = i; break; }
            }
            if (rep >= 0 && rep < arg_count) {
                size_t al = strlen(arg_list[rep]);
                if (o + al < cap - 1) { memcpy(out + o, arg_list[rep], al); o += al; }
            } else {
                if (o + n < cap - 1) { memcpy(out + o, st, n); o += n; }
            }
        } else {
            if (o < cap - 1) out[o++] = *q++;
        }
    }
    out[o] = '\0';
    for (int i = 0; i < arg_count; i++) free(arg_list[i]);
    return out;
}

/* B30 — TEK GECIS yerine SABIT NOKTAYA KADAR genisletme. Donus degeri: en az
   bir makro genisletildiyse 1. Cagiran (pp_expand_line) sonucu yeniden tarar;
   boylece `#define TWICE(x) ADD(x,x)` + `TWICE(5)` → `ADD(5,5)` → `((5)+(5))`
   olur. Eskiden `ADD(5,5)` DUZ METIN olarak kaliyor ve "unknown function 'ADD'"
   hatasiyla 0 donuyordu. */
static int pp_expand_pass(Preproc *pp, char *line, size_t line_sz, const char *src) {
    char out[MAX_LINE * 3];
    size_t o = 0;
    const char *p = src;
    int in_str = 0;
    char str_q = 0;
    int expanded_any = 0;
    while (*p && o < sizeof(out) - 1) {
        if (*p == '"' || *p == '\'') {
            if (!in_str) { in_str = 1; str_q = *p; }
            else if (*p == str_q) { in_str = 0; str_q = 0; }
            if (o < sizeof(out) - 1) out[o++] = *p;
            p++;
            continue;
        }
        if (in_str) {
            if (o < sizeof(out) - 1) out[o++] = *p;
            p++;
            continue;
        }
        if (*p == '\\' && p[1]) {
            if (o + 1 < sizeof(out) - 1) { out[o++] = *p; }
            p++;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            char word[512];
            const char *after = pp_scan_macro_ref(pp, p, word, sizeof(word));
            size_t n = (size_t)(after - start);
            Macro *m = pp_find_macro(pp, word);
            if (m && m->is_function) {
                /* Fonksiyon benzeri macro: adın hemen ardından ( argümanları oku */
                const char *q2 = after;
                while (*q2 == ' ' || *q2 == '\t') q2++;
                if (*q2 == '(') {
                    q2++;
                    const char *args_start = q2;
                    int depth = 0;
                    while (*q2) {
                        if (*q2 == '(' || *q2 == '[' || *q2 == '{') depth++;
                        else if (*q2 == ')' || *q2 == ']' || *q2 == '}') {
                            if (depth == 0) break;
                            depth--;
                        }
                        q2++;
                    }
                    /* args: args_start..q2 (exclusive) */
                    size_t arglen = (size_t)(q2 - args_start);
                    char args[MAX_LINE * 2];
                    if (arglen < sizeof(args)) {
                        memcpy(args, args_start, arglen);
                        args[arglen] = '\0';
                        char *expanded = expand_function_macro_alloc(pp, m, args);
                        if (expanded) {
                            size_t el = strlen(expanded);
                            if (o + el < sizeof(out) - 1) {
                                memcpy(out + o, expanded, el);
                                o += el;
                                expanded_any = 1;
                            }
                            free(expanded);
                            p = q2 + 1; /* kapanış ')' sonrası */
                            continue;
                        }
                    }
                }
            }
            if (m && !m->is_function && m->value) {
                size_t vl = strlen(m->value);
                if (o + vl < sizeof(out) - 1) {
                    memcpy(out + o, m->value, vl);
                    o += vl;
                    expanded_any = 1;
                    p = after;
                    continue;
                }
            }
            if (o + n < sizeof(out) - 1) {
                memcpy(out + o, start, n);
                o += n;
            }
            p = after;
        } else {
            if (o < sizeof(out) - 1) out[o++] = *p;
            p++;
        }
    }
    out[o] = '\0';
    snprintf(line, line_sz, "%s", out);
    return expanded_any;
}

/* pp_expand_line — B30: genisletme sonucunu SABIT NOKTAYA kadar yeniden tara.
   Karsilikli makrolar (A→B, B→A) sonsuz donguye girmesin diye tur sayisi
   SINIRLIDIR (C on-isleyicisinin "blue paint" kuralinin pragmatik karsiligi). */
static void pp_expand_line(Preproc *pp, char *line, size_t line_sz, const char *src) {
    char cur[MAX_LINE * 3];
    snprintf(cur, sizeof(cur), "%s", src ? src : "");
    for (int pass = 0; pass < 32; pass++) {
        char next[MAX_LINE * 3];
        if (!pp_expand_pass(pp, next, sizeof(next), cur)) break;  /* genisletme yok → bitti */
        snprintf(cur, sizeof(cur), "%s", next);
    }
    snprintf(line, line_sz, "%s", cur);
}

static int is_preproc_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return *p == '#';
}

static char *resolve_include(Preproc *pp, const char *fname, char *full_buf, size_t full_buf_sz) {
    for (int i = 0; i <= pp->include_dir_count; i++) {
        const char *dir = i < pp->include_dir_count ? pp->include_dirs[i] : "";
        /* Hem '/' hem '\' dene — Windows ve Linux uyumluluğu */
        if (dir[0]) {
            snprintf(full_buf, full_buf_sz, "%s/%s", dir, fname);
        } else {
            snprintf(full_buf, full_buf_sz, "%s", fname);
        }
        FILE *f = fopen(full_buf, "rb");
        if (f) { fclose(f); return full_buf; }
        if (dir[0]) {
            snprintf(full_buf, full_buf_sz, "%s\\%s", dir, fname);
            f = fopen(full_buf, "rb");
            if (f) { fclose(f); return full_buf; }
        }
    }
    return NULL;
}

static char *read_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* Renkli #warning (sarı), #error (kırmızı), #debug (mavi) çıktısı */
static void pp_colored(const char *msg, int kind) {
    /* kind: 0=warning(sarı), 1=error(kırmızı), 2=debug(mavi) */
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    WORD old = 0;
    if (h && h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &csbi)) {
        old = csbi.wAttributes;
    }
    switch (kind) {
        case 0: SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); break; /* sarı */
        case 1: SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY); break;                    /* kırmızı */
        case 2: SetConsoleTextAttribute(h, FOREGROUND_BLUE | FOREGROUND_INTENSITY); break;                  /* mavi */
    }
    fprintf(stderr, "%s", msg);
    if (h && h != INVALID_HANDLE_VALUE && old) SetConsoleTextAttribute(h, old);
    fprintf(stderr, "\n");
#else
    const char *code = kind == 0 ? "\033[33m" : (kind == 1 ? "\033[31m" : "\033[34m");
    fprintf(stderr, "%s%s\033[0m\n", code, msg);
#endif
}

/* ---- #if / #elif ifade değerlendirme (mini Pratt) ---- */

typedef struct {
    Preproc *pp;
    const char *p;
    int err;
} CondParse;

static void cond_skip_ws(CondParse *c) {
    while (*c->p == ' ' || *c->p == '\t') c->p++;
}

static double cond_parse_expr(CondParse *c, int min_prec);

static double cond_parse_primary(CondParse *c) {
    cond_skip_ws(c);
    if (*c->p == '(') {
        c->p++;
        double v = cond_parse_expr(c, 1);
        cond_skip_ws(c);
        if (*c->p == ')') c->p++;
        else c->err = 1;
        return v;
    }
    if (*c->p == '!') { c->p++; return !cond_parse_expr(c, 12); }
    if (*c->p == '-') { c->p++; return -cond_parse_expr(c, 12); }
    if (*c->p == '+') { c->p++; return cond_parse_expr(c, 12); }
    if (*c->p == '~') { c->p++; return ~(long)cond_parse_expr(c, 12); }
    if (isalpha((unsigned char)*c->p) || *c->p == '_') {
        const char *st = c->p;
        char word[512];
        c->p = pp_scan_greedy_name(c->p, word, sizeof(word));
        if (c->p == st) { c->err = 1; return 0; }
        if (strcmp(word, "defined") == 0) {
            cond_skip_ws(c);
            int paren = 0;
            if (*c->p == '(') { paren = 1; c->p++; cond_skip_ws(c); }
            const char *nst = c->p;
            char nword[512];
            c->p = pp_scan_greedy_name(c->p, nword, sizeof(nword));
            if (c->p == nst) c->err = 1;
            if (paren) { cond_skip_ws(c); if (*c->p == ')') c->p++; else c->err = 1; }
            return pp_macro_value(c->pp, nword) ? 1 : 0;
        }
        char *val = pp_macro_value(c->pp, word);
        if (val) {
            double r = 0;
            int neg = 0;
            const char *vp = val;
            while (*vp == ' ' || *vp == '\t') vp++;
            if (*vp == '-') { neg = 1; vp++; while (*vp == ' ' || *vp == '\t') vp++; }
            char *end = NULL;
            if (*vp == '0' && (vp[1]=='x'||vp[1]=='X')) r = (double)strtoll(vp, &end, 16);
            else r = strtod(vp, &end);
            if (end && *end == '\0') return neg ? -r : r;
            /* Macro değeri saf sayı değilse → varlık bazında 1 */
            return pp_macro_value(c->pp, word) ? 1 : 0;
        }
        return 0; /* tanımlı olmayan macro → 0 */
    }
    if (isdigit((unsigned char)*c->p)) {
        if (*c->p == '0' && (c->p[1]=='x'||c->p[1]=='X')) {
            double v = (double)strtoll(c->p, (char**)&c->p, 16);
            return v;
        }
        double v = strtod(c->p, (char**)&c->p);
        return v;
    }
    c->err = 1;
    return 0;
}

static int cond_binop_prec(const char *op) {
    if (strcmp(op, "||") == 0) return 1;
    if (strcmp(op, "&&") == 0) return 2;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 3;
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) return 4;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 5;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) return 6;
    return 0;
}

static double cond_apply_binop(const char *op, double a, double b) {
    if (strcmp(op, "||") == 0) return (a != 0 || b != 0) ? 1 : 0;
    if (strcmp(op, "&&") == 0) return (a != 0 && b != 0) ? 1 : 0;
    if (strcmp(op, "==") == 0) return a == b;
    if (strcmp(op, "!=") == 0) return a != b;
    if (strcmp(op, "<") == 0) return a < b;
    if (strcmp(op, ">") == 0) return a > b;
    if (strcmp(op, "<=") == 0) return a <= b;
    if (strcmp(op, ">=") == 0) return a >= b;
    if (strcmp(op, "+") == 0) return a + b;
    if (strcmp(op, "-") == 0) return a - b;
    if (strcmp(op, "*") == 0) return a * b;
    if (strcmp(op, "/") == 0) return b != 0 ? a / b : 0;
    if (strcmp(op, "%") == 0) return b != 0 ? (double)((long)a % (long)b) : 0;
    return 0;
}

static int match_cond_op(CondParse *c, const char *op) {
    size_t l = strlen(op);
    if (strncmp(c->p, op, l) == 0) { c->p += l; return 1; }
    return 0;
}

static double cond_parse_expr(CondParse *c, int min_prec) {
    double left = cond_parse_primary(c);
    for (;;) {
        cond_skip_ws(c);
        int prec = 0;
        const char *op = NULL;
        if (match_cond_op(c, "||")) { op = "||"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "&&")) { op = "&&"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "==")) { op = "=="; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "!=")) { op = "!="; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "<=")) { op = "<="; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, ">=")) { op = ">="; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "<")) { op = "<"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, ">")) { op = ">"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "+")) { op = "+"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "-")) { op = "-"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "*")) { op = "*"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "/")) { op = "/"; prec = cond_binop_prec(op); }
        else if (match_cond_op(c, "%")) { op = "%"; prec = cond_binop_prec(op); }
        if (!op || prec < min_prec) break;
        double right = cond_parse_expr(c, prec + 1);
        left = cond_apply_binop(op, left, right);
    }
    return left;
}

static int pp_eval_cond(Preproc *pp, const char *expr) {
    CondParse c;
    c.pp = pp;
    c.p = expr;
    c.err = 0;
    double v = cond_parse_expr(&c, 1);
    if (c.err) return 0;
    return v != 0 ? 1 : 0;
}

/* TURN 48 — WHAT A LINE OF THE OUTPUT NEEDS TO REMEMBER ABOUT ITSELF: which
   file it came from, which line of that file, and whether macro expansion
   rewrote its text (the renderer warns about those, because a caret placed on
   rewritten text can only be approximate). */
typedef struct {
    GclSourceMap *map;   /* NULL = nobody asked for a map; then this is free */
    int file;            /* index into the map's file table */
    int line;            /* 1-based line inside that file */
} PpOrigin;

/* B8 — A SKIPPED LINE IS STILL A LINE. Every "do not emit this line" path
   (`#if 0` body, `#pragma once` re-include, inactive `#include`, a `#|...|#`
   block, an empty line, a non-include directive) must still write exactly one
   '\n', or every diagnostic below it reports a line number that is too small.
   The idiom used to be copy-pasted EIGHT times in this function; a ninth skip
   path would have silently dropped it. One helper, one place to read.

   TURN 48 — AND A LINE IS NOT ENOUGH: it has to know WHERE IT CAME FROM. Those
   same eight paths are also the eight places a diagnostic's position is
   decided, and the renderer can only translate a position back into the user's
   own file if the map recorded an origin for the exact line it points at. So
   the origin travels WITH the newline instead of being added next to it: the
   buffer and the map are written by ONE statement, which is what keeps them
   from disagreeing about how many lines exist.    */
static void pp_out_newline(char *out, size_t *out_len, size_t out_cap,
                           const PpOrigin *org, int expanded) {
    if (*out_len + 2 < out_cap) {
        out[(*out_len)++] = '\n';
        out[*out_len] = '\0';
        if (org && org->map)
            gcl_smap_add_line(org->map, org->file, org->line, expanded);
    }
}

/* `include_errors` is an OUT parameter rather than a file-scope counter: the
   recursive call passes the SAME pointer down, so an `#include` that fails
   anywhere in the include tree is reported to the one caller that decides
   whether the build may continue. NULL is accepted (the nested calls always
   forward the outer pointer, so in practice it is never NULL). */
/* `map` may be NULL (no position translation requested) and `file_index` is
   then meaningless; `src` is one file's own text, so its lines are numbered
   from 1 INSIDE THIS CALL. */
static char *pp_process(Preproc *pp, const char *src, int *include_errors,
                        GclSourceMap *map, int file_index) {
    size_t out_cap = strlen(src) + 8192;
    char *out = (char *)malloc(out_cap);
    if (!out) return NULL;
    size_t out_len = 0;
    out[0] = '\0';

    char line[MAX_LINE];
    const char *p = src;
    CondFrame cond_stack[MAX_INCLUDES];
    int inc_depth = 0;
    /* TURN 48 — filled in step with the buffer: src_line counts the lines of
       THIS file, `org` carries that number plus the file's identity to every
       point that emits a newline. */
    PpOrigin org;
    org.map = map;
    org.file = file_index;
    org.line = 1;
    int src_line = 1;

    while (*p) {
        org.line = src_line;
        size_t line_len = 0;
        while (*p && *p != '\n' && *p != '\r' && line_len < sizeof(line) - 1) {
            line[line_len++] = *p++;
        }
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        src_line++;
        line[line_len] = '\0';

        if (!line[0] || line[0] == '\r') {
            pp_out_newline(out, &out_len, out_cap, &org, 0);
            continue;
        }

        /* GCL özel çok satırlı yorum: #| ... |# — satır bazlı işlemede atla.
           # direktifi sanılıp bozulmaması için burada yakalanır.
           Girintili (başında boşluk olan) #| blokları da desteklenir. */
        {
            const char *lq = line;
            while (*lq == ' ' || *lq == '\t') lq++;
            if (lq[0] == '#' && lq[1] == '|') {
                if (!strstr(lq, "|#")) {
                    while (*p) {
                        size_t ll = 0;
                        while (*p && *p != '\n' && *p != '\r' && ll < sizeof(line) - 1) {
                            line[ll++] = *p++;
                        }
                        if (*p == '\r') p++;
                        if (*p == '\n') p++;
                        src_line++;   /* a consumed line is a line, even in a comment */
                        line[ll] = '\0';
                        if (strstr(line, "|#")) break;
                    }
                }
                /* The whole block is ONE line of output, anchored at the line
                   the block STARTED on, which is the one org.line still holds. */
                pp_out_newline(out, &out_len, out_cap, &org, 0);
                continue;
            }
        }

        if (is_preproc_line(line)) {
            /* B8 — DIREKTIF SATIRI DA BIR YER KAPLAR.
               Eskiden her direktif dali (`#native`, `#define`, `#if`, `#endif`,
               `#undef`, `#pragma`, `#error`, ...) `continue` ile cikiyor ve
               HICBIR '\n' yazmiyordu. Her direktif onundeki satir sayisini 1
               azalttigi icin TUM sonraki tanilama satir numaralari kayiyordu:
               `_temp/bug_06b_diag.gcsf` kaynak 5. satirdaki hatayi "2:13"
               olarak bildiriyordu (olmasi gereken 5. satir).
               #include KENDI satirlarini zaten ekledigi icin ona fazladan
               '\n' yazilmaz; diger tum direktifler icin yazilir. */
            {
                const char *dq = line;
                while (*dq == ' ' || *dq == '\t') dq++;
                if (*dq == '#') dq++;
                while (*dq == ' ' || *dq == '\t') dq++;
                if (strncmp(dq, "include", 7) != 0) {
                    pp_out_newline(out, &out_len, out_cap, &org, 0);
                }
            }
            char *q = line;
            while (*q == ' ' || *q == '\t') q++;
            q++;
            while (*q == ' ' || *q == '\t') q++;

            if (strncmp(q, "include", 7) == 0) {
                /* TURN 24 — `#if` ETKINLIGI. Bu dal eskiden kosul durumunu
                   HIC kontrol etmiyordu: `#if 0 ... #include <x> ... #endif`
                   yazan bir program dosyayi YINE de dahil ediyordu, dosya
                   yoksa da "could not be found" hatasi basiyordu. C'de
                   etkin olmayan bir dala yazilmis `#include` hicbir sekilde
                   cozumlenmez. `#error`/`#warning`/`#debug` bu kontrolu zaten
                   yapiyordu; `#include` yapmiyordu. */
                int inc_active = 1;
                for (int i = 0; i < inc_depth; i++) {
                    if (!cond_stack[i].active) { inc_active = 0; break; }
                }
                if (!inc_active) {
                    /* Atlanan include de BIR YER KAPLAR (B8): satir sayisi
                       kaymasin diye bos satir yazilir. */
                    pp_out_newline(out, &out_len, out_cap, &org, 0);
                    continue;
                }
                q += 7;
                while (*q == ' ' || *q == '\t') q++;
                /* Hem <file.gcsf> hem "file.gcsf" formunu destekle */
                char fname[512];
                size_t fi = 0;
                if (*q == '<') {
                    q++;
                    while (*q && *q != '>' && fi < sizeof(fname) - 1) fname[fi++] = *q++;
                } else if (*q == '"') {
                    q++;
                    while (*q && *q != '"' && fi < sizeof(fname) - 1) fname[fi++] = *q++;
                }
                fname[fi] = '\0';
                if (fname[0]) {
                    char full[4096];
                    /* B21: olmayan #include eskiden SESSİZCE yok sayılıyordu
                       (gcl_diag.h'deki GCL_E_PP_INCLUDE_NOT_FOUND kodu hiç
                       üretilmiyordu) — kullanıcı dosyanın dahil edildiğini
                       sanıyordu. Artık aranan yerlerle birlikte bildirilir. */
                    const char *resolved_inc = resolve_include(pp, fname, full, sizeof(full));
                    if (!resolved_inc) {
                        fprintf(stderr, "Error: #include '%s' could not be found "
                                        "(looked in the source directory, include/ and lib/)\n",
                                fname);
                        if (include_errors) (*include_errors)++;
                    }
                    if (resolved_inc) {
                        if (g_include_depth >= GCL_MAX_INCLUDE_DEPTH) {
                            fprintf(stderr, "Error: #include depth limit exceeded (%d) including '%s'\n",
                                    GCL_MAX_INCLUDE_DEPTH, fname);
                            if (include_errors) (*include_errors)++;
                        } else {
                        char *inc_src = read_text_file(full);
                        if (inc_src) {
                            /* B28 — `#pragma once`: dosya bir kez dahil edildikten
                               sonra ikinci `#include` cagrisi tamamen ATLANIR
                               (icerik ikizlenmez). `#pragma once` icermeyen
                               dosyalar C semantiginde kalir. */
                            if (file_has_pragma_once(inc_src)) {
                                if (include_once_seen(full)) {
                                    /* TURN 24 — atlanan include de BIR YER
                                       KAPLAR (B8): satir sayisi kaymasin. */
                                    pp_out_newline(out, &out_len, out_cap, &org, 0);
                                    free(inc_src);
                                    continue;
                                }
                                include_once_add(full);
                            }
                            /* TURN 48 — the included file gets its OWN identity
                               in the map, so a line pasted from a header is
                               reported as a line of THAT file, not of the file
                               that pulled it in. `add_file` COPIES the text, so
                               the copy outlives the `free(inc_src)` below. */
                            int inc_file = org.file;
                            if (org.map) {
                                inc_file = gcl_smap_find_file(org.map, full);
                                if (inc_file < 0)
                                    inc_file = gcl_smap_add_file(org.map, full, inc_src,
                                                                 strlen(inc_src));
                            }
                            int origins_before = gcl_smap_line_count(org.map);
                            g_include_depth++;
                            /* The SAME pointer travels down the include tree, so
                               a failure in a nested file reaches the one caller
                               that decides whether the build stops. */
                            char *processed = pp_process(pp, inc_src, include_errors,
                                                         org.map, inc_file);
                            g_include_depth--;
                            free(inc_src);
                            if (processed) {
                                size_t plen = strlen(processed);
                                if (out_len + plen + 2 < out_cap) {
                                    memcpy(out + out_len, processed, plen);
                                    out_len += plen;
                                    pp_out_newline(out, &out_len, out_cap, &org, 0);
                                } else {
                                    /* The paste did not fit, so those lines are
                                       NOT in the buffer. The map must not keep
                                       them either, or every line after the
                                       include is off by what was dropped. */
                                    gcl_smap_truncate(org.map, origins_before);
                                }
                                free(processed);
                            }
                        }
                        }
                    }
                }
                continue;
            }
            if (strncmp(q, "define", 6) == 0) {
                q += 6;
                while (*q == ' ' || *q == '\t') q++;
                char name[512];
                q = (char *)pp_scan_greedy_name(q, name, sizeof(name));
                /* Function-like macro: adın hemen ardından ( varsa parametreler */
                char *save = q;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '(') {
                    q++;
                    char *params[MAX_MACRO_PARAMS];
                    int pc = 0;
                    char pname[256];
                    size_t pi = 0;
                    while (*q) {
                        if (*q == ',') {
                            if (pi) { pname[pi] = '\0'; if (pc < MAX_MACRO_PARAMS) params[pc++] = strdup(pname); pi = 0; }
                            q++;
                        } else if (*q == ')') {
                            if (pi) { pname[pi] = '\0'; if (pc < MAX_MACRO_PARAMS) params[pc++] = strdup(pname); pi = 0; }
                            q++;
                            break;
                        } else {
                            if (isalnum((unsigned char)*q) || *q == '_') {
                                if (pi < sizeof(pname) - 1) pname[pi++] = *q;
                            }
                            q++;
                        }
                    }
                    while (*q == ' ' || *q == '\t') q++;
                    pp_define_function(pp, name, params, pc, q);
                    for (int i = 0; i < pc; i++) free(params[i]);
                } else {
                    q = save;
                    while (*q == ' ' || *q == '\t') q++;
                    pp_define(pp, name, q);
                }
                continue;
            }
            if (strncmp(q, "undef", 5) == 0) {
                q += 5;
                while (*q == ' ' || *q == '\t') q++;
                char name[512];
                q = (char *)pp_scan_greedy_name(q, name, sizeof(name));
                pp_undef(pp, name);
                continue;
            }
            if (strncmp(q, "ifdef", 5) == 0) {
                q += 5;
                while (*q == ' ' || *q == '\t') q++;
                char name[512];
                q = (char *)pp_scan_greedy_name(q, name, sizeof(name));
                int cond = pp_macro_value(pp, name) ? 1 : 0;
                int parent = (inc_depth > 0) ? cond_stack[inc_depth - 1].active : 1;
                if (inc_depth < MAX_INCLUDES) {
                    cond_stack[inc_depth].active = parent && cond;
                    cond_stack[inc_depth].any_taken = (parent && cond) ? 1 : 0;
                    inc_depth++;
                }
                continue;
            }
            if (strncmp(q, "ifndef", 6) == 0) {
                q += 6;
                while (*q == ' ' || *q == '\t') q++;
                char name[512];
                q = (char *)pp_scan_greedy_name(q, name, sizeof(name));
                int cond = pp_macro_value(pp, name) ? 0 : 1;
                int parent = (inc_depth > 0) ? cond_stack[inc_depth - 1].active : 1;
                if (inc_depth < MAX_INCLUDES) {
                    cond_stack[inc_depth].active = parent && cond;
                    cond_stack[inc_depth].any_taken = (parent && cond) ? 1 : 0;
                    inc_depth++;
                }
                continue;
            }
            if (strncmp(q, "if", 2) == 0) {
                q += 2;
                while (*q == ' ' || *q == '\t') q++;
                int cond = pp_eval_cond(pp, q);
                int parent = (inc_depth > 0) ? cond_stack[inc_depth - 1].active : 1;
                if (inc_depth < MAX_INCLUDES) {
                    cond_stack[inc_depth].active = parent && cond;
                    cond_stack[inc_depth].any_taken = (parent && cond) ? 1 : 0;
                    inc_depth++;
                }
                continue;
            }
            if (strncmp(q, "elif", 4) == 0) {
                q += 4;
                while (*q == ' ' || *q == '\t') q++;
                if (inc_depth > 0) {
                    CondFrame *f = &cond_stack[inc_depth - 1];
                    int parent = (inc_depth > 1) ? cond_stack[inc_depth - 2].active : 1;
                    if (f->any_taken) f->active = 0;
                    else {
                        int cond = pp_eval_cond(pp, q);
                        f->active = parent && cond;
                        f->any_taken = f->active ? 1 : 0;
                    }
                }
                continue;
            }
            if (strncmp(q, "else", 4) == 0) {
                if (inc_depth > 0) {
                    CondFrame *f = &cond_stack[inc_depth - 1];
                    int parent = (inc_depth > 1) ? cond_stack[inc_depth - 2].active : 1;
                    if (f->any_taken) f->active = 0;
                    else { f->active = parent; f->any_taken = 1; }
                }
                continue;
            }
            if (strncmp(q, "endif", 5) == 0) {
                if (inc_depth > 0) inc_depth--;
                continue;
            }
            if (strncmp(q, "warning", 7) == 0) {
                q += 7;
                while (*q == ' ' || *q == '\t') q++;
                int act = 1;
                for (int i = 0; i < inc_depth; i++) if (!cond_stack[i].active) { act = 0; break; }
                if (act) {
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "[gcl] #warning: %s", q);
                    pp_colored(buf, 0);
                }
                continue;
            }
            if (strncmp(q, "error", 5) == 0) {
                q += 5;
                while (*q == ' ' || *q == '\t') q++;
                int act = 1;
                for (int i = 0; i < inc_depth; i++) if (!cond_stack[i].active) { act = 0; break; }
                if (act) {
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "[gcl] #error: %s", q);
                    pp_colored(buf, 1);
                    /* C semantigi: derleme burada durur. */
                    g_pp_error_hits++;
                }
                continue;
            }
            if (strncmp(q, "debug", 5) == 0) {
                q += 5;
                while (*q == ' ' || *q == '\t') q++;
                int act = 1;
                for (int i = 0; i < inc_depth; i++) if (!cond_stack[i].active) { act = 0; break; }
                if (act) {
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "[gcl] #debug: %s", q);
                    pp_colored(buf, 2);
                }
                continue;
            }
            continue;
        }

        int active = 1;
        for (int i = 0; i < inc_depth; i++) {
            if (!cond_stack[i].active) { active = 0; break; }
        }
        if (!active) {
            pp_out_newline(out, &out_len, out_cap, &org, 0);
            continue;
        }

        char expanded[MAX_LINE * 2];
        pp_expand_line(pp, expanded, sizeof(expanded), line);
        size_t elen = strlen(expanded);
        if (out_len + elen + 2 < out_cap) {
            memcpy(out + out_len, expanded, elen);
            out_len += elen;
            /* The text differs from the line the user typed exactly when a
               macro rewrote it, and THAT is the flag the renderer turns into
               "(macro-expanded line: caret is approximate)". Without it the
               frame would present expanded text as if the user had written it. */
            pp_out_newline(out, &out_len, out_cap, &org,
                           strcmp(line, expanded) != 0);
        }
    }

    return out;
}

/* #native <Modul> satırlarını kaynaktan topla (sadece ad) */
static void collect_native(const char *src, const char **modules, int *count) {
    *count = 0;
    const char *p = src;
    while (*p) {
        char line[MAX_LINE];
        size_t line_len = 0;
        while (*p && *p != '\n' && line_len < sizeof(line) - 1) {
            line[line_len++] = *p++;
        }
        if (*p == '\n') p++;
        line[line_len] = '\0';

        if (strstr(line, "#native") == line) {
            char *lt = strchr(line, '<');
            if (lt) {
                char name[64];
                size_t ni = 0;
                lt++;
                while (*lt && *lt != '>' && ni < sizeof(name) - 1) name[ni++] = *lt++;
                name[ni] = '\0';
                int exists = 0;
                for (int i = 0; i < *count; i++) {
                    if (strcmp(modules[i], name) == 0) { exists = 1; break; }
                }
                if (!exists && *count < MAX_MODULES) {
                    /* strdup() can FAIL. The old code stored the NULL and still
                       incremented the count, so the very next `#native` line ran
                       the dedup loop above and handed that NULL to strcmp() - a
                       NULL dereference (CWE-690, found by -fanalyzer). Only a
                       name that was really STORED is counted, so a failed
                       duplicate is skipped exactly like a repeated module. */
                    char *dup = strdup(name);
                    if (dup) { modules[*count] = dup; (*count)++; }
                }
            }
        }
    }
}

/* #extern <dll> satırlarını topla (string dizisine) */
static void collect_extern(const char *src, const char **dlls, int *count) {
    *count = 0;
    const char *p = src;
    while (*p) {
        char line[MAX_LINE];
        size_t line_len = 0;
        while (*p && *p != '\n' && line_len < sizeof(line) - 1) {
            line[line_len++] = *p++;
        }
        if (*p == '\n') p++;
        line[line_len] = '\0';
        if (strstr(line, "#extern") == line) {
            char *lt = strchr(line, '<');
            if (lt) {
                char name[256];
                size_t ni = 0;
                lt++;
                while (*lt && *lt != '>' && ni < sizeof(name) - 1) name[ni++] = *lt++;
                name[ni] = '\0';
                int exists = 0;
                for (int i = 0; i < *count; i++) {
                    if (strcmp(dlls[i], name) == 0) { exists = 1; break; }
                }
                if (!exists && *count < MAX_EXTERN_DLLS) {
                    /* Same defect as collect_native above: the NULL from a
                       failed strdup was stored AND counted, so the next
                       `#extern` line passed it to strcmp() (CWE-690). */
                    char *dup = strdup(name);
                    if (dup) { dlls[*count] = dup; (*count)++; }
                }
            }
        }
    }
}

/* #register <ret> <func>(<params>); — harici fonksiyon imzasını topla.
   Satır formatı: "#register void InitWindow(int width, int height, const char *title);" */
static void collect_register(const char *src, const char **recs, int *count) {
    *count = 0;
    const char *p = src;
    while (*p) {
        char line[MAX_LINE];
        size_t line_len = 0;
        while (*p && *p != '\n' && line_len < sizeof(line) - 1) {
            line[line_len++] = *p++;
        }
        if (*p == '\n') p++;
        line[line_len] = '\0';
        if (strstr(line, "#register") == line) {
            char *q = line + 9;
            while (*q == ' ' || *q == '\t') q++;
            /* return type: void | int | double | float | char */
            char ret[32];
            size_t ri = 0;
            while (*q && *q != ' ' && *q != '\t' && ri < sizeof(ret) - 1) ret[ri++] = *q++;
            ret[ri] = '\0';
            while (*q == ' ' || *q == '\t') q++;
            /* func name */
            char fname[128];
            size_t fi = 0;
            while (*q && *q != '(' && fi < sizeof(fname) - 1) fname[fi++] = *q++;
            fname[fi] = '\0';
            /* strip trailing spaces */
            while (fi > 0 && (fname[fi-1] == ' ' || fname[fi-1] == '\t')) fname[--fi] = '\0';
            if (!*q) continue;
            q++; /* ( */
            /* params: "int width, int height, const char *title" */
            /* Kayıt dizesi: ret|func|params — runner bunu parse eder */
            char buf[512];
            snprintf(buf, sizeof(buf), "%s|%s|%s", ret, fname, q);
            if (*count < MAX_EXTERN_REGS) {
                /* Third site of the same defect: a failed strdup stored a NULL
                   that every consumer of `recs` (the runner parses each record
                   as "ret|func|params") would dereference. Only a record that
                   was really stored is counted. */
                char *dup = strdup(buf);
                if (dup) { recs[*count] = dup; (*count)++; }
            }
        }
    }
}

int gcl_simple_run_source(const char *src, size_t len, const char *file_name,
                          const char *base_dir, int argc, char **argv) {
    if (!src) return -1;

    /* B28/B20 — koşu (run) başına bir kez: önceki koşudan kalan `#pragma once`
       listesi ve include derinliği temizlenir. Aynı süreçte birden fazla dosya
       çalıştırıldığında (IDE) durum sızmasın. */
    include_once_reset();
    g_include_depth = 0;
    g_pp_error_hits = 0;

    /* #pragma commandline — terminal uygulaması. Konsol yoksa (IDE/GUI çocuk
       süreci) program OTOMATİK olarak yeni bir terminal penceresinde yeniden
       başlatılır: 11_scanf_commandline_test.gcsf gibi Stdio.scanf kullanan
       programlar IDE içinden de çalışır. */
    if (gcl_terminal_pragma_present(src)) {
        if (gcl_terminal_enter()) return 0;
    }

    /* #native modül adlarını topla */
    const char *native_modules[MAX_MODULES] = {0};
    int native_count = 0;
    collect_native(src, native_modules, &native_count);

    /* #extern DLL'leri ve #register fonksiyonlarını topla */
    const char *extern_dlls[MAX_EXTERN_DLLS] = {0};
    int extern_dll_count = 0;
    collect_extern(src, extern_dlls, &extern_dll_count);

    const char *extern_regs[MAX_EXTERN_REGS] = {0};
    int extern_reg_count = 0;
    collect_register(src, extern_regs, &extern_reg_count);

    Preproc pp;
    pp_init(&pp, base_dir);

    /* TURN 48 — THE MAP THAT MAKES A POSITION MEANINGFUL. The lexer and the
       parser only ever see `processed`, so EVERY line number they report is a
       line of THAT buffer. Without this map the CLI had nothing to translate
       them with and printed them as if they were the user's own: a parse error
       inside an #include came out at the line the header happened to land on,
       naming the file that included it (measured: `main.gcsf:5` for an error
       really at `bad.gcsf:2`), and a macro-expanded line was quoted as if the
       user had typed the expansion. The map is filled by pp_process in step
       with the buffer and read by the renderer, which is the only consumer
       that knows how to read it. */
    GclSourceMap smap;
    gcl_smap_init(&smap);
    gcl_smap_add_file(&smap, file_name, src, strlen(src));

    /* TURN 27 — the include failure is an OUT value rather than a global: this
       is the only place that decides whether the build may continue, and the
       value cannot be left behind by an earlier run. */
    int include_errors = 0;
    char *processed = pp_process(&pp, src, &include_errors, &smap, 0);
    pp_free(&pp);
    if (!processed) { gcl_smap_free(&smap); return -1; }

    /* Aktif bir `#error` gorulduyse derleme BASARISIZDIR (C'deki gibi):
       hicbir deyim calismaz — stdout bos kalir — ve cikis kodu != 0 olur.
       Onceden yalnizca kirmizi bir satir basilir, program yine calisirdi. */
    if (g_pp_error_hits > 0) {
        fprintf(stderr, "Error: #error directive stopped compilation\n");
        gcl_smap_free(&smap);
        free(processed);
        return -1;
    }

    /* TURN 24 — basarisiz bir `#include` de derlemeyi DURDURUR (C semantigi).
       Eskiden yalnizca bir satir basilir, program yine derlenip calisirdi ve
       cikis kodu 0 olurdu. */
    if (include_errors > 0) {
        fprintf(stderr, "Error: #include failed, compilation stopped\n");
        gcl_smap_free(&smap);
        free(processed);
        return -1;
    }

    /* TURN 47 — THE SINK IS REAL NOW, and this is the caller that makes the
       whole rich pipeline (codes, spans, source frames, colour) reachable in
       production instead of only from tests. `gcl_lex_diag` / `gcl_parse_diag`
       are the sink-aware twins of the legacy wrappers, so the sentence a user
       reads is the diagnostic ITSELF: the message, its CODE and the position
       it points at cannot drift apart, and the header is the shape editors
       parse (`file:line:col: severity: message [CODE]`) followed by the
       offending line with a caret under the exact span.

       `file_name` is what makes that header useful; with NULL the renderer
       would print the `<input>` placeholder. It is borrowed, not owned, so it
       must outlive this call - the two callers pass the script path.

       The legacy wrappers are still used by gcl_lex/gcl_parse internally, so
       nothing about the old flat sentence was deleted; the CLI simply reports
       through the richer door now. */
    GclDiagList diags;
    gcl_diag_list_init(&diags, file_name);

    /* DİKKAT: token'ların lexeme pointer'ları processed buffer'ına işaret eder.
       Bu yüzden processed, gcl_run_program bitene kadar free edilmemeli. */
    GclTokenList *tokens = gcl_lex_diag(processed, strlen(processed), &diags);
    if (!tokens) {
        /* TURN 48 — print through the MAP, not through the preprocessed buffer.
           `src` is the fallback for a position the map cannot resolve; when it
           can, the header names the file the user actually wrote and the frame
           quotes the text they actually typed. */
        gcl_diag_list_print_mapped(stderr, &diags, &smap, src, strlen(src));
        gcl_diag_list_free(&diags);
        gcl_smap_free(&smap);
        free(processed);
        return -1;
    }

    GclProgram *prog = gcl_parse_diag(tokens, &diags);
    if (!prog) {
        gcl_diag_list_print_mapped(stderr, &diags, &smap, src, strlen(src));
        gcl_diag_list_free(&diags);
        gcl_smap_free(&smap);
        gcl_token_free(tokens);
        free(processed);
        return -1;
    }
    /* Parsed clean: nothing to report, and nothing left to translate. The list
       is emptied either way, so a warning added by an earlier stage can never
       leak into the next run. */
    gcl_diag_list_free(&diags);
    gcl_smap_free(&smap);

    int rc = gcl_run_program(prog, base_dir, native_modules, native_count,
                             extern_dlls, extern_dll_count,
                             extern_regs, extern_reg_count, argc, argv);

    for (int i = 0; i < native_count; i++) {
        free((char *)native_modules[i]);
    }
    for (int i = 0; i < extern_dll_count; i++) {
        free((char *)extern_dlls[i]);
    }
    for (int i = 0; i < extern_reg_count; i++) {
        free((char *)extern_regs[i]);
    }

    gcl_program_free(prog);
    gcl_token_free(tokens);
    free(processed);
    return rc;
}
