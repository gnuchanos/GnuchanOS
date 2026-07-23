#include "io.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *file_read(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    size_t usz = (size_t)sz;
    char *buf = malloc(usz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, usz, f);
    if (nread != usz) { free(buf); fclose(f); return NULL; }
    buf[usz] = '\0';
    fclose(f);
    return buf;
}

int file_copy_to_dir(const char *src, const char *dst_dir) {
    FILE *fs = fopen(src, "rb");
    if (!fs) return 0;
    fseek(fs, 0, SEEK_END);
    long sz = ftell(fs);
    if (sz < 0) { fclose(fs); return 0; }
    fseek(fs, 0, SEEK_SET);
    size_t usz = (size_t)sz;
    char *buf = malloc(usz);
    if (!buf) { fclose(fs); return 0; }
    size_t nread = fread(buf, 1, usz, fs);
    if (nread != usz) { free(buf); fclose(fs); return 0; }
    fclose(fs);

    const char *fname = strrchr(src, '/');
    const char *fname2 = strrchr(src, '\\');
    if (!fname) fname = fname2;
    else if (fname2 && fname2 > fname) fname = fname2;
    fname = fname ? fname + 1 : src;

    char dst[2048];
    snprintf(dst, sizeof(dst), "%.1024s/%.256s", dst_dir, fname);
    FILE *fd = fopen(dst, "wb");
    if (!fd) { free(buf); return 0; }
    fwrite(buf, 1, usz, fd);
    fclose(fd);
    free(buf);
    printf(CLR_PURPLE "[gcl]" CLR_RESET " copied %s → %s\n", src, dst);
    return 1;
}
