/*
 * complete_scope.h — Kapsam & sembol tablosu (§7).
 *
 * Metinden (ve istenirse include edilen dosyalardan) sembolleri çıkarır;
 * her sembole TİP BAĞI ekler. complete_type.c bu tabloyu üye çözümlemesi için
 * kullanır. Model düz/tekil tutulur; motor her sorguda yeniden kurar
 * (artımlı cache complete_index.c'de).
 */
#ifndef GCL_COMPLETE_SCOPE_H
#define GCL_COMPLETE_SCOPE_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCLC_MAX_SYMS    3072
#define GCLC_MAX_TYPES   256
#define GCLC_MAX_MEMBERS 64     /* yalnız başlangıç tahmini; artık sert sınır değil */
#define GCLC_NAME        96

typedef struct {
    char        name[GCLC_NAME];
    char        type[GCLC_NAME];   /* alanın tipi (ikinci seviye zincir için) */
    GclItemKind kind;
    int         vis_private;
} GclMember;

/* Üye listesi ARTIK DİNAMİK (members + member_cap). Eskiden sabit
   `GclMember members[64]` gömülüydü: her GclTypeDef ~13 KB yer kaplıyordu ve
   43 KB'lık dosyada 300 tip için her sorguda ~4 MB sıfırlama + realloc
   kopyası yapılıyordu (S7 ihlali). Dinamik dizi bunu ~600 b'ye indirir. */
typedef struct {
    char        name[GCLC_NAME];
    int         is_enum;
    int         is_private;
    char        file[512];
    GclMember  *members;
    int         member_count;
    int         member_cap;
} GclTypeDef;

typedef struct {
    char        name[GCLC_NAME];
    char        type[GCLC_NAME];   /* değişken/fonksiyon dönüş tipi */
    char        sig[192];          /* fonksiyon imzası (params) */
    GclItemKind kind;
    int         vis_private;
    int         scope_rank;        /* 0 local,1 param,2 global,3 include,4 builtin */
    int         is_call;           /* fonksiyon ise 1 */
    char        file[512];
} GclSym;

typedef struct {
    GclSym     *syms;
    int         sym_count;
    int         sym_cap;
    GclTypeDef *types;
    int         type_count;
    int         type_cap;
    char        main_file[512];
    char        workspace[1024];

    /* --- Sembol adı → indeks hash tablosu (açık adresleme, doğrusal sonda).
           Sembol ekleme/arama O(1) olsun diye (§12, S7): eskiden her ekleme
           tüm tabloyu tarayıp O(n²) yapıyordu (43 KB dosyada ~10 ms).
           slots[h] == 0 → boş; aksi halde (sym_index + 1). --- */
    int        *sym_slots;
    int         sym_slots_cap;      /* 2^n */
    int         sym_slots_used;
} GclScope;

GclScope *gcl_scope_create(void);
void      gcl_scope_destroy(GclScope *s);
void      gcl_scope_reset(GclScope *s);

/* Bir kaynak dosya metnini tara ve sembolleri ekle. scope_rank verilir. */
void gcl_scope_add_source(GclScope *s, const char *file, const char *text,
                          size_t len, int default_rank);

/* Kolaylık: aktif dosyayı (default_rank=2) tara. */
void gcl_scope_build(GclScope *s, const char *file, const char *text, size_t len,
                     const char *workspace);

/* 'src' kapsamını 'dst' içine DERİN kopyala (semboller + tipler + hash tablosu).
   Buffer içerik hash'i ile önbelleğe alınan tarama sonucunu taze bir kapsama
   aktarmak için kullanılır (§12); 'dst' önceden boş olmalıdır. */
void gcl_scope_copy_into(const GclScope *src, GclScope *dst);

const GclSym     *gcl_scope_find(const GclScope *s, const char *name);
const GclTypeDef *gcl_scope_find_type(const GclScope *s, const char *name);
int               gcl_scope_add_type(GclScope *s, const char *name, int is_enum,
                                     const char *file, int priv);

/* Yardımcı: 'name' bir kullanıcı birleşik tip adı mı? */
int gcl_scope_is_type(const GclScope *s, const char *name);

/* GCL anahtar sözcükleri ve preprocessor direktifleri (NULL ile biten).
   Motor raylib'e bağlanmadığından liste burada tutulur (ide_font.c'deki
   gcl_keywords ile aynı içerik; tek kaynak motordur). */
const char *const *gclc_keyword_list(void);
const char *const *gclc_directive_list(void);
/* GCL builtin/primitive tip adları (NULL ile biten). Keyword listesinden
   farklı olarak yalnız tip adları: int, float, gcChar, ... */
const char *const *gclc_builtin_type_list(void);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_SCOPE_H */
