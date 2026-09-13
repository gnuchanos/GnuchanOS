/*
 * complete_index.h — Artımlı dosya indeksi / cache (§12).
 *
 * Proje dosyaları her tuş vuruşunda yeniden okunmaz: her dosya mtime'ı ile
 * birlikte saklanır. Dosya değişmediyse diskten DEĞİL cache'ten döner.
 */
#ifndef GCL_COMPLETE_INDEX_H
#define GCL_COMPLETE_INDEX_H

#include "gcl_complete.h"
#include "complete_scope.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dosya içeriğini cache'ten (veya gerekirse diskten) ver.
 * Başarıda 1 döner; *out_text motora AİTTİR (free etme), bir sonraki
 * gcl_index_clear()'a kadar geçerlidir.
 */
int  gcl_index_read(const char *path, const char **out_text,
                    size_t *out_len, long *out_mtime);

/* Dosya değişmişse cache'i geçersiz kıl (mtime damgasıyla). */
void gcl_index_invalidate(const char *path);

/* Tüm cache'i boşalt (yeni proje açılışında çağrılır). */
void gcl_index_clear(void);

/*
 * Aktif buffer için kurulmuş KAPSAMI önbellekten al (§12, S7).
 *
 * Motor her tuş vuruşunda tüm buffer'ı yeniden ayrıştırıyordu (43 KB'da
 * ~2.5 ms). Bu önbellek, TARAMA SONUCUNU buffer içerik hash'i ile saklar:
 * metin değişmediyse (imleç hareketi, popup yeniden gösterimi, debounce
 * sonrası yeniden sorgu) ayrıştırma TEKRARLANMAZ.
 *
 * Dönen kapsam motora AİTTİR ve YENİDEN KULLANILIR — free edilmemelidir.
 * Çağıran onu salt-okunur kullanabilir; proje sembollerini eklemek gibi
 * eklemeler idempotenttir (aynı ad varsa güncellenir). Çağıran, dönen
 * işaretçiyi bir SONRAKİ gcl_index_scope() çağrısından sonra kullanmamalıdır.
 * Metin hash'i veya dosya/workspace değişirse kapsam yeniden kurulur.
 */
GclScope *gcl_index_scope(const char *file, const char *text, size_t len,
                          const char *workspace);

/* Kapsam önbelleğini boşalt (gcl_index_clear tarafından da çağrılır). */
void gcl_index_scope_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_INDEX_H */
