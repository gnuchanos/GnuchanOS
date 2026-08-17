/*
 * gcl_python_embed.c — Python embed module (.gcDL output) — PLATFORM-INDEPENDENT
 *
 *  Windows : the core Python runtime is carried inside
 *            Library/Python/pyLibrary/
 *            (python314.dll + *.pyd + python314.zip + Lib/).
 *            The .gcDL files (python.gcDL, python_raylib.gcDL) and the
 *            pyRaylib.py wrapper stay in Library/Python/.
 *            python.gcDL is linked into that folder. For -pyrun the cwd is
 *            NOT changed and PyConfig (3.8+ stable C API) explicitly adds
 *            Python/ (root), pyLibrary/, python314.zip and
 *            Lib/site-packages to module_search_paths.
 *            (Py_SetPath was REMOVED from the 3.13+ C API.)
 *  Linux   : a portable prebuilt runtime — no dependency on the system Python.
 *            The makefile installs the CPython tarball into _temp/python_linux
 *            and copies the runtime into Library/Python/pyLibrary/:
 *              pyLibrary/libpython3.14.so + pyLibrary/python3.14/<stdlib>
 *            python.gcDL / python_raylib.gcDL / pyRaylib.py stay one level up
 *            in Library/Python/; python.gcDL is rpaths to $ORIGIN/pyLibrary.
 *            module_search_paths = Python (root), pyLibrary/python3.14,
 *            .../site-packages, .../lib-dynload.
 *
 *  Call:  gcl -pyrun script.py
 *
 *  Build (Windows, done by makefile.py):
 *    gcc -shared gcl_python_embed.c -I<python_win>/include -L<python_win>/libs \
 *        -lpython314 -o Library/Python/python.gcDL
 *  Build (Linux, done by makefile.py):
 *    gcc -shared -fPIC gcl_python_embed.c -I<prefix>/include/python3.14 \
 *        -L<prefix>/lib -lpython3.14 -o Library/Python/python.gcDL
 */

#define _CRT_SECURE_NO_WARNINGS

/* Needed for glibc to expose readlink under strict -std=c11. */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>       /* wchar_t + sizeof wchar_t */

#if defined(_WIN32)
#define GCL_MODULE_EXPORT __declspec(dllexport)
#else
#define GCL_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#include <Python.h>

/* Linux: embedded stdlib subdirectory name INSIDE pyLibrary (pyLibrary/python3.14). */
#if !defined(GCL_PY_RUNTIME_DIR)
#define GCL_PY_RUNTIME_DIR "python3.14"
#endif

/* Shared runtime layout on both platforms: Library/Python/<GCL_PY_EMBED_DIR>.
 * Windows: python314.dll + *.pyd + python314.zip + Lib/
 * Linux  : libpython3.14.so + python3.14/ stdlib + lib-dynload
 * The .gcDL files and pyRaylib.py stay in Library/Python/. */
#if !defined(GCL_PY_EMBED_DIR)
#define GCL_PY_EMBED_DIR "pyLibrary"
#endif

#ifdef _WIN32
#include <windows.h>
#include <direct.h>      /* _wchdir() */
#else
#include <dlfcn.h>
#include <unistd.h>      /* readlink() */
#endif

/* python_raylib.gcDL export: returns the new gcl_raylib module */
typedef PyObject *(*python_raylib_attach_fn)(void);

/* Full path of the Library/Python/ directory next to the exe.
 * return: writes into the buffer, NULL on error. */
static const char *python_module_dir(char *out, size_t cap) {
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
        static const char pyrel[] = "Library/Python";
        if (dir_len + sizeof pyrel >= cap)
            return NULL;
        memcpy(out, exe_path, dir_len);
        memcpy(out + dir_len, pyrel, sizeof pyrel);
    }

    return out;
}

/* ---- cross-language bridge (gcl_bridge) ----
 * bridge.gcDL (Library/bridge/bridge.gcDL next to the exe) provides a
 * filesystem key->value store shared between Python and Lua processes.
 * We load it here and expose it as the "gcl_bridge" module:
 *   gcl_bridge.open(session) / set(key, value) / get(key) / delete(key) / list()
 */

typedef int (*bridge_open_fn)(const char *);
typedef int (*bridge_set_fn)(const char *, const char *);
typedef int (*bridge_get_fn)(const char *, char *, size_t);
typedef int (*bridge_delete_fn)(const char *);
typedef int (*bridge_list_fn)(char *, size_t);

static bridge_open_fn   g_bridge_open = NULL;
static bridge_set_fn    g_bridge_set = NULL;
static bridge_get_fn    g_bridge_get = NULL;
static bridge_delete_fn g_bridge_delete = NULL;
static bridge_list_fn   g_bridge_list = NULL;

/* Loads bridge.gcDL next to the exe. Returns NULL on failure. */
static void *bridge_load(void) {
    char exe_path[4096];
    char full[4200];
    const char *sep = NULL;
    void *handle = NULL;

#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
        if (n == 0 || n >= (DWORD)sizeof exe_path) return NULL;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (n <= 0 || n >= (ssize_t)sizeof exe_path) return NULL;
        exe_path[n] = '\0';
    }
