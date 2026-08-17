/*
 * gcdl_bridge.c — GCL cross-language data bridge (.gcDL)
 *
 * Lua ve Python embed'leri AYRI sureclerde calisir (gcl -luarun x.lua /
 * gcl -pyrun y.py). Her surec kendi raylib penceresini acar. Bu modul,
 * exe'nin yanindaki  Library/bridge/<session>/  klasorunde  key -> value
 * dosyalari yonetir (her deger kendi .gcv dosyasinda; yazma islemi tmp
 * dosyaya yazip atomik rename ile yerine koyar). Boylece ayni anda iki
 * surec ayni havuzda rahatca okur/yazar, hicbir kilit gerekmez:
 *
 *   - Lua agent    "ball_lua" degerini yazar   + "ball_py"yi okur
 *   - Python agent "ball_py" degerini yazar    + "ball_lua"yi okur
 *   - GCL her iki tarafa da ayni API'yi sunar:
 *       Lua   : gcl.bridge.open/get/set/delete/list
 *       Python: gcl_bridge.open/get/set/delete/list
 *
 * Degerler serbest metindir — JSON kullanmaniz onerilir
 * (ornek: "{\"x\":10,\"y\":20,\"vx\":1}") . Newline yasaktir.
 *
 * Build (makefile.py tarafindan):
 *   Windows: gcc -shared gcdl_bridge.c -o Library/bridge/bridge.gcDL
 *   Linux  : gcc -shared -fPIC gcdl_bridge.c -o Library/bridge/bridge.gcDL
 */

#define _CRT_SECURE_NO_WARNINGS

#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#if defined(_WIN32)
#define GCL_MODULE_EXPORT __declspec(dllexport)
#include <windows.h>
#include <direct.h>   /* _mkdir */
#else
#define GCL_MODULE_EXPORT __attribute__((visibility("default")))
#include <dlfcn.h>
#include <dirent.h>   /* opendir/readdir */
#include <unistd.h>   /* readlink() */
#include <sys/stat.h> /* mkdir */
#include <sys/types.h>
#include <time.h>
#endif

#define GCL_BRIDGE_MAX_PATH  4096
#define GCL_BRIDGE_MAX_KEY   128
#define GCL_BRIDGE_MAX_VALUE (1024 * 1024)

/* Aktif oturum. Ayni anda birden fazla oyun ayri oturum kullanabilir:
 *   gcl.bridge.open("mygame")  — Library/bridge/mygame/
 * varsayilan: "default". */
static char g_session[GCL_BRIDGE_MAX_PATH] = "default";

/* ---- yardimcilar ---- */

static int bridge_mkdirs(const char *path) {
    /* Path'i basamak basamak olusturur (Library, Library/bridge,
     * Library/bridge/<session>). Disk uzerinde ara dizinler yoksa da
     * calisir. */
    char tmp[GCL_BRIDGE_MAX_PATH];
    size_t len;

    if (path == NULL || path[0] == '\0') return -1;
    len = strlen(path);
    if (len >= sizeof tmp) return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '\\' || *p == '/') {
            char saved = *p;
            *p = '\0';
            if (tmp[0] != '\0') {
#ifdef _WIN32
                _mkdir(tmp);
#else
                mkdir(tmp, 0755);
#endif
            }
            *p = saved;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
    return 0;
}

/* exe'nin yanindaki Library/bridge/<session>/ dizinini doldurur. */
static int bridge_dir(char *out, size_t cap) {
    char exe_path[GCL_BRIDGE_MAX_PATH];
    const char *sep = NULL;

    if (out == NULL || cap == 0) return -1;

#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
        if (n == 0 || n >= (DWORD)sizeof exe_path) return -1;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (n <= 0 || n >= (ssize_t)sizeof exe_path) return -1;
        exe_path[n] = '\0';
    }
#endif

    for (char *p = exe_path; *p; p++) {
        if (*p == '\\' || *p == '/') sep = p;
    }
    if (sep == NULL) return -1;

    {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        const char *rel = g_session[0] != '\0' ? g_session : "default";
        int w;
        if (dir_len >= cap) return -1;
        memcpy(out, exe_path, dir_len);
        out[dir_len] = '\0';
        /* out = <dir>Library/bridge/<session>/ */
        w = snprintf(out + dir_len, cap - dir_len, "Library/bridge/%s/", rel);
        if (w < 0 || (size_t)w >= cap - dir_len) return -1;
        bridge_mkdirs(out);
    }
    return 0;
}

/* key -> gecerli mi? [A-Za-z0-9_.-]+  (dosya adi guvenligi) */
static int bridge_valid_key(const char *key) {
    size_t len;
    if (key == NULL) return 0;
    len = strlen(key);
    if (len == 0 || len >= GCL_BRIDGE_MAX_KEY) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = key[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!ok) return 0;
    }
    return 1;
}

/* <dir><key>.gcv  tam yolunu doldurur. */
static int bridge_file_path(char *out, size_t cap, const char *key) {
    char dir[GCL_BRIDGE_MAX_PATH];
    int w;
    if (!bridge_valid_key(key)) return -1;
    if (bridge_dir(dir, sizeof dir) != 0) return -1;
    w = snprintf(out, cap, "%s%s.gcv", dir, key);
    if (w < 0 || (size_t)w >= cap) return -1;
    return 0;
}

/* tmp dosyayi hedefe atomik tasir (Windows: MoveFileEx / POSIX: rename).
 * Okuyucu ya eski tam dosyayi ya yeni tam dosyayi gorur — hicbir zaman
 * karisik/yarim icerik gormez. */
