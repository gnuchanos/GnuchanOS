/*
 * gcdl_loader.c — gcDL loader
 *
 * GCDL = gcl Dynamic Library. The loader loads a .gcDL file into the
 * process memory at run time (Windows: LoadLibrary, Linux: dlopen) and
 * returns the addresses of code + data contained in it.
 *
 * On Windows the dependent DLLs of a .gcDL module (e.g. python314.dll for
 * python.gcDL) are resolved from the module's own directory. Since the
 * Python runtime lives in Library/Python/pyLibrary/ while python.gcDL
 * sits in Library/Python/, the pyLibrary subfolder (if present) is
 * added to the DLL search path for the duration of the load.
 */

#define _CRT_SECURE_NO_WARNINGS

/* Needed for glibc to expose readlink/PATH_MAX under strict -std=c11 */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include "gcdl_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

struct GclGcDl {
    void *handle;
};

static void set_err(char *err, size_t cap, const char *fmt, const char *arg) {
    if (err != NULL && cap > 0)
        snprintf(err, cap, fmt, arg ? arg : "");
}

static const char *last_sep(const char *s) {
    const char *slash = strrchr(s, '/');
    const char *bslash = strrchr(s, '\\');
    if (slash == NULL)
        return bslash;
    if (bslash == NULL)
        return slash;
    return (slash > bslash) ? slash : bslash;
}

/* Finds the Library folder next to the exe, whatever the working directory. */
GclGcDl *gcdl_load_adjacent(const char *rel_path, char *err, size_t err_cap) {
    char exe_path[4096];
    char full[4200];
    const char *sep;

    if (rel_path == NULL || rel_path[0] == '\0') {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "gcdl: rel_path is empty");
        return NULL;
    }

#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
        if (n == 0 || n >= (DWORD)sizeof exe_path) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "gcdl: cannot get exe path");
            return NULL;
        }
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (n <= 0 || n >= (ssize_t)sizeof exe_path) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "gcdl: cannot get exe path");
            return NULL;
        }
        exe_path[n] = '\0';
    }
#endif

    sep = last_sep(exe_path);
    if (sep == NULL) {
        /* exe invoked in current dir (no path) — e.g. "gcl.exe" */
        snprintf(full, sizeof full, "%s", rel_path);
    } else {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        if (dir_len + strlen(rel_path) + 1 >= sizeof full) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "gcdl: exe path too long");
            return NULL;
        }
        memcpy(full, exe_path, dir_len);
        memcpy(full + dir_len, rel_path, strlen(rel_path) + 1);
    }

    return gcdl_load(full, err, err_cap);
}

GclGcDl *gcdl_load(const char *path, char *err, size_t err_cap) {
    GclGcDl *mod;
    if (path == NULL || path[0] == '\0') {
        set_err(err, err_cap, "gcdl: empty file path", NULL);
        return NULL;
    }
    mod = (GclGcDl *)calloc(1, sizeof(GclGcDl));
    if (mod == NULL) {
        set_err(err, err_cap, "gcdl: out of memory", NULL);
        return NULL;
    }

#ifdef _WIN32
    {
        /* The .gcDL's dependent DLLs may live in the module's own directory
         * or in a runtime subfolder. Python's runtime lives in
         * <module-dir>\pyLibrary\ (Library/Python/pyLibrary/). Probe that
         * subfolder first, fall back to the module directory; restore the
         * previous DLL search path after the load. */
        char prev_dir[4096] = "";
        DWORD prev_len = 0;
        const char *sep = last_sep(path);
        char module_dir[4096];

        prev_len = GetDllDirectoryA((DWORD)sizeof prev_dir, prev_dir);
        if (sep != NULL) {
            size_t dlen = (size_t)(sep - path);
            static const char pyembed[] = "\\pyLibrary";
            if (dlen < sizeof module_dir) {
                char probe[4096];
                DWORD attrs;
                int use_probe = 0;

                memcpy(module_dir, path, dlen);
                module_dir[dlen] = '\0';

                /* Prefer <module-dir>\pyLibrary when present.  Only build the
                 * probe when the suffix actually fits, so the buffer can
                 * never overflow (and -Wformat-truncation stays quiet). */
                if (dlen + sizeof pyembed <= sizeof probe) {
                    memcpy(probe, module_dir, dlen);
                    memcpy(probe + dlen, pyembed, sizeof pyembed);
                    attrs = GetFileAttributesA(probe);
                    if (attrs != INVALID_FILE_ATTRIBUTES &&
                        (attrs & FILE_ATTRIBUTE_DIRECTORY))
                        use_probe = 1;
                }

                if (use_probe)
                    SetDllDirectoryA(probe);
                else
                    SetDllDirectoryA(module_dir);
            }
        }

        mod->handle = (void *)LoadLibraryA(path);

        if (prev_len > 0 && prev_len < (DWORD)sizeof prev_dir)
            SetDllDirectoryA(prev_dir);
        else
            SetDllDirectoryA(NULL);

        if (mod->handle == NULL) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "gcdl: failed to load (code %lu): %s",
                         (unsigned long)GetLastError(), path);
            free(mod);
            return NULL;
        }
    }
#else
    mod->handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (mod->handle == NULL) {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "gcdl: failed to load: %s (%s)",
                     path, dlerror() ? dlerror() : "unknown error");
        free(mod);
        return NULL;
    }
#endif

    return mod;
}

void *gcdl_get_proc(GclGcDl *mod, const char *name) {
    if (mod == NULL || mod->handle == NULL || name == NULL)
        return NULL;
#ifdef _WIN32
    return (void *)GetProcAddress((HMODULE)mod->handle, name);
#else
    return dlsym(mod->handle, name);
#endif
}

void gcdl_unload(GclGcDl *mod) {
    if (mod == NULL)
        return;
    if (mod->handle != NULL) {
#ifdef _WIN32
        FreeLibrary((HMODULE)mod->handle);
#else
        dlclose(mod->handle);
#endif
        mod->handle = NULL;
    }
    free(mod);
}
