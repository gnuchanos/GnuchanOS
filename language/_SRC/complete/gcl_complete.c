/*
 * gcl_complete.c — GCL "SMART" tamamlama motoru (facade, §3.1).
 *
 * Orkestrasyon: bağlam analizi → kapsam (scope) → tip çözümü → sıralama.
 * Editör yalnızca "imleç + buffer" verir; hangi önerinin listeleneceğine
 * motor karar verir. Çözümlenemeyen üye erişiminde liste BOŞ kalır ve
 * popup KAPANIR (S2 / §6.3).
 */
#include "gcl_complete.h"
#include "complete_context.h"
#include "complete_scope.h"
#include "complete_type.h"
#include "complete_native.h"
#include "complete_project.h"
#include "complete_index.h"
#include "complete_rank.h"
#include "complete_diag.h"
#include <stdio.h>

/* ================================================================== */
/* Sonuç yaşam döngüsü                                                 */
/* ================================================================== */

void gcl_complete_result_init(GclCompletionResult *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->best_index = 0;
}

static char *gclc_dup(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void gclc_item_free(GclItem *it) {
    if (!it) return;
    free(it->label);
    free(it->insert);
    free(it->type);
    free(it->params);
    free(it->signature);
    free(it->doc);
    free(it->origin_file);
    memset(it, 0, sizeof(*it));
}

void gcl_complete_result_free(GclCompletionResult *r) {
    if (!r) return;
    for (int i = 0; i < r->count; i++) gclc_item_free(&r->items[i]);
    free(r->items);
    free(r->dedup_slots);
    memset(r, 0, sizeof(*r));
}

/* ---- Dedup hash tablosu (bkz. gcl_complete.h: dedup_slots notu) ---- */

#define GCLC_DEDUP_INIT 1024

static unsigned gclc_dedup_hash(const char *label, GclItemKind kind) {
    unsigned h = 2166136261u;                          /* FNV-1a */
    for (const unsigned char *p = (const unsigned char *)label; *p; p++) {
        h ^= (unsigned)*p;
        h *= 16777619u;
    }
    h ^= (unsigned)kind * 2654435761u;
    h *= 16777619u;
    return h;
}

static int gclc_dedup_resize(GclCompletionResult *r, int cap) {
    int *n = (int *)calloc((size_t)cap, sizeof(int));
    if (!n) return 0;
    int *old = r->dedup_slots;
    int old_cap = r->dedup_cap;
    r->dedup_slots = n;
    r->dedup_cap = cap;
    r->dedup_used = 0;
    if (old) {
        unsigned mask = (unsigned)(cap - 1);
        for (int i = 0; i < old_cap; i++) {
            int v = old[i];
            if (v <= 0) continue;
            int idx = v - 1;
            if (idx < 0 || idx >= r->count || !r->items[idx].label) continue;
            unsigned h = gclc_dedup_hash(r->items[idx].label,
                                         r->items[idx].kind) & mask;
            while (n[h]) h = (h + 1) & mask;
            n[h] = v;
            r->dedup_used++;
        }
        free(old);
    }
    return 1;
}

/* Aynı etiket+tür zaten varsa indeksini, yoksa -1 döndür. O(1). */
static int gclc_dedup_find(const GclCompletionResult *r, const char *label,
                           GclItemKind kind) {
    if (!r->dedup_slots || r->dedup_cap <= 0) return -1;
    unsigned mask = (unsigned)(r->dedup_cap - 1);
    unsigned h = gclc_dedup_hash(label, kind) & mask;
    while (r->dedup_slots[h]) {
        int idx = r->dedup_slots[h] - 1;
        if (idx >= 0 && idx < r->count && r->items[idx].label &&
            r->items[idx].kind == kind &&
            strcmp(r->items[idx].label, label) == 0)
            return idx;
        h = (h + 1) & mask;
    }
    return -1;
}

static void gclc_dedup_insert(GclCompletionResult *r, int idx) {
    if (!r || idx < 0 || !r->items[idx].label) return;
    if (!r->dedup_slots || r->dedup_cap <= 0) {
        if (!gclc_dedup_resize(r, GCLC_DEDUP_INIT)) return;
    }
    if ((r->dedup_used + 1) * 4 >= r->dedup_cap * 3) {
        if (!gclc_dedup_resize(r, r->dedup_cap * 2)) return;
    }
    unsigned mask = (unsigned)(r->dedup_cap - 1);
    unsigned h = gclc_dedup_hash(r->items[idx].label, r->items[idx].kind) & mask;
    while (r->dedup_slots[h]) h = (h + 1) & mask;
    r->dedup_slots[h] = idx + 1;
    r->dedup_used++;
}

/* Sıralama sonrası liste yeniden dizildiği için dedup tablosu GEÇERSİZ olur;
   çağıran taraf (gcl_rank_apply) bu işlevi çağırıp temizler. */
void gcl_complete_dedup_invalidate(GclCompletionResult *r) {
    if (!r) return;
    if (r->dedup_slots && r->dedup_cap > 0)
        memset(r->dedup_slots, 0, (size_t)r->dedup_cap * sizeof(int));
    r->dedup_used = 0;
}

int gcl_complete_result_push(GclCompletionResult *r,
                             const char *label, const char *insert,
                             GclItemKind kind, const char *type,
                             const char *params, const char *signature,
                             const char *doc, const char *origin_file) {
    if (!r || !label || !label[0]) return -1;

    /* Dedup: aynı etiket + tür zaten eklenmişse ilk kazanır (O(1) hash). */
    int dup = gclc_dedup_find(r, label, kind);
    if (dup >= 0) return dup;

    if (r->count >= r->cap) {
        int ncap = r->cap ? r->cap * 2 : 64;
        GclItem *n = (GclItem *)realloc(r->items, (size_t)ncap * sizeof(GclItem));
        if (!n) return -1;
        r->items = n;
        r->cap = ncap;
    }
    int idx = r->count;
    GclItem *it = &r->items[idx];
    memset(it, 0, sizeof(*it));
    it->label       = gclc_dup(label);
    it->insert      = gclc_dup(insert);
    it->kind        = kind;
    it->type        = gclc_dup(type);
    it->params      = gclc_dup(params);
    it->signature   = gclc_dup(signature);
    it->doc         = gclc_dup(doc);
    it->origin_file = gclc_dup(origin_file);
    it->scope_rank  = 2;
    it->score       = 0;
    r->count++;
    gclc_dedup_insert(r, idx);
    return idx;
}

/* ================================================================== */
/* Öğe iticileri                                                       */
/* ================================================================== */

static int gclc_is_value_kind(GclItemKind k) {
    return k == CIK_VAR || k == CIK_PARAM || k == CIK_FIELD ||
           k == CIK_ENUM_VAL || k == CIK_CONST || k == CIK_MACRO;
}

static void gclc_push_sym(GclCompletionResult *out, const GclSym *s,
                          const char *fallback_file) {
    if (!s || !s->name[0]) return;
    char sig[256];
    sig[0] = '\0';
    if (s->kind == CIK_FUNC && s->sig[0])
        snprintf(sig, sizeof(sig), "%s %s(%s)",
                 s->type[0] ? s->type : "void", s->name, s->sig);

    int idx = gcl_complete_result_push(out, s->name, NULL, s->kind,
                                       s->type[0] ? s->type : NULL,
                                       (s->kind == CIK_FUNC && s->sig[0]) ? s->sig : NULL,
                                       sig[0] ? sig : NULL, NULL,
                                       s->file[0] ? s->file : fallback_file);
    if (idx >= 0) out->items[idx].scope_rank = s->scope_rank;
}

static void gclc_push_keyword(GclCompletionResult *out, const char *name) {
    int idx = gcl_complete_result_push(out, name, NULL, CIK_KEYWORD,
                                       NULL, NULL, NULL, NULL, NULL);
    if (idx >= 0) out->items[idx].scope_rank = 4;
}

static void gclc_push_type(GclCompletionResult *out, const char *name,
                           const char *file) {
    int idx = gcl_complete_result_push(out, name, NULL, CIK_TYPE,
                                       name, NULL, NULL, NULL, file);
    if (idx >= 0) out->items[idx].scope_rank = 4;
}

static void gclc_push_module(GclCompletionResult *out, const char *name) {
    int idx = gcl_complete_result_push(out, name, NULL, CIK_MODULE,
                                       NULL, NULL, NULL, "native module", NULL);
    if (idx >= 0) out->items[idx].scope_rank = 4;
}

#define GCLC_MAX_LIST 128
static int gclc_push_dir_files(GclCompletionResult *out, const char *dir,
                               const char *ext, const char *doc) {
    char files[GCLC_MAX_LIST][GCL_PROJECT_NAME];
    int n = gcl_project_list_dir(dir, ext, files, GCLC_MAX_LIST);
    for (int i = 0; i < n; i++) {
        int idx = gcl_complete_result_push(out, files[i], NULL, CIK_TYPE,
                                           NULL, NULL, NULL, doc, NULL);
        if (idx >= 0) out->items[idx].scope_rank = 3;
    }
    return n;
}

/* Yol tamamlaması (§Faz 6): tırnak içinde yazılan yolun DİZİN kısmını
   çözüp girdileri (dosyalar + alt dizinler '/'-sonlu) listeye ekler.
   Örn. LoadTexture("assets/te▮ → assets/ altındaki "te*" girdileri. */
static void gclc_fill_path(GclCompletionResult *out, const GclContext *ctx,
                           const char *workspace) {
    if (!out || !ctx) return;
    GclProject proj;
    gcl_project_load(&proj, (workspace && workspace[0]) ? workspace : ".");

    const char *typed = ctx->str_prefix;
    const char *slash = strrchr(typed, '/');
    char dirpart[512] = { 0 };
    if (slash) {
        size_t n = (size_t)(slash - typed) + 1;   /* '/' dahil */
        if (n >= sizeof(dirpart)) n = sizeof(dirpart) - 1;
        memcpy(dirpart, typed, n);
        dirpart[n] = '\0';
    }

    char dir[1400];
    if (dirpart[0]) {
        gcl_project_sub_path(&proj, dirpart, dir, sizeof(dir));
        /* "assets/..." ile başlamıyorsa varlık köküne göre yeniden dene. */
        char probe[1][GCL_PROJECT_NAME];
        size_t ap = strlen(proj.asset_path);
        if (gcl_project_list_entries(dir, probe, 1) == 0 &&
            strncmp(dirpart, proj.asset_path, ap) != 0) {
            char sub[1400];
            snprintf(sub, sizeof(sub), "%s/%s", proj.asset_path, dirpart);
            gcl_project_sub_path(&proj, sub, dir, sizeof(dir));
        }
    } else {
        gcl_project_sub_path(&proj, proj.asset_path, dir, sizeof(dir));
    }

    char entries[GCLC_MAX_LIST][GCL_PROJECT_NAME];
    int n = gcl_project_list_entries(dir, entries, GCLC_MAX_LIST);
    for (int i = 0; i < n; i++) {
        int idx = gcl_complete_result_push(out, entries[i], entries[i], CIK_FILE,
                                           NULL, NULL, NULL, "asset file", NULL);
        if (idx >= 0) out->items[idx].scope_rank = 3;
    }
}

/* ================================================================== */
/* Bağlama göre liste kurulumu (§5.3)                                  */
/* ================================================================== */

static void gclc_fill_ident(GclCompletionResult *out, const GclScope *scope,
                            const char *file) {
    const char *const *kw = gclc_keyword_list();
    for (int i = 0; kw[i]; i++) gclc_push_keyword(out, kw[i]);

    const char *const *bt = gclc_builtin_type_list();
    for (int i = 0; bt[i]; i++) gclc_push_type(out, bt[i], file);

    for (int i = 0; i < scope->sym_count; i++)
        gclc_push_sym(out, &scope->syms[i], file);

    const char *const *mods = gcl_native_module_names();
    for (int i = 0; mods[i]; i++) gclc_push_module(out, mods[i]);
}

static void gclc_fill_values(GclCompletionResult *out, const GclScope *scope,
                             const char *file) {
    for (int i = 0; i < scope->sym_count; i++) {
        if (!gclc_is_value_kind(scope->syms[i].kind)) continue;
        gclc_push_sym(out, &scope->syms[i], file);
    }
}

static void gclc_fill_types(GclCompletionResult *out, const GclScope *scope,
                            const char *file) {
    const char *const *bt = gclc_builtin_type_list();
    for (int i = 0; bt[i]; i++) gclc_push_type(out, bt[i], file);
    for (int i = 0; i < scope->type_count; i++)
        gclc_push_type(out, scope->types[i].name, file);
    for (int i = 0; i < scope->sym_count; i++)
        if (scope->syms[i].kind == CIK_MODULE)
            gclc_push_type(out, scope->syms[i].name, file);
}

/* ================================================================== */
/* İmza yardımı (§11.2)                                                */
/* ================================================================== */

/* İmleçten geriye: aktif çağrının '(' konumunu bul (iç içe parantezleri
   say). Bulunamazsa -1. Derinlik 0'da ';' / '{' / '}' geçilirse çağrı yok. */
static long gclc_find_call_paren(const char *text, size_t cursor) {
    int depth = 0;
    for (size_t i = cursor; i > 0; i--) {
        char c = text[i - 1];
        if (c == ')') depth++;
        else if (c == '(') {
            if (depth == 0) return (long)(i - 1);
            depth--;
        } else if (depth == 0 && (c == ';' || c == '{' || c == '}')) {
            return -1;
        }
    }
    return -1;
}

/* '(' öncesindeki çağrı adını çöz: "Mod.fn" veya "fn". */
static void gclc_read_callee(const char *text, size_t paren,
                             char *mod, size_t modcap,
                             char *fn, size_t fncap) {
    if (mod && modcap) mod[0] = '\0';
    if (fn && fncap) fn[0] = '\0';
    size_t e = paren;
    while (e > 0 && (text[e - 1] == ' ' || text[e - 1] == '\t')) e--;
    size_t s = e;
    while (s > 0 && gclc_ident_char(text[s - 1])) s--;
    if (s == e) return;
    size_t n = e - s;
    if (n >= fncap) n = fncap - 1;
    memcpy(fn, text + s, n);
    fn[n] = '\0';

    size_t d = s;
    while (d > 0 && (text[d - 1] == ' ' || text[d - 1] == '\t')) d--;
    if (d == 0 || text[d - 1] != '.') return;
    size_t de = d - 1;
    while (de > 0 && (text[de - 1] == ' ' || text[de - 1] == '\t')) de--;
    size_t ds = de;
    while (ds > 0 && gclc_ident_char(text[ds - 1])) ds--;
    if (ds >= de) return;
    size_t mn = de - ds;
    if (mn >= modcap) mn = modcap - 1;
    memcpy(mod, text + ds, mn);
    mod[mn] = '\0';
}

/* CTX_CALL_ARGS içinde çağrılan fonksiyonun imzasını çöz (§11.2). */
static void gclc_fill_signature(GclCompletionResult *out, const GclContext *ctx,
                                const GclScope *scope, const char *text,
                                size_t text_len, size_t cursor) {
    if (!out || !ctx || !text || ctx->kind != CTX_CALL_ARGS) return;
    if (cursor > text_len) cursor = text_len;

    long paren = gclc_find_call_paren(text, cursor);
    if (paren < 0) return;

    char mod[96], fn[96];
    gclc_read_callee(text, (size_t)paren, mod, sizeof(mod), fn, sizeof(fn));
    if (!fn[0]) return;

    const char *params = NULL;
    if (mod[0] && gcl_native_is_module(mod)) {
        const GclNativeMember *m = gcl_native_find(mod, fn);
        if (m && (m->flags & NF_FUNC)) params = m->params;
    } else {
        const GclSym *sy = gcl_scope_find(scope, fn);
        if (sy && sy->kind == CIK_FUNC && sy->sig[0]) params = sy->sig;
    }
    if (!params) return;

    out->have_signature = 1;
    snprintf(out->sig_label, sizeof(out->sig_label), "%s%s%s",
             mod[0] ? mod : "", mod[0] ? "." : "", fn);
    snprintf(out->sig_params, sizeof(out->sig_params), "%s", params);

    /* Aktif parametre: '(' ile imleç arasındaki üst-düzey virgül sayısı. */
    int depth = 0, active = 0;
    for (size_t i = (size_t)paren + 1; i < cursor; i++) {
        char c = text[i];
        if (c == '(' || c == '[') depth++;
        else if (c == ')' || c == ']') { if (depth > 0) depth--; }
        else if (c == ',' && depth == 0) active++;
        else if (c == '"' || c == '\'') {
            char q = c;
            i++;
            while (i < cursor && text[i] != q) {
                if (text[i] == '\\' && i + 1 < cursor) i++;
                i++;
            }
        }
    }
    out->sig_active_param = active;
}

/* ================================================================== */
/* Facade                                                              */
/* ================================================================== */

GclContextKind gcl_complete_query(const char *file, const char *text,
                                  size_t text_len, size_t cursor,
                                  const char *workspace,
                                  GclCompletionResult *out) {
    if (!out) return CTX_NONE;
    gcl_complete_result_init(out);

    if (!text || cursor > text_len) {
        out->suppressed = 1;
        return CTX_NONE;
    }

    /* 1) Bağlam */
    GclContext ctx;
    gcl_context_analyze(text, text_len, cursor, &ctx);
    out->context = ctx.kind;

    /* 1.5) Tanılar (§5.4): popup durumundan BAĞIMSIZ — imleç printf
       argümanlarında/kapanışında olsa da {} denetimi yapılır. */
    gcl_diag_check_printf(text, text_len, cursor, out);

    if (ctx.kind == CTX_NONE) {
        out->suppressed = 1;
        out->handled = 1;
        return ctx.kind;
    }

    /* 2) Kapsam: aktif dosya + (varsa) proje sembolleri.
       Aktif buffer'ın tarama sonucu içerik hash'i ile önbellekten gelir (§12):
       metin değişmediyse (imleç hareketi, popup yeniden açımı, debounce sonrası
       yeniden sorgu) 43 KB'lık buffer YENİDEN AYRIŞTIRILMAZ. Önbellek kapsamı
       motora aittir ve bu sorgu boyunca yeniden kullanılır — bu yüzden
       `gcl_scope_destroy()` ÇAĞRILMAZ. Proje sembolleri idempotent eklenir. */
    GclScope *scope =
        gcl_index_scope(file, text, text_len, workspace ? workspace : "");
    if (!scope) { out->suppressed = 1; return ctx.kind; }
    if (workspace && workspace[0]) {
        GclProject proj;
        gcl_project_load(&proj, workspace);
        gcl_project_scan_into_scope(&proj, scope, file);
    }

    /* 3) Bağlama göre liste */
    switch (ctx.kind) {
        case CTX_MEMBER:
            if (!gcl_type_resolve_members(scope, &ctx, file, out)) {
                /* Kapsam önbelleğe ait → destroy YOK (§6.3: popup kapalı). */
                out->suppressed = 1;
                out->handled = 1;
                return ctx.kind;
            }
            break;

        case CTX_CALL_ARGS:
        case CTX_VALUE_ASSIGN:
            gclc_fill_values(out, scope, file);
            break;

        case CTX_TYPE_DECL:
        case CTX_REGISTER:
            gclc_fill_types(out, scope, file);
            break;

        case CTX_PREPROC: {
            const char *const *dirs = gclc_directive_list();
            for (int i = 0; dirs[i]; i++) {
                int idx = gcl_complete_result_push(out, dirs[i], NULL, CIK_KEYWORD,
                                                   NULL, NULL, NULL, "directive", NULL);
                if (idx >= 0) out->items[idx].scope_rank = 4;
            }
            break;
        }

        case CTX_NATIVE_MODULE: {
            const char *const *mods = gcl_native_module_names();
            for (int i = 0; mods[i]; i++) gclc_push_module(out, mods[i]);
            break;
        }

        case CTX_INCLUDE: {
            GclProject proj;
            gcl_project_load(&proj, workspace ? workspace : ".");
            char dir[1200];
            gcl_project_sub_path(&proj, proj.include_path, dir, sizeof(dir));
            gclc_push_dir_files(out, dir, ".gcsf", "include file");
            break;
        }

        case CTX_EXTERN: {
            GclProject proj;
            gcl_project_load(&proj, workspace ? workspace : ".");
            char dir[1200];
            gcl_project_sub_path(&proj, proj.external_path, dir, sizeof(dir));
            gclc_push_dir_files(out, dir, ".dll", "external library");
            gclc_push_dir_files(out, dir, ".so", "external library");
            gclc_push_dir_files(out, dir, ".dylib", "external library");
            break;
        }

        case CTX_LIB: {
            GclProject proj;
            gcl_project_load(&proj, workspace ? workspace : ".");
            char dir[1200];
            gcl_project_sub_path(&proj, proj.lib_path, dir, sizeof(dir));
            gclc_push_dir_files(out, dir, ".gclib", "library file");
            break;
        }

        case CTX_STRING_PATH:
            gclc_fill_path(out, &ctx, workspace);
            break;

        case CTX_IDENT:
        default:
            gclc_fill_ident(out, scope, file);
            break;
    }

    /* 3.5) İmza yardımı (kapsam hâlâ canlıyken) */
    gclc_fill_signature(out, &ctx, scope, text, text_len, cursor);

    /* NOT: `scope` önbelleğe aittir → destroy edilmez (§12). */

    /* 4) Sıralama / filtre / best_index */
    gcl_rank_apply(out, ctx.prefix, ctx.kind, file);
    out->handled = 1;

    if (ctx.kind == CTX_MEMBER && out->count == 0)
        out->suppressed = 1;

    return ctx.kind;
}
