/*
 * complete_scope.c — Metinden sembol & tip tablosu çıkarımı (§6, §7).
 *
 * Basit ama etkili: yorum/string gürültüsü ayıklanır (strip_noise), sonra
 * satır bazlı tarama yapılır. Her değişkene bildirilen TİP bağlanır
 * ("Hello a;" → a : Hello), böylece resolve_member a. → Hello alanları
 * diyebilir.
 */
#include "complete_scope.h"
#include <stdio.h>

/* ---- Builtin tipler ---- */
static const char *gclc_builtin_types[] = {
    "int", "short", "long", "float", "double", "char", "bool", "void",
    "unsigned", "signed", "const", "global", "inline", "static", "auto", "register",
    "int8", "int16", "int32", "int64", "int128",
    "uint8", "uint16", "uint32", "uint64", "uint128",
    "float16", "float32", "float64", "float128",
    "gcChar", "size_t", "string",
    NULL
};

int gclc_is_builtin_type(const char *name) {
    if (!name) return 0;
    for (int i = 0; gclc_builtin_types[i]; i++)
        if (strcmp(gclc_builtin_types[i], name) == 0) return 1;
    return 0;
}

/* ---- Küçük yardımcılar ---- */

static void copy_fixed(char *dst, size_t cap, const char *src, size_t len) {
    if (cap == 0) return;
    if (len >= cap) len = cap - 1;
    if (len) memcpy(dst, src, len);
    dst[len] = '\0';
}

/* s[0..len) içindeki baştaki/sondaki boşluğu kırp (offset döndürür). */
static size_t trim_span(const char *s, size_t len, size_t *out_len) {
    size_t a = 0, b = len;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    *out_len = b - a;
    return a;
}

/* ---- Sembol adı hash tablosu (S7: O(n) ekleme/arama) ----
   Açık adresleme + doğrusal sonda. slots[h] == 0 → boş, aksi halde indeks+1.
   Tablo kapasitesi 2^n'dir; %75 dolulukta iki katına çıkar. */

#define GCLC_SYM_SLOTS_INIT 2048

static unsigned scope_hash_name(const char *name) {
    unsigned h = 2166136261u;                       /* FNV-1a */
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        h ^= (unsigned)*p;
        h *= 16777619u;
    }
    return h;
}

/* Adın sembol indeksini ver; yoksa -1. Tablo yoksa doğrusal yedeğe düşer. */
static int scope_sym_index(const GclScope *s, const char *name) {
    if (!s || !name || !name[0]) return -1;
    if (!s->sym_slots || s->sym_slots_cap <= 0) {
        for (int i = s->sym_count - 1; i >= 0; i--)
            if (strcmp(s->syms[i].name, name) == 0) return i;
        return -1;
    }
    unsigned mask = (unsigned)(s->sym_slots_cap - 1);
    unsigned h = scope_hash_name(name) & mask;
    while (s->sym_slots[h]) {
        int idx = s->sym_slots[h] - 1;
        if (idx >= 0 && idx < s->sym_count &&
            strcmp(s->syms[idx].name, name) == 0)
            return idx;
        h = (h + 1) & mask;
    }
    return -1;
}

/* Tabloyu 'cap' boyutunda yeniden kur (rehash). Başarıda 1. */
static int scope_sym_slots_resize(GclScope *s, int cap) {
    if (!s || cap <= 0) return 0;
    int *n = (int *)calloc((size_t)cap, sizeof(int));
    if (!n) return 0;
    int *old = s->sym_slots;
    int old_cap = s->sym_slots_cap;

    s->sym_slots = n;
    s->sym_slots_cap = cap;
    s->sym_slots_used = 0;
    if (old) {
        unsigned mask = (unsigned)(cap - 1);
        for (int i = 0; i < old_cap; i++) {
            int v = old[i];
            if (v <= 0 || v - 1 >= s->sym_count) continue;
            const char *nm = s->syms[v - 1].name;
            unsigned h = scope_hash_name(nm) & mask;
            while (s->sym_slots[h]) h = (h + 1) & mask;
            s->sym_slots[h] = v;
            s->sym_slots_used++;
        }
        free(old);
    }
    return 1;
}

/* Yeni eklenen sembolü (indeks 'idx') tabloya yaz. Gerekirse büyütür. */
static void scope_sym_slots_insert(GclScope *s, int idx) {
    if (!s || idx < 0) return;
    if (!s->sym_slots || s->sym_slots_cap <= 0) {
        if (!scope_sym_slots_resize(s, GCLC_SYM_SLOTS_INIT)) return;
    }
    /* %75 doluluk → iki katına çıkar. */
    if ((s->sym_slots_used + 1) * 4 >= s->sym_slots_cap * 3) {
        if (!scope_sym_slots_resize(s, s->sym_slots_cap * 2)) return;
    }
    unsigned mask = (unsigned)(s->sym_slots_cap - 1);
    unsigned h = scope_hash_name(s->syms[idx].name) & mask;
    while (s->sym_slots[h]) {
        int ex = s->sym_slots[h] - 1;
        /* Aynı isim zaten varsa EN YENİ indeksi tut (tek giriş/isim invariantı).
           Böylece scope_sym_index() her zaman son tanımı döndürür ve tablo
           büyümez → O(1) ekleme/arama (S7). */
        if (ex >= 0 && ex < s->sym_count &&
            strcmp(s->syms[ex].name, s->syms[idx].name) == 0) {
            s->sym_slots[h] = idx + 1;
            return;
        }
        h = (h + 1) & mask;
    }
    s->sym_slots[h] = idx + 1;
    s->sym_slots_used++;
}

