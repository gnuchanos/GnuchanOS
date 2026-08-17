/*
 * gcl_raylib_bind.c — Lua/raylib binding (gcDL modulu: raylib.gcDL)
 *
 * gcl.raylib tablosunu Lua state'ine kurar:
 *   local rl = gcl.raylib
 *   rl.InitWindow(800, 450, "baslik")
 *   rl.DrawRectangle(50, 60, 200, 80, rl.SKYBLUE)
 *   rl.DrawTriangle(rl.Vector2(600,200), ...)
 *   rl.raylib_version -> "6.1-dev"
 *
 * Lua odakli API kumesi:
 *   - Pencere/ekran:   InitWindow, CloseWindow, WindowShouldClose, GetScreenWidth/Height,
 *                      SetExitKey, SetTargetFPS, SetConfigFlags, SetWindowTitle/Size,
 *                      ToggleFullscreen, TakeScreenshot, SetTraceLogLevel,
 *                      SetWindowMinSize/MaxSize/Position/Opacity, Maximize/Minimize/Restore
 *   - Cizim:           Begin/EndDrawing, ClearBackground, FPS/metin, dikdortgen, cember,
 *                      gradyan, sector, ring, poligon, elips, cizgi, ucgen, pixel
 *   - Metin:           DrawText, DrawFPS, GetFPS, GetFrameTime
 *   - Girdi klavye:    IsKeyDown/Pressed/Released/Up, GetKeyPressed, GetCharPressed
 *   - Girdi fare:      IsMouseButtonDown/Pressed/Released/Up, GetMouseX/Y/Position/WheelMove,
 *                      GetMouseDelta, SetMousePosition/Scale
 *   - Texture:         LoadTexture, UnloadTexture, DrawTexture, DrawTextureEx
 *   - Yapici/sabit:    Vector2, Vector3, Rectangle, Color + KEY_/MOUSE_/FLAG_ sabitleri
 *
 * Bu modul kendi Lua kopyasini TASIMAZ: Lua API fonksiyonlarini,
 * gcl_lua.gcDL tarafindan doldurulan GclLua bridge yapisindan cagirir.
 */

#define _CRT_SECURE_NO_WARNINGS

#if defined(_WIN32)
#define GCL_MODULE_EXPORT __declspec(dllexport)
#else
#define GCL_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcl_lua_bridge.h"        /* GclLua yapisi — Lua API runtime bridge */
#include "raylib.h"                /* makefile -I ile verir (_temp/raylib/src) */

static const GclLua *R;            /* runtime Lua API */

/* ── Lua API makrolari (bridge uzerinden) ── */
#define lua_createtable    R->createtable
#define luaL_setfuncs      R->setfuncs
#define lua_pushstring     R->pushstring
#define lua_pushinteger    R->pushinteger
#define lua_pushnumber     R->pushnumber
#define lua_pushboolean    R->pushboolean
#define lua_pushcfunction  R->pushcfunction
#define lua_pop            R->pop
#define lua_getfield       R->getfield
#define lua_rawgeti        R->rawgeti
#define lua_setfield       R->setfield
#define lua_settable       R->settable
#define lua_rawseti        R->rawseti
#define lua_call           R->call
#define luaL_checkinteger  R->Lcheckinteger
#define luaL_optinteger    R->Loptinteger
#define luaL_checknumber   R->Lchecknumber
#define luaL_optnumber     R->Loptnumber
#define luaL_checkstring   R->Lcheckstring
#define luaL_checktype     R->Lchecktype

/* ── tabloya tam sayi sabiti ekle (bridge'de rawset yok — settable yeterli) ── */
#define RL_INT_CONST(L, name, value) do { \
    lua_pushstring((L), (name)); \
    lua_pushinteger((L), (long long)(value)); \
    lua_settable((L), -3); \
} while (0)

/* ── renk: tabloda name -> {r,g,b,a} dizi olarak ── */
#define RL_COLOR_TABLE(L, name, r, g, b, a) do { \
    lua_pushstring((L), (name)); \
    lua_createtable((L), 4, 0); \
    lua_pushinteger((L), (r)); lua_rawseti((L), -2, 1); \
    lua_pushinteger((L), (g)); lua_rawseti((L), -2, 2); \
    lua_pushinteger((L), (b)); lua_rawseti((L), -2, 3); \
    lua_pushinteger((L), (a)); lua_rawseti((L), -2, 4); \
    lua_settable((L), -3); \
} while (0)

