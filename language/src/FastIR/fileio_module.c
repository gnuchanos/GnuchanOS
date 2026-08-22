#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    int kind;
    int64_t i_val;
    double f_val;
    const char *s_val;
} Val;

typedef struct {
    FILE *handle;
    char path[512];
    char mode[16];
} FileHandle;

extern void store_var(void *st, const char *name, Val val);
extern Val val_int(int64_t v);
extern Val val_float(double v);
extern Val val_string(const char *s);
extern int64_t val_to_int(Val v);

#define MAX_OPEN_FILES 32

static FileHandle g_files[MAX_OPEN_FILES];
static int g_file_count = 0;

/* =========================================================
   FILE HANDLE MANAGEMENT
   ========================================================= */

static int get_file_handle_index(FILE *fp) {
    if (!fp) return -1;
    for (int i = 0; i < g_file_count; i++) {
        if (g_files[i].handle == fp)
            return i;
    }
    return -1;
}

static FILE *find_file_by_name(const char *path, const char *mode) {
    if (!path) return NULL;
    for (int i = 0; i < g_file_count; i++) {
        if (strcmp(g_files[i].path, path) == 0 &&
            strcmp(g_files[i].mode, mode) == 0) {
            return g_files[i].handle;
        }
    }
    return NULL;
}

static int register_file(FILE *fp, const char *path, const char *mode) {
    if (!fp || g_file_count >= MAX_OPEN_FILES) {
        return -1;
    }
    
    FileHandle *fh = &g_files[g_file_count];
    fh->handle = fp;
    
    /* Copy path safely */
    size_t plen = path ? strlen(path) : 0;
    if (plen > 511) plen = 511;
    if (plen > 0) {
        memcpy(fh->path, path, plen);
    }
    fh->path[plen] = '\0';
    
    /* Copy mode safely */
    size_t mlen = mode ? strlen(mode) : 0;
    if (mlen > 15) mlen = 15;
    if (mlen > 0) {
        memcpy(fh->mode, mode, mlen);
    }
    fh->mode[mlen] = '\0';
    
    return g_file_count++;
}

static void close_file(FILE *fp) {
    if (!fp) return;
    
    int idx = get_file_handle_index(fp);
    if (idx >= 0) {
        fclose(fp);
        
        /* Remove from registry by shifting */
        for (int i = idx; i < g_file_count - 1; i++) {
            g_files[i] = g_files[i + 1];
        }
        g_file_count--;
    }
}

static void close_all_files(void) {
    while (g_file_count > 0) {
        close_file(g_files[0].handle);
    }
}

/* =========================================================
   FOPEN - OPEN FILE
   =========================================================== */

void interp_fopen(void *st, const char *filename, const char *mode) {
    if (!st || !filename || !mode) {
        return;
    }
    
    /* Check for duplicate open in same mode */
    FILE *existing = find_file_by_name(filename, mode);
    if (existing) {
        /* Return existing handle as integer */
        store_var(st, "_file_handle", val_int((int64_t)(intptr_t)existing));
        return;
    }
    
    /* Open file with validation */
    FILE *fp = fopen(filename, mode);
    if (!fp) {
        fprintf(stderr, "gcl: error: cannot open file '%s' in mode '%s'\n", filename, mode);
        store_var(st, "_file_handle", val_int(0));  /* 0 = NULL */
        return;
    }
    
    /* Register and return handle */
    if (register_file(fp, filename, mode) >= 0) {
        store_var(st, "_file_handle", val_int((int64_t)(intptr_t)fp));
    } else {
        fprintf(stderr, "gcl: error: too many open files (max %d)\n", MAX_OPEN_FILES);
        fclose(fp);
        store_var(st, "_file_handle", val_int(0));
    }
}

/* =========================================================
   FCLOSE - CLOSE FILE
   ========================================================= */

void interp_fclose(void *st, int64_t handle) {
    if (!st) return;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle\n");
        return;
    }
    
    close_file(fp);
}

/* =========================================================
   FPRINTF - FORMATTED FILE OUTPUT
   ========================================================= */

void interp_fprintf(void *st, int64_t handle, const char *format, int argc) {
    (void)argc;  /* argc not used in simple implementation */
    if (!st || !format) return;
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fprintf\n");
        return;
    }
    
    /* Security: validate format string length */
    size_t flen = strlen(format);
    if (flen > 2048) {
        fprintf(stderr, "gcl: fprintf: format string too long (%zu bytes, max 2048)\n", flen);
        return;
    }
    
    /* Simple format string processing without full printf implementation
     * Just write the format string with basic %d, %s, %f support */
    
    const char *p = format;
    int arg_index = 0;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            
            /* Skip flags and width */
            while (*p && (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || 
                   *p == '0' || (*p >= '0' && *p <= '9'))) {
                p++;
            }
            
            /* Skip precision */
            if (*p == '.') {
                p++;
                while (*p && *p >= '0' && *p <= '9') p++;
            }
            
            /* Handle conversion specifier */
            switch (*p) {
            case 'd':
            case 'i': {
                /* TODO: would need argument passing from stack */
                fprintf(fp, "0");  /* placeholder */
                break;
            }
            case 's': {
                fprintf(fp, "(string)");  /* placeholder - argc not available in simple mode */
                break;
            }
            case 'f': {
                fprintf(fp, "0.0");  /* placeholder */
                break;
            }
            case '%': {
                fprintf(fp, "%%");
                break;
            }
            default:
                fprintf(fp, "%%%c", *p);
                break;
            }
            p++;
        } else if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case 'n': fprintf(fp, "\n"); break;
            case 't': fprintf(fp, "\t"); break;
            case 'r': fprintf(fp, "\r"); break;
            case '\\': fprintf(fp, "\\"); break;
            case '"': fprintf(fp, "\""); break;
            default: fprintf(fp, "%c", *p); break;
            }
            p++;
        } else {
            fprintf(fp, "%c", *p);
            p++;
        }
    }
}