/* Yorum ve string gövdelerini boşlukla değiştir (satır sonlarını KORU). */
static char *strip_noise(const char *text, size_t len) {
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, text, len);
    out[len] = '\0';
    int block = 0, line_comment = 0, quote = 0;
    for (size_t i = 0; i < len; i++) {
        char c = out[i];
        if (line_comment) {
            if (c == '\n') { line_comment = 0; continue; }
            out[i] = ' ';
            continue;
        }
        if (block == 1) {
            if (c == '*' && i + 1 < len && out[i + 1] == '/') { out[i] = ' '; out[i + 1] = ' '; i++; block = 0; continue; }
            if (c != '\n') out[i] = ' ';
            continue;
        }
        if (block == 2) {
            if (c == '|' && i + 1 < len && out[i + 1] == '#') { out[i] = ' '; out[i + 1] = ' '; i++; block = 0; continue; }
            if (c != '\n') out[i] = ' ';
            continue;
        }
        if (quote) {
            if (c == '\\' && i + 1 < len) { out[i] = ' '; out[i + 1] = ' '; i++; continue; }
            if (c == quote) { quote = 0; out[i] = ' '; continue; }
            if (c != '\n') out[i] = ' ';
            continue;
        }
        if (c == '/' && i + 1 < len && out[i + 1] == '/') { out[i] = ' '; out[i + 1] = ' '; i++; line_comment = 1; continue; }
        if (c == '/' && i + 1 < len && out[i + 1] == '*') { out[i] = ' '; out[i + 1] = ' '; i++; block = 1; continue; }
        if (c == '#' && i + 1 < len && out[i + 1] == '|') { out[i] = ' '; out[i + 1] = ' '; i++; block = 2; continue; }
        if (c == '"' || c == '\'') { quote = c; out[i] = ' '; continue; }
    }
    return out;
}

/* ---- Yaşam döngüsü ---- */

GclScope *gcl_scope_create(void) {
    GclScope *s = (GclScope *)calloc(1, sizeof(GclScope));
    if (!s) return NULL;
    s->sym_cap = 256;
    s->syms = (GclSym *)calloc((size_t)s->sym_cap, sizeof(GclSym));
    s->type_cap = 32;
    s->types = (GclTypeDef *)calloc((size_t)s->type_cap, sizeof(GclTypeDef));
    if (!s->syms || !s->types) { gcl_scope_destroy(s); return NULL; }
    return s;
}

/* Bir tipin dinamik üye listesini serbest bırak. */
static void type_free_members(GclTypeDef *t) {
    if (!t) return;
    free(t->members);
    t->members = NULL;
    t->member_count = 0;
    t->member_cap = 0;
}

void gcl_scope_destroy(GclScope *s) {
    if (!s) return;
    for (int i = 0; i < s->type_count; i++) type_free_members(&s->types[i]);
    free(s->syms);
    free(s->types);
    free(s->sym_slots);   /* eskiden sızıyordu */
    free(s);
}

void gcl_scope_reset(GclScope *s) {
    if (!s) return;
    for (int i = 0; i < s->type_count; i++) type_free_members(&s->types[i]);
    s->sym_count = 0;
    s->type_count = 0;
    s->sym_slots_used = 0;
    if (s->sym_slots && s->sym_slots_cap > 0)
        memset(s->sym_slots, 0, (size_t)s->sym_slots_cap * sizeof(int));
    s->main_file[0] = '\0';
}

static int scope_ensure_syms(GclScope *s, int need) {
    if (need <= s->sym_cap) return 1;
    int ncap = s->sym_cap;
    while (ncap < need) ncap *= 2;
    GclSym *n = (GclSym *)realloc(s->syms, (size_t)ncap * sizeof(GclSym));
    if (!n) return 0;
    s->syms = n;
    s->sym_cap = ncap;
    return 1;
}

int gcl_scope_add_type(GclScope *s, const char *name, int is_enum,
                       const char *file, int priv) {
    if (!s || !name || !name[0]) return -1;
    if (s->type_count >= s->type_cap) {
        int ncap = s->type_cap * 2;
        GclTypeDef *n = (GclTypeDef *)realloc(s->types, (size_t)ncap * sizeof(GclTypeDef));
        if (!n) return -1;
        s->types = n;
        s->type_cap = ncap;
    }
    int idx = s->type_count++;
    GclTypeDef *t = &s->types[idx];
    /* Yalnız gerekli alanlar başlatılır. Eskiden tüm struct sıfırlanıyordu;
       sabit 64 üyeli diziyle bu ~13 KB/tip idi (300 tip → ~4 MB) ve 43 KB'lık
       dosyada tek başına S7 bütçesini yiyordu. */
    t->name[0]      = '\0';
    t->file[0]      = '\0';
    t->is_enum      = is_enum;
    t->is_private   = priv;
    t->members      = NULL;
    t->member_count = 0;
    t->member_cap   = 0;
    copy_fixed(t->name, sizeof(t->name), name, strlen(name));
    if (file) copy_fixed(t->file, sizeof(t->file), file, strlen(file));
    return idx;
}

/* Bir tipe üye ekle (DİNAMİK; gerektiğinde büyür). Sabit GCLC_MAX_MEMBERS
   sınırı kalktı: eskiden 64'ten fazlası sessizce düşüyordu. */
static int type_push_member(GclTypeDef *t, const char *name, const char *type,
                            GclItemKind kind) {
    if (!t || !name || !name[0]) return 0;
    if (t->member_count >= t->member_cap) {
        int ncap = t->member_cap ? t->member_cap * 2 : 8;
        GclMember *n = (GclMember *)realloc(t->members,
                                            (size_t)ncap * sizeof(GclMember));
        if (!n) return 0;
        t->members = n;
        t->member_cap = ncap;
    }
    GclMember *m = &t->members[t->member_count];
    m->name[0] = '\0';
    m->type[0] = '\0';
    m->kind = kind;
    m->vis_private = t->is_private;
    copy_fixed(m->name, sizeof(m->name), name, strlen(name));
    copy_fixed(m->type, sizeof(m->type), type ? type : "",
               strlen(type ? type : ""));
    t->member_count++;
    return 1;
}

