/* ============================================================
 * run_lua.c — Lua script runner entry point
 *
 * Called by flag_luarun.c after argument parsing.  Delegates
 * to lua55_embed (which wraps lua_loader + lua_runner).
 *
 * Usage from gcl:
 *   gcl -luarun <script.lua> -dll <path/lua55.dll>
 *   gcl -luarun <script.lua> -so  <path/liblua55.so>
 * ============================================================ */

#include "run_lua.h"
#include "lua55_embed.h"
#include <stdio.h>

int run_lua(const char *script, const char *dll_path) {
    /* Load lua55.dll/so */
    Lua55Embed *lua = lua55_load(dll_path);
    if (!lua) {
        fprintf(stderr, "error: failed to load '%s'\n", dll_path);
        return 1;
    }

    /* Create Lua state */
    lua_State *L = lua55_newstate(lua);
    if (!L) {
        fprintf(stderr, "error: failed to create Lua state\n");
        lua55_unload(lua);
        return 1;
    }

    /* Open standard libraries */
    lua55_openlibs(lua, L);

    /* Run the script */
    int status = lua55_dofile(lua, L, script);
    if (status != LUA_OK) {
        fprintf(stderr, "error running '%s':\n", script);
        lua55_printerror(lua, L);
        lua->lua_close(L);
        lua55_unload(lua);
        return 1;
    }

    /* Cleanup */
    lua->lua_close(L);
    lua55_unload(lua);
    return 0;
}