#endif

    for (char *p = exe_path; *p; p++) {
        if (*p == '\\' || *p == '/') sep = p;
    }
    if (sep == NULL) return NULL;
    {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        static const char brel[] = "Library/bridge/bridge.gcDL";
        if (dir_len + sizeof brel >= sizeof full) return NULL;
        memcpy(full, exe_path, dir_len);
        memcpy(full + dir_len, brel, sizeof brel);
    }

#ifdef _WIN32
    handle = (void *)LoadLibraryA(full);
#else
    handle = dlopen(full, RTLD_NOW | RTLD_GLOBAL);
#endif
    return handle;
}

static int bridge_setup(void) {
    void *handle;
    if (g_bridge_open != NULL) return 0; /* once yuklendi */
    handle = bridge_load();
    if (handle == NULL) return -1;
#ifdef _WIN32
    g_bridge_open = (bridge_open_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_open");
    g_bridge_set = (bridge_set_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_set");
    g_bridge_get = (bridge_get_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_get");
    g_bridge_delete = (bridge_delete_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_delete");
    g_bridge_list = (bridge_list_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_list");
#else
    g_bridge_open = (bridge_open_fn)dlsym(handle, "gcdl_bridge_open");
    g_bridge_set = (bridge_set_fn)dlsym(handle, "gcdl_bridge_set");
    g_bridge_get = (bridge_get_fn)dlsym(handle, "gcdl_bridge_get");
    g_bridge_delete = (bridge_delete_fn)dlsym(handle, "gcdl_bridge_delete");
    g_bridge_list = (bridge_list_fn)dlsym(handle, "gcdl_bridge_list");
#endif
    if (g_bridge_open == NULL || g_bridge_set == NULL || g_bridge_get == NULL ||
        g_bridge_delete == NULL || g_bridge_list == NULL)
        return -1;
    return 0;
}

static PyObject *py_bridge_open(PyObject *self, PyObject *args) {
    const char *session = NULL;
    if (!PyArg_ParseTuple(args, "|z:open", &session)) return NULL;
    if (bridge_setup() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "gcl_bridge: bridge.gcDL not loaded");
        return NULL;
    }
    return PyLong_FromLong(g_bridge_open(session));
}

static PyObject *py_bridge_set(PyObject *self, PyObject *args) {
    const char *key, *value;
    if (!PyArg_ParseTuple(args, "ss:set", &key, &value)) return NULL;
    if (bridge_setup() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "gcl_bridge: bridge.gcDL not loaded");
        return NULL;
    }
    return PyLong_FromLong(g_bridge_set(key, value));
}

static PyObject *py_bridge_get(PyObject *self, PyObject *args) {
    const char *key;
    char *buf;
    int rc;
    if (!PyArg_ParseTuple(args, "s:get", &key)) return NULL;
    if (bridge_setup() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "gcl_bridge: bridge.gcDL not loaded");
        return NULL;
    }
    buf = (char *)malloc(1024 * 1024 + 1);
    if (buf == NULL) return PyErr_NoMemory();
    rc = g_bridge_get(key, buf, 1024 * 1024 + 1);
    if (rc != 0) {
        free(buf);
        Py_RETURN_NONE;
    }
    {
        PyObject *ret = PyUnicode_FromString(buf);
        free(buf);
        return ret;
    }
}

static PyObject *py_bridge_delete(PyObject *self, PyObject *args) {
    const char *key;
    if (!PyArg_ParseTuple(args, "s:delete", &key)) return NULL;
    if (bridge_setup() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "gcl_bridge: bridge.gcDL not loaded");
        return NULL;
    }
    return PyLong_FromLong(g_bridge_delete(key));
}

static PyObject *py_bridge_list(PyObject *self, PyObject *args) {
    char buf[4096];
    if (bridge_setup() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "gcl_bridge: bridge.gcDL not loaded");
        return NULL;
    }
    g_bridge_list(buf, sizeof buf);
    return PyUnicode_FromString(buf);
}

