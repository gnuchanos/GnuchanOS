/* ============================================================
 * lua_loader.h — Dynamic library loader for lua55.dll / liblua55.so
 *
 * Provides functions to:
 *   - load_lua_library(path)     → load a Lua shared library
 *   - resolve_lua_api(lib, api)  → resolve all Lua C API symbols
 *   - unload_lua_library(lib)    → free the library
 *
 * The default search order handles the common DLL/SO locations.
 * ============================================================ */

#ifndef LUA_LOADER_H
#define LUA_LOADER_H

#include "lua_platform.h"
#include "lua_api.h"

/**
 * load_lua_library - Try to load the Lua dynamic library.
 *
 * search_paths: NULL-terminated array of paths to try, or NULL to use defaults.
 * out_lib:      receives the loaded LIB_HANDLE on success.
 * out_used:     if non-NULL, receives the path that succeeded.
 *
 * Returns 0 on success, -1 on failure (error message printed to stderr).
 */
int load_lua_library(const char **search_paths, LIB_HANDLE *out_lib, const char **out_used);

/**
 * resolve_lua_api - Resolve all required Lua C API symbols from the library.
 * Returns 0 on success, -1 if any symbol is missing.
 */
int resolve_lua_api(LIB_HANDLE lib, LuaAPI *api);

/**
 * unload_lua_library - Free the loaded library handle.
 */
void unload_lua_library(LIB_HANDLE lib);

#endif /* LUA_LOADER_H */
