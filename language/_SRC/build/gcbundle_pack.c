/*
 * gcbundle_pack.c — bir dizini .gcBundle binary paketine dönüştür.
 *
 * Çıktı format:
 *   [ GcbHeader ] [ GcbEntryHeader[] ] [ blob ]
 *
 * Dizin içeriği (Library/, scripts/, assets/ vb.) tek pakete gömülür.
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
#define stat _stat
#ifndef S_ISDIR
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(x) ((x) & _S_IFREG)
#endif
#else
#include <dirent.h>
#include <unistd.h>
#endif

/* ---------- Entry koleksiyonu ---------- */

typedef struct {
    GcbEntryHeader header;
    unsigned char *data;   /* file içerik (memory) */
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

/* ---------- Bundle boyutunu küçültme filtreleri ---------- */

/* Dosya adı belirli bir sonekle bitiyor mu? (case-sensitive — uzantılar zaten küçük) */
static int has_suffix(const char *p, const char *suf) {
    size_t plen = strlen(p), slen = strlen(suf);
    if (plen < slen) return 0;
    return strcmp(p + plen - slen, suf) == 0;
}

/* Pakete GÖMÜLMEYECEK yollar.
   1.26 GB'lık bundle'ın nedeni: runtime dizini olduğu gibi gömülüyordu
   (Python stdlib full paketi + libraylib.a + .lib/.h/.pyc + test/tkinter...).
   Bunlar pakete girmez — bundle ~176 MB civarına iner. */
static int should_skip(const char *rel, int is_dir) {
    if (!rel || !rel[0]) return 0;
    /* __pycache__ ve *.pyc */
    if (strstr(rel, "__pycache__")) return 1;
    if (!is_dir) {
        if (has_suffix(rel, ".pyc") || has_suffix(rel, ".pyo") ||
            has_suffix(rel, ".a") || has_suffix(rel, ".lib") ||
            has_suffix(rel, ".o") || has_suffix(rel, ".def") ||
            has_suffix(rel, ".pdb") || has_suffix(rel, ".exp") ||
            has_suffix(rel, ".h"))
            return 1;
    }
    /* Python runtime stdlib'ünden gereksiz/optiksel paketler —
       çekirdek stdlib (os, sys, encodings, etc.) KALIR. */
    const char *skip_dirs[] = {
        "Library/Embeded/Python_Runtime/Python/Lib/test",
        "Library/Embeded/Python_Runtime/Python/Lib/tkinter",
        "Library/Embeded/Python_Runtime/Python/Lib/turtle",
        "Library/Embeded/Python_Runtime/Python/Lib/ensurepip",
        "Library/Embeded/Python_Runtime/Python/Lib/idlelib",
        "Library/Embeded/Python_Runtime/Python/Lib/ctypes/test",
        "Library/Embeded/Python_Runtime/Python/Lib/curses",
        "Library/Embeded/Python_Runtime/Python/Lib/dbm",
        "Library/Embeded/Python_Runtime/Python/Lib/lib2to3",
        "Library/Embeded/Python_Runtime/Python/Lib/venv",
        "Library/Embeded/Python_Runtime/Python/Lib/wsgiref",
        "Library/Embeded/Python_Runtime/Python/Lib/email/test",
        "Library/Embeded/Python_Runtime/Python/Lib/xml/test",
        "Library/Embeded/Python_Runtime/Python/Lib/unittest/test",
        "Library/Embeded/Python_Runtime/Python/Lib/site-packages",
        "Library/Embeded/Python_Runtime/Python/Scripts",
        "Library/Embeded/Python_Runtime/Python/libs",
        NULL
    };
    for (int i = 0; skip_dirs[i]; i++) {
        size_t sl = strlen(skip_dirs[i]);
        if (strncmp(rel, skip_dirs[i], sl) == 0) return 1;
    }
    return 0;
}

/* ---------- Dizin yürüyüşü ---------- */

static int collect_dir(const char *base, const char *rel) {
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", base, rel && rel[0] ? rel : "");

#ifdef _WIN32
    /* Windows: / yerine \ kullan (stat/FindFirstFile uyumluluğu).
       Linux'ta bu dönüşüm yapılmaz — aksi halde opendir()'a geçersiz yol verir. */
    for (char *p = full; *p; p++) if (*p == '/') *p = '\\';
    /* KRİTİK: kök dizinde "D:\dir\" (sonda \) üretir — _stat bu yolla FAIL eder.
       Sondaki \ karakterini kırp; böylece "D:\dir" geçerli bir dizin yoludur. */
    {
        size_t flen = strlen(full);
        while (flen > 1 && full[flen - 1] == '\\') { full[flen - 1] = '\0'; flen--; }
    }
    struct stat st;
    if (stat(full, &st) != 0) return -1;
    int is_dir = S_ISDIR(st.st_mode);
    int is_file = S_ISREG(st.st_mode);

    if (is_file) {
        if (should_skip(rel, 0)) return 0;
        FILE *f = fopen(full, "rb");
        if (!f) return -1;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
        long sz = ftell(f);
        rewind(f);
        unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
        if (!buf) { fclose(f); return -1; }
        size_t rd = fread(buf, 1, (size_t)sz, f);
        fclose(f);

        if (entries_grow() != 0) { free(buf); return -1; }
        Entry *e = &g_entries[g_entry_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->header.path, sizeof(e->header.path), "%s", rel);
        e->header.type = GCB_ENTRY_FILE;
        e->header.size = (uint32_t)rd;
        e->data = buf;
    } else if (is_dir) {
        if (should_skip(rel, 1)) return 0;
        if (entries_grow() != 0) return -1;
        Entry *e = &g_entries[g_entry_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->header.path, sizeof(e->header.path), "%s", rel);
        e->header.type = GCB_ENTRY_DIR;

        char pattern[8192];
        snprintf(pattern, sizeof(pattern), "%s\\*", full);
        WIN32_FIND_DATAA ffd;
        HANDLE h = FindFirstFileA(pattern, &ffd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
                char sub_rel[4200];
                snprintf(sub_rel, sizeof(sub_rel), "%s%s%s",
                         rel && rel[0] ? rel : "", rel && rel[0] ? "/" : "", ffd.cFileName);
                collect_dir(base, sub_rel);
            } while (FindNextFileA(h, &ffd));
            FindClose(h);
        }
    }
#else
    DIR *d = opendir(full);
    if (!d) return -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char sub_rel[4200];
        snprintf(sub_rel, sizeof(sub_rel), "%s%s%s",
                 rel && rel[0] ? rel : "", rel && rel[0] ? "/" : "", ent->d_name);
        char full_sub[4096];
        snprintf(full_sub, sizeof(full_sub), "%s/%s", base, sub_rel);

        struct stat st;
        if (stat(full_sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (should_skip(sub_rel, 1)) continue;
            if (entries_grow() != 0) { closedir(d); return -1; }
            Entry *e = &g_entries[g_entry_count++];
            memset(e, 0, sizeof(*e));
            snprintf(e->header.path, sizeof(e->header.path), "%s", sub_rel);
            e->header.type = GCB_ENTRY_DIR;
            if (collect_dir(base, sub_rel) != 0) { closedir(d); return -1; }
        } else if (S_ISREG(st.st_mode)) {
            if (should_skip(sub_rel, 0)) continue;
            FILE *f = fopen(full_sub, "rb");
            if (!f) continue;
            if (fseek(f, 0, SEEK_END) != 0) { fclose(f); continue; }
            long sz = ftell(f);
            rewind(f);
            unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
            if (!buf) { fclose(f); continue; }
            size_t rd = fread(buf, 1, (size_t)sz, f);
            fclose(f);

            if (entries_grow() != 0) { free(buf); closedir(d); return -1; }
            Entry *e = &g_entries[g_entry_count++];
            memset(e, 0, sizeof(*e));
            snprintf(e->header.path, sizeof(e->header.path), "%s", sub_rel);
            e->header.type = GCB_ENTRY_FILE;
            e->header.size = (uint32_t)rd;
            e->data = buf;
        }
    }
    closedir(d);
#endif
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

    entries_clear();
    /* Proje dosyaları (scripts/, assets/, main.gcsf, ...) */
    if (collect_dir(project_dir, "") != 0) {
        entries_clear();
        gcb_set_error("pack: could not traverse project directory");
        return -1;
    }
    /* Runtime (Library/ altındaki tüm DLL/SO) — "Library" prefix'iyle göm */
    if (runtime_dir && runtime_dir[0]) {
        if (collect_dir(runtime_dir, "Library") != 0) {
            entries_clear();
            gcb_set_error("pack: could not traverse runtime directory");
            return -1;
        }
    }
    return write_bundle(name, out_path);
}
