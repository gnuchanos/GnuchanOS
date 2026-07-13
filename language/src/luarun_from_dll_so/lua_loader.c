/* ============================================================
 * lua_loader.c — Implementation: Load lua55.dll / liblua55.so
 *                and resolve Lua C API symbols at runtime.
 *
 * NOTE: The resolve_lua_api function unavoidably casts between
 * incompatible pointer types (GetProcAddress/dlsym return values
 * to specific function-pointer types).  #pragma GCC diagnostic
 * suppresses the -Wpedantic / -Wcast-function-type warnings;
 * the code works correctly on all platforms.
 * ============================================================ */

#include "lua_loader.h"
#include "lua_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* load_lua_library — load exactly the given path; no fallback        */
/* ------------------------------------------------------------------ */
int load_lua_library(const char **search_paths, LIB_HANDLE *out_lib, const char **out_used) {
    if (!out_lib) return -1;

    /* Require caller to provide exactly one path */
    if (!search_paths || !search_paths[0] || search_paths[0][0] == '\0') {
        fprintf(stderr, "Error: no library path specified.\n");
        *out_lib = NULL;
        return -1;
    }

    LIB_HANDLE lib = LIB_LOAD(search_paths[0]);
    if (lib) {
        if (out_used) *out_used = search_paths[0];
        *out_lib = lib;
        printf("[LUA] Loaded: %s\n", search_paths[0]);
        return 0;
    }

    *out_lib = NULL;
    return -1;
}

/* ------------------------------------------------------------------ */
/* resolve_lua_api — resolve every symbol we need from the library    */
/* ------------------------------------------------------------------ */
int resolve_lua_api(LIB_HANDLE lib, LuaAPI *api) {
    if (!lib || !api) return -1;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcast-function-type"

#define RESOLVE(name) \
    do { \
        api->name = (name##_t)LIB_SYM(lib, #name); \
        if (!api->name) { \
            fprintf(stderr, "Error: " LUA_LIB_NAME " missing symbol '%s'\n", #name); \
            return -1; \
        } \
    } while(0)

    RESOLVE(luaL_newstate);
    RESOLVE(lua_close);
    RESOLVE(lua_pcallk);
    RESOLVE(luaL_loadfilex);
    RESOLVE(luaL_loadbufferx);
    RESOLVE(lua_gettop);
    RESOLVE(lua_settop);
    RESOLVE(lua_tolstring);
    RESOLVE(lua_toboolean);
    RESOLVE(luaL_requiref);

#undef RESOLVE

#define RESOLVE_LIB(name) \
    do { \
        api->name = (lua_CFunction)LIB_SYM(lib, #name); \
        if (!api->name) { \
            fprintf(stderr, "Error: " LUA_LIB_NAME " missing symbol '%s'\n", #name); \
            return -1; \
        } \
    } while(0)

    RESOLVE_LIB(luaopen_base);
    RESOLVE_LIB(luaopen_table);
    RESOLVE_LIB(luaopen_string);
    RESOLVE_LIB(luaopen_math);
    RESOLVE_LIB(luaopen_io);
    RESOLVE_LIB(luaopen_os);
    RESOLVE_LIB(luaopen_debug);
    RESOLVE_LIB(luaopen_coroutine);
    RESOLVE_LIB(luaopen_package);
    RESOLVE_LIB(luaopen_utf8);

#undef RESOLVE_LIB

#pragma GCC diagnostic pop
    return 0;
}

/* ------------------------------------------------------------------ */
/* unload_lua_library — free the library handle                       */
/* ------------------------------------------------------------------ */
void unload_lua_library(LIB_HANDLE lib) {
    if (lib)
        LIB_FREE(lib);
}
