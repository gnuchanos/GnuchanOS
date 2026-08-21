/*
 * raylib_syntax.c — raylib binding (rl.) tamamlama tablosu.
 *
 * lua_raylib.gcDL ve python_raylib.gcDL native .gcDL modullerdir; LSP
 * workspace taramasi bunlarin icini goremaz. "local rl = gcl.raylib" /
 * "import pyRaylib as rl" yazildiktan sonra "rl." tamamlamasi bu statik
 * tablodan gelir. Liste gen_reference.py'deki LUA_RAYLIB_FUNCS /
 * PY_RAYLIB_FUNCS + CONSTANTS ile birebir aynidir.
 */

#include "raylib_syntax.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *label;
  const char *kind;   /* "fn" | "const" */
  const char *detail;
} RlEntry;

/* ---- fonksiyonlar (Lua + Python ortak imza seti) ---- */
static const RlEntry RL_FUNCS[] = {
  {"InitWindow", "fn", "InitWindow(width, height, title)"},
  {"CloseWindow", "fn", "CloseWindow()"},
  {"WindowShouldClose", "fn", "WindowShouldClose() -> bool"},
  {"GetScreenWidth", "fn", "GetScreenWidth() -> int"},
  {"GetScreenHeight", "fn", "GetScreenHeight() -> int"},
  {"SetTargetFPS", "fn", "SetTargetFPS(fps)"},
  {"SetConfigFlags", "fn", "SetConfigFlags(flags)"},
  {"SetExitKey", "fn", "SetExitKey(key)"},
  {"ToggleFullscreen", "fn", "ToggleFullscreen()"},
  {"TakeScreenshot", "fn", "TakeScreenshot(filename)"},
  {"GetTime", "fn", "GetTime() -> number"},
  {"GetFrameTime", "fn", "GetFrameTime() -> number"},
  {"GetFPS", "fn", "GetFPS() -> int"},
  {"BeginDrawing", "fn", "BeginDrawing()"},
  {"EndDrawing", "fn", "EndDrawing()"},
  {"ClearBackground", "fn", "ClearBackground(color)"},
  {"DrawPixel", "fn", "DrawPixel(x, y, color)"},
  {"DrawLine", "fn", "DrawLine(x1, y1, x2, y2, color)"},
  {"DrawLineV", "fn", "DrawLineV(startPos, endPos, color)"},
  {"DrawLineEx", "fn", "DrawLineEx(startPos, endPos, thick, color)"},
  {"DrawCircle", "fn", "DrawCircle(centerX, centerY, radius, color)"},
  {"DrawCircleV", "fn", "DrawCircleV(center, radius, color)"},
  {"DrawCircleLines", "fn", "DrawCircleLines(centerX, centerY, radius, color)"},
  {"DrawCircleGradient", "fn", "DrawCircleGradient(center, radius, color1, color2)"},
  {"DrawCircleSector", "fn", "DrawCircleSector(center, radius, startAngle, endAngle, segments, color)"},
  {"DrawCircleSectorLines", "fn", "DrawCircleSectorLines(center, radius, startAngle, endAngle, segments, color)"},
  {"DrawRing", "fn", "DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)"},
  {"DrawRingLines", "fn", "DrawRingLines(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)"},
  {"DrawEllipse", "fn", "DrawEllipse(centerX, centerY, radiusX, radiusY, color)"},
  {"DrawRectangle", "fn", "DrawRectangle(x, y, width, height, color)"},
  {"DrawRectangleV", "fn", "DrawRectangleV(position, size, color)"},
  {"DrawRectangleRec", "fn", "DrawRectangleRec(rec, color)"},
  {"DrawRectangleLines", "fn", "DrawRectangleLines(x, y, width, height, color)"},
  {"DrawRectangleLinesEx", "fn", "DrawRectangleLinesEx(rec, lineThick, color)"},
  {"DrawRectangleRounded", "fn", "DrawRectangleRounded(rec, roundness, segments, color)"},
  {"DrawRectangleGradientV", "fn", "DrawRectangleGradientV(x, y, width, height, color1, color2)"},
  {"DrawRectangleGradientH", "fn", "DrawRectangleGradientH(x, y, width, height, color1, color2)"},
  {"DrawTriangle", "fn", "DrawTriangle(v1, v2, v3, color)"},
  {"DrawPoly", "fn", "DrawPoly(center, sides, radius, rotation, color)"},
  {"DrawPolyLines", "fn", "DrawPolyLines(center, sides, radius, rotation, color)"},
  {"DrawText", "fn", "DrawText(text, x, y, fontSize, color)"},
  {"DrawFPS", "fn", "DrawFPS(x, y)"},
  {"MeasureText", "fn", "MeasureText(text, fontSize) -> int"},
  {"IsKeyDown", "fn", "IsKeyDown(key) -> bool"},
  {"IsKeyPressed", "fn", "IsKeyPressed(key) -> bool"},
  {"IsKeyReleased", "fn", "IsKeyReleased(key) -> bool"},
  {"IsKeyUp", "fn", "IsKeyUp(key) -> bool"},
  {"GetKeyPressed", "fn", "GetKeyPressed() -> int"},
  {"GetCharPressed", "fn", "GetCharPressed() -> int"},
  {"IsMouseButtonDown", "fn", "IsMouseButtonDown(button) -> bool"},
  {"IsMouseButtonPressed", "fn", "IsMouseButtonPressed(button) -> bool"},
  {"GetMouseX", "fn", "GetMouseX() -> int"},
  {"GetMouseY", "fn", "GetMouseY() -> int"},
  {"GetMousePosition", "fn", "GetMousePosition() -> Vector2/tuple"},
  {"GetMouseDelta", "fn", "GetMouseDelta() -> Vector2/tuple"},
  {"GetMouseWheelMove", "fn", "GetMouseWheelMove() -> number"},
  {"SetMousePosition", "fn", "SetMousePosition(x, y)"},
  {"SetMouseScale", "fn", "SetMouseScale(scaleX, scaleY)"},
  {"LoadTexture", "fn", "LoadTexture(filename) -> Texture2D/dict"},
  {"UnloadTexture", "fn", "UnloadTexture(texture)"},
  {"DrawTexture", "fn", "DrawTexture(texture, x, y, color)"},
  {"DrawTextureEx", "fn", "DrawTextureEx(texture, position, rotation, scale, color)"},
  {"LoadRenderTexture", "fn", "LoadRenderTexture(width, height)"},
  {"BeginTextureMode", "fn", "BeginTextureMode(target)"},
  {"EndTextureMode", "fn", "EndTextureMode()"},
  {"LoadFont", "fn", "LoadFont(filename)"},
  {"DrawTextEx", "fn", "DrawTextEx(font, text, position, fontSize, spacing, color)"},
  {"BeginMode2D", "fn", "BeginMode2D(camera)"},
  {"EndMode2D", "fn", "EndMode2D()"},
  {"CheckCollisionRecs", "fn", "CheckCollisionRecs(rec1, rec2) -> bool"},
  {"CheckCollisionCircles", "fn", "CheckCollisionCircles(c1, r1, c2, r2) -> bool"},
  {"CheckCollisionPointRec", "fn", "CheckCollisionPointRec(point, rec) -> bool"},
  {"CheckCollisionPointCircle", "fn", "CheckCollisionPointCircle(point, center, radius) -> bool"},
  {"CheckCollisionPointLine", "fn", "CheckCollisionPointLine(point, p1, p2, threshold) -> bool"},
  {"GetCollisionRec", "fn", "GetCollisionRec(rec1, rec2) -> Rectangle/tuple"},
  {"InitAudioDevice", "fn", "InitAudioDevice()"},
  {"CloseAudioDevice", "fn", "CloseAudioDevice()"},
  {"AudioDeviceReady", "fn", "AudioDeviceReady() -> bool"},
  {"SetMasterVolume", "fn", "SetMasterVolume(volume)"},
  {"LoadSound", "fn", "LoadSound(filename)"},
  {"PlaySound", "fn", "PlaySound(sound)"},
  {"StopSound", "fn", "StopSound(sound)"},
  {"SetSoundVolume", "fn", "SetSoundVolume(sound, volume)"},
  {"LoadMusicStream", "fn", "LoadMusicStream(filename)"},
  {"PlayMusicStream", "fn", "PlayMusicStream(music)"},
  {"UpdateMusicStream", "fn", "UpdateMusicStream(music)"},
  {"StopMusicStream", "fn", "StopMusicStream(music)"},
  {"SetMusicVolume", "fn", "SetMusicVolume(music, volume)"},
  /* value constructors */
  {"Vector2", "fn", "Vector2(x, y)"},
  {"Vector3", "fn", "Vector3(x, y, z)"},
  {"Rectangle", "fn", "Rectangle(x, y, width, height)"},
  {"Color", "fn", "Color(r, g, b, a)"},
};

