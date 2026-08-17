/*
 * gclib_utils.c — .gcLib / .gcDL module query tool
 *
 *   gcl -libs                      -> list all modules inside Library
 *   gcl -libcheck <module>         -> does the module exist? (exit 0/1)
 *   gcl -lib <module> -luarun ...  -> if the module EXISTS run the requested work
 *
 * Module concept:
 *   lua          -> Library/Lua/lua.gcDL
 *   lua_raylib   -> Library/Lua/lua_raylib.gcDL
 *   raylib       -> alias: lua_raylib
 *   python       -> Library/Python/python.gcDL
 *   python_raylib-> Library/Python/python_raylib.gcDL
 *   <name>       -> user-defined: Library/Native/<name>.gcDL or
 *                   Library/Extern/<name>.gclib  (external module files)
 *                   also searched inside Lua/luaLibrary, Python/pyLibrary
 */

#define _CRT_SECURE_NO_WARNINGS

#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "gclib_utils.h"

/* The Library directory NEXT TO the exe:
 *   Windows: GetModuleFileNameA -> <exe>\Library
 *   Linux  : readlink /proc/self/exe -> <exe>/Library */
const char *gclib_library_dir(char *out, size_t cap) {
    char exe_path[4096];
    const char *sep = NULL;

    if (out == NULL || cap == 0)
        return NULL;

#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
        if (n == 0 || n >= (DWORD)sizeof exe_path)
            return NULL;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (n <= 0 || n >= (ssize_t)sizeof exe_path)
            return NULL;
        exe_path[n] = '\0';
    }
#endif

    for (char *p = exe_path; *p; p++) {
        if (*p == '\\' || *p == '/')
            sep = p;
    }
    if (sep == NULL)
        return NULL;

    {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        static const char librel[] = "Library";
        if (dir_len + sizeof librel >= cap)
            return NULL;
        memcpy(out, exe_path, dir_len);
        memcpy(out + dir_len, librel, sizeof librel);
    }

    return out;
}

/* Known module names -> path under Library.
 * 1 = found (out), 0 = not found. */
static int known_module_path(const char *name, char *out, size_t cap) {
    static const struct {
        const char *name;
        const char *rel;
    } known[] = {
        { "lua",           "Lua/lua.gcDL" },
        { "lua_raylib",    "Lua/lua_raylib.gcDL" },
        { "raylib",        "Lua/lua_raylib.gcDL" },   /* alias */
        { "python",        "Python/python.gcDL" },
        { "python_raylib", "Python/python_raylib.gcDL" },
    };
    char dir[4200];

    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) {
        if (strcmp(name, known[i].name) != 0)
            continue;
        if (gclib_library_dir(dir, sizeof dir) == NULL)
            return 0;
        {
            size_t dlen = strlen(dir);
            size_t rlen = strlen(known[i].rel);
            if (dlen + rlen + 2 >= cap)
                return 0;
            memcpy(out, dir, dlen);
            out[dlen] = '/';
            memcpy(out + dlen + 1, known[i].rel, rlen + 1);
        }
        return 1;
    }
    return 0;
}

