/*
 * gcl_luaraylib.c — GCL Embed Lua + Raylib binding.
 *
 * Lua tarafında `raylib` global tablosunu oluşturur:
 *   raylib.InitWindow(800, 600, "title")
 *   raylib.Rectangle(0, 0, 800, 30)  → userdata (raygui modülüne geçer)
 *   raylib.RAYWHITE / raylib.RED ...  → packed 32-bit renk integer
 *
 * Lua C module: luaopen_LuaRaylib (LuaRaylib.dll|.so olarak yüklenir).
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#define GCL_LUA_EXPORT __declspec(dllexport)
#else
#define GCL_LUA_EXPORT __attribute__((visibility("default")))
#endif

#define RECT_MT "__GclRaylibRect"

typedef struct {
    float x, y, w, h;
} LuaRect;

/* ---------- Renk yardımcıları ---------- */

static unsigned int color_to_uint(Color c) {
    return ((unsigned int)c.r) |
           (((unsigned int)c.g) << 8) |
           (((unsigned int)c.b) << 16) |
           (((unsigned int)c.a) << 24);
}

static Color uint_to_color(unsigned int v) {
    Color c;
    c.r = (unsigned char)(v & 0xFF);
    c.g = (unsigned char)((v >> 8) & 0xFF);
    c.b = (unsigned char)((v >> 16) & 0xFF);
    c.a = (unsigned char)((v >> 24) & 0xFF);
    return c;
}

static void push_color(lua_State *L, Color c) {
    lua_pushinteger(L, (lua_Integer)color_to_uint(c));
}

static LuaRect *check_rect(lua_State *L, int idx) {
    return (LuaRect *)luaL_checkudata(L, idx, RECT_MT);
}

/* ---------- Window ---------- */

static int l_init_window(lua_State *L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    const char *title = luaL_checkstring(L, 3);
    InitWindow(w, h, title);
    return 0;
}

static int l_window_should_close(lua_State *L) {
    lua_pushboolean(L, WindowShouldClose());
    return 1;
}

static int l_close_window(lua_State *L) {
    CloseWindow();
    return 0;
}

