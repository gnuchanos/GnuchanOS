/* ============================================================
 * run_lua.h — Lua script runner entry point
 *
 * Called by flag_luarun.c after argument parsing.  Delegates
 * to lua55_embed (which wraps lua_loader + lua_runner).
 * ============================================================ */

#ifndef RUN_LUA_H
#define RUN_LUA_H

/**
 * run_lua - Load lua55.dll/so, create state, open libs, run script.
 * @script:  path to the .lua file to execute.
 * @dll_path: path to lua55.dll or liblua55.so.
 *
 * Returns 0 on success, 1 on failure (error printed to stderr).
 */
int run_lua(const char *script, const char *dll_path);

#endif /* RUN_LUA_H */
