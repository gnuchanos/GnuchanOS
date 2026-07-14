/* ============================================================
 * gcl_raylib_binding.c — GCL Lua 5.4 binding for raylib
 *
 * Compile as a DLL, load via: local rl = require("gcl_raylib")
 * Requires: raylib (headers + lib), lua55.dll
 * ============================================================ */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* raylib headers — path set via -I in build */
#include "raylib.h"
#include "raymath.h"

/* ============================================================
 * HELPER: push Color as {r,g,b,a}
 * ============================================================ */
static void push_Color(lua_State *L, Color c) {
    lua_createtable(L, 0, 4);
    lua_pushinteger(L, c.r); lua_setfield(L, -2, "r");
    lua_pushinteger(L, c.g); lua_setfield(L, -2, "g");
    lua_pushinteger(L, c.b); lua_setfield(L, -2, "b");
    lua_pushinteger(L, c.a); lua_setfield(L, -2, "a");
}

/* ============================================================
 * WINDOW & CONTEXT
 * ============================================================ */
static int lw_InitWindow(lua_State *L) {
    int w      = (int)luaL_checkinteger(L, 1);
    int h      = (int)luaL_checkinteger(L, 2);
    const char *title = luaL_checkstring(L, 3);
    InitWindow(w, h, title);
    return 0;
}
static int lw_CloseWindow(lua_State *L) {
    CloseWindow(); return 0;
}
static int lw_WindowShouldClose(lua_State *L) {
    lua_pushboolean(L, WindowShouldClose());
    return 1;
}
static int lw_IsWindowReady(lua_State *L) {
    lua_pushboolean(L, IsWindowReady()); return 1;
}
static int lw_IsWindowFullscreen(lua_State *L) {
    lua_pushboolean(L, IsWindowFullscreen()); return 1;
}
static int lw_IsWindowHidden(lua_State *L) {
    lua_pushboolean(L, IsWindowHidden()); return 1;
}
static int lw_IsWindowMinimized(lua_State *L) {
    lua_pushboolean(L, IsWindowMinimized()); return 1;
}
static int lw_IsWindowMaximized(lua_State *L) {
    lua_pushboolean(L, IsWindowMaximized()); return 1;
}
static int lw_IsWindowFocused(lua_State *L) {
    lua_pushboolean(L, IsWindowFocused()); return 1;
}
static int lw_IsWindowResized(lua_State *L) {
    lua_pushboolean(L, IsWindowResized()); return 1;
}
static int lw_SetWindowTitle(lua_State *L) {
    SetWindowTitle(luaL_checkstring(L, 1)); return 0;
}
static int lw_SetWindowSize(lua_State *L) {
    SetWindowSize((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2));
    return 0;
}
static int lw_GetScreenWidth(lua_State *L) {
    lua_pushinteger(L, GetScreenWidth()); return 1;
}
static int lw_GetScreenHeight(lua_State *L) {
    lua_pushinteger(L, GetScreenHeight()); return 1;
}
static int lw_GetRenderWidth(lua_State *L) {
    lua_pushinteger(L, GetRenderWidth()); return 1;
}
static int lw_GetRenderHeight(lua_State *L) {
    lua_pushinteger(L, GetRenderHeight()); return 1;
}
static int lw_ToggleFullscreen(lua_State *L) {
    ToggleFullscreen(); return 0;
}
static int lw_MaximizeWindow(lua_State *L) {
    MaximizeWindow(); return 0;
}
static int lw_MinimizeWindow(lua_State *L) {
    MinimizeWindow(); return 0;
}
static int lw_RestoreWindow(lua_State *L) {
    RestoreWindow(); return 0;
}
static int lw_SetConfigFlags(lua_State *L) {
    SetConfigFlags((unsigned int)luaL_checkinteger(L,1));
    return 0;
}
static int lw_SetTargetFPS(lua_State *L) {
    SetTargetFPS((int)luaL_checkinteger(L,1)); return 0;
}
static int lw_GetFPS(lua_State *L) {
    lua_pushinteger(L, GetFPS()); return 1;
}
static int lw_GetFrameTime(lua_State *L) {
    lua_pushnumber(L, GetFrameTime()); return 1;
}
static int lw_GetTime(lua_State *L) {
    lua_pushnumber(L, GetTime()); return 1;
}

/* ============================================================
 * DRAWING
 * ============================================================ */