/* ── yardimcilar ── */

static Color lua_check_color(lua_State *L, int idx) {
    Color c = { 0, 0, 0, 255 };
    luaL_checktype(L, idx, LUA_TTABLE);
    lua_rawgeti(L, idx, 1); c.r = (unsigned char)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 2); c.g = (unsigned char)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 3); c.b = (unsigned char)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 4); c.a = (unsigned char)luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    return c;
}

static void lua_push_color(lua_State *L, Color c) {
    lua_createtable(L, 4, 0);
    lua_pushinteger(L, c.r); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, c.g); lua_rawseti(L, -2, 2);
    lua_pushinteger(L, c.b); lua_rawseti(L, -2, 3);
    lua_pushinteger(L, c.a); lua_rawseti(L, -2, 4);
}

static Vector2 lua_check_vec2(lua_State *L, int idx) {
    Vector2 v = { 0.0f, 0.0f };
    lua_getfield(L, idx, "x"); v.x = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "y"); v.y = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    return v;
}

static void lua_push_vec2(lua_State *L, Vector2 v) {
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, v.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y); lua_setfield(L, -2, "y");
}

static Vector3 lua_check_vec3(lua_State *L, int idx) {
    Vector3 v = { 0.0f, 0.0f, 0.0f };
    lua_getfield(L, idx, "x"); v.x = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "y"); v.y = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "z"); v.z = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    return v;
}

static Rectangle lua_check_rect(lua_State *L, int idx) {
    Rectangle r = { 0.0f, 0.0f, 0.0f, 0.0f };
    lua_getfield(L, idx, "x");      r.x = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "y");      r.y = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "width");  r.width = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    lua_getfield(L, idx, "height"); r.height = (float)luaL_optnumber(L, -1, 0.0); lua_pop(L, 1);
    return r;
}

/* Texture2D: {id, width, height, mipmaps, format} dizi */
static Texture2D lua_check_texture2d(lua_State *L, int idx) {
    Texture2D t = { 0 };
    luaL_checktype(L, idx, LUA_TTABLE);
    lua_rawgeti(L, idx, 1); t.id = (unsigned int)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 2); t.width = (int)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 3); t.height = (int)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 4); t.mipmaps = (int)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    lua_rawgeti(L, idx, 5); t.format = (int)luaL_optinteger(L, -1, 0); lua_pop(L, 1);
    return t;
}

static void lua_push_texture2d(lua_State *L, Texture2D t) {
    lua_createtable(L, 5, 0);
    lua_pushinteger(L, t.id);       lua_rawseti(L, -2, 1);
    lua_pushinteger(L, t.width);    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, t.height);   lua_rawseti(L, -2, 3);
    lua_pushinteger(L, t.mipmaps);  lua_rawseti(L, -2, 4);
    lua_pushinteger(L, t.format);   lua_rawseti(L, -2, 5);
}

/* ── Pencere/ekran ── */

