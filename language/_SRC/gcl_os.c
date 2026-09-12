/*
 * gcl_os.c — platform helpers.
 *
 * gcl_os_name: target platform name (windows/gnuLinux).
 * gcl_ensure_dir: creates missing directories.
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

/* Drive root ("D:" or "D:\"), filesystem root ("/") and UNC root
   ("\\server" or "\\server\share") already EXIST — mkdir must not be tried
   on them. Otherwise _mkdir returns another errno (e.g. EACCES/EINVAL)
   instead of EEXIST and wrongly makes gcl_ensure_dir return -1. */
static int gcl_is_root_dir(const char *p) {
    if (!p || !p[0]) return 0;
#ifdef _WIN32
    /* "D:" or "D:\" — drive root */
    if (p[0] && p[1] == ':') {
        if (p[2] == '\0') return 1;                                        /* "D:" */
        if ((p[2] == '\\' || p[2] == '/') && p[3] == '\0') return 1;       /* "D:\" */
        return 0;
    }
    /* "\\server" or "\\server\share" — UNC root (cannot be created) */
    if (p[0] == '\\' && p[1] == '\\') {
        const char *q = p + 2;                       /* skip the server name */
        while (*q && *q != '\\' && *q != '/') q++;
        if (!*q) return 1;                           /* "\\server" */
        q++;
        while (*q && *q != '\\' && *q != '/') q++;   /* skip the share name */
        if (!*q) return 1;                           /* "\\server\share" */
        return 0;
    }
#else
    if (p[0] == '/' && p[1] == '\0') return 1;       /* "/" */
#endif
    return 0;
}

/* Recursive directory creation: "a/b/c" -> a, a/b, a/b/c
   On Windows, absolute paths with a drive letter such as "C:\\a\\b" and UNC
   paths (\\server\\share\\...) are handled correctly. */
int gcl_ensure_dir(const char *dir) {
    if (!dir || !dir[0]) return 0;
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", dir);
#ifdef _WIN32
    /* Normalize to backslash and collapse repeated separators.
       The UNC prefix "\\" (two leading separators) and the drive root "D:\"
       are preserved; this cleans up dirty paths like "D:\/_2" -> "D:\_2". */
    size_t w = 0;
    char prev = '\0';
    for (size_t r = 0; tmp[r]; r++) {
        char c = tmp[r];
        if (c == '/' || c == '\\') {
            c = '\\';
            if (w > 0 && prev == '\\') {
                /* UNC: exactly two leading separators are preserved */
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

    /* Find the root directory position — iteration starts from here.
       This prevents trying invalid segments like "C:" for "C:\\a\\b". */
    size_t start = 1;
#ifdef _WIN32
    /* "C:\\..." or "C:/..." → skip drive + separator (first 3 characters) */
    if (len > 2 && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/')) {
        start = 3;
    }
    /* UNC: "\\server\share\..." → skip server+share names.
       No directory is CREATED under \server (the UNC root must already exist);
       the first mkdir starts at the first subdirectory under \server\share. */
    else if (len > 2 && tmp[0] == '\\' && tmp[1] == '\\') {
        char *sp = tmp + 2;
        while (*sp && *sp != '\\' && *sp != '/') sp++;  /* skip the server name */
        if (*sp) {
            sp++;
            while (*sp && *sp != '\\' && *sp != '/') sp++;  /* skip the share name */
        }
        if (*sp) sp++;  /* skip the last separator too */
        start = (size_t)(sp - tmp);
        if (start >= len) start = 1;  /* safety: if there is no separator at all */
    }
#else
    /* POSIX absolute path: "/..." → skip the leading separator */
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