static int l_set_target_fps(lua_State *L) {
    SetTargetFPS((int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_set_window_size(lua_State *L) {
    SetWindowSize((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}

static int l_get_screen_width(lua_State *L) {
    lua_pushinteger(L, GetScreenWidth());
    return 1;
}

static int l_get_screen_height(lua_State *L) {
    lua_pushinteger(L, GetScreenHeight());
    return 1;
}

/* ---------- Drawing ---------- */

static int l_begin_drawing(lua_State *L) {
    BeginDrawing();
    return 0;
}

static int l_end_drawing(lua_State *L) {
    EndDrawing();
    return 0;
}

static int l_clear_background(lua_State *L) {
    unsigned int c = (unsigned int)luaL_checkinteger(L, 1);
    ClearBackground(uint_to_color(c));
    return 0;
}

static int l_draw_rectangle(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    Color c = uint_to_color((unsigned int)luaL_checkinteger(L, 5));
    DrawRectangle(x, y, w, h, c);
    return 0;
}

static int l_draw_text(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int size = (int)luaL_checkinteger(L, 4);
    Color c = uint_to_color((unsigned int)luaL_checkinteger(L, 5));
    DrawText(text, x, y, size, c);
    return 0;
}

static int l_draw_circle(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    float r = (float)luaL_checknumber(L, 3);
    Color c = uint_to_color((unsigned int)luaL_checkinteger(L, 4));
    DrawCircle(x, y, r, c);
    return 0;
}

static int l_draw_line(lua_State *L) {
    int x1 = (int)luaL_checkinteger(L, 1);
    int y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3);
    int y2 = (int)luaL_checkinteger(L, 4);
    Color c = uint_to_color((unsigned int)luaL_checkinteger(L, 5));
    DrawLine(x1, y1, x2, y2, c);
    return 0;
}

static int l_draw_fps(lua_State *L) {
    DrawFPS((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}

/* ---------- Input ---------- */

static int l_is_key_pressed(lua_State *L) {
    lua_pushboolean(L, IsKeyPressed((int)luaL_checkinteger(L, 1)));
    return 1;
}

static int l_is_key_down(lua_State *L) {
    lua_pushboolean(L, IsKeyDown((int)luaL_checkinteger(L, 1)));
    return 1;
}

static int l_get_mouse_x(lua_State *L) {
    lua_pushinteger(L, GetMouseX());
    return 1;
}

static int l_get_mouse_y(lua_State *L) {
    lua_pushinteger(L, GetMouseY());
    return 1;
}

/* ---------- Rectangle / Vector2 ---------- */

static int l_rectangle(lua_State *L) {
    LuaRect *r = (LuaRect *)lua_newuserdata(L, sizeof(LuaRect));
    luaL_getmetatable(L, RECT_MT);
    lua_setmetatable(L, -2);
    r->x = (float)luaL_checknumber(L, 1);
    r->y = (float)luaL_checknumber(L, 2);
    r->w = (float)luaL_checknumber(L, 3);
    r->h = (float)luaL_checknumber(L, 4);
    return 1;
}

static int l_rect_get(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *field = luaL_checkstring(L, 2);
    if (strcmp(field, "x") == 0) lua_pushnumber(L, r->x);
    else if (strcmp(field, "y") == 0) lua_pushnumber(L, r->y);
    else if (strcmp(field, "w") == 0) lua_pushnumber(L, r->w);
    else if (strcmp(field, "h") == 0) lua_pushnumber(L, r->h);
    else lua_pushnil(L);
    return 1;
}

static int l_rect_set(lua_State *L) {
    LuaRect *r = check_rect(L, 1);
    const char *field = luaL_checkstring(L, 2);
    float v = (float)luaL_checknumber(L, 3);
    if (strcmp(field, "x") == 0) r->x = v;
    else if (strcmp(field, "y") == 0) r->y = v;
    else if (strcmp(field, "w") == 0) r->w = v;
    else if (strcmp(field, "h") == 0) r->h = v;
    return 0;
}

/* =========================================================================
   FULL RAYLIB LUA WRAPPER
   ========================================================================= */

/* ---- Window / Core ---- */
static int l_is_window_ready(lua_State *L) { lua_pushboolean(L, IsWindowReady()); return 1; }
static int l_is_window_fullscreen(lua_State *L) { lua_pushboolean(L, IsWindowFullscreen()); return 1; }
static int l_is_window_hidden(lua_State *L) { lua_pushboolean(L, IsWindowHidden()); return 1; }
static int l_is_window_minimized(lua_State *L) { lua_pushboolean(L, IsWindowMinimized()); return 1; }
static int l_is_window_maximized(lua_State *L) { lua_pushboolean(L, IsWindowMaximized()); return 1; }
static int l_is_window_focused(lua_State *L) { lua_pushboolean(L, IsWindowFocused()); return 1; }
static int l_is_window_resized(lua_State *L) { lua_pushboolean(L, IsWindowResized()); return 1; }
static int l_toggle_fullscreen(lua_State *L) { ToggleFullscreen(); return 0; }
static int l_toggle_borderless_windowed(lua_State *L) { ToggleBorderlessWindowed(); return 0; }
static int l_maximize_window(lua_State *L) { MaximizeWindow(); return 0; }
static int l_minimize_window(lua_State *L) { MinimizeWindow(); return 0; }
static int l_restore_window(lua_State *L) { RestoreWindow(); return 0; }
static int l_set_window_title(lua_State *L) { SetWindowTitle(luaL_checkstring(L, 1)); return 0; }
static int l_set_window_position(lua_State *L) { SetWindowPosition((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_window_monitor(lua_State *L) { SetWindowMonitor((int)luaL_checkinteger(L,1)); return 0; }
static int l_set_window_min_size(lua_State *L) { SetWindowMinSize((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_window_max_size(lua_State *L) { SetWindowMaxSize((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_window_opacity(lua_State *L) { SetWindowOpacity((float)luaL_checknumber(L,1)); return 0; }
static int l_set_window_focused(lua_State *L) { SetWindowFocused(); return 0; }
static int l_get_render_width(lua_State *L) { lua_pushinteger(L, GetRenderWidth()); return 1; }
static int l_get_render_height(lua_State *L) { lua_pushinteger(L, GetRenderHeight()); return 1; }
static int l_get_monitor_count(lua_State *L) { lua_pushinteger(L, GetMonitorCount()); return 1; }
static int l_get_current_monitor(lua_State *L) { lua_pushinteger(L, GetCurrentMonitor()); return 1; }
static int l_get_monitor_width(lua_State *L) { lua_pushinteger(L, GetMonitorWidth((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_monitor_height(lua_State *L) { lua_pushinteger(L, GetMonitorHeight((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_monitor_refresh_rate(lua_State *L) { lua_pushinteger(L, GetMonitorRefreshRate((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_window_position(lua_State *L) { Vector2 v = GetWindowPosition(); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_get_window_scale_dpi(lua_State *L) { Vector2 v = GetWindowScaleDPI(); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_set_clipboard_text(lua_State *L) { SetClipboardText(luaL_checkstring(L, 1)); return 0; }
static int l_get_clipboard_text(lua_State *L) { lua_pushstring(L, GetClipboardText()); return 1; }
static int l_enable_event_waiting(lua_State *L) { EnableEventWaiting(); return 0; }
static int l_disable_event_waiting(lua_State *L) { DisableEventWaiting(); return 0; }
static int l_set_exit_key(lua_State *L) { SetExitKey((int)luaL_checkinteger(L,1)); return 0; }
static int l_set_config_flags(lua_State *L) { SetConfigFlags((unsigned int)luaL_checkinteger(L,1)); return 0; }

/* ---- Cursor ---- */
static int l_show_cursor(lua_State *L) { ShowCursor(); return 0; }
static int l_hide_cursor(lua_State *L) { HideCursor(); return 0; }
static int l_is_cursor_hidden(lua_State *L) { lua_pushboolean(L, IsCursorHidden()); return 1; }
static int l_enable_cursor(lua_State *L) { EnableCursor(); return 0; }
static int l_disable_cursor(lua_State *L) { DisableCursor(); return 0; }
static int l_is_cursor_on_screen(lua_State *L) { lua_pushboolean(L, IsCursorOnScreen()); return 1; }

/* ---- Drawing modes ---- */
static Camera3D g_lua_cam = {0};  /* son kamera — UpdateCamera için kalıcı state */

static void lua_read_vec3(lua_State *L, int idx, Vector3 *v) {
    lua_getfield(L, idx, "x"); v->x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, idx, "y"); v->y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, idx, "z"); v->z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
}
static int l_vector3(lua_State *L) {
    lua_newtable(L);
    lua_pushnumber(L, (float)luaL_checknumber(L, 1)); lua_setfield(L, -2, "x");
    lua_pushnumber(L, (float)luaL_checknumber(L, 2)); lua_setfield(L, -2, "y");
    lua_pushnumber(L, (float)luaL_checknumber(L, 3)); lua_setfield(L, -2, "z");
    return 1;
}
static int l_camera2d(lua_State *L) {
    lua_newtable(L);
    lua_newtable(L); lua_pushnumber(L,0); lua_setfield(L,-2,"x"); lua_pushnumber(L,0); lua_setfield(L,-2,"y"); lua_setfield(L,-2,"offset");
    lua_newtable(L); lua_pushnumber(L,0); lua_setfield(L,-2,"x"); lua_pushnumber(L,0); lua_setfield(L,-2,"y"); lua_setfield(L,-2,"target");
    lua_pushnumber(L,0.0); lua_setfield(L,-2,"rotation");
    lua_pushnumber(L,1.0); lua_setfield(L,-2,"zoom");
    return 1;
}
static int l_camera3d(lua_State *L) {
    lua_newtable(L);
    lua_newtable(L); lua_pushnumber(L,0); lua_setfield(L,-2,"x"); lua_pushnumber(L,0); lua_setfield(L,-2,"y"); lua_pushnumber(L,0); lua_setfield(L,-2,"z"); lua_setfield(L,-2,"position");
    lua_newtable(L); lua_pushnumber(L,0); lua_setfield(L,-2,"x"); lua_pushnumber(L,0); lua_setfield(L,-2,"y"); lua_pushnumber(L,0); lua_setfield(L,-2,"z"); lua_setfield(L,-2,"target");
    lua_newtable(L); lua_pushnumber(L,0); lua_setfield(L,-2,"x"); lua_pushnumber(L,0); lua_setfield(L,-2,"y"); lua_pushnumber(L,1); lua_setfield(L,-2,"z"); lua_setfield(L,-2,"up");
    lua_pushnumber(L,45.0); lua_setfield(L,-2,"fovy");
    lua_pushinteger(L,CAMERA_PERSPECTIVE); lua_setfield(L,-2,"projection");
    return 1;
}
static int l_begin_mode_2d(lua_State *L) {
    Camera2D cam = {0};
    if (lua_gettop(L) == 1 && lua_istable(L, 1)) {
        lua_getfield(L,1,"offset"); lua_getfield(L,-1,"x"); cam.offset.x=(float)lua_tonumber(L,-1); lua_pop(L,1); lua_getfield(L,-1,"y"); cam.offset.y=(float)lua_tonumber(L,-1); lua_pop(L,1); lua_pop(L,1);
        lua_getfield(L,1,"target"); lua_getfield(L,-1,"x"); cam.target.x=(float)lua_tonumber(L,-1); lua_pop(L,1); lua_getfield(L,-1,"y"); cam.target.y=(float)lua_tonumber(L,-1); lua_pop(L,1); lua_pop(L,1);
        lua_getfield(L,1,"rotation"); cam.rotation=(float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L,1,"zoom"); cam.zoom=(float)lua_tonumber(L,-1); lua_pop(L,1);
    } else {
        cam.offset.x=(float)luaL_checknumber(L,1); cam.offset.y=(float)luaL_checknumber(L,2);
        cam.target.x=(float)luaL_checknumber(L,3); cam.target.y=(float)luaL_checknumber(L,4);
        cam.rotation=(float)luaL_checknumber(L,5); cam.zoom=(float)luaL_checknumber(L,6);
    }
    BeginMode2D(cam); return 0;
}
static int l_end_mode_2d(lua_State *L) { EndMode2D(); return 0; }
static int l_begin_mode_3d(lua_State *L) {
    if (lua_gettop(L) == 1 && lua_istable(L, 1)) {
        lua_getfield(L,1,"position"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.position); lua_pop(L,1);
        lua_getfield(L,1,"target"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.target); lua_pop(L,1);
        lua_getfield(L,1,"up"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.up); lua_pop(L,1);
        lua_getfield(L,1,"fovy"); g_lua_cam.fovy=(float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L,1,"projection"); g_lua_cam.projection=(int)lua_tointeger(L,-1); lua_pop(L,1);
    } else {
        g_lua_cam.position.x=(float)luaL_checknumber(L,1); g_lua_cam.position.y=(float)luaL_checknumber(L,2); g_lua_cam.position.z=(float)luaL_checknumber(L,3);
        g_lua_cam.target.x=(float)luaL_checknumber(L,4); g_lua_cam.target.y=(float)luaL_checknumber(L,5); g_lua_cam.target.z=(float)luaL_checknumber(L,6);
        g_lua_cam.up.x=(float)luaL_checknumber(L,7); g_lua_cam.up.y=(float)luaL_checknumber(L,8); g_lua_cam.up.z=(float)luaL_checknumber(L,9);
        g_lua_cam.fovy=(float)luaL_checknumber(L,10); g_lua_cam.projection=(int)luaL_checkinteger(L,11);
    }
    BeginMode3D(g_lua_cam); return 0;
}
static int l_end_mode_3d(lua_State *L) { EndMode3D(); return 0; }
static int l_update_camera(lua_State *L) {
    int n = lua_gettop(L);
    if (n >= 2 && lua_istable(L, 1)) {
        lua_getfield(L,1,"position"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.position); lua_pop(L,1);
        lua_getfield(L,1,"target"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.target); lua_pop(L,1);
        lua_getfield(L,1,"up"); if (lua_istable(L,-1)) lua_read_vec3(L,-1,&g_lua_cam.up); lua_pop(L,1);
        lua_getfield(L,1,"fovy"); g_lua_cam.fovy=(float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L,1,"projection"); g_lua_cam.projection=(int)lua_tointeger(L,-1); lua_pop(L,1);
        UpdateCamera(&g_lua_cam, (int)luaL_checkinteger(L,2));
    } else if (n >= 12) {
        g_lua_cam.position.x=(float)luaL_checknumber(L,1); g_lua_cam.position.y=(float)luaL_checknumber(L,2); g_lua_cam.position.z=(float)luaL_checknumber(L,3);
        g_lua_cam.target.x=(float)luaL_checknumber(L,4); g_lua_cam.target.y=(float)luaL_checknumber(L,5); g_lua_cam.target.z=(float)luaL_checknumber(L,6);
        g_lua_cam.up.x=(float)luaL_checknumber(L,7); g_lua_cam.up.y=(float)luaL_checknumber(L,8); g_lua_cam.up.z=(float)luaL_checknumber(L,9);
        g_lua_cam.fovy=(float)luaL_checknumber(L,10); g_lua_cam.projection=(int)luaL_checkinteger(L,11);
        UpdateCamera(&g_lua_cam, (int)luaL_checkinteger(L,12));
    } else {
        UpdateCamera(&g_lua_cam, (int)luaL_checkinteger(L,1));
    }
    return 0;
}
static int l_begin_texture_mode(lua_State *L) {
    RenderTexture2D rt; rt.id=(unsigned int)luaL_checkinteger(L,1); rt.texture.id=(unsigned int)luaL_checkinteger(L,2);
    rt.texture.width=(int)luaL_checkinteger(L,3); rt.texture.height=(int)luaL_checkinteger(L,4);
    rt.depth.id=(unsigned int)luaL_checkinteger(L,5); BeginTextureMode(rt); return 0;
}
static int l_end_texture_mode(lua_State *L) { EndTextureMode(); return 0; }
static int l_begin_shader_mode(lua_State *L) { Shader s; s.id=(unsigned int)luaL_checkinteger(L,1); BeginShaderMode(s); return 0; }
static int l_end_shader_mode(lua_State *L) { EndShaderMode(); return 0; }
static int l_begin_blend_mode(lua_State *L) { BeginBlendMode((int)luaL_checkinteger(L,1)); return 0; }
static int l_end_blend_mode(lua_State *L) { EndBlendMode(); return 0; }
static int l_begin_scissor_mode(lua_State *L) { BeginScissorMode((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),(int)luaL_checkinteger(L,3),(int)luaL_checkinteger(L,4)); return 0; }
static int l_end_scissor_mode(lua_State *L) { EndScissorMode(); return 0; }

/* ---- Timing ---- */
static int l_get_frame_time(lua_State *L) { lua_pushnumber(L, GetFrameTime()); return 1; }
static int l_get_time(lua_State *L) { lua_pushnumber(L, GetTime()); return 1; }
static int l_get_fps(lua_State *L) { lua_pushinteger(L, GetFPS()); return 1; }
static int l_wait_time(lua_State *L) { WaitTime((double)luaL_checknumber(L,1)); return 0; }

/* ---- Shader ---- */
static int l_load_shader(lua_State *L) { Shader s = LoadShader(luaL_checkstring(L,1), luaL_checkstring(L,2)); lua_pushinteger(L, s.id); return 1; }
static int l_load_shader_from_memory(lua_State *L) { Shader s = LoadShaderFromMemory(luaL_checkstring(L,1), luaL_checkstring(L,2)); lua_pushinteger(L, s.id); return 1; }
static int l_is_shader_valid(lua_State *L) { Shader s; s.id=(unsigned int)luaL_checkinteger(L,1); lua_pushboolean(L, IsShaderValid(s)); return 1; }
static int l_get_shader_location(lua_State *L) { Shader s; s.id=(unsigned int)luaL_checkinteger(L,1); lua_pushinteger(L, GetShaderLocation(s, luaL_checkstring(L,2))); return 1; }
static int l_get_shader_location_attrib(lua_State *L) { Shader s; s.id=(unsigned int)luaL_checkinteger(L,1); lua_pushinteger(L, GetShaderLocationAttrib(s, luaL_checkstring(L,2))); return 1; }
static int l_unload_shader(lua_State *L) { Shader s; s.id=(unsigned int)luaL_checkinteger(L,1); UnloadShader(s); return 0; }

/* ---- File system ---- */
static int l_file_exists(lua_State *L) { lua_pushboolean(L, FileExists(luaL_checkstring(L,1))); return 1; }
static int l_directory_exists(lua_State *L) { lua_pushboolean(L, DirectoryExists(luaL_checkstring(L,1))); return 1; }
static int l_load_file_text(lua_State *L) { char *t = LoadFileText(luaL_checkstring(L,1)); if (t) { lua_pushstring(L, t); UnloadFileText(t); } else lua_pushnil(L); return 1; }
static int l_save_file_text(lua_State *L) { lua_pushboolean(L, SaveFileText(luaL_checkstring(L,1), luaL_checkstring(L,2))); return 1; }
static int l_get_file_extension(lua_State *L) { lua_pushstring(L, GetFileExtension(luaL_checkstring(L,1))); return 1; }
static int l_get_file_name(lua_State *L) { lua_pushstring(L, GetFileName(luaL_checkstring(L,1))); return 1; }
static int l_get_file_name_without_ext(lua_State *L) { lua_pushstring(L, GetFileNameWithoutExt(luaL_checkstring(L,1))); return 1; }
static int l_get_directory_path(lua_State *L) { lua_pushstring(L, GetDirectoryPath(luaL_checkstring(L,1))); return 1; }
static int l_get_working_directory(lua_State *L) { lua_pushstring(L, GetWorkingDirectory()); return 1; }
static int l_get_application_directory(lua_State *L) { lua_pushstring(L, GetApplicationDirectory()); return 1; }
static int l_make_directory(lua_State *L) { lua_pushinteger(L, MakeDirectory(luaL_checkstring(L,1))); return 1; }

/* ---- Random ---- */
static int l_set_random_seed(lua_State *L) { SetRandomSeed((unsigned int)luaL_checkinteger(L,1)); return 0; }
static int l_get_random_value(lua_State *L) { lua_pushinteger(L, GetRandomValue((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }

/* ---- Misc ---- */
static int l_take_screenshot(lua_State *L) { TakeScreenshot(luaL_checkstring(L,1)); return 0; }
static int l_open_url(lua_State *L) { OpenURL(luaL_checkstring(L,1)); return 0; }

/* ---- Input: keyboard ---- */
static int l_is_key_pressed_repeat(lua_State *L) { lua_pushboolean(L, IsKeyPressedRepeat((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_key_released(lua_State *L) { lua_pushboolean(L, IsKeyReleased((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_key_up(lua_State *L) { lua_pushboolean(L, IsKeyUp((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_key_pressed(lua_State *L) { lua_pushinteger(L, GetKeyPressed()); return 1; }
static int l_get_char_pressed(lua_State *L) { lua_pushinteger(L, GetCharPressed()); return 1; }
static int l_get_key_name(lua_State *L) { lua_pushstring(L, GetKeyName((int)luaL_checkinteger(L,1))); return 1; }

/* ---- Input: gamepad ---- */
static int l_is_gamepad_available(lua_State *L) { lua_pushboolean(L, IsGamepadAvailable((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_gamepad_name(lua_State *L) { lua_pushstring(L, GetGamepadName((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_gamepad_button_pressed(lua_State *L) { lua_pushboolean(L, IsGamepadButtonPressed((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_is_gamepad_button_down(lua_State *L) { lua_pushboolean(L, IsGamepadButtonDown((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_is_gamepad_button_released(lua_State *L) { lua_pushboolean(L, IsGamepadButtonReleased((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_is_gamepad_button_up(lua_State *L) { lua_pushboolean(L, IsGamepadButtonUp((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_get_gamepad_button_pressed(lua_State *L) { lua_pushinteger(L, GetGamepadButtonPressed()); return 1; }
static int l_get_gamepad_axis_count(lua_State *L) { lua_pushinteger(L, GetGamepadAxisCount((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_gamepad_axis_movement(lua_State *L) { lua_pushnumber(L, GetGamepadAxisMovement((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_set_gamepad_mappings(lua_State *L) { lua_pushinteger(L, SetGamepadMappings(luaL_checkstring(L,1))); return 1; }
static int l_set_gamepad_vibration(lua_State *L) { SetGamepadVibration((int)luaL_checkinteger(L,1), (float)luaL_checknumber(L,2), (float)luaL_checknumber(L,3), (float)luaL_checknumber(L,4)); return 0; }

/* ---- Input: mouse ---- */
static int l_is_mouse_button_pressed(lua_State *L) { lua_pushboolean(L, IsMouseButtonPressed((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_mouse_button_down(lua_State *L) { lua_pushboolean(L, IsMouseButtonDown((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_mouse_button_released(lua_State *L) { lua_pushboolean(L, IsMouseButtonReleased((int)luaL_checkinteger(L,1))); return 1; }
static int l_is_mouse_button_up(lua_State *L) { lua_pushboolean(L, IsMouseButtonUp((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_mouse_position(lua_State *L) { Vector2 v = GetMousePosition(); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_get_mouse_delta(lua_State *L) { Vector2 v = GetMouseDelta(); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_set_mouse_position(lua_State *L) { SetMousePosition((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_mouse_offset(lua_State *L) { SetMouseOffset((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_mouse_scale(lua_State *L) { SetMouseScale((float)luaL_checknumber(L,1), (float)luaL_checknumber(L,2)); return 0; }
static int l_get_mouse_wheel_move(lua_State *L) { lua_pushnumber(L, GetMouseWheelMove()); return 1; }
static int l_set_mouse_cursor(lua_State *L) { SetMouseCursor((int)luaL_checkinteger(L,1)); return 0; }

/* ---- Input: touch ---- */
static int l_get_touch_x(lua_State *L) { lua_pushinteger(L, GetTouchX()); return 1; }
static int l_get_touch_y(lua_State *L) { lua_pushinteger(L, GetTouchY()); return 1; }
static int l_get_touch_position(lua_State *L) { Vector2 v = GetTouchPosition((int)luaL_checkinteger(L,1)); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_get_touch_point_id(lua_State *L) { lua_pushinteger(L, GetTouchPointId((int)luaL_checkinteger(L,1))); return 1; }
static int l_get_touch_point_count(lua_State *L) { lua_pushinteger(L, GetTouchPointCount()); return 1; }

/* ---- Gestures ---- */
static int l_set_gestures_enabled(lua_State *L) { SetGesturesEnabled((unsigned int)luaL_checkinteger(L,1)); return 0; }
static int l_is_gesture_detected(lua_State *L) { lua_pushboolean(L, IsGestureDetected((unsigned int)luaL_checkinteger(L,1))); return 1; }
static int l_get_gesture_detected(lua_State *L) { lua_pushinteger(L, GetGestureDetected()); return 1; }
static int l_get_gesture_hold_duration(lua_State *L) { lua_pushnumber(L, GetGestureHoldDuration()); return 1; }
static int l_get_gesture_drag_vector(lua_State *L) { Vector2 v = GetGestureDragVector(); lua_pushnumber(L, v.x); lua_pushnumber(L, v.y); return 2; }
static int l_get_gesture_drag_angle(lua_State *L) { lua_pushnumber(L, GetGestureDragAngle()); return 1; }

/* ---- Shapes: 2D ---------- */
static int l_draw_pixel(lua_State *L) { DrawPixel((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),uint_to_color((unsigned int)luaL_checkinteger(L,3))); return 0; }
static int l_draw_line_ex(lua_State *L) { DrawLineEx((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(Vector2){(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)},(float)luaL_checknumber(L,5),uint_to_color((unsigned int)luaL_checkinteger(L,6))); return 0; }
static int l_draw_triangle(lua_State *L) { DrawTriangle((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(Vector2){(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)},(Vector2){(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6)},uint_to_color((unsigned int)luaL_checkinteger(L,7))); return 0; }
static int l_draw_rectangle_v(lua_State *L) { DrawRectangleV((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(Vector2){(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4)},uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }
static int l_draw_rectangle_rec(lua_State *L) { LuaRect *r = check_rect(L,1); DrawRectangleRec((Rectangle){r->x,r->y,r->w,r->h},uint_to_color((unsigned int)luaL_checkinteger(L,2))); return 0; }
static int l_draw_rectangle_lines(lua_State *L) { DrawRectangleLines((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),(int)luaL_checkinteger(L,3),(int)luaL_checkinteger(L,4),uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }
static int l_draw_rectangle_lines_ex(lua_State *L) { LuaRect *r = check_rect(L,1); DrawRectangleLinesEx((Rectangle){r->x,r->y,r->w,r->h},(float)luaL_checknumber(L,2),uint_to_color((unsigned int)luaL_checkinteger(L,3))); return 0; }
static int l_draw_rectangle_rounded(lua_State *L) { LuaRect *r = check_rect(L,1); DrawRectangleRounded((Rectangle){r->x,r->y,r->w,r->h},(float)luaL_checknumber(L,2),(int)luaL_checkinteger(L,3),uint_to_color((unsigned int)luaL_checkinteger(L,4))); return 0; }
static int l_draw_circle_v(lua_State *L) { DrawCircleV((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(float)luaL_checknumber(L,3),uint_to_color((unsigned int)luaL_checkinteger(L,4))); return 0; }
static int l_draw_circle_gradient(lua_State *L) { DrawCircleGradient((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(float)luaL_checknumber(L,3),uint_to_color((unsigned int)luaL_checkinteger(L,4)),uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }
static int l_draw_circle_lines(lua_State *L) { DrawCircleLines((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),uint_to_color((unsigned int)luaL_checkinteger(L,4))); return 0; }
static int l_draw_ellipse(lua_State *L) { DrawEllipse((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }
static int l_draw_poly(lua_State *L) { DrawPoly((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(int)luaL_checkinteger(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),uint_to_color((unsigned int)luaL_checkinteger(L,6))); return 0; }
static int l_draw_poly_lines(lua_State *L) { DrawPolyLines((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(int)luaL_checkinteger(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),uint_to_color((unsigned int)luaL_checkinteger(L,6))); return 0; }
static int l_draw_ring(lua_State *L) { DrawRing((Vector2){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2)},(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6),(int)luaL_checkinteger(L,7),uint_to_color((unsigned int)luaL_checkinteger(L,8))); return 0; }
static int l_draw_grid(lua_State *L) { DrawGrid((int)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2)); return 0; }

/* ---- Textures / Images ---- */
static int l_load_texture(lua_State *L) { Texture2D t = LoadTexture(luaL_checkstring(L,1)); lua_pushinteger(L, t.id); return 1; }
static int l_load_texture_from_image(lua_State *L) { Image img; img.data=(void*)lua_touserdata(L,1); img.width=(int)luaL_checkinteger(L,2); img.height=(int)luaL_checkinteger(L,3); Texture2D t = LoadTextureFromImage(img); lua_pushinteger(L, t.id); return 1; }
static int l_unload_texture(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); UnloadTexture(t); return 0; }
static int l_is_texture_valid(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); lua_pushboolean(L, IsTextureValid(t)); return 1; }
static int l_set_texture_filter(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); SetTextureFilter(t, (int)luaL_checkinteger(L,2)); return 0; }
static int l_set_texture_wrap(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); SetTextureWrap(t, (int)luaL_checkinteger(L,2)); return 0; }
static int l_draw_texture(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); DrawTexture(t, (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3), uint_to_color((unsigned int)luaL_checkinteger(L,4))); return 0; }
static int l_draw_texture_ex(lua_State *L) { Texture2D t; t.id=(unsigned int)luaL_checkinteger(L,1); DrawTextureEx(t, (Vector2){(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, (float)luaL_checknumber(L,4), (float)luaL_checknumber(L,5), uint_to_color((unsigned int)luaL_checkinteger(L,6))); return 0; }
static int l_load_image(lua_State *L) { Image img = LoadImage(luaL_checkstring(L,1)); lua_pushlightuserdata(L, img.data); lua_pushinteger(L, img.width); lua_pushinteger(L, img.height); return 3; }
static int l_unload_image(lua_State *L) { Image img; img.data=lua_touserdata(L,1); img.width=(int)luaL_checkinteger(L,2); img.height=(int)luaL_checkinteger(L,3); UnloadImage(img); return 0; }
static int l_gen_image_color(lua_State *L) { Image img = GenImageColor((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),uint_to_color((unsigned int)luaL_checkinteger(L,3))); lua_pushlightuserdata(L, img.data); lua_pushinteger(L, img.width); lua_pushinteger(L, img.height); return 3; }

/* ---- Color helpers ---- */
static int l_fade(lua_State *L) { Color c = uint_to_color((unsigned int)luaL_checkinteger(L,1)); float a=(float)luaL_checknumber(L,2); lua_pushinteger(L, color_to_uint(Fade(c,a))); return 1; }
static int l_color_to_int(lua_State *L) { Color c = uint_to_color((unsigned int)luaL_checkinteger(L,1)); lua_pushinteger(L, ColorToInt(c)); return 1; }
static int l_color_from_hsv(lua_State *L) { Color c = ColorFromHSV((float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)); lua_pushinteger(L, color_to_uint(c)); return 1; }
static int l_color_alpha(lua_State *L) { Color c = uint_to_color((unsigned int)luaL_checkinteger(L,1)); float a=(float)luaL_checknumber(L,2); lua_pushinteger(L, color_to_uint(ColorAlpha(c,a))); return 1; }
static int l_color_tint(lua_State *L) { Color a=uint_to_color((unsigned int)luaL_checkinteger(L,1)); Color b=uint_to_color((unsigned int)luaL_checkinteger(L,2)); lua_pushinteger(L, color_to_uint(ColorTint(a,b))); return 1; }
static int l_color_brightness(lua_State *L) { Color c=uint_to_color((unsigned int)luaL_checkinteger(L,1)); float f=(float)luaL_checknumber(L,2); lua_pushinteger(L, color_to_uint(ColorBrightness(c,f))); return 1; }
static int l_color_contrast(lua_State *L) { Color c=uint_to_color((unsigned int)luaL_checkinteger(L,1)); float f=(float)luaL_checknumber(L,2); lua_pushinteger(L, color_to_uint(ColorContrast(c,f))); return 1; }
static int l_get_color(lua_State *L) { Color c = GetColor((unsigned int)luaL_checkinteger(L,1)); lua_pushinteger(L, color_to_uint(c)); return 1; }

/* ---- Text / Font ---- */
static int l_get_font_default(lua_State *L) { Font f = GetFontDefault(); lua_pushinteger(L, f.baseSize); lua_pushinteger(L, f.texture.id); return 2; }
static int l_load_font(lua_State *L) { Font f = LoadFont(luaL_checkstring(L,1)); lua_pushinteger(L, f.baseSize); lua_pushinteger(L, f.texture.id); return 2; }
static int l_load_font_ex(lua_State *L) { Font f = LoadFontEx(luaL_checkstring(L,1), (int)luaL_checkinteger(L,2), NULL, 0); lua_pushinteger(L, f.baseSize); lua_pushinteger(L, f.texture.id); return 2; }
static int l_unload_font(lua_State *L) { Font f; f.baseSize=(int)luaL_checkinteger(L,1); f.texture.id=(unsigned int)luaL_checkinteger(L,2); UnloadFont(f); return 0; }
static int l_draw_text_ex(lua_State *L) { Font f; f.baseSize=(int)luaL_checkinteger(L,1); f.texture.id=(unsigned int)luaL_checkinteger(L,2); DrawTextEx(f, luaL_checkstring(L,3), (Vector2){(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5)}, (float)luaL_checknumber(L,6), (float)luaL_checknumber(L,7), uint_to_color((unsigned int)luaL_checkinteger(L,8))); return 0; }
static int l_measure_text(lua_State *L) { lua_pushinteger(L, MeasureText(luaL_checkstring(L,1), (int)luaL_checkinteger(L,2))); return 1; }
static int l_text_format(lua_State *L) { lua_pushstring(L, TextFormat(luaL_checkstring(L,1))); return 1; }
static int l_text_to_upper(lua_State *L) { char *s = TextToUpper(luaL_checkstring(L,1)); lua_pushstring(L, s); return 1; }
static int l_text_to_lower(lua_State *L) { char *s = TextToLower(luaL_checkstring(L,1)); lua_pushstring(L, s); return 1; }
static int l_text_to_integer(lua_State *L) { lua_pushinteger(L, TextToInteger(luaL_checkstring(L,1))); return 1; }
static int l_text_to_float(lua_State *L) { lua_pushnumber(L, TextToFloat(luaL_checkstring(L,1))); return 1; }
static int l_text_length(lua_State *L) { lua_pushinteger(L, TextLength(luaL_checkstring(L,1))); return 1; }

/* ---- 3D Models ---- */
static int l_draw_cube(lua_State *L) {
    Vector3 pos = {0};
    int b = 4;
    if (lua_istable(L, 1)) { lua_read_vec3(L, 1, &pos); b = 2; }
    else { pos.x=(float)luaL_checknumber(L,1); pos.y=(float)luaL_checknumber(L,2); pos.z=(float)luaL_checknumber(L,3); }
    DrawCube(pos, (float)luaL_checknumber(L,b), (float)luaL_checknumber(L,b+1), (float)luaL_checknumber(L,b+2), uint_to_color((unsigned int)luaL_checkinteger(L,b+3))); return 0;
}
static int l_draw_cube_wires(lua_State *L) {
    Vector3 pos = {0};
    int b = 4;
    if (lua_istable(L, 1)) { lua_read_vec3(L, 1, &pos); b = 2; }
    else { pos.x=(float)luaL_checknumber(L,1); pos.y=(float)luaL_checknumber(L,2); pos.z=(float)luaL_checknumber(L,3); }
    DrawCubeWires(pos, (float)luaL_checknumber(L,b), (float)luaL_checknumber(L,b+1), (float)luaL_checknumber(L,b+2), uint_to_color((unsigned int)luaL_checkinteger(L,b+3))); return 0;
}
static int l_draw_sphere(lua_State *L) { DrawSphere((Vector3){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, (float)luaL_checknumber(L,4), uint_to_color((unsigned int)luaL_checkinteger(L,5))); return 0; }
static int l_draw_cylinder(lua_State *L) { DrawCylinder((Vector3){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, (float)luaL_checknumber(L,4), (float)luaL_checknumber(L,5), (float)luaL_checknumber(L,6), (int)luaL_checkinteger(L,7), uint_to_color((unsigned int)luaL_checkinteger(L,8))); return 0; }
static int l_draw_plane(lua_State *L) { DrawPlane((Vector3){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, (Vector2){(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5)}, uint_to_color((unsigned int)luaL_checkinteger(L,6))); return 0; }
static int l_draw_line_3d(lua_State *L) { DrawLine3D((Vector3){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, (Vector3){(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6)}, uint_to_color((unsigned int)luaL_checkinteger(L,7))); return 0; }
static int l_draw_point_3d(lua_State *L) { DrawPoint3D((Vector3){(float)luaL_checknumber(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3)}, uint_to_color((unsigned int)luaL_checkinteger(L,4))); return 0; }

/* ---- Audio ---- */
static int l_init_audio_device(lua_State *L) { InitAudioDevice(); return 0; }
static int l_close_audio_device(lua_State *L) { CloseAudioDevice(); return 0; }
static int l_is_audio_device_ready(lua_State *L) { lua_pushboolean(L, IsAudioDeviceReady()); return 1; }
static int l_set_master_volume(lua_State *L) { SetMasterVolume((float)luaL_checknumber(L,1)); return 0; }
static int l_get_master_volume(lua_State *L) { lua_pushnumber(L, GetMasterVolume()); return 1; }
static int l_load_sound(lua_State *L) { Sound s = LoadSound(luaL_checkstring(L,1)); lua_pushinteger(L, s.stream.sampleRate); return 1; }
static int l_unload_sound(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); UnloadSound(s); return 0; }
static int l_play_sound(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); PlaySound(s); return 0; }
static int l_stop_sound(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); StopSound(s); return 0; }
static int l_pause_sound(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); PauseSound(s); return 0; }
static int l_resume_sound(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); ResumeSound(s); return 0; }
static int l_is_sound_playing(lua_State *L) { Sound s; s.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); lua_pushboolean(L, IsSoundPlaying(s)); return 1; }
static int l_load_music_stream(lua_State *L) { Music m = LoadMusicStream(luaL_checkstring(L,1)); lua_pushinteger(L, m.stream.sampleRate); return 1; }
static int l_play_music_stream(lua_State *L) { Music m; m.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); PlayMusicStream(m); return 0; }
static int l_update_music_stream(lua_State *L) { Music m; m.stream.sampleRate=(unsigned int)luaL_checkinteger(L,1); UpdateMusicStream(m); return 0; }

/* ---------- Fonksiyon tablosu ---------- */

static const luaL_Reg raylib_funcs[] = {
    {"InitWindow", l_init_window},
    {"WindowShouldClose", l_window_should_close},
    {"CloseWindow", l_close_window},
    {"SetTargetFPS", l_set_target_fps},
    {"SetWindowSize", l_set_window_size},
    {"GetScreenWidth", l_get_screen_width},
    {"GetScreenHeight", l_get_screen_height},

    {"BeginDrawing", l_begin_drawing},
    {"EndDrawing", l_end_drawing},
    {"ClearBackground", l_clear_background},
    {"DrawRectangle", l_draw_rectangle},
    {"DrawText", l_draw_text},
    {"DrawCircle", l_draw_circle},
    {"DrawLine", l_draw_line},
    {"DrawFPS", l_draw_fps},

    {"IsKeyPressed", l_is_key_pressed},
    {"IsKeyDown", l_is_key_down},
    {"GetMouseX", l_get_mouse_x},
    {"GetMouseY", l_get_mouse_y},

    {"Rectangle", l_rectangle},
    {"Camera2D", l_camera2d},
    {"Camera3D", l_camera3d},
    {"Vector3", l_vector3},

    /* Window / Core */
    {"IsWindowReady", l_is_window_ready},
    {"IsWindowFullscreen", l_is_window_fullscreen},
    {"IsWindowHidden", l_is_window_hidden},
    {"IsWindowMinimized", l_is_window_minimized},
    {"IsWindowMaximized", l_is_window_maximized},
    {"IsWindowFocused", l_is_window_focused},
    {"IsWindowResized", l_is_window_resized},
    {"ToggleFullscreen", l_toggle_fullscreen},
    {"ToggleBorderlessWindowed", l_toggle_borderless_windowed},
    {"MaximizeWindow", l_maximize_window},
    {"MinimizeWindow", l_minimize_window},
    {"RestoreWindow", l_restore_window},
    {"SetWindowTitle", l_set_window_title},
    {"SetWindowPosition", l_set_window_position},
    {"SetWindowMonitor", l_set_window_monitor},
    {"SetWindowMinSize", l_set_window_min_size},
    {"SetWindowMaxSize", l_set_window_max_size},
    {"SetWindowOpacity", l_set_window_opacity},
    {"SetWindowFocused", l_set_window_focused},
    {"GetRenderWidth", l_get_render_width},
    {"GetRenderHeight", l_get_render_height},
    {"GetMonitorCount", l_get_monitor_count},
    {"GetCurrentMonitor", l_get_current_monitor},
    {"GetMonitorWidth", l_get_monitor_width},
    {"GetMonitorHeight", l_get_monitor_height},
    {"GetMonitorRefreshRate", l_get_monitor_refresh_rate},
    {"GetWindowPosition", l_get_window_position},
    {"GetWindowScaleDPI", l_get_window_scale_dpi},
    {"SetClipboardText", l_set_clipboard_text},
    {"GetClipboardText", l_get_clipboard_text},
    {"EnableEventWaiting", l_enable_event_waiting},
    {"DisableEventWaiting", l_disable_event_waiting},
    {"SetExitKey", l_set_exit_key},
    {"SetConfigFlags", l_set_config_flags},

    /* Cursor */
    {"ShowCursor", l_show_cursor},
    {"HideCursor", l_hide_cursor},
    {"IsCursorHidden", l_is_cursor_hidden},
    {"EnableCursor", l_enable_cursor},
    {"DisableCursor", l_disable_cursor},
    {"IsCursorOnScreen", l_is_cursor_on_screen},

    /* Drawing modes */
    {"BeginMode2D", l_begin_mode_2d},
    {"EndMode2D", l_end_mode_2d},
    {"BeginMode3D", l_begin_mode_3d},
    {"EndMode3D", l_end_mode_3d},
    {"UpdateCamera", l_update_camera},
    {"BeginTextureMode", l_begin_texture_mode},
    {"EndTextureMode", l_end_texture_mode},
    {"BeginShaderMode", l_begin_shader_mode},
    {"EndShaderMode", l_end_shader_mode},
    {"BeginBlendMode", l_begin_blend_mode},
    {"EndBlendMode", l_end_blend_mode},
    {"BeginScissorMode", l_begin_scissor_mode},
    {"EndScissorMode", l_end_scissor_mode},

    /* Timing */
    {"GetFrameTime", l_get_frame_time},
    {"GetTime", l_get_time},
    {"GetFPS", l_get_fps},
    {"WaitTime", l_wait_time},

    /* Shader */
    {"LoadShader", l_load_shader},
    {"LoadShaderFromMemory", l_load_shader_from_memory},
    {"IsShaderValid", l_is_shader_valid},
    {"GetShaderLocation", l_get_shader_location},
    {"GetShaderLocationAttrib", l_get_shader_location_attrib},
    {"UnloadShader", l_unload_shader},

    /* File system */
    {"FileExists", l_file_exists},
    {"DirectoryExists", l_directory_exists},
    {"LoadFileText", l_load_file_text},
    {"SaveFileText", l_save_file_text},
    {"GetFileExtension", l_get_file_extension},
    {"GetFileName", l_get_file_name},
    {"GetFileNameWithoutExt", l_get_file_name_without_ext},
    {"GetDirectoryPath", l_get_directory_path},
    {"GetWorkingDirectory", l_get_working_directory},
    {"GetApplicationDirectory", l_get_application_directory},
    {"MakeDirectory", l_make_directory},

    /* Random */
    {"SetRandomSeed", l_set_random_seed},
    {"GetRandomValue", l_get_random_value},

    /* Misc */
    {"TakeScreenshot", l_take_screenshot},
    {"OpenURL", l_open_url},

    /* Input: keyboard */
    {"IsKeyPressedRepeat", l_is_key_pressed_repeat},
    {"IsKeyReleased", l_is_key_released},
    {"IsKeyUp", l_is_key_up},
    {"GetKeyPressed", l_get_key_pressed},
    {"GetCharPressed", l_get_char_pressed},
    {"GetKeyName", l_get_key_name},

    /* Input: gamepad */
    {"IsGamepadAvailable", l_is_gamepad_available},
    {"GetGamepadName", l_get_gamepad_name},
    {"IsGamepadButtonPressed", l_is_gamepad_button_pressed},
    {"IsGamepadButtonDown", l_is_gamepad_button_down},
    {"IsGamepadButtonReleased", l_is_gamepad_button_released},
    {"IsGamepadButtonUp", l_is_gamepad_button_up},
    {"GetGamepadButtonPressed", l_get_gamepad_button_pressed},
    {"GetGamepadAxisCount", l_get_gamepad_axis_count},
    {"GetGamepadAxisMovement", l_get_gamepad_axis_movement},
    {"SetGamepadMappings", l_set_gamepad_mappings},
    {"SetGamepadVibration", l_set_gamepad_vibration},

    /* Input: mouse */
    {"IsMouseButtonPressed", l_is_mouse_button_pressed},
    {"IsMouseButtonDown", l_is_mouse_button_down},
    {"IsMouseButtonReleased", l_is_mouse_button_released},
    {"IsMouseButtonUp", l_is_mouse_button_up},
    {"GetMousePosition", l_get_mouse_position},
    {"GetMouseDelta", l_get_mouse_delta},
    {"SetMousePosition", l_set_mouse_position},
    {"SetMouseOffset", l_set_mouse_offset},
    {"SetMouseScale", l_set_mouse_scale},
    {"GetMouseWheelMove", l_get_mouse_wheel_move},
    {"SetMouseCursor", l_set_mouse_cursor},

    /* Input: touch */
    {"GetTouchX", l_get_touch_x},
    {"GetTouchY", l_get_touch_y},
    {"GetTouchPosition", l_get_touch_position},
    {"GetTouchPointId", l_get_touch_point_id},
    {"GetTouchPointCount", l_get_touch_point_count},

    /* Gestures */
    {"SetGesturesEnabled", l_set_gestures_enabled},
    {"IsGestureDetected", l_is_gesture_detected},
    {"GetGestureDetected", l_get_gesture_detected},
    {"GetGestureHoldDuration", l_get_gesture_hold_duration},
    {"GetGestureDragVector", l_get_gesture_drag_vector},
    {"GetGestureDragAngle", l_get_gesture_drag_angle},

    /* Shapes */
    {"DrawPixel", l_draw_pixel},
    {"DrawLineEx", l_draw_line_ex},
    {"DrawTriangle", l_draw_triangle},
    {"DrawRectangleV", l_draw_rectangle_v},
    {"DrawRectangleRec", l_draw_rectangle_rec},
    {"DrawRectangleLines", l_draw_rectangle_lines},
    {"DrawRectangleLinesEx", l_draw_rectangle_lines_ex},
    {"DrawRectangleRounded", l_draw_rectangle_rounded},
    {"DrawCircleV", l_draw_circle_v},
    {"DrawCircleGradient", l_draw_circle_gradient},
    {"DrawCircleLines", l_draw_circle_lines},
    {"DrawEllipse", l_draw_ellipse},
    {"DrawPoly", l_draw_poly},
    {"DrawPolyLines", l_draw_poly_lines},
    {"DrawRing", l_draw_ring},
    {"DrawGrid", l_draw_grid},

    /* Textures / Images */
    {"LoadTexture", l_load_texture},
    {"LoadTextureFromImage", l_load_texture_from_image},
    {"UnloadTexture", l_unload_texture},
    {"IsTextureValid", l_is_texture_valid},
    {"SetTextureFilter", l_set_texture_filter},
    {"SetTextureWrap", l_set_texture_wrap},
    {"DrawTexture", l_draw_texture},
    {"DrawTextureEx", l_draw_texture_ex},
    {"LoadImage", l_load_image},
    {"UnloadImage", l_unload_image},
    {"GenImageColor", l_gen_image_color},

    /* Color helpers */
    {"Fade", l_fade},
    {"ColorToInt", l_color_to_int},
    {"ColorFromHSV", l_color_from_hsv},
    {"ColorAlpha", l_color_alpha},
    {"ColorTint", l_color_tint},
    {"ColorBrightness", l_color_brightness},
    {"ColorContrast", l_color_contrast},
    {"GetColor", l_get_color},

    /* Text / Font */
    {"GetFontDefault", l_get_font_default},
    {"LoadFont", l_load_font},
    {"LoadFontEx", l_load_font_ex},
    {"UnloadFont", l_unload_font},
    {"DrawTextEx", l_draw_text_ex},
    {"MeasureText", l_measure_text},
    {"TextFormat", l_text_format},
    {"TextToUpper", l_text_to_upper},
    {"TextToLower", l_text_to_lower},
    {"TextToInteger", l_text_to_integer},
    {"TextToFloat", l_text_to_float},
    {"TextLength", l_text_length},

    /* 3D */
    {"DrawCube", l_draw_cube},
    {"DrawCubeWires", l_draw_cube_wires},
    {"DrawSphere", l_draw_sphere},
    {"DrawCylinder", l_draw_cylinder},
    {"DrawPlane", l_draw_plane},
    {"DrawLine3D", l_draw_line_3d},
    {"DrawPoint3D", l_draw_point_3d},

    /* Audio */
    {"InitAudioDevice", l_init_audio_device},
    {"CloseAudioDevice", l_close_audio_device},
    {"IsAudioDeviceReady", l_is_audio_device_ready},
    {"SetMasterVolume", l_set_master_volume},
    {"GetMasterVolume", l_get_master_volume},
    {"LoadSound", l_load_sound},
    {"UnloadSound", l_unload_sound},
    {"PlaySound", l_play_sound},
    {"StopSound", l_stop_sound},
    {"PauseSound", l_pause_sound},
    {"ResumeSound", l_resume_sound},
    {"IsSoundPlaying", l_is_sound_playing},
    {"LoadMusicStream", l_load_music_stream},
    {"PlayMusicStream", l_play_music_stream},
    {"UpdateMusicStream", l_update_music_stream},

    {NULL, NULL}
};

static const struct {
    const char *name;
    Color color;
} raylib_colors[] = {
    {"RAYWHITE", RAYWHITE},
    {"LIGHTGRAY", LIGHTGRAY},
    {"GRAY", GRAY},
    {"DARKGRAY", DARKGRAY},
    {"YELLOW", YELLOW},
    {"GOLD", GOLD},
    {"ORANGE", ORANGE},
    {"PINK", PINK},
    {"RED", RED},
    {"MAROON", MAROON},
    {"GREEN", GREEN},
    {"LIME", LIME},
    {"DARKGREEN", DARKGREEN},
    {"SKYBLUE", SKYBLUE},
    {"BLUE", BLUE},
    {"DARKBLUE", DARKBLUE},
    {"PURPLE", PURPLE},
    {"VIOLET", VIOLET},
    {"DARKPURPLE", DARKPURPLE},
    {"BEIGE", BEIGE},
    {"BROWN", BROWN},
    {"DARKBROWN", DARKBROWN},
    {"WHITE", WHITE},
    {"BLACK", BLACK},
    {"BLANK", BLANK},
    {"MAGENTA", MAGENTA},
    {NULL, {0, 0, 0, 0}}
};

/* Enum sabitleri: CAMERA_*, KEY_*, MOUSE_* */
static const struct {
    const char *name;
    int value;
} raylib_constants[] = {
    {"CAMERA_FREE", CAMERA_FREE},
    {"CAMERA_ORBITAL", CAMERA_ORBITAL},
    {"CAMERA_FIRST_PERSON", CAMERA_FIRST_PERSON},
    {"CAMERA_THIRD_PERSON", CAMERA_THIRD_PERSON},
    {"CAMERA_PERSPECTIVE", CAMERA_PERSPECTIVE},
    {"CAMERA_ORTHOGRAPHIC", CAMERA_ORTHOGRAPHIC},
    {"KEY_Z", KEY_Z},
    {"KEY_X", KEY_X},
    {"KEY_C", KEY_C},
    {"KEY_V", KEY_V},
    {"KEY_ESCAPE", KEY_ESCAPE},
    {"KEY_SPACE", KEY_SPACE},
    {"KEY_TAB", KEY_TAB},
    {"KEY_LEFT", KEY_LEFT},
    {"KEY_RIGHT", KEY_RIGHT},
    {"KEY_UP", KEY_UP},
    {"KEY_DOWN", KEY_DOWN},
    {"KEY_W", KEY_W},
    {"KEY_A", KEY_A},
    {"KEY_S", KEY_S},
    {"KEY_D", KEY_D},
    {"MOUSE_BUTTON_LEFT", MOUSE_BUTTON_LEFT},
    {"MOUSE_BUTTON_RIGHT", MOUSE_BUTTON_RIGHT},
    {"MOUSE_BUTTON_MIDDLE", MOUSE_BUTTON_MIDDLE},
    {NULL, 0}
};

/* Rectangle metatable */
static const luaL_Reg rect_meta[] = {
    {"__index", l_rect_get},
    {"__newindex", l_rect_set},
    {NULL, NULL}
};

GCL_LUA_EXPORT int luaopen_LuaRaylib(lua_State *L) {
    /* Rectangle metatable */
    luaL_newmetatable(L, RECT_MT);
    luaL_setfuncs(L, rect_meta, 0);
    lua_pop(L, 1);

    /* raylib global table */
    lua_newtable(L);
    for (int i = 0; raylib_funcs[i].name; i++) {
        lua_pushcfunction(L, raylib_funcs[i].func);
        lua_setfield(L, -2, raylib_funcs[i].name);
    }
    for (int i = 0; raylib_colors[i].name; i++) {
        push_color(L, raylib_colors[i].color);
        lua_setfield(L, -2, raylib_colors[i].name);
    }
    /* Enums: CAMERA_*, KEY_*, MOUSE_* */
    for (int i = 0; raylib_constants[i].name; i++) {
        lua_pushinteger(L, raylib_constants[i].value);
        lua_setfield(L, -2, raylib_constants[i].name);
    }
    lua_setglobal(L, "raylib");

    /* require("LuaRaylib") modülünü döndür */
    lua_getglobal(L, "raylib");
    return 1;
}
