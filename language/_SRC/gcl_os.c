/*
 * gcl_os.c — platform yardımcıları.
 *
 * gcl_os_name: hedef platform adı (windows/gnuLinux).
 * gcl_ensure_dir: eksik dizinleri oluşturur.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

const char *gcl_os_name(void) {
#ifdef _WIN32
    return "windows";
#elif defined(__linux__)
    return "gnuLinux";
#else
    return "unknown";
#endif
}

/* Sürücü kökü ("D:" veya "D:\"), dosya sistemi kökü ("/") ve UNC kökü
   ("\\server" veya "\\server\share") zaten VAR — bunlara mkdir denenmemeli.
   Aksi halde _mkdir, EEXIST yerine başka bir errno (örn. EACCES/EINVAL)
   döndürüp gcl_ensure_dir'i yanlışlıkla -1'e düşürür. */
static int gcl_is_root_dir(const char *p) {
    if (!p || !p[0]) return 0;
#ifdef _WIN32
    /* "D:" veya "D:\" — sürücü kökü */
    if (p[0] && p[1] == ':') {
        if (p[2] == '\0') return 1;                                        /* "D:" */
        if ((p[2] == '\\' || p[2] == '/') && p[3] == '\0') return 1;       /* "D:\" */
        return 0;
    }
    /* "\\server" veya "\\server\share" — UNC kökü (oluşturulamaz) */
    if (p[0] == '\\' && p[1] == '\\') {
        const char *q = p + 2;                       /* server adını atla */
        while (*q && *q != '\\' && *q != '/') q++;
        if (!*q) return 1;                           /* "\\server" */
        q++;
        while (*q && *q != '\\' && *q != '/') q++;   /* share adını atla */
        if (!*q) return 1;                           /* "\\server\share" */
        return 0;
    }
#else
    if (p[0] == '/' && p[1] == '\0') return 1;       /* "/" */
#endif
    return 0;
}

/* Recursive dizin oluşturma: "a/b/c" -> a, a/b, a/b/c
   Windows'ta "C:\\a\\b" gibi sürücü harfli mutlak yollar ve UNC yolları
   (\\server\\share\\...) doğru işlenir. */
int gcl_ensure_dir(const char *dir) {
    if (!dir || !dir[0]) return 0;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", dir);
#ifdef _WIN32
    /* Backslash'e normalize et ve tekrar eden ayraçları daralt.
       UNC öneki "\\" (iki baştaki ayraç) ve sürücü kökü "D:\" korunur;
       böylece "D:\/_2" -> "D:\_2" gibi kirli yollar temizlenir. */
    size_t w = 0;
    char prev = '\0';
    for (size_t r = 0; tmp[r]; r++) {
        char c = tmp[r];
        if (c == '/' || c == '\\') {
            c = '\\';
            if (w > 0 && prev == '\\') {
                /* UNC: tam olarak iki baştaki ayraç korunur */
                if (w == 1 && tmp[0] == '\\') {
                    tmp[w++] = c;
                    prev = c;
                }
                continue;
            }
        }
        tmp[w++] = c;
        prev = c;
    }
    tmp[w] = '\0';
#endif
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    /* Trim trailing separator */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = '\0';

    /* Kök dizin konumunu bul — iterasyon buradan başlar.
       Bu, "C:\\a\\b" için "C:" gibi geçersiz parçaların denenmesini önler. */
    size_t start = 1;
#ifdef _WIN32
    /* "C:\\..." veya "C:/..." → sürücü + ayraç atla (ilk 3 karakter) */
    if (len > 2 && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/')) {
        start = 3;
    }
    /* UNC: "\\server\share\..." → server+share adını atla.
       \server adında dizin OLUŞTURULMAZ (UNC root'u var olmalıdır);
       ilk mkdir \server\share altındaki ilk alt dizinden başlar. */
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
            char save = *p;
            *p = '\0';
            if (!gcl_is_root_dir(tmp)) {
#ifdef _WIN32
                if (_mkdir(tmp) != 0 && errno != EEXIST) { *p = save; return -1; }
#else
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { *p = save; return -1; }
#endif
            }
            *p = save;
        }
    }
    if (!gcl_is_root_dir(tmp)) {
#ifdef _WIN32
        if (_mkdir(tmp) != 0 && errno != EEXIST) return -1;
#else
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
#endif
    }
    return 0;
}
