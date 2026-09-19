/*
 * gcl_embed_lua.c — GCL Embed Lua backend.
 *
 * Lua 5.4 embed (lua.org) ile çalışır. simple_doc.md:
 *   Embed.Run("lua")   → luaL_newstate + luaL_openlibs
 *   Embed.Stop("lua")  → lua_close
 *   Embed.GetValue("lua", value) → global oku
 *   Embed.SendValue("lua", value) → global yaz
 */

#include "gcl_embed_lua.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gcl_shared_state.h"

static lua_State *g_L = NULL;

/* isim geçerli Lua identifier mı? */
static int is_valid_identifier(const char *name) {
    if (!name || !name[0]) return 0;
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return 0;
    for (const char *p = name + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) return 0;
    }
    return 1;
}

int gcl_lua_runtime_init(void) {
    if (g_L) return 1;
    g_L = luaL_newstate();
    if (!g_L) return 0;
    luaL_openlibs(g_L);
    return 1;
}

void gcl_lua_runtime_shutdown(void) {
    if (!g_L) return;
    lua_close(g_L);
    g_L = NULL;
}

int gcl_lua_runtime_is_active(void) {
    return g_L != NULL;
}

int gcl_lua_run_simple(const char *code) {
    if (!g_L) return 0;
    if (!code) return 0;
    if (luaL_dostring(g_L, code) != LUA_OK) {
        const char *err = lua_tostring(g_L, -1);
        fprintf(stderr, "Lua error: %s\n", err ? err : "unknown");
        lua_pop(g_L, 1);
        return 0;
    }
    return 1;
}

int gcl_lua_set_global(const char *name, const char *value) {
    if (!g_L) return 0;
    if (!is_valid_identifier(name)) return 0;
    if (!value) return 0;

    /* name = value; — kaynak string olarak değerlendir */
    size_t n = strlen(name) + strlen(value) + 8;
    char *code = (char *)malloc(n);
    if (!code) return 0;
    snprintf(code, n, "%s = %s", name, value);

    int ok = gcl_lua_run_simple(code);
    free(code);
    return ok;
}

int gcl_lua_get_global(const char *name, char *out, size_t out_size) {
    if (!g_L) return 0;
    if (!is_valid_identifier(name)) return 0;
    if (!out || out_size == 0) return 0;

    lua_getglobal(g_L, name);
    int ok = 0;

    if (lua_isnumber(g_L, -1)) {
        double v = lua_tonumber(g_L, -1);
        snprintf(out, out_size, "%g", v);
        ok = 1;
    } else if (lua_isstring(g_L, -1)) {
        size_t len = 0;
        const char *s = lua_tolstring(g_L, -1, &len);
        if (s) {
            size_t n = len < out_size - 1 ? len : out_size - 1;
            memcpy(out, s, n);
            out[n] = '\0';
            ok = 1;
        }
    } else if (lua_isboolean(g_L, -1)) {
        snprintf(out, out_size, "%s", lua_toboolean(g_L, -1) ? "1" : "0");
        ok = 1;
    }

    lua_pop(g_L, 1);
    return ok;
}

/* ---------- gcl table kurulumu (gcl.init / gcl.get_state / gcl.set_state) ---------- */
static int lua_gcl_init(lua_State *L) {
    (void)L;
    return 0;
}

static int lua_gcl_get_state(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    char buf[1024] = { 0 };
    if (gcl_shared_get(key, buf, sizeof(buf))) {
        /* Paylaşılan state sayısal bir değer ise number olarak push et.
           Böylece gcl.get_state("ball_x") sonucu string değil number olur
           ve Lua'da doğrudan aritmetik/karşılaştırma yapılabilir. */
        char *end = NULL;
        double d = strtod(buf, &end);
        if (end && end != buf && *end == '\0') {
            lua_pushnumber(L, d);
        } else {
            lua_pushstring(L, buf);
        }
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int lua_gcl_set_state(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    /* Hem sayı hem string kabul et — shared state string olarak saklanır */
    const char *val = NULL;
    if (lua_isnumber(L, 2)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", (double)lua_tonumber(L, 2));
        val = buf;
        gcl_shared_set(key, val);
    } else if (lua_isstring(L, 2)) {
        val = lua_tostring(L, 2);
        if (val) gcl_shared_set(key, val);
    }
    return 0;
}

/* Lua binding modülleri (gcl_luaraylib.c / gcl_luaraygui.c'den) */
extern int luaopen_LuaRaylib(lua_State *L);
extern int luaopen_LuaRaygui(lua_State *L);

void *gcl_lua_get_state(void) {
    return (void *)g_L;
}

int gcl_lua_run_file(const char *path) {
    if (!gcl_lua_runtime_init()) {
        fprintf(stderr, "Error: Lua runtime init failed\n");
        return 1;
    }
    lua_State *L = (lua_State *)gcl_lua_get_state();
    if (!L) return 1;

    /* raylib / raygui binding'lerini yükle */
    luaopen_LuaRaylib(L);
    luaopen_LuaRaygui(L);

    /* gcl.init() / gcl.get_state / gcl.set_state */
    lua_newtable(L);
    lua_pushcfunction(L, lua_gcl_init);
    lua_setfield(L, -2, "init");
    lua_pushcfunction(L, lua_gcl_get_state);
    lua_setfield(L, -2, "get_state");
    lua_pushcfunction(L, lua_gcl_set_state);
    lua_setfield(L, -2, "set_state");
    lua_setglobal(L, "gcl");

    if (luaL_dofile(L, path) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        fprintf(stderr, "Lua error: %s\n", err ? err : "unknown");
        lua_pop(L, 1);
        gcl_shared_cleanup();
        return 1;
    }
    gcl_shared_cleanup();
    return 0;
}
