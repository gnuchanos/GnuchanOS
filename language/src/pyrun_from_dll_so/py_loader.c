/* ============================================================
 * py_loader.c — Implementation: Load python314.dll / .so
 *                and resolve Python C API symbols at runtime.
 *
 * NOTE: resolve_py_api unavoidably casts between incompatible
 * pointer types (GetProcAddress/dlsym return values to specific
 * function-pointer types).  #pragma GCC diagnostic suppresses
 * the -Wpedantic / -Wcast-function-type warnings.
 * ============================================================ */

#include "py_loader.h"
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* load_py_library — load the given path                              */
/* ------------------------------------------------------------------ */
int load_py_library(const char *dll_path, LIB_HANDLE *out_lib) {
    if (!dll_path || !dll_path[0] || !out_lib) {
        fprintf(stderr, "Error: invalid arguments to load_py_library\n");
        return -1;
    }

    LIB_HANDLE lib = LIB_LOAD(dll_path);
    if (!lib) {
        fprintf(stderr, "Error: failed to load '%s'\n", dll_path);
        *out_lib = NULL;
        return -1;
    }

    printf("[PY] Loaded: %s\n", dll_path);
    *out_lib = lib;
    return 0;
}

/* ------------------------------------------------------------------ */
/* resolve_py_api — resolve every symbol we need from the library     */
/* ------------------------------------------------------------------ */
int resolve_py_api(LIB_HANDLE lib, PythonAPI *api) {
    if (!lib || !api) return -1;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcast-function-type"

#define RESOLVE(name) \
    do { \
        api->name = (name##_t)LIB_SYM(lib, #name); \
        if (!api->name) { \
            fprintf(stderr, "Error: missing symbol '%s'\n", #name); \
            return -1; \
        } \
    } while(0)

    RESOLVE(Py_Initialize);
    RESOLVE(Py_IsInitialized);
    RESOLVE(Py_Finalize);
    RESOLVE(Py_FinalizeEx);
    RESOLVE(PyRun_SimpleString);
    RESOLVE(PyRun_SimpleFile);
    RESOLVE(Py_GetVersion);
    RESOLVE(Py_GetCopyright);

#undef RESOLVE

#pragma GCC diagnostic pop
    return 0;
}

/* ------------------------------------------------------------------ */
/* unload_py_library — free the library handle                        */
/* ------------------------------------------------------------------ */
void unload_py_library(LIB_HANDLE lib) {
    if (lib)
        LIB_FREE(lib);
}