/* Sembol ekle (aynı isim+dosya varsa tip/sig güncelle). */
static void scope_add_sym(GclScope *s, const char *name, const char *type,
                          GclItemKind kind, const char *sig,
                          int is_call, int priv, int rank, const char *file) {
    if (!s || !name || !name[0]) return;
    /* O(1) arama: hash tablosu. Eskiden TÜM semboller doğrusal taranıyordu →
       43 KB dosyada O(n²) (~2 ms); bkz. S7 / test_perf. */
    int ex = scope_sym_index(s, name);
    if (ex >= 0) {
        /* Yerel gölgeleme: aynı isim daha yakın kapsamda → üzerine yaz. */
        if (rank <= s->syms[ex].scope_rank) {
            if (type && type[0]) copy_fixed(s->syms[ex].type, sizeof(s->syms[ex].type), type, strlen(type));
            if (sig && sig[0]) copy_fixed(s->syms[ex].sig, sizeof(s->syms[ex].sig), sig, strlen(sig));
            s->syms[ex].kind = kind;
            s->syms[ex].is_call = is_call;
            s->syms[ex].scope_rank = rank;
        }
        return;
    }
    if (!scope_ensure_syms(s, s->sym_count + 1)) return;
    int idx = s->sym_count++;
    GclSym *y = &s->syms[idx];
    /* Yalnız kullanılan alanlar başlatılır (tam memset 900 sembolde boşa yazı). */
    y->name[0] = '\0';
    y->type[0] = '\0';
    y->sig[0]  = '\0';
    y->file[0] = '\0';
    copy_fixed(y->name, sizeof(y->name), name, strlen(name));
    if (type) copy_fixed(y->type, sizeof(y->type), type, strlen(type));
    if (sig) copy_fixed(y->sig, sizeof(y->sig), sig, strlen(sig));
    y->kind = kind;
    y->is_call = is_call;
    y->vis_private = priv;
    y->scope_rank = rank;
    if (file) copy_fixed(y->file, sizeof(y->file), file, strlen(file));
    scope_sym_slots_insert(s, idx);
}

const GclSym *gcl_scope_find(const GclScope *s, const char *name) {
    if (!s || !name) return NULL;
    int i = scope_sym_index(s, name);
    return (i >= 0) ? &s->syms[i] : NULL;
}

const GclTypeDef *gcl_scope_find_type(const GclScope *s, const char *name) {
    if (!s || !name) return NULL;
    for (int i = 0; i < s->type_count; i++)
        if (strcmp(s->types[i].name, name) == 0) return &s->types[i];
    return NULL;
}

int gcl_scope_is_type(const GclScope *s, const char *name) {
    return gcl_scope_find_type(s, name) != NULL;
}

/* ---- Üye / bildirim ayrıştırma ---- */

/* Bildirilen tip metnini TEK KANONİK ada indir.
   Atılanlar: baştaki niteleyiciler (const/global/static/inline/local),
   struct/enum/union etiketi, sondaki pointer/referans ('*','&') ve dizi
   son ekleri ("[10]").

   NEDEN: üye çözümlemesi (complete_type.c) bir tipi ADIYLA arar. "struct Point"
   veya "Point *" yazan bir sembol, tip tablosundaki "Point" girdisiyle
   eşleşmediği için '.' zinciri SESSİZCE durur. `split_declarator()` bunu
   değişkenler için zaten yapıyordu; fonksiyon DÖNÜŞ tipi için yapılmıyordu, bu
   yüzden kullanıcı fonksiyonunun dönüşü zincirlenemiyordu
   (`makePoint(1).` → hiçbir öneri).

   Yerleşik tipler korunur: "unsigned int" → "unsigned int" (üye listesi yok,
   ama tip adı bozulmamalı). "Raylib.Vector2 *" → "Raylib.Vector2" (modül öneki
   burada SOYULMAZ; onu complete_type.c'deki strip_module_prefix yapar — kapsam
   katmanı native modül listesini bilmemeli). */
static void normalize_type_name(char *t, size_t cap) {
    if (!t || cap == 0) return;
    size_t n = strlen(t);
    size_t a = 0;
    while (a < n && (t[a] == ' ' || t[a] == '\t')) a++;

    /* Baştaki niteleyici ve etiket sözcükleri (birden fazla olabilir:
       "const struct Point"). */
    static const char *lead[] = {
        "const", "global", "static", "inline", "local",
        "struct", "enum", "union", NULL
    };
    for (;;) {
        size_t adv = 0;
        for (int i = 0; lead[i]; i++) {
            size_t l = strlen(lead[i]);
            if (n - a >= l && strncmp(t + a, lead[i], l) == 0 &&
                (n - a == l || !gclc_ident_char(t[a + l]))) { adv = l; break; }
        }
        if (adv == 0) break;
        a += adv;
        while (a < n && (t[a] == ' ' || t[a] == '\t')) a++;
    }

    /* Sondaki boşluk, pointer/referans ve dizi son ekleri. */
    size_t b = n;
    for (;;) {
        while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) b--;
        if (b > a && (t[b - 1] == '*' || t[b - 1] == '&')) { b--; continue; }
        if (b > a && t[b - 1] == ']') {
            int d = 0;
            size_t k = b;
            while (k > a) {
                k--;
                if (t[k] == ']') d++;
                else if (t[k] == '[') { d--; if (d == 0) break; }
            }
            if (d == 0 && t[k] == '[') { b = k; continue; }
        }
        break;
    }
    if (b <= a) { t[0] = '\0'; return; }
    memmove(t, t + a, b - a);
    t[b - a] = '\0';
}

