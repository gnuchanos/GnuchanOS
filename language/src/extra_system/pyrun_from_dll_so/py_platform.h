/* ============================================================
 * py_platform.h — Platform abstraction for dynamic library
 *                  loading (Windows / Linux)
 *
 * Provides macros for LoadLibrary (Win) / dlopen (Linux)
 * to load python314.dll/libpython3.14.so at runtime.
 * ============================================================ */

#ifndef PY_PLATFORM_H
#define PY_PLATFORM_H

#ifdef _WIN32
    #include <windows.h>
    #define LIB_HANDLE           HMODULE
    #define LIB_LOAD(lib)        LoadLibraryA(lib)
    #define LIB_SYM(h, n)        GetProcAddress(h, n)
    #define LIB_FREE(h)          FreeLibrary(h)
    #define LIB_ERR_MSG()        "unknown error"
#else
    #include <dlfcn.h>
    #define LIB_HANDLE           void*
    #define LIB_LOAD(lib)        dlopen(lib, RTLD_LAZY | RTLD_LOCAL)
    #define LIB_SYM(h, n)        dlsym(h, n)
    #define LIB_FREE(h)          dlclose(h)
    #define LIB_ERR_MSG()        dlerror()
#endif

#endif /* PY_PLATFORM_H */
