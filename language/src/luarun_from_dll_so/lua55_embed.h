/* ============================================================
 * lua55_embed.h — High-level embed API for lua55.dll/so
 *
 * Provides a single-header convenience interface wrapping
 * lua_loader and lua_runner. Used by flag_luarun.c and
 * test_lua_embed.c.
 *
 * Functions:
 *   lua55_load(path)          → Lua55Embed*  (load DLL, resolve API)
 *   lua55_newstate(embed)     → lua_State*   (create Lua state)
 *   lua55_openlibs(embed, L)  → void         (open std libs)
 *   lua55_dofile(embed, L, p) → int          (run .lua file)
 *   lua55_printerror(embed,L) → void         (print error msg)
 *   lua55_unload(embed)       → void         (close state + unload DLL)
 * ============================================================ */

#ifndef LUA55_EMBED_H
#define LUA55_EMBED_H

#include "lua_api.h"
#include "lua_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lua 5.x status codes (not from headers since we load at runtime) */
#define LUA_OK       0
#define LUA_YIELD    1
#define LUA_ERRRUN   2
#define LUA_ERRSYNTAX 3
#define LUA_ERRMEM   4
#define LUA_ERRERR   5

/* ---- Opaque struct holding loaded DLL handle + resolved API ---- */
typedef struct Lua55Embed {
    LIB_HANDLE lib;               /* HMODULE (Win) or void* (Linux)  */
    LuaAPI     api;               /* resolved function pointers       */

    /* Convenience pointers into api (avoids embed->api.lua_close) */
    luaL_newstate_t luaL_newstate;
    lua_close_t     lua_close;
    lua_pcallk_t    lua_pcallk;
    luaL_loadfilex_t luaL_loadfilex;
    luaL_requiref_t luaL_requiref;
    lua_gettop_t    lua_gettop;
    lua_settop_t    lua_settop;
    lua_tolstring_t lua_tolstring;
} Lua55Embed;

/* ---- Lifecycle ---- */

/**
 * lua55_load - Load lua55.dll/so and resolve all Lua C API symbols.
 * @dll_path: filesystem path to the shared library.
 * Returns a heap-allocated Lua55Embed* on success, NULL on failure.
 */
Lua55Embed* lua55_load(const char *dll_path);

/**
 * lua55_newstate - Create a new Lua state via luaL_newstate.
 * @embed: initialized Lua55Embed from lua55_load.
 * Returns a valid lua_State* or NULL on failure.
 */
lua_State* lua55_newstate(Lua55Embed *embed);

/**
 * lua55_openlibs - Open all standard Lua libraries on the state.
 * @embed: initialized Lua55Embed.
 * @L:     lua_State returned by lua55_newstate.
 * Returns 0 on success, -1 on failure.
 */
int lua55_openlibs(Lua55Embed *embed, lua_State *L);

/**
 * lua55_dofile - Load and execute a .lua file.
 * @embed: initialized Lua55Embed.
 * @L:     lua_State from lua55_newstate.
 * @path:  path to the .lua script.
 * Returns LUA_OK (0) on success, non-zero on failure.
 */
int lua55_dofile(Lua55Embed *embed, lua_State *L, const char *path);

/**
 * lua55_printerror - Print the Lua error string on top of the stack.
 * @embed: initialized Lua55Embed.
 * @L:     lua_State with an error on the stack.
 */
void lua55_printerror(Lua55Embed *embed, lua_State *L);

/**
 * lua55_unload - Close Lua state (if open) and unload the DLL.
 * @embed: Lua55Embed to tear down (is freed, do not reuse).
 */
void lua55_unload(Lua55Embed *embed);

#ifdef __cplusplus
}
#endif

#endif /* LUA55_EMBED_H */