/* name: son tanımlayıcı; type: öncesi (normalize_type_name ile sadeleşir). */
static void split_declarator(const char *s, size_t len,
                            char *type_out, size_t tcap,
                            char *name_out, size_t ncap) {
    size_t a, l;
    a = trim_span(s, len, &l);
    const char *p = s + a;
    /* sondaki ';' kırp */
    while (l > 0 && (p[l - 1] == ';' || p[l - 1] == ' ' || p[l - 1] == '\t')) l--;
    if (l == 0) { type_out[0] = '\0'; name_out[0] = '\0'; return; }
    /* '=' sonrasını at */
    for (size_t i = 0; i < l; i++) { if (p[i] == '=') { l = i; break; } }
    /* Dizi bildirimi: "Type name[N]" / "name[N][M]" → tanımlayıcı '[' ÖNCESİNDEdir.
       Eskiden imleç ']' üzerinde kalıyor, ardından gclc_ident_char(']') false
       döndüğü için tanımlayıcı BULUNAMIYOR ve dizi değişkenleri/alanları
       kapsama HİÇ girmiyordu. Bu yüzden "char name[20]" üyesi kayboluyor
       (örnek: 8_typedef_struct.gcsf içindeki person.name) ve "people[i]."
       hiçbir öneri vermiyordu (todo #6: dizi+struct karışımı). */
    for (;;) {
        size_t e = l;
        while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t')) e--;
        if (e == 0 || p[e - 1] != ']') break;
        int br = 0;
        size_t s2 = e;
        while (s2 > 0) {
            s2--;
            if (p[s2] == ']') br++;
            else if (p[s2] == '[') { br--; if (br == 0) break; }
        }
        if (br != 0) break;          /* eşleşmeyen '[' → dizi değil, dokunma */
        l = s2;                      /* '[' konumuna kırp; çok boyutlu için döngü */
    }
    while (l > 0 && (p[l - 1] == ' ' || p[l - 1] == '\t')) l--;
    if (l == 0) { type_out[0] = '\0'; name_out[0] = '\0'; return; }
    /* son ident = isim */
    size_t name_end = l;
    while (name_end > 0 && (p[name_end - 1] == ' ' || p[name_end - 1] == '\t')) name_end--;
    size_t name_start = name_end;
    while (name_start > 0 && gclc_ident_char(p[name_start - 1])) name_start--;
    if (name_start == name_end) { type_out[0] = '\0'; name_out[0] = '\0'; return; }
    copy_fixed(name_out, ncap, p + name_start, name_end - name_start);
    /* tip = isimden öncesi (kanonik ada indirgenir; bkz. normalize_type_name) */
    size_t ta, tl;
    ta = trim_span(p, name_start, &tl);
    copy_fixed(type_out, tcap, p + ta, tl);
    normalize_type_name(type_out, tcap);
}

/* Tip kısmı '.' ile bitiyorsa bu bir BİLDİRİM DEĞİL, nokta ile nitelenmiş üye
   erişimidir: "Raylib.InitWindow(800, 600)", "player.pos.x = 5", "Raylib.WHITE".
   `split_declarator` ismi sondaki tanımlayıcıdan alırken tipi ondan ÖNCEKİ
   metin olarak bırakır; üye erişiminde bu metin nokta ile biter.

   Eskiden bu ayrım yapılmıyordu ve sonuç olarak:
     (a) `Raylib.InitWindow(...)` çağrı yerleri "InitWindow" adlı GLOBAL bir
         fonksiyon sembolü (tipi "Raylib.") olarak kapsama giriyordu → modül
         üyeleri `Raylib.` yazılmadan düz önekte tamamlama olarak sızıyordu,
     (b) `Raylib.WHITE` gibi nitelenmiş sabitler parametre olarak ekleniyordu. */
static int type_is_qualified(const char *type) {
    size_t n = type ? strlen(type) : 0;
    return n > 0 && type[n - 1] == '.';
}

/* Çağrı argümanı segmenti GERÇEKTEN "Tip isim" bildirimi mi?
   Argüman listeleri ifade içerir: "800", "20", "x + 1", "a * b", "Raylib.WHITE".
   Yalnızca tip + isim desenindeki segmentler parametre sayılır; operatör,
   nokta veya parantez içeren segment ifadedir ve ATILIR. Eskiden hepsi
   parametre sanılıp kapsama ekleniyordu; bu yüzden çağrılara yazılan sayılar
   (20, 60, 600, 800) tamamlama listesinde öneri olarak görünüyordu. */
static int segment_is_param_decl(const char *seg, size_t len) {
    int has_ident = 0;
    for (size_t i = 0; i < len; i++) {
        char c = seg[i];
        if (gclc_ident_char(c)) { has_ident = 1; continue; }
        /* İzinli: ayraç boşluk, işaretçi '*'/'&', dizi köşeli parantezleri. */
        if (c == ' ' || c == '\t' || c == '*' || c == '&' ||
            c == '[' || c == ']') continue;
        return 0;
    }
    return has_ident;
}

/* 'p' ile 'end' arasındaki İLK üst-düzey ',' ayracı (yoksa 'end').
   Parantez/köşeli parantez derinliği sayılır, böylece "f(1,2)" veya
   "a[1,2]" içindeki virgüller ayraç sayılmaz. */
static const char *find_top_comma(const char *p, const char *end) {
    int d = 0;
    for (const char *q = p; q < end; q++) {
        if (*q == '(' || *q == '[') d++;
        else if (*q == ')' || *q == ']') { if (d > 0) d--; }
        else if (*q == ',' && d == 0) return q;
    }
    return end;
}

/* "," ile ayrılmış bir bildirim segmenti GERÇEKTEN bir tanımlayıcı adı mı
   üretir? Başlatıcı gövdeleri ve ifade parçaları bildirim DEĞİLDİR:
   "= { {1,20,\"Ali\"}, ... }" içindeki virgüller de üst-düzey görünür, ama
   bu segmentlerin içinde '{'/'}' vardır. Ayrıca bir ad RAKAMLA başlayamaz
   ("20" sayı literalidir, tamamlama listesine sızMAMALIdır). */
static const char *segment_decl_name(const char *seg, size_t len) {
    static char nm[GCLC_NAME];
    nm[0] = '\0';
    if (len == 0) return nm;
    if (memchr(seg, '{', len) || memchr(seg, '}', len)) return nm;
    split_declarator(seg, len, (char[GCLC_NAME]){0}, GCLC_NAME, nm, sizeof(nm));
    if (!nm[0] || !gclc_ident_start(nm[0])) nm[0] = '\0';
    return nm;
}

/* "Type a, b, c;" → TİP ilk bildirimden alınır, kalan ',' segmentlerinin son
   tanımlayıcısı aynı tipe bağlanır.

   ÖNEMLİ: eskiden tip TÜM satırdan çıkarılıyordu, o yüzden split_declarator
   son tanımlayıcıyı ad sanıp tipi ÇÖP bırakıyordu: "int x, y;" → y : "int x,"
   (x hiç kaydedilmiyordu), "struct S { int a, b; }" → yalnız b üyesi. */
