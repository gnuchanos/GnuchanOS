/*
 * gcl_lua_embed.c — Lua 5.4.7 embed module (.gcDL output)
 *
 * The Lua runtime + interpreter are embedded. gcdl_loader loads this module
 * at run time and calls the gcdl_lua_run symbol.
 *
 * Build:
 *   gcc -shared gcl_lua_embed.c + lua-5.4.7/src/*.c  ->  lua.gcDL
 */

#define _CRT_SECURE_NO_WARNINGS

/* Needed for glibc to expose readlink under strict -std=c11.
 * Must appear BEFORE the first include: features.h processes this flag. */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define GCL_MODULE_EXPORT __declspec(dllexport)
#else
#define GCL_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* bridge.gcDL degerleri icin maksimum boyut (gcdl_bridge.c ile ayni) */
#define GCL_BRIDGE_MAX_VALUE (1024 * 1024)

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>      /* readlink(): exe path (Linux/macOS) */
#endif

/* raylib bridge struct: lua.gcDL fills it, lua_raylib.gcDL calls it. */
typedef struct GclLua GclLua;
typedef struct lua_State lua_State;
typedef struct luaL_Reg luaL_Reg;
typedef long long lua_Integer;
typedef double lua_Number;

typedef struct GclLua {
    void (*createtable)(lua_State *L, int narr, int nrec);
    void (*setfuncs)(lua_State *L, const luaL_Reg *l, int nup);
    const char *(*pushstring)(lua_State *L, const char *s);
    void (*pushinteger)(lua_State *L, lua_Integer n);
    void (*pushnumber)(lua_State *L, lua_Number n);
    void (*pushboolean)(lua_State *L, int b);
    void (*pushcfunction)(lua_State *L, int (*f)(lua_State *));
    void (*pop)(lua_State *L, int n);
    int (*getfield)(lua_State *L, int idx, const char *k);
    int (*rawgeti)(lua_State *L, int idx, lua_Integer n);
    void (*setfield)(lua_State *L, int idx, const char *k);
    void (*settable)(lua_State *L, int idx);
    void (*rawseti)(lua_State *L, int idx, lua_Integer n);
    void (*call)(lua_State *L, int nargs, int nresults);
    lua_Integer (*Lcheckinteger)(lua_State *L, int arg);
    lua_Integer (*Loptinteger)(lua_State *L, int arg, lua_Integer d);
    lua_Number (*Lchecknumber)(lua_State *L, int arg);
    lua_Number (*Loptnumber)(lua_State *L, int arg, lua_Number d);
    const char *(*Lcheckstring)(lua_State *L, int arg);
    void (*Lchecktype)(lua_State *L, int arg, int t);
} GclLua;

/* lua_pushcfunction / lua_pop / lua_call / luaL_checkstring are macros; their
 * addresses cannot be taken — so we wrap them in real functions: */
static void bridge_pushcfunction(lua_State *L, int (*f)(lua_State *)) {
    lua_pushcfunction(L, f);
}
static void bridge_pop(lua_State *L, int n) {
    lua_pop(L, n);
}
static void bridge_call(lua_State *L, int nargs, int nresults) {
    lua_call(L, nargs, nresults);
}
static const char *bridge_checkstring(lua_State *L, int arg) {
    return luaL_checklstring(L, arg, NULL);
}

/* lua_raylib.gcDL export: takes bridge + state, builds the gcl.raylib table. */
typedef void (*gcdl_raylib_attach_fn)(lua_State *, const GclLua *);

/* STATIC bridge: R->... calls from the raylib functions stay valid at all
 * times. If defined on the stack, R would dangle after gcdl_setup_raylib
 * returns. */
static GclLua bridge;

/* Loads lua_raylib.gcDL from Library/Lua/ next to the exe and sets up
 * gcl.raylib. On failure writes an explanatory message to err; the gcl table
 * is still created empty. */