/* =========================================================
   FSCANF - FORMATTED FILE INPUT
   ========================================================= */

void interp_fscanf(void *st, int64_t handle, const char *format) {
    if (!st || !format) return;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fscanf\n");
        return;
    }
    
    /* Security: validate format string */
    size_t flen = strlen(format);
    if (flen > 2048) {
        fprintf(stderr, "gcl: fscanf: format string too long (%zu bytes, max 2048)\n", flen);
        return;
    }
    
    /* Basic fscanf - placeholder for now */
    /* Real implementation would parse format and read from file */
}

/* =========================================================
   FREAD - READ BINARY DATA
   ========================================================= */

int64_t interp_fread(void *st, int64_t handle, int64_t size, int64_t count) {
    if (!st) return 0;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fread\n");
        return 0;
    }
    
    if (size <= 0 || count <= 0) return 0;
    
    /* Allocate buffer */
    size_t total_size = (size_t)size * (size_t)count;
    if (total_size > 1048576) {  /* 1MB limit */
        fprintf(stderr, "gcl: error: fread size too large (%zu bytes)\n", total_size);
        return 0;
    }
    
    char *buffer = (char *)malloc(total_size);
    if (!buffer) {
        fprintf(stderr, "gcl: error: fread malloc failed\n");
        return 0;
    }
    
    size_t read_count = fread(buffer, (size_t)size, (size_t)count, fp);
    
    free(buffer);
    return (int64_t)read_count;
}

/* =========================================================
   FWRITE - WRITE BINARY DATA
   ========================================================= */

int64_t interp_fwrite(void *st, int64_t handle, const char *data, int64_t size) {
    if (!st || !data) return 0;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fwrite\n");
        return 0;
    }
    
    if (size <= 0) return 0;
    
    size_t written = fwrite(data, 1, (size_t)size, fp);
    return (int64_t)written;
}

/* =========================================================
   FGETS - READ LINE FROM FILE
   ========================================================= */

void interp_fgets(void *st, const char *var_name, int64_t handle, int64_t max_len) {
    if (!st || !var_name) return;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fgets\n");
        store_var(st, var_name, val_string(""));
        return;
    }
    
    if (max_len <= 1) {
        store_var(st, var_name, val_string(""));
        return;
    }
    
    /* Cap to reasonable limit */
    if (max_len > 4096) max_len = 4096;
    
    char *buffer = (char *)malloc((size_t)max_len);
    if (!buffer) {
        fprintf(stderr, "gcl: error: fgets malloc failed\n");
        store_var(st, var_name, val_string(""));
        return;
    }
    
    char *result = fgets(buffer, (int)max_len, fp);
    
    if (!result) {
        free(buffer);
        store_var(st, var_name, val_string(""));
        return;
    }
    
    /* Store the line */
    store_var(st, var_name, val_string(buffer));
    free(buffer);
}

/* =========================================================
   FPUTS - WRITE LINE TO FILE
   ========================================================= */

int64_t interp_fputs(void *st, const char *text, int64_t handle) {
    if (!st || !text) return -1;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fputs\n");
        return -1;
    }
    
    int result = fputs(text, fp);
    return (int64_t)result;
}

/* =========================================================
   FSEEK - SEEK IN FILE
   ========================================================= */

int64_t interp_fseek(void *st, int64_t handle, int64_t offset, int64_t whence) {
    if (!st) return -1;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in fseek\n");
        return -1;
    }
    
    /* whence: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END */
    int seek_whence = SEEK_SET;
    if (whence == 1) seek_whence = SEEK_CUR;
    else if (whence == 2) seek_whence = SEEK_END;
    
    int result = fseek(fp, (long)offset, seek_whence);
    return (int64_t)result;
}

/* =========================================================
   FTELL - GET CURRENT FILE POSITION
   ========================================================= */

int64_t interp_ftell(void *st, int64_t handle) {
    if (!st) return -1;
    
    FILE *fp = (FILE *)(intptr_t)handle;
    if (!fp) {
        fprintf(stderr, "gcl: error: invalid file handle in ftell\n");
        return -1;
    }
    
    long pos = ftell(fp);
    return (int64_t)pos;
}

/* =========================================================
   CLEANUP
   ========================================================= */

void fileio_cleanup(void) {
    close_all_files();
}

