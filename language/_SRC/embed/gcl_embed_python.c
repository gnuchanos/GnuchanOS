/*
 * gcl_embed_python.c — GCL Embed Python backend.
 *
 * Python 3.14 embed (astral-sh/python-build-standalone) ile çalışır.
 * simple_doc.md:
 *   Embed.Run("python")   → Py_Initialize
 *   Embed.Stop("python")  → Py_Finalize
 *   Embed.GetValue("python", value) → global oku
 *   Embed.SendValue("python", value) → global yaz
 *
 * Windows: MSVC .lib import library'leri MinGW ile uyumsuz olduğu için
 * Python C-API'si **dinamik yükleme** (LoadLibrary) ile çözülür.
 * Linux: libpython doğrudan link edilir (GCL_HAVE_PYTHON).
 *
 * NOT: Python memory layout'u (PyObject.ob_type) Linux/Windows aynıdır.
 * PyLong_Type vb. global nesne adresleri dinamik yüklemede GetProcAddress
 * ile alınır; Py_TYPE(o) makrosu (ob_type üzerinden) her modda çalışır.
 */

#include "gcl_embed_python.h"

#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "gcl_shared_state.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* ---------- Python C-API fonksiyon imzaları ---------- */
typedef void (*fn_py_initialize)(void);
typedef int  (*fn_py_is_initialized)(void);
typedef void (*fn_py_finalize)(void);
typedef int  (*fn_py_run_simple_string)(const char *code);
typedef PyObject *(*fn_py_import_add_module)(const char *name);
typedef PyObject *(*fn_py_object_get_attr_string)(PyObject *obj, const char *name);
typedef void (*fn_py_err_clear)(void);
typedef PyObject *(*fn_py_unicode_from_string)(const char *str);
typedef int  (*fn_py_list_append)(PyObject *list, PyObject *item);
typedef PyObject *(*fn_py_sys_get_object)(const char *name);
typedef long long (*fn_py_long_as_longlong)(PyObject *obj);
typedef double (*fn_py_float_as_double)(PyObject *obj);
typedef const char *(*fn_py_unicode_as_utf8)(PyObject *obj);
typedef char *(*fn_py_bytes_as_string)(PyObject *obj);
typedef void (*fn_py_dealloc)(PyObject *obj);
typedef int  (*fn_py_type_is_subtype)(PyTypeObject *a, PyTypeObject *b);
typedef int  (*fn_py_object_is_true)(PyObject *obj);
typedef int  (*fn_py_status_exception)(PyStatus status);
typedef wchar_t *(*fn_py_decode_locale)(const char *arg, size_t *size);
typedef void (*fn_py_mem_raw_free)(void *ptr);

/* PyConfig API (Python 3.14) */
typedef void  (*fn_py_config_init_python_config)(PyConfig *config);
typedef PyStatus (*fn_py_config_set_string)(PyConfig *config, wchar_t **config_str, const wchar_t *str);
typedef PyStatus (*fn_py_config_read)(PyConfig *config);
typedef PyStatus (*fn_py_initialize_from_config)(const PyConfig *config);
typedef void  (*fn_py_config_clear)(PyConfig *config);

/* ---------- API yapısı ---------- */
typedef struct {
    void *handle;                          /* HMODULE / dlopen handle */
    int loaded;

    fn_py_initialize initialize;
    fn_py_is_initialized is_initialized;
    fn_py_finalize finalize;
    fn_py_run_simple_string run_simple_string;
    fn_py_import_add_module import_add_module;
    fn_py_object_get_attr_string object_get_attr_string;
    fn_py_err_clear err_clear;
    fn_py_unicode_from_string unicode_from_string;
    fn_py_list_append list_append;
    fn_py_sys_get_object sys_get_object;
    fn_py_long_as_longlong long_as_longlong;
    fn_py_float_as_double float_as_double;
    fn_py_unicode_as_utf8 unicode_as_utf8;
    fn_py_bytes_as_string bytes_as_string;
    fn_py_dealloc dealloc;
    fn_py_type_is_subtype type_is_subtype;
    fn_py_object_is_true object_is_true;
    fn_py_status_exception status_exception;
    fn_py_decode_locale decode_locale;
    fn_py_mem_raw_free mem_raw_free;
    fn_py_config_init_python_config config_init_python_config;
    fn_py_config_set_string config_set_string;
    fn_py_config_read config_read;
    fn_py_initialize_from_config initialize_from_config;
    fn_py_config_clear config_clear;

    /* Global tip nesneleri (veri symbol) */
    PyTypeObject *long_type;
    PyTypeObject *float_type;
    PyTypeObject *bool_type;
    PyTypeObject *unicode_type;
    PyTypeObject *bytes_type;
} PyApi;