static void gcdl_setup_raylib(lua_State *L, char *err, size_t err_cap) {
    char exe_path[4096];
    char full[4200];
    const char *sep = NULL;
    void *handle = NULL;
    gcdl_raylib_attach_fn attach = NULL;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
    if (n == 0 || n >= (DWORD)sizeof exe_path) return;
#else
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
    if (n <= 0 || n >= (ssize_t)sizeof exe_path) return;
    exe_path[n] = '\0';
#endif

    sep = strrchr(exe_path, '/');
    {
        const char *bs = strrchr(exe_path, '\\');
        if (bs && (sep == NULL || bs > sep)) sep = bs;
    }
    if (sep == NULL) return;
    {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        static const char raypath[] = "Library/Lua/lua_raylib.gcDL";
        if (dir_len + sizeof raypath >= sizeof full) return;
        memcpy(full, exe_path, dir_len);
        memcpy(full + dir_len, raypath, sizeof raypath);
    }

#ifdef _WIN32
    handle = (void *)LoadLibraryA(full);
#else
    handle = dlopen(full, RTLD_NOW | RTLD_GLOBAL);
#endif
    if (handle == NULL) {
        if (err != NULL && err_cap > 0) {
#ifdef _WIN32
            snprintf(err, err_cap, "failed to load lua_raylib.gcDL: %s (code %lu)", full,
                     (unsigned long)GetLastError());
#else
            snprintf(err, err_cap, "failed to load lua_raylib.gcDL: %s (%s)", full,
                     dlerror() ? dlerror() : "unknown error");
#endif
        }
        /* gcl = {} — a script that uses gcl.raylib gets a clear Lua error */
        lua_createtable(L, 0, 0);
        lua_setglobal(L, "gcl");
        return;
    }

#ifdef _WIN32
    attach = (gcdl_raylib_attach_fn)GetProcAddress((HMODULE)handle, "gcdl_raylib_attach");
#else
    attach = (gcdl_raylib_attach_fn)dlsym(handle, "gcdl_raylib_attach");
#endif
    if (attach == NULL) {
        if (err != NULL && err_cap > 0)
            snprintf(err, err_cap, "symbol gcdl_raylib_attach not found: %s", full);
        /* gcl = {} — a script that uses gcl.raylib gets a clear Lua error */
        lua_createtable(L, 0, 0);
        lua_setglobal(L, "gcl");
        return;
    }

    /* Fill the Lua API pointers: the bind does not carry its own Lua copy */
    bridge.createtable   = (void (*)(lua_State *, int, int))lua_createtable;
    bridge.setfuncs      = (void (*)(lua_State *, const luaL_Reg *, int))luaL_setfuncs;
    bridge.pushstring    = (const char *(*)(lua_State *, const char *))lua_pushstring;
    bridge.pushinteger   = (void (*)(lua_State *, lua_Integer))lua_pushinteger;
    bridge.pushnumber    = (void (*)(lua_State *, lua_Number))lua_pushnumber;
    bridge.pushboolean   = (void (*)(lua_State *, int))lua_pushboolean;
    bridge.pushcfunction = bridge_pushcfunction;
    bridge.pop           = bridge_pop;
    bridge.getfield      = (int (*)(lua_State *, int, const char *))lua_getfield;
    bridge.rawgeti       = (int (*)(lua_State *, int, lua_Integer))lua_rawgeti;
    bridge.setfield      = (void (*)(lua_State *, int, const char *))lua_setfield;
    bridge.settable      = (void (*)(lua_State *, int))lua_settable;
    bridge.rawseti       = (void (*)(lua_State *, int, lua_Integer))lua_rawseti;
    bridge.call          = bridge_call;
    bridge.Lcheckinteger = (lua_Integer (*)(lua_State *, int))luaL_checkinteger;
    bridge.Loptinteger   = (lua_Integer (*)(lua_State *, int, lua_Integer))luaL_optinteger;
    bridge.Lchecknumber  = (lua_Number (*)(lua_State *, int))luaL_checknumber;
    bridge.Loptnumber    = (lua_Number (*)(lua_State *, int, lua_Number))luaL_optnumber;
    bridge.Lcheckstring  = bridge_checkstring;
    bridge.Lchecktype    = (void (*)(lua_State *, int, int))luaL_checktype;

    /* attach(): puts the gcl.raylib table on top of the stack */
    attach(L, &bridge);

    /* global variable: gcl = { raylib = <table> }
     * note: the script says "gcl.raylib" — it expects a global gcl, not registry. */
    lua_createtable(L, 0, 1);            /* gcl_table */
    lua_pushvalue(L, -2);                /* gcl_table raylib_table */
    lua_setfield(L, -2, "raylib");       /* gcl_table.raylib = raylib_table */
    lua_setglobal(L, "gcl");             /* _G.gcl = gcl_table */
    lua_pop(L, 1);                       /* raylib_table */

    /* handle: kept open for the lifetime of the Lua state (intentionally not closed) */
}