static PyMethodDef gcl_bridge_methods[] = {
    {"open",   py_bridge_open,   METH_VARARGS, "Open a bridge session."},
    {"set",    py_bridge_set,    METH_VARARGS, "Set a value (key, value)."},
    {"get",    py_bridge_get,    METH_VARARGS, "Get a value or None."},
    {"delete", py_bridge_delete, METH_VARARGS, "Delete a value."},
    {"list",   py_bridge_list,   METH_NOARGS,  "List keys (comma separated)."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef gcl_bridge_module = {
    PyModuleDef_HEAD_INIT,
    "gcl_bridge",
    "GCL cross-language data bridge (Lua <-> Python).",
    -1,
    gcl_bridge_methods,
    NULL, NULL, NULL, NULL
};

/* bridge'i yukler ve sys.modules["gcl_bridge"] kurar. */
static void gcl_bridge_install(void) {
    PyObject *mod;
    if (bridge_setup() != 0) return;
    mod = PyModule_Create(&gcl_bridge_module);
    if (mod != NULL) {
        PyObject *sys = PyImport_ImportModule("sys");
        if (sys != NULL) {
            PyObject *mods = PyObject_GetAttrString(sys, "modules");
            if (mods != NULL) {
                PyDict_SetItemString(mods, "gcl_bridge", mod);
                Py_DECREF(mods);
            }
            Py_DECREF(sys);
        }
        Py_DECREF(mod);
    }
}

/* Embedded Python version (gcl -pyrun -version). */
GCL_MODULE_EXPORT const char *gcdl_python_version(void) {
    return PY_VERSION;
}

/* Runs a Python script. */
GCL_MODULE_EXPORT int gcdl_python_run(const char *script, char *err, size_t err_cap) {
    char mod_dir[4200];
    FILE *fp;
    long fsize = 0;
    char *src = NULL;
    int rc = 0;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (script == NULL || script[0] == '\0') {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "script path is empty");
        return 1;
    }

    fp = fopen(script, "rb");
    if (fp == NULL) {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "cannot open script: %s", script);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        if (err && err_cap > 0)
            snprintf(err, err_cap, "cannot get script size");
        return 1;
    }
    fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        if (err && err_cap > 0)
            snprintf(err, err_cap, "cannot get script size");
        return 1;
    }
    rewind(fp);
    src = (char *)malloc((size_t)fsize + 1);
    if (src == NULL) {
        fclose(fp);
        if (err && err_cap > 0)
            snprintf(err, err_cap, "out of memory");
        return 1;
    }
    if (fsize > 0 && fread(src, 1, (size_t)fsize, fp) != (size_t)fsize) {
        fclose(fp);
        free(src);
        if (err && err_cap > 0)
            snprintf(err, err_cap, "cannot read script: %s", script);
        return 1;
    }
    fclose(fp);
    src[fsize] = '\0';

#ifdef _WIN32
    /* Windows: core runtime inside Library/Python/pyLibrary/.
     * module_search_paths = Python (root), pyLibrary, pyLibrary\python314.zip,
     *                       pyLibrary\Lib\site-packages */
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyStatus st;
        PyConfig cfg;

        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            wchar_t *wdir = Py_DecodeLocale(dir, NULL);
            if (wdir != NULL) {
                static const wchar_t wembedrel[] = L"\\" GCL_PY_EMBED_DIR;
                size_t wlen = wcslen(wdir);
                size_t relen = sizeof(wembedrel) / sizeof(wchar_t) - 1;
                wchar_t *wembed = (wchar_t *)PyMem_RawMalloc(
                    (wlen + relen + 1) * sizeof(wchar_t));

                /* <Python> : pyRaylib.py + any pure-python helper here */
                PyWideStringList_Append(&cfg.module_search_paths, wdir);

                if (wembed != NULL) {
                    wcscpy(wembed, wdir);
                    wcscat(wembed, wembedrel);

                    {
                        char embed_ansi[4200];
                        snprintf(embed_ansi, sizeof embed_ansi, "%s\\%s",
                                 dir, GCL_PY_EMBED_DIR);
                        cfg.home = Py_DecodeLocale(embed_ansi, NULL);
                    }

                    /* <pyLibrary> : C extension modules (*.pyd) + zip live here */
                    PyWideStringList_Append(&cfg.module_search_paths, wembed);

                    /* <pyLibrary>\python314.zip : stdlib archive */
                    {
                        static const wchar_t wzip[] = L"\\python314.zip";
                        size_t zlen = sizeof(wzip) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wzip);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }

                    /* <pyLibrary>\Lib : stdlib (install_only paketinde Lib\ icinde) */
                    {
                        static const wchar_t wlib[] = L"\\Lib";
                        size_t zlen2 = sizeof(wlib) / sizeof(wchar_t) - 1;
                        wchar_t *wpath2 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen2 + 1) * sizeof(wchar_t));
                        if (wpath2 != NULL) {
                            wcscpy(wpath2, wembed);
                            wcscat(wpath2, wlib);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath2);
                        }
                    }
                    /* <pyLibrary>\DLLs : C uzanti modulleri (install_only) */
                    {
                        static const wchar_t wdll[] = L"\\DLLs";
                        size_t zlen3 = sizeof(wdll) / sizeof(wchar_t) - 1;
                        wchar_t *wpath3 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen3 + 1) * sizeof(wchar_t));
                        if (wpath3 != NULL) {
                            wcscpy(wpath3, wembed);
                            wcscat(wpath3, wdll);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath3);
                        }
                    }

                    /* <pyLibrary>\Lib\site-packages : packages installed via gcl -m pip */
                    {
                        static const wchar_t wsp[] = L"\\Lib\\site-packages";
                        size_t zlen = sizeof(wsp) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wsp);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
        if (PyStatus_IsError(st)) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot initialize Python: %s",
                         st.err_msg != NULL ? st.err_msg
                                            : "unknown error (is Library/Python/pyLibrary missing?)");
            free(src);
            return 1;
        }
    }
