/*
 * complete_type.h — Tip çıkarımı ve üye çözümleme (§6).
 *
 * `X.` yazıldığında X'in TİPİNİ çözüp yalnızca o tipin üyelerini döndürür.
 * Çözüm başarısızsa liste BOŞ kalır ve popup KAPANIR (S2 / §6.3): "eşleşmezse
 * başka struct'ın üyelerini göster" davranışı KESİNLİKLE yasaktır.
 */
#ifndef GCL_COMPLETE_TYPE_H
#define GCL_COMPLETE_TYPE_H

#include "gcl_complete.h"
#include "complete_scope.h"
#include "complete_context.h"
#include "complete_native.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Üye erişimi zincirini çöz: ctx->chain[0..len-1] "X.▮" tabanıdır.
 *   - chain[0] native modül (Raylib) → modül üyeleri
 *   - chain[0] değişken → değişkenin tipinin üyeleri
 *   - chain[0] tip adı → (statik) tipin üyeleri
 * Ara adımlar fonksiyon dönüş tipini / alan tipini takip eder (zincirleme).
 *
 * Başarıda 1 döndürür (out doldurulur). Çözümlenemezse 0 döndürür → çağıran
 * popup'ı KAPATIR.
 */
int gcl_type_resolve_members(const GclScope *scope, const GclContext *ctx,
                             const char *file, GclCompletionResult *out);

/* Bir adın tipini çöz: değişken→bildirilen tip, native struct→kendisi. */
int gcl_type_of_name(const GclScope *scope, const char *name,
                     char *out_type, size_t out_cap);

/* Bir tipin üyelerini (native struct / kullanıcı tipi) out'a yaz. */
int gcl_type_members_of(const GclScope *scope, const char *type_name,
                        const char *file, GclCompletionResult *out);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_TYPE_H */
