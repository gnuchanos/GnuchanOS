/*
 * gcbundle_reader.c — .gcBundle okuma (AppImage modeli).
 *
 * .gcBundle dosyası belleğe okunur; entry tablosu üzerinden dosyalara
 * pointer'la erişilir. HİÇBİR ŞEY DISKE YAZILMAZ.
 */

#include "gcbundle.h"
#include "gcbundle_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>  /* _mkdir */
#else
#include <sys/stat.h> /* mkdir */
#include <sys/types.h>
#endif

struct GcbBundle {
    unsigned char *data;
    size_t         size;
    GcbHeader      header;
    GcbEntryHeader *entries;
    size_t         entry_count;
    char           name[GCB_NAME_MAX];
    int            valid;
};

static char g_last_error[1024] = "";

void gcb_set_error(const char *msg) {
    if (!msg) return;
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

const char *gcb_last_error(void) { return g_last_error; }

const void *gcb_raw_data(const GcbBundle *b) { return b ? b->data : NULL; }
size_t gcb_raw_size(const GcbBundle *b) { return b ? b->size : 0; }

static int validate(GcbBundle *b) {
    if (!b->data || b->size < sizeof(GcbHeader)) {
        gcb_set_error("GCB: insufficient data for header");
        return -1;
    }
    GcbHeader *h = (GcbHeader *)b->data;

    if (memcmp(h->magic, GCB_MAGIC, GCB_MAGIC_BYTES) != 0) {
        gcb_set_error("GCB: bad magic");
        return -1;
    }
    if (h->version != GCB_VERSION) {
        gcb_set_error("GCB: unsupported version");
        return -1;
    }

    b->header = *h;
    snprintf(b->name, sizeof(b->name), "%s", h->name);
    b->name[GCB_NAME_MAX - 1] = '\0';

    if (h->entry_count > 65536) {
        gcb_set_error("GCB: too many entries");
        return -1;
    }
    /* uint32 taşmasını önle: aritmetiği size_t (64-bit) üzerinden yap */
    size_t table_bytes = (size_t)h->entry_count * sizeof(GcbEntryHeader);
    if ((size_t)h->entry_offset + table_bytes > b->size) {
        gcb_set_error("GCB: entry table out of bounds");
        return -1;
    }

    b->entry_count = h->entry_count;
    b->entries = (GcbEntryHeader *)(b->data + h->entry_offset);
    b->valid = 1;
    return 0;
}

GcbBundle *gcb_open(const char *path) {
    if (!path) { gcb_set_error("GCB: path is NULL"); return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) { gcb_set_error("GCB: cannot open"); return NULL; }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); gcb_set_error("GCB: seek failed"); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); gcb_set_error("GCB: tell failed"); return NULL; }
    rewind(f);

    GcbBundle *b = (GcbBundle *)calloc(1, sizeof(GcbBundle));
    if (!b) { fclose(f); gcb_set_error("GCB: out of memory"); return NULL; }

    b->data = (unsigned char *)malloc((size_t)sz + 1);
    if (!b->data) { free(b); fclose(f); gcb_set_error("GCB: out of memory"); return NULL; }
    size_t rd = fread(b->data, 1, (size_t)sz, f);
    fclose(f);
    b->size = rd;

    if (validate(b) != 0) {
        free(b->data);
        free(b);
        return NULL;
    }
    return b;
}

GcbBundle *gcb_open_memory(const void *data, size_t size) {
    if (!data || size == 0) { gcb_set_error("GCB: null memory"); return NULL; }
    GcbBundle *b = (GcbBundle *)calloc(1, sizeof(GcbBundle));
    if (!b) { gcb_set_error("GCB: out of memory"); return NULL; }

    b->data = (unsigned char *)malloc(size);
    if (!b->data) { free(b); gcb_set_error("GCB: out of memory"); return NULL; }
    memcpy(b->data, data, size);
    b->size = size;

    if (validate(b) != 0) {
        free(b->data);
        free(b);
        return NULL;
    }
    return b;
}

void gcb_close(GcbBundle *b) {
    if (!b) return;
    if (b->data) free(b->data);
    free(b);
}

const char *gcb_name(const GcbBundle *b) { return b && b->valid ? b->name : NULL; }
uint32_t gcb_entry_count(const GcbBundle *b) { return b && b->valid ? (uint32_t)b->entry_count : 0; }

const char *gcb_entry_path(const GcbBundle *b, uint32_t i) {
    if (!b || !b->valid || i >= b->entry_count) return NULL;
    return b->entries[i].path;
}

uint32_t gcb_entry_type(const GcbBundle *b, uint32_t i) {
    if (!b || !b->valid || i >= b->entry_count) return 0;
    return b->entries[i].type;
}

