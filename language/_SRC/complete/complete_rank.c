/*
 * complete_rank.c — Önek/sıralama çekirdeği (§10).
 *
 * Filtre: kullanıcı bir önek yazdıysa yalnızca eşleşenler kalır (önce
 * case-insensitive önek, sonra fuzzy + CamelCase akronim).
 * Sıralama: puan (score) azalan; eşitlikte alfabetik.
 */
#include "complete_rank.h"
#include <ctype.h>

static int ci_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* Case-insensitive önek eşleşmesi. */
static int ci_prefix(const char *label, const char *prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        if (!label[i]) return 0;
        if (ci_lower((unsigned char)label[i]) != ci_lower((unsigned char)prefix[i]))
            return 0;
    }
    return 1;
}

/* Fuzzy: prefix karakterleri label içinde SIRALI olarak bulunmalı. */
static int fuzzy_match(const char *name, const char *query) {
    if (!name || !query || !query[0]) return 1;
    size_t qi = 0;
    for (const char *p = name; *p && query[qi]; p++) {
        if (ci_lower((unsigned char)*p) == ci_lower((unsigned char)query[qi])) qi++;
    }
    return query[qi] == '\0';
}

/* CamelCase akronim: query karakterleri label'ın kelime başlarında sıralı.
   Örn. "IP" → "InitWindow...Position", "GM" → "GetMousePosition". */
static int camel_acronym(const char *name, const char *query) {
    if (!name || !query || !query[0]) return 0;
    size_t qi = 0;
    int at_boundary = 1; /* ilk karakter her zaman sınır sayılır */
    for (const char *p = name; *p && query[qi]; p++) {
        int is_boundary = at_boundary || (*p == '_') ||
                          (*p >= 'A' && *p <= 'Z' && (p == name || p[-1] != '_'));
        if (is_boundary) {
            if (ci_lower((unsigned char)*p) == ci_lower((unsigned char)query[qi])) qi++;
        }
        at_boundary = (*p == '_');
    }
    return query[qi] == '\0';
}

int gcl_rank_match(const char *label, const char *prefix) {
    if (!prefix || !prefix[0]) return 1;
    if (!label || !label[0]) return 0;
    if (ci_prefix(label, prefix)) return 1;
    if (camel_acronym(label, prefix)) return 1;
    return fuzzy_match(label, prefix);
}

int gcl_rank_kind_bonus(GclItemKind kind, GclContextKind ctx) {
    /* Argüman/değer bağlamında DEĞER > fonksiyon/tip. */
    int is_value = (kind == CIK_VAR || kind == CIK_PARAM || kind == CIK_FIELD ||
                    kind == CIK_ENUM_VAL || kind == CIK_CONST || kind == CIK_MACRO);
    switch (ctx) {
        case CTX_CALL_ARGS:
        case CTX_VALUE_ASSIGN:
            return is_value ? 150 : -80;
        case CTX_MEMBER:
            /* Üye erişiminde alan/fonksiyon önde, sabitler arkada. */
            if (kind == CIK_FUNC || kind == CIK_FIELD || kind == CIK_PARAM) return 150;
            if (kind == CIK_VAR) return 120;
            if (kind == CIK_CONST || kind == CIK_ENUM_VAL) return 60;
            if (kind == CIK_TYPE) return 40;
            return 0;
        case CTX_TYPE_DECL:
            return (kind == CIK_TYPE || kind == CIK_MODULE) ? 150 : -80;
        case CTX_NATIVE_MODULE:
        case CTX_INCLUDE:
        case CTX_EXTERN:
        case CTX_LIB:
            return 150;
        case CTX_IDENT:
        default:
            /* Düz önek: fonksiyon/değişken hafif önde, keyword arkada. */
            if (kind == CIK_KEYWORD) return 0;
            if (kind == CIK_TYPE || kind == CIK_MODULE) return 80;
            return 120;
    }
}

/* Puanlama (§10.2).
 *
 * ÖNEMLİ: "eşleşmedi" ile "düşük/negatif puan" AYRI sinyallerdir ve bu
 * fonksiyon artık onları karıştırmaz. Eskiden eşleşmeyen için -1 dönülüyor,
 * çağıran da "sc < 0 → filtrele" yapıyordu. Ama gcl_rank_kind_bonus() bağlama
 * göre NEGATİF ceza verir (değer/argüman bağlamında değer OLMAYAN tür için
 * -80) ve kapsam puanı düşükse (builtin/modül = +20) toplam NEGATİF olur:
 *     modül @ CTX_VALUE_ASSIGN = 200 - 4*45 - 80 = -60
 * Bu yüzden GEÇERLİ adaylar sessizce listeden düşüyordu; cezanın amacı adayı
 * GERİYE ATMAK, listeden silmek değil (§10.2 "DEĞER > fonksiyon/tip").
 * Sözleşme: eşleşme 0/1 döner, puan `out_score` ile verilir.
 */