/* User-defined <name>.gcDL / <name>.gclib search. */
static int custom_module_path(const char *name, char *out, size_t cap) {
    static const char *subdirs[] = {
        "Native", "Extern", "Lua", "Python",
        "Lua/luaLibrary", "Python/pyLibrary"
    };
    char dir[4200];
    char rel[512];

    if (gclib_library_dir(dir, sizeof dir) == NULL)
        return 0;

    for (size_t s = 0; s < sizeof subdirs / sizeof subdirs[0]; s++) {
        /* <Library>/<subdir>/<name>.gcDL */
        snprintf(rel, sizeof rel, "%s/%s.gcDL", subdirs[s], name);
        {
            size_t dlen = strlen(dir);
            size_t rlen = strlen(rel);
            if (dlen + rlen + 2 < cap) {
                memcpy(out, dir, dlen);
                out[dlen] = '/';
                memcpy(out + dlen + 1, rel, rlen + 1);
                {
                    FILE *fp = fopen(out, "rb");
                    if (fp != NULL) {
                        fclose(fp);
                        return 1;
                    }
                }
            }
        }
        /* <Library>/<subdir>/<name>.gclib */
        snprintf(rel, sizeof rel, "%s/%s.gclib", subdirs[s], name);
        {
            size_t dlen = strlen(dir);
            size_t rlen = strlen(rel);
            if (dlen + rlen + 2 < cap) {
                memcpy(out, dir, dlen);
                out[dlen] = '/';
                memcpy(out + dlen + 1, rel, rlen + 1);
                {
                    FILE *fp = fopen(out, "rb");
                    if (fp != NULL) {
                        fclose(fp);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

int gclib_find_module(const char *name, char *out, size_t cap) {
    if (name == NULL || name[0] == '\0')
        return 0;

    if (known_module_path(name, out, cap) == 1)
        return 1;
    return custom_module_path(name, out, cap);
}

static void free_lines(char **lines, size_t count) {
    if (lines == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        free(lines[i]);
    free(lines);
}

static int ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t xlen = strlen(suffix);
    return slen >= xlen && strcmp(s + slen - xlen, suffix) == 0;
}

/* Collects .gcDL / .gclib files in a folder as "<subdir>/<file>" lines.
 * Windows: FindFirstFileA, Linux: opendir/readdir. */
static int list_dir_gcdl(const char *rel_dir, const char *subdir,
                         char ***out, size_t *out_n) {
    char dir[4200];
    char **lines = NULL;
    size_t n = 0;

    *out = NULL;
    *out_n = 0;

    if (gclib_library_dir(dir, sizeof dir) == NULL)
        return -1;
    {
        size_t dlen = strlen(dir);
        size_t rlen = strlen(rel_dir);
        if (dlen + rlen + 2 >= sizeof dir)
            return -1;
        dir[dlen] = '/';
        memcpy(dir + dlen + 1, rel_dir, rlen + 1);
    }

#ifdef _WIN32
    {
        WIN32_FIND_DATAA fd;
        char pattern[4300];
        HANDLE h;
        snprintf(pattern, sizeof pattern, "%s/*", dir);
        h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return 0;   /* folder missing: empty list */
        do {
            const char *fname = fd.cFileName;
            char **grown;
            char *line;
            size_t len;
            if (!ends_with(fname, ".gcDL") && !ends_with(fname, ".gclib"))
                continue;
            grown = (char **)realloc(lines, (n + 1) * sizeof(char *));
            if (grown == NULL) {
                free_lines(lines, n);
                FindClose(h);
                *out_n = 0;
                return -1;
            }
            lines = grown;
            len = strlen(subdir) + 1 + strlen(fname) + 1;
            line = (char *)malloc(len);
            if (line == NULL) {
                free_lines(lines, n);
                FindClose(h);
                *out_n = 0;
                return -1;
            }
            snprintf(line, len, "%s/%s", subdir, fname);
            lines[n++] = line;
        } while (FindNextFileA(h, &fd) != 0);
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        struct dirent *ent;
        if (d == NULL)
            return 0;   /* folder missing: empty list */
        while ((ent = readdir(d)) != NULL) {
            const char *fname = ent->d_name;
            char **grown;
            char *line;
            size_t len;
            if (!ends_with(fname, ".gcDL") && !ends_with(fname, ".gclib"))
                continue;
            grown = (char **)realloc(lines, (n + 1) * sizeof(char *));
            if (grown == NULL) {
                free_lines(lines, n);
                closedir(d);
                *out_n = 0;
                return -1;
            }
            lines = grown;
            len = strlen(subdir) + 1 + strlen(fname) + 1;
            line = (char *)malloc(len);
            if (line == NULL) {
                free_lines(lines, n);
                closedir(d);
                *out_n = 0;
                return -1;
            }
            snprintf(line, len, "%s/%s", subdir, fname);
            lines[n++] = line;
        }
        closedir(d);
    }
#endif

    *out = lines;
    *out_n = n;
    return 0;
}

int gclib_list_all(char *err, size_t err_cap) {
    static const char *dirs[] = {
        "Lua", "Python", "Lua/luaLibrary", "Python/pyLibrary",
        "Native", "Extern"
    };
    static const char *labels[] = {
        "Lua", "Python", "Lua/luaLibrary", "Python/pyLibrary",
        "Native", "Extern"
    };
    char **all = NULL;
    size_t all_n = 0;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        char **lines = NULL;
        size_t n = 0;
        if (list_dir_gcdl(dirs[i], labels[i], &lines, &n) != 0) {
            free_lines(all, all_n);
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot scan directory %s", dirs[i]);
            return -1;
        }
        for (size_t k = 0; k < n; k++) {
            char **grown = (char **)realloc(all, (all_n + 1) * sizeof(char *));
            if (grown == NULL) {
                free_lines(all, all_n);
                free_lines(lines, n);
                if (err && err_cap > 0)
                    snprintf(err, err_cap, "out of memory");
                return -1;
            }
            all = grown;
            all[all_n++] = lines[k];
        }
        free(lines);
    }

    printf("Library modules:\n");
    if (all_n == 0) {
        printf("  (empty)\n");
    } else {
        for (size_t i = 0; i < all_n; i++)
            printf("  %s\n", all[i]);
    }
    free_lines(all, all_n);
    return 0;
}
