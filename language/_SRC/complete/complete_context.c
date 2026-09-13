/*
 * complete_context.c — İmleç bağlam motoru.
 *
 * §5.2 adımları:
 *   1) Lexical tarama (buffer başından): string/char, // , / * * /, GCL #| |#.
 *   2) Anchor/önek çıkarımı.
 *   3) Zincir çözümleme (çok seviyeli, '()' çağrıları dahil).
 *   4) Sözdizim bağlamı (preproc / member / args / value-assign / type-decl).
 */
#include "complete_context.h"
#include <stdio.h>

/* Buffer başından imlece kadar lexical tarama + parantez derinliği.
   `quote_pos`: AÇIK tırnağın konumu (yoksa -1) — string-yol bağlamı için. */
static void scan_lex(const char *text, size_t cursor,
                     int *in_string, int *in_comment, int *paren_depth,
                     long *last_open_brace, long *quote_pos) {
    int block = 0, line_comment = 0, quote = 0, depth = 0;
    long brace = -1, qpos = -1;
    for (size_t i = 0; i < cursor; i++) {
        char c = text[i];
        if (line_comment) { if (c == '\n') line_comment = 0; continue; }
        if (block == 1) {
            if (c == '*' && i + 1 < cursor && text[i + 1] == '/') { block = 0; i++; }
            continue;
        }
        if (block == 2) {
            if (c == '|' && i + 1 < cursor && text[i + 1] == '#') { block = 0; i++; }
            continue;
        }
        if (quote) {
            if (c == '\\' && i + 1 < cursor) { i++; continue; }
            if (c == quote) { quote = 0; qpos = -1; }
            continue;
        }
        if (c == '/' && i + 1 < cursor && text[i + 1] == '/') { line_comment = 1; i++; continue; }
        if (c == '/' && i + 1 < cursor && text[i + 1] == '*') { block = 1; i++; continue; }
        if (c == '#' && i + 1 < cursor && text[i + 1] == '|') { block = 2; i++; continue; }
        if (c == '"' || c == '\'') { quote = c; qpos = (long)i; continue; }
        if (c == '(') depth++;
        else if (c == ')') { if (depth > 0) depth--; }
        else if (c == '{') brace = (long)i;
        else if (c == '}') brace = -1;
    }
    if (in_string)  *in_string  = quote != 0;
    if (in_comment) *in_comment = (block != 0) || line_comment;
    if (paren_depth) *paren_depth = depth;
    if (last_open_brace) *last_open_brace = brace;
    if (quote_pos) *quote_pos = quote ? qpos : -1;
}

static void line_bounds(const char *text, size_t len, size_t cursor,
                        size_t *ls, size_t *le) {
    size_t s = cursor;
    while (s > 0 && text[s - 1] != '\n') s--;
    size_t e = cursor;
    while (e < len && text[e] != '\n') e++;
    *ls = s; *le = e;
}

/* İmleçten geriye [A-Za-z0-9_] dizisi (önek). */
static void read_prefix(const char *text, size_t ls, size_t cursor,
                        size_t *begin, char *out, size_t outsz) {
    size_t b = cursor;
    while (b > ls && gclc_ident_char(text[b - 1])) b--;
    size_t n = cursor - b;
    if (n >= outsz) { b = cursor - (outsz - 1); n = outsz - 1; }
    if (n) memcpy(out, text + b, n);
    out[n] = '\0';
    *begin = b;
}

/* Zinciri imlecin solundaki '.' 'ler üzerinden geriye doğru çöz.
   'Raylib' ve 'GetMousePosition()' gibi '()' içeren tabanları da tanır. */
