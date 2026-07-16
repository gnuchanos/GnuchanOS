/* ============================================================
 * lua_runner.c — Implementation: Lua state management & script runner
 * ============================================================ */

#include "lua_runner.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* create_lua_state                                                    */
/* ------------------------------------------------------------------ */
lua_State* create_lua_state(const LuaAPI *api) {
    if (!api || !api->luaL_newstate) {
        fprintf(stderr, "Error: LuaAPI not initialized\n");
        return NULL;
    }
    lua_State *L = api->luaL_newstate();
    if (!L)
        fprintf(stderr, "Error: luaL_newstate() failed\n");
    return L;
}

/* ------------------------------------------------------------------ */
/* open_lua_libraries                                                  */
/* ------------------------------------------------------------------ */
int open_lua_libraries(const LuaAPI *api, lua_State *L) {
    if (!api || !L) return -1;

    /* Each entry: { module_name, luaopen_function } */
    typedef struct { const char *name; lua_CFunction openfunc; } LibEntry;

    LibEntry libs[] = {
        {"_G",          api->luaopen_base},
        {"table",       api->luaopen_table},
        {"string",      api->luaopen_string},
        {"math",        api->luaopen_math},
        {"io",          api->luaopen_io},
        {"os",          api->luaopen_os},
        {"debug",       api->luaopen_debug},
        {"coroutine",   api->luaopen_coroutine},
        {"package",     api->luaopen_package},
        {"utf8",        api->luaopen_utf8},
        {NULL, NULL}
    };

    for (int i = 0; libs[i].name; i++) {
        if (!libs[i].openfunc) {
            fprintf(stderr, "Warning: luaopen_%s not available, skipping\n", libs[i].name);
            continue;
        }
        api->luaL_requiref(L, libs[i].name, libs[i].openfunc, 1);
        api->lua_settop(L, api->lua_gettop(L) - 1); /* pop result */
    }

    printf("[LUA] Libraries loaded: %s\n",
        "base, table, string, math, io, os, debug, coroutine, package, utf8");
    return 0;
}

/* ------------------------------------------------------------------ */
/* run_lua_script — load a .lua file and call pcallk                   */
/* ------------------------------------------------------------------ */
int run_lua_script(const LuaAPI *api, lua_State *L, const char *path) {
    if (!api || !L || !path) {
        fprintf(stderr, "Error: invalid arguments to run_lua_script\n");
        return 1;
    }

    printf("[LUA] Running: %s\n", path);

    int top = api->lua_gettop(L);
    (void)top; /* may be unused in non-error path */

    /* Load the file */
    int load_result = api->luaL_loadfilex(L, path, NULL);
    if (load_result != 0) {
        const char *err = api->lua_tolstring(L, -1, NULL);
        fprintf(stderr, "Error loading Lua script: %s\n", err ? err : "unknown error");
        api->lua_settop(L, top);
        return 1;
    }

    /* Execute */
    int call_result = api->lua_pcallk(L, 0, 0, 0, 0);
    if (call_result != 0) {
        const char *err = api->lua_tolstring(L, -1, NULL);
        fprintf(stderr, "Error executing Lua script: %s\n", err ? err : "unknown error");
        api->lua_settop(L, top);
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* close_lua_state                                                     */
/* ------------------------------------------------------------------ */
void close_lua_state(const LuaAPI *api, lua_State *L) {
    if (api && L)
        api->lua_close(L);
}