static int bridge_atomic_replace(const char *tmp, const char *dst) {
#ifdef _WIN32
    if (MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
        return -1;
    return 0;
#else
    if (rename(tmp, dst) != 0) return -1;
    return 0;
#endif
}

static unsigned long long bridge_millis(void) {
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)(ts.tv_nsec / 1000000);
#endif
}

/* ---- API ---- */

/* Oturum acar: Library/bridge/<session>/ dizinini hazirlar.
 * return: 0 basarili. */
GCL_MODULE_EXPORT int gcdl_bridge_open(const char *session) {
    char dir[GCL_BRIDGE_MAX_PATH];

    if (session != NULL && session[0] != '\0') {
        if (strlen(session) >= sizeof g_session) return -1;
        /* session da dosya adi guvenligi: sadece guvenli karakterler */
        for (const char *p = session; *p; p++) {
            char c = *p;
            int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
            if (!ok) return -1;
        }
        memcpy(g_session, session, strlen(session) + 1);
    } else {
        memcpy(g_session, "default", sizeof "default");
    }

    if (bridge_dir(dir, sizeof dir) != 0) return -1;
    return 0;
}

/* key'e deger yazar (newline yasak). return: 0 basarili. */
GCL_MODULE_EXPORT int gcdl_bridge_set(const char *key, const char *value) {
    char path[GCL_BRIDGE_MAX_PATH];
    char tmp[GCL_BRIDGE_MAX_PATH];
    FILE *f;
    size_t vlen;

    if (!bridge_valid_key(key)) return 1;
    if (value == NULL) value = "";
    vlen = strlen(value);
    if (vlen > GCL_BRIDGE_MAX_VALUE) return 1;
    if (strchr(value, '\n') != NULL) return 1;      /* line format: satirlardan olusur */
    if (bridge_file_path(path, sizeof path, key) != 0) return 1;

    snprintf(tmp, sizeof tmp, "%s.tmp.%llu", path,
             (unsigned long long)bridge_millis());

    f = fopen(tmp, "wb");
    if (f == NULL) return 1;
    if (vlen > 0 && fwrite(value, 1, vlen, f) != vlen) {
        fclose(f);
        remove(tmp);
        return 1;
    }
    fclose(f);

    if (bridge_atomic_replace(tmp, path) != 0) {
        remove(tmp);
        return 1;
    }
    return 0;
}

/* key'in degerini out'a kopyalar. return: 0 bulundu, 1 yok, -1 hata. */
GCL_MODULE_EXPORT int gcdl_bridge_get(const char *key, char *out, size_t cap) {
    char path[GCL_BRIDGE_MAX_PATH];
    FILE *f;
    long size;
    size_t n;

    if (out == NULL || cap == 0) return -1;
    out[0] = '\0';
    if (!bridge_valid_key(key)) return -1;
    if (bridge_file_path(path, sizeof path, key) != 0) return -1;

    f = fopen(path, "rb");
    if (f == NULL) return 1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    size = ftell(f);
    if (size < 0) { fclose(f); return -1; }
    rewind(f);
    if ((unsigned long)size >= cap) { fclose(f); return -1; }
    if (size > 0) {
        n = fread(out, 1, (size_t)size, f);
        if (n != (size_t)size) { fclose(f); return -1; }
    }
    fclose(f);
    out[size] = '\0';
    return 0;
}

/* key'i siler. return: 0 silindi, 1 yoktu, -1 hata. */
GCL_MODULE_EXPORT int gcdl_bridge_delete(const char *key) {
    char path[GCL_BRIDGE_MAX_PATH];
    if (!bridge_valid_key(key)) return -1;
    if (bridge_file_path(path, sizeof path, key) != 0) return -1;
    if (remove(path) != 0) return 1;
    return 0;
}

/* Tum anahtarlari virgulle ayrilmis olarak out'a yazar. */
GCL_MODULE_EXPORT int gcdl_bridge_list(char *out, size_t cap) {
    char dir[GCL_BRIDGE_MAX_PATH];
    size_t used = 0;

    if (out == NULL || cap == 0) return -1;
    out[0] = '\0';
    if (bridge_dir(dir, sizeof dir) != 0) return -1;

#ifdef _WIN32
    {
        char pattern[GCL_BRIDGE_MAX_PATH];
        WIN32_FIND_DATAA fd;
        HANDLE h;
        snprintf(pattern, sizeof pattern, "%s*.gcv", dir);
        h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) return 0;
        do {
            size_t name_len = strlen(fd.cFileName);
            if (name_len > 4 && memcmp(fd.cFileName + name_len - 4, ".gcv", 4) == 0) {
                size_t klen = name_len - 4;
                int clen = snprintf(out + used, cap - used, "%s%.*s",
                                    used > 0 ? "," : "",
                                    (int)klen, fd.cFileName);
                if (clen > 0 && (size_t)clen < cap - used) {
                    used += (size_t)clen;
                    out[used] = '\0';
                }
            }
        } while (FindNextFileA(h, &fd) != 0);
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        struct dirent *e;
        if (d == NULL) return 0;
        while ((e = readdir(d)) != NULL) {
            size_t name_len = strlen(e->d_name);
            if (name_len > 4 && memcmp(e->d_name + name_len - 4, ".gcv", 4) == 0 &&
                e->d_name[0] != '.') {
                int clen = snprintf(out + used, cap - used, "%s%.*s",
                                    used > 0 ? "," : "",
                                    (int)(name_len - 4), e->d_name);
                if (clen > 0 && (size_t)clen < cap - used) {
                    used += (size_t)clen;
                    out[used] = '\0';
                }
            }
        }
        closedir(d);
    }
#endif
    return 0;
}