static void parse_chain(const char *text, size_t ls, size_t dot_index,
                        GclContext *ctx) {
    char names[GCLC_MAX_CHAIN][GCLC_CHAIN_NAME];
    int  calls[GCLC_MAX_CHAIN];
    int  n = 0;
    size_t i = dot_index; /* i = '.' konumu */
    while (n < GCLC_MAX_CHAIN) {
        if (i <= ls) break;
        size_t end = i; /* '.' konumu */
        while (end > ls && (text[end - 1] == ' ' || text[end - 1] == '\t')) end--;
        if (end <= ls) break;

        size_t idstart, idend;
        int is_call = 0;
        if (text[end - 1] == ')') {
            int d = 1;
            size_t p = end - 1;
            while (p > ls && d > 0) {
                p--;
                if (text[p] == ')') d++;
                else if (text[p] == '(') d--;
            }
            if (d != 0) break;
            size_t e2 = p;
            while (e2 > ls && (text[e2 - 1] == ' ' || text[e2 - 1] == '\t')) e2--;
            idend = e2;
            idstart = idend;
            while (idstart > ls && gclc_ident_char(text[idstart - 1])) idstart--;
            is_call = 1;
        } else {
            idend = end;
            idstart = idend;
            while (idstart > ls && gclc_ident_char(text[idstart - 1])) idstart--;
        }
        if (idstart == idend) break;

        size_t nl = idend - idstart;
        if (nl >= GCLC_CHAIN_NAME) nl = GCLC_CHAIN_NAME - 1;
        memcpy(names[n], text + idstart, nl);
        names[n][nl] = '\0';
        calls[n] = is_call;
        n++;

        size_t nd = idstart;
        while (nd > ls && (text[nd - 1] == ' ' || text[nd - 1] == '\t')) nd--;
        if (nd > ls && text[nd - 1] == '.') { i = nd - 1; continue; }
        break;
    }
    ctx->chain_len = n;
    for (int k = 0; k < n; k++) {
        int src = n - 1 - k;
        snprintf(ctx->chain[k], GCLC_CHAIN_NAME, "%s", names[src]);
        ctx->chain_is_call[k] = calls[src];
    }
}

/* Son eşleşmemiş '{' gövdesi struct/enum/typedef mi? */
static int body_is_type(const char *text, long brace_index) {
    if (brace_index < 0) return 0;
    size_t s = (size_t)brace_index;
    while (s > 0 && text[s - 1] != '\n') s--;
    const char *p = text + s;
    while (*p == ' ' || *p == '\t') p++;
    return (strncmp(p, "struct", 6) == 0 ||
            strncmp(p, "enum", 4) == 0 ||
            strncmp(p, "typedef", 7) == 0);
}

/* Yol (dosya yolu) bekleyen fonksiyonlar (§Faz 6): string argümanı, proje
   assets/ ağacından yol tamamlar. GCL raylib yükleyicileri + Stdio dosya API. */
static const char *gclc_path_fns[] = {
    "LoadTexture", "LoadTextureFromImage", "LoadTextureCubemap", "LoadImage",
    "LoadImageRaw", "LoadImageAnim", "LoadImageSvg", "LoadImageFromTexture",
    "LoadFont", "LoadFontEx", "LoadFontFromImage", "LoadFontData",
    "LoadSound", "LoadMusicStream", "LoadWave", "LoadAudioStream",
    "LoadShader", "LoadShaderFromMemory", "LoadModel", "LoadModelAnimations",
    "LoadFileText", "LoadFileData",
    "readFile", "writeFile", "appendFile", "fileExists", "deleteFile",
    "openFile", "closeFile", "fileSize",
    NULL
};

static int gclc_is_path_fn(const char *fn) {
    for (int i = 0; gclc_path_fns[i]; i++)
        if (strcmp(gclc_path_fns[i], fn) == 0) return 1;
    return 0;
}

/* Açık tırnak (quote_pos), yol bekleyen bir fonksiyonun İLK argümanının
   başlangıcı mı? "LoadTexture(\"assets/▮" → evet. */
static int path_string_at(const char *text, size_t ls, long quote_pos) {
    if (quote_pos <= 0) return 0;
    size_t k = (size_t)quote_pos;
    while (k > ls && (text[k - 1] == ' ' || text[k - 1] == '\t')) k--;
    if (k == 0 || text[k - 1] != '(') return 0;
    size_t e = k - 1;
    while (e > ls && (text[e - 1] == ' ' || text[e - 1] == '\t')) e--;
    size_t s = e;
    while (s > 0 && gclc_ident_char(text[s - 1])) s--;
    if (s == e) return 0;
    char fn[64];
    size_t n = e - s;
    if (n >= sizeof(fn)) n = sizeof(fn) - 1;
    memcpy(fn, text + s, n);
    fn[n] = '\0';
    return gclc_is_path_fn(fn);
}

