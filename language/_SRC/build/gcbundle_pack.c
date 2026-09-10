/*
 * gcbundle_pack.c — turn a directory into a .gcBundle binary package.
 *
 * Output format:
 *   [ GcbHeader ] [ GcbEntryHeader[] ] [ blob ]
 *
 * Directory contents (Library/, scripts/, assets/, etc.) are embedded into a
 * single package.
 *
 * Portability:
 *   Linux and Windows must have the same BEHAVIOR — unreadable files,
 *   unlistable directories and overly long paths are SKIPPED on BOTH
 *   platforms (the package is not corrupted). Only out-of-memory is a real
 *   error. Paths inside the bundle are ALWAYS stored with '/' (even on
 *   Windows) — so the package stays readable across platforms.
 */

#include "gcbundle.h"
#include "gcbundle_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

/* ---------- Entry collection ---------- */

typedef struct {
    GcbEntryHeader header;
    unsigned char *data;   /* file content (in memory) */
} Entry;

static Entry *g_entries;
static size_t g_entry_count;
static size_t g_entry_cap;

static int entries_grow(void) {
    if (g_entry_count < g_entry_cap) return 0;
    size_t new_cap = g_entry_cap ? g_entry_cap * 2 : 64;
    Entry *ne = (Entry *)realloc(g_entries, new_cap * sizeof(Entry));
    if (!ne) return -1;
    g_entries = ne;
    g_entry_cap = new_cap;
    return 0;
}

static void entries_clear(void) {
    if (!g_entries) return;
    for (size_t i = 0; i < g_entry_count; i++) {
        if (g_entries[i].data) free(g_entries[i].data);
    }
    free(g_entries);
    g_entries = NULL;
    g_entry_count = 0;
    g_entry_cap = 0;
}