static void parse_decl_list(GclScope *s, const char *s2, size_t len,
                            int rank, const char *file, int priv,
                            const char *type) {
    const char *end = s2 + len;
    const char *q = s2;
    while (q < end) {
        const char *c = find_top_comma(q, end);
        const char *nm = segment_decl_name(q, (size_t)(c - q));
        /* Yerleşik tip adları DEĞİŞKEN değildir: "int x, long" gibi bozuk
           kuyruklar sembol üretmesin. */
        if (nm[0] && !gclc_is_builtin_type(nm))
            scope_add_sym(s, nm, type, CIK_VAR, NULL, 0, priv, rank, file);
        if (c >= end) break;
        q = c + 1;
    }
}

static void parse_simple_decl(GclScope *s, const char *s2, size_t len,
                              int rank, const char *file, int priv) {
    const char *end = s2 + len;
    const char *c1 = find_top_comma(s2, end);
    char type[GCLC_NAME] = {0}, name[GCLC_NAME] = {0};
    split_declarator(s2, (size_t)(c1 - s2), type, sizeof(type), name, sizeof(name));
    if (!name[0]) return;
    if (gclc_is_builtin_type(name) && !type[0]) return;
    if (type_is_qualified(type)) return;          /* üye erişimi, bildirim değil */
    scope_add_sym(s, name, type, CIK_VAR, NULL, 0, priv, rank, file);
    if (c1 >= end) return;
    parse_decl_list(s, c1 + 1, (size_t)(end - (c1 + 1)), rank, file, priv, type);
}

/* Fonksiyon bildirimi: "ret name(params)". */
static void parse_function_decl(GclScope *s, const char *s2, size_t len,
                                int rank, const char *file, int priv) {
    size_t a, l;
    a = trim_span(s2, len, &l);
    const char *p = s2 + a;
    const char *lp = NULL;
    for (size_t i = 0; i < l; i++) { if (p[i] == '(') { lp = p + i; break; } }
    if (!lp) return;
    /* isim = '(' öncesi son ident */
    size_t ne = (size_t)(lp - p);
    while (ne > 0 && (p[ne - 1] == ' ' || p[ne - 1] == '\t')) ne--;
    size_t ns = ne;
    while (ns > 0 && gclc_ident_char(p[ns - 1])) ns--;
    if (ns == ne) return;
    char name[GCLC_NAME];
    copy_fixed(name, sizeof(name), p + ns, ne - ns);
    if (gclc_is_builtin_type(name)) return;

    /* dönüş tipi — KANONİK ada indirgenir.
       Eskiden ham metin saklanıyordu ("struct Point", "Point *"), üye
       çözümlemesi ise tipi ADIYLA arıyor: bu yüzden kullanıcı fonksiyonunun
       dönüşü asla zincirlenemiyordu (`makePoint(1).` → hiçbir öneri, sessiz). */
    size_t ta, tl;
    ta = trim_span(p, ns, &tl);
    char ret[GCLC_NAME];
    copy_fixed(ret, sizeof(ret), p + ta, tl);
    normalize_type_name(ret, sizeof(ret));

    /* "Raylib.InitWindow(800, 600)" / "Stdio.printf(...)" → ÜYE ÇAĞRISI.
       İsimden hemen önceki metin '.' ile bitiyorsa bu bir FONKSİYON BİLDİRİMİ
       değildir; çağrının kendisidir. Eskiden çağrılan ad ("InitWindow",
       "DrawText", "printf") global bir fonksiyon sembolü olarak kapsama
       giriyordu ve `Raylib.` yazılmadan düz önekte öneriliyordu — modül
       üyeleri yalnızca "Modül.üye" olarak önerilmelidir (kullanıcı
       bildirimi: "Raylib. yazmadan raylib içindekiler görünüyor"). */
    if (type_is_qualified(ret)) return;

    /* parametreler (kaba): parantez içi */
    const char *rp = lp + 1;
    const char *re = rp;
    int d = 1;
    for (; re < p + l; re++) {
        if (*re == '(') d++;
        else if (*re == ')') { d--; if (d == 0) break; }
    }
    char sig[192];
    copy_fixed(sig, sizeof(sig), rp, (size_t)(re - rp));

    /* private görünürlük (public/private anahtarı zaten ayıklandı) */
    scope_add_sym(s, name, ret, CIK_FUNC, sig, 1, priv, rank, file);

    /* Parametreleri de ekle (fonksiyon gövdesi tamamlaması için). */
    const char *q = rp;
    while (q < re) {
        const char *comma = q;
        int dd = 0;
        while (comma < re) {
            if (*comma == '(' || *comma == '[') dd++;
            else if (*comma == ')' || *comma == ']') { if (dd > 0) dd--; }
            else if (*comma == ',' && dd == 0) break;
            comma++;
        }
        char ptype[GCLC_NAME] = {0}, pname[GCLC_NAME] = {0};
        size_t seglen = (size_t)(comma - q);
        split_declarator(q, seglen, ptype, sizeof(ptype), pname, sizeof(pname));
        /* YALNIZCA "Tip isim" deseni parametre sayılır. Argüman konumunda
           ifade olan segmentler ("800", "20", "x + 1", "Raylib.WHITE")
           atılır; aksi halde çağrılara yazılan sayılar kapsama CIK_PARAM
           olarak girip tamamlama listesinde öneri olarak görünüyordu
           (kullanıcı bildirimi: 20, 60, 600, 800). */
        if (segment_is_param_decl(q, seglen) && ptype[0] &&
            pname[0] && gclc_ident_start(pname[0]) &&
            !type_is_qualified(ptype) &&
            strcmp(pname, "void") != 0 && !gclc_is_builtin_type(pname))
            scope_add_sym(s, pname, ptype, CIK_PARAM, NULL, 0, 0, 1, file);
        q = comma + 1;
    }
}