#else
    /* Linux: EMBEDDED portable runtime (Library/Python/pyLibrary/).
     * module_search_paths = Python (root), pyLibrary/python3.14,
     *                       pyLibrary/python3.14/site-packages,
     *                       pyLibrary/python3.14/lib-dynload */
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyStatus st;
        PyConfig cfg;

        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            /* <Python> : pyRaylib.py + any pure-python helper here */
            {
                wchar_t *wdir = Py_DecodeLocale(dir, NULL);
                if (wdir != NULL)
                    PyWideStringList_Append(&cfg.module_search_paths, wdir);
            }

            /* home = <dir>/pyLibrary : real prefix for sys.prefix */
            {
                static const char embedrel[] = "/" GCL_PY_EMBED_DIR;
                size_t dlen = strlen(dir);
                wchar_t *whome = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof embedrel) * sizeof(wchar_t));
                if (whome != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(embedrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(whome, wd);
                        wcscat(whome, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    cfg.home = whome;
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR  (stdlib: pyLibrary/python3.14) */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR;
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR/site-packages */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/site-packages";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR/lib-dynload  (C extensions) */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/lib-dynload";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
        if (PyStatus_IsError(st)) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot initialize Python: %s",
                         st.err_msg != NULL ? st.err_msg
                                            : "unknown error (is the Library/Python/pyLibrary embedded runtime missing?)");
            free(src);
            return 1;
        }
    }
#endif

    /* Load python_raylib.gcDL from Library/Python/ next to the exe:
     * if present install it as sys.modules["gcl_raylib"]; if missing skip
     * silently (raylib is optional). */
    {
        char ray_full[4200];
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        if (dir != NULL) {
            size_t dlen = strlen(dir);
            static const char rlrel[] = "/python_raylib.gcDL";
            void *handle = NULL;
            python_raylib_attach_fn attach = NULL;

            if (dlen + sizeof rlrel < sizeof ray_full) {
                memcpy(ray_full, dir, dlen);
                memcpy(ray_full + dlen, rlrel, sizeof rlrel);
            } else {
                ray_full[0] = '\0';
            }

#ifdef _WIN32
            handle = (void *)LoadLibraryA(ray_full);
#else
            handle = dlopen(ray_full, RTLD_NOW | RTLD_GLOBAL);
#endif
            if (handle != NULL) {
#ifdef _WIN32
                attach = (python_raylib_attach_fn)GetProcAddress(
                    (HMODULE)handle, "python_raylib_attach");
#else
                attach = (python_raylib_attach_fn)dlsym(handle, "python_raylib_attach");
#endif
                if (attach != NULL) {
                    PyObject *mod = attach();
                    if (mod != NULL) {
                        PyObject *sys = PyImport_ImportModule("sys");
                        if (sys != NULL) {
                            PyObject *mods = PyObject_GetAttrString(sys, "modules");
                            if (mods != NULL) {
                                PyDict_SetItemString(mods, "gcl_raylib", mod);
                                Py_DECREF(mods);
                            }
                            Py_DECREF(sys);
                        }
                        Py_DECREF(mod);
                    }
                }
            }
            /* handle intentionally kept open: valid for the Python state's lifetime */
        }
    }

    /* Install gcl_bridge into sys.modules (used by scripts). */
    gcl_bridge_install();

    /* Script dizinini sys.path'a ekle: `import <src-modul>` calissin.
     * Normal Python, script'in dizinini otomatik sys.path[0] yapar;
     * embed (PyRun_StringFlags) bunu yapmaz. Ayrac yoksa cwd (".") eklenir.
     *
     * EXTRA: script dizininin PARENT'i da sys.path'a eklenir. Boylece
     * src/ icinde klasor paketleri (src/pyFiles/helpers.py) `import
     * pyFiles.helpers` ile cozumlenir — kullanici src/ icinde pyFiles
     * klasoru actiginda python embeded importlari gorur. */
    {
        const char *slash = strrchr(script, '/');
        const char *back = strrchr(script, '\\');
        const char *last = slash == NULL ? back
                          : (back == NULL ? slash : (slash > back ? slash : back));
        char dir_buf[4200];
        char parent_buf[4200];
        const char *dir = NULL;
        const char *parent = NULL;
        if (last != NULL) {
            size_t dlen = (size_t)(last - script);
            if (dlen >= sizeof dir_buf) dlen = sizeof dir_buf - 1;
            memcpy(dir_buf, script, dlen);
            dir_buf[dlen] = '\0';
            dir = dir_buf;

            /* parent: dir icindeki son ayiriciya kadar (src/pyFiles -> src) */
            {
                const char *s2 = strrchr(dir, '/');
                const char *b2 = strrchr(dir, '\\');
                const char *l2 = s2 == NULL ? b2
                                : (b2 == NULL ? s2 : (s2 > b2 ? s2 : b2));
                if (l2 != NULL) {
                    size_t plen = (size_t)(l2 - dir);
                    if (plen == 0) {
                        parent = "/";  /* kok dizin */
                    } else {
                        if (plen >= sizeof parent_buf) plen = sizeof parent_buf - 1;
                        memcpy(parent_buf, dir, plen);
                        parent_buf[plen] = '\0';
                        parent = parent_buf;
                    }
                }
            }
        } else {
            dir = ".";
        }
        if (dir != NULL) {
            PyObject *sys = PyImport_ImportModule("sys");
            if (sys != NULL) {
                PyObject *path = PyObject_GetAttrString(sys, "path");
                if (path != NULL) {
                    /* once parent (klasor paketlerinin tabani), sonra dir */
                    if (parent != NULL) {
                        PyObject *par_obj = PyUnicode_FromString(parent);
                        if (par_obj != NULL) {
                            if (PyList_Insert(path, 0, par_obj) != 0)
                                PyErr_Clear();
                            Py_DECREF(par_obj);
                        }
                    }
                    {
                        PyObject *dir_obj = PyUnicode_FromString(dir);
                        if (dir_obj != NULL) {
                            if (PyList_Insert(path, 0, dir_obj) != 0)
                                PyErr_Clear(); /* path'e eklenemese de devam */
                            Py_DECREF(dir_obj);
                        }
                    }
                    Py_DECREF(path);
                }
                Py_DECREF(sys);
            }
        }
    }

    {
        PyObject *main = PyImport_AddModule("__main__");
        PyObject *g = PyModule_GetDict(main);
        PyObject *builtins = PyImport_ImportModule("builtins");
        PyObject *ret = NULL;
        if (g != NULL)
            PyDict_SetItemString(g, "__builtins__", builtins);
        Py_XDECREF(builtins);

        ret = PyRun_StringFlags(src, Py_file_input, g, g, NULL);
        if (ret == NULL) {
            if (PyErr_Occurred())
                PyErr_Print();
            if (err && err_cap > 0)
                snprintf(err, err_cap, "Python error: %s", script);
            rc = 1;
        }
        Py_XDECREF(ret);
    }

    Py_Finalize();
    free(src);
    return rc;
}