/* ---- sabitler: renkler + tuslar + fare + pencere bayraklari ---- */
static const RlEntry RL_CONSTS[] = {
  {"RAYWHITE", "const", "(245,245,245,255)"},
  {"WHITE", "const", "(255,255,255,255)"},
  {"BLACK", "const", "(0,0,0,255)"},
  {"GRAY", "const", "(130,130,130,255)"},
  {"DARKGRAY", "const", "(80,80,80,255)"},
  {"LIGHTGRAY", "const", "(200,200,200,255)"},
  {"RED", "const", "(230,41,55,255)"},
  {"GREEN", "const", "(0,228,48,255)"},
  {"BLUE", "const", "(0,121,241,255)"},
  {"SKYBLUE", "const", "(102,191,255,255)"},
  {"YELLOW", "const", "(253,249,0,255)"},
  {"GOLD", "const", "(255,203,0,255)"},
  {"ORANGE", "const", "(255,161,0,255)"},
  {"PURPLE", "const", "(200,122,255,255)"},
  {"PINK", "const", "(255,109,194,255)"},
  {"LIME", "const", "(0,158,47,255)"},
  {"BROWN", "const", "(127,106,79,255)"},
  {"BEIGE", "const", "(211,176,131,255)"},
  {"MAGENTA", "const", "(255,0,255,255)"},
  {"MAROON", "const", "(190,33,55,255)"},
  {"KEY_A", "const", "65"},
  {"KEY_B", "const", "66"},
  {"KEY_W", "const", "87"},
  {"KEY_SPACE", "const", "32"},
  {"KEY_ENTER", "const", "257"},
  {"KEY_ESCAPE", "const", "256"},
  {"KEY_UP", "const", "265"},
  {"KEY_DOWN", "const", "264"},
  {"KEY_LEFT", "const", "263"},
  {"KEY_RIGHT", "const", "262"},
  {"KEY_F1", "const", "290"},
  {"MOUSE_BUTTON_LEFT", "const", "0"},
  {"MOUSE_BUTTON_RIGHT", "const", "1"},
  {"MOUSE_BUTTON_MIDDLE", "const", "2"},
  {"FLAG_VSYNC_HINT", "const", "0x40"},
  {"FLAG_FULLSCREEN_MODE", "const", "0x02"},
  {"FLAG_WINDOW_RESIZABLE", "const", "0x04"},
  {"FLAG_WINDOW_UNDECORATED", "const", "0x08"},
  {"FLAG_WINDOW_HIDDEN", "const", "0x80"},
  {"FLAG_WINDOW_MINIMIZED", "const", "0x200"},
  {"FLAG_WINDOW_MAXIMIZED", "const", "0x400"},
  {"FLAG_WINDOW_HIGHDPI", "const", "0x2000"},
  {"FLAG_MSAA_4X_HINT", "const", "0x20"},
  {"FLAG_WINDOW_MOUSE_PASSTHROUGH", "const", "0x4000"},
  {"FLAG_BORDERLESS_WINDOWED_MODE", "const", "0x8000"},
  {"FLAG_WINDOW_TOPMOST", "const", "0x1000"},
  {"FLAG_INTERLACED_HINT", "const", "0x10000"},
  {"FLAG_WINDOW_UNFOCUSED", "const", "0x800"},
  {"CAMERA_ORTHOGRAPHIC", "const", "1"},
  {"CAMERA_PERSPECTIVE", "const", "0"},
};

