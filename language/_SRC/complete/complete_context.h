/*
 * complete_context.h — İmleç bağlam analizi.
 *
 * İmlecin sözdizimsel durumunu çıkarır: string/yorum içinde mi, üye erişimi
 * (X.), zincir (a.b.c.), fonksiyon argümanı, preprocessor satırı vb.
 * Eski motordaki "iki nokta koruması" (strchr(prefix,'.') != last_dot) burada
 * KALDIRILMIŞTIR; zincirleme tamamlama desteklenir (bug fix, §1.2).
 */
#ifndef GCL_COMPLETE_CONTEXT_H
#define GCL_COMPLETE_CONTEXT_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCLC_MAX_CHAIN 16
#define GCLC_CHAIN_NAME 128

typedef struct {
    /* Çapa (anchor) öneki: imlecin hemen solundaki [A-Za-z0-9_] dizisi. */
    size_t         prefix_begin;   /* bayt offset */
    size_t         prefix_end;     /* == cursor */
    char           prefix[256];    /* aktif kelime */

    /* Bağlam */
    GclContextKind kind;
    int            in_string;
    int            in_comment;
    int            line_is_preproc;
    int            call_depth;      /* f( içindeysek > 0 */

    /* Üye zinciri (soldan sağa). chain[chain_len-1] en yakın tabandır. */
    int            chain_len;
    char           chain[GCLC_MAX_CHAIN][GCLC_CHAIN_NAME];
    int            chain_is_call[GCLC_MAX_CHAIN]; /* "GetMousePosition()" ise 1 */

    /* Preprocessor bilgisi */
    char           directive[32];   /* "#native" -> "native" */
    char           pp_arg[256];     /* '<' sonrası ya da direktif sonrası metin */
    int            in_angle;        /* '<' ... '>' içinde */

    /* "Type var = " bağlamı */
    int            has_decl_type;
    char           decl_type[128];  /* "Type" */

    /* struct/enum tanım gövdesi mi */
    int            in_type_body;

    /* String-yol bağlamı (§Faz 6): imleç, yol bekleyen bir fonksiyonun
       (LoadTexture, readFile, …) string argümanı içinde. */
    int            is_path_string;
    char           str_prefix[512];  /* tırnak içinde yazılan tam yol */
} GclContext;

/* İmleç konumundaki bağlamı analiz et. text/text_len: tüm buffer. */
void gcl_context_analyze(const char *text, size_t text_len, size_t cursor,
                         GclContext *ctx);

/* Bir üye erişiminin tabanı: chain'i verir; boşsa (düz önek) chain_len==0. */
static inline int gcl_context_is_member(const GclContext *c) {
    return c->kind == CTX_MEMBER && c->chain_len > 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_CONTEXT_H */
