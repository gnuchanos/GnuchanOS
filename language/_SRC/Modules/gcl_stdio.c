/*
 * gcl_stdio.c — GCL Stdio modülü (.dll/.so).
 *
 * simple_doc.md:
 *   Stdio.printf(); Stdio.scanf("%", ...);
 *   openFile/writeFile/readFile/closeFile/appendFile,
 *   fileExists/deleteFile/renameFile/fileSize/flushFile
 *   (camelCase ve lowercase varyantları)
 */

#include "gcl_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

static double fn_printf(int argc, const char **argv) {
    if (argc <= 0 || !argv[0]) return 0.0;
    const char *fmt = argv[0];
    int argi = 1;
    const char *p = fmt;
    while (*p) {
        if (*p == '{') {
            int prec = -1;
            p++;
            if (*p == '.') {
                p++;
                if (isdigit((unsigned char)*p)) { prec = *p - '0'; p++; }
                if (*p == 'f') p++;
            }
            if (*p == '}') p++;
            if (argi < argc && argv[argi]) {
                if (prec >= 0) {
                    double d = atof(argv[argi]);
                    printf("%.*f", prec, d);
                } else {
                    fputs(argv[argi], stdout);
                }
            }
            argi++;
        } else if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                default: putchar(*p); break;
            }
            p++;
        } else {
            putchar(*p);
            p++;
        }
    }
    fflush(stdout);
    return 0.0;
}

static double fn_scanf(int argc, const char **argv) {
    (void)argc; (void)argv;
    return 0.0;
}

static double fn_open_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    FILE *f = fopen(file, "r");
    if (f) fclose(f);
    return f ? 1.0 : 0.0;
}

static double fn_write_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    const char *text = argc > 1 ? argv[1] : "";
    FILE *f = fopen(file, "w");
    if (!f) return 0.0;
    fputs(text, f);
    fclose(f);
    return 1.0;
}

static double fn_read_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    FILE *f = fopen(file, "rb");
    if (!f) return -1.0;
    long sz = 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1.0; }
    sz = ftell(f);
    fclose(f);
    return (double)sz;
}

static double fn_close_file(int argc, const char **argv) {
    (void)argc; (void)argv;
    return 0.0;
}

static double fn_append_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    const char *text = argc > 1 ? argv[1] : "";
    FILE *f = fopen(file, "a");
    if (!f) return 0.0;
    fputs(text, f);
    fclose(f);
    return 1.0;
}

static double fn_file_exists(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    struct stat st;
    return stat(file, &st) == 0 ? 1.0 : 0.0;
}

static double fn_delete_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    return remove(file) == 0 ? 1.0 : 0.0;
}

static double fn_rename_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    const char *newName = argc > 1 ? argv[1] : "";
    return rename(file, newName) == 0 ? 1.0 : 0.0;
}

static double fn_file_size(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    struct stat st;
    if (stat(file, &st) != 0) return -1.0;
    return (double)st.st_size;
}

static double fn_flush_file(int argc, const char **argv) {
    const char *file = argc > 0 ? argv[0] : "";
    FILE *f = fopen(file, "r+");
    if (f) { fflush(f); fclose(f); }
    return f ? 1.0 : 0.0;
}

static const GclNativeEntry g_entries[] = {
    {"printf", fn_printf},
    {"scanf", fn_scanf},
    {"openFile", fn_open_file},
    {"openfile", fn_open_file},
    {"writeFile", fn_write_file},
    {"writefile", fn_write_file},
    {"readFile", fn_read_file},
    {"readfile", fn_read_file},
    {"closeFile", fn_close_file},
    {"closefile", fn_close_file},
    {"appendFile", fn_append_file},
    {"appendfile", fn_append_file},
    {"fileExists", fn_file_exists},
    {"fileexists", fn_file_exists},
    {"deleteFile", fn_delete_file},
    {"deletefile", fn_delete_file},
    {"renameFile", fn_rename_file},
    {"renamefile", fn_rename_file},
    {"fileSize", fn_file_size},
    {"filesize", fn_file_size},
    {"flushFile", fn_flush_file},
    {"flushfile", fn_flush_file},
};

GCL_EXPORT const GclNativeEntry *gcl_stdio_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