static int lw_BeginDrawing(lua_State *L) {
    BeginDrawing(); return 0;
}
static int lw_EndDrawing(lua_State *L) {
    EndDrawing(); return 0;
}
static int lw_ClearBackground(lua_State *L) {
    /* expect 4 integers: r,g,b,a */
    Color c = {
        (unsigned char)luaL_checkinteger(L, 1),
        (unsigned char)luaL_checkinteger(L, 2),
        (unsigned char)luaL_checkinteger(L, 3),
        (unsigned char)luaL_optinteger(L, 4, 255)
    };
    ClearBackground(c);
    return 0;
}
static int lw_BeginMode2D(lua_State *L) {
    /* Camera2D as 6 nums: offset.x offset.y target.x target.y rotation zoom */
    Camera2D cam;
    cam.offset.x   = (float)luaL_checknumber(L, 1);
    cam.offset.y   = (float)luaL_checknumber(L, 2);
    cam.target.x   = (float)luaL_checknumber(L, 3);
    cam.target.y   = (float)luaL_checknumber(L, 4);
    cam.rotation   = (float)luaL_checknumber(L, 5);
    cam.zoom       = (float)luaL_checknumber(L, 6);
    BeginMode2D(cam);
    return 0;
}
static int lw_EndMode2D(lua_State *L) {
    EndMode2D(); return 0;
}
static int lw_BeginMode3D(lua_State *L) {
    /* Camera3D: position.x .y .z target.x .y .z up.x .y .z fovy projection */
    Camera3D cam;
    cam.position.x = (float)luaL_checknumber(L, 1);
    cam.position.y = (float)luaL_checknumber(L, 2);
    cam.position.z = (float)luaL_checknumber(L, 3);
    cam.target.x   = (float)luaL_checknumber(L, 4);
    cam.target.y   = (float)luaL_checknumber(L, 5);
    cam.target.z   = (float)luaL_checknumber(L, 6);
    cam.up.x       = (float)luaL_checknumber(L, 7);
    cam.up.y       = (float)luaL_checknumber(L, 8);
    cam.up.z       = (float)luaL_checknumber(L, 9);
    cam.fovy       = (float)luaL_checknumber(L, 10);
    cam.projection = (int)luaL_optinteger(L, 11, CAMERA_PERSPECTIVE);
    BeginMode3D(cam);
    return 0;
}
static int lw_EndMode3D(lua_State *L) {
    EndMode3D(); return 0;
}
static int lw_BeginTextureMode(lua_State *L) {
    RenderTexture2D rt;
    rt.id = (unsigned int)luaL_checkinteger(L, 1);
    BeginTextureMode(rt);
    return 0;
}
static int lw_EndTextureMode(lua_State *L) {
    EndTextureMode(); return 0;
}
static int lw_DrawFPS(lua_State *L) {
    int x = (int)luaL_optinteger(L, 1, 10);
    int y = (int)luaL_optinteger(L, 2, 10);
    DrawFPS(x, y);
    return 0;
}

/* ============================================================
 * SHAPES — 2D
 * ============================================================ */