/* Üye alan ekle (struct/enum gövdesi). */
static void parse_member_list(GclScope *s, GclTypeDef *t, const char *s2, size_t len) {
    const char *end = s2 + len;
    const char *seg = s2;
    while (seg < end) {
        /* segment sonu ';' (veya satır sonu) */
        const char *sc = seg;
        int depth = 0;
        while (sc < end) {
            if (*sc == '(' || *sc == '[') depth++;
            else if (*sc == ')' || *sc == ']') { if (depth > 0) depth--; }
            else if ((*sc == ';') && depth == 0) break;
            sc++;
        }
        if (sc > seg) {
            if (t->is_enum) {
                /* Enum gövdesinde ',' ile ayrılmış HER AD bir sabittir; tip
                   kavramı yoktur ve "name = 3" başlatıcısı atılır.
                   Eskiden tüm gövde TEK bildirim sanılıyordu: "RED, GREEN, BLUE"
                   → tip "RED, GREEN," ve yalnızca SON ad üye oluyordu; yani
                   kullanıcı `Color.` yazdığında RED/GREEN hiç görünmüyordu. */
                const char *q = seg;
                while (q < sc) {
                    const char *c = find_top_comma(q, sc);
                    const char *name_end = c;
                    for (const char *k = q; k < c; k++)
                        if (*k == '=') { name_end = k; break; }
                    char nm[GCLC_NAME] = {0};
                    split_declarator(q, (size_t)(name_end - q),
                                     (char[GCLC_NAME]){0}, GCLC_NAME, nm, sizeof(nm));
                    if (nm[0] && gclc_ident_start(nm[0]))
                        type_push_member(t, nm, "", CIK_ENUM_VAL);
                    if (c >= sc) break;
                    q = c + 1;
                }
            } else {
                /* "Type a, b;" → TİP ilk bildirimden alınır; kalan ','
                   segmentlerinden yalnız AD alınır (parse_decl_list ile aynı
                   kural). Eskiden tip tüm segmentten çıkarılıyordu:
                   "int a, b" → yalnız 'b' üyesi, tipi de "int a," oluyordu. */
                const char *c1 = find_top_comma(seg, sc);
                char type[GCLC_NAME] = {0}, name[GCLC_NAME] = {0};
                split_declarator(seg, (size_t)(c1 - seg), type, sizeof(type),
                                 name, sizeof(name));
                if (name[0])
                    type_push_member(t, name, type, CIK_FIELD);
                const char *q = c1;
                while (q < sc) {
                    const char *c = find_top_comma(q + 1, sc);
                    const char *nm = segment_decl_name(q + 1, (size_t)(c - (q + 1)));
                    if (nm[0]) type_push_member(t, nm, type, CIK_FIELD);
                    if (c >= sc) break;
                    q = c;
                }
            }
        }
        seg = sc + 1;
    }
}
/* ---- Satır / preprocessor yardımcıları ---- */

static int starts_word(const char *s, size_t len, const char *w) {
    size_t n = strlen(w);
    if (len < n) return 0;
    if (strncmp(s, w, n) != 0) return 0;
    return (len == n) || !gclc_ident_char(s[n]);
}

static int is_control_word(const char *s, size_t len) {
    static const char *kw[] = { "return","if","else","for","while","switch",
                                "case","do","break","continue","goto","sizeof",
                                "new","delete","await","yield", NULL };
    for (int i = 0; kw[i]; i++)
        if (starts_word(s, len, kw[i])) return 1;
    return 0;
}

static size_t skip_qualifiers(const char *s, size_t len, int *priv) {
    size_t i = 0;
    *priv = 0;
    for (;;) {
        while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
        if (starts_word(s + i, len - i, "public"))        { i += 6; continue; }
        if (starts_word(s + i, len - i, "private"))       { *priv = 1; i += 7; continue; }
        if (starts_word(s + i, len - i, "global"))        { i += 6; continue; }
        if (starts_word(s + i, len - i, "const"))         { i += 5; continue; }
        if (starts_word(s + i, len - i, "static"))        { i += 6; continue; }
        if (starts_word(s + i, len - i, "inline"))        { i += 6; continue; }
        break;
    }
    return i;
}

/* Üye listesini DERİN kopyala (dinamik dizi; sığ kopya çift serbest yapar).
   `src` ile `dst` aynı scope'ta farklı tiplerdir; dst sıfırdan büyür. */
static void copy_type_members(GclTypeDef *dst, const GclTypeDef *src) {
    if (!dst || !src) return;
    for (int i = 0; i < src->member_count; i++)
        type_push_member(dst, src->members[i].name, src->members[i].type,
                         src->members[i].kind);
}

static void handle_preproc(GclScope *s, const char *ln, size_t L,
                           const char *file, int rank) {
    size_t i = 1;
    while (i < L && (ln[i] == ' ' || ln[i] == '\t')) i++;
    size_t ds = i;
    while (i < L && gclc_ident_char(ln[i])) i++;
    char dir[32];
    copy_fixed(dir, sizeof(dir), ln + ds, i - ds);
    if (strcmp(dir, "native") == 0) {
        size_t j = i;
        while (j < L && (ln[j] == ' ' || ln[j] == '\t')) j++;
        if (j < L && (ln[j] == '<' || ln[j] == '"')) {
            char close = (ln[j] == '<') ? '>' : '"';
            size_t a = j + 1, e = a;
            while (e < L && ln[e] != close) e++;
            char mod[64];
            copy_fixed(mod, sizeof(mod), ln + a, e - a);
            if (mod[0]) scope_add_sym(s, mod, "", CIK_MODULE, NULL, 0, 0, rank, file);
        }
    } else if (strcmp(dir, "define") == 0) {
        size_t j = i;
        while (j < L && (ln[j] == ' ' || ln[j] == '\t')) j++;
        size_t ns = j;
        while (j < L && gclc_ident_char(ln[j])) j++;
        char nm[GCLC_NAME];
        copy_fixed(nm, sizeof(nm), ln + ns, j - ns);
        if (nm[0]) scope_add_sym(s, nm, "", CIK_MACRO, NULL, 0, 0, rank, file);
    }
}

/* Enum üyeleri C/GCL'de ÇIPLAK sabittir: `enum Day { MONDAY, TUESDAY };`
   tanımından sonra kullanıcı `today = MONDAY;` yazar (`Day.MONDAY` değil).
   Üye listesi tipe kaydedildikten sonra aynı adlar düz kapsama da eklenir;
   aksi hâlde `MON` yazıldığında MONDAY HİÇ önerilmez, yalnızca `Day.` ile
   görünürdü ve GCL bunu gerektirmez.

   Sabitin TİPİ enum'un adıdır (`MONDAY : Day`), yani `Day d = MONDAY;`
   yazılırsa tip bağı da kurulur. Anonim enum'da (adı "?" iken) tip boş
   bırakılır — uydurma bir tip bağı kurmaktansa hiç bağ kurmamak doğrudur. */
