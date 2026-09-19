/*
 * gcbundle.h — gcBundle public API + binary format.
 *
 * gcBundle = AppImage-like single-package output:
 *   gcl -build project.gcdata -o path/dir
 *       -> Project_name.exe
 *       -> Project_name.gcBundle   (all runtime + dll/so in a single package)
 *
 * At runtime the gcBundle is read INTO MEMORY; files are accessed via
 * pointers through the entry table. NOTHING IS WRITTEN TO DISK.
 */

#ifndef GCBUNDLE_H
#define GCBUNDLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Format constants ---------- */

#define GCB_MAGIC       "GCBL"
#define GCB_MAGIC_BYTES 4
#define GCB_VERSION     1

#define GCB_NAME_MAX    64
#define GCB_PATH_MAX    512

/* Entry types */
#define GCB_ENTRY_FILE  0
#define GCB_ENTRY_DIR   1

/* ---------- Format structures (packed) ---------- */

/*
 * Portable "packed" macros. GCC/Clang use __attribute__((packed));
 * MSVC does not UNDERSTAND this attribute (it is a compile error). On MSVC,
 * #pragma pack(1) provides the SAME layout — so the .gcBundle format stays
 * byte-for-byte compatible across all compilers and platforms (offsets are
 * written with sizeof()).
 */
#if defined(_MSC_VER)
#  define GCB_PACK_BEGIN __pragma(pack(push, 1))
#  define GCB_PACK_END   __pragma(pack(pop))
#  define GCB_PACKED
#else
#  define GCB_PACK_BEGIN
#  define GCB_PACK_END
#  define GCB_PACKED __attribute__((packed))
#endif

GCB_PACK_BEGIN

typedef struct GCB_PACKED {
    char     magic[GCB_MAGIC_BYTES]; /* "GCBL" */
    uint16_t version;                 /* GCB_VERSION */
    uint16_t flags;                   /* 0 */
    char     name[GCB_NAME_MAX];      /* project name */
    uint32_t entry_count;             /* number of elements in the entry table */
    uint32_t entry_offset;            /* offset of the entry table from the start of the file */
    uint32_t blob_offset;             /* offset of the blob data from the start of the file */
} GcbHeader;

typedef struct GCB_PACKED {
    char     path[GCB_PATH_MAX];
    uint32_t type;    /* GCB_ENTRY_FILE / GCB_ENTRY_DIR */
    uint32_t offset;  /* file data offset from the start of the file */
    uint32_t size;    /* file size */
} GcbEntryHeader;

GCB_PACK_END

/* gcBundle opened in memory (opaque). */
typedef struct GcbBundle GcbBundle;

/* ---------- Lifecycle ---------- */

/* Opens the .gcBundle file in memory. Success: GcbBundle*, error: NULL. */
GcbBundle *gcb_open(const char *path);

/* Opens from an in-memory buffer. */
GcbBundle *gcb_open_memory(const void *data, size_t size);

/* Closes and frees the memory. */
void gcb_close(GcbBundle *b);

/* ---------- Info ---------- */

const char *gcb_name(const GcbBundle *b);
uint32_t gcb_entry_count(const GcbBundle *b);
const char *gcb_entry_path(const GcbBundle *b, uint32_t i);
uint32_t gcb_entry_type(const GcbBundle *b, uint32_t i);
uint32_t gcb_entry_size(const GcbBundle *b, uint32_t i);

/* Returns a pointer to the file content for the given path (in memory). */
const void *gcb_read_path(const GcbBundle *b, const char *path, uint32_t *size);

/* Writes ALL files (file entries) in the bundle under out_dir.
   Directories are created recursively. On first run, the built exe extracts
   this next to itself — so Library/, scripts/, assets/ are written to disk
   and the native modules + Embed.Run scripts are found. Success: 0, error: -1. */
int gcb_extract_all(const GcbBundle *b, const char *out_dir);

/* ---------- Packing ---------- */

/*
 * Converts a directory into a .gcBundle package and writes it to out_path.
 * name: package name (project name).
 * Success: 0, error: -1.
 */
int gcb_pack_dir(const char *dir, const char *name, const char *out_path);

/*
 * Converts the project directory + runtime directory (Library/) into one package.
 * runtime_dir: language/build/<os>/ (all DLL/SO runtime under Library/)
 *              is embedded into the package with the "Library" prefix.
 * Success: 0, error: -1.
 */
int gcb_pack_project_runtime(const char *project_dir, const char *runtime_dir,
                             const char *name, const char *out_path);

/* ---------- Error ---------- */

const char *gcb_last_error(void);

/* Raw data access. */
const void *gcb_raw_data(const GcbBundle *b);
size_t gcb_raw_size(const GcbBundle *b);

#ifdef __cplusplus
}
#endif

#endif /* GCBUNDLE_H */
