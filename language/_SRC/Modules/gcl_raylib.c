/*
 * gcl_raylib.c — GCL Raylib modülü (.dll/.so) — TAM wrapper.
 *
 * #native <Raylib> ile yüklenir; Raylib.* çağrıları raylib fonksiyonlarına bağlanır.
 *
 * GCL modül sistemi: fn(int argc, const char **argv) -> double.
 *   - Sayısal parametreler atof/strtol ile çevrilir.
 *   - Renkler 32-bit packed uint olarak taşınır (R | G<<8 | B<<16 | A<<24).
 *   - Struct dönen fonksiyonlar g_* global'lerine yazar; dönüş handle (int) olarak verilir.
 *   - String dönen fonksiyonlar g_str global'ine yazar; dönüş 0.
 *   - Struct parametreleri genişletilmiş argümanlarla verilir:
 *        Rectangle -> 4 arg (x, y, w, h)
 *        Vector2   -> 2 arg (x, y)
 *        Vector3   -> 3 arg (x, y, z)
 *        Color     -> 1 arg (packed uint)
 */

#include "gcl_module.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* ---------- Handle registry ---------- */
#define MAX_HANDLES 512

static Texture2D      g_tex_h[MAX_HANDLES];    static int g_tex_n = 0;
static Image          g_img_h[MAX_HANDLES];    static int g_img_n = 0;
static Font           g_font_h[MAX_HANDLES];   static int g_font_n = 0;
static RenderTexture2D g_rt_h[MAX_HANDLES];    static int g_rt_n = 0;
static Sound          g_snd_h[MAX_HANDLES];    static int g_snd_n = 0;
static Music          g_mus_h[MAX_HANDLES];    static int g_mus_n = 0;
static Shader         g_shader_h[MAX_HANDLES]; static int g_shader_n = 0;
static Mesh           g_mesh_h[MAX_HANDLES];   static int g_mesh_n = 0;
static Model          g_model_h[MAX_HANDLES];  static int g_model_n = 0;
static Material       g_mat_h[MAX_HANDLES];    static int g_mat_n = 0;
static Wave           g_wave_h[MAX_HANDLES];   static int g_wave_n = 0;
static AudioStream    g_ast_h[MAX_HANDLES];    static int g_ast_n = 0;
static FilePathList   g_fpl_h[MAX_HANDLES];    static int g_fpl_n = 0;
static AutomationEventList g_aevl_h[MAX_HANDLES]; static int g_aevl_n = 0;

static int g_last_handle = -1;
static char g_str[8192] = {0};

static int reg_tex(Texture2D v){ if(g_tex_n>=MAX_HANDLES) return -1; g_tex_h[g_tex_n]=v; return g_tex_n++; }
static int reg_img(Image v){ if(g_img_n>=MAX_HANDLES) return -1; g_img_h[g_img_n]=v; return g_img_n++; }
static int reg_font(Font v){ if(g_font_n>=MAX_HANDLES) return -1; g_font_h[g_font_n]=v; return g_font_n++; }
static int reg_rt(RenderTexture2D v){ if(g_rt_n>=MAX_HANDLES) return -1; g_rt_h[g_rt_n]=v; return g_rt_n++; }
static int reg_snd(Sound v){ if(g_snd_n>=MAX_HANDLES) return -1; g_snd_h[g_snd_n]=v; return g_snd_n++; }
static int reg_mus(Music v){ if(g_mus_n>=MAX_HANDLES) return -1; g_mus_h[g_mus_n]=v; return g_mus_n++; }
static int reg_shader(Shader v){ if(g_shader_n>=MAX_HANDLES) return -1; g_shader_h[g_shader_n]=v; return g_shader_n++; }
static int reg_mesh(Mesh v){ if(g_mesh_n>=MAX_HANDLES) return -1; g_mesh_h[g_mesh_n]=v; return g_mesh_n++; }
static int reg_model(Model v){ if(g_model_n>=MAX_HANDLES) return -1; g_model_h[g_model_n]=v; return g_model_n++; }
static int reg_mat(Material v){ if(g_mat_n>=MAX_HANDLES) return -1; g_mat_h[g_mat_n]=v; return g_mat_n++; }
static int reg_wave(Wave v){ if(g_wave_n>=MAX_HANDLES) return -1; g_wave_h[g_wave_n]=v; return g_wave_n++; }
static int reg_ast(AudioStream v){ if(g_ast_n>=MAX_HANDLES) return -1; g_ast_h[g_ast_n]=v; return g_ast_n++; }
static int reg_fpl(FilePathList v){ if(g_fpl_n>=MAX_HANDLES) return -1; g_fpl_h[g_fpl_n]=v; return g_fpl_n++; }
static int reg_aevl(AutomationEventList v){ if(g_aevl_n>=MAX_HANDLES) return -1; g_aevl_h[g_aevl_n]=v; return g_aevl_n++; }

static Texture2D      get_tex(int h){ return (h>=0&&h<g_tex_n)? g_tex_h[h] : (Texture2D){0}; }
static Image          get_img(int h){ return (h>=0&&h<g_img_n)? g_img_h[h] : (Image){0}; }
static Font           get_font(int h){ return (h>=0&&h<g_font_n)? g_font_h[h] : (Font){0}; }
static RenderTexture2D get_rt(int h){ return (h>=0&&h<g_rt_n)? g_rt_h[h] : (RenderTexture2D){0}; }
static Sound          get_snd(int h){ return (h>=0&&h<g_snd_n)? g_snd_h[h] : (Sound){0}; }
static Music          get_mus(int h){ return (h>=0&&h<g_mus_n)? g_mus_h[h] : (Music){0}; }
static Shader         get_shader(int h){ return (h>=0&&h<g_shader_n)? g_shader_h[h] : (Shader){0}; }
static Mesh           get_mesh(int h){ return (h>=0&&h<g_mesh_n)? g_mesh_h[h] : (Mesh){0}; }
static Model          get_model(int h){ return (h>=0&&h<g_model_n)? g_model_h[h] : (Model){0}; }
static Material       get_mat(int h){ return (h>=0&&h<g_mat_n)? g_mat_h[h] : (Material){0}; }
static Wave           get_wave(int h){ return (h>=0&&h<g_wave_n)? g_wave_h[h] : (Wave){0}; }
static AudioStream    get_ast(int h){ return (h>=0&&h<g_ast_n)? g_ast_h[h] : (AudioStream){0}; }
static FilePathList   get_fpl(int h){ return (h>=0&&h<g_fpl_n)? g_fpl_h[h] : (FilePathList){0}; }
static AutomationEventList get_aevl(int h){ return (h>=0&&h<g_aevl_n)? g_aevl_h[h] : (AutomationEventList){0}; }

/* ---------- Yardımcılar ---------- */
static unsigned int color_to_uint(Color c){
    return ((unsigned int)c.r)|(((unsigned int)c.g)<<8)|(((unsigned int)c.b)<<16)|(((unsigned int)c.a)<<24);
}
static Color uint_to_color(unsigned int v){
    Color c; c.r=(unsigned char)(v&0xFF); c.g=(unsigned char)((v>>8)&0xFF); c.b=(unsigned char)((v>>16)&0xFF); c.a=(unsigned char)((v>>24)&0xFF); return c;
}
static Color ci(const char **a,int i){ return (a&&a[i])? uint_to_color((unsigned int)strtoul(a[i],NULL,10)) : (Color){0,0,0,255}; }
static int ii(const char *s){ return s? (int)atof(s) : 0; }
static float ff(const char *s){ return s? (float)atof(s) : 0.0f; }
static const char *ss(const char *s){ return s? s : ""; }
static Rectangle rect_arg(const char **a,int i){
    return (Rectangle){ ff(a[i]), ff(a[i+1]), ff(a[i+2]), ff(a[i+3]) };
}
static Vector2 v2_arg(const char **a,int i){ return (Vector2){ ff(a[i]), ff(a[i+1]) }; }
static Vector3 v3_arg(const char **a,int i){ return (Vector3){ ff(a[i]), ff(a[i+1]), ff(a[i+2]) }; }

/* ---------- Son üretilen değerler ---------- */
static Vector2        g_last_v2={0};
static Vector3        g_last_v3={0};
static Vector4        g_last_v4={0};
static Matrix         g_last_mat={0};
static Rectangle      g_last_rect={0};
static Camera         g_last_cam={0};
static Camera2D       g_last_cam2d={0};
static Ray            g_last_ray={0};
static RayCollision   g_last_raycol={0};
static BoundingBox    g_last_bb={0};
static NPatchInfo     g_last_npatch={0};
static GlyphInfo      g_last_glyph={0};
static VrStereoConfig g_last_vr={0};
static AutomationEvent g_last_aevent={0};
GCL_EXPORT Rectangle *gcl_raylib_last_rectangle(void){ return &g_last_rect; }

/* ---------- Color helper fonksiyonları ---------- */
static double fn_Fade(int argc,const char**argv){
    Color c=ci(argv,0); float a=(argc>1)?ff(argv[1]):1.0f; return (double)color_to_uint(Fade(c,a));
}
static double fn_ColorToInt(int argc,const char**argv){ return (double)ColorToInt(ci(argv,0)); }
static double fn_ColorNormalize(int argc,const char**argv){ g_last_v4=ColorNormalize(ci(argv,0)); return 0.0; }
static double fn_ColorFromNormalized(int argc,const char**argv){
    Vector4 v={ff(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3])}; return (double)color_to_uint(ColorFromNormalized(v));
}
static double fn_ColorToHSV(int argc,const char**argv){ g_last_v3=ColorToHSV(ci(argv,0)); return 0.0; }
static double fn_ColorFromHSV(int argc,const char**argv){ return (double)color_to_uint(ColorFromHSV(ff(argv[0]),ff(argv[1]),ff(argv[2]))); }
static double fn_ColorTint(int argc,const char**argv){ return (double)color_to_uint(ColorTint(ci(argv,0),ci(argv,1))); }
static double fn_ColorBrightness(int argc,const char**argv){ return (double)color_to_uint(ColorBrightness(ci(argv,0),ff(argv[1]))); }
static double fn_ColorContrast(int argc,const char**argv){ return (double)color_to_uint(ColorContrast(ci(argv,0),ff(argv[1]))); }
static double fn_ColorAlpha(int argc,const char**argv){ return (double)color_to_uint(ColorAlpha(ci(argv,0),ff(argv[1]))); }
static double fn_ColorAlphaBlend(int argc,const char**argv){ return (double)color_to_uint(ColorAlphaBlend(ci(argv,0),ci(argv,1),ci(argv,2))); }
static double fn_ColorLerp(int argc,const char**argv){ return (double)color_to_uint(ColorLerp(ci(argv,0),ci(argv,1),ff(argv[2]))); }
static double fn_GetColor(int argc,const char**argv){ return (double)color_to_uint(GetColor((unsigned int)strtoul(ss(argv[0]),NULL,10))); }
static double fn_GetPixelColor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetPixelColor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetPixelDataSize(int argc,const char**argv){ return (double)GetPixelDataSize(ii(argv[0]),ii(argv[1]),ii(argv[2])); }
static double fn_ColorIsEqual(int argc,const char**argv){ return ColorIsEqual(ci(argv,0),ci(argv,1))?1.0:0.0; }

/* ---------- Struct constructor'ları (g_* global'lerine yazar, return 0) ---------- */
static double fn_Rectangle(int argc,const char**argv){(void)argc;(void)argv;g_last_rect=(Rectangle){ff(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3])};return 0.0;}
static double fn_Vector2(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=(Vector2){ff(argv[0]),ff(argv[1])};return 0.0;}
static double fn_Vector3(int argc,const char**argv){(void)argc;(void)argv;g_last_v3=(Vector3){ff(argv[0]),ff(argv[1]),ff(argv[2])};return 0.0;}
static double fn_Vector4(int argc,const char**argv){(void)argc;(void)argv;g_last_v4=(Vector4){ff(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3])};return 0.0;}
static double fn_Matrix(int argc,const char**argv){(void)argc;(void)argv;g_last_mat=(Matrix){ff(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3]),ff(argv[4]),ff(argv[5]),ff(argv[6]),ff(argv[7]),ff(argv[8]),ff(argv[9]),ff(argv[10]),ff(argv[11]),ff(argv[12]),ff(argv[13]),ff(argv[14]),ff(argv[15])};return 0.0;}
static double fn_Camera(int argc,const char**argv){(void)argc;(void)argv;g_last_cam=(Camera3D){v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ff(argv[9]),ii(argv[10])};return 0.0;}
static double fn_Camera2D(int argc,const char**argv){(void)argc;(void)argv;g_last_cam2d=(Camera2D){v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ff(argv[5])};return 0.0;}
static double fn_Ray(int argc,const char**argv){(void)argc;(void)argv;g_last_ray=(Ray){v3_arg(argv,0),v3_arg(argv,3)};return 0.0;}
static double fn_BoundingBox(int argc,const char**argv){(void)argc;(void)argv;g_last_bb=(BoundingBox){v3_arg(argv,0),v3_arg(argv,3)};return 0.0;}
static double fn_NPatchInfo(int argc,const char**argv){(void)argc;(void)argv;g_last_npatch=(NPatchInfo){rect_arg(argv,0),ii(argv[4]),ii(argv[5]),ii(argv[6]),ii(argv[7]),ii(argv[8])};return 0.0;}
static double fn_GlyphInfo(int argc,const char**argv){(void)argc;(void)argv;g_last_glyph=(GlyphInfo){ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),get_img(ii(argv[4]))};return 0.0;}
static double fn_VrStereoConfig(int argc,const char**argv){(void)argc;(void)argv;memset(&g_last_vr,0,sizeof(g_last_vr));return 0.0;}
static double fn_AutomationEvent(int argc,const char**argv){(void)argc;(void)argv;g_last_aevent=(AutomationEvent){(unsigned int)strtoul(argv[0],NULL,10),(unsigned int)strtoul(argv[1],NULL,10),ii(argv[2]),ii(argv[3]),ii(argv[4]),ii(argv[5])};return 0.0;}

