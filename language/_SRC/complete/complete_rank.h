/*
 * complete_rank.h — Sıralama & filtreleme (§10).
 *
 * Önek/fuzzy eşleştirme + puanlama (tam eşleşme, önek, CamelCase akronim,
 * kapsam yakınlığı, tür önceliği, dosya kökeni) ve `best_index` seçimi.
 */
#ifndef GCL_COMPLETE_RANK_H
#define GCL_COMPLETE_RANK_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Önek bir etiketle eşleşiyor mu? (case-insensitive önek VEYA fuzzy) */
int  gcl_rank_match(const char *label, const char *prefix);

/* Bağlama göre tür önceliği puanı (§10.2 "Tür önceliği"). */
int  gcl_rank_kind_bonus(GclItemKind kind, GclContextKind ctx);

/*
 * Sonuç listesini puanla, filtrele (önek yoksa hepsi kalır), azalan puana
 * göre sırala ve `best_index`'i ata. `prefix` boş olabilir.
 */
void gcl_rank_apply(GclCompletionResult *r, const char *prefix,
                    GclContextKind ctx, const char *active_file);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_RANK_H */
