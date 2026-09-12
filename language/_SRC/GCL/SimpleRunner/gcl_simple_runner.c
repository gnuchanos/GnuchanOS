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

static void pp_expand_line(Preproc *pp, char *line, size_t line_sz, const char *src) {
    char out[MAX_LINE * 3];
    size_t o = 0;
    const char *p = src;
    int in_str = 0;
    char str_q = 0;
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
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t n = (size_t)(p - start);
            char word[256];
            if (n < sizeof(word)) {
                memcpy(word, start, n);
                word[n] = '\0';
            }
            Macro *m = pp_find_macro(pp, word);
            if (m && m->is_function) {
                /* Fonksiyon benzeri macro: adın hemen ardından ( argümanları oku */
                const char *q2 = p;
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
                    continue;
                }
            }
            if (o + n < sizeof(out) - 1) {
                memcpy(out + o, start, n);
                o += n;
            }
        } else {
            if (o < sizeof(out) - 1) out[o++] = *p;
            p++;
        }
    }
    out[o] = '\0';
    snprintf(line, line_sz, "%s", out);
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
        while (isalnum((unsigned char)*c->p) || *c->p == '_') c->p++;
        size_t n = (size_t)(c->p - st);
        char word[256];
        if (n < sizeof(word)) { memcpy(word, st, n); word[n] = '\0'; }
        if (strcmp(word, "defined") == 0) {
            cond_skip_ws(c);
            int paren = 0;
            if (*c->p == '(') { paren = 1; c->p++; cond_skip_ws(c); }
            const char *nst = c->p;
            while (isalnum((unsigned char)*c->p) || *c->p == '_') c->p++;
            size_t nn = (size_t)(c->p - nst);
            char nword[256];
            if (nn < sizeof(nword)) { memcpy(nword, nst, nn); nword[nn] = '\0'; }
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

static char *pp_process(Preproc *pp, const char *src, int *stack_pos) {
    (void)stack_pos;
    size_t out_cap = strlen(src) + 8192;
    char *out = (char *)malloc(out_cap);
    if (!out) return NULL;
    size_t out_len = 0;
    out[0] = '\0';

    char line[MAX_LINE];
    const char *p = src;
    CondFrame cond_stack[MAX_INCLUDES];
    int inc_depth = 0;

    while (*p) {
        size_t line_len = 0;
        while (*p && *p != '\n' && *p != '\r' && line_len < sizeof(line) - 1) {
            line[line_len++] = *p++;
        }
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        line[line_len] = '\0';

        if (!line[0] || line[0] == '\r') {
            if (out_len + 2 < out_cap) { out[out_len++] = '\n'; out[out_len] = '\0'; }
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
                        line[ll] = '\0';
                        if (strstr(line, "|#")) break;
                    }
                }
                if (out_len + 2 < out_cap) { out[out_len++] = '\n'; out[out_len] = '\0'; }
                continue;
            }
        }

        if (is_preproc_line(line)) {
            char *q = line;
            while (*q == ' ' || *q == '\t') q++;
            q++;
            while (*q == ' ' || *q == '\t') q++;

            if (strncmp(q, "include", 7) == 0) {
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
                    if (resolve_include(pp, fname, full, sizeof(full))) {
                        char *inc_src = read_text_file(full);
                        if (inc_src) {
                            char *processed = pp_process(pp, inc_src, NULL);
                            free(inc_src);
                            if (processed) {
                                size_t plen = strlen(processed);
                                if (out_len + plen + 2 < out_cap) {
                                    memcpy(out + out_len, processed, plen);
                                    out_len += plen;
                                    if (out_len + 2 < out_cap) { out[out_len++] = '\n'; out[out_len] = '\0'; }
                                }
                                free(processed);
                            }
                        }
                    }
                }
                continue;
            }
            if (strncmp(q, "define", 6) == 0) {
                q += 6;
                while (*q == ' ' || *q == '\t') q++;
                char name[256];
                size_t ni = 0;
                while (isalnum((unsigned char)*q) || *q == '_') name[ni++] = *q++;
                name[ni] = '\0';
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
                char name[256];
                size_t ni = 0;
                while (isalnum((unsigned char)*q) || *q == '_') name[ni++] = *q++;
                name[ni] = '\0';
                pp_undef(pp, name);
                continue;
            }
            if (strncmp(q, "ifdef", 5) == 0) {
                q += 5;
                while (*q == ' ' || *q == '\t') q++;
                char name[256];
                size_t ni = 0;
                while (isalnum((unsigned char)*q) || *q == '_') name[ni++] = *q++;
                name[ni] = '\0';
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
                char name[256];
                size_t ni = 0;
                while (isalnum((unsigned char)*q) || *q == '_') name[ni++] = *q++;
                name[ni] = '\0';
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
            if (out_len + 2 < out_cap) { out[out_len++] = '\n'; out[out_len] = '\0'; }
            continue;
        }

        char expanded[MAX_LINE * 2];
        pp_expand_line(pp, expanded, sizeof(expanded), line);
        size_t elen = strlen(expanded);
        if (out_len + elen + 2 < out_cap) {
            memcpy(out + out_len, expanded, elen);
            out_len += elen;
            if (out_len + 2 < out_cap) { out[out_len++] = '\n'; out[out_len] = '\0'; }
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
                    modules[*count] = strdup(name);
                    (*count)++;
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
                    dlls[*count] = strdup(name);
                    (*count)++;
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
                recs[*count] = strdup(buf);
                (*count)++;
            }
        }
    }
}

int gcl_simple_run_source(const char *src, size_t len, const char *base_dir,
                          int argc, char **argv) {
    if (!src) return -1;

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

    char *processed = pp_process(&pp, src, NULL);
    pp_free(&pp);
    if (!processed) return -1;

    char *lex_err = NULL;
    /* DİKKAT: token'ların lexeme pointer'ları processed buffer'ına işaret eder.
       Bu yüzden processed, gcl_run_program bitene kadar free edilmemeli. */
    GclTokenList *tokens = gcl_lex(processed, strlen(processed), &lex_err);
    if (!tokens) {
        fprintf(stderr, "Error: %s\n", lex_err ? lex_err : "lexer error");
        free(lex_err);
        free(processed);
        return -1;
    }

    char *parse_err = NULL;
    GclProgram *prog = gcl_parse(tokens, &parse_err);
    if (!prog) {
        fprintf(stderr, "Error: %s\n", parse_err ? parse_err : "parse error");
        free(parse_err);
        gcl_token_free(tokens);
        return -1;
    }

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
