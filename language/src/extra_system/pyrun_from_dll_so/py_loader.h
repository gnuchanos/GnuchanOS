/* ============================================================
 * py_loader.h — Dynamic library loader for python314.dll/so
 *
 * Provides functions to:
 *   - load_py_library(path)     → load Python shared library
 *   - resolve_py_api(lib, api)  → resolve all Python C API symbols
 *   - unload_py_library(lib)    → free the library
 * ============================================================ */

#ifndef PY_LOADER_H
#define PY_LOADER_H

#include "py_platform.h"
#include "py_api.h"

/**
 * load_py_library - Load the Python dynamic library.
 *
 * dll_path: filesystem path to python314.dll or libpython3.14.so.
 * out_lib:  receives the loaded LIB_HANDLE on success.
 *
 * Returns 0 on success, -1 on failure (error message printed to stderr).
 */
int load_py_library(const char *dll_path, LIB_HANDLE *out_lib);

/**
 * resolve_py_api - Resolve all required Python C API symbols from the library.
 * Returns 0 on success, -1 if any symbol is missing.
 */
int resolve_py_api(LIB_HANDLE lib, PythonAPI *api);

/**
 * unload_py_library - Free the loaded library handle.
 */
void unload_py_library(LIB_HANDLE lib);

#endif /* PY_LOADER_H */
