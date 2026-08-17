/*
 * gcdl_loader.h — gcDL loader (API)
 *
 * GCDL = gcl Dynamic Library: gcl'nin kendi dinamik kutuphane modulu.
 * Bir dosya, icinde islemci kodu. Arsiv yok, manifest yok, derleme yok.
 *
 * Loader, modulu calismada yukler ve fonksiyon adresi verir.
 * Platform detayi (dll/so) iceride kalir.
 */

#ifndef GCDL_LOADER_H
#define GCDL_LOADER_H

#include <stddef.h>

typedef struct GclGcDl GclGcDl;   /* opaque module handle */

/* Dosyayi yukler. Hata metni err/err_cap icine yazilir. NULL = hata. */
GclGcDl *gcdl_load(const char *path, char *err, size_t err_cap);

/* gcl.exe'nin YANINDAKI bir dosyayi yukler (rel_path, orn. "Library/gcl_lua.gcDL").
 * Calisma dizininden bagimsizdir: exe neredeyse, Library orada aranir. */
GclGcDl *gcdl_load_adjacent(const char *rel_path, char *err, size_t err_cap);

/* Yuklu modulden isimli sembolun adresini verir (NULL: yok). */
void *gcdl_get_proc(GclGcDl *mod, const char *name);

/* Modulu bosaltir. */
void gcdl_unload(GclGcDl *mod);

#endif /* GCDL_LOADER_H */
