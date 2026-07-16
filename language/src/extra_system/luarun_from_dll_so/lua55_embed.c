/* ============================================================
 * lua55_embed.c — Implementation of the high-level embed API
 *
 * Wraps lua_loader (load/resolve) and lua_runner (state, libs,
 * script execution) into a single Lua55Embed struct for easy
 * use by flag_luarun.c and test_lua_embed.c.
 * ============================================================ */

#include "lua55_embed.h"
#include "lua_loader.h"
#include "lua_runner.h"
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* lua55_load — load DLL, resolve API, populate embed struct           */
/* ------------------------------------------------------------------ */
Lua55Embed* lua55_load(const char *dll_path) {
    if (!dll_path) {
        fprintf(stderr, "Error: lua55_load: NULL dll_path\n");
        return NULL;
    }

    Lua55Embed *embed = (Lua55Embed*)calloc(1, sizeof(Lua55Embed));
    if (!embed) {
        fprintf(stderr, "Error: lua55_load: out of memory\n");
        return NULL;
    }

    /* Load the library */
    const char *search[] = { dll_path, NULL };
    if (load_lua_library(search, &embed->lib, NULL) != 0) {
        fprintf(stderr, "Error: lua55_load: failed to load '%s'\n", dll_path);
        free(embed);
        return NULL;
    }

    /* Resolve all Lua API symbols */
    if (resolve_lua_api(embed->lib, &embed->api) != 0) {
        fprintf(stderr, "Error: lua55_load: symbol resolution failed\n");
        unload_lua_library(embed->lib);
        free(embed);
        return NULL;
    }

    /* Populate convenience pointers */
    embed->luaL_newstate  = embed->api.luaL_newstate;
    embed->lua_close      = embed->api.lua_close;
    embed->lua_pcallk     = embed->api.lua_pcallk;
    embed->luaL_loadfilex = embed->api.luaL_loadfilex;
    embed->luaL_requiref  = embed->api.luaL_requiref;
    embed->lua_gettop     = embed->api.lua_gettop;
    embed->lua_settop     = embed->api.lua_settop;
    embed->lua_tolstring  = embed->api.lua_tolstring;

    return embed;
}

/* ------------------------------------------------------------------ */
/* lua55_newstate — create a new Lua state                             */
/* ------------------------------------------------------------------ */
lua_State* lua55_newstate(Lua55Embed *embed) {
    return create_lua_state(&embed->api);
}

/* ------------------------------------------------------------------ */
/* lua55_openlibs — open all standard Lua libraries                    */
/* ------------------------------------------------------------------ */
int lua55_openlibs(Lua55Embed *embed, lua_State *L) {
    return open_lua_libraries(&embed->api, L);
}

/* ------------------------------------------------------------------ */
/* lua55_dofile — load and run a .lua file                             */
/* ------------------------------------------------------------------ */
int lua55_dofile(Lua55Embed *embed, lua_State *L, const char *path) {
    return run_lua_script(&embed->api, L, path);
}

/* ------------------------------------------------------------------ */
/* lua55_printerror — print the error string on top of the Lua stack   */
/* ------------------------------------------------------------------ */
void lua55_printerror(Lua55Embed *embed, lua_State *L) {
    (void)embed;
    if (!L) return;
    const char *err = NULL;
    if (embed->api.lua_tolstring)
        err = embed->api.lua_tolstring(L, -1, NULL);
    fprintf(stderr, "%s\n", err ? err : "(unknown error)");
}

/* ------------------------------------------------------------------ */
/* lua55_unload — close Lua state, unload DLL, free embed              */
/* ------------------------------------------------------------------ */
void lua55_unload(Lua55Embed *embed) {
    if (!embed) return;
    if (embed->lib) {
        unload_lua_library(embed->lib);
        embed->lib = NULL;
    }
    free(embed);
}