/* Does this directory exist? (for top-level validation) */
static int path_is_dir(const char *p) {
#ifdef _WIN32
    struct _stat st;
    return (_stat(p, &st) == 0 && (st.st_mode & _S_IFDIR));
#else
    struct stat st;
    return (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

/* ---------- Bundle size reduction filters ---------- */

/* Does the file name end with a given suffix?
   PK-4: The comparison is ASCII case-insensitive on BOTH platforms. This way
   Linux and Windows have the SAME behavior; uppercase extensions such as
   ".LIB" / ".H" coming from Windows/MSYS compilers are skipped on Linux too
   (otherwise the package would come out with a different size/content across
   platforms). */
static int has_suffix(const char *p, const char *suf) {
    size_t plen = strlen(p), slen = strlen(suf);
    if (plen < slen) return 0;
    const char *a = p + plen - slen;
    for (size_t i = 0; i < slen; i++) {
        char ca = a[i], cb = suf[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return 0;
    }
    return 1;
}

/* p, pre ile başlıyor mu? */
static int has_prefix(const char *p, const char *pre) {
    return strncmp(p, pre, strlen(pre)) == 0;
}

/*
 * Python stdlib'inden atlanacak paket adları. Windows embed düzeni
 *   Library/Embeded/Python_Runtime/Python/Lib/<pkg>/...
 * Linux (python-build-standalone) düzeni ise
 *   Library/Embeded/Python_Runtime/Python/lib/python3.X/<pkg>/...
 * şeklindedir. Önek (Lib/ vs lib/python3.X/) platformlar arası FARKLI olduğu
 * için, gerçek paket adı bulunup ona göre karar verilir. Bu sayede Linux
 * build'inde de test/tkinter/... gömülmez (aksi halde bundle ~1 GB şişer).
 */
static const char *const GCB_JUNK_PACKAGES[] = {
    "test", "tests", "tkinter", "turtle", "idlelib", "ensurepip",
    "curses", "dbm", "lib2to3", "venv", "wsgiref", "site-packages",
    "turtledemo", "distutils", "setuptools", "pip", "unittest2",
    NULL
};

static int is_junk_package(const char *name, size_t len) {
    for (int i = 0; GCB_JUNK_PACKAGES[i]; i++) {
        if (strlen(GCB_JUNK_PACKAGES[i]) == len &&
            strncmp(name, GCB_JUNK_PACKAGES[i], len) == 0)
            return 1;
    }
    return 0;
}

static int should_skip_python_stdlib(const char *rel) {
    static const char *root = "Library/Embeded/Python_Runtime/Python/";
    size_t rl = strlen(root);
    if (strncmp(rel, root, rl) != 0) return 0;
    const char *tail = rel + rl;

    /* Windows: "Lib/..." ; Linux: "lib/python3.X/..." önekini atla */
    if (has_prefix(tail, "Lib/")) {
        tail += 4;
    } else if (has_prefix(tail, "lib/python3.")) {
        tail += 12;                                /* "python3." bitti */
        while (*tail && *tail != '/') tail++;      /* sürüm rakamlarını atla */
        if (*tail == '/') tail++;
    } else {
        return 0;
    }

    /* tail içindeki HERHANGİ bir / ile ayrılmış bileşen junk paket adı mı?
       (örn. ctypes/test, email/test, xml/test gibi iç içe test dizinleri) */
    const char *p = tail;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t clen = slash ? (size_t)(slash - p) : strlen(p);
        if (is_junk_package(p, clen)) return 1;
        if (!slash) break;
        p = slash + 1;
    }
    return 0;
}

/* Pakete GÖMÜLMEYECEK yollar.
   1.26 GB'lık bundle'ın nedeni: runtime dizini olduğu gibi gömülüyordu
   (Python stdlib full paketi + libraylib.a + .lib/.h/.pyc + test/tkinter...).
   Bunlar pakete girmez — bundle ~176 MB civarına iner. */
static int should_skip(const char *rel, int is_dir) {
    if (!rel || !rel[0]) return 0;

    /* __pycache__ ve derlenmiş/ara dosyalar */
    if (strstr(rel, "__pycache__")) return 1;
    if (!is_dir) {
        if (has_suffix(rel, ".pyc") || has_suffix(rel, ".pyo") ||
            has_suffix(rel, ".a")   || has_suffix(rel, ".lib") ||
            has_suffix(rel, ".o")   || has_suffix(rel, ".def") ||
            has_suffix(rel, ".pdb") || has_suffix(rel, ".exp") ||
            has_suffix(rel, ".h"))
            return 1;
    }

    /* Windows embed yan ürünleri (Scripts/, libs/) — çalışma zamanında gerekmez */
    if (has_prefix(rel, "Library/Embeded/Python_Runtime/Python/Scripts")) return 1;
    if (has_prefix(rel, "Library/Embeded/Python_Runtime/Python/libs")) return 1;

    /* Python stdlib gereksiz paketleri (Windows Lib/ + Linux lib/python3.X/) */
    if (should_skip_python_stdlib(rel)) return 1;

    return 0;
}

/* ---------- Entry ekleme ---------- */

/* Entry tablosunda yer var mı? */
static int entry_slot(Entry **out) {
    if (entries_grow() != 0) return -1;
    Entry *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    *out = e;
    return 0;
}

/* Bir dosyayı okuyup FILE entry olarak ekle. Atlandı: 0, bellek hatası: -1. */
static int collect_file(const char *full, const char *rel) {
    if (!rel || !rel[0]) return 0;
    if (strlen(rel) >= GCB_PATH_MAX) {
        fprintf(stderr, "[gcbundle] atlandı (yol çok uzun): %s\n", rel);
        return 0;
    }
    if (should_skip(rel, 0)) return 0;

    FILE *f = fopen(full, "rb");
    if (!f) return 0;                      /* okunamayan dosyayı ATLA (tüm platformlar) */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    Entry *e = NULL;
    if (entry_slot(&e) != 0) { free(buf); return -1; }
    snprintf(e->header.path, sizeof(e->header.path), "%s", rel);
    e->header.type = GCB_ENTRY_FILE;
    e->header.size = (uint32_t)rd;
    e->data = buf;
    return 0;
}

/* Dizin entry'si ekle. Atlandı: 0, bellek hatası: -1. */
static int add_dir_entry(const char *rel) {
    if (!rel || !rel[0]) return 0;
    if (strlen(rel) >= GCB_PATH_MAX) {
        fprintf(stderr, "[gcbundle] atlandı (yol çok uzun): %s\n", rel);
        return 0;
    }
    Entry *e = NULL;
    if (entry_slot(&e) != 0) return -1;
    snprintf(e->header.path, sizeof(e->header.path), "%s", rel);
    e->header.type = GCB_ENTRY_DIR;
    return 0;
}

/* ---------- Dizin yürüyüşü ---------- */

/*
 * Yürüyüş tampon sınırları.
 *   GCB_REL_MAX  : bundle içi göreli yol üst sınırı. GCB_PATH_MAX ile AYNI —
 *                  bundler zaten bu boyuttan uzun yolu saklamaz, dolayısıyla
 *                  bu sınırın ötesine inmek anlamsızdır (PK-2).
 *   GCB_FULL_MAX : base (mutlak yol, en fazla ~4096) + '/' + rel + NUL.
 *                  Taşma ÖNCEDEN kontrol edilir; snprintf'in sessizce kesmesi
 *                  engellenir (PK-1).
 *   GCB_MAX_DEPTH: self-referencing symlink (Linux) veya junction/reparse
 *                  point (Windows) döngülerinde sonsuz özyinelemeyi keser (PK-3).
 */
#define GCB_REL_MAX   GCB_PATH_MAX
#define GCB_FULL_MAX  (4096 + GCB_PATH_MAX + 8)
#define GCB_MAX_DEPTH 64

/* Etkin dizin özyineleme derinliği (GCB_MAX_DEPTH koruması için). */
static int g_dir_depth = 0;

static int collect_dir(const char *base, const char *rel) {
    int top_level = (!rel || rel[0] == '\0');
    const char *r = (rel && rel[0]) ? rel : "";

    /* PK-6: base zaten ayraçla bitiyorsa ikinci ayraç eklenmez — çift ayraç
       (özellikle UNC / kök yollarda) yanlış çözümlenebilir. */
    size_t blen = strlen(base);
    int base_has_sep = (blen > 0 && (base[blen - 1] == '/' || base[blen - 1] == '\\'));

    /* full = base + '/' + rel. Taşmayı ÖNCEDEN kontrol et: snprintf sessizce
       keserse yanlış bir yol stat'lanır ve dosya/dizin SESSİZCE atlanır. */
    size_t need = blen + (base_has_sep ? 0 : 1) + strlen(r) + 1;
    if (need > GCB_FULL_MAX) {
        fprintf(stderr, "[gcbundle] atlandı (yol çok uzun): %s\n", r);
        return 0;
    }

    /* rel içindeki yollar HER ZAMAN '/' ile; Windows'ta stat/FindFirstFile için
       yerel ayraca çevrilir. Bundle'a yazılan yol ise '/' kalır. */
    char full[GCB_FULL_MAX];
    if (base_has_sep || !r[0])
        snprintf(full, sizeof(full), "%s%s", base, r);
    else
        snprintf(full, sizeof(full), "%s/%s", base, r);
#ifdef _WIN32
    for (char *p = full; *p; p++) if (*p == '/') *p = '\\';
    {
        size_t flen = strlen(full);
        /* Sondaki ayraçları kırp ama sürücü kökünü ("C:\") BOZMA. */
        while (flen > 1 && full[flen - 1] == '\\' && full[flen - 2] != ':') {
            full[flen - 1] = '\0'; flen--;
        }
    }
    struct _stat st;
    if (_stat(full, &st) != 0) return top_level ? -1 : 0;
    if (st.st_mode & _S_IFREG) return collect_file(full, rel);
    if (!(st.st_mode & _S_IFDIR)) return 0;
#else
    struct stat st;
    if (stat(full, &st) != 0) return top_level ? -1 : 0;
    if (S_ISREG(st.st_mode)) return collect_file(full, rel);
    if (!S_ISDIR(st.st_mode)) return 0;
#endif

    /* Bu bir dizin */
    if (should_skip(rel, 1)) return 0;
    if (rel && rel[0]) {
        int rc = add_dir_entry(rel);
        if (rc != 0) return -1;
    }

    /* Sonsuz özyineleme koruması (symlink/junction döngüsü). */
    if (g_dir_depth >= GCB_MAX_DEPTH) {
        fprintf(stderr, "[gcbundle] atlandı (derinlik limiti %d): %s\n",
                GCB_MAX_DEPTH, r);
        return 0;
    }
    g_dir_depth++;

#ifdef _WIN32
    char pattern[GCB_FULL_MAX + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", full);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE) { g_dir_depth--; return 0; }  /* listelenemeyen dizini atla */
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        char sub_rel[GCB_REL_MAX];
        int n = snprintf(sub_rel, sizeof(sub_rel), "%s%s%s",
                         r, r[0] ? "/" : "", ffd.cFileName);
        if (n < 0 || (size_t)n >= sizeof(sub_rel)) {
            fprintf(stderr, "[gcbundle] atlandı (rel çok uzun): %s\n", ffd.cFileName);
            continue;
        }
        if (collect_dir(base, sub_rel) != 0) { FindClose(h); g_dir_depth--; return -1; }
    } while (FindNextFileA(h, &ffd));
    FindClose(h);
#else
    DIR *d = opendir(full);
    if (!d) { g_dir_depth--; return 0; }         /* listelenemeyen dizini atla */
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char sub_rel[GCB_REL_MAX];
        int n = snprintf(sub_rel, sizeof(sub_rel), "%s%s%s",
                         r, r[0] ? "/" : "", ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(sub_rel)) {
            fprintf(stderr, "[gcbundle] atlandı (rel çok uzun): %s\n", ent->d_name);
            continue;
        }
        if (collect_dir(base, sub_rel) != 0) { closedir(d); g_dir_depth--; return -1; }
    }
    closedir(d);
#endif
    g_dir_depth--;
    return 0;
}

/* ---------- Yazma ---------- */

static int write_all(FILE *f, const void *data, size_t size) {
    if (!size) return 0;
    return fwrite(data, 1, size, f) == size ? 0 : -1;
}

/* Toplanan g_entries'leri paket olarak yazar (header + entry tablosu + blob). */
static int write_bundle(const char *name, const char *out_path) {
    FILE *f = fopen(out_path, "wb");
    if (!f) { entries_clear(); gcb_set_error("pack: cannot write output"); return -1; }

    /* 4 GB sınırı (PK-5): format offset/boyut alanları uint32 olduğundan
       header + entry tablosu + blob toplamı UINT32_MAX'i aşamaz. Aşarsa
       sessizce uint32 taşması (veri bozulması) yerine NET hata ver. */
    {
        size_t table_sz = (size_t)g_entry_count * sizeof(GcbEntryHeader);
        size_t blob_sz = 0;
        for (size_t i = 0; i < g_entry_count; i++) {
            if (g_entries[i].header.type == GCB_ENTRY_FILE)
                blob_sz += (size_t)g_entries[i].header.size;
        }
        size_t total = sizeof(GcbHeader) + table_sz + blob_sz;
        if (total > (size_t)UINT32_MAX) {
            fclose(f);
            entries_clear();
            gcb_set_error("pack: bundle exceeds 4GB (uint32 offset) limit");
            return -1;
        }
    }

    GcbHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, GCB_MAGIC, GCB_MAGIC_BYTES);
    header.version = GCB_VERSION;
    header.flags = 0;
    snprintf(header.name, sizeof(header.name), "%s", name ? name : "project");
    header.entry_count = (uint32_t)g_entry_count;
    header.entry_offset = (uint32_t)sizeof(GcbHeader);
    header.blob_offset = (uint32_t)(sizeof(GcbHeader) + (size_t)g_entry_count * sizeof(GcbEntryHeader));

    if (write_all(f, &header, sizeof(header)) != 0) goto fail;

    uint32_t blob_off = header.blob_offset;
    for (size_t i = 0; i < g_entry_count; i++) {
        GcbEntryHeader eh;
        memset(&eh, 0, sizeof(eh));
        snprintf(eh.path, sizeof(eh.path), "%s", g_entries[i].header.path);
        eh.type = g_entries[i].header.type;
        if (eh.type == GCB_ENTRY_FILE) {
            eh.offset = blob_off;
            eh.size = g_entries[i].data ? g_entries[i].header.size : 0;
        }
        blob_off += eh.size;
        if (write_all(f, &eh, sizeof(eh)) != 0) goto fail;
    }

    for (size_t i = 0; i < g_entry_count; i++) {
        if (g_entries[i].header.type == GCB_ENTRY_FILE && g_entries[i].data) {
            if (write_all(f, g_entries[i].data, g_entries[i].header.size) != 0) goto fail;
        }
    }

    fclose(f);
    entries_clear();
    return 0;

fail:
    fclose(f);
    entries_clear();
    gcb_set_error("pack: write failed");
    return -1;
}