/* ---- full-system introspection (gcl -pyrun -resolve) ----
 * Used by gcl-lsp for the "fridge" model: static scanning of .py files
 * cannot see the Windows embedded stdlib (python314.zip) or C-extension
 * (.pyd) modules. So gcl-lsp asks the real embedded Python:
 *
 *   spec = "<module>|<prefix>"    e.g. "os|p", "os.path|joi", "builtins|pri"
 *
 * The module is imported with the same PyConfig module_search_paths as
 * -pyrun (pyLibrary, python314.zip, Lib/site-packages), so imports follow
 * Python's own rules — including C extensions. One NDJSON completion item
 * is printed per matching attribute (label/kind/detail) to stdout; the
 * error buffer carries a message on failure.
 */

static int valid_modpath(const char *s) {
    if (s == NULL || *s == '\0') return 0;
    if (!(*s == '_' || (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')))
        return 0;
    s++;
    for (; *s; s++) {
        if (!(*s == '_' || *s == '.' ||
              (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
              (*s >= '0' && *s <= '9')))
            return 0;
    }
    return 1;
}

static int valid_prefix(const char *s) {
    if (s == NULL) return 0;
    for (; *s; s++) {
        if (!(*s == '_' ||
              (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
              (*s >= '0' && *s <= '9')))
            return 0;
    }
    return 1;
}

/* Python snippet run with PyRun_SimpleString: imports the module, filters
 * attributes by prefix (underscore-private hidden), classifies each as
 * fn/class/module/const and prints one NDJSON item per attribute. */
static const char gcl_resolve_prog[] =
    "import importlib, inspect, json, sys\n"
    "modname = \"%s\"\n"
    "prefix = \"%s\"\n"
    "try:\n"
    "    m = importlib.import_module(modname)\n"
    "except BaseException as e:\n"
    "    print(json.dumps({'label': modname, 'kind': 'module', "
    "'detail': 'import error: ' + str(e)}))\n"
    "    sys.stdout.flush()\n"
    "    raise SystemExit(code=0)\n"
    "names = [n for n in dir(m) if not n.startswith('_')]\n"
    "if prefix:\n"
    "    names = [n for n in names if n.startswith(prefix)]\n"
    "for n in names:\n"
    "    try:\n"
    "        o = getattr(m, n)\n"
    "    except BaseException:\n"
    "        continue\n"
    "    try:\n"
    "        if inspect.ismodule(o):\n"
    "            k = 'module'; d = 'module ' + n\n"
    "        elif inspect.isclass(o):\n"
    "            k = 'class'; d = 'class ' + n\n"
    "        elif callable(o):\n"
    "            k = 'fn'\n"
    "            try:\n"
    "                s = str(inspect.signature(o))\n"
    "                d = n + s\n"
    "            except BaseException:\n"
    "                d = n + '()'\n"
    "        else:\n"
    "            k = 'const'\n"
    "            try:\n"
    "                r = repr(o)\n"
    "                if len(r) > 60:\n"
    "                    r = r[:60] + '...'\n"
    "            except BaseException:\n"
    "                r = '<?>'\n"
    "            d = n + ' = ' + r\n"
    "    except BaseException:\n"
    "        continue\n"
    "    print(json.dumps({'label': n, 'kind': k, 'detail': d}))\n"
    "sys.stdout.flush()\n";

GCL_MODULE_EXPORT int gcdl_python_resolve(const char *spec, char *out,
                                          size_t out_cap) {
    char modname[520];
    char prefix[520];
    const char *bar;
    char mod_dir[4200];
    char program[16384];
    PyStatus st;
    PyConfig cfg;
    int rc;

    if (out != NULL && out_cap > 0)
        out[0] = '\0';

    if (spec == NULL || spec[0] == '\0') {
        if (out && out_cap > 0)
            snprintf(out, out_cap, "resolve spec is empty (<module>|<prefix>)");
        return 1;
    }

    bar = strchr(spec, '|');
    if (bar == NULL || bar == spec) {
        if (out && out_cap > 0)
            snprintf(out, out_cap,
                     "resolve spec must be '<module>|<prefix>', got: %s", spec);
        return 1;
    }
    {
        size_t mlen = (size_t)(bar - spec);
        if (mlen >= sizeof modname) mlen = sizeof modname - 1;
        memcpy(modname, spec, mlen);
        modname[mlen] = '\0';
    }
    {
        size_t plen = strlen(bar + 1);
        if (plen >= sizeof prefix) plen = sizeof prefix - 1;
        memcpy(prefix, bar + 1, plen);
        prefix[plen] = '\0';
    }
    if (!valid_modpath(modname) || !valid_prefix(prefix)) {
        if (out && out_cap > 0)
            snprintf(out, out_cap, "invalid resolve spec: %s", spec);
        return 1;
    }

#ifdef _WIN32
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            wchar_t *wdir = Py_DecodeLocale(dir, NULL);
            if (wdir != NULL) {
                static const wchar_t wembedrel[] = L"\\" GCL_PY_EMBED_DIR;
                size_t wlen = wcslen(wdir);
                size_t relen = sizeof(wembedrel) / sizeof(wchar_t) - 1;
                wchar_t *wembed = (wchar_t *)PyMem_RawMalloc(
                    (wlen + relen + 1) * sizeof(wchar_t));

                /* <Python> : pyRaylib.py + any pure-python helper here */
                PyWideStringList_Append(&cfg.module_search_paths, wdir);

                if (wembed != NULL) {
                    wcscpy(wembed, wdir);
                    wcscat(wembed, wembedrel);

                    {
                        char embed_ansi[4200];
                        snprintf(embed_ansi, sizeof embed_ansi, "%s\\%s",
                                 dir, GCL_PY_EMBED_DIR);
                        cfg.home = Py_DecodeLocale(embed_ansi, NULL);
                    }

                    /* <pyLibrary> : runtime (python314.dll + *.pyd + zip) */
                    PyWideStringList_Append(&cfg.module_search_paths, wembed);

                    /* <pyLibrary>\python314.zip : stdlib archive */
                    {
                        static const wchar_t wzip[] = L"\\python314.zip";
                        size_t zlen = sizeof(wzip) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wzip);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }

                    /* <pyLibrary>\Lib : stdlib (install_only paketinde Lib\ icinde) */
                    {
                        static const wchar_t wlib[] = L"\\Lib";
                        size_t zlen2 = sizeof(wlib) / sizeof(wchar_t) - 1;
                        wchar_t *wpath2 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen2 + 1) * sizeof(wchar_t));
                        if (wpath2 != NULL) {
                            wcscpy(wpath2, wembed);
                            wcscat(wpath2, wlib);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath2);
                        }
                    }
                    /* <pyLibrary>\DLLs : C uzanti modulleri (install_only) */
                    {
                        static const wchar_t wdll[] = L"\\DLLs";
                        size_t zlen3 = sizeof(wdll) / sizeof(wchar_t) - 1;
                        wchar_t *wpath3 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen3 + 1) * sizeof(wchar_t));
                        if (wpath3 != NULL) {
                            wcscpy(wpath3, wembed);
                            wcscat(wpath3, wdll);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath3);
                        }
                    }

                    /* <pyLibrary>\Lib\site-packages : pip packages */
                    {
                        static const wchar_t wsp[] = L"\\Lib\\site-packages";
                        size_t zlen = sizeof(wsp) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wsp);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
    }