static void register_enum_constants(GclScope *s, int idx, int rank,
                                   const char *file, int priv) {
    if (!s || idx < 0 || idx >= s->type_count) return;
    if (!s->types[idx].is_enum) return;
    const char *tname = s->types[idx].name;
    if (tname[0] == '\0' || strcmp(tname, "?") == 0) tname = "";
    for (int i = 0; i < s->types[idx].member_count; i++) {
        const char *nm = s->types[idx].members[i].name;
        if (nm[0])
            scope_add_sym(s, nm, tname, CIK_ENUM_VAL, NULL, 0, priv, rank, file);
    }
}

/* '}' sonrası alias'ı işle ve anonim tipi adlandır.
   ÖNEMLİ: `gcl_scope_add_type()` `s->types`'ı realloc ile TAŞIYABİLİR; bu
   yüzden elemanlara işaretçi (GclTypeDef*) realloc BOYUNCA saklanmaz, yalnızca
   dizin (idx) saklanır ve her erişimde yeniden adreslenir. Aksi hâlde
   serbest bırakılmış belleği okumak (use-after-free) ve bozuk `member_count`
   ile sınır dışına yazmak (yığın bozulması) söz konusuydu. */
static void finish_type_alias(GclScope *s, int idx, const char *after, size_t alen,
                              const char *file) {
    if (idx < 0) return;
    size_t a, l;
    a = trim_span(after, alen, &l);
    const char *an = after + a;
    size_t ae = 0;
    while (ae < l && gclc_ident_char(an[ae])) ae++;
    if (ae == 0) return;
    char alias[GCLC_NAME];
    copy_fixed(alias, sizeof(alias), an, ae);
    if (alias[0] == '\0') return;

    const char *cur_name = s->types[idx].name;
    int cur_is_enum = s->types[idx].is_enum;
    if (cur_name[0] == '\0' || strcmp(cur_name, "?") == 0 ||
        strcmp(cur_name, alias) == 0) {
        copy_fixed(s->types[idx].name, sizeof(s->types[idx].name),
                   alias, strlen(alias));
        return;
    }
    int ai = gcl_scope_add_type(s, alias, cur_is_enum, file, 0);
    if (ai >= 0) copy_type_members(&s->types[ai], &s->types[idx]);
}

void gcl_scope_add_source(GclScope *s, const char *file, const char *text,
                          size_t len, int default_rank) {
    if (!s || !text) return;
    char *src = strip_noise(text, len);
    if (!src) return;

    int cur_type = -1;
    size_t i = 0;
    while (i < len) {
        size_t ls = i;
        while (i < len && src[i] != '\n') i++;
        size_t le = i;
        if (i < len) i++;
        const char *line_all = src + ls;
        size_t line_len = le - ls;
        size_t a, L;
        a = trim_span(line_all, line_len, &L);
        const char *ln = line_all + a;
        if (L == 0) continue;

        if (cur_type >= 0) {
            const char *rb = (const char *)memchr(ln, '}', L);
            if (rb) {
                size_t before = (size_t)(rb - ln);
                parse_member_list(s, &s->types[cur_type], ln, before);
                finish_type_alias(s, cur_type, rb + 1,
                                  (size_t)((ln + L) - (rb + 1)), file);
                /* Çok satıra yayılmış enum gövdesi burada kapanır. */
                register_enum_constants(s, cur_type, default_rank, file, 0);
                cur_type = -1;
            } else {
                parse_member_list(s, &s->types[cur_type], ln, L);
            }
            continue;
        }

        if (ln[0] == '#') {
            handle_preproc(s, ln, L, file, default_rank);
            continue;
        }

        if (is_control_word(ln, L)) continue;

        int priv = 0;
        size_t o = skip_qualifiers(ln, L, &priv);
        if (o >= L) continue;
        const char *d = ln + o;
        size_t dl = L - o;

        int is_typedef = starts_word(d, dl, "typedef");
        const char *body = d;
        size_t bodylen = dl;
        if (is_typedef) {
            size_t k = 7;
            while (k < dl && (d[k] == ' ' || d[k] == '\t')) k++;
            body = d + k;
            bodylen = dl - k;
        }
        int is_struct = starts_word(body, bodylen, "struct");
        int is_enum   = starts_word(body, bodylen, "enum");
        int is_union  = starts_word(body, bodylen, "union");

        if (is_struct || is_enum || is_union) {
            size_t k = is_struct ? 6 : (is_enum ? 4 : 5);
            while (k < bodylen && (body[k] == ' ' || body[k] == '\t')) k++;
            size_t ns = k;
            while (k < bodylen && gclc_ident_char(body[k])) k++;
            char tname[GCLC_NAME];
            copy_fixed(tname, sizeof(tname), body + ns, k - ns);

            const char *lb = (const char *)memchr(body, '{', bodylen);
            const char *semi = (const char *)memchr(body, ';', bodylen);
            /* Etiket ile '{' ARASINDA bir tanımlayıcı varsa bu bir TİP TANIMI
               değil DEĞİŞKEN bildirimidir: "struct Person people[3] = {"
               başlatıcının '{' i yüzünden eskiden tip tanımı sanılıyor,
               'people' hiç sembol olmuyor ve "people[i].id" çalışmıyordu
               (örnek: 8_typedef_struct.gcsf). */
            int decl_like = 0;
            if (lb) {
                for (const char *q = body + k; q < lb; q++) {
                    if (gclc_ident_char(*q)) { decl_like = 1; break; }
                }
            }
            if (decl_like || (!lb && semi)) {
                /* "struct Foo bar;" / "struct Foo bar[N] = {...};" → bildirim.
                   AMA "struct Foo make(int a);" bir FONKSİYON prototipidir:
                   `struct` etiketi yüzünden bu dala giriyordu ve
                   parse_simple_decl() son tanımlayıcıyı (parametre adını)
                   değişken sanıp ÇÖP bir sembol üretiyordu
                   ("a : Point make(int"); fonksiyon adı ve dönüş tipi kapsama
                   HİÇ girmiyordu → "make(...)." zincirlenemezdi.
                   Ölçüt düz bildirimle aynı: '(' hem ';' hem '=' öncesinde. */
                if (!decl_like && !lb && semi) {
                    const char *flp = (const char *)memchr(body, '(', bodylen);
                    const char *feq = (const char *)memchr(body, '=', bodylen);
                    if (flp && flp < semi && (!feq || flp < feq)) {
                        parse_function_decl(s, d, dl, default_rank, file, priv);
                        continue;
                    }
                }
                parse_simple_decl(s, d, dl, default_rank, file, priv);
                continue;
            }
            char use_name[GCLC_NAME];
            copy_fixed(use_name, sizeof(use_name),
                       tname[0] ? tname : "?", strlen(tname[0] ? tname : "?"));
            int idx = gcl_scope_add_type(s, use_name, is_enum, file, priv);
            if (idx < 0) continue;
            if (!lb) { cur_type = idx; continue; }
            const char *rem = lb + 1;
            size_t remlen = (size_t)((body + bodylen) - rem);
            const char *rb = (const char *)memchr(rem, '}', remlen);
            if (rb) {
                parse_member_list(s, &s->types[idx], rem, (size_t)(rb - rem));
                finish_type_alias(s, idx, rb + 1,
                                  (size_t)((rem + remlen) - (rb + 1)), file);
                /* finish_type_alias'tan SONRA: anonim enum'un adı orada
                   belirlenir, sabitler o adı tip olarak almalı. */
                register_enum_constants(s, idx, default_rank, file, priv);
            } else {
                parse_member_list(s, &s->types[idx], rem, remlen);
                cur_type = idx;
            }
            continue;
        }

        if (is_typedef) {
            /* typedef <type> Alias; → yeni tip adı (üyesiz) */
            const char *semi = (const char *)memchr(body, ';', bodylen);
            size_t blen = semi ? (size_t)(semi - body) : bodylen;
            char ty[GCLC_NAME] = {0}, nm[GCLC_NAME] = {0};
            split_declarator(body, blen, ty, sizeof(ty), nm, sizeof(nm));
            if (nm[0] && !gclc_is_builtin_type(nm)) {
                int idx = gcl_scope_add_type(s, nm, 0, file, priv);
                (void)idx;
            }
            continue;
        }

        /* Normal bildirim: fonksiyon mu, değişken mi? */
        const char *lp = (const char *)memchr(d, '(', dl);
        const char *eq = (const char *)memchr(d, '=', dl);
        const char *semi = (const char *)memchr(d, ';', dl);
        if (lp && (!eq || lp < eq) && (!semi || lp < semi)) {
            parse_function_decl(s, d, dl, default_rank, file, priv);
        } else if (semi || eq) {
            parse_simple_decl(s, d, dl, default_rank, file, priv);
        }
    }

    free(src);
}