int gcb_pack_dir(const char *dir, const char *name, const char *out_path) {
    if (!dir || !out_path) { gcb_set_error("pack: null path"); return -1; }
    if (!path_is_dir(dir)) { gcb_set_error("pack: directory not found"); return -1; }

    entries_clear();
    if (collect_dir(dir, "") != 0) {
        entries_clear();
        gcb_set_error("pack: could not traverse directory");
        return -1;
    }
    return write_bundle(name, out_path);
}

int gcb_pack_project_runtime(const char *project_dir, const char *runtime_dir,
                             const char *name, const char *out_path) {
    if (!project_dir || !out_path) { gcb_set_error("pack: null path"); return -1; }
    if (!path_is_dir(project_dir)) { gcb_set_error("pack: project directory not found"); return -1; }

    entries_clear();
    /* Proje dosyaları (scripts/, assets/, main.gcsf, ...) */
    if (collect_dir(project_dir, "") != 0) {
        entries_clear();
        gcb_set_error("pack: could not traverse project directory");
        return -1;
    }
    /* Runtime (Library/ altındaki tüm DLL/SO) — "Library" prefix'iyle göm.
       runtime_dir altında Library/ YOKSA paket eksik olur → net hata ver. */
    if (runtime_dir && runtime_dir[0]) {
        char lib_dir[4096];
        snprintf(lib_dir, sizeof(lib_dir), "%s/Library", runtime_dir);
#ifdef _WIN32
        for (char *p = lib_dir; *p; p++) if (*p == '/') *p = '\\';
#endif
        if (!path_is_dir(lib_dir)) {
            entries_clear();
            gcb_set_error("pack: runtime 'Library' directory not found");
            return -1;
        }
        if (collect_dir(runtime_dir, "Library") != 0) {
            entries_clear();
            gcb_set_error("pack: could not traverse runtime directory");
            return -1;
        }
    }
    return write_bundle(name, out_path);
}
