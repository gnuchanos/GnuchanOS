/*
 * gcl_lua_bridge.h — Lua API runtime bridge (ortak tip/sozlesme)
 *
 * gcl_lua.gcDL ile raylib.gcDL ayri DLL'lerdir. raylib.gcDL kendi Lua
 * kopyasini TASIMAZ; Lua fonksiyonlarini gcl_lua.gcDL'den bekler.
 *
 * Derleme-zamani bagimlilik olmasin diye, gcl_lua.gcDL calisma aninda
 * GclLua yapisini gercek lua_* / luaL_* fonksiyon adresleriyle doldurur
 * ve raylib.gcDL'deki gcdl_raylib_attach()'a verir. Bind kodu bu yapidan
 * cagirir. Boylece raylib.gcDL icinde hicbir lua_* sembolu yoktur.
 */

#ifndef GCL_LUA_BRIDGE_H
#define GCL_LUA_BRIDGE_H

#include <stddef.h>

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);
typedef long long lua_Integer;
typedef double lua_Number;

/* Bind kendi lua.h'ini include etmez; gereken tek sabit: */
#ifndef LUA_TTABLE
#define LUA_TTABLE 5
#endif

typedef struct luaL_Reg {
    const char *name;
    lua_CFunction func;
} luaL_Reg;

/* Bind'in kullandigi Lua API alt kumesi. */
typedef struct GclLua {
    void (*createtable)(lua_State *L, int narr, int nrec);
    void (*setfuncs)(lua_State *L, const luaL_Reg *l, int nup);
    const char *(*pushstring)(lua_State *L, const char *s);        /* lua_pushstring int degil */
    void (*pushinteger)(lua_State *L, lua_Integer n);
    void (*pushnumber)(lua_State *L, lua_Number n);
    void (*pushboolean)(lua_State *L, int b);
    void (*pushcfunction)(lua_State *L, lua_CFunction f);          /* wrapper: lua_pushcclosure */
    void (*pop)(lua_State *L, int n);                              /* wrapper: lua_settop */
    int (*getfield)(lua_State *L, int idx, const char *k);         /* lua_getfield int dondurur */
    int (*rawgeti)(lua_State *L, int idx, lua_Integer n);          /* lua_rawgeti int dondurur */
    void (*setfield)(lua_State *L, int idx, const char *k);
    void (*settable)(lua_State *L, int idx);
    void (*rawseti)(lua_State *L, int idx, lua_Integer n);
    void (*call)(lua_State *L, int nargs, int nresults);           /* wrapper: lua_callk */
    lua_Integer (*Lcheckinteger)(lua_State *L, int arg);
    lua_Integer (*Loptinteger)(lua_State *L, int arg, lua_Integer d);
    lua_Number (*Lchecknumber)(lua_State *L, int arg);
    lua_Number (*Loptnumber)(lua_State *L, int arg, lua_Number d);
    const char *(*Lcheckstring)(lua_State *L, int arg);            /* wrapper: luaL_checklstring */
    void (*Lchecktype)(lua_State *L, int arg, int t);
} GclLua;

#endif /* GCL_LUA_BRIDGE_H */