uint32_t gcb_entry_size(const GcbBundle *b, uint32_t i) {
    if (!b || !b->valid || i >= b->entry_count) return 0;
    return b->entries[i].size;
}

static const GcbEntryHeader *find_entry(const GcbBundle *b, const char *path) {
    if (!b || !b->valid || !path) return NULL;
    for (size_t i = 0; i < b->entry_count; i++) {
        if (strcmp(b->entries[i].path, path) == 0 && b->entries[i].type == GCB_ENTRY_FILE) {
            return &b->entries[i];
        }
    }
    return NULL;
}

const void *gcb_read_path(const GcbBundle *b, const char *path, uint32_t *size) {
    if (size) *size = 0;
    const GcbEntryHeader *e = find_entry(b, path);
    if (!e) { gcb_set_error("GCB: path not found"); return NULL; }
    /* uint32 taşmasını önle: aritmetiği size_t (64-bit) üzerinden yap */
    if ((size_t)e->offset + (size_t)e->size > b->size) { gcb_set_error("GCB: entry out of bounds"); return NULL; }
    if (size) *size = e->size;
    return b->data + e->offset;
}

/* Alt dizinleriyle birlikte dizin oluştur (mkdir -p).
   Windows'ta "C:\path\to\dir" gibi drive letter'lı mutlak yollar ve UNC
   (\\server\share\...) doğru işlenir — "C:" gibi geçersiz parçalar denenmez. */
static int mkdir_p(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
#ifdef _WIN32
    for (char *p = tmp; *p; p++) if (*p == '/') *p = '\\';
#endif
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    /* Sondaki ayracı kırp */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = '\0';

    /* Kök dizin konumunu bul — iterasyon buradan başlar */
    size_t start = 1;
#ifdef _WIN32
    /* "C:\..." → drive letter + ayraç atla */
    if (len > 2 && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/')) {
        start = 3;
    }
    /* UNC: "\\server\share\..." → server+share adını atla.
       \\server adında dizin OLUŞTURULMAZ (UNC root'u var olmalıdır);
       ilk mkdir \\server\share altındaki ilk alt dizinden başlar. */
    else if (len > 2 && tmp[0] == '\\' && tmp[1] == '\\') {
        char *sp = tmp + 2;
        while (*sp && *sp != '\\' && *sp != '/') sp++;  /* server adını atla */
        if (*sp) {
            sp++;
            while (*sp && *sp != '\\' && *sp != '/') sp++;  /* share adını atla */
        }
        if (*sp) sp++;  /* son ayracı da atla */
        start = (size_t)(sp - tmp);
        if (start >= len) start = 1;  /* güvenlik: hiç seperator yoksa */
    }
#else
    /* POSIX mutlak yol: "/..." → baştaki ayracı atla */
    if (tmp[0] == '/') start = 1;
#endif

    for (char *p = tmp + start; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = c;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
    return 0;
}

/* Bundle içindeki TÜM dosyaları out_dir altına yazar (recursive). */
int gcb_extract_all(const GcbBundle *b, const char *out_dir) {
    if (!b || !b->valid || !out_dir) { gcb_set_error("GCB: extract null param"); return -1; }
    mkdir_p(out_dir);
    for (uint32_t i = 0; i < b->entry_count; i++) {
        if (b->entries[i].type != GCB_ENTRY_FILE) continue;
        char full[8192];
#ifdef _WIN32
        snprintf(full, sizeof(full), "%s\\%s", out_dir, b->entries[i].path);
        for (char *p = full; *p; p++) if (*p == '/') *p = '\\';
#else
        snprintf(full, sizeof(full), "%s/%s", out_dir, b->entries[i].path);
#endif
        /* full içindeki son \'den sonrası dosya adı — parent dizini mkdir_p ile yap */
        char parent[8192];
        snprintf(parent, sizeof(parent), "%s", full);
        char *slash = strrchr(parent, '\\');
        if (!slash) slash = strrchr(parent, '/');
        if (slash) {
            *slash = '\0';
            if (parent[0]) mkdir_p(parent);
        }
        if ((size_t)b->entries[i].offset + (size_t)b->entries[i].size > b->size) {
            gcb_set_error("GCB: extract entry out of bounds");
            return -1;
        }
        FILE *f = fopen(full, "wb");
        if (!f) { gcb_set_error("GCB: cannot write extract"); return -1; }
        size_t wr = fwrite(b->data + b->entries[i].offset, 1, b->entries[i].size, f);
        fclose(f);
        if (wr != b->entries[i].size) { gcb_set_error("GCB: extract write short"); return -1; }
    }
    return 0;
}