/* "Type var = " atama bağlamı mı? İmleç '=' sonrasındaysa decl_type doldurulur. */
static int parse_value_assign(const char *text, size_t ls, size_t cursor,
                              GclContext *ctx) {
    size_t eq = (size_t)-1;
    for (size_t i = ls; i < cursor; i++) {
        if (text[i] != '=') continue;
        char prev = (i > ls) ? text[i - 1] : 0;
        char next = (i + 1 < cursor) ? text[i + 1] : 0;
        if (prev == '=' || prev == '!' || prev == '<' || prev == '>' ||
            prev == '+' || prev == '-' || prev == '*' || prev == '/' ||
            prev == '%' || prev == '&' || prev == '|' || prev == '^') continue;
        if (next == '=') continue;
        eq = i;
    }
    if (eq == (size_t)-1) return 0;
    /* '=' ile önek arasında yalnızca boşluk olmalı. */
    for (size_t i = eq + 1; i < ctx->prefix_begin; i++) {
        if (text[i] != ' ' && text[i] != '\t') return 0;
    }
    /* '=' öncesi: "Type [*&] var [ [..] ]" deseni. */
    size_t v = eq;
    while (v > ls && (text[v - 1] == ' ' || text[v - 1] == '\t')) v--;
    if (v > ls && text[v - 1] == ']') {
        /* Geriye doğru '[' ']' eşleştirmesi. d=0 ile başlanır; p zaten
           kapanış ']'nin BİR SONRASINI gösterir, bu yüzden ilk ']' d'yi 0→1
           yapmalı. (Eski kod d=1 ile başlayıp aynı ']'yi İKİ kez sayıyordu →
           "char a[] = " gibi dizi bildirimleri değer-atama bağlamı sayılmıyor,
           keyword listesi sızıyordu; §2.5 E7 regresyonu.) */
        int d = 0;
        size_t p = v;
        while (p > ls) {
            p--;
            if (text[p] == ']') d++;
            else if (text[p] == '[') { d--; if (d == 0) break; }
        }
        if (d != 0) return 0;
        v = p;
        while (v > ls && (text[v - 1] == ' ' || text[v - 1] == '\t')) v--;
    }
    while (v > ls && (text[v - 1] == '*' || text[v - 1] == '&')) {
        v--;
        while (v > ls && (text[v - 1] == ' ' || text[v - 1] == '\t')) v--;
    }
    size_t ve = v;
    while (v > ls && gclc_ident_char(text[v - 1])) v--;
    if (v == ve) return 0; /* değişken adı yok */

    size_t t = v;
    while (t > ls && (text[t - 1] == ' ' || text[t - 1] == '\t')) t--;
    size_t te = t;
    while (t > ls && gclc_ident_char(text[t - 1])) t--;
    if (t == te) return 0; /* tip adı yok */

    size_t tl = te - t;
    if (tl >= sizeof(ctx->decl_type)) tl = sizeof(ctx->decl_type) - 1;
    memcpy(ctx->decl_type, text + t, tl);
    ctx->decl_type[tl] = '\0';
    ctx->has_decl_type = 1;
    return 1;
}