void gcl_scope_build(GclScope *s, const char *file, const char *text, size_t len,
                     const char *workspace) {
    if (!s) return;
    if (file) copy_fixed(s->main_file, sizeof(s->main_file), file, strlen(file));
    if (workspace) copy_fixed(s->workspace, sizeof(s->workspace), workspace, strlen(workspace));
    gcl_scope_add_source(s, file, text, len, 2);
}

/* 'src' kapsamını 'dst' içine DERİN kopyala (§12).
   Semboller düz dizidir (içlerinde işaretçi YOK) → memcpy güvenlidir; hash
   tablosu yeniden kurulur. Tiplerin üye listeleri DİNAMİK olduğundan sığ
   kopya çift serbest yapardı → üyeler tek tek eklenir. 'dst' boş olmalıdır. */
void gcl_scope_copy_into(const GclScope *src, GclScope *dst) {
    if (!src || !dst) return;

    /* 1) Semboller: bit düzeyinde kopya + hash tablosunu yeniden inşa. */
    if (src->sym_count > 0 && scope_ensure_syms(dst, src->sym_count)) {
        memcpy(dst->syms, src->syms, (size_t)src->sym_count * sizeof(GclSym));
        dst->sym_count = src->sym_count;
        for (int i = 0; i < dst->sym_count; i++) scope_sym_slots_insert(dst, i);
    }

    /* 2) Tipler: üye listeleri dinamik → derin kopya. */
    for (int i = 0; i < src->type_count; i++) {
        const GclTypeDef *st = &src->types[i];
        int idx = gcl_scope_add_type(dst, st->name, st->is_enum, st->file,
                                     st->is_private);
        if (idx >= 0) copy_type_members(&dst->types[idx], st);
    }

    copy_fixed(dst->main_file, sizeof(dst->main_file), src->main_file,
               strlen(src->main_file));
    copy_fixed(dst->workspace, sizeof(dst->workspace), src->workspace,
               strlen(src->workspace));
}
/* ---- Anahtar sözcükler & preprocessor direktifleri (§5.3, §7.3) ---- */

/* ide_font.c'deki gcl_keywords[] ile birebir aynı içerik. Motor tek başına
   derlenebilsin (testler raylib link etmez) diye liste buraya taşındı. */
static const char *gclc_keywords[] = {
    /* veri tipleri */
    "int","short","long","float","double","void","char","bool",
    /* kontrol akışı */
    "return","if","else","while","for","break","continue",
    "switch","case","default","do","goto",
    /* yapılar */
    "struct","enum","typedef","const","sizeof",
    /* görünürlük / yaşam */
    "global","local","inline","public","private",
    /* yerleşik kavramlar.
       DİKKAT: "printf"/"scanf"/"strlen" BURADAN KALDIRILDI — bunlar GCL
       anahtar sözcüğü DEĞİL, Stdio modülünün üyeleridir ve yalnızca
       "Stdio.printf" olarak önerilmelidir. Çıplak önerildiklerinde imleç
       boş bir satırdayken "printf" listeye sızıyordu (todo #1). */
    "true","false","null",
    NULL
};

static const char *gclc_directives[] = {
    "include","lib","native","extern","register","define","undef",
    "if","ifdef","ifndef","elif","else","endif",
    "pragma","warning","error","debug", NULL
};

const char *const *gclc_keyword_list(void)  { return gclc_keywords; }
const char *const *gclc_directive_list(void) { return gclc_directives; }
const char *const *gclc_builtin_type_list(void) { return gclc_builtin_types; }
