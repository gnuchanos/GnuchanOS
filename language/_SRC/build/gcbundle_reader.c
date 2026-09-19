/*
 * gcbundle_reader.c — .gcBundle reading (AppImage model).
 *
 * The .gcBundle file is read into memory; files are accessed via pointers
 * through the entry table. NOTHING IS WRITTEN TO DISK.
 */

#include "gcbundle.h"
#include "gcbundle_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>    /* for distinguishing mkdir errors (EEXIST) */

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

/* Get the file size as 64-bit and rewind the position.
   CRITICAL (Windows): because long is 32-bit, ftell() OVERFLOWS on files >2GB;
   _fseeki64/_ftelli64 are used. On Linux, ftello/fseeko (off_t 64-bit).
   This way large bundles are read correctly on Windows too. Success: >=0, error: -1. */
static long long gcb_file_size(FILE *f) {
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) return -1;
    long long sz = (long long)_ftelli64(f);
    if (_fseeki64(f, 0, SEEK_SET) != 0) return -1;
    return sz;
#else
    if (fseeko(f, 0, SEEK_END) != 0) return -1;
    long long sz = (long long)ftello(f);
    if (fseeko(f, 0, SEEK_SET) != 0) return -1;
    return sz;
#endif
}

/* out_dir + in-bundle rel path → full path for writing (native separator on Windows). */
static void build_out_path(char *out, size_t outsz, const char *out_dir, const char *rel) {
#ifdef _WIN32
    snprintf(out, outsz, "%s\\%s", out_dir, rel ? rel : "");
    for (char *p = out; *p; p++) if (*p == '/') *p = '\\';
#else
    snprintf(out, outsz, "%s/%s", out_dir, rel ? rel : "");
#endif
}

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
    /* prevent uint32 overflow: do the arithmetic through size_t (64-bit) */
    size_t table_bytes = (size_t)h->entry_count * sizeof(GcbEntryHeader);

    /* RD-3a: the entry table must start AFTER the header and stay within the file. */
    if ((size_t)h->entry_offset < sizeof(GcbHeader) ||
        (size_t)h->entry_offset + table_bytes > b->size) {
        gcb_set_error("GCB: entry table out of bounds");
        return -1;
    }
    /* RD-3b: the blob must come AFTER the entry table and stay within the file. */
    if ((size_t)h->blob_offset < (size_t)h->entry_offset + table_bytes ||
        (size_t)h->blob_offset > b->size) {
        gcb_set_error("GCB: blob offset out of bounds");
        return -1;
    }

    b->entry_count = h->entry_count;
    b->entries = (GcbEntryHeader *)(b->data + h->entry_offset);
    b->valid = 1;

    /* RD-2: every entry.path[GCB_PATH_MAX] field MUST be NUL-terminated.
       Otherwise strcmp in find_entry/gcb_entry_path reads out of bounds
       (crash on a corrupt/truncated bundle). Non-terminated path = corrupt
       package → reject. */
    for (size_t i = 0; i < b->entry_count; i++) {
        if (memchr(b->entries[i].path, '\0', GCB_PATH_MAX) == NULL) {
            b->valid = 0;
            gcb_set_error("GCB: entry path not terminated");
            return -1;
        }
        if (b->entries[i].type != GCB_ENTRY_FILE &&
            b->entries[i].type != GCB_ENTRY_DIR) {
            b->valid = 0;
            gcb_set_error("GCB: bad entry type");
            return -1;
        }
    }
    return 0;
}

GcbBundle *gcb_open(const char *path) {
    if (!path) { gcb_set_error("GCB: path is NULL"); return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) { gcb_set_error("GCB: cannot open"); return NULL; }

    long long sz = gcb_file_size(f);
    if (sz < 0) { fclose(f); gcb_set_error("GCB: seek/tell failed"); return NULL; }
    /* prevent 32-bit size_t overflow (size + 1 terminator byte) */
    if ((unsigned long long)sz >= (unsigned long long)SIZE_MAX) {
        fclose(f); gcb_set_error("GCB: file too large");
        return NULL;
    }

    GcbBundle *b = (GcbBundle *)calloc(1, sizeof(GcbBundle));
    if (!b) { fclose(f); gcb_set_error("GCB: out of memory"); return NULL; }

    b->data = (unsigned char *)malloc((size_t)sz + 1);
    if (!b->data) { free(b); fclose(f); gcb_set_error("GCB: out of memory"); return NULL; }
    size_t rd = fread(b->data, 1, (size_t)sz, f);
    fclose(f);

    /* RD-1: A short read (corrupt/truncated file, network drive) must NOT be
       silently accepted — otherwise the entry table/blob is read over
       incomplete data. */
    if (rd != (size_t)sz) {
        free(b->data);
        free(b);
        gcb_set_error("GCB: short read (truncated file)");
        return NULL;
    }
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
    /* prevent uint32 overflow: do the arithmetic through size_t (64-bit) */
    if ((size_t)e->offset + (size_t)e->size > b->size) { gcb_set_error("GCB: entry out of bounds"); return NULL; }
    if (size) *size = e->size;
    return b->data + e->offset;
}

