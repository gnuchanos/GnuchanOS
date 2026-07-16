/* ============================================================
 * lua_runner.h — Lua state creation, library loading, script execution
 *
 * Wraps the resolved LuaAPI to provide a clean interface for:
 *   - create_lua_state()      → create a new Lua state
 *   - open_lua_libraries()    → open standard Lua libraries
 *   - run_lua_script()        → load and execute a .lua file
 *   - close_lua_state()       → destroy Lua state
 * ============================================================ */

#ifndef LUA_RUNNER_H
#define LUA_RUNNER_H

#include "lua_api.h"

/**
 * create_lua_state - Create a new Lua state via luaL_newstate.
 * Returns a valid lua_State*, or NULL on failure.
 */
lua_State* create_lua_state(const LuaAPI *api);

/**
 * open_lua_libraries - Open all standard Lua libraries on the state.
 * Prints a summary to stdout.
 * Returns 0 on success, -1 on failure.
 */
int open_lua_libraries(const LuaAPI *api, lua_State *L);

/**
 * run_lua_script - Load and execute a .lua file.
 * path: file path of the script.
 * Returns 0 on success, 1 on error.
 */
int run_lua_script(const LuaAPI *api, lua_State *L, const char *path);

/**
 * close_lua_state - Close the Lua state (no-op if L is NULL).
 */
void close_lua_state(const LuaAPI *api, lua_State *L);

#endif /* LUA_RUNNER_H */