void gcl_context_analyze(const char *text, size_t text_len, size_t cursor,
                         GclContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->kind = CTX_IDENT;
    if (!text) return;
    if (cursor > text_len) cursor = text_len;

    size_t ls = 0, le = 0;
    line_bounds(text, text_len, cursor, &ls, &le);

    int in_string = 0, in_comment = 0, paren_depth = 0;
    long last_brace = -1, quote_pos = -1;
    scan_lex(text, cursor, &in_string, &in_comment, &paren_depth, &last_brace,
             &quote_pos);
    ctx->in_string = in_string;
    ctx->in_comment = in_comment;
    ctx->call_depth = paren_depth;

    size_t pbegin = 0;
    read_prefix(text, ls, cursor, &pbegin, ctx->prefix, sizeof(ctx->prefix));
    ctx->prefix_begin = pbegin;
    ctx->prefix_end = cursor;

    /* Preprocessor satırı mı? (string/comment korumasından ÖNCE) */
    size_t p = ls;
    while (p < cursor && (text[p] == ' ' || text[p] == '\t')) p++;
    int is_preproc = (p < cursor && text[p] == '#');

    if (!is_preproc && (in_string || in_comment)) {
        /* Yol bekleyen fonksiyonun string argümanı → assets/ yol tamamlaması.
           Diğer TÜM string/yorum bağlamları kapalıdır (§2.5 E5/E6). */
        if (in_string && !in_comment && path_string_at(text, ls, quote_pos)) {
            ctx->kind = CTX_STRING_PATH;
            ctx->is_path_string = 1;
            size_t a = (size_t)quote_pos + 1;
            size_t n = (cursor > a) ? (cursor - a) : 0;
            if (n >= sizeof(ctx->str_prefix)) n = sizeof(ctx->str_prefix) - 1;
            memcpy(ctx->str_prefix, text + a, n);
            ctx->str_prefix[n] = '\0';
            return;
        }
        ctx->kind = CTX_NONE;
        return;
    }
    if (is_preproc) {
        ctx->line_is_preproc = 1;
        size_t d = p + 1;
        while (d < cursor && (text[d] == ' ' || text[d] == '\t')) d++;
        size_t ds = d;
        while (d < cursor && gclc_ident_char(text[d])) d++;
        size_t dn = d - ds;
        if (dn >= sizeof(ctx->directive)) dn = sizeof(ctx->directive) - 1;
        memcpy(ctx->directive, text + ds, dn);
        ctx->directive[dn] = '\0';

        size_t q = d;
        while (q < cursor && (text[q] == ' ' || text[q] == '\t')) q++;
        if (q < cursor && (text[q] == '<' || text[q] == '"')) {
            char close = (text[q] == '<') ? '>' : '"';
            size_t a = q + 1;
            size_t cl = a;
            int found = 0;
            while (cl < cursor) { if (text[cl] == close) { found = 1; break; } cl++; }
            if (found) {
                char modname[256];
                size_t ml = cl - a;
                if (ml >= sizeof(modname)) ml = sizeof(modname) - 1;
                memcpy(modname, text + a, ml);
                modname[ml] = '\0';
                size_t after = cl + 1;
                while (after < cursor && (text[after] == ' ' || text[after] == '\t')) after++;
                if (after < cursor && text[after] == '.') {
                    ctx->kind = CTX_MEMBER;
                    ctx->chain_len = 1;
                    snprintf(ctx->chain[0], GCLC_CHAIN_NAME, "%s", modname);
                    ctx->chain_is_call[0] = 0;
                } else {
                    ctx->kind = CTX_PREPROC;
                }
                return;
            }
            /* Kapanmamış '<' → modül/dosya adı yazılıyor. */
            ctx->in_angle = 1;
            size_t al = cursor - a;
            if (al >= sizeof(ctx->pp_arg)) al = sizeof(ctx->pp_arg) - 1;
            memcpy(ctx->pp_arg, text + a, al);
            ctx->pp_arg[al] = '\0';
        } else {
            size_t al = cursor - q;
            if (al >= sizeof(ctx->pp_arg)) al = sizeof(ctx->pp_arg) - 1;
            memcpy(ctx->pp_arg, text + q, al);
            ctx->pp_arg[al] = '\0';
        }

        if (ctx->in_angle) {
            if (strcmp(ctx->directive, "native") == 0)       ctx->kind = CTX_NATIVE_MODULE;
            else if (strcmp(ctx->directive, "include") == 0) ctx->kind = CTX_INCLUDE;
            else if (strcmp(ctx->directive, "extern") == 0)  ctx->kind = CTX_EXTERN;
            else if (strcmp(ctx->directive, "lib") == 0)     ctx->kind = CTX_LIB;
            else                                             ctx->kind = CTX_PREPROC;
        } else {
            if (strcmp(ctx->directive, "register") == 0) ctx->kind = CTX_REGISTER;
            else                                         ctx->kind = CTX_PREPROC;
        }
        return;
    }

    /* Üye erişimi: önekin hemen solundaki anlamlı karakter '.' mı? */
    size_t w = pbegin;
    while (w > ls && (text[w - 1] == ' ' || text[w - 1] == '\t')) w--;
    if (w > ls && text[w - 1] == '.') {
        ctx->kind = CTX_MEMBER;
        parse_chain(text, ls, w - 1, ctx);
        return;
    }

    /* Fonksiyon argümanı içi */
    if (paren_depth > 0) { ctx->kind = CTX_CALL_ARGS; return; }

    /* struct/enum/typedef gövdesi */
    if (body_is_type(text, last_brace)) {
        ctx->kind = CTX_TYPE_DECL;
        ctx->in_type_body = 1;
        return;
    }

    /* "Type var = " */
    if (parse_value_assign(text, ls, cursor, ctx)) {
        ctx->kind = CTX_VALUE_ASSIGN;
        return;
    }

    ctx->kind = CTX_IDENT;
}