static int f_InitWindow(lua_State *L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    const char *title = luaL_checkstring(L, 3);
    InitWindow(w, h, title);
    return 0;
}
static int f_CloseWindow(lua_State *L) { CloseWindow(); return 0; }
static int f_WindowShouldClose(lua_State *L) { lua_pushboolean(L, WindowShouldClose()); return 1; }
static int f_IsWindowReady(lua_State *L) { lua_pushboolean(L, IsWindowReady()); return 1; }
static int f_GetScreenWidth(lua_State *L) { lua_pushinteger(L, GetScreenWidth()); return 1; }
static int f_GetScreenHeight(lua_State *L) { lua_pushinteger(L, GetScreenHeight()); return 1; }
static int f_SetExitKey(lua_State *L) { SetExitKey((int)luaL_checkinteger(L, 1)); return 0; }
static int f_SetTargetFPS(lua_State *L) { SetTargetFPS((int)luaL_checkinteger(L, 1)); return 0; }
static int f_SetConfigFlags(lua_State *L) { SetConfigFlags((unsigned int)luaL_checkinteger(L, 1)); return 0; }
static int f_SetWindowTitle(lua_State *L) { SetWindowTitle(luaL_checkstring(L, 1)); return 0; }
static int f_SetWindowSize(lua_State *L) { SetWindowSize((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int f_SetWindowMinSize(lua_State *L) { SetWindowMinSize((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int f_SetWindowMaxSize(lua_State *L) { SetWindowMaxSize((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int f_SetWindowPosition(lua_State *L) { SetWindowPosition((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int f_SetWindowOpacity(lua_State *L) { SetWindowOpacity((float)luaL_checknumber(L, 1)); return 0; }
static int f_MaximizeWindow(lua_State *L) { MaximizeWindow(); return 0; }
static int f_MinimizeWindow(lua_State *L) { MinimizeWindow(); return 0; }
static int f_RestoreWindow(lua_State *L) { RestoreWindow(); return 0; }
static int f_ToggleFullscreen(lua_State *L) { ToggleFullscreen(); return 0; }
static int f_TakeScreenshot(lua_State *L) { TakeScreenshot(luaL_checkstring(L, 1)); return 0; }
static int f_SetTraceLogLevel(lua_State *L) { SetTraceLogLevel((int)luaL_checkinteger(L, 1)); return 0; }

/* ── zaman/fps ── */

static int f_GetTime(lua_State *L) { lua_pushnumber(L, GetTime()); return 1; }
static int f_GetFrameTime(lua_State *L) { lua_pushnumber(L, GetFrameTime()); return 1; }
static int f_GetFPS(lua_State *L) { lua_pushinteger(L, GetFPS()); return 1; }

/* ── cizim ── */

static int f_BeginDrawing(lua_State *L) { BeginDrawing(); return 0; }
static int f_EndDrawing(lua_State *L) { EndDrawing(); return 0; }
static int f_ClearBackground(lua_State *L) { ClearBackground(lua_check_color(L, 1)); return 0; }

static int f_DrawRectangle(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3), h = (int)luaL_checkinteger(L, 4);
    DrawRectangle(x, y, w, h, lua_check_color(L, 5));
    return 0;
}
static int f_DrawRectangleV(lua_State *L) {
    DrawRectangleV(lua_check_vec2(L, 1), lua_check_vec2(L, 2), lua_check_color(L, 3));
    return 0;
}
static int f_DrawRectangleRec(lua_State *L) {
    DrawRectangleRec(lua_check_rect(L, 1), lua_check_color(L, 2));
    return 0;
}
static int f_DrawRectangleLines(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3), h = (int)luaL_checkinteger(L, 4);
    DrawRectangleLines(x, y, w, h, lua_check_color(L, 5));
    return 0;
}
static int f_DrawRectangleLinesEx(lua_State *L) {
    DrawRectangleLinesEx(lua_check_rect(L, 1), (float)luaL_checknumber(L, 2),
                         lua_check_color(L, 3));
    return 0;
}
static int f_DrawRectangleRounded(lua_State *L) {
    DrawRectangleRounded(lua_check_rect(L, 1), (float)luaL_checknumber(L, 2),
                         (int)luaL_checkinteger(L, 3), lua_check_color(L, 4));
    return 0;
}
static int f_DrawRectangleRoundedLines(lua_State *L) {
    DrawRectangleRoundedLines(lua_check_rect(L, 1), (float)luaL_checknumber(L, 2),
                              (int)luaL_checkinteger(L, 3), lua_check_color(L, 4));
    return 0;
}
static int f_DrawRectangleGradientV(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3), h = (int)luaL_checkinteger(L, 4);
    DrawRectangleGradientV(x, y, w, h, lua_check_color(L, 5), lua_check_color(L, 6));
    return 0;
}
static int f_DrawRectangleGradientH(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3), h = (int)luaL_checkinteger(L, 4);
    DrawRectangleGradientH(x, y, w, h, lua_check_color(L, 5), lua_check_color(L, 6));
    return 0;
}
static int f_DrawCircle(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    float r = (float)luaL_checknumber(L, 3);
    DrawCircle(x, y, r, lua_check_color(L, 4));
    return 0;
}
static int f_DrawCircleV(lua_State *L) {
    DrawCircleV(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2), lua_check_color(L, 3));
    return 0;
}
static int f_DrawCircleLines(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1), y = (int)luaL_checkinteger(L, 2);
    float r = (float)luaL_checknumber(L, 3);
    DrawCircleLines(x, y, r, lua_check_color(L, 4));
    return 0;
}
static int f_DrawCircleGradient(lua_State *L) {
    DrawCircleGradient(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2),
                       lua_check_color(L, 3), lua_check_color(L, 4));
    return 0;
}
static int f_DrawCircleSector(lua_State *L) {
    DrawCircleSector(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2),
                     (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                     (int)luaL_checkinteger(L, 5), lua_check_color(L, 6));
    return 0;
}
static int f_DrawCircleSectorLines(lua_State *L) {
    DrawCircleSectorLines(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2),
                          (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                          (int)luaL_checkinteger(L, 5), lua_check_color(L, 6));
    return 0;
}
static int f_DrawRing(lua_State *L) {
    DrawRing(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2),
             (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
             (float)luaL_checknumber(L, 5), (int)luaL_checkinteger(L, 6),
             lua_check_color(L, 7));
    return 0;
}
static int f_DrawRingLines(lua_State *L) {
    DrawRingLines(lua_check_vec2(L, 1), (float)luaL_checknumber(L, 2),
                  (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                  (float)luaL_checknumber(L, 5), (int)luaL_checkinteger(L, 6),
                  lua_check_color(L, 7));
    return 0;
}
static int f_DrawEllipse(lua_State *L) {
    int cx = (int)luaL_checkinteger(L, 1), cy = (int)luaL_checkinteger(L, 2);
    float rx = (float)luaL_checknumber(L, 3), ry = (float)luaL_checknumber(L, 4);
    DrawEllipse(cx, cy, rx, ry, lua_check_color(L, 5));
    return 0;
}
static int f_DrawLine(lua_State *L) {
    int x1 = (int)luaL_checkinteger(L, 1), y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3), y2 = (int)luaL_checkinteger(L, 4);
    DrawLine(x1, y1, x2, y2, lua_check_color(L, 5));
    return 0;
}
static int f_DrawLineV(lua_State *L) {
    DrawLineV(lua_check_vec2(L, 1), lua_check_vec2(L, 2), lua_check_color(L, 3));
    return 0;
}
static int f_DrawTriangle(lua_State *L) {
    DrawTriangle(lua_check_vec2(L, 1), lua_check_vec2(L, 2), lua_check_vec2(L, 3),
                 lua_check_color(L, 4));
    return 0;
}
static int f_DrawPoly(lua_State *L) {
    DrawPoly(lua_check_vec2(L, 1), (int)luaL_checkinteger(L, 2),
             (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
             lua_check_color(L, 5));
    return 0;
}
static int f_DrawPolyLines(lua_State *L) {
    DrawPolyLines(lua_check_vec2(L, 1), (int)luaL_checkinteger(L, 2),
                  (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                  lua_check_color(L, 5));
    return 0;
}
static int f_DrawPixel(lua_State *L) {
    DrawPixel((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), lua_check_color(L, 3));
    return 0;
}

/* ── metin ── */

static int f_DrawText(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3);
    int size = (int)luaL_checkinteger(L, 4);
    DrawText(text, x, y, size, lua_check_color(L, 5));
    return 0;
}
static int f_DrawFPS(lua_State *L) { DrawFPS((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }

/* ── girdi: klavye ── */

static int f_IsKeyDown(lua_State *L) { lua_pushboolean(L, IsKeyDown((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsKeyPressed(lua_State *L) { lua_pushboolean(L, IsKeyPressed((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsKeyReleased(lua_State *L) { lua_pushboolean(L, IsKeyReleased((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsKeyUp(lua_State *L) { lua_pushboolean(L, IsKeyUp((int)luaL_checkinteger(L, 1))); return 1; }
static int f_GetKeyPressed(lua_State *L) { lua_pushinteger(L, GetKeyPressed()); return 1; }
static int f_GetCharPressed(lua_State *L) { lua_pushinteger(L, GetCharPressed()); return 1; }

/* ── girdi: fare ── */

static int f_IsMouseButtonDown(lua_State *L) { lua_pushboolean(L, IsMouseButtonDown((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsMouseButtonPressed(lua_State *L) { lua_pushboolean(L, IsMouseButtonPressed((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsMouseButtonReleased(lua_State *L) { lua_pushboolean(L, IsMouseButtonReleased((int)luaL_checkinteger(L, 1))); return 1; }
static int f_IsMouseButtonUp(lua_State *L) { lua_pushboolean(L, IsMouseButtonUp((int)luaL_checkinteger(L, 1))); return 1; }
static int f_GetMouseX(lua_State *L) { lua_pushinteger(L, GetMouseX()); return 1; }
static int f_GetMouseY(lua_State *L) { lua_pushinteger(L, GetMouseY()); return 1; }
static int f_GetMousePosition(lua_State *L) { lua_push_vec2(L, GetMousePosition()); return 1; }
static int f_GetMouseDelta(lua_State *L) { lua_push_vec2(L, GetMouseDelta()); return 1; }
static int f_GetMouseWheelMove(lua_State *L) { lua_pushnumber(L, GetMouseWheelMove()); return 1; }
static int f_SetMousePosition(lua_State *L) { SetMousePosition((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int f_SetMouseScale(lua_State *L) { SetMouseScale((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)); return 0; }

/* ── texture ── */

static int f_LoadTexture(lua_State *L) {
    Texture2D t = LoadTexture(luaL_checkstring(L, 1));
    lua_push_texture2d(L, t);
    return 1;
}
static int f_UnloadTexture(lua_State *L) { UnloadTexture(lua_check_texture2d(L, 1)); return 0; }
static int f_DrawTexture(lua_State *L) {
    DrawTexture(lua_check_texture2d(L, 1), (int)luaL_checkinteger(L, 2),
                (int)luaL_checkinteger(L, 3), lua_check_color(L, 4));
    return 0;
}
static int f_DrawTextureEx(lua_State *L) {
    DrawTextureEx(lua_check_texture2d(L, 1), lua_check_vec2(L, 2),
                  (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                  lua_check_color(L, 5));
    return 0;
}

/* ── yapicilar ── */

static int f_Vector2(lua_State *L) {
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, (float)luaL_optnumber(L, 1, 0.0)); lua_setfield(L, -2, "x");
    lua_pushnumber(L, (float)luaL_optnumber(L, 2, 0.0)); lua_setfield(L, -2, "y");
    return 1;
}
static int f_Vector3(lua_State *L) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, (float)luaL_optnumber(L, 1, 0.0)); lua_setfield(L, -2, "x");
    lua_pushnumber(L, (float)luaL_optnumber(L, 2, 0.0)); lua_setfield(L, -2, "y");
    lua_pushnumber(L, (float)luaL_optnumber(L, 3, 0.0)); lua_setfield(L, -2, "z");
    return 1;
}
static int f_Rectangle(lua_State *L) {
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, (float)luaL_optnumber(L, 1, 0.0)); lua_setfield(L, -2, "x");
    lua_pushnumber(L, (float)luaL_optnumber(L, 2, 0.0)); lua_setfield(L, -2, "y");
    lua_pushnumber(L, (float)luaL_optnumber(L, 3, 0.0)); lua_setfield(L, -2, "width");
    lua_pushnumber(L, (float)luaL_optnumber(L, 4, 0.0)); lua_setfield(L, -2, "height");
    return 1;
}
static int f_Color(lua_State *L) {
    lua_createtable(L, 4, 0);
    lua_pushinteger(L, luaL_optinteger(L, 1, 0));   lua_rawseti(L, -2, 1);
    lua_pushinteger(L, luaL_optinteger(L, 2, 0));   lua_rawseti(L, -2, 2);
    lua_pushinteger(L, luaL_optinteger(L, 3, 0));   lua_rawseti(L, -2, 3);
    lua_pushinteger(L, luaL_optinteger(L, 4, 255)); lua_rawseti(L, -2, 4);
    return 1;
}

/* ── modul kaydi ── */

static const luaL_Reg rayfuncs[] = {
    /* pencere/ekran */
    { "InitWindow", f_InitWindow },
    { "CloseWindow", f_CloseWindow },
    { "WindowShouldClose", f_WindowShouldClose },
    { "IsWindowReady", f_IsWindowReady },
    { "GetScreenWidth", f_GetScreenWidth },
    { "GetScreenHeight", f_GetScreenHeight },
    { "SetExitKey", f_SetExitKey },
    { "SetTargetFPS", f_SetTargetFPS },
    { "SetConfigFlags", f_SetConfigFlags },
    { "SetWindowTitle", f_SetWindowTitle },
    { "SetWindowSize", f_SetWindowSize },
    { "SetWindowMinSize", f_SetWindowMinSize },
    { "SetWindowMaxSize", f_SetWindowMaxSize },
    { "SetWindowPosition", f_SetWindowPosition },
    { "SetWindowOpacity", f_SetWindowOpacity },
    { "MaximizeWindow", f_MaximizeWindow },
    { "MinimizeWindow", f_MinimizeWindow },
    { "RestoreWindow", f_RestoreWindow },
    { "ToggleFullscreen", f_ToggleFullscreen },
    { "TakeScreenshot", f_TakeScreenshot },
    { "SetTraceLogLevel", f_SetTraceLogLevel },
    /* zaman/fps */
    { "GetTime", f_GetTime },
    { "GetFrameTime", f_GetFrameTime },
    { "GetFPS", f_GetFPS },
    /* cizim */
    { "BeginDrawing", f_BeginDrawing },
    { "EndDrawing", f_EndDrawing },
    { "ClearBackground", f_ClearBackground },
    { "DrawRectangle", f_DrawRectangle },
    { "DrawRectangleV", f_DrawRectangleV },
    { "DrawRectangleRec", f_DrawRectangleRec },
    { "DrawRectangleLines", f_DrawRectangleLines },
    { "DrawRectangleLinesEx", f_DrawRectangleLinesEx },
    { "DrawRectangleRounded", f_DrawRectangleRounded },
    { "DrawRectangleRoundedLines", f_DrawRectangleRoundedLines },
    { "DrawRectangleGradientV", f_DrawRectangleGradientV },
    { "DrawRectangleGradientH", f_DrawRectangleGradientH },
    { "DrawCircle", f_DrawCircle },
    { "DrawCircleV", f_DrawCircleV },
    { "DrawCircleLines", f_DrawCircleLines },
    { "DrawCircleGradient", f_DrawCircleGradient },
    { "DrawCircleSector", f_DrawCircleSector },
    { "DrawCircleSectorLines", f_DrawCircleSectorLines },
    { "DrawRing", f_DrawRing },
    { "DrawRingLines", f_DrawRingLines },
    { "DrawEllipse", f_DrawEllipse },
    { "DrawLine", f_DrawLine },
    { "DrawLineV", f_DrawLineV },
    { "DrawTriangle", f_DrawTriangle },
    { "DrawPoly", f_DrawPoly },
    { "DrawPolyLines", f_DrawPolyLines },
    { "DrawPixel", f_DrawPixel },
    /* metin */
    { "DrawText", f_DrawText },
    { "DrawFPS", f_DrawFPS },
    /* klavye */
    { "IsKeyDown", f_IsKeyDown },
    { "IsKeyPressed", f_IsKeyPressed },
    { "IsKeyReleased", f_IsKeyReleased },
    { "IsKeyUp", f_IsKeyUp },
    { "GetKeyPressed", f_GetKeyPressed },
    { "GetCharPressed", f_GetCharPressed },
    /* fare */
    { "IsMouseButtonDown", f_IsMouseButtonDown },
    { "IsMouseButtonPressed", f_IsMouseButtonPressed },
    { "IsMouseButtonReleased", f_IsMouseButtonReleased },
    { "IsMouseButtonUp", f_IsMouseButtonUp },
    { "GetMouseX", f_GetMouseX },
    { "GetMouseY", f_GetMouseY },
    { "GetMousePosition", f_GetMousePosition },
    { "GetMouseDelta", f_GetMouseDelta },
    { "GetMouseWheelMove", f_GetMouseWheelMove },
    { "SetMousePosition", f_SetMousePosition },
    { "SetMouseScale", f_SetMouseScale },
    /* texture */
    { "LoadTexture", f_LoadTexture },
    { "UnloadTexture", f_UnloadTexture },
    { "DrawTexture", f_DrawTexture },
    { "DrawTextureEx", f_DrawTextureEx },
    /* yapicilar */
    { "Vector2", f_Vector2 },
    { "Vector3", f_Vector3 },
    { "Rectangle", f_Rectangle },
    { "Color", f_Color },
    { NULL, NULL }
};

/* gcl_lua.gcDL'nin cagirdigi tek export: bridge + state ver, gcl.raylib kur */
GCL_MODULE_EXPORT void gcdl_raylib_attach(lua_State *L, const GclLua *api) {
    R = api;

    lua_createtable(L, 0, 8);   /* tablo (lua_newtable makrosu) — 8 anahtar on-boyut */
    luaL_setfuncs(L, rayfuncs, 0);

    lua_pushstring(L, RAYLIB_VERSION);
    lua_setfield(L, -2, "raylib_version");
    lua_pushinteger(L, RAYLIB_VERSION_MAJOR);
    lua_setfield(L, -2, "RAYLIB_VERSION_MAJOR");
    lua_pushinteger(L, RAYLIB_VERSION_MINOR);
    lua_setfield(L, -2, "RAYLIB_VERSION_MINOR");
    lua_pushinteger(L, RAYLIB_VERSION_PATCH);
    lua_setfield(L, -2, "RAYLIB_VERSION_PATCH");

    /* renkler (raylib.h Color degerleri) */
    RL_COLOR_TABLE(L, "RAYWHITE",  245, 245, 245, 255);
    RL_COLOR_TABLE(L, "WHITE",     255, 255, 255, 255);
    RL_COLOR_TABLE(L, "BLACK",     0,   0,   0,   255);
    RL_COLOR_TABLE(L, "GRAY",      130, 130, 130, 255);
    RL_COLOR_TABLE(L, "DARKGRAY",  80,  80,  80,  255);
    RL_COLOR_TABLE(L, "LIGHTGRAY", 200, 200, 200, 255);
    RL_COLOR_TABLE(L, "RED",       230, 41,  55,  255);
    RL_COLOR_TABLE(L, "DARKRED",   190, 33,  55,  255);
    RL_COLOR_TABLE(L, "GREEN",     0,   228, 48,  255);
    RL_COLOR_TABLE(L, "DARKGREEN", 0,   117, 44,  255);
    RL_COLOR_TABLE(L, "LIME",      0,   158, 47,  255);
    RL_COLOR_TABLE(L, "BLUE",      0,   121, 241, 255);
    RL_COLOR_TABLE(L, "DARKBLUE",  0,   82,  172, 255);
    RL_COLOR_TABLE(L, "SKYBLUE",   102, 191, 255, 255);
    RL_COLOR_TABLE(L, "YELLOW",    253, 249, 0,   255);
    RL_COLOR_TABLE(L, "GOLD",      255, 203, 0,   255);
    RL_COLOR_TABLE(L, "ORANGE",    255, 161, 0,   255);
    RL_COLOR_TABLE(L, "PURPLE",    200, 122, 255, 255);
    RL_COLOR_TABLE(L, "MAGENTA",   255, 0,   255, 255);
    RL_COLOR_TABLE(L, "PINK",      255, 109, 194, 255);
    RL_COLOR_TABLE(L, "MAROON",    190, 33,  55,  255);
    RL_COLOR_TABLE(L, "BEIGE",     211, 176, 131, 255);
    RL_COLOR_TABLE(L, "BROWN",     127, 106, 79,  255);

    /* klavye sabitleri (raylib.h KeyboardKey) */
    RL_INT_CONST(L, "KEY_NULL",        0);
    RL_INT_CONST(L, "KEY_SPACE",       32);
    RL_INT_CONST(L, "KEY_ESCAPE",      256);
    RL_INT_CONST(L, "KEY_ENTER",       257);
    RL_INT_CONST(L, "KEY_TAB",         258);
    RL_INT_CONST(L, "KEY_BACKSPACE",   259);
    RL_INT_CONST(L, "KEY_INSERT",      260);
    RL_INT_CONST(L, "KEY_DELETE",      261);
    RL_INT_CONST(L, "KEY_RIGHT",       262);
    RL_INT_CONST(L, "KEY_LEFT",        263);
    RL_INT_CONST(L, "KEY_DOWN",        264);
    RL_INT_CONST(L, "KEY_UP",          265);
    RL_INT_CONST(L, "KEY_HOME",        268);
    RL_INT_CONST(L, "KEY_END",         269);
    RL_INT_CONST(L, "KEY_F1",          290);
    RL_INT_CONST(L, "KEY_F2",          291);
    RL_INT_CONST(L, "KEY_F3",          292);
    RL_INT_CONST(L, "KEY_F4",          293);
    RL_INT_CONST(L, "KEY_F5",          294);
    RL_INT_CONST(L, "KEY_F6",          295);
    RL_INT_CONST(L, "KEY_F7",          296);
    RL_INT_CONST(L, "KEY_F8",          297);
    RL_INT_CONST(L, "KEY_F9",          298);
    RL_INT_CONST(L, "KEY_F10",         299);
    RL_INT_CONST(L, "KEY_F11",         300);
    RL_INT_CONST(L, "KEY_F12",         301);
    RL_INT_CONST(L, "KEY_LEFT_SHIFT",  340);
    RL_INT_CONST(L, "KEY_LEFT_CONTROL", 341);
    RL_INT_CONST(L, "KEY_LEFT_ALT",    342);
    RL_INT_CONST(L, "KEY_RIGHT_SHIFT", 344);
    RL_INT_CONST(L, "KEY_RIGHT_CONTROL", 345);
    RL_INT_CONST(L, "KEY_RIGHT_ALT",   346);
    RL_INT_CONST(L, "KEY_A",       65);
    RL_INT_CONST(L, "KEY_B",       66);
    RL_INT_CONST(L, "KEY_C",       67);
    RL_INT_CONST(L, "KEY_D",       68);
    RL_INT_CONST(L, "KEY_E",       69);
    RL_INT_CONST(L, "KEY_F",       70);
    RL_INT_CONST(L, "KEY_G",       71);
    RL_INT_CONST(L, "KEY_H",       72);
    RL_INT_CONST(L, "KEY_I",       73);
    RL_INT_CONST(L, "KEY_J",       74);
    RL_INT_CONST(L, "KEY_K",       75);
    RL_INT_CONST(L, "KEY_L",       76);
    RL_INT_CONST(L, "KEY_M",       77);
    RL_INT_CONST(L, "KEY_N",       78);
    RL_INT_CONST(L, "KEY_O",       79);
    RL_INT_CONST(L, "KEY_P",       80);
    RL_INT_CONST(L, "KEY_Q",       81);
    RL_INT_CONST(L, "KEY_R",       82);
    RL_INT_CONST(L, "KEY_S",       83);
    RL_INT_CONST(L, "KEY_T",       84);
    RL_INT_CONST(L, "KEY_U",       85);
    RL_INT_CONST(L, "KEY_V",       86);
    RL_INT_CONST(L, "KEY_W",       87);
    RL_INT_CONST(L, "KEY_X",       88);
    RL_INT_CONST(L, "KEY_Y",       89);
    RL_INT_CONST(L, "KEY_Z",       90);

    /* fare sabitleri */
    RL_INT_CONST(L, "MOUSE_BUTTON_LEFT",   0);
    RL_INT_CONST(L, "MOUSE_BUTTON_RIGHT",  1);
    RL_INT_CONST(L, "MOUSE_BUTTON_MIDDLE", 2);
    RL_INT_CONST(L, "MOUSE_BUTTON_SIDE",   3);
    RL_INT_CONST(L, "MOUSE_BUTTON_EXTRA",  4);
    RL_INT_CONST(L, "MOUSE_BUTTON_FORWARD", 5);
    RL_INT_CONST(L, "MOUSE_BUTTON_BACK",   6);

    /* pencere bayraklari (raylib.h ConfigFlags) */
    RL_INT_CONST(L, "FLAG_VSYNC_HINT",              0x40);
    RL_INT_CONST(L, "FLAG_FULLSCREEN_MODE",         0x02);
    RL_INT_CONST(L, "FLAG_WINDOW_RESIZABLE",        0x04);
    RL_INT_CONST(L, "FLAG_WINDOW_UNDECORATED",      0x08);
    RL_INT_CONST(L, "FLAG_WINDOW_HIDDEN",           0x80);
    RL_INT_CONST(L, "FLAG_WINDOW_MINIMIZED",        0x200);
    RL_INT_CONST(L, "FLAG_WINDOW_MAXIMIZED",        0x400);
    RL_INT_CONST(L, "FLAG_WINDOW_UNFOCUSED",        0x800);
    RL_INT_CONST(L, "FLAG_WINDOW_TOPMOST",          0x1000);
    RL_INT_CONST(L, "FLAG_WINDOW_HIGHDPI",          0x2000);
    RL_INT_CONST(L, "FLAG_WINDOW_MOUSE_PASSTHROUGH", 0x4000);
    RL_INT_CONST(L, "FLAG_BORDERLESS_WINDOWED_MODE", 0x8000);
    RL_INT_CONST(L, "FLAG_MSAA_4X_HINT",            0x20);
    RL_INT_CONST(L, "FLAG_INTERLACED_HINT",         0x10000);

    /* kamera */
    RL_INT_CONST(L, "CAMERA_ORTHOGRAPHIC", 1);
    RL_INT_CONST(L, "CAMERA_PERSPECTIVE",  0);
}