/* ---- cross-language bridge (gcl.bridge) ----
 * bridge.gcDL (Library/bridge/bridge.gcDL next to the exe) provides a
 * filesystem key->value store shared between Lua and Python processes.
 * We load it here and expose it as gcl.bridge: open/get/set/delete/list. */

typedef int (*bridge_open_fn)(const char *);
typedef int (*bridge_set_fn)(const char *, const char *);
typedef int (*bridge_get_fn)(const char *, char *, size_t);
typedef int (*bridge_delete_fn)(const char *);
typedef int (*bridge_list_fn)(char *, size_t);

static bridge_open_fn   g_bridge_open = NULL;
static bridge_set_fn    g_bridge_set = NULL;
static bridge_get_fn    g_bridge_get = NULL;
static bridge_delete_fn g_bridge_delete = NULL;
static bridge_list_fn   g_bridge_list = NULL;

static int lua_bridge_open(lua_State *L) {
    const char *session = luaL_optstring(L, 1, NULL);
    if (g_bridge_open == NULL) return luaL_error(L, "gcl.bridge: bridge.gcDL not loaded");
    lua_pushinteger(L, g_bridge_open(session));
    return 1;
}
static int lua_bridge_set(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    const char *value = luaL_checkstring(L, 2);
    if (g_bridge_set == NULL) return luaL_error(L, "gcl.bridge: bridge.gcDL not loaded");
    lua_pushinteger(L, g_bridge_set(key, value));
    return 1;
}
static int lua_bridge_get(lua_State *L) {
    char buf[GCL_BRIDGE_MAX_VALUE + 1];
    int rc;
    const char *key = luaL_checkstring(L, 1);
    if (g_bridge_get == NULL) return luaL_error(L, "gcl.bridge: bridge.gcDL not loaded");
    rc = g_bridge_get(key, buf, sizeof buf);
    if (rc != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
static int lua_bridge_delete(lua_State *L) {
    const char *key = luaL_checkstring(L, 1);
    if (g_bridge_delete == NULL) return luaL_error(L, "gcl.bridge: bridge.gcDL not loaded");
    lua_pushinteger(L, g_bridge_delete(key));
    return 1;
}
static int lua_bridge_list(lua_State *L) {
    char buf[4096];
    if (g_bridge_list == NULL) return luaL_error(L, "gcl.bridge: bridge.gcDL not loaded");
    g_bridge_list(buf, sizeof buf);
    lua_pushstring(L, buf);
    return 1;
}

static void lua_bridge_setup(lua_State *L, char *err, size_t err_cap) {
    char exe_path[4096];
    char full[4200];
    const char *sep = NULL;
    void *handle = NULL;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

#ifdef _WIN32
    {
        DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
        if (n == 0 || n >= (DWORD)sizeof exe_path) return;
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof exe_path - 1);
        if (n <= 0 || n >= (ssize_t)sizeof exe_path) return;
        exe_path[n] = '\0';
    }
#endif

    sep = strrchr(exe_path, '/');
    {
        const char *bs = strrchr(exe_path, '\\');
        if (bs && (sep == NULL || bs > sep)) sep = bs;
    }
    if (sep == NULL) return;
    {
        size_t dir_len = (size_t)(sep - exe_path) + 1;
        static const char bpath[] = "Library/bridge/bridge.gcDL";
        if (dir_len + sizeof bpath >= sizeof full) return;
        memcpy(full, exe_path, dir_len);
        memcpy(full + dir_len, bpath, sizeof bpath);
    }

#ifdef _WIN32
    handle = (void *)LoadLibraryA(full);
#else
    handle = dlopen(full, RTLD_NOW | RTLD_GLOBAL);
#endif
    if (handle == NULL) {
        if (err != NULL && err_cap > 0)
            snprintf(err, err_cap, "bridge.gcDL yuklenemedi: %s", full);
        return;
    }

#ifdef _WIN32
    g_bridge_open = (bridge_open_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_open");
    g_bridge_set = (bridge_set_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_set");
    g_bridge_get = (bridge_get_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_get");
    g_bridge_delete = (bridge_delete_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_delete");
    g_bridge_list = (bridge_list_fn)GetProcAddress((HMODULE)handle, "gcdl_bridge_list");
#else
    g_bridge_open = (bridge_open_fn)dlsym(handle, "gcdl_bridge_open");
    g_bridge_set = (bridge_set_fn)dlsym(handle, "gcdl_bridge_set");
    g_bridge_get = (bridge_get_fn)dlsym(handle, "gcdl_bridge_get");
    g_bridge_delete = (bridge_delete_fn)dlsym(handle, "gcdl_bridge_delete");
    g_bridge_list = (bridge_list_fn)dlsym(handle, "gcdl_bridge_list");
#endif

    if (g_bridge_open == NULL || g_bridge_set == NULL || g_bridge_get == NULL ||
        g_bridge_delete == NULL || g_bridge_list == NULL) {
        if (err != NULL && err_cap > 0)
            snprintf(err, err_cap, "bridge.gcDL symbol eksik: %s", full);
        return;
    }

    /* gcl.bridge tablosunu global gcl'ye ekle (gcl varsa) */
    {
        static const luaL_Reg bridge_lib[] = {
            {"open",   lua_bridge_open},
            {"set",    lua_bridge_set},
            {"get",    lua_bridge_get},
            {"delete", lua_bridge_delete},
            {"list",   lua_bridge_list},
            {NULL, NULL}
        };
        lua_getglobal(L, "gcl");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_createtable(L, 0, 0);
            lua_setglobal(L, "gcl");
            lua_getglobal(L, "gcl");
        }
        lua_createtable(L, 0, 5);
        luaL_setfuncs(L, bridge_lib, 0);
        lua_setfield(L, -2, "bridge");
        lua_pop(L, 1);
    }
    /* handle: acik kalir */
}

/* Runs a Lua script.
 *   script  : file path
 *   debug   : 0/1 — on error print a traceback
 *   err     : error text buffer (empty means success)
 *   err_cap : buffer size
 * Returns: 0 on success, 1 on error.
 */
/* Embedded Lua version (gcl -luarun -version). */
GCL_MODULE_EXPORT const char *gcdl_lua_version(void) {
    return LUA_RELEASE;
}

GCL_MODULE_EXPORT int gcdl_lua_run(const char *script, int debug,
                                   char *err, size_t err_cap) {
    lua_State *L;
    int rc = 0;

    if (script == NULL || script[0] == '\0') {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "script path is empty");
        return 1;
    }

    L = luaL_newstate();
    if (L == NULL) {
        if (err && err_cap > 0)
            snprintf(err, err_cap, "cannot create Lua state");
        return 1;
    }
    luaL_openlibs(L);

    /* Auto-attach gcl.raylib (if Library/Lua/lua_raylib.gcDL exists);
     * on failure print the reason to stderr (e.g. lua_raylib.gcDL missing). */
    {
        char raylib_err[512];
        gcdl_setup_raylib(L, raylib_err, sizeof raylib_err);
        if (raylib_err[0] != '\0')
            fprintf(stderr, "gcl: error: gcl.raylib setup failed — %s\n", raylib_err);
    }

    /* Auto-attach gcl.bridge (cross-language data bridge to Python);
     * Lua scripti gcl.bridge.get/set/... kullanabilir. */
    {
        char bridge_err[512];
        lua_bridge_setup(L, bridge_err, sizeof bridge_err);
        if (bridge_err[0] != '\0')
            fprintf(stderr, "gcl: error: gcl.bridge setup failed — %s\n", bridge_err);
    }

    if (luaL_dofile(L, script) != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL)
            msg = "unknown Lua error";
        if (debug) {
            luaL_traceback(L, L, msg, 1);
            msg = lua_tostring(L, -1);
            if (msg == NULL)
                msg = "unknown Lua error";
        }
        if (err && err_cap > 0)
            snprintf(err, err_cap, "%s", msg);
        rc = 1;
    }

    lua_close(L);
    return rc;
}
