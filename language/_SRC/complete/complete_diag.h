/*
 * complete_diag.h — Motor tanıları (diagnostic, §5.4).
 *
 * GCL'de printf yer-tutucusu `{}`'tir (`%s` yok): `printf("a={} b={}", x, y)`.
 * Bu modül, imleç bir printf çağrısının argümanları içindeyken `{}` sayısı ile
 * takip eden argüman sayısını karşılaştırır ve uyuşmazlığı bildirir.
 *
 * Tanı, popup/liste durumundan BAĞIMSIZDIR (CTX_NONE olsa da üretilebilir).
 */
#ifndef GCL_COMPLETE_DIAG_H
#define GCL_COMPLETE_DIAG_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * printf {} yer-tutucu denetimi (§5.4). İmleç, kalıp string'i KAPANMIŞ bir
 * printf çağrısının içindeyse `{}` sayısı ile argüman sayısını karşılaştırır.
 * Uyuşmazlıkta out->have_diagnostic = 1 ve out->diag_message doldurulur.
 * Aksi halde out'un tanı alanlarına DOKUNMAZ.
 */
void gcl_diag_check_printf(const char *text, size_t text_len, size_t cursor,
                           GclCompletionResult *out);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_DIAG_H */