/* =========================================================================
   COLOR CONSTANTS
   ========================================================================= */
#define COLOR_FN(cn,cv) static double fn_##cn(int argc,const char**argv){(void)argc;(void)argv;return (double)color_to_uint((Color)cv);}
COLOR_FN(RAYWHITE,RAYWHITE)
COLOR_FN(LIGHTGRAY,LIGHTGRAY)
COLOR_FN(GRAY,GRAY)
COLOR_FN(DARKGRAY,DARKGRAY)
COLOR_FN(YELLOW,YELLOW)
COLOR_FN(GOLD,GOLD)
COLOR_FN(ORANGE,ORANGE)
COLOR_FN(PINK,PINK)
COLOR_FN(RED,RED)
COLOR_FN(MAROON,MAROON)
COLOR_FN(GREEN,GREEN)
COLOR_FN(LIME,LIME)
COLOR_FN(DARKGREEN,DARKGREEN)
COLOR_FN(SKYBLUE,SKYBLUE)
COLOR_FN(BLUE,BLUE)
COLOR_FN(DARKBLUE,DARKBLUE)
COLOR_FN(PURPLE,PURPLE)
COLOR_FN(VIOLET,VIOLET)
COLOR_FN(DARKPURPLE,DARKPURPLE)
COLOR_FN(BEIGE,BEIGE)
COLOR_FN(BROWN,BROWN)
COLOR_FN(DARKBROWN,DARKBROWN)
COLOR_FN(WHITE,WHITE)
COLOR_FN(BLACK,BLACK)
COLOR_FN(BLANK,BLANK)
COLOR_FN(MAGENTA,MAGENTA)

/* =========================================================================
   CORE — WINDOW
   ========================================================================= */