#define RL_FUNCS_N (int)(sizeof RL_FUNCS / sizeof RL_FUNCS[0])
#define RL_CONSTS_N (int)(sizeof RL_CONSTS / sizeof RL_CONSTS[0])

static void rl_fill(int idx, const char *prefix, char *label, size_t label_cap,
                    char *kind, size_t kind_cap, char *detail, size_t detail_cap) {
  const RlEntry *e;
  if (idx < RL_FUNCS_N) e = &RL_FUNCS[idx];
  else e = &RL_CONSTS[idx - RL_FUNCS_N];
  if (label && label_cap) snprintf(label, label_cap, "%s", e->label);
  if (kind && kind_cap) snprintf(kind, kind_cap, "%s", e->kind);
  if (detail && detail_cap) snprintf(detail, detail_cap, "%s", e->detail);
}

int raylib_syntax_count(const char *prefix) {
  int n = 0;
  size_t plen = prefix ? strlen(prefix) : 0;
  for (int i = 0; i < RL_FUNCS_N + RL_CONSTS_N; i++) {
    const RlEntry *e = i < RL_FUNCS_N ? &RL_FUNCS[i] : &RL_CONSTS[i - RL_FUNCS_N];
    if (!plen || strncmp(e->label, prefix, plen) == 0) n++;
  }
  return n;
}

void raylib_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                      char *kind, size_t kind_cap, char *detail, size_t detail_cap) {
  size_t plen = prefix ? strlen(prefix) : 0;
  int seen = 0;
  for (int i = 0; i < RL_FUNCS_N + RL_CONSTS_N; i++) {
    const RlEntry *e = i < RL_FUNCS_N ? &RL_FUNCS[i] : &RL_CONSTS[i - RL_FUNCS_N];
    if (plen && strncmp(e->label, prefix, plen) != 0) continue;
    if (seen == idx) { rl_fill(i, prefix, label, label_cap, kind, kind_cap, detail, detail_cap); return; }
    seen++;
  }
}
