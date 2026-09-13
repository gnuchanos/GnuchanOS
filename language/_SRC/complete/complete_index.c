/*
 * complete_index.c — Artımlı dosya cache'i (§12).
 *
 * Her dosya yolu → {mtime, içerik} eşlemesi tutulur. mtime değişmediyse
 * içerik diskten YENİDEN okunmaz. Küçük kapasiteli, FIFO tahliyeli basit
 * bir cache; IDE tek iş parçacığı olduğu için kilide gerek yoktur.
 */
#include "complete_index.h"
#include <stdio.h>
#include <sys/stat.h>

#define GCI_MAX 64

typedef struct {
    char   path[1024];
    long   mtime;
    char  *text;
    size_t len;
} GciEntry;

static GciEntry g_idx[GCI_MAX];
static int      g_idx_count = 0;

static long file_mtime(const char *path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0) return -1;
#else
    struct stat st;
    if (stat(path, &st) != 0) return -1;
#endif
    return (long)st.st_mtime;
}

static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

static GciEntry *find_entry(const char *path) {
    for (int i = 0; i < g_idx_count; i++)
        if (strcmp(g_idx[i].path, path) == 0) return &g_idx[i];
    return NULL;
}

void gcl_index_clear(void) {
    for (int i = 0; i < g_idx_count; i++) free(g_idx[i].text);
    g_idx_count = 0;
    gcl_index_scope_clear();   /* kapsam önbelleği de geçersiz */
}

void gcl_index_invalidate(const char *path) {
    if (!path) return;
    GciEntry *e = find_entry(path);
    if (e) e->mtime = -1; /* bir sonraki okuma tazeler */
}

int gcl_index_read(const char *path, const char **out_text,
                   size_t *out_len, long *out_mtime) {
    if (!path || !path[0]) return 0;
    long mt = file_mtime(path);
    if (mt < 0) return 0;

    GciEntry *e = find_entry(path);
    if (e && e->mtime == mt && e->text) {
        if (out_text)  *out_text  = e->text;
        if (out_len)   *out_len   = e->len;
        if (out_mtime) *out_mtime = mt;
        return 1;
    }

    size_t nl = 0;
    char *nt = read_whole_file(path, &nl);
    if (!nt) return 0;

    if (e) {
        free(e->text);
        e->text = nt;
        e->len = nl;
        e->mtime = mt;
    } else {
        if (g_idx_count >= GCI_MAX) {
            /* FIFO tahliye: en eski girdiyi at. */
            free(g_idx[0].text);
            memmove(&g_idx[0], &g_idx[1], sizeof(GciEntry) * (size_t)(GCI_MAX - 1));
            g_idx_count = GCI_MAX - 1;
        }
        e = &g_idx[g_idx_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->path, sizeof(e->path), "%s", path);
        e->text = nt;
        e->len = nl;
        e->mtime = mt;
    }

    if (out_text)  *out_text  = e->text;
    if (out_len)   *out_len   = e->len;
    if (out_mtime) *out_mtime = mt;
    return 1;
}
/* ================================================================== */
/* Aktif buffer kapsam önbelleği (§12, S7)                             */
/* ================================================================== */

/* Tek girişli önbellek: motor tek iş parçacıklı ve tek aktif buffer'lıdır.
   Tarama sonucu (GclScope) buffer içerik hash'i ile saklanır. */
typedef struct {
    int                 used;
    char                file[512];
    char                workspace[1024];
    unsigned long long  hash;
    GclScope           *scope;
} GciScopeEntry;

static GciScopeEntry g_scope;

void gcl_index_scope_clear(void) {
    if (g_scope.scope) gcl_scope_destroy(g_scope.scope);
    g_scope.scope = NULL;
    g_scope.used = 0;
    g_scope.hash = 0;
    g_scope.file[0] = '\0';
    g_scope.workspace[0] = '\0';
}

/* FNV-1a 64: içerik değişimini O(n) tespit eder (parsing'den çok daha ucuz). */
static unsigned long long gci_hash_bytes(const char *p, size_t n) {
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

GclScope *gcl_index_scope(const char *file, const char *text, size_t len,
                          const char *workspace) {
    if (!text) return NULL;
    const char *f  = file ? file : "";
    const char *ws = workspace ? workspace : "";

    unsigned long long h = gci_hash_bytes(text, len);
    if (g_scope.used && g_scope.scope && g_scope.hash == h &&
        strcmp(g_scope.file, f) == 0 && strcmp(g_scope.workspace, ws) == 0)
        return g_scope.scope;

    if (!g_scope.scope) {
        g_scope.scope = gcl_scope_create();
        if (!g_scope.scope) return NULL;
    }
    gcl_scope_reset(g_scope.scope);
    gcl_scope_build(g_scope.scope, f, text, len, ws);

    snprintf(g_scope.file, sizeof(g_scope.file), "%s", f);
    snprintf(g_scope.workspace, sizeof(g_scope.workspace), "%s", ws);
    g_scope.hash = h;
    g_scope.used = 1;
    return g_scope.scope;
}
