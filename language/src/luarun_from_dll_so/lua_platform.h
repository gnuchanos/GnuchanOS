/* ============================================================
 * lua_platform.h — Platform abstraction for dynamic library loading
 *
 * Provides macros that abstract away LoadLibrary (Windows) vs
 * dlopen (Linux) for loading lua55.dll / liblua55.so at runtime.
 * ============================================================ */

#ifndef LUA_PLATFORM_H
#define LUA_PLATFORM_H

#ifdef _WIN32
    #include <windows.h>
    #define LIB_HANDLE           HMODULE
    #define LIB_LOAD(lib)        LoadLibraryA(lib)
    #define LIB_SYM(h, n)        GetProcAddress(h, n)
    #define LIB_FREE(h)          FreeLibrary(h)
    #define LIB_ERR_MSG()        "unknown error"
    #define LUA_LIB_NAME         "lua55.dll"
    #define LUA_LIB_TEST         "_test_area/lua_test/lua55.dll"
#else
    #include <dlfcn.h>
    #define LIB_HANDLE           void*
    #define LIB_LOAD(lib)        dlopen(lib, RTLD_LAZY | RTLD_LOCAL)
    #define LIB_SYM(h, n)        dlsym(h, n)
    #define LIB_FREE(h)          dlclose(h)
    #define LIB_ERR_MSG()        dlerror()
    #define LUA_LIB_NAME         "liblua55.so"
    #define LUA_LIB_TEST         "_test_area/lua_test/liblua55.so"
#endif

#endif /* LUA_PLATFORM_H */