static double fn_InitWindow(int argc,const char**argv){
    InitWindow(ii(argv[0]),ii(argv[1]),ss(argv[2])); return 0.0;
}
static double fn_CloseWindow(int argc,const char**argv){(void)argc;(void)argv;CloseWindow();return 0.0;}
static double fn_WindowShouldClose(int argc,const char**argv){(void)argc;(void)argv;return WindowShouldClose()?1.0:0.0;}
static double fn_IsWindowReady(int argc,const char**argv){(void)argc;(void)argv;return IsWindowReady()?1.0:0.0;}
static double fn_IsWindowFullscreen(int argc,const char**argv){(void)argc;(void)argv;return IsWindowFullscreen()?1.0:0.0;}
static double fn_IsWindowHidden(int argc,const char**argv){(void)argc;(void)argv;return IsWindowHidden()?1.0:0.0;}
static double fn_IsWindowMinimized(int argc,const char**argv){(void)argc;(void)argv;return IsWindowMinimized()?1.0:0.0;}
static double fn_IsWindowMaximized(int argc,const char**argv){(void)argc;(void)argv;return IsWindowMaximized()?1.0:0.0;}
static double fn_IsWindowFocused(int argc,const char**argv){(void)argc;(void)argv;return IsWindowFocused()?1.0:0.0;}
static double fn_IsWindowResized(int argc,const char**argv){(void)argc;(void)argv;return IsWindowResized()?1.0:0.0;}
static double fn_IsWindowState(int argc,const char**argv){
    unsigned int f=(argc>0)?(unsigned int)strtoul(argv[0],NULL,10):0; return IsWindowState(f)?1.0:0.0;
}
static double fn_SetWindowState(int argc,const char**argv){
    unsigned int f=(argc>0)?(unsigned int)strtoul(argv[0],NULL,10):0; SetWindowState(f); return 0.0;
}
static double fn_ClearWindowState(int argc,const char**argv){
    unsigned int f=(argc>0)?(unsigned int)strtoul(argv[0],NULL,10):0; ClearWindowState(f); return 0.0;
}
static double fn_ToggleFullscreen(int argc,const char**argv){(void)argc;(void)argv;ToggleFullscreen();return 0.0;}
static double fn_ToggleBorderlessWindowed(int argc,const char**argv){(void)argc;(void)argv;ToggleBorderlessWindowed();return 0.0;}
static double fn_MaximizeWindow(int argc,const char**argv){(void)argc;(void)argv;MaximizeWindow();return 0.0;}
static double fn_MinimizeWindow(int argc,const char**argv){(void)argc;(void)argv;MinimizeWindow();return 0.0;}
static double fn_RestoreWindow(int argc,const char**argv){(void)argc;(void)argv;RestoreWindow();return 0.0;}
static double fn_SetWindowIcon(int argc,const char**argv){
    int h=(argc>0)?(int)atof(argv[0]):-1; SetWindowIcon(get_img(h)); return 0.0;
}
static double fn_SetWindowIcons(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetWindowTitle(int argc,const char**argv){ SetWindowTitle(ss(argv[0])); return 0.0; }
static double fn_SetWindowPosition(int argc,const char**argv){ SetWindowPosition(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowMonitor(int argc,const char**argv){ SetWindowMonitor(ii(argv[0])); return 0.0; }
static double fn_SetWindowMinSize(int argc,const char**argv){ SetWindowMinSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowMaxSize(int argc,const char**argv){ SetWindowMaxSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowSize(int argc,const char**argv){ SetWindowSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowOpacity(int argc,const char**argv){ SetWindowOpacity(ff(argv[0])); return 0.0; }
static double fn_SetWindowFocused(int argc,const char**argv){(void)argc;(void)argv;SetWindowFocused();return 0.0;}
static double fn_GetWindowHandle(int argc,const char**argv){(void)argc;(void)argv;return (double)(intptr_t)GetWindowHandle();}
static double fn_GetScreenWidth(int argc,const char**argv){(void)argc;(void)argv;return (double)GetScreenWidth();}
static double fn_GetScreenHeight(int argc,const char**argv){(void)argc;(void)argv;return (double)GetScreenHeight();}
static double fn_GetRenderWidth(int argc,const char**argv){(void)argc;(void)argv;return (double)GetRenderWidth();}
static double fn_GetRenderHeight(int argc,const char**argv){(void)argc;(void)argv;return (double)GetRenderHeight();}
static double fn_GetMonitorCount(int argc,const char**argv){(void)argc;(void)argv;return (double)GetMonitorCount();}
static double fn_GetCurrentMonitor(int argc,const char**argv){(void)argc;(void)argv;return (double)GetCurrentMonitor();}
static double fn_GetMonitorPosition(int argc,const char**argv){ g_last_v2=GetMonitorPosition(ii(argv[0])); return 0.0; }
static double fn_GetMonitorWidth(int argc,const char**argv){ return (double)GetMonitorWidth(ii(argv[0])); }
static double fn_GetMonitorHeight(int argc,const char**argv){ return (double)GetMonitorHeight(ii(argv[0])); }
static double fn_GetMonitorPhysicalWidth(int argc,const char**argv){ return (double)GetMonitorPhysicalWidth(ii(argv[0])); }
static double fn_GetMonitorPhysicalHeight(int argc,const char**argv){ return (double)GetMonitorPhysicalHeight(ii(argv[0])); }
static double fn_GetMonitorRefreshRate(int argc,const char**argv){ return (double)GetMonitorRefreshRate(ii(argv[0])); }
static double fn_GetWindowPosition(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetWindowPosition();return 0.0;}
static double fn_GetWindowScaleDPI(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetWindowScaleDPI();return 0.0;}
static double fn_GetMonitorName(int argc,const char**argv){
    const char *s=GetMonitorName(ii(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0;
}
static double fn_SetClipboardText(int argc,const char**argv){ SetClipboardText(ss(argv[0])); return 0.0; }
static double fn_GetClipboardText(int argc,const char**argv){
    const char *s=GetClipboardText(); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0;
}
static double fn_GetClipboardImage(int argc,const char**argv){ g_last_handle=reg_img(GetClipboardImage()); return (double)g_last_handle; }
static double fn_EnableEventWaiting(int argc,const char**argv){(void)argc;(void)argv;EnableEventWaiting();return 0.0;}
static double fn_DisableEventWaiting(int argc,const char**argv){(void)argc;(void)argv;DisableEventWaiting();return 0.0;}

/* cursor */
static double fn_ShowCursor(int argc,const char**argv){(void)argc;(void)argv;ShowCursor();return 0.0;}
static double fn_HideCursor(int argc,const char**argv){(void)argc;(void)argv;HideCursor();return 0.0;}
static double fn_IsCursorHidden(int argc,const char**argv){(void)argc;(void)argv;return IsCursorHidden()?1.0:0.0;}
static double fn_EnableCursor(int argc,const char**argv){(void)argc;(void)argv;EnableCursor();return 0.0;}
static double fn_DisableCursor(int argc,const char**argv){(void)argc;(void)argv;DisableCursor();return 0.0;}
static double fn_IsCursorOnScreen(int argc,const char**argv){(void)argc;(void)argv;return IsCursorOnScreen()?1.0:0.0;}

/* drawing */
static double fn_ClearBackground(int argc,const char**argv){ ClearBackground(ci(argv,0)); return 0.0; }
static double fn_BeginDrawing(int argc,const char**argv){(void)argc;(void)argv;BeginDrawing();return 0.0;}
static double fn_EndDrawing(int argc,const char**argv){(void)argc;(void)argv;EndDrawing();return 0.0;}
static double fn_BeginMode2D(int argc,const char**argv){
    if (argc >= 6) g_last_cam2d=(Camera2D){v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ff(argv[5])};
    /* Argümansız çağrı: Raylib.Camera2D(...) ile oluşturulan son kamerayı kullan */
    BeginMode2D(g_last_cam2d); return 0.0;
}
static double fn_EndMode2D(int argc,const char**argv){(void)argc;(void)argv;EndMode2D();return 0.0;}
static double fn_BeginMode3D(int argc,const char**argv){
    if (argc >= 11) g_last_cam=(Camera3D){v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ff(argv[9]),ii(argv[10])};
    /* Argümansız çağrı: Raylib.Camera(...) ile oluşturulan son kamerayı kullan */
    BeginMode3D(g_last_cam); return 0.0;
}
static double fn_EndMode3D(int argc,const char**argv){(void)argc;(void)argv;EndMode3D();return 0.0;}
static double fn_BeginTextureMode(int argc,const char**argv){ BeginTextureMode(get_rt(ii(argv[0]))); return 0.0; }
static double fn_EndTextureMode(int argc,const char**argv){(void)argc;(void)argv;EndTextureMode();return 0.0;}
static double fn_BeginShaderMode(int argc,const char**argv){ BeginShaderMode(get_shader(ii(argv[0]))); return 0.0; }
static double fn_EndShaderMode(int argc,const char**argv){(void)argc;(void)argv;EndShaderMode();return 0.0;}
static double fn_BeginBlendMode(int argc,const char**argv){ BeginBlendMode(ii(argv[0])); return 0.0; }
static double fn_EndBlendMode(int argc,const char**argv){(void)argc;(void)argv;EndBlendMode();return 0.0;}
static double fn_BeginScissorMode(int argc,const char**argv){ BeginScissorMode(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3])); return 0.0; }
static double fn_EndScissorMode(int argc,const char**argv){(void)argc;(void)argv;EndScissorMode();return 0.0;}
static double fn_BeginVrStereoMode(int argc,const char**argv){(void)argc;(void)argv;BeginVrStereoMode(g_last_vr);return 0.0;}
static double fn_EndVrStereoMode(int argc,const char**argv){(void)argc;(void)argv;EndVrStereoMode();return 0.0;}

/* VR */
static double fn_LoadVrStereoConfig(int argc,const char**argv){
    VrDeviceInfo d; memset(&d,0,sizeof(d));
    d.hResolution=ii(argv[0]); d.vResolution=ii(argv[1]); d.hScreenSize=ff(argv[2]); d.vScreenSize=ff(argv[3]);
    d.eyeToScreenDistance=ff(argv[4]); d.lensSeparationDistance=ff(argv[5]); d.interpupillaryDistance=ff(argv[6]);
    g_last_vr=LoadVrStereoConfig(d); return 0.0;
}
static double fn_UnloadVrStereoConfig(int argc,const char**argv){(void)argc;(void)argv;UnloadVrStereoConfig(g_last_vr);return 0.0;}

/* shaders */
static double fn_LoadShader(int argc,const char**argv){ g_last_handle=reg_shader(LoadShader(ss(argv[0]),ss(argv[1]))); return (double)g_last_handle; }
static double fn_LoadShaderFromMemory(int argc,const char**argv){ g_last_handle=reg_shader(LoadShaderFromMemory(ss(argv[0]),ss(argv[1]))); return (double)g_last_handle; }
static double fn_IsShaderValid(int argc,const char**argv){ return IsShaderValid(get_shader(ii(argv[0])))?1.0:0.0; }
static double fn_GetShaderLocation(int argc,const char**argv){ return (double)GetShaderLocation(get_shader(ii(argv[0])),ss(argv[1])); }
static double fn_GetShaderLocationAttrib(int argc,const char**argv){ return (double)GetShaderLocationAttrib(get_shader(ii(argv[0])),ss(argv[1])); }
static double fn_SetShaderValue(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetShaderValueV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetShaderValueMatrix(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetShaderValueTexture(int argc,const char**argv){ SetShaderValueTexture(get_shader(ii(argv[0])),ii(argv[1]),get_tex(ii(argv[2]))); return 0.0; }
static double fn_UnloadShader(int argc,const char**argv){ UnloadShader(get_shader(ii(argv[0]))); return 0.0; }

/* screen space */
static double fn_GetScreenToWorldRay(int argc,const char**argv){ g_last_ray=GetScreenToWorldRay(v2_arg(argv,0),g_last_cam); return 0.0; }
static double fn_GetScreenToWorldRayEx(int argc,const char**argv){ g_last_ray=GetScreenToWorldRayEx(v2_arg(argv,0),g_last_cam,ii(argv[2]),ii(argv[3])); return 0.0; }
static double fn_GetWorldToScreen(int argc,const char**argv){ g_last_v2=GetWorldToScreen(v3_arg(argv,0),g_last_cam); return 0.0; }
static double fn_GetWorldToScreenEx(int argc,const char**argv){ g_last_v2=GetWorldToScreenEx(v3_arg(argv,0),g_last_cam,ii(argv[3]),ii(argv[4])); return 0.0; }
static double fn_GetWorldToScreen2D(int argc,const char**argv){ g_last_v2=GetWorldToScreen2D(v2_arg(argv,0),g_last_cam2d); return 0.0; }
static double fn_GetScreenToWorld2D(int argc,const char**argv){ g_last_v2=GetScreenToWorld2D(v2_arg(argv,0),g_last_cam2d); return 0.0; }
static double fn_GetCameraMatrix(int argc,const char**argv){ g_last_mat=GetCameraMatrix(g_last_cam); return 0.0; }
static double fn_GetCameraMatrix2D(int argc,const char**argv){ g_last_mat=GetCameraMatrix2D(g_last_cam2d); return 0.0; }

/* timing */
static double fn_SetTargetFPS(int argc,const char**argv){ SetTargetFPS(ii(argv[0])); return 0.0; }
static double fn_GetFrameTime(int argc,const char**argv){(void)argc;(void)argv;return (double)GetFrameTime();}
static double fn_GetTime(int argc,const char**argv){(void)argc;(void)argv;return GetTime();}
static double fn_GetFPS(int argc,const char**argv){(void)argc;(void)argv;return (double)GetFPS();}
static double fn_SwapScreenBuffer(int argc,const char**argv){(void)argc;(void)argv;SwapScreenBuffer();return 0.0;}
static double fn_PollInputEvents(int argc,const char**argv){(void)argc;(void)argv;PollInputEvents();return 0.0;}
static double fn_WaitTime(int argc,const char**argv){ WaitTime(atof(argv[0])); return 0.0; }
static double fn_SetRandomSeed(int argc,const char**argv){ SetRandomSeed((unsigned int)strtoul(argv[0],NULL,10)); return 0.0; }
static double fn_GetRandomValue(int argc,const char**argv){ return (double)GetRandomValue(ii(argv[0]),ii(argv[1])); }
static double fn_LoadRandomSequence(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadRandomSequence(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TakeScreenshot(int argc,const char**argv){ TakeScreenshot(ss(argv[0])); return 0.0; }
static double fn_SetConfigFlags(int argc,const char**argv){ SetConfigFlags((unsigned int)strtoul(argv[0],NULL,10)); return 0.0; }
static double fn_OpenURL(int argc,const char**argv){ OpenURL(ss(argv[0])); return 0.0; }
static double fn_SetTraceLogLevel(int argc,const char**argv){ SetTraceLogLevel(ii(argv[0])); return 0.0; }
static double fn_TraceLog(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetTraceLogCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_MemAlloc(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_MemRealloc(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_MemFree(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}

/* file system */
static double fn_LoadFileData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadFileData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SaveFileData(int argc,const char**argv){ return SaveFileData(ss(argv[0]),argv[1],ii(argv[2]))?1.0:0.0; }
static double fn_ExportDataAsCode(int argc,const char**argv){ return ExportDataAsCode((const unsigned char*)(argv[0]?argv[0]:(const char*)""),ii(argv[1]),ss(argv[2]))?1.0:0.0; }
static double fn_LoadFileText(int argc,const char**argv){ char *t=LoadFileText(ss(argv[0])); if(t){snprintf(g_str,sizeof(g_str),"%s",t); UnloadFileText(t);} else g_str[0]=0; return 0.0; }
static double fn_UnloadFileText(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SaveFileText(int argc,const char**argv){ return SaveFileText(ss(argv[0]),ss(argv[1]))?1.0:0.0; }
static double fn_SetLoadFileDataCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetSaveFileDataCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetLoadFileTextCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetSaveFileTextCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_FileRename(int argc,const char**argv){ return (double)FileRename(ss(argv[0]),ss(argv[1])); }
static double fn_FileRemove(int argc,const char**argv){ return (double)FileRemove(ss(argv[0])); }
static double fn_FileCopy(int argc,const char**argv){ return (double)FileCopy(ss(argv[0]),ss(argv[1])); }
static double fn_FileMove(int argc,const char**argv){ return (double)FileMove(ss(argv[0]),ss(argv[1])); }
static double fn_FileTextReplace(int argc,const char**argv){ return (double)FileTextReplace(ss(argv[0]),ss(argv[1]),ss(argv[2])); }
static double fn_FileTextFindIndex(int argc,const char**argv){ return (double)FileTextFindIndex(ss(argv[0]),ss(argv[1])); }
static double fn_FileExists(int argc,const char**argv){ return FileExists(ss(argv[0]))?1.0:0.0; }
static double fn_DirectoryExists(int argc,const char**argv){ return DirectoryExists(ss(argv[0]))?1.0:0.0; }
static double fn_IsFileExtension(int argc,const char**argv){ return IsFileExtension(ss(argv[0]),ss(argv[1]))?1.0:0.0; }
static double fn_GetFileLength(int argc,const char**argv){ return (double)GetFileLength(ss(argv[0])); }
static double fn_GetFileModTime(int argc,const char**argv){ return (double)GetFileModTime(ss(argv[0])); }
static double fn_GetFileExtension(int argc,const char**argv){ const char *s=GetFileExtension(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0; }
static double fn_GetFileName(int argc,const char**argv){ const char *s=GetFileName(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0; }
static double fn_GetFileNameWithoutExt(int argc,const char**argv){ const char *s=GetFileNameWithoutExt(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0; }
static double fn_GetDirectoryPath(int argc,const char**argv){ const char *s=GetDirectoryPath(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0; }
static double fn_GetPrevDirectoryPath(int argc,const char**argv){ const char *s=GetPrevDirectoryPath(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} else g_str[0]=0; return 0.0; }
static double fn_GetWorkingDirectory(int argc,const char**argv){ const char *s=GetWorkingDirectory(); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_GetApplicationDirectory(int argc,const char**argv){ const char *s=GetApplicationDirectory(); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_MakeDirectory(int argc,const char**argv){ return (double)MakeDirectory(ss(argv[0])); }
static double fn_ChangeDirectory(int argc,const char**argv){ return (double)ChangeDirectory(ss(argv[0])); }
static double fn_IsPathFile(int argc,const char**argv){ return IsPathFile(ss(argv[0]))?1.0:0.0; }
static double fn_IsPathDirectory(int argc,const char**argv){ return IsPathDirectory(ss(argv[0]))?1.0:0.0; }
static double fn_IsPathAbsolute(int argc,const char**argv){ return IsPathAbsolute(ss(argv[0]))?1.0:0.0; }
static double fn_IsFileNameValid(int argc,const char**argv){ return IsFileNameValid(ss(argv[0]))?1.0:0.0; }
static double fn_LoadDirectoryFiles(int argc,const char**argv){ g_last_handle=reg_fpl(LoadDirectoryFiles(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadDirectoryFilesEx(int argc,const char**argv){ g_last_handle=reg_fpl(LoadDirectoryFilesEx(ss(argv[0]),ss(argv[1]),(argc>2)?(ii(argv[2])!=0):0)); return (double)g_last_handle; }
static double fn_UnloadDirectoryFiles(int argc,const char**argv){ UnloadDirectoryFiles(get_fpl(ii(argv[0]))); return 0.0; }
static double fn_IsFileDropped(int argc,const char**argv){ return IsFileDropped()?1.0:0.0; }
static double fn_LoadDroppedFiles(int argc,const char**argv){ g_last_handle=reg_fpl(LoadDroppedFiles()); return (double)g_last_handle; }
static double fn_UnloadDroppedFiles(int argc,const char**argv){ UnloadDroppedFiles(get_fpl(ii(argv[0]))); return 0.0; }
static double fn_GetDirectoryFileCount(int argc,const char**argv){ return (double)GetDirectoryFileCount(ss(argv[0])); }
static double fn_GetDirectoryFileCountEx(int argc,const char**argv){ return (double)GetDirectoryFileCountEx(ss(argv[0]),ss(argv[1]),(argc>2)?(ii(argv[2])!=0):0); }
static double fn_CompressData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DecompressData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_EncodeDataBase64(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DecodeDataBase64(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ComputeCRC32(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ComputeMD5(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ComputeSHA1(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ComputeSHA256(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadAutomationEventList(int argc,const char**argv){ g_last_handle=reg_aevl(LoadAutomationEventList(ss(argv[0]))); return (double)g_last_handle; }
static double fn_UnloadAutomationEventList(int argc,const char**argv){ UnloadAutomationEventList(get_aevl(ii(argv[0]))); return 0.0; }
static double fn_ExportAutomationEventList(int argc,const char**argv){ return ExportAutomationEventList(get_aevl(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_SetAutomationEventList(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetAutomationEventBaseFrame(int argc,const char**argv){ SetAutomationEventBaseFrame(ii(argv[0])); return 0.0; }
static double fn_StartAutomationEventRecording(int argc,const char**argv){(void)argc;(void)argv;StartAutomationEventRecording();return 0.0;}
static double fn_StopAutomationEventRecording(int argc,const char**argv){(void)argc;(void)argv;StopAutomationEventRecording();return 0.0;}
static double fn_PlayAutomationEvent(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}

/* =========================================================================
   CONSTANTS — CAMERA_* / KEY_* / MOUSE_* (Raylib.CAMERA_PERSPECTIVE vb.)
   ========================================================================= */
#define ENUM_FN(cn,cv) static double fn_##cn(int argc,const char**argv){(void)argc;(void)argv;return (double)(cv);}
ENUM_FN(CAMERA_FREE,CAMERA_FREE)
ENUM_FN(CAMERA_ORBITAL,CAMERA_ORBITAL)
ENUM_FN(CAMERA_FIRST_PERSON,CAMERA_FIRST_PERSON)
ENUM_FN(CAMERA_THIRD_PERSON,CAMERA_THIRD_PERSON)
ENUM_FN(CAMERA_PERSPECTIVE,CAMERA_PERSPECTIVE)
ENUM_FN(CAMERA_ORTHOGRAPHIC,CAMERA_ORTHOGRAPHIC)
ENUM_FN(KEY_Z,KEY_Z)
ENUM_FN(KEY_ESCAPE,KEY_ESCAPE)
ENUM_FN(KEY_SPACE,KEY_SPACE)
ENUM_FN(KEY_W,KEY_W)
ENUM_FN(KEY_A,KEY_A)
ENUM_FN(KEY_S,KEY_S)
ENUM_FN(KEY_D,KEY_D)
ENUM_FN(MOUSE_BUTTON_LEFT,MOUSE_BUTTON_LEFT)
ENUM_FN(MOUSE_BUTTON_RIGHT,MOUSE_BUTTON_RIGHT)
ENUM_FN(MOUSE_BUTTON_MIDDLE,MOUSE_BUTTON_MIDDLE)

/* =========================================================================
   INPUT
   ========================================================================= */
static double fn_IsKeyPressed(int argc,const char**argv){ return IsKeyPressed(ii(argv[0]))?1.0:0.0; }
static double fn_IsKeyPressedRepeat(int argc,const char**argv){ return IsKeyPressedRepeat(ii(argv[0]))?1.0:0.0; }
static double fn_IsKeyDown(int argc,const char**argv){ return IsKeyDown(ii(argv[0]))?1.0:0.0; }
static double fn_IsKeyReleased(int argc,const char**argv){ return IsKeyReleased(ii(argv[0]))?1.0:0.0; }
static double fn_IsKeyUp(int argc,const char**argv){ return IsKeyUp(ii(argv[0]))?1.0:0.0; }
static double fn_GetKeyPressed(int argc,const char**argv){(void)argc;(void)argv;return (double)GetKeyPressed();}
static double fn_GetCharPressed(int argc,const char**argv){(void)argc;(void)argv;return (double)GetCharPressed();}
static double fn_GetKeyName(int argc,const char**argv){ const char *s=GetKeyName(ii(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_SetExitKey(int argc,const char**argv){ SetExitKey(ii(argv[0])); return 0.0; }

static double fn_IsGamepadAvailable(int argc,const char**argv){ return IsGamepadAvailable(ii(argv[0]))?1.0:0.0; }
static double fn_GetGamepadName(int argc,const char**argv){ const char *s=GetGamepadName(ii(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_IsGamepadButtonPressed(int argc,const char**argv){ return IsGamepadButtonPressed(ii(argv[0]),ii(argv[1]))?1.0:0.0; }
static double fn_IsGamepadButtonDown(int argc,const char**argv){ return IsGamepadButtonDown(ii(argv[0]),ii(argv[1]))?1.0:0.0; }
static double fn_IsGamepadButtonReleased(int argc,const char**argv){ return IsGamepadButtonReleased(ii(argv[0]),ii(argv[1]))?1.0:0.0; }
static double fn_IsGamepadButtonUp(int argc,const char**argv){ return IsGamepadButtonUp(ii(argv[0]),ii(argv[1]))?1.0:0.0; }
static double fn_GetGamepadButtonPressed(int argc,const char**argv){(void)argc;(void)argv;return (double)GetGamepadButtonPressed();}
static double fn_GetGamepadAxisCount(int argc,const char**argv){ return (double)GetGamepadAxisCount(ii(argv[0])); }
static double fn_GetGamepadAxisMovement(int argc,const char**argv){ return (double)GetGamepadAxisMovement(ii(argv[0]),ii(argv[1])); }
static double fn_SetGamepadMappings(int argc,const char**argv){ return (double)SetGamepadMappings(ss(argv[0])); }
static double fn_SetGamepadVibration(int argc,const char**argv){ SetGamepadVibration(ii(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3])); return 0.0; }

static double fn_IsMouseButtonPressed(int argc,const char**argv){ return IsMouseButtonPressed(ii(argv[0]))?1.0:0.0; }
static double fn_IsMouseButtonDown(int argc,const char**argv){ return IsMouseButtonDown(ii(argv[0]))?1.0:0.0; }
static double fn_IsMouseButtonReleased(int argc,const char**argv){ return IsMouseButtonReleased(ii(argv[0]))?1.0:0.0; }
static double fn_IsMouseButtonUp(int argc,const char**argv){ return IsMouseButtonUp(ii(argv[0]))?1.0:0.0; }
static double fn_GetMouseX(int argc,const char**argv){ return (double)GetMouseX(); }
static double fn_GetMouseY(int argc,const char**argv){ return (double)GetMouseY(); }
static double fn_GetMousePosition(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetMousePosition();return 0.0;}
static double fn_GetMouseDelta(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetMouseDelta();return 0.0;}
static double fn_SetMousePosition(int argc,const char**argv){ SetMousePosition(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetMouseOffset(int argc,const char**argv){ SetMouseOffset(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetMouseScale(int argc,const char**argv){ SetMouseScale(ff(argv[0]),ff(argv[1])); return 0.0; }
static double fn_GetMouseWheelMove(int argc,const char**argv){(void)argc;(void)argv;return (double)GetMouseWheelMove();}
static double fn_GetMouseWheelMoveV(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetMouseWheelMoveV();return 0.0;}
static double fn_SetMouseCursor(int argc,const char**argv){ SetMouseCursor(ii(argv[0])); return 0.0; }

static double fn_GetTouchX(int argc,const char**argv){(void)argc;(void)argv;return (double)GetTouchX();}
static double fn_GetTouchY(int argc,const char**argv){(void)argc;(void)argv;return (double)GetTouchY();}
static double fn_GetTouchPosition(int argc,const char**argv){ g_last_v2=GetTouchPosition(ii(argv[0])); return 0.0; }
static double fn_GetTouchPointId(int argc,const char**argv){ return (double)GetTouchPointId(ii(argv[0])); }
static double fn_GetTouchPointCount(int argc,const char**argv){(void)argc;(void)argv;return (double)GetTouchPointCount();}

/* gestures */
static double fn_SetGesturesEnabled(int argc,const char**argv){ SetGesturesEnabled((unsigned int)strtoul(argv[0],NULL,10)); return 0.0; }
static double fn_IsGestureDetected(int argc,const char**argv){ return IsGestureDetected((unsigned int)strtoul(argv[0],NULL,10))?1.0:0.0; }
static double fn_GetGestureDetected(int argc,const char**argv){(void)argc;(void)argv;return (double)GetGestureDetected();}
static double fn_GetGestureHoldDuration(int argc,const char**argv){(void)argc;(void)argv;return (double)GetGestureHoldDuration();}
static double fn_GetGestureDragVector(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetGestureDragVector();return 0.0;}
static double fn_GetGestureDragAngle(int argc,const char**argv){(void)argc;(void)argv;return (double)GetGestureDragAngle();}
static double fn_GetGesturePinchVector(int argc,const char**argv){(void)argc;(void)argv;g_last_v2=GetGesturePinchVector();return 0.0;}
static double fn_GetGesturePinchAngle(int argc,const char**argv){(void)argc;(void)argv;return (double)GetGesturePinchAngle();}

/* camera */
static double fn_UpdateCamera(int argc,const char**argv){
    /* UpdateCamera(&camera, CAMERA_FREE) — runner Camera3D üyelerini
       düzleştirip 11 arg (pos.xyz, tgt.xyz, up.xyz, fovy, proj) + mode verir. */
    if (argc >= 12) {
        g_last_cam=(Camera3D){v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ff(argv[9]),ii(argv[10])};
        UpdateCamera(&g_last_cam,ii(argv[11]));
    } else {
        UpdateCamera(&g_last_cam,ii(argv[0]));
    }
    return 0.0;
}
static double fn_UpdateCameraPro(int argc,const char**argv){ UpdateCameraPro(&g_last_cam,v3_arg(argv,0),v3_arg(argv,3),ff(argv[6])); return 0.0; }

/* =========================================================================
   2D SHAPES
   ========================================================================= */
static double fn_SetShapesTexture(int argc,const char**argv){ SetShapesTexture(get_tex(ii(argv[0])),rect_arg(argv,1)); return 0.0; }
static double fn_GetShapesTexture(int argc,const char**argv){ g_last_handle=reg_tex(GetShapesTexture()); return (double)g_last_handle; }
static double fn_GetShapesTextureRectangle(int argc,const char**argv){(void)argc;(void)argv;g_last_rect=GetShapesTextureRectangle();return 0.0;}

static double fn_DrawPixel(int argc,const char**argv){ DrawPixel(ii(argv[0]),ii(argv[1]),ci(argv,2)); return 0.0; }
static double fn_DrawPixelV(int argc,const char**argv){ DrawPixelV(v2_arg(argv,0),ci(argv,2)); return 0.0; }
static double fn_DrawLine(int argc,const char**argv){ DrawLine(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawLineV(int argc,const char**argv){ DrawLineV(v2_arg(argv,0),v2_arg(argv,2),ci(argv,4)); return 0.0; }
static double fn_DrawLineEx(int argc,const char**argv){ DrawLineEx(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawLineStrip(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawLineBezier(int argc,const char**argv){ DrawLineBezier(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawLineDashed(int argc,const char**argv){ DrawLineDashed(v2_arg(argv,0),v2_arg(argv,2),ii(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawTriangle(int argc,const char**argv){ DrawTriangle(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ci(argv,6)); return 0.0; }
static double fn_DrawTriangleLines(int argc,const char**argv){ DrawTriangleLines(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ci(argv,6)); return 0.0; }
static double fn_DrawTriangleFan(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawTriangleStrip(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawRectangle(int argc,const char**argv){ DrawRectangle(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawRectangleV(int argc,const char**argv){ DrawRectangleV(v2_arg(argv,0),v2_arg(argv,2),ci(argv,4)); return 0.0; }
static double fn_DrawRectangleRec(int argc,const char**argv){ DrawRectangleRec(rect_arg(argv,0),ci(argv,4)); return 0.0; }
static double fn_DrawRectanglePro(int argc,const char**argv){ DrawRectanglePro(rect_arg(argv,0),v2_arg(argv,4),ff(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawRectangleGradientV(int argc,const char**argv){ DrawRectangleGradientV(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4),ci(argv,5)); return 0.0; }
static double fn_DrawRectangleGradientH(int argc,const char**argv){ DrawRectangleGradientH(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4),ci(argv,5)); return 0.0; }
static double fn_DrawRectangleGradientEx(int argc,const char**argv){ DrawRectangleGradientEx(rect_arg(argv,0),ci(argv,4),ci(argv,5),ci(argv,6),ci(argv,7)); return 0.0; }
static double fn_DrawRectangleLines(int argc,const char**argv){ DrawRectangleLines(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawRectangleLinesEx(int argc,const char**argv){ DrawRectangleLinesEx(rect_arg(argv,0),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawRectangleRounded(int argc,const char**argv){ DrawRectangleRounded(rect_arg(argv,0),ff(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawRectangleRoundedLines(int argc,const char**argv){ DrawRectangleRoundedLines(rect_arg(argv,0),ff(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawRectangleRoundedLinesEx(int argc,const char**argv){ DrawRectangleRoundedLinesEx(rect_arg(argv,0),ff(argv[4]),ii(argv[5]),ff(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawPoly(int argc,const char**argv){ DrawPoly(v2_arg(argv,0),ii(argv[2]),ff(argv[3]),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawPolyLines(int argc,const char**argv){ DrawPolyLines(v2_arg(argv,0),ii(argv[2]),ff(argv[3]),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawPolyLinesEx(int argc,const char**argv){ DrawPolyLinesEx(v2_arg(argv,0),ii(argv[2]),ff(argv[3]),ff(argv[4]),ff(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCircle(int argc,const char**argv){ DrawCircle(ii(argv[0]),ii(argv[1]),ff(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawCircleV(int argc,const char**argv){ DrawCircleV(v2_arg(argv,0),ff(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawCircleGradient(int argc,const char**argv){ DrawCircleGradient(v2_arg(argv,0),ff(argv[2]),ci(argv,3),ci(argv,4)); return 0.0; }
static double fn_DrawCircleSector(int argc,const char**argv){ DrawCircleSector(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCircleSectorLines(int argc,const char**argv){ DrawCircleSectorLines(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCircleSectorLinesEx(int argc,const char**argv){ DrawCircleSectorLinesEx(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ii(argv[5]),ff(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawCircleLines(int argc,const char**argv){ DrawCircleLines(ii(argv[0]),ii(argv[1]),ff(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawCircleLinesV(int argc,const char**argv){ DrawCircleLinesV(v2_arg(argv,0),ff(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawCircleLinesEx(int argc,const char**argv){ DrawCircleLinesEx(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawEllipse(int argc,const char**argv){ DrawEllipse(ii(argv[0]),ii(argv[1]),ff(argv[2]),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawEllipseV(int argc,const char**argv){ DrawEllipseV(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawEllipseLines(int argc,const char**argv){ DrawEllipseLines(ii(argv[0]),ii(argv[1]),ff(argv[2]),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawEllipseLinesV(int argc,const char**argv){ DrawEllipseLinesV(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawEllipseLinesEx(int argc,const char**argv){ DrawEllipseLinesEx(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawRing(int argc,const char**argv){ DrawRing(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ff(argv[5]),ii(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawRingLines(int argc,const char**argv){ DrawRingLines(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ff(argv[5]),ii(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawRingLinesEx(int argc,const char**argv){ DrawRingLinesEx(v2_arg(argv,0),ff(argv[2]),ff(argv[3]),ff(argv[4]),ff(argv[5]),ii(argv[6]),ff(argv[7]),ci(argv,8)); return 0.0; }
static double fn_DrawSplineLinear(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawSplineBasis(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawSplineCatmullRom(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawSplineBezierQuadratic(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawSplineBezierCubic(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawSplineSegmentLinear(int argc,const char**argv){ DrawSplineSegmentLinear(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawSplineSegmentBasis(int argc,const char**argv){ DrawSplineSegmentBasis(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawSplineSegmentCatmullRom(int argc,const char**argv){ DrawSplineSegmentCatmullRom(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawSplineSegmentBezierQuadratic(int argc,const char**argv){ DrawSplineSegmentBezierQuadratic(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ff(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawSplineSegmentBezierCubic(int argc,const char**argv){ DrawSplineSegmentBezierCubic(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8]),ci(argv,9)); return 0.0; }
static double fn_GetSplinePointLinear(int argc,const char**argv){ g_last_v2=GetSplinePointLinear(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4])); return 0.0; }
static double fn_GetSplinePointBasis(int argc,const char**argv){ g_last_v2=GetSplinePointBasis(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8])); return 0.0; }
static double fn_GetSplinePointCatmullRom(int argc,const char**argv){ g_last_v2=GetSplinePointCatmullRom(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8])); return 0.0; }
static double fn_GetSplinePointBezierQuadratic(int argc,const char**argv){ g_last_v2=GetSplinePointBezierQuadratic(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ff(argv[6])); return 0.0; }
static double fn_GetSplinePointBezierCubic(int argc,const char**argv){ g_last_v2=GetSplinePointBezierCubic(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6),ff(argv[8])); return 0.0; }

/* collision */
static double fn_CheckCollisionRecs(int argc,const char**argv){ return CheckCollisionRecs(rect_arg(argv,0),rect_arg(argv,4))?1.0:0.0; }
static double fn_CheckCollisionCircles(int argc,const char**argv){ return CheckCollisionCircles(v2_arg(argv,0),ff(argv[2]),v2_arg(argv,3),ff(argv[5]))?1.0:0.0; }
static double fn_CheckCollisionCircleRec(int argc,const char**argv){ return CheckCollisionCircleRec(v2_arg(argv,0),ff(argv[2]),rect_arg(argv,3))?1.0:0.0; }
static double fn_CheckCollisionCircleLine(int argc,const char**argv){ return CheckCollisionCircleLine(v2_arg(argv,0),ff(argv[2]),v2_arg(argv,3),v2_arg(argv,5))?1.0:0.0; }
static double fn_CheckCollisionPointRec(int argc,const char**argv){ return CheckCollisionPointRec(v2_arg(argv,0),rect_arg(argv,2))?1.0:0.0; }
static double fn_CheckCollisionPointCircle(int argc,const char**argv){ return CheckCollisionPointCircle(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]))?1.0:0.0; }
static double fn_CheckCollisionPointTriangle(int argc,const char**argv){ return CheckCollisionPointTriangle(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),v2_arg(argv,6))?1.0:0.0; }
static double fn_CheckCollisionPointLine(int argc,const char**argv){ return CheckCollisionPointLine(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ii(argv[6]))?1.0:0.0; }
static double fn_CheckCollisionPointPoly(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_CheckCollisionLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetCollisionRec(int argc,const char**argv){ g_last_rect=GetCollisionRec(rect_arg(argv,0),rect_arg(argv,4)); return 0.0; }

/* =========================================================================
   TEXTURES / IMAGES
   ========================================================================= */
static double fn_LoadImage(int argc,const char**argv){ g_last_handle=reg_img(LoadImage(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadImageRaw(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_img((Image){0});}
static double fn_LoadImageAnim(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_img((Image){0});}
static double fn_LoadImageAnimFromMemory(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_img((Image){0});}
static double fn_LoadImageFromMemory(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_img((Image){0});}
static double fn_LoadImageFromTexture(int argc,const char**argv){ g_last_handle=reg_img(LoadImageFromTexture(get_tex(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadImageFromScreen(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_img(LoadImageFromScreen());return (double)g_last_handle;}
static double fn_IsImageValid(int argc,const char**argv){ return IsImageValid(get_img(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadImage(int argc,const char**argv){ UnloadImage(get_img(ii(argv[0]))); return 0.0; }
static double fn_ExportImage(int argc,const char**argv){ return ExportImage(get_img(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_ExportImageToMemory(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ExportImageAsCode(int argc,const char**argv){ return ExportImageAsCode(get_img(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_GenImageColor(int argc,const char**argv){ g_last_handle=reg_img(GenImageColor(ii(argv[0]),ii(argv[1]),ci(argv,2))); return (double)g_last_handle; }
static double fn_GenImageGradientLinear(int argc,const char**argv){ g_last_handle=reg_img(GenImageGradientLinear(ii(argv[0]),ii(argv[1]),ii(argv[2]),ci(argv,3),ci(argv,4))); return (double)g_last_handle; }
static double fn_GenImageGradientRadial(int argc,const char**argv){ g_last_handle=reg_img(GenImageGradientRadial(ii(argv[0]),ii(argv[1]),ff(argv[2]),ci(argv,3),ci(argv,4))); return (double)g_last_handle; }
static double fn_GenImageGradientSquare(int argc,const char**argv){ g_last_handle=reg_img(GenImageGradientSquare(ii(argv[0]),ii(argv[1]),ff(argv[2]),ci(argv,3),ci(argv,4))); return (double)g_last_handle; }
static double fn_GenImageChecked(int argc,const char**argv){ g_last_handle=reg_img(GenImageChecked(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4),ci(argv,5))); return (double)g_last_handle; }
static double fn_GenImageWhiteNoise(int argc,const char**argv){ g_last_handle=reg_img(GenImageWhiteNoise(ii(argv[0]),ii(argv[1]),ff(argv[2]))); return (double)g_last_handle; }
static double fn_GenImagePerlinNoise(int argc,const char**argv){ g_last_handle=reg_img(GenImagePerlinNoise(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ff(argv[4]))); return (double)g_last_handle; }
static double fn_GenImageCellular(int argc,const char**argv){ g_last_handle=reg_img(GenImageCellular(ii(argv[0]),ii(argv[1]),ii(argv[2]))); return (double)g_last_handle; }
static double fn_GenImageText(int argc,const char**argv){ g_last_handle=reg_img(GenImageText(ii(argv[0]),ii(argv[1]),ss(argv[2]))); return (double)g_last_handle; }
static double fn_ImageCopy(int argc,const char**argv){ g_last_handle=reg_img(ImageCopy(get_img(ii(argv[0])))); return (double)g_last_handle; }
static double fn_ImageFromImage(int argc,const char**argv){ g_last_handle=reg_img(ImageFromImage(get_img(ii(argv[0])),rect_arg(argv,1))); return (double)g_last_handle; }
static double fn_ImageFromChannel(int argc,const char**argv){ g_last_handle=reg_img(ImageFromChannel(get_img(ii(argv[0])),ii(argv[1]))); return (double)g_last_handle; }
static double fn_ImageText(int argc,const char**argv){ g_last_handle=reg_img(ImageText(ss(argv[0]),ii(argv[1]),ci(argv,2))); return (double)g_last_handle; }
static double fn_ImageTextEx(int argc,const char**argv){ g_last_handle=reg_img(ImageTextEx(get_font(ii(argv[0])),ss(argv[1]),ff(argv[2]),ff(argv[3]),ci(argv,4))); return (double)g_last_handle; }
static double fn_ImageFormat(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageToPOT(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageCrop(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageAlphaCrop(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageAlphaClear(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageAlphaMask(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageAlphaPremultiply(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageBlurGaussian(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageKernelConvolution(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageResize(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageResizeNN(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageResizeCanvas(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageMipmaps(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDither(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageFlipVertical(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageFlipHorizontal(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageRotate(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageRotateCW(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageRotateCCW(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorTint(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorInvert(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorGrayscale(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorContrast(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorBrightness(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageColorReplace(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadImageColors(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadImagePalette(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadImageColors(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadImagePalette(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetImageAlphaBorder(int argc,const char**argv){ g_last_rect=GetImageAlphaBorder(get_img(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_GetImageColor(int argc,const char**argv){ return (double)color_to_uint(GetImageColor(get_img(ii(argv[0])),ii(argv[1]),ii(argv[2]))); }

/* image drawing */
static double fn_ImageClearBackground(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawPixel(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawPixelV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawLine(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawLineV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawLineEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawLineStrip(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTriangle(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTriangleGradient(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTriangleLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTriangleFan(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTriangleStrip(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangle(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangleV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangleRec(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectanglePro(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangleLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangleLinesEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawRectangleGradientEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawCircle(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawCircleV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawCircleLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawCircleLinesV(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawCircleGradient(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawImage(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawImageEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawImageRec(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawImagePro(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawText(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTextEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ImageDrawTextPro(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}

/* texture */
static double fn_LoadTexture(int argc,const char**argv){ g_last_handle=reg_tex(LoadTexture(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadTextureFromImage(int argc,const char**argv){ g_last_handle=reg_tex(LoadTextureFromImage(get_img(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadTextureCubemap(int argc,const char**argv){ g_last_handle=reg_tex(LoadTextureCubemap(get_img(ii(argv[0])),ii(argv[1]))); return (double)g_last_handle; }
static double fn_LoadRenderTexture(int argc,const char**argv){ g_last_handle=reg_rt(LoadRenderTexture(ii(argv[0]),ii(argv[1]))); return (double)g_last_handle; }
static double fn_IsTextureValid(int argc,const char**argv){ return IsTextureValid(get_tex(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadTexture(int argc,const char**argv){ UnloadTexture(get_tex(ii(argv[0]))); return 0.0; }
static double fn_IsRenderTextureValid(int argc,const char**argv){ return IsRenderTextureValid(get_rt(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadRenderTexture(int argc,const char**argv){ UnloadRenderTexture(get_rt(ii(argv[0]))); return 0.0; }
static double fn_UpdateTexture(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UpdateTextureRec(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GenTextureMipmaps(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetTextureFilter(int argc,const char**argv){ SetTextureFilter(get_tex(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_SetTextureWrap(int argc,const char**argv){ SetTextureWrap(get_tex(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_DrawTexture(int argc,const char**argv){ DrawTexture(get_tex(ii(argv[0])),ii(argv[1]),ii(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawTextureV(int argc,const char**argv){ DrawTextureV(get_tex(ii(argv[0])),v2_arg(argv,1),ci(argv,3)); return 0.0; }
static double fn_DrawTextureEx(int argc,const char**argv){ DrawTextureEx(get_tex(ii(argv[0])),v2_arg(argv,1),ff(argv[3]),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawTextureRec(int argc,const char**argv){ DrawTextureRec(get_tex(ii(argv[0])),rect_arg(argv,1),v2_arg(argv,5),ci(argv,7)); return 0.0; }
static double fn_DrawTexturePro(int argc,const char**argv){ DrawTexturePro(get_tex(ii(argv[0])),rect_arg(argv,1),rect_arg(argv,5),v2_arg(argv,9),ff(argv[11]),ci(argv,12)); return 0.0; }
static double fn_DrawTextureNPatch(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}

/* =========================================================================
   TEXT / FONT
   ========================================================================= */
static double fn_GetFontDefault(int argc,const char**argv){ g_last_handle=reg_font(GetFontDefault()); return (double)g_last_handle; }
static double fn_LoadFont(int argc,const char**argv){ g_last_handle=reg_font(LoadFont(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadFontEx(int argc,const char**argv){ g_last_handle=reg_font(LoadFontEx(ss(argv[0]),ii(argv[1]),NULL,0)); return (double)g_last_handle; }
static double fn_LoadFontFromImage(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_font((Font){0});}
static double fn_LoadFontFromMemory(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_font((Font){0});}
static double fn_IsFontValid(int argc,const char**argv){ return IsFontValid(get_font(ii(argv[0])))?1.0:0.0; }
static double fn_LoadFontData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GenImageFontAtlas(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_img((Image){0});}
static double fn_UnloadFontData(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadFont(int argc,const char**argv){ UnloadFont(get_font(ii(argv[0]))); return 0.0; }
static double fn_ExportFontAsCode(int argc,const char**argv){ return ExportFontAsCode(get_font(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_DrawFPS(int argc,const char**argv){ DrawFPS(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_DrawText(int argc,const char**argv){ DrawText(ss(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawTextEx(int argc,const char**argv){ DrawTextEx(get_font(ii(argv[0])),ss(argv[1]),v2_arg(argv,2),ff(argv[4]),ff(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawTextPro(int argc,const char**argv){ DrawTextPro(get_font(ii(argv[0])),ss(argv[1]),v2_arg(argv,2),v2_arg(argv,4),ff(argv[6]),ff(argv[7]),ff(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawTextCodepoint(int argc,const char**argv){ DrawTextCodepoint(get_font(ii(argv[0])),ii(argv[1]),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawTextCodepoints(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetTextLineSpacing(int argc,const char**argv){ SetTextLineSpacing(ii(argv[0])); return 0.0; }
static double fn_MeasureText(int argc,const char**argv){ return (double)MeasureText(ss(argv[0]),ii(argv[1])); }
static double fn_MeasureTextEx(int argc,const char**argv){ g_last_v2=MeasureTextEx(get_font(ii(argv[0])),ss(argv[1]),ff(argv[2]),ff(argv[3])); return 0.0; }
static double fn_MeasureTextCodepoints(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetGlyphIndex(int argc,const char**argv){ return (double)GetGlyphIndex(get_font(ii(argv[0])),ii(argv[1])); }
static double fn_GetGlyphInfo(int argc,const char**argv){ g_last_glyph=GetGlyphInfo(get_font(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_GetGlyphAtlasRec(int argc,const char**argv){ g_last_rect=GetGlyphAtlasRec(get_font(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_LoadUTF8(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadUTF8(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadCodepoints(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadCodepoints(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetCodepointCount(int argc,const char**argv){ return (double)GetCodepointCount(ss(argv[0])); }
static double fn_GetCodepoint(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetCodepointNext(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetCodepointPrevious(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_CodepointToUTF8(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadTextLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadTextLines(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextCopy(int argc,const char**argv){ return (double)TextCopy((char*)argv[0],ss(argv[1])); }
static double fn_TextIsEqual(int argc,const char**argv){ return TextIsEqual(ss(argv[0]),ss(argv[1]))?1.0:0.0; }
static double fn_TextLength(int argc,const char**argv){ return (double)TextLength(ss(argv[0])); }
static double fn_TextFormat(int argc,const char**argv){ const char *s=TextFormat(ss(argv[0]),ss(argv[1]),ss(argv[2]),ss(argv[3]),ss(argv[4]),ss(argv[5]),ss(argv[6]),ss(argv[7]),ss(argv[8]),ss(argv[9]),ss(argv[10]),ss(argv[11]),ss(argv[12]),ss(argv[13]),ss(argv[14]),ss(argv[15])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextSubtext(int argc,const char**argv){ const char *s=TextSubtext(ss(argv[0]),ii(argv[1]),ii(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextRemoveSpaces(int argc,const char**argv){ const char *s=TextRemoveSpaces(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_GetTextBetween(int argc,const char**argv){ char *s=GetTextBetween(ss(argv[0]),ss(argv[1]),ss(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextReplace(int argc,const char**argv){ char *s=TextReplace(ss(argv[0]),ss(argv[1]),ss(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextReplaceAlloc(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextReplaceBetween(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextReplaceBetweenAlloc(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextInsert(int argc,const char**argv){ char *s=TextInsert(ss(argv[0]),ss(argv[1]),ii(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextInsertAlloc(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextJoin(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextSplit(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextAppend(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_TextFindIndex(int argc,const char**argv){ return (double)TextFindIndex(ss(argv[0]),ss(argv[1])); }
static double fn_TextToUpper(int argc,const char**argv){ char *s=TextToUpper(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextToLower(int argc,const char**argv){ char *s=TextToLower(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextToPascal(int argc,const char**argv){ char *s=TextToPascal(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextToSnake(int argc,const char**argv){ char *s=TextToSnake(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextToCamel(int argc,const char**argv){ char *s=TextToCamel(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextToInteger(int argc,const char**argv){ return (double)TextToInteger(ss(argv[0])); }
static double fn_TextToFloat(int argc,const char**argv){ return (double)TextToFloat(ss(argv[0])); }

/* =========================================================================
   3D MODELS
   ========================================================================= */
static double fn_DrawLine3D(int argc,const char**argv){ DrawLine3D(v3_arg(argv,0),v3_arg(argv,3),ci(argv,6)); return 0.0; }
static double fn_DrawPoint3D(int argc,const char**argv){ DrawPoint3D(v3_arg(argv,0),ci(argv,3)); return 0.0; }
static double fn_DrawCircle3D(int argc,const char**argv){ DrawCircle3D(v3_arg(argv,0),ff(argv[3]),v3_arg(argv,4),ff(argv[7]),ci(argv,8)); return 0.0; }
static double fn_DrawTriangle3D(int argc,const char**argv){ DrawTriangle3D(v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ci(argv,9)); return 0.0; }
static double fn_DrawTriangleStrip3D(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DrawCube(int argc,const char**argv){ DrawCube(v3_arg(argv,0),ff(argv[3]),ff(argv[4]),ff(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCubeV(int argc,const char**argv){ DrawCubeV(v3_arg(argv,0),v3_arg(argv,3),ci(argv,6)); return 0.0; }
static double fn_DrawCubeWires(int argc,const char**argv){ DrawCubeWires(v3_arg(argv,0),ff(argv[3]),ff(argv[4]),ff(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCubeWiresV(int argc,const char**argv){ DrawCubeWiresV(v3_arg(argv,0),v3_arg(argv,3),ci(argv,6)); return 0.0; }
static double fn_DrawSphere(int argc,const char**argv){ DrawSphere(v3_arg(argv,0),ff(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawSphereEx(int argc,const char**argv){ DrawSphereEx(v3_arg(argv,0),ff(argv[3]),ii(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawSphereWires(int argc,const char**argv){ DrawSphereWires(v3_arg(argv,0),ff(argv[3]),ii(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawCylinder(int argc,const char**argv){ DrawCylinder(v3_arg(argv,0),ff(argv[3]),ff(argv[4]),ff(argv[5]),ii(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawCylinderEx(int argc,const char**argv){ DrawCylinderEx(v3_arg(argv,0),v3_arg(argv,3),ff(argv[6]),ff(argv[7]),ii(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawCylinderWires(int argc,const char**argv){ DrawCylinderWires(v3_arg(argv,0),ff(argv[3]),ff(argv[4]),ff(argv[5]),ii(argv[6]),ci(argv,7)); return 0.0; }
static double fn_DrawCylinderWiresEx(int argc,const char**argv){ DrawCylinderWiresEx(v3_arg(argv,0),v3_arg(argv,3),ff(argv[6]),ff(argv[7]),ii(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawCapsule(int argc,const char**argv){ DrawCapsule(v3_arg(argv,0),v3_arg(argv,3),ff(argv[6]),ii(argv[7]),ii(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawCapsuleWires(int argc,const char**argv){ DrawCapsuleWires(v3_arg(argv,0),v3_arg(argv,3),ff(argv[6]),ii(argv[7]),ii(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawPlane(int argc,const char**argv){ DrawPlane(v3_arg(argv,0),v2_arg(argv,3),ci(argv,5)); return 0.0; }
static double fn_DrawRay(int argc,const char**argv){ DrawRay(g_last_ray,ci(argv,0)); return 0.0; }
static double fn_DrawGrid(int argc,const char**argv){ DrawGrid(ii(argv[0]),ff(argv[1])); return 0.0; }

static double fn_LoadModel(int argc,const char**argv){ g_last_handle=reg_model(LoadModel(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadModelFromMesh(int argc,const char**argv){ g_last_handle=reg_model(LoadModelFromMesh(get_mesh(ii(argv[0])))); return (double)g_last_handle; }
static double fn_IsModelValid(int argc,const char**argv){ return IsModelValid(get_model(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadModel(int argc,const char**argv){ UnloadModel(get_model(ii(argv[0]))); return 0.0; }
static double fn_GetModelBoundingBox(int argc,const char**argv){ g_last_bb=GetModelBoundingBox(get_model(ii(argv[0]))); return 0.0; }
static double fn_DrawModel(int argc,const char**argv){ DrawModel(get_model(ii(argv[0])),v3_arg(argv,1),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawModelEx(int argc,const char**argv){ DrawModelEx(get_model(ii(argv[0])),v3_arg(argv,1),v3_arg(argv,4),ff(argv[7]),v3_arg(argv,8),ci(argv,11)); return 0.0; }
static double fn_DrawModelWires(int argc,const char**argv){ DrawModelWires(get_model(ii(argv[0])),v3_arg(argv,1),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawModelWiresEx(int argc,const char**argv){ DrawModelWiresEx(get_model(ii(argv[0])),v3_arg(argv,1),v3_arg(argv,4),ff(argv[7]),v3_arg(argv,8),ci(argv,11)); return 0.0; }
static double fn_DrawBoundingBox(int argc,const char**argv){ DrawBoundingBox(g_last_bb,ci(argv,0)); return 0.0; }
static double fn_DrawBillboard(int argc,const char**argv){ DrawBillboard(g_last_cam,get_tex(ii(argv[0])),v3_arg(argv,1),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawBillboardRec(int argc,const char**argv){ DrawBillboardRec(g_last_cam,get_tex(ii(argv[0])),rect_arg(argv,1),v3_arg(argv,5),v2_arg(argv,8),ci(argv,10)); return 0.0; }
static double fn_DrawBillboardPro(int argc,const char**argv){ DrawBillboardPro(g_last_cam,get_tex(ii(argv[0])),rect_arg(argv,1),v3_arg(argv,5),v3_arg(argv,8),v2_arg(argv,11),v2_arg(argv,13),ff(argv[15]),ci(argv,16)); return 0.0; }

static double fn_UploadMesh(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UpdateMeshBuffer(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadMesh(int argc,const char**argv){ UnloadMesh(get_mesh(ii(argv[0]))); return 0.0; }
static double fn_DrawMesh(int argc,const char**argv){ DrawMesh(get_mesh(ii(argv[0])),get_mat(ii(argv[1])),g_last_mat); return 0.0; }
static double fn_DrawMeshInstanced(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_GetMeshBoundingBox(int argc,const char**argv){ g_last_bb=GetMeshBoundingBox(get_mesh(ii(argv[0]))); return 0.0; }
static double fn_GenMeshTangents(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_ExportMesh(int argc,const char**argv){ return ExportMesh(get_mesh(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_ExportMeshAsCode(int argc,const char**argv){ return ExportMeshAsCode(get_mesh(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_GenMeshPoly(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshPoly(ii(argv[0]),ff(argv[1]))); return (double)g_last_handle; }
static double fn_GenMeshPlane(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshPlane(ff(argv[0]),ff(argv[1]),ii(argv[2]),ii(argv[3]))); return (double)g_last_handle; }
static double fn_GenMeshCube(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshCube(ff(argv[0]),ff(argv[1]),ff(argv[2]))); return (double)g_last_handle; }
static double fn_GenMeshSphere(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshSphere(ff(argv[0]),ii(argv[1]),ii(argv[2]))); return (double)g_last_handle; }
static double fn_GenMeshHemiSphere(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshHemiSphere(ff(argv[0]),ii(argv[1]),ii(argv[2]))); return (double)g_last_handle; }
static double fn_GenMeshCylinder(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshCylinder(ff(argv[0]),ff(argv[1]),ii(argv[2]))); return (double)g_last_handle; }
static double fn_GenMeshCone(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshCone(ff(argv[0]),ff(argv[1]),ii(argv[2]))); return (double)g_last_handle; }
static double fn_GenMeshTorus(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshTorus(ff(argv[0]),ff(argv[1]),ii(argv[2]),ii(argv[3]))); return (double)g_last_handle; }
static double fn_GenMeshKnot(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshKnot(ff(argv[0]),ff(argv[1]),ii(argv[2]),ii(argv[3]))); return (double)g_last_handle; }
static double fn_GenMeshHeightmap(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshHeightmap(get_img(ii(argv[0])),v3_arg(argv,1))); return (double)g_last_handle; }
static double fn_GenMeshCubicmap(int argc,const char**argv){ g_last_handle=reg_mesh(GenMeshCubicmap(get_img(ii(argv[0])),v3_arg(argv,1))); return (double)g_last_handle; }

static double fn_LoadMaterials(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadMaterialDefault(int argc,const char**argv){ g_last_handle=reg_mat(LoadMaterialDefault()); return (double)g_last_handle; }
static double fn_IsMaterialValid(int argc,const char**argv){ return IsMaterialValid(get_mat(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadMaterial(int argc,const char**argv){ UnloadMaterial(get_mat(ii(argv[0]))); return 0.0; }
static double fn_SetMaterialTexture(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_SetModelMeshMaterial(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadModelAnimations(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UpdateModelAnimation(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UpdateModelAnimationEx(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadModelAnimations(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_IsModelAnimationValid(int argc,const char**argv){ return IsModelAnimationValid(get_model(ii(argv[0])),(ModelAnimation){0})?1.0:0.0; }

static double fn_CheckCollisionSpheres(int argc,const char**argv){ return CheckCollisionSpheres(v3_arg(argv,0),ff(argv[3]),v3_arg(argv,4),ff(argv[7]))?1.0:0.0; }
static double fn_CheckCollisionBoxes(int argc,const char**argv){ return CheckCollisionBoxes(g_last_bb,g_last_bb)?1.0:0.0; }
static double fn_CheckCollisionBoxSphere(int argc,const char**argv){ return CheckCollisionBoxSphere(g_last_bb,v3_arg(argv,0),ff(argv[3]))?1.0:0.0; }
static double fn_GetRayCollisionSphere(int argc,const char**argv){ g_last_raycol=GetRayCollisionSphere(g_last_ray,v3_arg(argv,0),ff(argv[3])); return 0.0; }
static double fn_GetRayCollisionBox(int argc,const char**argv){ g_last_raycol=GetRayCollisionBox(g_last_ray,g_last_bb); return 0.0; }
static double fn_GetRayCollisionMesh(int argc,const char**argv){ g_last_raycol=GetRayCollisionMesh(g_last_ray,get_mesh(ii(argv[0])),g_last_mat); return 0.0; }
static double fn_GetRayCollisionTriangle(int argc,const char**argv){ g_last_raycol=GetRayCollisionTriangle(g_last_ray,v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6)); return 0.0; }
static double fn_GetRayCollisionQuad(int argc,const char**argv){ g_last_raycol=GetRayCollisionQuad(g_last_ray,v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),v3_arg(argv,9)); return 0.0; }

/* =========================================================================
   AUDIO
   ========================================================================= */
static double fn_InitAudioDevice(int argc,const char**argv){(void)argc;(void)argv;InitAudioDevice();return 0.0;}
static double fn_CloseAudioDevice(int argc,const char**argv){(void)argc;(void)argv;CloseAudioDevice();return 0.0;}
static double fn_IsAudioDeviceReady(int argc,const char**argv){(void)argc;(void)argv;return IsAudioDeviceReady()?1.0:0.0;}
static double fn_SetMasterVolume(int argc,const char**argv){ SetMasterVolume(ff(argv[0])); return 0.0; }
static double fn_GetMasterVolume(int argc,const char**argv){(void)argc;(void)argv;return (double)GetMasterVolume();}
static double fn_LoadWave(int argc,const char**argv){ g_last_handle=reg_wave(LoadWave(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadWaveFromMemory(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_wave((Wave){0});}
static double fn_IsWaveValid(int argc,const char**argv){ return IsWaveValid(get_wave(ii(argv[0])))?1.0:0.0; }
static double fn_LoadSound(int argc,const char**argv){ g_last_handle=reg_snd(LoadSound(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadSoundFromWave(int argc,const char**argv){ g_last_handle=reg_snd(LoadSoundFromWave(get_wave(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadSoundAlias(int argc,const char**argv){ g_last_handle=reg_snd(LoadSoundAlias(get_snd(ii(argv[0])))); return (double)g_last_handle; }
static double fn_IsSoundValid(int argc,const char**argv){ return IsSoundValid(get_snd(ii(argv[0])))?1.0:0.0; }
static double fn_UpdateSound(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadWave(int argc,const char**argv){ UnloadWave(get_wave(ii(argv[0]))); return 0.0; }
static double fn_UnloadSound(int argc,const char**argv){ UnloadSound(get_snd(ii(argv[0]))); return 0.0; }
static double fn_UnloadSoundAlias(int argc,const char**argv){ UnloadSoundAlias(get_snd(ii(argv[0]))); return 0.0; }
static double fn_ExportWave(int argc,const char**argv){ return ExportWave(get_wave(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_ExportWaveAsCode(int argc,const char**argv){ return ExportWaveAsCode(get_wave(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_PlaySound(int argc,const char**argv){ PlaySound(get_snd(ii(argv[0]))); return 0.0; }
static double fn_StopSound(int argc,const char**argv){ StopSound(get_snd(ii(argv[0]))); return 0.0; }
static double fn_PauseSound(int argc,const char**argv){ PauseSound(get_snd(ii(argv[0]))); return 0.0; }
static double fn_ResumeSound(int argc,const char**argv){ ResumeSound(get_snd(ii(argv[0]))); return 0.0; }
static double fn_IsSoundPlaying(int argc,const char**argv){ return IsSoundPlaying(get_snd(ii(argv[0])))?1.0:0.0; }
static double fn_SetSoundVolume(int argc,const char**argv){ SetSoundVolume(get_snd(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetSoundPitch(int argc,const char**argv){ SetSoundPitch(get_snd(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetSoundPan(int argc,const char**argv){ SetSoundPan(get_snd(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_WaveCopy(int argc,const char**argv){ g_last_handle=reg_wave(WaveCopy(get_wave(ii(argv[0])))); return (double)g_last_handle; }
static double fn_WaveCrop(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_WaveFormat(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadWaveSamples(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_UnloadWaveSamples(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_LoadMusicStream(int argc,const char**argv){ g_last_handle=reg_mus(LoadMusicStream(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadMusicStreamFromMemory(int argc,const char**argv){(void)argc;(void)argv;return (double)reg_mus((Music){0});}
static double fn_IsMusicValid(int argc,const char**argv){ return IsMusicValid(get_mus(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadMusicStream(int argc,const char**argv){ UnloadMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_PlayMusicStream(int argc,const char**argv){ PlayMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_IsMusicStreamPlaying(int argc,const char**argv){ return IsMusicStreamPlaying(get_mus(ii(argv[0])))?1.0:0.0; }
static double fn_UpdateMusicStream(int argc,const char**argv){ UpdateMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_StopMusicStream(int argc,const char**argv){ StopMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_PauseMusicStream(int argc,const char**argv){ PauseMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_ResumeMusicStream(int argc,const char**argv){ ResumeMusicStream(get_mus(ii(argv[0]))); return 0.0; }
static double fn_SeekMusicStream(int argc,const char**argv){ SeekMusicStream(get_mus(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetMusicVolume(int argc,const char**argv){ SetMusicVolume(get_mus(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetMusicPitch(int argc,const char**argv){ SetMusicPitch(get_mus(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetMusicPan(int argc,const char**argv){ SetMusicPan(get_mus(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_GetMusicTimeLength(int argc,const char**argv){ return (double)GetMusicTimeLength(get_mus(ii(argv[0]))); }
static double fn_GetMusicTimePlayed(int argc,const char**argv){ return (double)GetMusicTimePlayed(get_mus(ii(argv[0]))); }
static double fn_LoadAudioStream(int argc,const char**argv){ g_last_handle=reg_ast(LoadAudioStream((unsigned int)strtoul(argv[0],NULL,10),(unsigned int)strtoul(argv[1],NULL,10),(unsigned int)strtoul(argv[2],NULL,10))); return (double)g_last_handle; }
static double fn_IsAudioStreamValid(int argc,const char**argv){ return IsAudioStreamValid(get_ast(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadAudioStream(int argc,const char**argv){ UnloadAudioStream(get_ast(ii(argv[0]))); return 0.0; }
static double fn_UpdateAudioStream(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_IsAudioStreamProcessed(int argc,const char**argv){ return IsAudioStreamProcessed(get_ast(ii(argv[0])))?1.0:0.0; }
static double fn_PlayAudioStream(int argc,const char**argv){ PlayAudioStream(get_ast(ii(argv[0]))); return 0.0; }
static double fn_PauseAudioStream(int argc,const char**argv){ PauseAudioStream(get_ast(ii(argv[0]))); return 0.0; }
static double fn_ResumeAudioStream(int argc,const char**argv){ ResumeAudioStream(get_ast(ii(argv[0]))); return 0.0; }
static double fn_IsAudioStreamPlaying(int argc,const char**argv){ return IsAudioStreamPlaying(get_ast(ii(argv[0])))?1.0:0.0; }
static double fn_StopAudioStream(int argc,const char**argv){ StopAudioStream(get_ast(ii(argv[0]))); return 0.0; }
static double fn_SetAudioStreamVolume(int argc,const char**argv){ SetAudioStreamVolume(get_ast(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetAudioStreamPitch(int argc,const char**argv){ SetAudioStreamPitch(get_ast(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetAudioStreamPan(int argc,const char**argv){ SetAudioStreamPan(get_ast(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_SetAudioStreamBufferSizeDefault(int argc,const char**argv){ SetAudioStreamBufferSizeDefault(ii(argv[0])); return 0.0; }
static double fn_SetAudioStreamCallback(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_AttachAudioStreamProcessor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DetachAudioStreamProcessor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_AttachAudioMixedProcessor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}
static double fn_DetachAudioMixedProcessor(int argc,const char**argv){(void)argc;(void)argv;return 0.0;}

/* =========================================================================
   Fonksiyon tablosu
   ========================================================================= */
#define E(NAME) {#NAME, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    /* Colors */
    E(RAYWHITE),E(LIGHTGRAY),E(GRAY),E(DARKGRAY),E(YELLOW),E(GOLD),E(ORANGE),E(PINK),E(RED),E(MAROON),
    E(GREEN),E(LIME),E(DARKGREEN),E(SKYBLUE),E(BLUE),E(DARKBLUE),E(PURPLE),E(VIOLET),E(DARKPURPLE),E(BEIGE),
    E(BROWN),E(DARKBROWN),E(WHITE),E(BLACK),E(BLANK),E(MAGENTA),

    /* Window / Core */
    E(InitWindow),E(CloseWindow),E(WindowShouldClose),E(IsWindowReady),E(IsWindowFullscreen),E(IsWindowHidden),
    E(IsWindowMinimized),E(IsWindowMaximized),E(IsWindowFocused),E(IsWindowResized),E(IsWindowState),
    E(SetWindowState),E(ClearWindowState),E(ToggleFullscreen),E(ToggleBorderlessWindowed),E(MaximizeWindow),
    E(MinimizeWindow),E(RestoreWindow),E(SetWindowIcon),E(SetWindowIcons),E(SetWindowTitle),E(SetWindowPosition),
    E(SetWindowMonitor),E(SetWindowMinSize),E(SetWindowMaxSize),E(SetWindowSize),E(SetWindowOpacity),
    E(SetWindowFocused),E(GetWindowHandle),E(GetScreenWidth),E(GetScreenHeight),E(GetRenderWidth),E(GetRenderHeight),
    E(GetMonitorCount),E(GetCurrentMonitor),E(GetMonitorPosition),E(GetMonitorWidth),E(GetMonitorHeight),
    E(GetMonitorPhysicalWidth),E(GetMonitorPhysicalHeight),E(GetMonitorRefreshRate),E(GetWindowPosition),
    E(GetWindowScaleDPI),E(GetMonitorName),E(SetClipboardText),E(GetClipboardText),E(GetClipboardImage),
    E(EnableEventWaiting),E(DisableEventWaiting),

    /* Cursor */
    E(ShowCursor),E(HideCursor),E(IsCursorHidden),E(EnableCursor),E(DisableCursor),E(IsCursorOnScreen),

    /* Drawing modes */
    E(ClearBackground),E(BeginDrawing),E(EndDrawing),E(BeginMode2D),E(EndMode2D),E(BeginMode3D),E(EndMode3D),
    E(BeginTextureMode),E(EndTextureMode),E(BeginShaderMode),E(EndShaderMode),E(BeginBlendMode),E(EndBlendMode),
    E(BeginScissorMode),E(EndScissorMode),E(BeginVrStereoMode),E(EndVrStereoMode),

    /* VR */
    E(LoadVrStereoConfig),E(UnloadVrStereoConfig),

    /* Shader */
    E(LoadShader),E(LoadShaderFromMemory),E(IsShaderValid),E(GetShaderLocation),E(GetShaderLocationAttrib),
    E(SetShaderValue),E(SetShaderValueV),E(SetShaderValueMatrix),E(SetShaderValueTexture),E(UnloadShader),

    /* Screen space */
    E(GetScreenToWorldRay),E(GetScreenToWorldRayEx),E(GetWorldToScreen),E(GetWorldToScreenEx),E(GetWorldToScreen2D),
    E(GetScreenToWorld2D),E(GetCameraMatrix),E(GetCameraMatrix2D),

    /* Timing */
    E(SetTargetFPS),E(GetFrameTime),E(GetTime),E(GetFPS),E(SwapScreenBuffer),E(PollInputEvents),E(WaitTime),
    E(SetRandomSeed),E(GetRandomValue),E(LoadRandomSequence),E(UnloadRandomSequence),E(TakeScreenshot),
    E(SetConfigFlags),E(OpenURL),E(SetTraceLogLevel),E(TraceLog),E(SetTraceLogCallback),E(MemAlloc),E(MemRealloc),E(MemFree),

    /* File system */
    E(LoadFileData),E(UnloadFileData),E(SaveFileData),E(ExportDataAsCode),E(LoadFileText),E(UnloadFileText),
    E(SaveFileText),E(SetLoadFileDataCallback),E(SetSaveFileDataCallback),E(SetLoadFileTextCallback),E(SetSaveFileTextCallback),
    E(FileRename),E(FileRemove),E(FileCopy),E(FileMove),E(FileTextReplace),E(FileTextFindIndex),E(FileExists),
    E(DirectoryExists),E(IsFileExtension),E(GetFileLength),E(GetFileModTime),E(GetFileExtension),E(GetFileName),
    E(GetFileNameWithoutExt),E(GetDirectoryPath),E(GetPrevDirectoryPath),E(GetWorkingDirectory),E(GetApplicationDirectory),
    E(MakeDirectory),E(ChangeDirectory),E(IsPathFile),E(IsPathDirectory),E(IsPathAbsolute),E(IsFileNameValid),
    E(LoadDirectoryFiles),E(LoadDirectoryFilesEx),E(UnloadDirectoryFiles),E(IsFileDropped),E(LoadDroppedFiles),
    E(UnloadDroppedFiles),E(GetDirectoryFileCount),E(GetDirectoryFileCountEx),E(CompressData),E(DecompressData),
    E(EncodeDataBase64),E(DecodeDataBase64),E(ComputeCRC32),E(ComputeMD5),E(ComputeSHA1),E(ComputeSHA256),
    E(LoadAutomationEventList),E(UnloadAutomationEventList),E(ExportAutomationEventList),E(SetAutomationEventList),
    E(SetAutomationEventBaseFrame),E(StartAutomationEventRecording),E(StopAutomationEventRecording),E(PlayAutomationEvent),

    /* Input */
    E(CAMERA_FREE),E(CAMERA_ORBITAL),E(CAMERA_FIRST_PERSON),E(CAMERA_THIRD_PERSON),
    E(CAMERA_PERSPECTIVE),E(CAMERA_ORTHOGRAPHIC),E(KEY_Z),E(KEY_ESCAPE),E(KEY_SPACE),E(KEY_W),E(KEY_A),
    E(KEY_S),E(KEY_D),E(MOUSE_BUTTON_LEFT),E(MOUSE_BUTTON_RIGHT),E(MOUSE_BUTTON_MIDDLE),
    E(IsKeyPressed),E(IsKeyPressedRepeat),E(IsKeyDown),E(IsKeyReleased),E(IsKeyUp),E(GetKeyPressed),E(GetCharPressed),
    E(GetKeyName),E(SetExitKey),
    E(IsGamepadAvailable),E(GetGamepadName),E(IsGamepadButtonPressed),E(IsGamepadButtonDown),E(IsGamepadButtonReleased),
    E(IsGamepadButtonUp),E(GetGamepadButtonPressed),E(GetGamepadAxisCount),E(GetGamepadAxisMovement),E(SetGamepadMappings),
    E(SetGamepadVibration),
    E(IsMouseButtonPressed),E(IsMouseButtonDown),E(IsMouseButtonReleased),E(IsMouseButtonUp),E(GetMouseX),E(GetMouseY),
    E(GetMousePosition),E(GetMouseDelta),E(SetMousePosition),E(SetMouseOffset),E(SetMouseScale),E(GetMouseWheelMove),
    E(GetMouseWheelMoveV),E(SetMouseCursor),E(GetTouchX),E(GetTouchY),E(GetTouchPosition),E(GetTouchPointId),E(GetTouchPointCount),

    /* Gestures */
    E(SetGesturesEnabled),E(IsGestureDetected),E(GetGestureDetected),E(GetGestureHoldDuration),E(GetGestureDragVector),
    E(GetGestureDragAngle),E(GetGesturePinchVector),E(GetGesturePinchAngle),

    /* Camera */
    E(UpdateCamera),E(UpdateCameraPro),

    /* Shapes 2D */
    E(SetShapesTexture),E(GetShapesTexture),E(GetShapesTextureRectangle),
    E(DrawPixel),E(DrawPixelV),E(DrawLine),E(DrawLineV),E(DrawLineEx),E(DrawLineStrip),E(DrawLineBezier),E(DrawLineDashed),
    E(DrawTriangle),E(DrawTriangleLines),E(DrawTriangleFan),E(DrawTriangleStrip),
    E(DrawRectangle),E(DrawRectangleV),E(DrawRectangleRec),E(DrawRectanglePro),E(DrawRectangleGradientV),E(DrawRectangleGradientH),
    E(DrawRectangleGradientEx),E(DrawRectangleLines),E(DrawRectangleLinesEx),E(DrawRectangleRounded),E(DrawRectangleRoundedLines),
    E(DrawRectangleRoundedLinesEx),E(DrawPoly),E(DrawPolyLines),E(DrawPolyLinesEx),E(DrawCircle),E(DrawCircleV),E(DrawCircleGradient),
    E(DrawCircleSector),E(DrawCircleSectorLines),E(DrawCircleSectorLinesEx),E(DrawCircleLines),E(DrawCircleLinesV),E(DrawCircleLinesEx),
    E(DrawEllipse),E(DrawEllipseV),E(DrawEllipseLines),E(DrawEllipseLinesV),E(DrawEllipseLinesEx),E(DrawRing),E(DrawRingLines),E(DrawRingLinesEx),
    E(DrawSplineLinear),E(DrawSplineBasis),E(DrawSplineCatmullRom),E(DrawSplineBezierQuadratic),E(DrawSplineBezierCubic),
    E(DrawSplineSegmentLinear),E(DrawSplineSegmentBasis),E(DrawSplineSegmentCatmullRom),E(DrawSplineSegmentBezierQuadratic),
    E(DrawSplineSegmentBezierCubic),E(GetSplinePointLinear),E(GetSplinePointBasis),E(GetSplinePointCatmullRom),
    E(GetSplinePointBezierQuadratic),E(GetSplinePointBezierCubic),
    E(CheckCollisionRecs),E(CheckCollisionCircles),E(CheckCollisionCircleRec),E(CheckCollisionCircleLine),E(CheckCollisionPointRec),
    E(CheckCollisionPointCircle),E(CheckCollisionPointTriangle),E(CheckCollisionPointLine),E(CheckCollisionPointPoly),
    E(CheckCollisionLines),E(GetCollisionRec),

    /* Textures / Images */
    E(LoadImage),E(LoadImageRaw),E(LoadImageAnim),E(LoadImageAnimFromMemory),E(LoadImageFromMemory),E(LoadImageFromTexture),
    E(LoadImageFromScreen),E(IsImageValid),E(UnloadImage),E(ExportImage),E(ExportImageToMemory),E(ExportImageAsCode),
    E(GenImageColor),E(GenImageGradientLinear),E(GenImageGradientRadial),E(GenImageGradientSquare),E(GenImageChecked),
    E(GenImageWhiteNoise),E(GenImagePerlinNoise),E(GenImageCellular),E(GenImageText),
    E(ImageCopy),E(ImageFromImage),E(ImageFromChannel),E(ImageText),E(ImageTextEx),E(ImageFormat),E(ImageToPOT),E(ImageCrop),
    E(ImageAlphaCrop),E(ImageAlphaClear),E(ImageAlphaMask),E(ImageAlphaPremultiply),E(ImageBlurGaussian),E(ImageKernelConvolution),
    E(ImageResize),E(ImageResizeNN),E(ImageResizeCanvas),E(ImageMipmaps),E(ImageDither),E(ImageFlipVertical),E(ImageFlipHorizontal),
    E(ImageRotate),E(ImageRotateCW),E(ImageRotateCCW),E(ImageColorTint),E(ImageColorInvert),E(ImageColorGrayscale),
    E(ImageColorContrast),E(ImageColorBrightness),E(ImageColorReplace),E(LoadImageColors),E(LoadImagePalette),E(UnloadImageColors),
    E(UnloadImagePalette),E(GetImageAlphaBorder),E(GetImageColor),
    E(ImageClearBackground),E(ImageDrawPixel),E(ImageDrawPixelV),E(ImageDrawLine),E(ImageDrawLineV),E(ImageDrawLineEx),
    E(ImageDrawLineStrip),E(ImageDrawTriangle),E(ImageDrawTriangleGradient),E(ImageDrawTriangleLines),E(ImageDrawTriangleFan),
    E(ImageDrawTriangleStrip),E(ImageDrawRectangle),E(ImageDrawRectangleV),E(ImageDrawRectangleRec),E(ImageDrawRectanglePro),
    E(ImageDrawRectangleLines),E(ImageDrawRectangleLinesEx),E(ImageDrawRectangleGradientEx),E(ImageDrawCircle),E(ImageDrawCircleV),
    E(ImageDrawCircleLines),E(ImageDrawCircleLinesV),E(ImageDrawCircleGradient),E(ImageDrawImage),E(ImageDrawImageEx),
    E(ImageDrawImageRec),E(ImageDrawImagePro),E(ImageDrawText),E(ImageDrawTextEx),E(ImageDrawTextPro),
    E(LoadTexture),E(LoadTextureFromImage),E(LoadTextureCubemap),E(LoadRenderTexture),E(IsTextureValid),E(UnloadTexture),
    E(IsRenderTextureValid),E(UnloadRenderTexture),E(UpdateTexture),E(UpdateTextureRec),E(GenTextureMipmaps),E(SetTextureFilter),
    E(SetTextureWrap),E(DrawTexture),E(DrawTextureV),E(DrawTextureEx),E(DrawTextureRec),E(DrawTexturePro),E(DrawTextureNPatch),

    /* Text / Font */
    E(GetFontDefault),E(LoadFont),E(LoadFontEx),E(LoadFontFromImage),E(LoadFontFromMemory),E(IsFontValid),E(LoadFontData),
    E(GenImageFontAtlas),E(UnloadFontData),E(UnloadFont),E(ExportFontAsCode),
    E(DrawFPS),E(DrawText),E(DrawTextEx),E(DrawTextPro),E(DrawTextCodepoint),E(DrawTextCodepoints),E(SetTextLineSpacing),
    E(MeasureText),E(MeasureTextEx),E(MeasureTextCodepoints),E(GetGlyphIndex),E(GetGlyphInfo),E(GetGlyphAtlasRec),
    E(LoadUTF8),E(UnloadUTF8),E(LoadCodepoints),E(UnloadCodepoints),E(GetCodepointCount),E(GetCodepoint),E(GetCodepointNext),
    E(GetCodepointPrevious),E(CodepointToUTF8),E(LoadTextLines),E(UnloadTextLines),E(TextCopy),E(TextIsEqual),E(TextLength),
    E(TextFormat),E(TextSubtext),E(TextRemoveSpaces),E(GetTextBetween),E(TextReplace),E(TextReplaceAlloc),E(TextReplaceBetween),
    E(TextReplaceBetweenAlloc),E(TextInsert),E(TextInsertAlloc),E(TextJoin),E(TextSplit),E(TextAppend),E(TextFindIndex),
    E(TextToUpper),E(TextToLower),E(TextToPascal),E(TextToSnake),E(TextToCamel),E(TextToInteger),E(TextToFloat),

    /* 3D */
    E(DrawLine3D),E(DrawPoint3D),E(DrawCircle3D),E(DrawTriangle3D),E(DrawTriangleStrip3D),E(DrawCube),E(DrawCubeV),
    E(DrawCubeWires),E(DrawCubeWiresV),E(DrawSphere),E(DrawSphereEx),E(DrawSphereWires),E(DrawCylinder),E(DrawCylinderEx),
    E(DrawCylinderWires),E(DrawCylinderWiresEx),E(DrawCapsule),E(DrawCapsuleWires),E(DrawPlane),E(DrawRay),E(DrawGrid),
    E(LoadModel),E(LoadModelFromMesh),E(IsModelValid),E(UnloadModel),E(GetModelBoundingBox),E(DrawModel),E(DrawModelEx),
    E(DrawModelWires),E(DrawModelWiresEx),E(DrawBoundingBox),E(DrawBillboard),E(DrawBillboardRec),E(DrawBillboardPro),
    E(UploadMesh),E(UpdateMeshBuffer),E(UnloadMesh),E(DrawMesh),E(DrawMeshInstanced),E(GetMeshBoundingBox),E(GenMeshTangents),
    E(ExportMesh),E(ExportMeshAsCode),E(GenMeshPoly),E(GenMeshPlane),E(GenMeshCube),E(GenMeshSphere),E(GenMeshHemiSphere),
    E(GenMeshCylinder),E(GenMeshCone),E(GenMeshTorus),E(GenMeshKnot),E(GenMeshHeightmap),E(GenMeshCubicmap),
    E(LoadMaterials),E(LoadMaterialDefault),E(IsMaterialValid),E(UnloadMaterial),E(SetMaterialTexture),E(SetModelMeshMaterial),
    E(LoadModelAnimations),E(UpdateModelAnimation),E(UpdateModelAnimationEx),E(UnloadModelAnimations),E(IsModelAnimationValid),
    E(CheckCollisionSpheres),E(CheckCollisionBoxes),E(CheckCollisionBoxSphere),E(GetRayCollisionSphere),E(GetRayCollisionBox),
    E(GetRayCollisionMesh),E(GetRayCollisionTriangle),E(GetRayCollisionQuad),

    /* Audio */
    E(InitAudioDevice),E(CloseAudioDevice),E(IsAudioDeviceReady),E(SetMasterVolume),E(GetMasterVolume),E(LoadWave),
    E(LoadWaveFromMemory),E(IsWaveValid),E(LoadSound),E(LoadSoundFromWave),E(LoadSoundAlias),E(IsSoundValid),E(UpdateSound),
    E(UnloadWave),E(UnloadSound),E(UnloadSoundAlias),E(ExportWave),E(ExportWaveAsCode),E(PlaySound),E(StopSound),E(PauseSound),
    E(ResumeSound),E(IsSoundPlaying),E(SetSoundVolume),E(SetSoundPitch),E(SetSoundPan),E(WaveCopy),E(WaveCrop),E(WaveFormat),
    E(LoadWaveSamples),E(UnloadWaveSamples),E(LoadMusicStream),E(LoadMusicStreamFromMemory),E(IsMusicValid),E(UnloadMusicStream),
    E(PlayMusicStream),E(IsMusicStreamPlaying),E(UpdateMusicStream),E(StopMusicStream),E(PauseMusicStream),E(ResumeMusicStream),
    E(SeekMusicStream),E(SetMusicVolume),E(SetMusicPitch),E(SetMusicPan),E(GetMusicTimeLength),E(GetMusicTimePlayed),
    E(LoadAudioStream),E(IsAudioStreamValid),E(UnloadAudioStream),E(UpdateAudioStream),E(IsAudioStreamProcessed),E(PlayAudioStream),
    E(PauseAudioStream),E(ResumeAudioStream),E(IsAudioStreamPlaying),E(StopAudioStream),E(SetAudioStreamVolume),E(SetAudioStreamPitch),
    E(SetAudioStreamPan),E(SetAudioStreamBufferSizeDefault),E(SetAudioStreamCallback),E(AttachAudioStreamProcessor),
    E(DetachAudioStreamProcessor),E(AttachAudioMixedProcessor),E(DetachAudioMixedProcessor),

    /* Color helpers */
    E(Fade),E(ColorToInt),E(ColorNormalize),E(ColorFromNormalized),E(ColorToHSV),E(ColorFromHSV),E(ColorTint),E(ColorBrightness),
    E(ColorContrast),E(ColorAlpha),E(ColorAlphaBlend),E(ColorLerp),E(GetColor),E(GetPixelColor),E(SetPixelColor),
    E(GetPixelDataSize),E(ColorIsEqual),

    /* Shapes constructors */
    E(Rectangle),E(Vector2),E(Vector3),E(Vector4),E(Matrix),E(Camera),E(Camera2D),E(Ray),E(BoundingBox),
    E(NPatchInfo),E(GlyphInfo),E(VrStereoConfig),E(AutomationEvent),
};

GCL_EXPORT const GclNativeEntry *gcl_raylib_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