static int score_item(const GclItem *it, const char *prefix,
                      GclContextKind ctx, const char *active_file,
                      int *out_score) {
    const char *label = it->label ? it->label : "";
    int score = 0;

    if (prefix && prefix[0]) {
        if (strcmp(label, prefix) == 0)               score += 1000; /* tam */
        else if (ci_prefix(label, prefix))            score += 500;  /* önek */
        else if (camel_acronym(label, prefix))        score += 300;  /* CamelCase */
        else if (fuzzy_match(label, prefix))          score += 50;   /* fuzzy */
        else return 0;                                               /* eşleşmez */
    }

    /* Kapsam yakınlığı: 0 local .. 4 builtin. */
    int sr = it->scope_rank;
    if (sr < 0) sr = 2;
    if (sr > 4) sr = 4;
    score += (200 - sr * 45);

    /* Tür önceliği (bağlama göre). */
    score += gcl_rank_kind_bonus(it->kind, ctx);

    /* Dosya kökeni: aktif dosya > proje > builtin. */
    if (active_file && it->origin_file && it->origin_file[0]) {
        if (strcmp(it->origin_file, active_file) == 0) score += 100;
        else score += 40;
    }

    /* Kısa adlar hafif avantajlı (eşitlik bozucu). */
    score -= (int)strlen(label) / 4;
    if (out_score) *out_score = score;
    return 1;
}

typedef struct { int idx; int score; } RankRef;

/* Azalan puana göre sırala; eşitlikte alfabetik. */
static GclCompletionResult *g_sort_result = NULL;

static int rank_cmp_stable(const void *a, const void *b) {
    const RankRef *ra = (const RankRef *)a;
    const RankRef *rb = (const RankRef *)b;
    if (ra->score != rb->score) return rb->score - ra->score;
    if (g_sort_result) {
        const char *la = g_sort_result->items[ra->idx].label;
        const char *lb = g_sort_result->items[rb->idx].label;
        if (la && lb) {
            int c = strcmp(la, lb);
            if (c) return c;
        }
    }
    return ra->idx - rb->idx; /* kararlı */
}

void gcl_rank_apply(GclCompletionResult *r, const char *prefix,
                    GclContextKind ctx, const char *active_file) {
    if (!r || r->count <= 0) { if (r) r->best_index = 0; return; }

    RankRef *refs = (RankRef *)malloc((size_t)r->count * sizeof(RankRef));
    if (!refs) { r->best_index = 0; return; }

    int n = 0;
    for (int i = 0; i < r->count; i++) {
        int sc = 0;
        /* Yalnız EŞLEŞMEYEN öğeler filtrelenir. Negatif puan bir eleme
           gerekçesi DEĞİLDİR: bağlam cezası (-80) adayı geriye atar, ama
           listede bırakır; aksi halde "int x = Raylib." gibi ifadelerde
           modül adı hiç önerilmezdi. */
        if (!score_item(&r->items[i], prefix, ctx, active_file, &sc)) continue;
        r->items[i].score = sc;
        refs[n].idx = i;
        refs[n].score = sc;
        n++;
    }

    if (n == 0) {
        /* TÜM öğeler filtrelendi (yazılan önek hiçbir şeyle eşleşmedi).
           Eskiden yalnız `r->count = 0` yapılıyordu; gcl_complete_result_free()
           öğeleri `i < r->count` ile dolaştığı için başlık/gövde string'lerinin
           TAMAMI sızıyordu (eşleşmeyen her tuş vuruşunda N adet malloc). */
        for (int i = 0; i < r->count; i++) {
            GclItem *it = &r->items[i];
            free(it->label); free(it->insert); free(it->type); free(it->params);
            free(it->signature); free(it->doc); free(it->origin_file);
            memset(it, 0, sizeof(*it));
        }
        r->count = 0;
        r->best_index = 0;
        gcl_complete_dedup_invalidate(r);
        free(refs);
        return;
    }

    g_sort_result = r;
    qsort(refs, (size_t)n, sizeof(RankRef), rank_cmp_stable);
    g_sort_result = NULL;

    /* Yeniden diz: önce eşleşenler (puan sırası), SONRA eşleşmeyenler.
       ÖNEMLİ (eski hata): yalnız ilk n öğe kopyalandığında, [n, count)
       aralığı hâlâ kopyalanmış öğelerin AYNI string işaretçilerini taşıyordu;
       aşağıdaki serbest bırakma bu yüzden ÇİFT SERBEST (double free) olup
       yığın bozulmasına (STATUS_HEAP_CORRUPTION) yol açıyordu. Düşen öğeler
       artık ayrı bir 'matched' haritasıyla listenin SONUNA taşınır. */
    int old_count = r->count;
    GclItem *tmp = (GclItem *)malloc((size_t)old_count * sizeof(GclItem));
    if (!tmp) { free(refs); return; }
    char *matched = (char *)calloc((size_t)old_count, 1);
    if (!matched) { free(tmp); free(refs); return; }

    int w = 0;
    for (int i = 0; i < n; i++) {
        tmp[w++] = r->items[refs[i].idx];
        matched[refs[i].idx] = 1;
    }
    for (int i = 0; i < old_count; i++)
        if (!matched[i]) tmp[w++] = r->items[i];
    free(matched);

    for (int i = 0; i < old_count; i++) r->items[i] = tmp[i];
    free(tmp);

    /* Filtre nedeniyle düşen öğeler artık listenin SONUNDA (n..old_count);
       yalnız onların string'leri serbest bırakılır. */
    for (int i = n; i < old_count; i++) {
        GclItem *it = &r->items[i];
        free(it->label); free(it->insert); free(it->type); free(it->params);
        free(it->signature); free(it->doc); free(it->origin_file);
        memset(it, 0, sizeof(*it));
    }
    r->count = n;
    r->best_index = 0; /* en yüksek puan ilk sırada */
    /* Liste yeniden dizildi → dedup indeksleri artık geçersiz; gcl_complete.h
       bu temizliğin sıralayıcı tarafından yapılmasını şart koşar. */
    gcl_complete_dedup_invalidate(r);
    free(refs);
}
