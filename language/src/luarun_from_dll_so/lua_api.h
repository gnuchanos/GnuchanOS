/* ============================================================
 * lua_api.h — Lua C API function pointer types
 *
 * All Lua 5.x C API functions used by the luarun runner are
 * declared as function-pointer typedefs here. They are resolved
 * at runtime from lua55.dll / liblua55.so via lua_loader.
 * ============================================================ */

#ifndef LUA_API_H
#define LUA_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Lua state handle */
typedef struct lua_State lua_State;

/* Generic C-function callback type used by Lua */
typedef int (*lua_CFunction)(lua_State*);

/* ---- Core Lua API function pointers ---- */
typedef lua_State* (*luaL_newstate_t)(void);
typedef void       (*lua_close_t)(lua_State*);
typedef int        (*lua_pcallk_t)(lua_State*, int nargs, int nresults, int msgh, int ctx);
typedef int        (*luaL_loadfilex_t)(lua_State*, const char* filename, const char* mode);
typedef int        (*luaL_loadbufferx_t)(lua_State*, const char* buff, size_t sz, const char* name, const char* mode);

/* ---- Stack operations ---- */
typedef int        (*lua_gettop_t)(lua_State*);
typedef void       (*lua_settop_t)(lua_State*, int idx);
typedef const char*(*lua_tolstring_t)(lua_State*, int idx, size_t* len);
typedef int        (*lua_toboolean_t)(lua_State*, int idx);

/* ---- Library registration ---- */
typedef int        (*luaL_requiref_t)(lua_State*, const char* modname, lua_CFunction openf, int glb);

/* ---- luaopen_* library init functions ---- */
/* Each is just a lua_CFunction; typedef for clarity */
typedef lua_CFunction luaopen_lib_t;

/* ---- Struct holding all resolved function pointers ---- */
typedef struct LuaAPI {
    luaL_newstate_t         luaL_newstate;
    lua_close_t             lua_close;
    lua_pcallk_t            lua_pcallk;
    luaL_loadfilex_t        luaL_loadfilex;
    luaL_loadbufferx_t      luaL_loadbufferx;
    lua_gettop_t            lua_gettop;
    lua_settop_t            lua_settop;
    lua_tolstring_t         lua_tolstring;
    lua_toboolean_t         lua_toboolean;
    luaL_requiref_t         luaL_requiref;

    /* Standard library open functions */
    lua_CFunction           luaopen_base;
    lua_CFunction           luaopen_table;
    lua_CFunction           luaopen_string;
    lua_CFunction           luaopen_math;
    lua_CFunction           luaopen_io;
    lua_CFunction           luaopen_os;
    lua_CFunction           luaopen_debug;
    lua_CFunction           luaopen_coroutine;
    lua_CFunction           luaopen_package;
    lua_CFunction           luaopen_utf8;
} LuaAPI;

#ifdef __cplusplus
}
#endif

#endif /* LUA_API_H */
