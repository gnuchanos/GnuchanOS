/*
 * gcl_complete.h — GCL "SMART" tamamlama motoru (facade + veri tipleri).
 *
 * Bu başlık motorun kamuya açık yüzüdür: bağlam/öğe/ sonuç tipleri ve tek
 * giriş noktası `gcl_complete_query()`. Editör (ide_editor.c) yalnızca
 * "imleç + buffer" sağlar; hangi önerinin listeleneceğine motor karar verir
 * (bkz. todo.md §3).
 *
 * Motor bağımsızdır ve UI'dan ayrıdır; birim testleri (tests/complete) bu
 * API üzerinden çalışır.
 */
#ifndef GCL_COMPLETE_H
#define GCL_COMPLETE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Temel yardımcılar                                                   */
/* ------------------------------------------------------------------ */

/* Bir karakter GCL tanımlayıcı karakteri mi ([A-Za-z0-9_])? */
static inline int gclc_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static inline int gclc_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* GCL builtin/primitive tip adı mı? (complete_scope.c'de tanımlı) */
int gclc_is_builtin_type(const char *name);

/* ------------------------------------------------------------------ */
/* Bağlam türleri (§3.2)                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    CTX_NONE = 0,        /* tamamlama KAPALI (string/yorum/çözümsüz üye) */
    CTX_MEMBER,          /* X.  → X tipinin üyeleri                       */
    CTX_IDENT,           /* düz önek (ad, keyword, tip)                   */
    CTX_CALL_ARGS,       /* f( ... imleç ... )                            */
    CTX_TYPE_DECL,       /* struct/enum/typedef gövdesi                   */
    CTX_VALUE_ASSIGN,    /* "Type x = " sonrası                           */
    CTX_PREPROC,         /* satır başı '#'                                 */
    CTX_NATIVE_MODULE,   /* #native <...>                                  */
    CTX_INCLUDE,         /* #include <...>                                 */
    CTX_EXTERN,          /* #extern <...>                                  */
    CTX_LIB,             /* #lib <...>                                     */
    CTX_REGISTER,        /* #register ...                                  */
    CTX_DOC_COMMENT,     /* ///                                            */
    CTX_STRING_PATH      /* "assets/▮" (yol bekleyen fonksiyon argümanı)   */
} GclContextKind;

/* ------------------------------------------------------------------ */
/* Öğe türleri                                                         */
/* ------------------------------------------------------------------ */

typedef enum {
    CIK_FUNC = 0,
    CIK_VAR,
    CIK_FIELD,
    CIK_TYPE,
    CIK_ENUM_VAL,
    CIK_MACRO,
    CIK_MODULE,
    CIK_KEYWORD,
    CIK_CONST,
    CIK_PARAM,
    CIK_LABEL,
    CIK_FILE             /* assets/ yolu (string-yol tamamlaması)          */
} GclItemKind;

/* Tek bir tamamlama önerisi. Tüm char* alanlar motora AİTTİR (sahipli);
   `gcl_complete_result_free()` serbest bırakır. */
typedef struct {
    char        *label;        /* gösterilen ad      ("GetMousePosition") */
    char        *insert;       /* eklenen metin      (opsiyonel)          */
    GclItemKind  kind;
    char        *type;         /* dönüş/değişken tipi ("Vector2")         */
    char        *params;       /* parametre listesi ("int w, int h")      */
    char        *signature;    /* "Vector2 GetMousePosition(void)"        */
    char        *doc;          /* kısa açıklama (opsiyonel)               */
    char        *origin_file;  /* kaynak dosya (köken sıralaması)         */
    int          scope_rank;   /* 0 local .. 4 builtin (sıralama, §10)    */
    int          score;        /* sıralama puanı (rank aşamasında)        */
} GclItem;

typedef struct {
    GclItem *items;
    int      count;
    int      cap;
    int      best_index;       /* Enter'ın seçeceği varsayılan            */
    int      suppressed;       /* 1 → popup KAPALI (liste gösterilmez)    */
    int      handled;          /* 1 → motor bu bağlamı tamamen işledi     */
    GclContextKind context;    /* algılanan bağlam                        */

    /* İmza yardımı (§11.2) */
    int      have_signature;
    char     sig_label[256];
    char     sig_params[256];
    int      sig_active_param; /* 0-tabanlı aktif parametre               */

    /* Tanı (diagnostic) — ör. printf {} yer-tutucu sayısı uyuşmazlığı (§5.4).
       UI uyarı şeridi için; liste/popup durumundan BAĞIMSIZDIR. */
    int      have_diagnostic;
    char     diag_message[192];

    /* --- DAHİLİ: dedup hash tablosu (gcl_complete_result_push) ---
       Eskiden aynı etiket+tür denetimi için listenin TAMAMI doğrusal
       taranıyordu → O(n²). 670 üyeli `Raylib.` listesinde ~224k, düz önekte
       ~480k strcmp demekti ve S7 bütçesinin yarısını yiyordu. slots[h] == 0
       → boş, aksi halde (item_index + 1). Dışarıdan kullanılmaz. */
    int     *dedup_slots;
    int      dedup_cap;        /* 2^n */
    int      dedup_used;
} GclCompletionResult;

/* ------------------------------------------------------------------ */
/* Sonuç yaşam döngüsü                                                 */
/* ------------------------------------------------------------------ */

void      gcl_complete_result_init(GclCompletionResult *r);
void      gcl_complete_result_free(GclCompletionResult *r);

/* Öğe ekler (tüm string'ler kopyalanır). Boş "label" eklenmez.
   Başarıda öğe indeksini, aksi halde -1 döndürür. */
int       gcl_complete_result_push(GclCompletionResult *r,
                                   const char *label, const char *insert,
                                   GclItemKind kind, const char *type,
                                   const char *params, const char *signature,
                                   const char *doc, const char *origin_file);

/* Liste yeniden dizildiğinde (gcl_rank_apply) dedup indeksleri geçersiz olur;
   sıralayıcı bunu çağırıp tabloyu temizler. */
void      gcl_complete_dedup_invalidate(GclCompletionResult *r);

/* ------------------------------------------------------------------ */
/* Facade                                                              */
/* ------------------------------------------------------------------ */

/*
 * İmleç bağlamındaki tamamlamayı hesapla.
 *   file      : aktif dosya yolu (köken/sıralama; NULL olabilir)
 *   text      : tüm buffer metni (NULL sonlandırmalı olması gerekmez)
 *   text_len  : buffer uzunluğu
 *   cursor    : imleç bayt offset'i
 *   workspace : proje kökü (include/lib/scripts taraması; NULL olabilir)
 *
 * Sonuç `out` içine yazılır; çağırıcı sonunda gcl_complete_result_free()
 * çağırmalıdır. Dönüş: yönetilen bağlam (GclContextKind).
 */
GclContextKind gcl_complete_query(const char *file, const char *text,
                                  size_t text_len, size_t cursor,
                                  const char *workspace,
                                  GclCompletionResult *out);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_H */