static PyApi g_api;
static int g_py_initialized = 0;

/* Python embed dizinini bul (PYTHON_EMBED_DIR makefile'dan gelir) */
static const char *py_embed_dir(void) {
#ifdef PYTHON_EMBED_DIR
    return PYTHON_EMBED_DIR;
#else
    return NULL;
#endif
}

/* isim geçerli Python identifier mı? (değişken adı güvenliği) */
static int is_valid_identifier(const char *name) {
    if (!name || !name[0]) return 0;
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return 0;
    for (const char *p = name + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) return 0;
    }
    return 1;
}

/* DECREF: refcount azalt, 0 ise dealloc çağır.
   Not: Py_DECREF makrosu _Py_Dealloc sembolüne bağımlıdır; dinamik yüklemede
   doğrudan kullanılamaz. Bu yüzden manuel refcount + dealloc kullanılır. */
static void py_decref(PyObject *obj) {
    if (!obj) return;
    if (--obj->ob_refcnt == 0 && g_api.dealloc) {
        g_api.dealloc(obj);
    }
}

/* Tip kontrolü: o, type'ın alt sınıfı mı? (PyObject_TypeCheck benzeri) */
static int py_is_type(PyObject *o, PyTypeObject *type) {
    if (!o || !type || !g_api.type_is_subtype) return 0;
    return Py_TYPE(o) == type || g_api.type_is_subtype(Py_TYPE(o), type);
}