#else
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            /* <Python> : pyRaylib.py + any pure-python helper here */
            {
                wchar_t *wdir = Py_DecodeLocale(dir, NULL);
                if (wdir != NULL)
                    PyWideStringList_Append(&cfg.module_search_paths, wdir);
            }

            /* home = <dir>/pyLibrary : real prefix for sys.prefix */
            {
                static const char embedrel[] = "/" GCL_PY_EMBED_DIR;
                size_t dlen = strlen(dir);
                wchar_t *whome = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof embedrel) * sizeof(wchar_t));
                if (whome != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(embedrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(whome, wd);
                        wcscat(whome, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    cfg.home = whome;
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR  (stdlib) */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR;
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR/site-packages */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/site-packages";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }

            /* <dir>/pyLibrary/GCL_PY_RUNTIME_DIR/lib-dynload  (C extensions) */
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/lib-dynload";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
    }
#endif

    if (PyStatus_IsError(st)) {
        if (out && out_cap > 0)
            snprintf(out, out_cap, "cannot initialize Python: %s",
                     st.err_msg != NULL ? st.err_msg
                                        : "unknown error (pyLibrary missing?)");
        return 1;
    }

    snprintf(program, sizeof program, gcl_resolve_prog, modname, prefix);
    rc = PyRun_SimpleString(program);
    Py_Finalize();
    if (rc != 0) {
        if (out && out_cap > 0)
            snprintf(out, out_cap, "python resolve error: %s", spec);
        return 1;
    }
    return 0;
}

/* Runs a Python module:  gcl -m <module> [args...] */
GCL_MODULE_EXPORT int gcdl_python_module(const char *modname, int mod_argc,
                                         const char **mod_argv, char *err,
                                         size_t err_cap) {
    char mod_dir[4200];
    int rc = 0;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (modname == NULL || modname[0] == '\0') {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "module name is empty (gcl -m <module>)");
        return 1;
    }

#ifdef _WIN32
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyStatus st;
        PyConfig cfg;

        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            wchar_t *wdir = Py_DecodeLocale(dir, NULL);
            if (wdir != NULL) {
                static const wchar_t wembedrel[] = L"\\" GCL_PY_EMBED_DIR;
                size_t wlen = wcslen(wdir);
                size_t relen = sizeof(wembedrel) / sizeof(wchar_t) - 1;
                wchar_t *wembed = (wchar_t *)PyMem_RawMalloc(
                    (wlen + relen + 1) * sizeof(wchar_t));

                /* <Python> : pyRaylib.py + any pure-python helper here */
                PyWideStringList_Append(&cfg.module_search_paths, wdir);

                if (wembed != NULL) {
                    wcscpy(wembed, wdir);
                    wcscat(wembed, wembedrel);

                    {
                        char embed_ansi[4200];
                        snprintf(embed_ansi, sizeof embed_ansi, "%s\\%s",
                                 dir, GCL_PY_EMBED_DIR);
                        cfg.home = Py_DecodeLocale(embed_ansi, NULL);
                    }

                    /* <pyLibrary> : runtime (python314.dll + *.pyd + python314.zip) */
                    PyWideStringList_Append(&cfg.module_search_paths, wembed);

                    {
                        static const wchar_t wzip[] = L"\\python314.zip";
                        size_t zlen = sizeof(wzip) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wzip);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }
                    /* <pyLibrary>\Lib : stdlib (install_only paketinde Lib\ icinde) */
                    {
                        static const wchar_t wlib[] = L"\\Lib";
                        size_t zlen2 = sizeof(wlib) / sizeof(wchar_t) - 1;
                        wchar_t *wpath2 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen2 + 1) * sizeof(wchar_t));
                        if (wpath2 != NULL) {
                            wcscpy(wpath2, wembed);
                            wcscat(wpath2, wlib);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath2);
                        }
                    }
                    /* <pyLibrary>\DLLs : C uzanti modulleri (install_only) */
                    {
                        static const wchar_t wdll[] = L"\\DLLs";
                        size_t zlen3 = sizeof(wdll) / sizeof(wchar_t) - 1;
                        wchar_t *wpath3 = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen3 + 1) * sizeof(wchar_t));
                        if (wpath3 != NULL) {
                            wcscpy(wpath3, wembed);
                            wcscat(wpath3, wdll);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath3);
                        }
                    }
                    {
                        static const wchar_t wsp[] = L"\\Lib\\site-packages";
                        size_t zlen = sizeof(wsp) / sizeof(wchar_t) - 1;
                        wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                            (wlen + relen + zlen + 1) * sizeof(wchar_t));
                        if (wpath != NULL) {
                            wcscpy(wpath, wembed);
                            wcscat(wpath, wsp);
                            PyWideStringList_Append(&cfg.module_search_paths, wpath);
                        }
                    }
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
        if (PyStatus_IsError(st)) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot initialize Python: %s",
                         st.err_msg != NULL ? st.err_msg
                                            : "unknown error (is Library/Python/pyLibrary missing?)");
            return 1;
        }
    }