/* Create a single directory. EEXIST (already exists) is considered NORMAL;
   other errors (no permission, invalid path, ...) return -1. RD-4: a failure
   is not silently swallowed — so gcb_extract_all can report a clear error. */
static int mkdir_one(const char *dir) {
#ifdef _WIN32
    if (_mkdir(dir) != 0 && errno != EEXIST) return -1;
#else
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
#endif
    return 0;
}

/* Create a directory along with its subdirectories (mkdir -p).
   On Windows, absolute paths with a drive letter such as "C:\path\to\dir" and
   UNC paths (\\server\share\...) are handled correctly — invalid fragments
   like "C:" are never attempted. Success: 0, real error: -1. */
static int mkdir_p(const char *path) {
    int rc = 0;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
#ifdef _WIN32
    for (char *p = tmp; *p; p++) if (*p == '/') *p = '\\';
#endif
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    /* Trim the trailing separator */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = '\0';

    /* Find the root directory position — iteration starts here */
    size_t start = 1;
#ifdef _WIN32
    /* "C:\..." → skip drive letter + separator */
    if (len > 2 && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/')) {
        start = 3;
    }
    /* UNC: "\\server\share\..." → skip the server+share name.
       No directory is CREATED on \\server (the UNC root must already exist);
       the first mkdir starts from the first subdirectory under \\server\share. */
    else if (len > 2 && tmp[0] == '\\' && tmp[1] == '\\') {
        char *sp = tmp + 2;
        while (*sp && *sp != '\\' && *sp != '/') sp++;  /* skip server name */
        if (*sp) {
            sp++;
            while (*sp && *sp != '\\' && *sp != '/') sp++;  /* skip share name */
        }
        if (*sp) sp++;  /* also skip the final separator */
        start = (size_t)(sp - tmp);
        if (start >= len) start = 1;  /* safety: if there is no separator at all */
    }
#else
    /* POSIX absolute path: "/..." → skip the leading separator */
    if (tmp[0] == '/') start = 1;
#endif

    for (char *p = tmp + start; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
            if (mkdir_one(tmp) != 0) rc = -1;
            *p = c;
        }
    }
    if (mkdir_one(tmp) != 0) rc = -1;
    return rc;
}

/* Writes ALL files in the bundle under out_dir (recursive). */
int gcb_extract_all(const GcbBundle *b, const char *out_dir) {
    if (!b || !b->valid || !out_dir) { gcb_set_error("GCB: extract null param"); return -1; }
    if (mkdir_p(out_dir) != 0) {
        gcb_set_error("GCB: cannot create extract directory");
        return -1;
    }

    /* 1) Create directory entries — so empty directories (like assets/) are kept.
       Otherwise only directories containing files would be written to disk. */
    for (uint32_t i = 0; i < b->entry_count; i++) {
        if (b->entries[i].type != GCB_ENTRY_DIR) continue;
        if (!b->entries[i].path[0]) continue;   /* skip the root directory entry */
        char full[8192];
        build_out_path(full, sizeof(full), out_dir, b->entries[i].path);
        if (mkdir_p(full) != 0) {
            gcb_set_error("GCB: cannot create extract subdirectory");
            return -1;
        }
    }

    /* 2) Write files — create the parent directory if it does not exist. */
    for (uint32_t i = 0; i < b->entry_count; i++) {
        if (b->entries[i].type != GCB_ENTRY_FILE) continue;
        char full[8192];
        build_out_path(full, sizeof(full), out_dir, b->entries[i].path);

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