static int lw_DrawPixel(lua_State *L) {
    int x = (int)luaL_checkinteger(L,1);
    int y = (int)luaL_checkinteger(L,2);
    Color c = {(unsigned char)luaL_checkinteger(L,3),
               (unsigned char)luaL_checkinteger(L,4),
               (unsigned char)luaL_checkinteger(L,5),
               (unsigned char)luaL_optinteger(L,6,255)};
    DrawPixel(x, y, c);
    return 0;
}
static int lw_DrawLine(lua_State *L) {
    int x1=(int)luaL_checkinteger(L,1), y1=(int)luaL_checkinteger(L,2);
    int x2=(int)luaL_checkinteger(L,3), y2=(int)luaL_checkinteger(L,4);
    Color c={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_optinteger(L,8,255)};
    DrawLine(x1,y1,x2,y2,c);
    return 0;
}
static int lw_DrawRectangle(lua_State *L) {
    int x=(int)luaL_checkinteger(L,1), y=(int)luaL_checkinteger(L,2);
    int w=(int)luaL_checkinteger(L,3), h=(int)luaL_checkinteger(L,4);
    Color c={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_optinteger(L,8,255)};
    DrawRectangle(x,y,w,h,c);
    return 0;
}
static int lw_DrawRectangleV(lua_State *L) {
    Vector2 p = {(float)luaL_checknumber(L,1), (float)luaL_checknumber(L,2)};
    Vector2 s = {(float)luaL_checknumber(L,3), (float)luaL_checknumber(L,4)};
    Color c={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_optinteger(L,8,255)};
    DrawRectangleV(p,s,c);
    return 0;
}
static int lw_DrawCircle(lua_State *L) {
    int cx=(int)luaL_checkinteger(L,1), cy=(int)luaL_checkinteger(L,2);
    float r=(float)luaL_checknumber(L,3);
    Color c={(unsigned char)luaL_checkinteger(L,4),
             (unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_optinteger(L,7,255)};
    DrawCircle(cx,cy,r,c);
    return 0;
}
static int lw_DrawCircleV(lua_State *L) {
    Vector2 center={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    float r=(float)luaL_checknumber(L,3);
    Color c={(unsigned char)luaL_checkinteger(L,4),
             (unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_optinteger(L,7,255)};
    DrawCircleV(center,r,c);
    return 0;
}
static int lw_DrawTriangle(lua_State *L) {
    Vector2 v1={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 v2={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    Vector2 v3={(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6)};
    Color c={(unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_checkinteger(L,8),
             (unsigned char)luaL_checkinteger(L,9),
             (unsigned char)luaL_optinteger(L,10,255)};
    DrawTriangle(v1,v2,v3,c);
    return 0;
}
static int lw_DrawEllipse(lua_State *L) {
    int cx=(int)luaL_checkinteger(L,1), cy=(int)luaL_checkinteger(L,2);
    float rx=(float)luaL_checknumber(L,3), ry=(float)luaL_checknumber(L,4);
    Color c={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_optinteger(L,8,255)};
    DrawEllipse(cx,cy,rx,ry,c);
    return 0;
}
static int lw_DrawRing(lua_State *L) {
    Vector2 c={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    float iR=(float)luaL_checknumber(L,3), oR=(float)luaL_checknumber(L,4);
    float sA=(float)luaL_checknumber(L,5), eA=(float)luaL_checknumber(L,6);
    int seg=(int)luaL_optinteger(L,7,0);
    Color col={(unsigned char)luaL_checkinteger(L,8),
               (unsigned char)luaL_checkinteger(L,9),
               (unsigned char)luaL_checkinteger(L,10),
               (unsigned char)luaL_optinteger(L,11,255)};
    DrawRing(c,iR,oR,sA,eA,seg,col);
    return 0;
}

/* ============================================================
 * TEXT
 * ============================================================ */
static int lw_DrawText(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    int x  = (int)luaL_checkinteger(L, 2);
    int y  = (int)luaL_checkinteger(L, 3);
    int sz = (int)luaL_checkinteger(L, 4);
    Color c={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_optinteger(L,8,255)};
    DrawText(text, x, y, sz, c);
    return 0;
}
static int lw_MeasureText(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    int sz = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, MeasureText(text, sz));
    return 1;
}
static int lw_TextLength(lua_State *L) {
    lua_pushinteger(L, TextLength(luaL_checkstring(L,1)));
    return 1;
}
static int lw_GetFontDefault(lua_State *L) {
    Font f = GetFontDefault();
    lua_createtable(L, 0, 3);
    lua_pushinteger(L, f.baseSize);   lua_setfield(L, -2, "baseSize");
    lua_pushinteger(L, f.glyphCount); lua_setfield(L, -2, "glyphCount");
    lua_pushinteger(L, f.glyphPadding); lua_setfield(L, -2, "glyphPadding");
    return 1;
}
static int lw_LoadFont(lua_State *L) {
    Font f = LoadFont(luaL_checkstring(L,1));
    lua_pushinteger(L, f.baseSize);
    lua_pushinteger(L, f.glyphCount);
    return 2;
}
static int lw_UnloadFont(lua_State *L) {
    /* can't unload font from lua easily, skip for now */
    return 0;
}

/* ============================================================
 * TEXTURES / IMAGES
 * ============================================================ */
static int lw_LoadTexture(lua_State *L) {
    Texture2D t = LoadTexture(luaL_checkstring(L,1));
    lua_pushinteger(L, t.id);
    lua_pushinteger(L, t.width);
    lua_pushinteger(L, t.height);
    return 3;
}
static int lw_LoadTextureFromImage(lua_State *L) {
    /* image passed as table with data,width,height,format — simplified */
    return luaL_error(L, "LoadTextureFromImage not implemented");
}
static int lw_UnloadTexture(lua_State *L) {
    Texture2D t;
    t.id     = (unsigned int)luaL_checkinteger(L,1);
    t.width  = (int)luaL_optinteger(L,2,0);
    t.height = (int)luaL_optinteger(L,3,0);
    UnloadTexture(t);
    return 0;
}
static int lw_DrawTexture(lua_State *L) {
    Texture2D t;
    t.id     = (unsigned int)luaL_checkinteger(L,1);
    t.width  = 0;  /* not used for drawing */
    t.height = 0;
    int x    = (int)luaL_checkinteger(L,2);
    int y    = (int)luaL_checkinteger(L,3);
    Color c  = {(unsigned char)luaL_optinteger(L,4,255),
                (unsigned char)luaL_optinteger(L,5,255),
                (unsigned char)luaL_optinteger(L,6,255),
                (unsigned char)luaL_optinteger(L,7,255)};
    DrawTexture(t, x, y, c);
    return 0;
}
static int lw_DrawTextureEx(lua_State *L) {
    Texture2D t;
    t.id = (unsigned int)luaL_checkinteger(L,1);
    t.width  = 0;
    t.height = 0;
    Vector2 pos = {(float)luaL_checknumber(L,2), (float)luaL_checknumber(L,3)};
    float rot  = (float)luaL_checknumber(L,4);
    float sc   = (float)luaL_checknumber(L,5);
    Color c    = {(unsigned char)luaL_optinteger(L,6,255),
                  (unsigned char)luaL_optinteger(L,7,255),
                  (unsigned char)luaL_optinteger(L,8,255),
                  (unsigned char)luaL_optinteger(L,9,255)};
    DrawTextureEx(t, pos, rot, sc, c);
    return 0;
}
static int lw_GenTextureColor(lua_State *L) {
    /* generate a colored checker texture for testing */
    int w = (int)luaL_checkinteger(L,1);
    int h = (int)luaL_checkinteger(L,2);
    Image img = GenImageColor(w, h, RAYWHITE);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    lua_pushinteger(L, t.id);
    lua_pushinteger(L, t.width);
    lua_pushinteger(L, t.height);
    return 3;
}
static int lw_LoadImage(lua_State *L) {
    Image img = LoadImage(luaL_checkstring(L,1));
    lua_pushlightuserdata(L, img.data); /* not ideal but works */
    lua_pushinteger(L, img.width);
    lua_pushinteger(L, img.height);
    lua_pushinteger(L, img.mipmaps);
    lua_pushinteger(L, img.format);
    return 5;
}
static int lw_UnloadImage(lua_State *L) {
    /* can't easily reconstruct Image from lua, skip */
    return 0;
}
static int lw_ExportImage(lua_State *L) {
    /* simplified: export current screen */
    Image img = LoadImageFromScreen();
    ExportImage(img, luaL_checkstring(L,1));
    UnloadImage(img);
    return 0;
}

/* ============================================================
 * INPUT — KEYBOARD
 * ============================================================ */
static int lw_IsKeyDown(lua_State *L) {
    lua_pushboolean(L, IsKeyDown((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsKeyPressed(lua_State *L) {
    lua_pushboolean(L, IsKeyPressed((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsKeyReleased(lua_State *L) {
    lua_pushboolean(L, IsKeyReleased((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsKeyUp(lua_State *L) {
    lua_pushboolean(L, IsKeyUp((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_GetKeyPressed(lua_State *L) {
    lua_pushinteger(L, GetKeyPressed());
    return 1;
}
static int lw_GetCharPressed(lua_State *L) {
    lua_pushinteger(L, GetCharPressed());
    return 1;
}

/* ============================================================
 * INPUT — MOUSE
 * ============================================================ */
static int lw_IsMouseButtonDown(lua_State *L) {
    lua_pushboolean(L, IsMouseButtonDown((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsMouseButtonPressed(lua_State *L) {
    lua_pushboolean(L, IsMouseButtonPressed((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsMouseButtonReleased(lua_State *L) {
    lua_pushboolean(L, IsMouseButtonReleased((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsMouseButtonUp(lua_State *L) {
    lua_pushboolean(L, IsMouseButtonUp((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_GetMouseX(lua_State *L) {
    lua_pushinteger(L, GetMouseX()); return 1;
}
static int lw_GetMouseY(lua_State *L) {
    lua_pushinteger(L, GetMouseY()); return 1;
}
static int lw_GetMousePosition(lua_State *L) {
    Vector2 p = GetMousePosition();
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    return 2;
}
static int lw_SetMousePosition(lua_State *L) {
    SetMousePosition((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2));
    return 0;
}

/* ============================================================
 * INPUT — GAMEPAD
 * ============================================================ */
static int lw_IsGamepadAvailable(lua_State *L) {
    lua_pushboolean(L, IsGamepadAvailable((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_GetGamepadName(lua_State *L) {
    lua_pushstring(L, GetGamepadName((int)luaL_checkinteger(L,1)));
    return 1;
}
static int lw_IsGamepadButtonDown(lua_State *L) {
    lua_pushboolean(L, IsGamepadButtonDown((int)luaL_checkinteger(L,1),
                                            (int)luaL_checkinteger(L,2)));
    return 1;
}
static int lw_IsGamepadButtonPressed(lua_State *L) {
    lua_pushboolean(L, IsGamepadButtonPressed((int)luaL_checkinteger(L,1),
                                               (int)luaL_checkinteger(L,2)));
    return 1;
}
static int lw_GetGamepadAxisMovement(lua_State *L) {
    lua_pushnumber(L, GetGamepadAxisMovement((int)luaL_checkinteger(L,1),
                                              (int)luaL_checkinteger(L,2)));
    return 1;
}

/* ============================================================
 * COLOR
 * ============================================================ */
static int lw_ColorFromHSV(lua_State *L) {
    Color c = ColorFromHSV((float)luaL_checknumber(L,1),
                           (float)luaL_checknumber(L,2),
                           (float)luaL_checknumber(L,3));
    push_Color(L, c);
    return 1;
}
static int lw_ColorAlpha(lua_State *L) {
    Color base={(unsigned char)luaL_checkinteger(L,1),
                (unsigned char)luaL_checkinteger(L,2),
                (unsigned char)luaL_checkinteger(L,3),
                (unsigned char)luaL_checkinteger(L,4)};
    Color c = ColorAlpha(base, (float)luaL_checknumber(L,5));
    push_Color(L, c);
    return 1;
}
static int lw_ColorLerp(lua_State *L) {
    Color a={(unsigned char)luaL_checkinteger(L,1),
             (unsigned char)luaL_checkinteger(L,2),
             (unsigned char)luaL_checkinteger(L,3),
             (unsigned char)luaL_checkinteger(L,4)};
    Color b={(unsigned char)luaL_checkinteger(L,5),
             (unsigned char)luaL_checkinteger(L,6),
             (unsigned char)luaL_checkinteger(L,7),
             (unsigned char)luaL_checkinteger(L,8)};
    Color c = ColorLerp(a, b, (float)luaL_checknumber(L,9));
    push_Color(L, c);
    return 1;
}
static int lw_ColorToInt(lua_State *L) {
    Color c={(unsigned char)luaL_checkinteger(L,1),
             (unsigned char)luaL_checkinteger(L,2),
             (unsigned char)luaL_checkinteger(L,3),
             (unsigned char)luaL_checkinteger(L,4)};
    lua_pushinteger(L, ColorToInt(c));
    return 1;
}
static int lw_Fade(lua_State *L) {
    Color base={(unsigned char)luaL_checkinteger(L,1),
                (unsigned char)luaL_checkinteger(L,2),
                (unsigned char)luaL_checkinteger(L,3),
                (unsigned char)luaL_optinteger(L,4,255)};
    Color c = Fade(base, (float)luaL_checknumber(L,5));
    push_Color(L, c);
    return 1;
}

/* ============================================================
 * MATH / VECTOR
 * ============================================================ */
static int lw_Vector2Create(lua_State *L) {
    lua_pushnumber(L, (float)luaL_checknumber(L,1));
    lua_pushnumber(L, (float)luaL_checknumber(L,2));
    return 2;
}
static int lw_Vector2Add(lua_State *L) {
    Vector2 a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 b={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    Vector2 r = Vector2Add(a,b);
    lua_pushnumber(L, r.x); lua_pushnumber(L, r.y);
    return 2;
}
static int lw_Vector2Subtract(lua_State *L) {
    Vector2 a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 b={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    Vector2 r = Vector2Subtract(a,b);
    lua_pushnumber(L, r.x); lua_pushnumber(L, r.y);
    return 2;
}
static int lw_Vector2Length(lua_State *L) {
    Vector2 v={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    lua_pushnumber(L, Vector2Length(v));
    return 1;
}
static int lw_Vector2Distance(lua_State *L) {
    Vector2 a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 b={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    lua_pushnumber(L, Vector2Distance(a,b));
    return 1;
}
static int lw_Vector2Normalize(lua_State *L) {
    Vector2 v={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 r = Vector2Normalize(v);
    lua_pushnumber(L, r.x); lua_pushnumber(L, r.y);
    return 2;
}
static int lw_Vector2DotProduct(lua_State *L) {
    Vector2 a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    Vector2 b={(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    lua_pushnumber(L, Vector2DotProduct(a,b));
    return 1;
}

/* ============================================================
 * COLLISION
 * ============================================================ */
static int lw_CheckCollisionRecs(lua_State *L) {
    Rectangle a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),
                 (float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    Rectangle b={(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),
                 (float)luaL_checknumber(L,7),(float)luaL_checknumber(L,8)};
    lua_pushboolean(L, CheckCollisionRecs(a,b));
    return 1;
}
static int lw_CheckCollisionCircleRec(lua_State *L) {
    Vector2 center={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)};
    float rad = (float)luaL_checknumber(L,3);
    Rectangle rec={(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),
                   (float)luaL_checknumber(L,6),(float)luaL_checknumber(L,7)};
    lua_pushboolean(L, CheckCollisionCircleRec(center, rad, rec));
    return 1;
}
static int lw_GetCollisionRec(lua_State *L) {
    Rectangle a={(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),
                 (float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)};
    Rectangle b={(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),
                 (float)luaL_checknumber(L,7),(float)luaL_checknumber(L,8)};
    Rectangle r = GetCollisionRec(a,b);
    lua_pushnumber(L, r.x); lua_pushnumber(L, r.y);
    lua_pushnumber(L, r.width); lua_pushnumber(L, r.height);
    return 4;
}

/* ============================================================
 * AUDIO
 * ============================================================ */
static int lw_InitAudioDevice(lua_State *L) {
    InitAudioDevice(); return 0;
}
static int lw_CloseAudioDevice(lua_State *L) {
    CloseAudioDevice(); return 0;
}
static int lw_IsAudioDeviceReady(lua_State *L) {
    lua_pushboolean(L, IsAudioDeviceReady()); return 1;
}
static int lw_LoadSound(lua_State *L) {
    Sound s = LoadSound(luaL_checkstring(L,1));
    lua_pushlightuserdata(L, (void*)s.stream.buffer);
    lua_pushinteger(L, s.frameCount);
    return 2;
}
static int lw_UnloadSound(lua_State *L) {
    /* skip */
    return 0;
}
static int lw_PlaySound(lua_State *L) {
    /* simplified */
    return 0;
}
static int lw_StopSound(lua_State *L) {
    return 0;
}
static int lw_LoadMusicStream(lua_State *L) {
    Music m = LoadMusicStream(luaL_checkstring(L,1));
    lua_pushlightuserdata(L, (void*)m.stream.buffer);
    return 1;
}
static int lw_PlayMusicStream(lua_State *L) {
    return 0;
}
static int lw_UpdateMusicStream(lua_State *L) {
    return 0;
}
static int lw_StopMusicStream(lua_State *L) {
    return 0;
}

/* ============================================================
 * SCREEN / CAMERA
 * ============================================================ */
static int lw_TakeScreenshot(lua_State *L) {
    TakeScreenshot(luaL_checkstring(L,1));
    return 0;
}

/* ============================================================
 * MODULE REGISTRATION
 * ============================================================ */
static const struct luaL_Reg raylib_funcs[] = {
    /* Window */
    {"InitWindow",          lw_InitWindow},
    {"CloseWindow",         lw_CloseWindow},
    {"WindowShouldClose",   lw_WindowShouldClose},
    {"IsWindowReady",       lw_IsWindowReady},
    {"IsWindowFullscreen",  lw_IsWindowFullscreen},
    {"IsWindowHidden",      lw_IsWindowHidden},
    {"IsWindowMinimized",   lw_IsWindowMinimized},
    {"IsWindowMaximized",   lw_IsWindowMaximized},
    {"IsWindowFocused",     lw_IsWindowFocused},
    {"IsWindowResized",     lw_IsWindowResized},
    {"SetWindowTitle",      lw_SetWindowTitle},
    {"SetWindowSize",       lw_SetWindowSize},
    {"GetScreenWidth",      lw_GetScreenWidth},
    {"GetScreenHeight",     lw_GetScreenHeight},
    {"GetRenderWidth",      lw_GetRenderWidth},
    {"GetRenderHeight",     lw_GetRenderHeight},
    {"ToggleFullscreen",    lw_ToggleFullscreen},
    {"MaximizeWindow",      lw_MaximizeWindow},
    {"MinimizeWindow",      lw_MinimizeWindow},
    {"RestoreWindow",       lw_RestoreWindow},
    {"SetConfigFlags",      lw_SetConfigFlags},
    {"SetTargetFPS",        lw_SetTargetFPS},
    {"GetFPS",              lw_GetFPS},
    {"GetFrameTime",        lw_GetFrameTime},
    {"GetTime",             lw_GetTime},

    /* Drawing */
    {"BeginDrawing",        lw_BeginDrawing},
    {"EndDrawing",          lw_EndDrawing},
    {"ClearBackground",     lw_ClearBackground},
    {"BeginMode2D",         lw_BeginMode2D},
    {"EndMode2D",           lw_EndMode2D},
    {"BeginMode3D",         lw_BeginMode3D},
    {"EndMode3D",           lw_EndMode3D},
    {"BeginTextureMode",    lw_BeginTextureMode},
    {"EndTextureMode",      lw_EndTextureMode},
    {"DrawFPS",             lw_DrawFPS},

    /* Shapes */
    {"DrawPixel",           lw_DrawPixel},
    {"DrawLine",            lw_DrawLine},
    {"DrawRectangle",       lw_DrawRectangle},
    {"DrawRectangleV",      lw_DrawRectangleV},
    {"DrawCircle",          lw_DrawCircle},
    {"DrawCircleV",         lw_DrawCircleV},
    {"DrawTriangle",        lw_DrawTriangle},
    {"DrawEllipse",         lw_DrawEllipse},
    {"DrawRing",            lw_DrawRing},

    /* Text */
    {"DrawText",            lw_DrawText},
    {"MeasureText",         lw_MeasureText},
    {"TextLength",          lw_TextLength},
    {"GetFontDefault",      lw_GetFontDefault},
    {"LoadFont",            lw_LoadFont},

    /* Textures */
    {"LoadTexture",         lw_LoadTexture},
    {"UnloadTexture",       lw_UnloadTexture},
    {"DrawTexture",         lw_DrawTexture},
    {"DrawTextureEx",       lw_DrawTextureEx},
    {"GenTextureColor",     lw_GenTextureColor},
    {"LoadImage",           lw_LoadImage},
    {"ExportImage",         lw_ExportImage},

    /* Keyboard */
    {"IsKeyDown",           lw_IsKeyDown},
    {"IsKeyPressed",        lw_IsKeyPressed},
    {"IsKeyReleased",       lw_IsKeyReleased},
    {"IsKeyUp",             lw_IsKeyUp},
    {"GetKeyPressed",       lw_GetKeyPressed},
    {"GetCharPressed",      lw_GetCharPressed},

    /* Mouse */
    {"IsMouseButtonDown",   lw_IsMouseButtonDown},
    {"IsMouseButtonPressed",lw_IsMouseButtonPressed},
    {"IsMouseButtonReleased",lw_IsMouseButtonReleased},
    {"IsMouseButtonUp",     lw_IsMouseButtonUp},
    {"GetMouseX",           lw_GetMouseX},
    {"GetMouseY",           lw_GetMouseY},
    {"GetMousePosition",    lw_GetMousePosition},
    {"SetMousePosition",    lw_SetMousePosition},

    /* Gamepad */
    {"IsGamepadAvailable",  lw_IsGamepadAvailable},
    {"GetGamepadName",      lw_GetGamepadName},
    {"IsGamepadButtonDown", lw_IsGamepadButtonDown},
    {"IsGamepadButtonPressed",lw_IsGamepadButtonPressed},
    {"GetGamepadAxisMovement",lw_GetGamepadAxisMovement},

    /* Color */
    {"ColorFromHSV",        lw_ColorFromHSV},
    {"ColorAlpha",          lw_ColorAlpha},
    {"ColorLerp",           lw_ColorLerp},
    {"ColorToInt",          lw_ColorToInt},
    {"Fade",                lw_Fade},

    /* Math */
    {"Vector2Create",       lw_Vector2Create},
    {"Vector2Add",          lw_Vector2Add},
    {"Vector2Subtract",     lw_Vector2Subtract},
    {"Vector2Length",       lw_Vector2Length},
    {"Vector2Distance",     lw_Vector2Distance},
    {"Vector2Normalize",    lw_Vector2Normalize},
    {"Vector2DotProduct",   lw_Vector2DotProduct},

    /* Collision */
    {"CheckCollisionRecs",      lw_CheckCollisionRecs},
    {"CheckCollisionCircleRec", lw_CheckCollisionCircleRec},
    {"GetCollisionRec",         lw_GetCollisionRec},

    /* Audio */
    {"InitAudioDevice",     lw_InitAudioDevice},
    {"CloseAudioDevice",    lw_CloseAudioDevice},
    {"IsAudioDeviceReady",  lw_IsAudioDeviceReady},
    {"LoadSound",           lw_LoadSound},
    {"PlaySound",           lw_PlaySound},
    {"StopSound",           lw_StopSound},
    {"LoadMusicStream",     lw_LoadMusicStream},
    {"PlayMusicStream",     lw_PlayMusicStream},
    {"UpdateMusicStream",   lw_UpdateMusicStream},
    {"StopMusicStream",     lw_StopMusicStream},

    /* Misc */
    {"TakeScreenshot",      lw_TakeScreenshot},

    {NULL, NULL}
};

/* ============================================================
 * CONSTANT PUSHING (colors, key codes, etc.)
 * ============================================================ */
static void push_constants(lua_State *L, int table_idx) {
    /* ---- ConfigFlags ---- */
    lua_pushinteger(L, FLAG_VSYNC_HINT);       lua_setfield(L, table_idx, "FLAG_VSYNC_HINT");
    lua_pushinteger(L, FLAG_FULLSCREEN_MODE);  lua_setfield(L, table_idx, "FLAG_FULLSCREEN_MODE");
    lua_pushinteger(L, FLAG_WINDOW_RESIZABLE); lua_setfield(L, table_idx, "FLAG_WINDOW_RESIZABLE");
    lua_pushinteger(L, FLAG_MSAA_4X_HINT);     lua_setfield(L, table_idx, "FLAG_MSAA_4X_HINT");
    lua_pushinteger(L, FLAG_WINDOW_UNDECORATED);lua_setfield(L, table_idx, "FLAG_WINDOW_UNDECORATED");

    /* ---- Keyboard keys ---- */
    lua_pushinteger(L, KEY_NULL);      lua_setfield(L, table_idx, "KEY_NULL");
    lua_pushinteger(L, KEY_SPACE);     lua_setfield(L, table_idx, "KEY_SPACE");
    lua_pushinteger(L, KEY_ESCAPE);    lua_setfield(L, table_idx, "KEY_ESCAPE");
    lua_pushinteger(L, KEY_ENTER);     lua_setfield(L, table_idx, "KEY_ENTER");
    lua_pushinteger(L, KEY_TAB);       lua_setfield(L, table_idx, "KEY_TAB");
    lua_pushinteger(L, KEY_BACKSPACE); lua_setfield(L, table_idx, "KEY_BACKSPACE");
    lua_pushinteger(L, KEY_INSERT);    lua_setfield(L, table_idx, "KEY_INSERT");
    lua_pushinteger(L, KEY_DELETE);    lua_setfield(L, table_idx, "KEY_DELETE");
    lua_pushinteger(L, KEY_RIGHT);     lua_setfield(L, table_idx, "KEY_RIGHT");
    lua_pushinteger(L, KEY_LEFT);      lua_setfield(L, table_idx, "KEY_LEFT");
    lua_pushinteger(L, KEY_DOWN);      lua_setfield(L, table_idx, "KEY_DOWN");
    lua_pushinteger(L, KEY_UP);        lua_setfield(L, table_idx, "KEY_UP");
    lua_pushinteger(L, KEY_HOME);      lua_setfield(L, table_idx, "KEY_HOME");
    lua_pushinteger(L, KEY_END);       lua_setfield(L, table_idx, "KEY_END");
    lua_pushinteger(L, KEY_A);         lua_setfield(L, table_idx, "KEY_A");
    lua_pushinteger(L, KEY_B);         lua_setfield(L, table_idx, "KEY_B");
    lua_pushinteger(L, KEY_C);         lua_setfield(L, table_idx, "KEY_C");
    lua_pushinteger(L, KEY_D);         lua_setfield(L, table_idx, "KEY_D");
    lua_pushinteger(L, KEY_E);         lua_setfield(L, table_idx, "KEY_E");
    lua_pushinteger(L, KEY_F);         lua_setfield(L, table_idx, "KEY_F");
    lua_pushinteger(L, KEY_G);         lua_setfield(L, table_idx, "KEY_G");
    lua_pushinteger(L, KEY_H);         lua_setfield(L, table_idx, "KEY_H");
    lua_pushinteger(L, KEY_I);         lua_setfield(L, table_idx, "KEY_I");
    lua_pushinteger(L, KEY_J);         lua_setfield(L, table_idx, "KEY_J");
    lua_pushinteger(L, KEY_K);         lua_setfield(L, table_idx, "KEY_K");
    lua_pushinteger(L, KEY_L);         lua_setfield(L, table_idx, "KEY_L");
    lua_pushinteger(L, KEY_M);         lua_setfield(L, table_idx, "KEY_M");
    lua_pushinteger(L, KEY_N);         lua_setfield(L, table_idx, "KEY_N");
    lua_pushinteger(L, KEY_O);         lua_setfield(L, table_idx, "KEY_O");
    lua_pushinteger(L, KEY_P);         lua_setfield(L, table_idx, "KEY_P");
    lua_pushinteger(L, KEY_Q);         lua_setfield(L, table_idx, "KEY_Q");
    lua_pushinteger(L, KEY_R);         lua_setfield(L, table_idx, "KEY_R");
    lua_pushinteger(L, KEY_S);         lua_setfield(L, table_idx, "KEY_S");
    lua_pushinteger(L, KEY_T);         lua_setfield(L, table_idx, "KEY_T");
    lua_pushinteger(L, KEY_U);         lua_setfield(L, table_idx, "KEY_U");
    lua_pushinteger(L, KEY_V);         lua_setfield(L, table_idx, "KEY_V");
    lua_pushinteger(L, KEY_W);         lua_setfield(L, table_idx, "KEY_W");
    lua_pushinteger(L, KEY_X);         lua_setfield(L, table_idx, "KEY_X");
    lua_pushinteger(L, KEY_Y);         lua_setfield(L, table_idx, "KEY_Y");
    lua_pushinteger(L, KEY_Z);         lua_setfield(L, table_idx, "KEY_Z");
    lua_pushinteger(L, KEY_ZERO);      lua_setfield(L, table_idx, "KEY_ZERO");
    lua_pushinteger(L, KEY_ONE);       lua_setfield(L, table_idx, "KEY_ONE");
    lua_pushinteger(L, KEY_TWO);       lua_setfield(L, table_idx, "KEY_TWO");
    lua_pushinteger(L, KEY_THREE);     lua_setfield(L, table_idx, "KEY_THREE");
    lua_pushinteger(L, KEY_FOUR);      lua_setfield(L, table_idx, "KEY_FOUR");
    lua_pushinteger(L, KEY_FIVE);      lua_setfield(L, table_idx, "KEY_FIVE");
    lua_pushinteger(L, KEY_SIX);       lua_setfield(L, table_idx, "KEY_SIX");
    lua_pushinteger(L, KEY_SEVEN);     lua_setfield(L, table_idx, "KEY_SEVEN");
    lua_pushinteger(L, KEY_EIGHT);     lua_setfield(L, table_idx, "KEY_EIGHT");
    lua_pushinteger(L, KEY_NINE);      lua_setfield(L, table_idx, "KEY_NINE");
    lua_pushinteger(L, KEY_F1);        lua_setfield(L, table_idx, "KEY_F1");
    lua_pushinteger(L, KEY_F2);        lua_setfield(L, table_idx, "KEY_F2");
    lua_pushinteger(L, KEY_F3);        lua_setfield(L, table_idx, "KEY_F3");
    lua_pushinteger(L, KEY_F4);        lua_setfield(L, table_idx, "KEY_F4");
    lua_pushinteger(L, KEY_F5);        lua_setfield(L, table_idx, "KEY_F5");
    lua_pushinteger(L, KEY_F6);        lua_setfield(L, table_idx, "KEY_F6");
    lua_pushinteger(L, KEY_F7);        lua_setfield(L, table_idx, "KEY_F7");
    lua_pushinteger(L, KEY_F8);        lua_setfield(L, table_idx, "KEY_F8");
    lua_pushinteger(L, KEY_F9);        lua_setfield(L, table_idx, "KEY_F9");
    lua_pushinteger(L, KEY_F10);       lua_setfield(L, table_idx, "KEY_F10");
    lua_pushinteger(L, KEY_F11);       lua_setfield(L, table_idx, "KEY_F11");
    lua_pushinteger(L, KEY_F12);       lua_setfield(L, table_idx, "KEY_F12");
    lua_pushinteger(L, KEY_LEFT_SHIFT);   lua_setfield(L, table_idx, "KEY_LEFT_SHIFT");
    lua_pushinteger(L, KEY_LEFT_CONTROL); lua_setfield(L, table_idx, "KEY_LEFT_CONTROL");
    lua_pushinteger(L, KEY_LEFT_ALT);     lua_setfield(L, table_idx, "KEY_LEFT_ALT");
    lua_pushinteger(L, KEY_RIGHT_SHIFT);  lua_setfield(L, table_idx, "KEY_RIGHT_SHIFT");
    lua_pushinteger(L, KEY_RIGHT_CONTROL);lua_setfield(L, table_idx, "KEY_RIGHT_CONTROL");
    lua_pushinteger(L, KEY_RIGHT_ALT);    lua_setfield(L, table_idx, "KEY_RIGHT_ALT");

    /* ---- Mouse buttons ---- */
    lua_pushinteger(L, MOUSE_BUTTON_LEFT);   lua_setfield(L, table_idx, "MOUSE_BUTTON_LEFT");
    lua_pushinteger(L, MOUSE_BUTTON_RIGHT);  lua_setfield(L, table_idx, "MOUSE_BUTTON_RIGHT");
    lua_pushinteger(L, MOUSE_BUTTON_MIDDLE); lua_setfield(L, table_idx, "MOUSE_BUTTON_MIDDLE");

    /* ---- Colors (as tables with r,g,b,a) ---- */
#define PUSH_COLOR(name, rv,gv,bv,av) do { \
    lua_createtable(L, 0, 4); \
    lua_pushinteger(L, rv); lua_setfield(L, -2, "r"); \
    lua_pushinteger(L, gv); lua_setfield(L, -2, "g"); \
    lua_pushinteger(L, bv); lua_setfield(L, -2, "b"); \
    lua_pushinteger(L, av); lua_setfield(L, -2, "a"); \
    lua_setfield(L, table_idx, name); \
} while(0)
    PUSH_COLOR("LIGHTGRAY",  200, 200, 200, 255);
    PUSH_COLOR("GRAY",       130, 130, 130, 255);
    PUSH_COLOR("DARKGRAY",    80,  80,  80, 255);
    PUSH_COLOR("YELLOW",     253, 249,   0, 255);
    PUSH_COLOR("GOLD",       255, 203,   0, 255);
    PUSH_COLOR("ORANGE",     255, 161,   0, 255);
    PUSH_COLOR("PINK",       255, 109, 194, 255);
    PUSH_COLOR("RED",        230,  41,  55, 255);
    PUSH_COLOR("MAROON",     190,  33,  55, 255);
    PUSH_COLOR("GREEN",        0, 228,  48, 255);
    PUSH_COLOR("LIME",         0, 158,  47, 255);
    PUSH_COLOR("DARKGREEN",    0, 117,  44, 255);
    PUSH_COLOR("SKYBLUE",    102, 191, 255, 255);
    PUSH_COLOR("BLUE",         0, 121, 241, 255);
    PUSH_COLOR("DARKBLUE",     0,  82, 172, 255);
    PUSH_COLOR("PURPLE",     200, 122, 255, 255);
    PUSH_COLOR("VIOLET",     135,  60, 190, 255);
    PUSH_COLOR("DARKPURPLE", 112,  31, 126, 255);
    PUSH_COLOR("BEIGE",      211, 176, 131, 255);
    PUSH_COLOR("BROWN",      127, 106,  79, 255);
    PUSH_COLOR("DARKBROWN",   76,  63,  47, 255);
    PUSH_COLOR("WHITE",      255, 255, 255, 255);
    PUSH_COLOR("BLACK",        0,   0,   0, 255);
    PUSH_COLOR("BLANK",        0,   0,   0,   0);
    PUSH_COLOR("MAGENTA",    255,   0, 255, 255);
    PUSH_COLOR("RAYWHITE",   245, 245, 245, 255);
#undef PUSH_COLOR

    /* ---- Camera projection ---- */
    lua_pushinteger(L, CAMERA_PERSPECTIVE);  lua_setfield(L, table_idx, "CAMERA_PERSPECTIVE");
    lua_pushinteger(L, CAMERA_ORTHOGRAPHIC); lua_setfield(L, table_idx, "CAMERA_ORTHOGRAPHIC");
}

/* ============================================================
 * ENTRY POINT — called by Lua's require("gcl_raylib")
 * ============================================================ */
int __declspec(dllexport) luaopen_gcl_raylib(lua_State *L) {
    luaL_newlib(L, raylib_funcs);
    push_constants(L, lua_gettop(L));
    return 1;
}