#else
    {
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        PyStatus st;
        PyConfig cfg;

        PyConfig_InitPythonConfig(&cfg);
        cfg.program_name = Py_DecodeLocale("gcl", NULL);
        cfg.isolated = 1;
        cfg.module_search_paths_set = 1;

        if (dir != NULL) {
            /* <Python> : pyRaylib.py + any pure-python helper here */
            {
                wchar_t *wdir = Py_DecodeLocale(dir, NULL);
                if (wdir != NULL)
                    PyWideStringList_Append(&cfg.module_search_paths, wdir);
            }

            /* home = <dir>/pyLibrary */
            {
                static const char embedrel[] = "/" GCL_PY_EMBED_DIR;
                size_t dlen = strlen(dir);
                wchar_t *whome = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof embedrel) * sizeof(wchar_t));
                if (whome != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(embedrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(whome, wd);
                        wcscat(whome, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    cfg.home = whome;
                }
            }
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR;
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/site-packages";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }
            {
                static const char pyrel[] = "/" GCL_PY_EMBED_DIR "/" GCL_PY_RUNTIME_DIR "/lib-dynload";
                size_t dlen = strlen(dir);
                wchar_t *wpath = (wchar_t *)PyMem_RawMalloc(
                    (dlen + sizeof pyrel) * sizeof(wchar_t));
                if (wpath != NULL) {
                    wchar_t *wd = Py_DecodeLocale(dir, NULL);
                    wchar_t *wr = Py_DecodeLocale(pyrel, NULL);
                    if (wd != NULL && wr != NULL) {
                        wcscpy(wpath, wd);
                        wcscat(wpath, wr);
                    }
                    PyMem_RawFree(wd);
                    PyMem_RawFree(wr);
                    PyWideStringList_Append(&cfg.module_search_paths, wpath);
                }
            }
        }

        st = Py_InitializeFromConfig(&cfg);
        PyConfig_Clear(&cfg);
        if (PyStatus_IsError(st)) {
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot initialize Python: %s",
                         st.err_msg != NULL ? st.err_msg
                                            : "unknown error (is the Library/Python/pyLibrary embedded runtime missing?)");
            return 1;
        }
    }
