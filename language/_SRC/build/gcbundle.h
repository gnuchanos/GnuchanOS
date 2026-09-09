/*
 * gcbundle.h — gcBundle public API + binary format.
 *
 * gcBundle = AppImage benzeri tek paket çıktısı:
 *   gcl -build project.gcdata -o path/dir
 *       -> Project_name.exe
 *       -> Project_name.gcBundle   (tüm runtime + dll/so tek pakette)
 *
 * Çalışma zamanında gcBundle BELLEĞE okunur; dosyalara entry tablosu
 * üzerinden pointer'la erişilir. HİÇBİR ŞEY DISKE YAZILMAZ.
 */

#ifndef GCBUNDLE_H
#define GCBUNDLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Format sabitleri ---------- */

#define GCB_MAGIC       "GCBL"
#define GCB_MAGIC_BYTES 4
#define GCB_VERSION     1

#define GCB_NAME_MAX    64
#define GCB_PATH_MAX    512

/* Entry türleri */
#define GCB_ENTRY_FILE  0
#define GCB_ENTRY_DIR   1

/* ---------- Format yapıları (packed) ---------- */

typedef struct __attribute__((packed)) {
    char     magic[GCB_MAGIC_BYTES]; /* "GCBL" */
    uint16_t version;                 /* GCB_VERSION */
    uint16_t flags;                   /* 0 */
    char     name[GCB_NAME_MAX];      /* proje adı */
    uint32_t entry_count;             /* entry tablosu eleman sayısı */
    uint32_t entry_offset;            /* entry tablosunun dosya başlangıcından offset'i */
    uint32_t blob_offset;             /* blob verisinin dosya başlangıcından offset'i */
} GcbHeader;

typedef struct __attribute__((packed)) {
    char     path[GCB_PATH_MAX];
    uint32_t type;    /* GCB_ENTRY_FILE / GCB_ENTRY_DIR */
    uint32_t offset;  /* dosya başlangıcından file data offset'i */
    uint32_t size;    /* file boyutu */
} GcbEntryHeader;

/* Bellekte açılmış gcBundle (opaque). */
typedef struct GcbBundle GcbBundle;

/* ---------- Yaşam döngüsü ---------- */

/* .gcBundle dosyasını bellekte açar. Başarı: GcbBundle*, hata: NULL. */
GcbBundle *gcb_open(const char *path);

/* Bellekteki tampondan açar. */
GcbBundle *gcb_open_memory(const void *data, size_t size);

/* Kapatır, belleği temizler. */
void gcb_close(GcbBundle *b);

/* ---------- Bilgi ---------- */

const char *gcb_name(const GcbBundle *b);
uint32_t gcb_entry_count(const GcbBundle *b);
const char *gcb_entry_path(const GcbBundle *b, uint32_t i);
uint32_t gcb_entry_type(const GcbBundle *b, uint32_t i);
uint32_t gcb_entry_size(const GcbBundle *b, uint32_t i);

/* Path'e göre dosya içeriğine pointer döndürür (bellekte). */
const void *gcb_read_path(const GcbBundle *b, const char *path, uint32_t *size);

/* Bundle içindeki TÜM dosyaları (file entry'leri) out_dir altına yazar.
   Dizinler recursive oluşturulur. Build edilen exe ilk çalıştırmada bunu
   kendine yanına açar — böylece Library/, scripts/, assets/ diske çıkar ve
   native modüller + Embed.Run script'leri bulunur. Başarı: 0, hata: -1. */
int gcb_extract_all(const GcbBundle *b, const char *out_dir);

/* ---------- Paketleme ---------- */

/*
 * Bir dizini .gcBundle paketine dönüştürür ve out_path'e yazar.
 * name: paket adı (proje adı).
 * Başarı: 0, hata: -1.
 */
int gcb_pack_dir(const char *dir, const char *name, const char *out_path);

/*
 * Proje dizinini + runtime dizinini (Library/) tek pakete dönüştürür.
 * runtime_dir: language/build/<os>/ (Library/ altındaki tüm DLL/SO runtime)
 *              prefix "Library" ile pakete gömülür.
 * Başarı: 0, hata: -1.
 */
int gcb_pack_project_runtime(const char *project_dir, const char *runtime_dir,
                             const char *name, const char *out_path);

/* ---------- Hata ---------- */

const char *gcb_last_error(void);

/* Ham veriye erişim. */
const void *gcb_raw_data(const GcbBundle *b);
size_t gcb_raw_size(const GcbBundle *b);

#ifdef __cplusplus
}
#endif

#endif /* GCBUNDLE_H */