/* ---------- API yükleme ---------- */
#ifdef GCL_HAVE_PYTHON
/* Doğrudan link modu (Linux): fonksiyonlar zaten bağlı */
static int api_load(void) {
    g_api.handle = NULL;
    g_api.initialize = Py_Initialize;
    g_api.is_initialized = Py_IsInitialized;
    g_api.finalize = Py_Finalize;
    g_api.run_simple_string = PyRun_SimpleString;
    g_api.import_add_module = PyImport_AddModule;
    g_api.object_get_attr_string = PyObject_GetAttrString;
    g_api.err_clear = PyErr_Clear;
    g_api.unicode_from_string = PyUnicode_FromString;
    g_api.list_append = PyList_Append;
    g_api.sys_get_object = PySys_GetObject;
    g_api.long_as_longlong = PyLong_AsLongLong;
    g_api.float_as_double = PyFloat_AsDouble;
    g_api.unicode_as_utf8 = PyUnicode_AsUTF8;
    g_api.bytes_as_string = PyBytes_AsString;
    g_api.dealloc = _Py_Dealloc;
    g_api.type_is_subtype = PyType_IsSubtype;
    g_api.object_is_true = PyObject_IsTrue;
    g_api.status_exception = PyStatus_Exception;
    g_api.decode_locale = Py_DecodeLocale;
    g_api.mem_raw_free = PyMem_RawFree;
    g_api.config_init_python_config = PyConfig_InitPythonConfig;
    g_api.config_set_string = PyConfig_SetString;
    g_api.config_read = PyConfig_Read;
    g_api.initialize_from_config = Py_InitializeFromConfig;
    g_api.config_clear = PyConfig_Clear;
    g_api.long_type = &PyLong_Type;
    g_api.float_type = &PyFloat_Type;
    g_api.bool_type = &PyBool_Type;
    g_api.unicode_type = &PyUnicode_Type;
    g_api.bytes_type = &PyBytes_Type;
    g_api.loaded = 1;
    return 1;
}
#else
/* Dinamik yükleme modu (Windows / Linux) */
static int api_load(void) {
#ifdef _WIN32
    /* python3.dll, stable ABI shim'dir — PyConfig_* / Py_InitializeFromConfig
       export etmez. Bu yüzden önce tam C-API'li python314.dll'i dene; shim'i atla. */
    const char *dir = py_embed_dir();
    char dll_path[MAX_PATH];
    g_api.handle = NULL;

    /* 1) Tam Python 3.14 DLL (PYTHON_EMBED_DIR içinden) */
    if (dir && dir[0]) {
        snprintf(dll_path, sizeof(dll_path), "%s\\python314.dll", dir);
        g_api.handle = LoadLibraryA(dll_path);
    }
    /* 2) PATH üzerinden */
    if (!g_api.handle) g_api.handle = LoadLibraryA("python314.dll");
    /* 3) Dosya taraması: python*.dll ama python3.dll shim'ini atla */
    if (!g_api.handle && dir && dir[0]) {
        char search[MAX_PATH];
        snprintf(search, sizeof(search), "%s\\python*.dll", dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(search, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (_stricmp(fd.cFileName, "python3.dll") != 0) {
                    snprintf(dll_path, sizeof(dll_path), "%s\\%s", dir, fd.cFileName);
                    g_api.handle = LoadLibraryA(dll_path);
                    if (g_api.handle) break;
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    /* 4) Son çare stable ABI shim */
    if (!g_api.handle) g_api.handle = LoadLibraryA("python3.dll");
    if (!g_api.handle) return 0;

#define RESOLVE(var, sym) do { \
    var = (void *)(uintptr_t)GetProcAddress((HMODULE)g_api.handle, sym); \
    if (!var) return 0; \
} while (0)
#define RESOLVE_DATA(var, sym) do { \
    var = (void *)(uintptr_t)GetProcAddress((HMODULE)g_api.handle, sym); \
    if (!var) return 0; \
} while (0)

    RESOLVE(g_api.initialize, "Py_Initialize");
    RESOLVE(g_api.is_initialized, "Py_IsInitialized");
    RESOLVE(g_api.finalize, "Py_Finalize");
    RESOLVE(g_api.run_simple_string, "PyRun_SimpleString");
    RESOLVE(g_api.import_add_module, "PyImport_AddModule");
    RESOLVE(g_api.object_get_attr_string, "PyObject_GetAttrString");
    RESOLVE(g_api.err_clear, "PyErr_Clear");
    RESOLVE(g_api.unicode_from_string, "PyUnicode_FromString");
    RESOLVE(g_api.list_append, "PyList_Append");
    RESOLVE(g_api.sys_get_object, "PySys_GetObject");
    RESOLVE(g_api.long_as_longlong, "PyLong_AsLongLong");
    RESOLVE(g_api.float_as_double, "PyFloat_AsDouble");
    RESOLVE(g_api.unicode_as_utf8, "PyUnicode_AsUTF8");
    RESOLVE(g_api.bytes_as_string, "PyBytes_AsString");
    RESOLVE(g_api.type_is_subtype, "PyType_IsSubtype");
    RESOLVE(g_api.object_is_true, "PyObject_IsTrue");
    RESOLVE(g_api.status_exception, "PyStatus_Exception");
    RESOLVE(g_api.decode_locale, "Py_DecodeLocale");
    RESOLVE(g_api.mem_raw_free, "PyMem_RawFree");
    RESOLVE(g_api.config_init_python_config, "PyConfig_InitPythonConfig");
    RESOLVE(g_api.config_set_string, "PyConfig_SetString");
    RESOLVE(g_api.config_read, "PyConfig_Read");
    RESOLVE(g_api.initialize_from_config, "Py_InitializeFromConfig");
    RESOLVE(g_api.config_clear, "PyConfig_Clear");
    /* _Py_Dealloc isteğe bağlı — bulunamazsa refcount azaltılır ama dealloc çağrılmaz */
    g_api.dealloc = (fn_py_dealloc)(void *)(uintptr_t)GetProcAddress((HMODULE)g_api.handle, "_Py_Dealloc");
    /* Global tip nesneleri (data symbol) */
    RESOLVE_DATA(g_api.long_type, "PyLong_Type");
    RESOLVE_DATA(g_api.float_type, "PyFloat_Type");
    RESOLVE_DATA(g_api.bool_type, "PyBool_Type");
    RESOLVE_DATA(g_api.unicode_type, "PyUnicode_Type");
    RESOLVE_DATA(g_api.bytes_type, "PyBytes_Type");
#undef RESOLVE
#undef RESOLVE_DATA
    g_api.loaded = 1;
    return 1;
#else
    const char *candidates[] = {
        "libpython3.14.so", "libpython3.14.so.1.0",
        "libpython3.13.so", "libpython3.12.so", "libpython3.11.so",
        "libpython3.so", NULL
    };
    for (int i = 0; candidates[i]; i++) {
        g_api.handle = dlopen(candidates[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_api.handle) break;
    }
    if (!g_api.handle) return 0;

#define RESOLVE(var, sym) do { \
    *(void **)(&var) = dlsym(g_api.handle, sym); \
    if (!var) return 0; \
} while (0)

    RESOLVE(g_api.initialize, "Py_Initialize");
    RESOLVE(g_api.is_initialized, "Py_IsInitialized");
    RESOLVE(g_api.finalize, "Py_Finalize");
    RESOLVE(g_api.run_simple_string, "PyRun_SimpleString");
    RESOLVE(g_api.import_add_module, "PyImport_AddModule");
    RESOLVE(g_api.object_get_attr_string, "PyObject_GetAttrString");
    RESOLVE(g_api.err_clear, "PyErr_Clear");
    RESOLVE(g_api.unicode_from_string, "PyUnicode_FromString");
    RESOLVE(g_api.list_append, "PyList_Append");
    RESOLVE(g_api.sys_get_object, "PySys_GetObject");
    RESOLVE(g_api.long_as_longlong, "PyLong_AsLongLong");
    RESOLVE(g_api.float_as_double, "PyFloat_AsDouble");
    RESOLVE(g_api.unicode_as_utf8, "PyUnicode_AsUTF8");
    RESOLVE(g_api.bytes_as_string, "PyBytes_AsString");
    RESOLVE(g_api.type_is_subtype, "PyType_IsSubtype");
    RESOLVE(g_api.object_is_true, "PyObject_IsTrue");
    RESOLVE(g_api.status_exception, "PyStatus_Exception");
    RESOLVE(g_api.decode_locale, "Py_DecodeLocale");
    RESOLVE(g_api.mem_raw_free, "PyMem_RawFree");
    RESOLVE(g_api.config_init_python_config, "PyConfig_InitPythonConfig");
    RESOLVE(g_api.config_set_string, "PyConfig_SetString");
    RESOLVE(g_api.config_read, "PyConfig_Read");
    RESOLVE(g_api.initialize_from_config, "Py_InitializeFromConfig");
    RESOLVE(g_api.config_clear, "PyConfig_Clear");
    /* _Py_Dealloc isteğe bağlı */
    g_api.dealloc = (fn_py_dealloc)(void *)dlsym(g_api.handle, "_Py_Dealloc");
    /* Global tip nesneleri (data symbol) */
    g_api.long_type = (PyTypeObject *)dlsym(g_api.handle, "PyLong_Type");
    g_api.float_type = (PyTypeObject *)dlsym(g_api.handle, "PyFloat_Type");
    g_api.bool_type = (PyTypeObject *)dlsym(g_api.handle, "PyBool_Type");
    g_api.unicode_type = (PyTypeObject *)dlsym(g_api.handle, "PyUnicode_Type");
    g_api.bytes_type = (PyTypeObject *)dlsym(g_api.handle, "PyBytes_Type");
#undef RESOLVE
    g_api.loaded = 1;
    return 1;
#endif
}
#endif

static void api_unload(void) {
    if (!g_api.handle) return;
#ifdef _WIN32
    FreeLibrary((HMODULE)g_api.handle);
#else
    dlclose(g_api.handle);
#endif
    memset(&g_api, 0, sizeof(g_api));
}

/* ---------- Runtime API ---------- */
int gcl_py_runtime_init(void) {
    if (g_py_initialized) return 1;
    if (!g_api.loaded) {
        if (!api_load()) return 0;
    }

    const char *home = py_embed_dir();
    PyConfig config;
    g_api.config_init_python_config(&config);

    if (home && home[0]) {
        /* Standalone Python build: home yolu, stdlib'i bulmak için. */
        wchar_t *wpath = g_api.decode_locale(home, NULL);
        if (wpath) {
            g_api.config_set_string(&config, &config.home, wpath);
            g_api.mem_raw_free(wpath);
        }
    }

    config.install_signal_handlers = 0;
    config.parse_argv = 0;

    PyStatus status = g_api.config_read(&config);
    if (g_api.status_exception(status)) {
        g_api.config_clear(&config);
        return 0;
    }
    status = g_api.initialize_from_config(&config);
    if (g_api.status_exception(status)) {
        g_api.config_clear(&config);
        return 0;
    }
    g_api.config_clear(&config);

    /* embed dizinini sys.path'e ekle — gömülü stdlib/modül bulunsun */
    if (home && home[0]) {
        PyObject *sys_path = g_api.sys_get_object("path");
        if (sys_path) {
            PyObject *pystr = g_api.unicode_from_string(home);
            if (pystr) {
                g_api.list_append(sys_path, pystr);
                py_decref(pystr);
            }
        }
    }

    g_py_initialized = 1;
    return 1;
}

void gcl_py_runtime_shutdown(void) {
    if (!g_py_initialized) return;
    g_api.finalize();
    g_py_initialized = 0;
}

int gcl_py_runtime_is_active(void) {
    return g_py_initialized;
}

int gcl_py_run_simple(const char *code) {
    if (!g_py_initialized) return 0;
    if (!code) return 0;
    if (g_api.run_simple_string(code) != 0) {
        g_api.err_clear();
        return 0;
    }
    return 1;
}

int gcl_py_set_global(const char *name, const char *value) {
    if (!g_py_initialized) return 0;
    if (!is_valid_identifier(name)) return 0;
    if (!value) return 0;

    /* name = value; olarak atama yap */
    size_t n = strlen(name) + strlen(value) + 8;
    char *code = (char *)malloc(n);
    if (!code) return 0;
    snprintf(code, n, "%s = %s", name, value);

    int ok = gcl_py_run_simple(code);
    free(code);
    return ok;
}

int gcl_py_get_global(const char *name, char *out, size_t out_size) {
    if (!g_py_initialized) return 0;
    if (!is_valid_identifier(name)) return 0;
    if (!out || out_size == 0) return 0;

    PyObject *main_mod = g_api.import_add_module("__main__");
    if (!main_mod) {
        g_api.err_clear();
        return 0;
    }

    /* __main__ globals'ından değişkeni al */
    PyObject *val = g_api.object_get_attr_string(main_mod, name);
    if (!val) {
        g_api.err_clear();
        return 0;
    }

    int ok = 0;
    if (py_is_type(val, g_api.long_type)) {
        long long v = g_api.long_as_longlong(val);
        snprintf(out, out_size, "%lld", v);
        ok = 1;
    } else if (py_is_type(val, g_api.float_type)) {
        double v = g_api.float_as_double(val);
        snprintf(out, out_size, "%g", v);
        ok = 1;
    } else if (py_is_type(val, g_api.unicode_type)) {
        const char *s = g_api.unicode_as_utf8(val);
        if (s) {
            snprintf(out, out_size, "%s", s);
            ok = 1;
        }
    } else if (py_is_type(val, g_api.bool_type)) {
        int t = g_api.object_is_true(val);
        snprintf(out, out_size, "%s", t ? "1" : "0");
        ok = 1;
    } else if (py_is_type(val, g_api.bytes_type)) {
        char *s = g_api.bytes_as_string(val);
        if (s) {
            snprintf(out, out_size, "%s", s);
            ok = 1;
        }
    }

    py_decref(val);
    g_api.err_clear();
    return ok;
}

int gcl_py_add_path(const char *path) {
    if (!g_py_initialized) return 0;
    if (!path || !path[0]) return 0;

    PyObject *sys_path = g_api.sys_get_object("path");
    if (!sys_path) {
        g_api.err_clear();
        return 0;
    }
    PyObject *pystr = g_api.unicode_from_string(path);
    if (!pystr) {
        g_api.err_clear();
        return 0;
    }
    int rc = g_api.list_append(sys_path, pystr);
    py_decref(pystr);
    if (rc != 0) {
        g_api.err_clear();
        return 0;
    }
    return 1;
}

/* Python dosya çalıştır (gcl -pyrun) — Embed.dll'den export edilir.
   sys.path'e script dizini + embed library dizinleri ekler, raylib.pyd
   bağımlılığını (Library/) çözer. döner: 0=başarı, 1=hata. */
int gcl_py_run_file(const char *path) {
    if (!path || !path[0]) return 1;

    if (!gcl_py_runtime_init()) {
        fprintf(stderr, "Error: Python runtime init failed\n");
        return 1;
    }

    /* script dizinini sys.path'e ekle — göreli dosya adıysa geçerli dizini kullan */
    char script_dir[4096];
    snprintf(script_dir, sizeof(script_dir), "%s", path);
    int has_sep = 0;
    char *slash = strrchr(script_dir, '\\');
    if (slash) { *slash = '\0'; has_sep = 1; }
    else {
        slash = strrchr(script_dir, '/');
        if (slash) { *slash = '\0'; has_sep = 1; }
    }
    if (!has_sep) snprintf(script_dir, sizeof(script_dir), ".");
    gcl_py_add_path(script_dir);

    /* exe dizinini bul → Library/Embeded/Python_Runtime + Library ekle (.pyd için) */
    char exe_dir[4096] = "";
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe_dir, (DWORD)sizeof(exe_dir));
#else
    ssize_t n = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (n > 0) exe_dir[n] = '\0';
#endif
    char old_dll_dir[4096] = "";
    int dll_changed = 0;
    if (exe_dir[0]) {
        char *es = strrchr(exe_dir, '\\');
        if (!es) es = strrchr(exe_dir, '/');
        if (es) *es = '\0';

        char pyd_dir[4096];
        snprintf(pyd_dir, sizeof(pyd_dir), "%s/Library/Embeded/Python_Runtime", exe_dir);
        gcl_py_add_path(pyd_dir);
        snprintf(pyd_dir, sizeof(pyd_dir), "%s/Library", exe_dir);
        gcl_py_add_path(pyd_dir);

#ifdef _WIN32
        /* raylib.pyd → Raylib.dll bağımlılığını çöz: Library/ dizinini DLL arama yoluna ekle.
           Önceki DLL arama yolunu sakla — işlem sonunda geri yükle. */
        DWORD old_len = GetDllDirectoryA((DWORD)sizeof(old_dll_dir), old_dll_dir);
        if (old_len == 0 || old_len >= sizeof(old_dll_dir)) old_dll_dir[0] = '\0';
        char dll_dir[4096];
        snprintf(dll_dir, sizeof(dll_dir), "%s\\Library", exe_dir);
        SetDllDirectoryA(dll_dir);
        dll_changed = 1;
#endif
    }

    int rc = 1;
    /* Dosyayı oku ve çalıştır */
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot read '%s'\n", path);
        goto cleanup;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); goto cleanup; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); goto cleanup; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); goto cleanup; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    rc = gcl_py_run_simple(buf) ? 0 : 1;
    free(buf);
    gcl_shared_cleanup();

cleanup:
#ifdef _WIN32
    if (dll_changed) {
        /* Önceki DLL arama yolunu geri yükle (NULL → işletim sistemi varsayılanı) */
        if (old_dll_dir[0]) SetDllDirectoryA(old_dll_dir);
        else SetDllDirectoryA(NULL);
    }
#endif
    return rc;
}