#endif

    /* Also install gcl_raylib (usable in scripts after gcl -m pip) */
    {
        char ray_full[4200];
        const char *dir = python_module_dir(mod_dir, sizeof mod_dir);
        if (dir != NULL) {
            size_t dlen = strlen(dir);
            static const char rlrel[] = "/python_raylib.gcDL";
            void *handle = NULL;
            python_raylib_attach_fn attach = NULL;

            if (dlen + sizeof rlrel < sizeof ray_full) {
                memcpy(ray_full, dir, dlen);
                memcpy(ray_full + dlen, rlrel, sizeof rlrel);
            } else {
                ray_full[0] = '\0';
            }

#ifdef _WIN32
            handle = (void *)LoadLibraryA(ray_full);
#else
            handle = dlopen(ray_full, RTLD_NOW | RTLD_GLOBAL);
#endif
            if (handle != NULL) {
#ifdef _WIN32
                attach = (python_raylib_attach_fn)GetProcAddress(
                    (HMODULE)handle, "python_raylib_attach");
#else
                attach = (python_raylib_attach_fn)dlsym(handle, "python_raylib_attach");
#endif
                if (attach != NULL) {
                    PyObject *mod = attach();
                    if (mod != NULL) {
                        PyObject *sys = PyImport_ImportModule("sys");
                        if (sys != NULL) {
                            PyObject *mods = PyObject_GetAttrString(sys, "modules");
                            if (mods != NULL) {
                                PyDict_SetItemString(mods, "gcl_raylib", mod);
                                Py_DECREF(mods);
                            }
                            Py_DECREF(sys);
                        }
                        Py_DECREF(mod);
                    }
                }
            }
        }
    }

    /* Install gcl_bridge into sys.modules (used by modules too). */
    gcl_bridge_install();

    /* sys.argv = [modname, args...] */
    {
        PyObject *sysmod = PyImport_ImportModule("sys");
        if (sysmod != NULL) {
            PyObject *py_argv = PyList_New((Py_ssize_t)mod_argc + 1);
            if (py_argv != NULL) {
                PyList_SetItem(py_argv, 0, PyUnicode_FromString(modname));
                for (int i = 0; i < mod_argc; i++)
                    PyList_SetItem(py_argv, (Py_ssize_t)i + 1,
                                   mod_argv[i] != NULL ? PyUnicode_FromString(mod_argv[i])
                                                       : PyUnicode_FromString(""));
                if (PyObject_SetAttrString(sysmod, "argv", py_argv) != 0)
                    PyErr_Clear();
                Py_DECREF(py_argv);
            }
            Py_DECREF(sysmod);
        }
    }

    /* runpy.run_module(modname, run_name="__main__", alter_sys=True) */
    {
        PyObject *runpy = PyImport_ImportModule("runpy");
        PyObject *run_module = NULL;
        PyObject *ret = NULL;
        if (runpy == NULL) {
            PyErr_Print();
            if (err && err_cap > 0)
                snprintf(err, err_cap, "cannot import runpy");
            rc = 1;
        } else {
            run_module = PyObject_GetAttrString(runpy, "run_module");
            Py_DECREF(runpy);
            if (run_module == NULL) {
                PyErr_Print();
                if (err && err_cap > 0)
                    snprintf(err, err_cap, "runpy.run_module not found");
                rc = 1;
            } else {
                PyObject *args = PyTuple_New(1);
                PyObject *kwargs = PyDict_New();
                if (args == NULL || kwargs == NULL) {
                    Py_XDECREF(args);
                    Py_XDECREF(kwargs);
                    Py_DECREF(run_module);
                    if (err && err_cap > 0)
                        snprintf(err, err_cap, "out of memory (runpy call)");
                    return 1;
                }
                PyTuple_SetItem(args, 0, PyUnicode_FromString(modname));
                PyDict_SetItemString(kwargs, "run_name", PyUnicode_FromString("__main__"));
                PyDict_SetItemString(kwargs, "alter_sys", Py_True);
                ret = PyObject_Call(run_module, args, kwargs);
                Py_DECREF(args);
                Py_DECREF(kwargs);
                Py_DECREF(run_module);
            }
            if (ret == NULL) {
                if (PyErr_Occurred())
                    PyErr_Print();
                if (err && err_cap > 0)
                    snprintf(err, err_cap, "cannot run module '%s'", modname);
                rc = 1;
            }
            Py_XDECREF(ret);
        }
    }

    Py_Finalize();
    return rc;
}
