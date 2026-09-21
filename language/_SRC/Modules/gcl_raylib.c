/*
 * gcl_raylib.c — GCL Raylib modülü (.dll/.so) — TAM wrapper.
 *
 * #native <Raylib> ile yüklenir; Raylib.* çağrıları raylib fonksiyonlarına bağlanır.
 *
 * GCL modül sistemi: fn(int argc, const char **argv) -> double.
 *   - Sayısal parametreler atof/strtol ile çevrilir.
 *   - Renkler 32-bit packed uint olarak taşınır (R | G<<8 | B<<16 | A<<24).
 *   - Struct dönen fonksiyonlar g_* global'lerine yazar; dönüş handle (int) olarak verilir.
 *   - String dönen fonksiyonlar metni g_str global'ine yazar. KURAL: dönüş
 *     değeri 0'dır, metnin uzunluğunu çağıran `LastStringLen()` ile alır
 *     (bkz. language/readme.md "string" satırı).
 *     İSTİSNA: sonucu metnin KENDİSİ olan ve uzunluğu doğrudan anlamlı olan
 *     iki üye bayt sayısını döner — `LoadFileText` ve `LoadUTF8`. Dönen sayı
 *     `LastStringLen()` ile AYNI olmak zorundadır; böylece 0 "boş/başarısız",
 *     >0 "şu kadar bayt geldi" demektir ve ikisi karışmaz.
 *     `LoadFileData` ayrıca HAM bayt sayısını döner: yuva hex kodlanmıştır,
 *     bu yüzden `strlen(g_str)` o sayıyı VEREMEZ (size=3 ama baytlar "123").
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

/* ---------- Animasyon / glyph-set kayıtları ----------
   raylib bu API'lerde DİZİ döndürür (ModelAnimation *, GlyphInfo *); GCL'de
   dizi değeri yoktur. Modül diziyi kendi tutar, GCL'e taban indeks verir ve
   öğe sayısını g_last_int'te yayınlar (GCL: Raylib.LastInt()).
   UpdateModelAnimation(Ex) / GenImageFontAtlas / UnloadModelAnimations bu
   indekslerle çalışır, böylece animasyon ve font-atlas zinciri kullanılabilir. */
#define GCL_MAX_ANIMS 128
static ModelAnimation g_anim_h[GCL_MAX_ANIMS];
static int            g_anim_n     = 0;   /* kullanılan toplam slot */
static int            g_anim_set[GCL_MAX_ANIMS];   /* slotun ait olduğu yükleme */
static int            g_anim_live[GCL_MAX_ANIMS];  /* 0: boş, 1: geçerli */
static int            g_anim_sets  = 0;   /* kaç kez LoadModelAnimations çağrıldı */

#define GCL_MAX_GLYPH_SETS 16
static GlyphInfo *g_glyphset[GCL_MAX_GLYPH_SETS]      = {0};
static int        g_glyphset_count[GCL_MAX_GLYPH_SETS] = {0};
static Rectangle *g_glyphset_recs[GCL_MAX_GLYPH_SETS]  = {0};

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
/* ---------- Varlık (asset) yolu çözümü ----------

   Bir proje resim/model/font dosyalarını TEK bir varlık dizininde tutar
   (project.gcdata: "raylib_asset_directory_path", varsayılan "assets") ve
   script yüklemeyi KISA yazar:

       Raylib.LoadTexture("terrain.png");     // "assets/terrain.png" DEĞİL

   Sürecin çalışma dizini exe/bundle'ın bulunduğu klasördür — varlık dizini
   DEĞİL. Bu yüzden kısa yazım "[terrain.png] Failed to open file" ile
   başarısız oluyor ve her yükleme BOŞ doku/model döndürüyordu: hata görünür
   ama SAHNE BOŞ kalırdı (FPS demosunda arazi hiç çizilmiyordu).

   Kural: ad zaten bir yol ise (ayraç içeriyorsa) dokunulmaz; olduğu yerde
   varsa o kullanılır; yoksa `<GCL_PROJECT_DIR>/assets/<ad>` denenir; o da
   yoksa `assets/<ad>`; hiçbiri yoksa çağıranın yazdığı ad AYNEN döner ki
   raylib'in kendi hata yolu işlesin.

   Dosya adı gibi GÖRÜNMEYEN (nokta içermeyen) metinler hiç yoklanmaz: her
   karede çizilen `Raylib.DrawText("FPS", ...)` bir dosya sistemine bakmaz. */
static const char *asset_path(const char *name) {
    static char resolved[4096];
    const char *dir;
    if (!name || !name[0]) return name ? name : "";
    if (!strchr(name, '.')) return name;                        /* metin, dosya değil */
    if (strchr(name, '/') || strchr(name, '\\')) return name;   /* zaten yol */
    if (FileExists(name)) return name;                          /* olduğu yerde */
    dir = getenv("GCL_PROJECT_DIR");
    if (dir && dir[0]) {
        snprintf(resolved, sizeof(resolved), "%s/assets/%s", dir, name);
        if (FileExists(resolved)) return resolved;
    }
    snprintf(resolved, sizeof(resolved), "assets/%s", name);
    if (FileExists(resolved)) return resolved;
    return name;
}

/* Aynı kural diğer modüller için (RaylibSimpleMesh.LoadModel): dosya adı →
   çözülmüş ad. Modül bu sembolü yoklarsa kendi dizesini kullanır. */
GCL_EXPORT const char *gcl_raylib_asset_path(const char *name) {
    return asset_path(name);
}

static const char *ss(const char *s){ return asset_path(s? s : ""); }
static Rectangle rect_arg(const char **a,int i){
    return (Rectangle){ ff(a[i]), ff(a[i+1]), ff(a[i+2]), ff(a[i+3]) };
}
static Vector2 v2_arg(const char **a,int i){ return (Vector2){ ff(a[i]), ff(a[i+1]) }; }
static Vector3 v3_arg(const char **a,int i){ return (Vector3){ ff(a[i]), ff(a[i+1]), ff(a[i+2]) }; }

/* ---------- Son üretilen değerler ---------- */
/* g_last_int: int döndüren/out-parametreli çağrıların ikinci sonucu
   (ör. GetCodepoint bayt sayısı, LoadMaterials malzeme sayısı,
   LoadImageAnim kare sayısı). GCL tarafında Raylib.LastInt(). */
static int            g_last_int=0;
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
/* Tip kurucularının scratch alanları. Aşağıdaki GCL_RAYLIB_LAST_LIST bunları
   DOĞRUDAN kullanır, bu yüzden listenin ÜSTÜNDE tanımlı olmak zorundadırlar:
   dosyanın altında tanımlıyken modül "undeclared identifier" ile derlenmiyordu. */
static Quaternion     g_last_quat={0};
static Transform      g_last_transform={0};
static ModelSkeleton  g_last_skeleton={0};
static ModelAnimation g_last_manim={0};
static VrDeviceInfo   g_last_vrdev={0};
static MaterialMap    g_last_matmap={0};
static BoneInfo       g_last_bone={0};
GCL_EXPORT Rectangle *gcl_raylib_last_rectangle(void){ return &g_last_rect; }

/* ---------- Host API — GCL'e geri çağrı köprüsü ----------

   Raylib'in bir C FONKSİYON İŞARETÇİSİ alan üyeleri (SetTraceLogCallback,
   SetLoadFile*Callback, SetAudioStreamCallback, Attach*Processor) eskiden
   no-op stub'tı: modüle yalnızca `const char **argv` geldiği için GCL
   fonksiyonu C'ye geçirilemiyordu. Artık çağrılacak GCL fonksiyonunun ADI
   argüman olarak verilir ve trambolin host->call_gcl ile gerçekten çağırır.

   İŞ PARÇACICI GÜVENLİĞİ: host->call_gcl yalnızca ANA iş parçacığından
   çağrılabilir (yorumlayıcı ortamı paylaşılan durumdur). Bu yüzden:
     • TraceLog / dosya callback'leri: raylib onları ÇAĞIRAN iş parçacığında
       koşturur (ana iş parçacığı) → doğrudan çağrı güvenlidir.
     • Ses callback'i: AYRI iş parçacığında koşar → orada ASLA call_gcl
       çağrılmaz. Örnekler bir halka tamponda üretilir (ana iş parçacığı,
       gcl_module_tick), gerçek zamanlı trambolin yalnızca tamponu boşaltır. */
static const GclHostApi *g_host = NULL;

GCL_EXPORT void gcl_module_set_host(const GclHostApi *api) { g_host = api; }

/* Çağrılacak GCL fonksiyonunun adı (boş = callback kapalı). */
static char g_trace_fn[128] = "";

static void gcl_trace_trampoline(int logLevel, const char *text, va_list args) {
    char buf[1024];
    /* raylib metni printf biçiminde verir; GCL'e TEK bir metin olarak geçir. */
    vsnprintf(buf, sizeof(buf), text ? text : "", args);
    if (!g_host || !g_host->call_gcl || !g_trace_fn[0]) {
        fprintf(stderr, "%s", buf);
        return;
    }
    GclHostArg a[2];
    a[0].is_string = 0; a[0].num = (double)logLevel; a[0].str = NULL;
    a[1].is_string = 1; a[1].num = 0.0;              a[1].str = buf;
    g_host->call_gcl(g_host->host, g_trace_fn, 2, a, NULL);
}

/* =========================================================================
   READ-BACK — g_last_* scratch değerlerini GCL'e taşır.
   raylib'de struct döndüren çağrılar (GetMousePosition, MeasureTextEx,
   GetWorldToScreen, GetScreenToWorldRay, GetGlyphInfo, ColorToHSV, ...)
   sonucu bu global'lere yazar; modül ABI'si yalnızca tek bir double
   döndürebildiği için bu değerler önceden HİÇ okunamıyordu. Bu üyeler
   bileşenleri tek tek erişilebilir yapar.

   Tek bir X-macro listesi hem fn_Last<NAME> gövdelerini hem g_entries
   satırlarını üretir; böylece ikisi asla ayrışmaz.
   ========================================================================= */
#define GCL_RAYLIB_LAST_LIST(X) \
    X(Int, g_last_int) \
    X(V2X, g_last_v2.x) X(V2Y, g_last_v2.y) \
    X(V3X, g_last_v3.x) X(V3Y, g_last_v3.y) X(V3Z, g_last_v3.z) \
    X(V4X, g_last_v4.x) X(V4Y, g_last_v4.y) X(V4Z, g_last_v4.z) X(V4W, g_last_v4.w) \
    X(RectX, g_last_rect.x) X(RectY, g_last_rect.y) \
    X(RectW, g_last_rect.width) X(RectH, g_last_rect.height) \
    X(RayX, g_last_ray.position.x) X(RayY, g_last_ray.position.y) X(RayZ, g_last_ray.position.z) \
    X(RayDirX, g_last_ray.direction.x) X(RayDirY, g_last_ray.direction.y) X(RayDirZ, g_last_ray.direction.z) \
    X(RayColHit, g_last_raycol.hit) X(RayColDistance, g_last_raycol.distance) \
    X(RayColPX, g_last_raycol.point.x) X(RayColPY, g_last_raycol.point.y) X(RayColPZ, g_last_raycol.point.z) \
    X(RayColNX, g_last_raycol.normal.x) X(RayColNY, g_last_raycol.normal.y) X(RayColNZ, g_last_raycol.normal.z) \
    X(BoxMinX, g_last_bb.min.x) X(BoxMinY, g_last_bb.min.y) X(BoxMinZ, g_last_bb.min.z) \
    X(BoxMaxX, g_last_bb.max.x) X(BoxMaxY, g_last_bb.max.y) X(BoxMaxZ, g_last_bb.max.z) \
    X(NPatchX, g_last_npatch.source.x) X(NPatchY, g_last_npatch.source.y) \
    X(NPatchW, g_last_npatch.source.width) X(NPatchH, g_last_npatch.source.height) \
    X(NPatchLeft, g_last_npatch.left) X(NPatchTop, g_last_npatch.top) \
    X(NPatchRight, g_last_npatch.right) X(NPatchBottom, g_last_npatch.bottom) \
    X(NPatchLayout, g_last_npatch.layout) \
    X(GlyphValue, g_last_glyph.value) X(GlyphOffsetX, g_last_glyph.offsetX) \
    X(GlyphOffsetY, g_last_glyph.offsetY) X(GlyphAdvanceX, g_last_glyph.advanceX) \
    X(GlyphImgWidth, g_last_glyph.image.width) X(GlyphImgHeight, g_last_glyph.image.height) \
    X(CamPosX, g_last_cam.position.x) X(CamPosY, g_last_cam.position.y) X(CamPosZ, g_last_cam.position.z) \
    X(CamTargetX, g_last_cam.target.x) X(CamTargetY, g_last_cam.target.y) X(CamTargetZ, g_last_cam.target.z) \
    X(CamUpX, g_last_cam.up.x) X(CamUpY, g_last_cam.up.y) X(CamUpZ, g_last_cam.up.z) \
    X(CamFovy, g_last_cam.fovy) X(CamProjection, g_last_cam.projection) \
    X(Cam2DOffsetX, g_last_cam2d.offset.x) X(Cam2DOffsetY, g_last_cam2d.offset.y) \
    X(Cam2DTargetX, g_last_cam2d.target.x) X(Cam2DTargetY, g_last_cam2d.target.y) \
    X(Cam2DRotation, g_last_cam2d.rotation) X(Cam2DZoom, g_last_cam2d.zoom) \
    X(QuatX, g_last_quat.x) X(QuatY, g_last_quat.y) X(QuatZ, g_last_quat.z) X(QuatW, g_last_quat.w) \
    X(TransformTX, g_last_transform.translation.x) X(TransformTY, g_last_transform.translation.y) \
    X(TransformTZ, g_last_transform.translation.z) \
    X(TransformRX, g_last_transform.rotation.x) X(TransformRY, g_last_transform.rotation.y) \
    X(TransformRZ, g_last_transform.rotation.z) \
    X(TransformSX, g_last_transform.scale.x) X(TransformSY, g_last_transform.scale.y) \
    X(TransformSZ, g_last_transform.scale.z) \
    X(MatMapValue, g_last_matmap.value) \
    X(BoneParent, g_last_bone.parent) \
    X(VrDevHRes, g_last_vrdev.hResolution) X(VrDevVRes, g_last_vrdev.vResolution) \
    X(VrDevHScreen, g_last_vrdev.hScreenSize) X(VrDevVScreen, g_last_vrdev.vScreenSize) \
    X(VrDevEyeDist, g_last_vrdev.eyeToScreenDistance) \
    X(VrDevLensSep, g_last_vrdev.lensSeparationDistance) \
    X(VrDevIpd, g_last_vrdev.interpupillaryDistance) \
    X(AEventFrame, g_last_aevent.frame) X(AEventType, g_last_aevent.type) \
    X(AEventP0, g_last_aevent.params[0]) X(AEventP1, g_last_aevent.params[1]) \
    X(AEventP2, g_last_aevent.params[2]) X(AEventP3, g_last_aevent.params[3])

#define GCL_LAST_FN(NAME, EXPR) \
    static double fn_Last##NAME(int argc,const char**argv){(void)argc;(void)argv;return (double)(EXPR);}
GCL_RAYLIB_LAST_LIST(GCL_LAST_FN)
#undef GCL_LAST_FN

/* Matris bileşeni (0..15) ve string çıktısı: argüman gerektirdiği için
   listede değil, ayrı üyeler. String için n = LastStringLen(), sonra
   LastStringByte(i) ile bayt bayt okunur. */
static double fn_LastMatrix(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : 0;
    const float *m = &g_last_mat.m0;
    if (i < 0 || i > 15) return 0.0;
    return (double)m[i];
}
static double fn_LastStringLen(int argc,const char**argv){(void)argc;(void)argv;return (double)strlen(g_str);}
static double fn_LastStringByte(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    int n = (int)strlen(g_str);
    if (i < 0 || i >= n) return 0.0;
    return (double)(unsigned char)g_str[i];
}
/* VrDeviceInfo'nun float[4] alanları indeksle okunur: X-macro listesi argüman
   alamadığı için (LastMatrix gibi) ayrı üyelerdir. */
static double fn_LastVrDevLensDistortion(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    if (i < 0 || i > 3) return 0.0;
    return (double)g_last_vrdev.lensDistortionValues[i];
}
static double fn_LastVrDevChromaAb(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    if (i < 0 || i > 3) return 0.0;
    return (double)g_last_vrdev.chromaAbCorrection[i];
}
/* VrStereoConfig okuma: LoadVrStereoConfig'in ürettiği matrislerin 16 bileşeni
   (0..15) ve 8 çift (lens/screen center, scale, scaleIn) 0..15 indeksiyle. */
static double fn_LastVrProjection(int argc,const char**argv){
    int eye = (argc > 0) ? ii(argv[0]) : 0;
    int i   = (argc > 1) ? ii(argv[1]) : -1;
    if (eye < 0 || eye > 1 || i < 0 || i > 15) return 0.0;
    return (double)(&g_last_vr.projection[eye].m0)[i];
}
static double fn_LastVrViewOffset(int argc,const char**argv){
    int eye = (argc > 0) ? ii(argv[0]) : 0;
    int i   = (argc > 1) ? ii(argv[1]) : -1;
    if (eye < 0 || eye > 1 || i < 0 || i > 15) return 0.0;
    return (double)(&g_last_vr.viewOffset[eye].m0)[i];
}
/* VrStereoConfig'in 8 çifti tek indeks uzayında okunur:
   0-1: leftLensCenter, 2-3: rightLensCenter, 4-5: leftScreenCenter,
   6-7: rightScreenCenter, 8-9: scale, 10-11: scaleIn. */
static double fn_LastVrPair(int argc,const char**argv){
    int idx = (argc > 0) ? ii(argv[0]) : -1;
    const float *p = NULL;
    if (idx < 0 || idx > 11) return 0.0;
    switch (idx / 2) {
        case 0: p = g_last_vr.leftLensCenter;   break;
        case 1: p = g_last_vr.rightLensCenter;  break;
        case 2: p = g_last_vr.leftScreenCenter; break;
        case 3: p = g_last_vr.rightScreenCenter;break;
        case 4: p = g_last_vr.scale;            break;
        default: p = g_last_vr.scaleIn;         break;
    }
    return (double)p[idx % 2];
}





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
/* Packed-color scratch shared by SetPixelColor/GetPixelColor. */
static double g_pixel_scratch[16] = {0};
/* GetPixelColor(srcPtr, format): reads one pixel out of raw pixel DATA.
   GCL has no pointer channel, so the bitmap arrives as a string (for example
   straight out of LoadFileData). Called with a numeric first argument it
   instead returns the value kept by SetPixelColor. */
static double fn_GetPixelColor(int argc,const char**argv){
    if (argc > 0 && argv[0] && argv[0][0] != '#') {
        int fmt = (argc > 1) ? ii(argv[1]) : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return (double)color_to_uint(GetPixelColor((const void *)argv[0], fmt));
    }
    return g_pixel_scratch[0];
}
/* SetPixelColor(dstPtr, color, format): stores the packed color module-side.
   GCL strings are read-only, so the destination pointer cannot be written
   through; the value is kept in g_pixel_scratch and read back with
   GetPixelColor(0, format). dstPtr is accepted for raylib parity and ignored. */
static double fn_SetPixelColor(int argc,const char**argv){
    int slot = (argc >= 3) ? 1 : 0;   /* (dst,color,format) or (color[,format]) */
    g_pixel_scratch[0] = (double)color_to_uint(ci(argv, slot));
    return g_pixel_scratch[0];
}
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

/* --------------------------------------------------------------------------
   Cross-module helper (EXPORTED, called from RaylibFPS.dll)

   `Raylib.BeginMode3D()` draws with g_last_cam, and g_last_cam is normally
   filled by `Raylib.Camera3D(...)`. RaylibFPS runs in the SAME process and
   shares the same raylib state, so `Player.Camera()` can write the player
   camera here directly. The script is then two lines:

       RaylibFPS.Camera(player);   // Player.Camera
       Raylib.BeginMode3D();

   instead of eleven numbers spelled out by hand. Nothing else in the module
   changes: Camera3D()/BeginMode3D() keep working exactly as before.
   -------------------------------------------------------------------------- */
GCL_EXPORT void gcl_raylib_camera_set(double px, double py, double pz,
                                      double tx, double ty, double tz,
                                      double ux, double uy, double uz,
                                      double fovy, double projection) {
    g_last_cam.position   = (Vector3){(float)px, (float)py, (float)pz};
    g_last_cam.target     = (Vector3){(float)tx, (float)ty, (float)tz};
    g_last_cam.up         = (Vector3){(float)ux, (float)uy, (float)uz};
    g_last_cam.fovy       = (float)fovy;
    g_last_cam.projection = (int)projection;
}

/* Cross-module helper (EXPORTED, called from RaylibSimpleMesh.dll).

   Textures live in this module's registry, so a terrain material asks here
   for the real Texture2D behind a `Raylib.LoadTexture(...)` handle instead of
   keeping a second registry in the other module. An unknown handle answers a
   zero texture, which raylib draws untinted. */
GCL_EXPORT Texture2D gcl_raylib_texture_get(int handle) {
    return get_tex(handle);
}
static double fn_Ray(int argc,const char**argv){(void)argc;(void)argv;g_last_ray=(Ray){v3_arg(argv,0),v3_arg(argv,3)};return 0.0;}
static double fn_BoundingBox(int argc,const char**argv){(void)argc;(void)argv;g_last_bb=(BoundingBox){v3_arg(argv,0),v3_arg(argv,3)};return 0.0;}
static double fn_NPatchInfo(int argc,const char**argv){(void)argc;(void)argv;g_last_npatch=(NPatchInfo){rect_arg(argv,0),ii(argv[4]),ii(argv[5]),ii(argv[6]),ii(argv[7]),ii(argv[8])};return 0.0;}
static double fn_GlyphInfo(int argc,const char**argv){(void)argc;(void)argv;g_last_glyph=(GlyphInfo){ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),get_img(ii(argv[4]))};return 0.0;}
/* VrStereoConfig: projection/viewOffset matrislerini yalnizca raylib
   hesaplayabilir, bu yuzden kurucu sifirlar (IDE bu tip icin parametre de
   gostermez). Gercek deger icin Raylib.LoadVrStereoConfig(...) kullanin;
   sonuc LastVrProjection/LastVrViewOffset/LastVrPair ile okunur. */
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

/* ---- B25: pencere/GL bağlamı gerektiren üyeler için koruma --------------
   `InitWindow` çağrılmadan bu üyeler çağrılınca raylib GEÇERSİZ duruma erişiyor:
   `Raylib.CloseWindow()` → ACCESS_VIOLATION (EXIT 0xC0000005) — yakalanamayan
   bir çökme. Diğer bazı üyeler (DrawText, SetTargetFPS, GetScreenWidth) raylib
   tarafından tolere ediliyor, bu yüzden yalnızca pencere/GL/cursor DURUMUNA
   doğrudan dokunan üyeler korunur. Pencere yoksa çağrı HİÇ yapılmaz, bir kez
   stderr'e uyarı yazılır ve 0 döner (sessiz çökme yerine).

   ÖNEMLİ: koruma üye GÖVDESİNİN içindedir (tablo değil). `tools/gen_native_db.py`
   üye kümesini `g_entries[]` içindeki `E(NAME)` satırlarından, dönüş tipini de
   `fn_<NAME>` gövdesinden okur; sarmalayıcı bir fn_ üretmek tabloyu ve tipleri
   bozar (1142 → 1099 üye, 3 tamamlama testi kırılıyordu). */
static int g_no_window_warned = 0;

static void gcl_no_window_warn(const char *name) {
    if (g_no_window_warned) return;
    g_no_window_warned = 1;
    fprintf(stderr, "Raylib.%s: window not ready "
                    "(call Raylib.InitWindow first) - call skipped\n",
            name ? name : "?");
}

#define GCL_NO_WINDOW(NAME) \
    do { if (!IsWindowReady()) { gcl_no_window_warn(#NAME); return 0.0; } } while (0)

static double fn_InitWindow(int argc,const char**argv){
    InitWindow(ii(argv[0]),ii(argv[1]),ss(argv[2])); return 0.0;
}
static double fn_CloseWindow(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(CloseWindow);CloseWindow();return 0.0;}
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
    unsigned int f=(argc>0)?(unsigned int)strtoul(argv[0],NULL,10):0; GCL_NO_WINDOW(SetWindowState); SetWindowState(f); return 0.0;
}
static double fn_ClearWindowState(int argc,const char**argv){
    unsigned int f=(argc>0)?(unsigned int)strtoul(argv[0],NULL,10):0; GCL_NO_WINDOW(ClearWindowState); ClearWindowState(f); return 0.0;
}
static double fn_ToggleFullscreen(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(ToggleFullscreen);ToggleFullscreen();return 0.0;}
static double fn_ToggleBorderlessWindowed(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(ToggleBorderlessWindowed);ToggleBorderlessWindowed();return 0.0;}
static double fn_MaximizeWindow(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(MaximizeWindow);MaximizeWindow();return 0.0;}
static double fn_MinimizeWindow(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(MinimizeWindow);MinimizeWindow();return 0.0;}
static double fn_RestoreWindow(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(RestoreWindow);RestoreWindow();return 0.0;}
static double fn_SetWindowIcon(int argc,const char**argv){
    GCL_NO_WINDOW(SetWindowIcon);
    int h=(argc>0)?(int)atof(argv[0]):-1; SetWindowIcon(get_img(h)); return 0.0;
}
/* SetWindowIcons(count, image0[, image1[, ...]]): raylib takes an
   `Image *images` array; GCL passes a count followed by registry handles. */
static double fn_SetWindowIcons(int argc,const char**argv){
    static Image imgs[8];
    int count = (argc > 0) ? ii(argv[0]) : 0;
    GCL_NO_WINDOW(SetWindowIcons);
    if (count < 0) count = 0;
    if (count > 8) count = 8;
    for (int i = 0; i < count; i++) imgs[i] = get_img(ii(argv[1 + i]));
    if (count > 0) SetWindowIcons(imgs, count);
    return 0.0;
}
static double fn_SetWindowTitle(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowTitle); SetWindowTitle(ss(argv[0])); return 0.0; }
static double fn_SetWindowPosition(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowPosition); SetWindowPosition(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowMonitor(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowMonitor); SetWindowMonitor(ii(argv[0])); return 0.0; }
static double fn_SetWindowMinSize(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowMinSize); SetWindowMinSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowMaxSize(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowMaxSize); SetWindowMaxSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowSize(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowSize); SetWindowSize(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_SetWindowOpacity(int argc,const char**argv){ GCL_NO_WINDOW(SetWindowOpacity); SetWindowOpacity(ff(argv[0])); return 0.0; }
static double fn_SetWindowFocused(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(SetWindowFocused);SetWindowFocused();return 0.0;}
static double fn_GetWindowHandle(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(GetWindowHandle);return (double)(intptr_t)GetWindowHandle();}
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
static double fn_ShowCursor(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(ShowCursor);ShowCursor();return 0.0;}
static double fn_HideCursor(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(HideCursor);HideCursor();return 0.0;}
static double fn_IsCursorHidden(int argc,const char**argv){(void)argc;(void)argv;return IsCursorHidden()?1.0:0.0;}
static double fn_EnableCursor(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EnableCursor);EnableCursor();return 0.0;}
static double fn_DisableCursor(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(DisableCursor);DisableCursor();return 0.0;}
static double fn_IsCursorOnScreen(int argc,const char**argv){(void)argc;(void)argv;return IsCursorOnScreen()?1.0:0.0;}

/* drawing — B25: GL bağlamı (rlgl toplu çizim) gerektirirler. */
static double fn_ClearBackground(int argc,const char**argv){ GCL_NO_WINDOW(ClearBackground); ClearBackground(ci(argv,0)); return 0.0; }
static double fn_BeginDrawing(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(BeginDrawing);BeginDrawing();return 0.0;}
static double fn_EndDrawing(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndDrawing);EndDrawing();return 0.0;}
static double fn_BeginMode2D(int argc,const char**argv){
    if (argc >= 6) g_last_cam2d=(Camera2D){v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ff(argv[5])};
    /* Argümansız çağrı: Raylib.Camera2D(...) ile oluşturulan son kamerayı kullan */
    GCL_NO_WINDOW(BeginMode2D);
    BeginMode2D(g_last_cam2d); return 0.0;
}
static double fn_EndMode2D(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndMode2D);EndMode2D();return 0.0;}
static double fn_BeginMode3D(int argc,const char**argv){
    if (argc >= 11) g_last_cam=(Camera3D){v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ff(argv[9]),ii(argv[10])};
    /* Argümansız çağrı: Raylib.Camera(...) ile oluşturulan son kamerayı kullan */
    GCL_NO_WINDOW(BeginMode3D);
    BeginMode3D(g_last_cam); return 0.0;
}
static double fn_EndMode3D(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndMode3D);EndMode3D();return 0.0;}
static double fn_BeginTextureMode(int argc,const char**argv){ GCL_NO_WINDOW(BeginTextureMode); BeginTextureMode(get_rt(ii(argv[0]))); return 0.0; }
static double fn_EndTextureMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndTextureMode);EndTextureMode();return 0.0;}
static double fn_BeginShaderMode(int argc,const char**argv){ GCL_NO_WINDOW(BeginShaderMode); BeginShaderMode(get_shader(ii(argv[0]))); return 0.0; }
static double fn_EndShaderMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndShaderMode);EndShaderMode();return 0.0;}
static double fn_BeginBlendMode(int argc,const char**argv){ GCL_NO_WINDOW(BeginBlendMode); BeginBlendMode(ii(argv[0])); return 0.0; }
static double fn_EndBlendMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndBlendMode);EndBlendMode();return 0.0;}
static double fn_BeginScissorMode(int argc,const char**argv){ GCL_NO_WINDOW(BeginScissorMode); BeginScissorMode(ii(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3])); return 0.0; }
static double fn_EndScissorMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndScissorMode);EndScissorMode();return 0.0;}
static double fn_BeginVrStereoMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(BeginVrStereoMode);BeginVrStereoMode(g_last_vr);return 0.0;}
static double fn_EndVrStereoMode(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(EndVrStereoMode);EndVrStereoMode();return 0.0;}

/* VR */
/* LoadVrStereoConfig(hRes, vRes, hScreen, vScreen, eyeDist, lensSep, ipd
                     [, lensDistortion0..3][, chromaAbCorrection0..3])
   ARGUMANSIZ cagri en son Raylib.VrDeviceInfo(...) ile kurulan cihazi kullanir,
   boylece VrDeviceInfo kurucusu tek basina ise yarar. Opsiyonel 8 deger
   (lens bozulmasi + kromatik sapma) argümanla da verilebilir. */
static double fn_LoadVrStereoConfig(int argc,const char**argv){
    VrDeviceInfo d;
    if (argc <= 0) {
        d = g_last_vrdev;                    /* Raylib.VrDeviceInfo(...) sonucu */
    } else {
        memset(&d,0,sizeof(d));
        d.hResolution            = (argc > 0) ? ii(argv[0]) : 0;
        d.vResolution            = (argc > 1) ? ii(argv[1]) : 0;
        d.hScreenSize            = (argc > 2) ? ff(argv[2]) : 0.0f;
        d.vScreenSize            = (argc > 3) ? ff(argv[3]) : 0.0f;
        d.eyeToScreenDistance    = (argc > 4) ? ff(argv[4]) : 0.0f;
        d.lensSeparationDistance = (argc > 5) ? ff(argv[5]) : 0.0f;
        d.interpupillaryDistance = (argc > 6) ? ff(argv[6]) : 0.0f;
        for (int i = 0; i < 4; i++) {
            if (argc > 7 + i)  d.lensDistortionValues[i] = ff(argv[7 + i]);
            if (argc > 11 + i) d.chromaAbCorrection[i]   = ff(argv[11 + i]);
        }
    }
    g_last_vr=LoadVrStereoConfig(d); return 0.0;
}
static double fn_UnloadVrStereoConfig(int argc,const char**argv){(void)argc;(void)argv;UnloadVrStereoConfig(g_last_vr);return 0.0;}

/* shaders */
static double fn_LoadShader(int argc,const char**argv){ g_last_handle=reg_shader(LoadShader(ss(argv[0]),ss(argv[1]))); return (double)g_last_handle; }
static double fn_LoadShaderFromMemory(int argc,const char**argv){ g_last_handle=reg_shader(LoadShaderFromMemory(ss(argv[0]),ss(argv[1]))); return (double)g_last_handle; }
static double fn_IsShaderValid(int argc,const char**argv){ return IsShaderValid(get_shader(ii(argv[0])))?1.0:0.0; }
static double fn_GetShaderLocation(int argc,const char**argv){ return (double)GetShaderLocation(get_shader(ii(argv[0])),ss(argv[1])); }
static double fn_GetShaderLocationAttrib(int argc,const char**argv){ return (double)GetShaderLocationAttrib(get_shader(ii(argv[0])),ss(argv[1])); }
/* SetShaderValue(shader, locIndex, uniformType, x[, y[, z[, w]]]).
   raylib takes a `const void *` whose layout depends on uniformType; the module
   builds that value from the numeric arguments instead, so a GCL script never
   has to know the C union layout. */
static double fn_SetShaderValue(int argc,const char**argv){
    Shader sh = get_shader((argc > 0) ? ii(argv[0]) : -1);
    int loc   = (argc > 1) ? ii(argv[1]) : -1;
    int ut    = (argc > 2) ? ii(argv[2]) : SHADER_UNIFORM_FLOAT;
    float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int n = argc - 3;
    if (n > 4) n = 4;
    for (int i = 0; i < n; i++) v[i] = ff(argv[3 + i]);
    if (ut == SHADER_UNIFORM_INT || ut == SHADER_UNIFORM_IVEC2 ||
        ut == SHADER_UNIFORM_IVEC3 || ut == SHADER_UNIFORM_IVEC4) {
        int iv[4] = {(int)v[0], (int)v[1], (int)v[2], (int)v[3]};
        SetShaderValue(sh, loc, (const void *)iv, ut);
    } else if (ut == SHADER_UNIFORM_FLOAT) {
        SetShaderValue(sh, loc, (const void *)&v[0], ut);
    } else {
        SetShaderValue(sh, loc, (const void *)v, ut);
    }
    return 0.0;
}
/* SetShaderValueV(shader, locIndex, uniformType, count, v0, v1, ...). */
static double fn_SetShaderValueV(int argc,const char**argv){
    Shader sh = get_shader((argc > 0) ? ii(argv[0]) : -1);
    int loc   = (argc > 1) ? ii(argv[1]) : -1;
    int ut    = (argc > 2) ? ii(argv[2]) : SHADER_UNIFORM_FLOAT;
    int count = (argc > 3) ? ii(argv[3]) : 0;
    float v[64];
    if (count < 0) count = 0;
    if (count > 64) count = 64;
    for (int i = 0; i < count; i++) v[i] = (4 + i < argc) ? ff(argv[4 + i]) : 0.0f;
    if (count > 0) SetShaderValueV(sh, loc, (const void *)v, ut, count);
    return 0.0;
}
/* SetShaderValueMatrix(shader, locIndex, m0 ... m15). */
static double fn_SetShaderValueMatrix(int argc,const char**argv){
    Shader sh = get_shader((argc > 0) ? ii(argv[0]) : -1);
    int loc   = (argc > 1) ? ii(argv[1]) : -1;
    Matrix m;
    float *f = &m.m0;
    for (int i = 0; i < 16; i++) f[i] = (2 + i < argc) ? ff(argv[2 + i]) : 0.0f;
    SetShaderValueMatrix(sh, loc, m);
    return 0.0;
}
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
static double fn_SwapScreenBuffer(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(SwapScreenBuffer);SwapScreenBuffer();return 0.0;}
static double fn_PollInputEvents(int argc,const char**argv){(void)argc;(void)argv;GCL_NO_WINDOW(PollInputEvents);PollInputEvents();return 0.0;}
static double fn_WaitTime(int argc,const char**argv){ WaitTime(atof(argv[0])); return 0.0; }
static double fn_SetRandomSeed(int argc,const char**argv){ SetRandomSeed((unsigned int)strtoul(argv[0],NULL,10)); return 0.0; }
static double fn_GetRandomValue(int argc,const char**argv){ return (double)GetRandomValue(ii(argv[0]),ii(argv[1])); }
/* LoadRandomSequence(count, min, max): raylib returns an int*; GCL has no array
   values, so the sequence is written into g_str as a comma separated list
   ("3,17,4,...") and the membership is released immediately. */
static double fn_LoadRandomSequence(int argc,const char**argv){
    int count = (argc > 0) ? ii(argv[0]) : 0;
    int mn    = (argc > 1) ? ii(argv[1]) : 0;
    int mx    = (argc > 2) ? ii(argv[2]) : 100;
    if (count <= 0) { g_str[0] = 0; return 0.0; }
    if (count > 512) count = 512;
    int *seq = LoadRandomSequence((unsigned int)count, mn, mx);
    if (!seq) { g_str[0] = 0; return 0.0; }
    int len = 0;
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + len, sizeof(g_str) - (size_t)len,
                             "%s%d", (i ? "," : ""), seq[i]);
        if (wrote < 0 || (size_t)(len + wrote) >= sizeof(g_str)) break;
        len += wrote;
    }
    UnloadRandomSequence(seq);
    return (double)count;
}
static double fn_UnloadRandomSequence(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_TakeScreenshot(int argc,const char**argv){ GCL_NO_WINDOW(TakeScreenshot); TakeScreenshot(ss(argv[0])); return 0.0; }
static double fn_SetConfigFlags(int argc,const char**argv){ SetConfigFlags((unsigned int)strtoul(argv[0],NULL,10)); return 0.0; }
static double fn_OpenURL(int argc,const char**argv){ OpenURL(ss(argv[0])); return 0.0; }
static double fn_SetTraceLogLevel(int argc,const char**argv){ SetTraceLogLevel(ii(argv[0])); return 0.0; }
/* TraceLog(logLevel, text): the variadic raylib form is reduced to a single
   formatted message, which is what a GCL script can actually produce. */
static double fn_TraceLog(int argc,const char**argv){
    int lvl = (argc > 0) ? ii(argv[0]) : LOG_INFO;
    TraceLog(lvl, "%s", ss((argc > 1) ? argv[1] : ""));
    return 0.0;
}
/* SetTraceLogCallback("onTrace") — raylib'e verilen C callback'i modülün
   trambolinidir; trambolin, çağrılacak GCL fonksiyonunu ADIYLA bulur ve
   host->call_gcl ile çalıştırır (bkz. dosya başındaki Host API bölümü).
   Argümansız çağrı callback'i kaldırır. */
static double fn_SetTraceLogCallback(int argc,const char**argv){
    /* SetTraceLogCallback("onTrace") — GCL fonksiyonunun ADI verilir.
       Argümansız çağrı callback'i KALDIRIR. */
    if (argc < 1 || !argv[0] || !argv[0][0]) {
        snprintf(g_trace_fn, sizeof(g_trace_fn), "");
        SetTraceLogCallback(NULL);
        return 0.0;
    }
    snprintf(g_trace_fn, sizeof(g_trace_fn), "%s", argv[0]);
    SetTraceLogCallback(gcl_trace_trampoline);
    return 0.0;
}

/* ---------- Opaque memory handles (MemAlloc / MemRealloc / MemFree) ----------
   GCL values are doubles and strings, so a raw pointer cannot cross the ABI.
   MemAlloc therefore returns a SLOT INDEX into this table, which keeps the
   allocate/reallocate/free round trip possible from a script. */
#define GCL_MEM_SLOTS 64
static void *g_mem[GCL_MEM_SLOTS] = {0};
static double fn_MemAlloc(int argc,const char**argv){
    int size = (argc > 0) ? ii(argv[0]) : 0;
    if (size <= 0) return -1.0;
    for (int i = 0; i < GCL_MEM_SLOTS; i++) {
        if (!g_mem[i]) { g_mem[i] = MemAlloc((unsigned int)size); return (double)i; }
    }
    return -1.0;
}
static double fn_MemRealloc(int argc,const char**argv){
    int slot = (argc > 0) ? ii(argv[0]) : -1;
    int size = (argc > 1) ? ii(argv[1]) : 0;
    if (slot < 0 || slot >= GCL_MEM_SLOTS || size <= 0) return -1.0;
    g_mem[slot] = MemRealloc(g_mem[slot], (unsigned int)size);
    return (double)slot;
}
static double fn_MemFree(int argc,const char**argv){
    int slot = (argc > 0) ? ii(argv[0]) : -1;
    if (slot < 0 || slot >= GCL_MEM_SLOTS) return 0.0;
    if (g_mem[slot]) { MemFree(g_mem[slot]); g_mem[slot] = NULL; }
    return 0.0;
}

/* file system */
/* ---------- Byte <-> text bridges for the binary raylib APIs ----------
   A GCL string is the only byte container available, so raw buffers are moved
   across the module ABI as text: HEX when the payload must survive exactly
   (compression, hashes, decoded base64), plain text when it is human readable
   (LoadFileData, decompressed output). */
static const char GCL_HEX[] = "0123456789abcdef";
static void gcl_bytes_to_hex(const unsigned char *data, int size){
    int n = 0;
    if (!data || size <= 0) { g_str[0] = 0; return; }
    for (int i = 0; i < size && n + 3 < (int)sizeof(g_str); i++) {
        g_str[n++] = GCL_HEX[(data[i] >> 4) & 0x0F];
        g_str[n++] = GCL_HEX[data[i] & 0x0F];
    }
    g_str[n] = 0;
}
static void gcl_uint_words_to_hex(const unsigned int *words, int count){
    int n = 0;
    if (!words) { g_str[0] = 0; return; }
    for (int i = 0; i < count; i++) {
        unsigned int v = words[i];
        for (int b = 3; b >= 0; b--) {
            if (n + 3 >= (int)sizeof(g_str)) { g_str[n] = 0; return; }
            g_str[n++] = GCL_HEX[(v >> (8 * b + 4)) & 0x0F];
            g_str[n++] = GCL_HEX[(v >> (8 * b)) & 0x0F];
        }
    }
    g_str[n] = 0;
}
static void gcl_bytes_to_str(const unsigned char *data, int size){
    int n = (size < (int)sizeof(g_str) - 1) ? size : (int)sizeof(g_str) - 1;
    if (!data || n <= 0) { g_str[0] = 0; return; }
    memcpy(g_str, data, (size_t)n);
    g_str[n] = 0;
}
static int gcl_hex_val(int c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* Decodes a hex string into a fresh malloc'd buffer (caller frees). */
static unsigned char *gcl_hex_to_bytes(const char *hex, int hex_len, int *out_size){
    int n = 0;
    unsigned char *out;
    *out_size = 0;
    if (!hex || hex_len <= 0) return NULL;
    out = (unsigned char *)malloc((size_t)(hex_len / 2) + 1);
    if (!out) return NULL;
    for (int i = 0; i + 1 < hex_len; i += 2) {
        int hi = gcl_hex_val((unsigned char)hex[i]);
        int lo = gcl_hex_val((unsigned char)hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
    }
    *out_size = n;
    return out;
}
/* Optional size argument: defaults to strlen(data) when omitted. */
static int gcl_opt_size(int argc, const char **argv, int idx, const char *data){
    if (argc > idx && argv[idx]) return ii(argv[idx]);
    return data ? (int)strlen(data) : 0;
}

/* LoadFileData(fileName): raylib returns a byte* plus an out-size. GCL receives
   the bytes as text in g_str and the byte count as the return value. */
static double fn_LoadFileData(int argc,const char**argv){
    int size = 0;
    unsigned char *data = LoadFileData(ss((argc > 0) ? argv[0] : ""), &size);
    if (data) { gcl_bytes_to_str(data, size); UnloadFileData(data); }
    else g_str[0] = 0;
    return (double)size;
}
/* UnloadFileData(data): the module frees its own copy, so there is nothing left
   to release. Kept so the raylib call sequence stays mirrored. */
static double fn_UnloadFileData(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_SaveFileData(int argc,const char**argv){ return SaveFileData(ss(argv[0]),argv[1],ii(argv[2]))?1.0:0.0; }
static double fn_ExportDataAsCode(int argc,const char**argv){ return ExportDataAsCode((const unsigned char*)(argv[0]?argv[0]:(const char*)""),ii(argv[1]),ss(argv[2]))?1.0:0.0; }
/* LoadFileText(fileName): raylib returns a char*. GCL copies the text into
   g_str and returns its BYTE COUNT - the same shape as LoadUTF8 (source ->
   NUL-terminated text in the slot -> byte count of what was stored).
   ESKI HATA: bu uye 0.0 donuyordu, yani uzunlugu donus degerinden almak
   imkansizdi ve cagiran `LastStringLen()`e mecburdu - oysa AYNI isi yapan
   `LoadUTF8` uzunlugu donuyordu. 0.0 ayrica "bos dosya" ile "okunamadi"yi
   birbirinden ayirt edilemez kiliyordu; simdi >0 "metin geldi" demektir.
   Uzunluk g_str'ye YAZILANDAN olculur (snprintf kisaltmis olabilir), boylece
   donen sayi `LastStringLen()` ile HER ZAMAN aynidir.
   NOT: `LoadFileData` ile ayni DEGILDIR - orada donen sayi HAM bayt sayisidir
   ve yuva hex kodlanmis oldugu icin `strlen(g_str)` onu veremez (bkz.
   raylib_file_callback.gcsf: size=3 ama baytlar "123"). */
static double fn_LoadFileText(int argc,const char**argv){
    char *t = LoadFileText(ss((argc > 0) ? argv[0] : ""));
    if (t) { snprintf(g_str, sizeof(g_str), "%s", t); UnloadFileText(t); }
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
static double fn_UnloadFileText(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_SaveFileText(int argc,const char**argv){ return SaveFileText(ss(argv[0]),ss(argv[1]))?1.0:0.0; }
/* Bu dört üye artık GERÇEKTEN çalışır — aşağıdaki "Dosya callback köprüsü"
   bölümüne bakın. Eski not şunu iddia ediyordu: "GCL bir C fonksiyonu
   geçiremez, bu yüzden bunlar bilinçli no-op'tur; NULL geçmek sonraki
   Load*File* çağrısını çökertir." Ad tabanlı host köprüsü bu iddiayı
   geçersiz kıldı: GCL fonksiyonunun ADI verilir, modül trambolini kurar;
   boş ad verilirse callback KALDIRILIR (raylib NULL kabul eder — testte
   çökme gözlenmedi, varsayılan davranışa dönülüyor). */
/* ---------- Dosya callback köprüsü ----------

   raylib bu callback'leri dosya okuma/yazma SIRASINDA, ÇAĞIRAN iş
   parçacığında koşturur → host->call_gcl doğrudan çağrılabilir (ses
   callback'inde olduğu gibi ertelenmesi GEREKMEZ).

   Bayt taşıma: GCL'de işaretçi yoktur, bu yüzden modül tek bir bayt tamponu
   tutar. OKUMA callback'i GCL'den baytları FileDataPush(v) ile alır ve
   raylib'e malloc'lanmış blok döndürür (raylib onu MemFree ile bırakır).
   YAZMA callback'i GCL'e yolu (ve boyutu) verir; GCL FileDataAt(i) ile
   baytları okur. Metin varyantları aynı tamponu kullanır. */
#define GCL_FILEBUF 65536
static unsigned char g_filebuf[GCL_FILEBUF];
static int  g_filelen = 0;
static char g_ldata_fn[128] = "", g_sdata_fn[128] = "";
static char g_ltext_fn[128] = "", g_stext_fn[128] = "";

static unsigned char *gcl_load_data_trampoline(const char *fileName, int *dataSize){
    g_filelen = 0;
    if (dataSize) *dataSize = 0;
    if (!g_host || !g_host->call_gcl || !g_ldata_fn[0]) return NULL;
    GclHostArg a[1];
    a[0].is_string = 1; a[0].num = 0.0; a[0].str = fileName ? fileName : "";
    g_host->call_gcl(g_host->host, g_ldata_fn, 1, a, NULL);
    if (g_filelen <= 0) return NULL;
    unsigned char *out = (unsigned char *)malloc((size_t)g_filelen);
    if (!out) return NULL;
    memcpy(out, g_filebuf, (size_t)g_filelen);
    if (dataSize) *dataSize = g_filelen;
    return out;
}
/* NOT: bu raylib sürümü yazma callback'lerinde `const` bildirir
   (SaveFileDataCallback: const void*, SaveFileTextCallback: const char*);
   imza birebir eşleşmezse atama uyumsuz işaretçi hatası verir. */
static bool gcl_save_data_trampoline(const char *fileName, const void *data, int dataSize){
    if (!g_host || !g_host->call_gcl || !g_sdata_fn[0]) return false;
    int n = dataSize;
    if (n > GCL_FILEBUF) n = GCL_FILEBUF;
    if (n > 0 && data) memcpy(g_filebuf, data, (size_t)n);
    g_filelen = n;
    GclHostArg a[2];
    a[0].is_string = 1; a[0].num = 0.0; a[0].str = fileName ? fileName : "";
    a[1].is_string = 0; a[1].num = (double)dataSize; a[1].str = NULL;
    double ret = 0.0;
    g_host->call_gcl(g_host->host, g_sdata_fn, 2, a, &ret);
    return ret != 0.0;
}
static char *gcl_load_text_trampoline(const char *fileName){
    g_filelen = 0;
    if (!g_host || !g_host->call_gcl || !g_ltext_fn[0]) return NULL;
    GclHostArg a[1];
    a[0].is_string = 1; a[0].num = 0.0; a[0].str = fileName ? fileName : "";
    g_host->call_gcl(g_host->host, g_ltext_fn, 1, a, NULL);
    char *out = (char *)malloc((size_t)g_filelen + 1);
    if (!out) return NULL;
    if (g_filelen > 0) memcpy(out, g_filebuf, (size_t)g_filelen);
    out[g_filelen] = '\0';
    return out;
}
static bool gcl_save_text_trampoline(const char *fileName, const char *text){
    if (!g_host || !g_host->call_gcl || !g_stext_fn[0]) return false;
    GclHostArg a[2];
    a[0].is_string = 1; a[0].num = 0.0; a[0].str = fileName ? fileName : "";
    a[1].is_string = 1; a[1].num = 0.0; a[1].str = text ? text : "";
    double ret = 0.0;
    g_host->call_gcl(g_host->host, g_stext_fn, 2, a, &ret);
    return ret != 0.0;
}

/* Set*File*Callback("gclFn") — GCL fonksiyonunun ADI verilir; boş ad
   callback'i kaldırır. */
static double fn_SetLoadFileDataCallback(int argc,const char**argv){
    snprintf(g_ldata_fn, sizeof(g_ldata_fn), "%s", (argc > 0 && argv[0]) ? argv[0] : "");
    SetLoadFileDataCallback(g_ldata_fn[0] ? gcl_load_data_trampoline : NULL);
    return 0.0;
}
static double fn_SetSaveFileDataCallback(int argc,const char**argv){
    snprintf(g_sdata_fn, sizeof(g_sdata_fn), "%s", (argc > 0 && argv[0]) ? argv[0] : "");
    SetSaveFileDataCallback(g_sdata_fn[0] ? gcl_save_data_trampoline : NULL);
    return 0.0;
}
static double fn_SetLoadFileTextCallback(int argc,const char**argv){
    snprintf(g_ltext_fn, sizeof(g_ltext_fn), "%s", (argc > 0 && argv[0]) ? argv[0] : "");
    SetLoadFileTextCallback(g_ltext_fn[0] ? gcl_load_text_trampoline : NULL);
    return 0.0;
}
static double fn_SetSaveFileTextCallback(int argc,const char**argv){
    snprintf(g_stext_fn, sizeof(g_stext_fn), "%s", (argc > 0 && argv[0]) ? argv[0] : "");
    SetSaveFileTextCallback(g_stext_fn[0] ? gcl_save_text_trampoline : NULL);
    return 0.0;
}

/* Bayt tamponu erişimcileri — GCL callback'i bunlarla bayt basar/okur. */
static double fn_FileDataReset(int argc,const char**argv){ (void)argc;(void)argv; g_filelen = 0; return 0.0; }
static double fn_FileDataPush(int argc,const char**argv){
    if (g_filelen >= GCL_FILEBUF) return 0.0;
    g_filebuf[g_filelen++] = (unsigned char)((argc > 0 && argv[0]) ? ((long long)atof(argv[0])) : 0);
    return 1.0;
}
static double fn_FileDataSize(int argc,const char**argv){ (void)argc;(void)argv; return (double)g_filelen; }
static double fn_FileDataAt(int argc,const char**argv){
    int i = (argc > 0) ? ii(argv[0]) : -1;
    if (i < 0 || i >= g_filelen) return 0.0;
    return (double)g_filebuf[i];
}
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
/* CompressData(data[, dataSize]) -> hex of the compressed bytes in g_str.
   Hex keeps the deflate payload intact through a GCL string, and the result can
   be handed straight back to DecompressData. */
static double fn_CompressData(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int src_size = gcl_opt_size(argc, argv, 1, (const char *)src);
    int comp_size = 0;
    unsigned char *comp = CompressData(src, src_size, &comp_size);
    if (comp) { gcl_bytes_to_hex(comp, comp_size); MemFree(comp); }
    else g_str[0] = 0;
    return (double)comp_size;
}
/* DecompressData(hexData[, hexSize]) -> the decompressed bytes as text. */
static double fn_DecompressData(int argc,const char**argv){
    int hex_len = gcl_opt_size(argc, argv, 1, argv[0]);
    int bin_len = 0;
    unsigned char *bin = gcl_hex_to_bytes(ss(argv[0]), hex_len, &bin_len);
    int out_size = 0;
    unsigned char *out;
    if (!bin) { g_str[0] = 0; return 0.0; }
    out = DecompressData(bin, bin_len, &out_size);
    if (out) { gcl_bytes_to_str(out, out_size); MemFree(out); }
    else g_str[0] = 0;
    free(bin);
    return (double)out_size;
}
/* EncodeDataBase64(data[, dataSize]) -> base64 text in g_str. */
static double fn_EncodeDataBase64(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int src_size = gcl_opt_size(argc, argv, 1, (const char *)src);
    int out_size = 0;
    char *b64 = EncodeDataBase64(src, src_size, &out_size);
    if (b64) { snprintf(g_str, sizeof(g_str), "%s", b64); MemFree(b64); }
    else g_str[0] = 0;
    return (double)out_size;
}
/* DecodeDataBase64(text) -> the decoded bytes as text in g_str. */
static double fn_DecodeDataBase64(int argc,const char**argv){
    int out_size = 0;
    unsigned char *dec = DecodeDataBase64((const unsigned char *)ss(argv[0]), &out_size);
    if (dec) { gcl_bytes_to_str(dec, out_size); MemFree(dec); }
    else g_str[0] = 0;
    return (double)out_size;
}
/* ComputeCRC32(data[, dataSize]) -> the checksum itself. */
static double fn_ComputeCRC32(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int size = gcl_opt_size(argc, argv, 1, (const char *)src);
    return (double)ComputeCRC32(src, size);
}
/* The digest functions return int[] words; each word is printed big-endian so
   g_str holds the familiar hex digest (32 / 40 / 64 characters). */
static double fn_ComputeMD5(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int size = gcl_opt_size(argc, argv, 1, (const char *)src);
    gcl_uint_words_to_hex(ComputeMD5(src, size), 4);
    return 0.0;
}
static double fn_ComputeSHA1(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int size = gcl_opt_size(argc, argv, 1, (const char *)src);
    gcl_uint_words_to_hex(ComputeSHA1(src, size), 5);
    return 0.0;
}
static double fn_ComputeSHA256(int argc,const char**argv){
    const unsigned char *src = (const unsigned char *)ss(argv[0]);
    int size = gcl_opt_size(argc, argv, 1, (const char *)src);
    gcl_uint_words_to_hex(ComputeSHA256(src, size), 8);
    return 0.0;
}
static double fn_LoadAutomationEventList(int argc,const char**argv){ g_last_handle=reg_aevl(LoadAutomationEventList(ss(argv[0]))); return (double)g_last_handle; }
static double fn_UnloadAutomationEventList(int argc,const char**argv){ UnloadAutomationEventList(get_aevl(ii(argv[0]))); return 0.0; }
static double fn_ExportAutomationEventList(int argc,const char**argv){ return ExportAutomationEventList(get_aevl(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
/* SetAutomationEventList(listHandle): raylib keeps the pointer, so the list has
   to live in a module-owned scratch slot instead of the caller's stack. */
static AutomationEventList g_aevl_scratch = {0};
static double fn_SetAutomationEventList(int argc,const char**argv){
    g_aevl_scratch = get_aevl((argc > 0) ? ii(argv[0]) : -1);
    SetAutomationEventList(&g_aevl_scratch);
    return 0.0;
}
static double fn_SetAutomationEventBaseFrame(int argc,const char**argv){ SetAutomationEventBaseFrame(ii(argv[0])); return 0.0; }
static double fn_StartAutomationEventRecording(int argc,const char**argv){(void)argc;(void)argv;StartAutomationEventRecording();return 0.0;}
static double fn_StopAutomationEventRecording(int argc,const char**argv){(void)argc;(void)argv;StopAutomationEventRecording();return 0.0;}
/* PlayAutomationEvent(eventFrame, type, p0, p1, p2, p3) — or no arguments to
   replay the event last built with Raylib.AutomationEvent(...). */
static double fn_PlayAutomationEvent(int argc,const char**argv){
    AutomationEvent ev;
    if (argc >= 6) {
        ev = (AutomationEvent){(unsigned int)strtoul(argv[0], NULL, 10),
                               (unsigned int)strtoul(argv[1], NULL, 10),
                               ii(argv[2]), ii(argv[3]), ii(argv[4]), ii(argv[5])};
    } else {
        ev = g_last_aevent;
    }
    PlayAutomationEvent(ev);
    return 0.0;
}

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
   CONSTANTS — CONFIG FLAGS / LOG LEVELS / CAMERA MODE
   ========================================================================= */
ENUM_FN(FLAG_VSYNC_HINT,FLAG_VSYNC_HINT)
ENUM_FN(FLAG_FULLSCREEN_MODE,FLAG_FULLSCREEN_MODE)
ENUM_FN(FLAG_WINDOW_RESIZABLE,FLAG_WINDOW_RESIZABLE)
ENUM_FN(FLAG_WINDOW_UNDECORATED,FLAG_WINDOW_UNDECORATED)
ENUM_FN(FLAG_WINDOW_HIDDEN,FLAG_WINDOW_HIDDEN)
ENUM_FN(FLAG_WINDOW_MINIMIZED,FLAG_WINDOW_MINIMIZED)
ENUM_FN(FLAG_WINDOW_MAXIMIZED,FLAG_WINDOW_MAXIMIZED)
ENUM_FN(FLAG_WINDOW_UNFOCUSED,FLAG_WINDOW_UNFOCUSED)
ENUM_FN(FLAG_WINDOW_TOPMOST,FLAG_WINDOW_TOPMOST)
ENUM_FN(FLAG_WINDOW_ALWAYS_RUN,FLAG_WINDOW_ALWAYS_RUN)
ENUM_FN(FLAG_WINDOW_TRANSPARENT,FLAG_WINDOW_TRANSPARENT)
ENUM_FN(FLAG_WINDOW_HIGHDPI,FLAG_WINDOW_HIGHDPI)
ENUM_FN(FLAG_WINDOW_MOUSE_PASSTHROUGH,FLAG_WINDOW_MOUSE_PASSTHROUGH)
ENUM_FN(FLAG_BORDERLESS_WINDOWED_MODE,FLAG_BORDERLESS_WINDOWED_MODE)
ENUM_FN(FLAG_MSAA_4X_HINT,FLAG_MSAA_4X_HINT)
ENUM_FN(FLAG_INTERLACED_HINT,FLAG_INTERLACED_HINT)

ENUM_FN(LOG_ALL,LOG_ALL)
ENUM_FN(LOG_TRACE,LOG_TRACE)
ENUM_FN(LOG_DEBUG,LOG_DEBUG)
ENUM_FN(LOG_INFO,LOG_INFO)
ENUM_FN(LOG_WARNING,LOG_WARNING)
ENUM_FN(LOG_ERROR,LOG_ERROR)
ENUM_FN(LOG_FATAL,LOG_FATAL)
ENUM_FN(LOG_NONE,LOG_NONE)

ENUM_FN(CAMERA_CUSTOM,CAMERA_CUSTOM)

/* =========================================================================
   CONSTANTS — KEYBOARD KEYS
   ========================================================================= */
ENUM_FN(KEY_NULL,KEY_NULL)
ENUM_FN(KEY_APOSTROPHE,KEY_APOSTROPHE)
ENUM_FN(KEY_COMMA,KEY_COMMA)
ENUM_FN(KEY_MINUS,KEY_MINUS)
ENUM_FN(KEY_PERIOD,KEY_PERIOD)
ENUM_FN(KEY_SLASH,KEY_SLASH)
ENUM_FN(KEY_ZERO,KEY_ZERO)
ENUM_FN(KEY_ONE,KEY_ONE)
ENUM_FN(KEY_TWO,KEY_TWO)
ENUM_FN(KEY_THREE,KEY_THREE)
ENUM_FN(KEY_FOUR,KEY_FOUR)
ENUM_FN(KEY_FIVE,KEY_FIVE)
ENUM_FN(KEY_SIX,KEY_SIX)
ENUM_FN(KEY_SEVEN,KEY_SEVEN)
ENUM_FN(KEY_EIGHT,KEY_EIGHT)
ENUM_FN(KEY_NINE,KEY_NINE)
ENUM_FN(KEY_SEMICOLON,KEY_SEMICOLON)
ENUM_FN(KEY_EQUAL,KEY_EQUAL)
ENUM_FN(KEY_B,KEY_B)
ENUM_FN(KEY_C,KEY_C)
ENUM_FN(KEY_E,KEY_E)
ENUM_FN(KEY_F,KEY_F)
ENUM_FN(KEY_G,KEY_G)
ENUM_FN(KEY_H,KEY_H)
ENUM_FN(KEY_I,KEY_I)
ENUM_FN(KEY_J,KEY_J)
ENUM_FN(KEY_K,KEY_K)
ENUM_FN(KEY_L,KEY_L)
ENUM_FN(KEY_M,KEY_M)
ENUM_FN(KEY_N,KEY_N)
ENUM_FN(KEY_O,KEY_O)
ENUM_FN(KEY_P,KEY_P)
ENUM_FN(KEY_Q,KEY_Q)
ENUM_FN(KEY_R,KEY_R)
ENUM_FN(KEY_T,KEY_T)
ENUM_FN(KEY_U,KEY_U)
ENUM_FN(KEY_V,KEY_V)
ENUM_FN(KEY_X,KEY_X)
ENUM_FN(KEY_Y,KEY_Y)
ENUM_FN(KEY_LEFT_BRACKET,KEY_LEFT_BRACKET)
ENUM_FN(KEY_BACKSLASH,KEY_BACKSLASH)
ENUM_FN(KEY_RIGHT_BRACKET,KEY_RIGHT_BRACKET)
ENUM_FN(KEY_GRAVE,KEY_GRAVE)
ENUM_FN(KEY_ENTER,KEY_ENTER)
ENUM_FN(KEY_TAB,KEY_TAB)
ENUM_FN(KEY_BACKSPACE,KEY_BACKSPACE)
ENUM_FN(KEY_INSERT,KEY_INSERT)
ENUM_FN(KEY_DELETE,KEY_DELETE)
ENUM_FN(KEY_RIGHT,KEY_RIGHT)
ENUM_FN(KEY_LEFT,KEY_LEFT)
ENUM_FN(KEY_DOWN,KEY_DOWN)
ENUM_FN(KEY_UP,KEY_UP)
ENUM_FN(KEY_PAGE_UP,KEY_PAGE_UP)
ENUM_FN(KEY_PAGE_DOWN,KEY_PAGE_DOWN)
ENUM_FN(KEY_HOME,KEY_HOME)
ENUM_FN(KEY_END,KEY_END)
ENUM_FN(KEY_CAPS_LOCK,KEY_CAPS_LOCK)
ENUM_FN(KEY_SCROLL_LOCK,KEY_SCROLL_LOCK)
ENUM_FN(KEY_NUM_LOCK,KEY_NUM_LOCK)
ENUM_FN(KEY_PRINT_SCREEN,KEY_PRINT_SCREEN)
ENUM_FN(KEY_PAUSE,KEY_PAUSE)
ENUM_FN(KEY_F1,KEY_F1)
ENUM_FN(KEY_F2,KEY_F2)
ENUM_FN(KEY_F3,KEY_F3)
ENUM_FN(KEY_F4,KEY_F4)
ENUM_FN(KEY_F5,KEY_F5)
ENUM_FN(KEY_F6,KEY_F6)
ENUM_FN(KEY_F7,KEY_F7)
ENUM_FN(KEY_F8,KEY_F8)
ENUM_FN(KEY_F9,KEY_F9)
ENUM_FN(KEY_F10,KEY_F10)
ENUM_FN(KEY_F11,KEY_F11)
ENUM_FN(KEY_F12,KEY_F12)
ENUM_FN(KEY_LEFT_SHIFT,KEY_LEFT_SHIFT)
ENUM_FN(KEY_LEFT_CONTROL,KEY_LEFT_CONTROL)
ENUM_FN(KEY_LEFT_ALT,KEY_LEFT_ALT)
ENUM_FN(KEY_LEFT_SUPER,KEY_LEFT_SUPER)
ENUM_FN(KEY_RIGHT_SHIFT,KEY_RIGHT_SHIFT)
ENUM_FN(KEY_RIGHT_CONTROL,KEY_RIGHT_CONTROL)
ENUM_FN(KEY_RIGHT_ALT,KEY_RIGHT_ALT)
ENUM_FN(KEY_RIGHT_SUPER,KEY_RIGHT_SUPER)
ENUM_FN(KEY_KB_MENU,KEY_KB_MENU)
ENUM_FN(KEY_KP_0,KEY_KP_0)
ENUM_FN(KEY_KP_1,KEY_KP_1)
ENUM_FN(KEY_KP_2,KEY_KP_2)
ENUM_FN(KEY_KP_3,KEY_KP_3)
ENUM_FN(KEY_KP_4,KEY_KP_4)
ENUM_FN(KEY_KP_5,KEY_KP_5)
ENUM_FN(KEY_KP_6,KEY_KP_6)
ENUM_FN(KEY_KP_7,KEY_KP_7)
ENUM_FN(KEY_KP_8,KEY_KP_8)
ENUM_FN(KEY_KP_9,KEY_KP_9)
ENUM_FN(KEY_KP_DECIMAL,KEY_KP_DECIMAL)
ENUM_FN(KEY_KP_DIVIDE,KEY_KP_DIVIDE)
ENUM_FN(KEY_KP_MULTIPLY,KEY_KP_MULTIPLY)
ENUM_FN(KEY_KP_SUBTRACT,KEY_KP_SUBTRACT)
ENUM_FN(KEY_KP_ADD,KEY_KP_ADD)
ENUM_FN(KEY_KP_ENTER,KEY_KP_ENTER)
ENUM_FN(KEY_KP_EQUAL,KEY_KP_EQUAL)
ENUM_FN(KEY_BACK,KEY_BACK)
ENUM_FN(KEY_MENU,KEY_MENU)
ENUM_FN(KEY_VOLUME_UP,KEY_VOLUME_UP)
ENUM_FN(KEY_VOLUME_DOWN,KEY_VOLUME_DOWN)

/* =========================================================================
   CONSTANTS — MOUSE BUTTONS / CURSORS
   ========================================================================= */
ENUM_FN(MOUSE_BUTTON_SIDE,MOUSE_BUTTON_SIDE)
ENUM_FN(MOUSE_BUTTON_EXTRA,MOUSE_BUTTON_EXTRA)
ENUM_FN(MOUSE_BUTTON_FORWARD,MOUSE_BUTTON_FORWARD)
ENUM_FN(MOUSE_BUTTON_BACK,MOUSE_BUTTON_BACK)
ENUM_FN(MOUSE_LEFT_BUTTON,MOUSE_LEFT_BUTTON)
ENUM_FN(MOUSE_RIGHT_BUTTON,MOUSE_RIGHT_BUTTON)
ENUM_FN(MOUSE_MIDDLE_BUTTON,MOUSE_MIDDLE_BUTTON)

ENUM_FN(MOUSE_CURSOR_DEFAULT,MOUSE_CURSOR_DEFAULT)
ENUM_FN(MOUSE_CURSOR_ARROW,MOUSE_CURSOR_ARROW)
ENUM_FN(MOUSE_CURSOR_IBEAM,MOUSE_CURSOR_IBEAM)
ENUM_FN(MOUSE_CURSOR_CROSSHAIR,MOUSE_CURSOR_CROSSHAIR)
ENUM_FN(MOUSE_CURSOR_POINTING_HAND,MOUSE_CURSOR_POINTING_HAND)
ENUM_FN(MOUSE_CURSOR_RESIZE_EW,MOUSE_CURSOR_RESIZE_EW)
ENUM_FN(MOUSE_CURSOR_RESIZE_NS,MOUSE_CURSOR_RESIZE_NS)
ENUM_FN(MOUSE_CURSOR_RESIZE_NWSE,MOUSE_CURSOR_RESIZE_NWSE)
ENUM_FN(MOUSE_CURSOR_RESIZE_NESW,MOUSE_CURSOR_RESIZE_NESW)
ENUM_FN(MOUSE_CURSOR_RESIZE_ALL,MOUSE_CURSOR_RESIZE_ALL)
ENUM_FN(MOUSE_CURSOR_NOT_ALLOWED,MOUSE_CURSOR_NOT_ALLOWED)

/* =========================================================================
   CONSTANTS — GAMEPAD BUTTONS / AXES
   ========================================================================= */
ENUM_FN(GAMEPAD_BUTTON_UNKNOWN,GAMEPAD_BUTTON_UNKNOWN)
ENUM_FN(GAMEPAD_BUTTON_LEFT_FACE_UP,GAMEPAD_BUTTON_LEFT_FACE_UP)
ENUM_FN(GAMEPAD_BUTTON_LEFT_FACE_RIGHT,GAMEPAD_BUTTON_LEFT_FACE_RIGHT)
ENUM_FN(GAMEPAD_BUTTON_LEFT_FACE_DOWN,GAMEPAD_BUTTON_LEFT_FACE_DOWN)
ENUM_FN(GAMEPAD_BUTTON_LEFT_FACE_LEFT,GAMEPAD_BUTTON_LEFT_FACE_LEFT)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_FACE_UP,GAMEPAD_BUTTON_RIGHT_FACE_UP)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_FACE_DOWN,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_FACE_LEFT,GAMEPAD_BUTTON_RIGHT_FACE_LEFT)
ENUM_FN(GAMEPAD_BUTTON_LEFT_TRIGGER_1,GAMEPAD_BUTTON_LEFT_TRIGGER_1)
ENUM_FN(GAMEPAD_BUTTON_LEFT_TRIGGER_2,GAMEPAD_BUTTON_LEFT_TRIGGER_2)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_TRIGGER_1,GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_TRIGGER_2,GAMEPAD_BUTTON_RIGHT_TRIGGER_2)
ENUM_FN(GAMEPAD_BUTTON_MIDDLE_LEFT,GAMEPAD_BUTTON_MIDDLE_LEFT)
ENUM_FN(GAMEPAD_BUTTON_MIDDLE,GAMEPAD_BUTTON_MIDDLE)
ENUM_FN(GAMEPAD_BUTTON_MIDDLE_RIGHT,GAMEPAD_BUTTON_MIDDLE_RIGHT)
ENUM_FN(GAMEPAD_BUTTON_LEFT_THUMB,GAMEPAD_BUTTON_LEFT_THUMB)
ENUM_FN(GAMEPAD_BUTTON_RIGHT_THUMB,GAMEPAD_BUTTON_RIGHT_THUMB)
ENUM_FN(GAMEPAD_AXIS_LEFT_X,GAMEPAD_AXIS_LEFT_X)
ENUM_FN(GAMEPAD_AXIS_LEFT_Y,GAMEPAD_AXIS_LEFT_Y)
ENUM_FN(GAMEPAD_AXIS_RIGHT_X,GAMEPAD_AXIS_RIGHT_X)
ENUM_FN(GAMEPAD_AXIS_RIGHT_Y,GAMEPAD_AXIS_RIGHT_Y)
ENUM_FN(GAMEPAD_AXIS_LEFT_TRIGGER,GAMEPAD_AXIS_LEFT_TRIGGER)
ENUM_FN(GAMEPAD_AXIS_RIGHT_TRIGGER,GAMEPAD_AXIS_RIGHT_TRIGGER)

/* =========================================================================
   CONSTANTS — MATERIAL MAPS / SHADER LOCATIONS / UNIFORM + ATTRIB TYPES
   ========================================================================= */
ENUM_FN(MATERIAL_MAP_ALBEDO,MATERIAL_MAP_ALBEDO)
ENUM_FN(MATERIAL_MAP_METALNESS,MATERIAL_MAP_METALNESS)
ENUM_FN(MATERIAL_MAP_NORMAL,MATERIAL_MAP_NORMAL)
ENUM_FN(MATERIAL_MAP_ROUGHNESS,MATERIAL_MAP_ROUGHNESS)
ENUM_FN(MATERIAL_MAP_OCCLUSION,MATERIAL_MAP_OCCLUSION)
ENUM_FN(MATERIAL_MAP_EMISSION,MATERIAL_MAP_EMISSION)
ENUM_FN(MATERIAL_MAP_HEIGHT,MATERIAL_MAP_HEIGHT)
ENUM_FN(MATERIAL_MAP_CUBEMAP,MATERIAL_MAP_CUBEMAP)
ENUM_FN(MATERIAL_MAP_IRRADIANCE,MATERIAL_MAP_IRRADIANCE)
ENUM_FN(MATERIAL_MAP_PREFILTER,MATERIAL_MAP_PREFILTER)
ENUM_FN(MATERIAL_MAP_BRDF,MATERIAL_MAP_BRDF)
ENUM_FN(MATERIAL_MAP_DIFFUSE,MATERIAL_MAP_DIFFUSE)
ENUM_FN(MATERIAL_MAP_SPECULAR,MATERIAL_MAP_SPECULAR)

ENUM_FN(SHADER_LOC_VERTEX_POSITION,SHADER_LOC_VERTEX_POSITION)
ENUM_FN(SHADER_LOC_VERTEX_TEXCOORD01,SHADER_LOC_VERTEX_TEXCOORD01)
ENUM_FN(SHADER_LOC_VERTEX_TEXCOORD02,SHADER_LOC_VERTEX_TEXCOORD02)
ENUM_FN(SHADER_LOC_VERTEX_NORMAL,SHADER_LOC_VERTEX_NORMAL)
ENUM_FN(SHADER_LOC_VERTEX_TANGENT,SHADER_LOC_VERTEX_TANGENT)
ENUM_FN(SHADER_LOC_VERTEX_COLOR,SHADER_LOC_VERTEX_COLOR)
ENUM_FN(SHADER_LOC_MATRIX_MVP,SHADER_LOC_MATRIX_MVP)
ENUM_FN(SHADER_LOC_MATRIX_VIEW,SHADER_LOC_MATRIX_VIEW)
ENUM_FN(SHADER_LOC_MATRIX_PROJECTION,SHADER_LOC_MATRIX_PROJECTION)
ENUM_FN(SHADER_LOC_MATRIX_MODEL,SHADER_LOC_MATRIX_MODEL)
ENUM_FN(SHADER_LOC_MATRIX_NORMAL,SHADER_LOC_MATRIX_NORMAL)
ENUM_FN(SHADER_LOC_VECTOR_VIEW,SHADER_LOC_VECTOR_VIEW)
ENUM_FN(SHADER_LOC_COLOR_DIFFUSE,SHADER_LOC_COLOR_DIFFUSE)
ENUM_FN(SHADER_LOC_COLOR_SPECULAR,SHADER_LOC_COLOR_SPECULAR)
ENUM_FN(SHADER_LOC_COLOR_AMBIENT,SHADER_LOC_COLOR_AMBIENT)
ENUM_FN(SHADER_LOC_MAP_ALBEDO,SHADER_LOC_MAP_ALBEDO)
ENUM_FN(SHADER_LOC_MAP_METALNESS,SHADER_LOC_MAP_METALNESS)
ENUM_FN(SHADER_LOC_MAP_NORMAL,SHADER_LOC_MAP_NORMAL)
ENUM_FN(SHADER_LOC_MAP_ROUGHNESS,SHADER_LOC_MAP_ROUGHNESS)
ENUM_FN(SHADER_LOC_MAP_OCCLUSION,SHADER_LOC_MAP_OCCLUSION)
ENUM_FN(SHADER_LOC_MAP_EMISSION,SHADER_LOC_MAP_EMISSION)
ENUM_FN(SHADER_LOC_MAP_HEIGHT,SHADER_LOC_MAP_HEIGHT)
ENUM_FN(SHADER_LOC_MAP_CUBEMAP,SHADER_LOC_MAP_CUBEMAP)
ENUM_FN(SHADER_LOC_MAP_IRRADIANCE,SHADER_LOC_MAP_IRRADIANCE)
ENUM_FN(SHADER_LOC_MAP_PREFILTER,SHADER_LOC_MAP_PREFILTER)
ENUM_FN(SHADER_LOC_MAP_BRDF,SHADER_LOC_MAP_BRDF)
ENUM_FN(SHADER_LOC_VERTEX_BONEIDS,SHADER_LOC_VERTEX_BONEIDS)
ENUM_FN(SHADER_LOC_VERTEX_BONEWEIGHTS,SHADER_LOC_VERTEX_BONEWEIGHTS)
ENUM_FN(SHADER_LOC_MATRIX_BONETRANSFORMS,SHADER_LOC_MATRIX_BONETRANSFORMS)
ENUM_FN(SHADER_LOC_VERTEX_INSTANCETRANSFORM,SHADER_LOC_VERTEX_INSTANCETRANSFORM)
ENUM_FN(SHADER_LOC_MAP_DIFFUSE,SHADER_LOC_MAP_DIFFUSE)
ENUM_FN(SHADER_LOC_MAP_SPECULAR,SHADER_LOC_MAP_SPECULAR)

ENUM_FN(SHADER_UNIFORM_FLOAT,SHADER_UNIFORM_FLOAT)
ENUM_FN(SHADER_UNIFORM_VEC2,SHADER_UNIFORM_VEC2)
ENUM_FN(SHADER_UNIFORM_VEC3,SHADER_UNIFORM_VEC3)
ENUM_FN(SHADER_UNIFORM_VEC4,SHADER_UNIFORM_VEC4)
ENUM_FN(SHADER_UNIFORM_INT,SHADER_UNIFORM_INT)
ENUM_FN(SHADER_UNIFORM_IVEC2,SHADER_UNIFORM_IVEC2)
ENUM_FN(SHADER_UNIFORM_IVEC3,SHADER_UNIFORM_IVEC3)
ENUM_FN(SHADER_UNIFORM_IVEC4,SHADER_UNIFORM_IVEC4)
ENUM_FN(SHADER_UNIFORM_UINT,SHADER_UNIFORM_UINT)
ENUM_FN(SHADER_UNIFORM_UIVEC2,SHADER_UNIFORM_UIVEC2)
ENUM_FN(SHADER_UNIFORM_UIVEC3,SHADER_UNIFORM_UIVEC3)
ENUM_FN(SHADER_UNIFORM_UIVEC4,SHADER_UNIFORM_UIVEC4)
ENUM_FN(SHADER_UNIFORM_SAMPLER2D,SHADER_UNIFORM_SAMPLER2D)

ENUM_FN(SHADER_ATTRIB_FLOAT,SHADER_ATTRIB_FLOAT)
ENUM_FN(SHADER_ATTRIB_VEC2,SHADER_ATTRIB_VEC2)
ENUM_FN(SHADER_ATTRIB_VEC3,SHADER_ATTRIB_VEC3)
ENUM_FN(SHADER_ATTRIB_VEC4,SHADER_ATTRIB_VEC4)

/* =========================================================================
   CONSTANTS — PIXEL FORMATS / TEXTURE FILTER + WRAP / CUBEMAP / FONT /
                BLEND MODES / GESTURES / NPATCH LAYOUT
   ========================================================================= */
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R5G6B5,PIXELFORMAT_UNCOMPRESSED_R5G6B5)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R8G8B8,PIXELFORMAT_UNCOMPRESSED_R8G8B8)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,PIXELFORMAT_UNCOMPRESSED_R5G5B5A1)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,PIXELFORMAT_UNCOMPRESSED_R4G4B4A4)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R32,PIXELFORMAT_UNCOMPRESSED_R32)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R32G32B32,PIXELFORMAT_UNCOMPRESSED_R32G32B32)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R16,PIXELFORMAT_UNCOMPRESSED_R16)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R16G16B16,PIXELFORMAT_UNCOMPRESSED_R16G16B16)
ENUM_FN(PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,PIXELFORMAT_UNCOMPRESSED_R16G16B16A16)
ENUM_FN(PIXELFORMAT_COMPRESSED_DXT1_RGB,PIXELFORMAT_COMPRESSED_DXT1_RGB)
ENUM_FN(PIXELFORMAT_COMPRESSED_DXT1_RGBA,PIXELFORMAT_COMPRESSED_DXT1_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_DXT3_RGBA,PIXELFORMAT_COMPRESSED_DXT3_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_DXT5_RGBA,PIXELFORMAT_COMPRESSED_DXT5_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_ETC1_RGB,PIXELFORMAT_COMPRESSED_ETC1_RGB)
ENUM_FN(PIXELFORMAT_COMPRESSED_ETC2_RGB,PIXELFORMAT_COMPRESSED_ETC2_RGB)
ENUM_FN(PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA,PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_PVRT_RGB,PIXELFORMAT_COMPRESSED_PVRT_RGB)
ENUM_FN(PIXELFORMAT_COMPRESSED_PVRT_RGBA,PIXELFORMAT_COMPRESSED_PVRT_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA,PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA)
ENUM_FN(PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA,PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA)

ENUM_FN(TEXTURE_FILTER_POINT,TEXTURE_FILTER_POINT)
ENUM_FN(TEXTURE_FILTER_BILINEAR,TEXTURE_FILTER_BILINEAR)
ENUM_FN(TEXTURE_FILTER_TRILINEAR,TEXTURE_FILTER_TRILINEAR)
ENUM_FN(TEXTURE_FILTER_ANISOTROPIC_4X,TEXTURE_FILTER_ANISOTROPIC_4X)
ENUM_FN(TEXTURE_FILTER_ANISOTROPIC_8X,TEXTURE_FILTER_ANISOTROPIC_8X)
ENUM_FN(TEXTURE_FILTER_ANISOTROPIC_16X,TEXTURE_FILTER_ANISOTROPIC_16X)

ENUM_FN(TEXTURE_WRAP_REPEAT,TEXTURE_WRAP_REPEAT)
ENUM_FN(TEXTURE_WRAP_CLAMP,TEXTURE_WRAP_CLAMP)
ENUM_FN(TEXTURE_WRAP_MIRROR_REPEAT,TEXTURE_WRAP_MIRROR_REPEAT)
ENUM_FN(TEXTURE_WRAP_MIRROR_CLAMP,TEXTURE_WRAP_MIRROR_CLAMP)

ENUM_FN(CUBEMAP_LAYOUT_AUTO_DETECT,CUBEMAP_LAYOUT_AUTO_DETECT)
ENUM_FN(CUBEMAP_LAYOUT_LINE_VERTICAL,CUBEMAP_LAYOUT_LINE_VERTICAL)
ENUM_FN(CUBEMAP_LAYOUT_LINE_HORIZONTAL,CUBEMAP_LAYOUT_LINE_HORIZONTAL)
ENUM_FN(CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR)
ENUM_FN(CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE,CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE)

ENUM_FN(FONT_DEFAULT,FONT_DEFAULT)
ENUM_FN(FONT_BITMAP,FONT_BITMAP)
ENUM_FN(FONT_SDF,FONT_SDF)

ENUM_FN(BLEND_ALPHA,BLEND_ALPHA)
ENUM_FN(BLEND_ADDITIVE,BLEND_ADDITIVE)
ENUM_FN(BLEND_MULTIPLIED,BLEND_MULTIPLIED)
ENUM_FN(BLEND_ADD_COLORS,BLEND_ADD_COLORS)
ENUM_FN(BLEND_SUBTRACT_COLORS,BLEND_SUBTRACT_COLORS)
ENUM_FN(BLEND_ALPHA_PREMULTIPLY,BLEND_ALPHA_PREMULTIPLY)
ENUM_FN(BLEND_CUSTOM,BLEND_CUSTOM)
ENUM_FN(BLEND_CUSTOM_SEPARATE,BLEND_CUSTOM_SEPARATE)

ENUM_FN(GESTURE_NONE,GESTURE_NONE)
ENUM_FN(GESTURE_TAP,GESTURE_TAP)
ENUM_FN(GESTURE_DOUBLETAP,GESTURE_DOUBLETAP)
ENUM_FN(GESTURE_HOLD,GESTURE_HOLD)
ENUM_FN(GESTURE_DRAG,GESTURE_DRAG)
ENUM_FN(GESTURE_SWIPE_RIGHT,GESTURE_SWIPE_RIGHT)
ENUM_FN(GESTURE_SWIPE_LEFT,GESTURE_SWIPE_LEFT)
ENUM_FN(GESTURE_SWIPE_UP,GESTURE_SWIPE_UP)
ENUM_FN(GESTURE_SWIPE_DOWN,GESTURE_SWIPE_DOWN)
ENUM_FN(GESTURE_PINCH_IN,GESTURE_PINCH_IN)
ENUM_FN(GESTURE_PINCH_OUT,GESTURE_PINCH_OUT)

ENUM_FN(NPATCH_NINE_PATCH,NPATCH_NINE_PATCH)
ENUM_FN(NPATCH_THREE_PATCH_VERTICAL,NPATCH_THREE_PATCH_VERTICAL)
ENUM_FN(NPATCH_THREE_PATCH_HORIZONTAL,NPATCH_THREE_PATCH_HORIZONTAL)

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
/* ---------- Point-list helpers ----------
   Functions that take `const Vector2 *points, int pointCount` receive the count
   first and then the flattened coordinates (`count, x0,y0, x1,y1, ...`), which
   is the only way to express an array across the module ABI. */
#define GCL_MAX_POINTS 256
static int gcl_read_points2(int argc,const char**argv,int count_idx,Vector2 *out,int max_pts){
    int count = (count_idx < argc) ? ii(argv[count_idx]) : 0;
    if (count < 0) count = 0;
    if (count > max_pts) count = max_pts;
    for (int i = 0; i < count; i++) {
        int b = count_idx + 1 + i * 2;
        out[i] = (Vector2){ (b < argc) ? ff(argv[b]) : 0.0f,
                            (b + 1 < argc) ? ff(argv[b + 1]) : 0.0f };
    }
    return count;
}
static int gcl_read_points3(int argc,const char**argv,int count_idx,Vector3 *out,int max_pts){
    int count = (count_idx < argc) ? ii(argv[count_idx]) : 0;
    if (count < 0) count = 0;
    if (count > max_pts) count = max_pts;
    for (int i = 0; i < count; i++) {
        int b = count_idx + 1 + i * 3;
        out[i] = (Vector3){ (b < argc) ? ff(argv[b]) : 0.0f,
                            (b + 1 < argc) ? ff(argv[b + 1]) : 0.0f,
                            (b + 2 < argc) ? ff(argv[b + 2]) : 0.0f };
    }
    return count;
}
/* DrawLineStrip(pointCount, x0,y0, x1,y1, ..., Color) */
static double fn_DrawLineStrip(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int n = gcl_read_points2(argc, argv, 0, pts, GCL_MAX_POINTS);
    if (n > 1) DrawLineStrip(pts, n, ci(argv, 1 + n * 2));
    return 0.0;
}
static double fn_DrawLineBezier(int argc,const char**argv){ DrawLineBezier(v2_arg(argv,0),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawLineDashed(int argc,const char**argv){ DrawLineDashed(v2_arg(argv,0),v2_arg(argv,2),ii(argv[4]),ii(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawTriangle(int argc,const char**argv){ DrawTriangle(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ci(argv,6)); return 0.0; }
static double fn_DrawTriangleLines(int argc,const char**argv){ DrawTriangleLines(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ci(argv,6)); return 0.0; }
/* DrawTriangleFan(pointCount, x0,y0, ..., Color) */
static double fn_DrawTriangleFan(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int n = gcl_read_points2(argc, argv, 0, pts, GCL_MAX_POINTS);
    if (n > 2) DrawTriangleFan(pts, n, ci(argv, 1 + n * 2));
    return 0.0;
}
/* DrawTriangleStrip(pointCount, x0,y0, ..., Color) */
static double fn_DrawTriangleStrip(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int n = gcl_read_points2(argc, argv, 0, pts, GCL_MAX_POINTS);
    if (n > 2) DrawTriangleStrip(pts, n, ci(argv, 1 + n * 2));
    return 0.0;
}
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
/* Every DrawSpline* member takes (pointCount, x0,y0, ..., thick, Color). */
#define GCL_SPLINE_FN(NAME, CALL)                                              \
static double fn_##NAME(int argc,const char**argv){                            \
    static Vector2 pts[GCL_MAX_POINTS];                                        \
    int n = gcl_read_points2(argc, argv, 0, pts, GCL_MAX_POINTS);              \
    if (n > 1) CALL(pts, n, ff(argv[1 + n * 2]), ci(argv, 2 + n * 2));         \
    return 0.0;                                                                \
}
GCL_SPLINE_FN(DrawSplineLinear, DrawSplineLinear)
GCL_SPLINE_FN(DrawSplineBasis, DrawSplineBasis)
GCL_SPLINE_FN(DrawSplineCatmullRom, DrawSplineCatmullRom)
GCL_SPLINE_FN(DrawSplineBezierQuadratic, DrawSplineBezierQuadratic)
GCL_SPLINE_FN(DrawSplineBezierCubic, DrawSplineBezierCubic)
#undef GCL_SPLINE_FN
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
/* CheckCollisionPointPoly(pointX, pointY, pointCount, x0,y0, ...) */
static double fn_CheckCollisionPointPoly(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    Vector2 p = v2_arg(argv, 0);
    int n = gcl_read_points2(argc, argv, 2, pts, GCL_MAX_POINTS);
    if (n <= 0) return 0.0;
    return CheckCollisionPointPoly(p, pts, n) ? 1.0 : 0.0;
}
/* CheckCollisionLines(s1x,s1y, e1x,e1y, s2x,s2y, e2x,e2y) — the intersection
   point raylib writes through the out-parameter lands in g_last_v2. */
static double fn_CheckCollisionLines(int argc,const char**argv){
    Vector2 hit_point = {0.0f, 0.0f};
    bool hit = CheckCollisionLines(v2_arg(argv, 0), v2_arg(argv, 2),
                                   v2_arg(argv, 4), v2_arg(argv, 6), &hit_point);
    g_last_v2 = hit_point;
    return hit ? 1.0 : 0.0;
}
static double fn_GetCollisionRec(int argc,const char**argv){ g_last_rect=GetCollisionRec(rect_arg(argv,0),rect_arg(argv,4)); return 0.0; }

/* =========================================================================
   TEXTURES / IMAGES
   ========================================================================= */
static double fn_LoadImage(int argc,const char**argv){ g_last_handle=reg_img(LoadImage(ss(argv[0]))); return (double)g_last_handle; }
/* LoadImageRaw(fileName, width, height, format, headerSize): raylib zaten
   dosya yolunu alır; önceki sürüm boş bir Image kaydediyordu. */
static double fn_LoadImageRaw(int argc,const char**argv){
    Image img = LoadImageRaw(ss((argc > 0) ? argv[0] : ""),
                             (argc > 1) ? ii(argv[1]) : 0,
                             (argc > 2) ? ii(argv[2]) : 0,
                             (argc > 3) ? ii(argv[3]) : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                             (argc > 4) ? ii(argv[4]) : 0);
    g_last_handle = reg_img(img);
    return (double)g_last_handle;
}
/* LoadImageAnim(fileName) -> Image handle; KARE SAYISI g_last_int'te
   (Raylib.LastInt()). raylib `int *frames` out-parametresi ister. */
static double fn_LoadImageAnim(int argc,const char**argv){
    int frames = 0;
    Image img = LoadImageAnim(ss((argc > 0) ? argv[0] : ""), &frames);
    g_last_int = frames;
    g_last_handle = reg_img(img);
    return (double)g_last_handle;
}
/* LoadImageAnimFromMemory(fileName, fileType) -> Image handle; kare sayısı
   g_last_int'te. raylib `unsigned char *fileData` ister; GCL'de bayt dizisi
   kanalı olmadığı için modül dosyayı okur ve dosya tipini AÇIKÇA kullanır
   (uzantısı raylib'in tanımadığı dosyalar için). */
static double fn_LoadImageAnimFromMemory(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    const char *dot = strrchr(file, '.');
    const char *type = (argc > 1 && argv[1] && argv[1][0]) ? ss(argv[1]) : (dot ? dot : "");
    int dataSize = 0;
    int frames = 0;
    Image img = (Image){0};
    unsigned char *data = LoadFileData(file, &dataSize);
    if (data) {
        img = LoadImageAnimFromMemory(type, data, dataSize, &frames);
        UnloadFileData(data);
    }
    g_last_int = frames;
    g_last_handle = reg_img(img);
    return (double)g_last_handle;
}
/* LoadImageFromMemory(fileName, fileType) -> Image handle.
   raylib `unsigned char *fileData` ister; GCL'de bayt dizisi kanalı olmadığı
   için modül dosyayı okur ve dosya tipini AÇIKÇA kullanır. Böylece uzantısı
   raylib'in tanımadığı bir dosyadan (ör. gömülü bir .dat) da görüntü
   yüklenebilir — LoadImage'ın yapamadığı tek şey budur. */
static double fn_LoadImageFromMemory(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    const char *dot = strrchr(file, '.');
    const char *type = (argc > 1 && argv[1] && argv[1][0]) ? ss(argv[1]) : (dot ? dot : "");
    int dataSize = 0;
    Image img = (Image){0};
    unsigned char *data = LoadFileData(file, &dataSize);
    if (data) {
        img = LoadImageFromMemory(type, data, dataSize);
        UnloadFileData(data);
    }
    g_last_handle = reg_img(img);
    return (double)g_last_handle;
}
static double fn_LoadImageFromTexture(int argc,const char**argv){ g_last_handle=reg_img(LoadImageFromTexture(get_tex(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadImageFromScreen(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_img(LoadImageFromScreen());return (double)g_last_handle;}
static double fn_IsImageValid(int argc,const char**argv){ return IsImageValid(get_img(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadImage(int argc,const char**argv){ UnloadImage(get_img(ii(argv[0]))); return 0.0; }
static double fn_ExportImage(int argc,const char**argv){ return ExportImage(get_img(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
/* ExportImageToMemory(imageHandle, fileType) -> üretilen bayt sayısı; baytlar
   g_str'de HEX olarak (raylib `unsigned char *` döndürür ve çağıran bırakır;
   modül kopyaladıktan sonra MemFree eder). */
static double fn_ExportImageToMemory(int argc,const char**argv){
    int size = 0;
    unsigned char *data = ExportImageToMemory(get_img((argc > 0) ? ii(argv[0]) : -1),
                                              ss((argc > 1) ? argv[1] : "png"),
                                              &size);
    if (!data) { g_str[0] = 0; return 0.0; }
    gcl_bytes_to_hex(data, size);
    MemFree(data);
    return (double)size;
}
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
/* In-place Image operations. raylib takes an `Image *` and rewrites the struct
   in place; GCL passes the registry HANDLE and the mutated struct is stored
   back into the slot, so the script keeps using the same handle afterwards. */
#define GCL_IMG_MUT(NAME, ...)                                                 \
static double fn_##NAME(int argc,const char**argv){                            \
    int h = (argc > 0) ? ii(argv[0]) : -1;                                     \
    Image img = get_img(h);                                                    \
    if (h >= 0 && h < g_img_n) { __VA_ARGS__; g_img_h[h] = img; }              \
    return 0.0;                                                                \
}
GCL_IMG_MUT(ImageFormat, ImageFormat(&img, ii(argv[1])))
GCL_IMG_MUT(ImageToPOT, ImageToPOT(&img, ci(argv, 1)))
GCL_IMG_MUT(ImageCrop, ImageCrop(&img, rect_arg(argv, 1)))
GCL_IMG_MUT(ImageAlphaCrop, ImageAlphaCrop(&img, ff(argv[1])))
GCL_IMG_MUT(ImageAlphaClear, ImageAlphaClear(&img, ci(argv, 1), ff(argv[2])))
GCL_IMG_MUT(ImageAlphaMask, ImageAlphaMask(&img, get_img(ii(argv[1]))))
GCL_IMG_MUT(ImageAlphaPremultiply, ImageAlphaPremultiply(&img))
GCL_IMG_MUT(ImageBlurGaussian, ImageBlurGaussian(&img, ii(argv[1])))
GCL_IMG_MUT(ImageResize, ImageResize(&img, ii(argv[1]), ii(argv[2])))
GCL_IMG_MUT(ImageResizeNN, ImageResizeNN(&img, ii(argv[1]), ii(argv[2])))
GCL_IMG_MUT(ImageResizeCanvas, ImageResizeCanvas(&img, ii(argv[1]), ii(argv[2]),
                                                 ii(argv[3]), ii(argv[4]), ci(argv, 5)))
GCL_IMG_MUT(ImageMipmaps, ImageMipmaps(&img))
GCL_IMG_MUT(ImageDither, ImageDither(&img, ii(argv[1]), ii(argv[2]), ii(argv[3]), ii(argv[4])))
GCL_IMG_MUT(ImageFlipVertical, ImageFlipVertical(&img))
GCL_IMG_MUT(ImageFlipHorizontal, ImageFlipHorizontal(&img))
GCL_IMG_MUT(ImageRotate, ImageRotate(&img, ii(argv[1])))
GCL_IMG_MUT(ImageRotateCW, ImageRotateCW(&img))
GCL_IMG_MUT(ImageRotateCCW, ImageRotateCCW(&img))
GCL_IMG_MUT(ImageColorTint, ImageColorTint(&img, ci(argv, 1)))
GCL_IMG_MUT(ImageColorInvert, ImageColorInvert(&img))
GCL_IMG_MUT(ImageColorGrayscale, ImageColorGrayscale(&img))
GCL_IMG_MUT(ImageColorContrast, ImageColorContrast(&img, ii(argv[1])))
GCL_IMG_MUT(ImageColorBrightness, ImageColorBrightness(&img, ii(argv[1])))
GCL_IMG_MUT(ImageColorReplace, ImageColorReplace(&img, ci(argv, 1), ci(argv, 2)))
#undef GCL_IMG_MUT

/* ImageKernelConvolution(imageHandle, kernelSize, k0, k1, ...) — the C form
   takes a `const float *`; GCL supplies the size followed by the weights. */
static double fn_ImageKernelConvolution(int argc,const char**argv){
    static float kernel[256];
    int h = (argc > 0) ? ii(argv[0]) : -1;
    int ksize = (argc > 1) ? ii(argv[1]) : 0;
    Image img = get_img(h);
    if (ksize < 0) ksize = 0;
    if (ksize > 256) ksize = 256;
    for (int i = 0; i < ksize; i++) kernel[i] = (2 + i < argc) ? ff(argv[2 + i]) : 0.0f;
    if (h >= 0 && h < g_img_n && ksize > 0) {
        ImageKernelConvolution(&img, kernel, ksize);
        g_img_h[h] = img;
    }
    return 0.0;
}

/* Loaded color lists are rendered into g_str as comma separated packed colors,
   because a GCL script has no other way to hold an array of values. */
static void gcl_colors_to_str(const Color *colors, int count){
    int n = 0;
    if (!colors || count <= 0) { g_str[0] = 0; return; }
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + n, sizeof(g_str) - (size_t)n, "%s%u",
                             (i ? "," : ""), color_to_uint(colors[i]));
        if (wrote < 0 || (size_t)(n + wrote) >= sizeof(g_str)) break;
        n += wrote;
    }
}
/* LoadImageColors(imageHandle) -> packed colors in g_str, count returned. */
static double fn_LoadImageColors(int argc,const char**argv){
    Image img = get_img((argc > 0) ? ii(argv[0]) : -1);
    int count = img.width * img.height;
    Color *colors;
    if (count <= 0) { g_str[0] = 0; return 0.0; }
    colors = LoadImageColors(img);
    if (!colors) { g_str[0] = 0; return 0.0; }
    gcl_colors_to_str(colors, count);
    UnloadImageColors(colors);
    return (double)count;
}
/* LoadImagePalette(imageHandle[, maxPaletteSize]) -> packed colors in g_str. */
static double fn_LoadImagePalette(int argc,const char**argv){
    Image img = get_img((argc > 0) ? ii(argv[0]) : -1);
    int max_size = (argc > 1) ? ii(argv[1]) : 256;
    int count = 0;
    Color *palette;
    if (max_size <= 0) { g_str[0] = 0; return 0.0; }
    palette = LoadImagePalette(img, max_size, &count);
    if (!palette) { g_str[0] = 0; return 0.0; }
    gcl_colors_to_str(palette, count);
    UnloadImagePalette(palette);
    return (double)count;
}
/* The module releases color lists itself, so Unload* have nothing left to do.
   They stay as members so the raylib call sequence can be mirrored verbatim. */
/* UnloadImageColors / UnloadImagePalette: renkler g_str'ye virgülle ayrılmış
   paketlenmiş uint olarak kopyalanıp raylib dizisi HEMEN bırakıldığı için
   (bkz. LoadImageColors/LoadImagePalette) GCL tarafında serbest bırakılacak
   bir tahsis kalmıyor. Üyeler API yüzeyi raylib ile eşleşsin diye durur. */
static double fn_UnloadImageColors(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_UnloadImagePalette(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_GetImageAlphaBorder(int argc,const char**argv){ g_last_rect=GetImageAlphaBorder(get_img(ii(argv[0])),ff(argv[1])); return 0.0; }
static double fn_GetImageColor(int argc,const char**argv){ return (double)color_to_uint(GetImageColor(get_img(ii(argv[0])),ii(argv[1]),ii(argv[2]))); }

/* image drawing */
/* CPU-side drawing into an Image. The first argument is always the DESTINATION
   image handle; the pixels live behind img.data, so the struct itself is only
   written back to keep the registry entry authoritative. */
#define GCL_IMG_DRAW(NAME, ...)                                                \
static double fn_##NAME(int argc,const char**argv){                            \
    int h = (argc > 0) ? ii(argv[0]) : -1;                                     \
    Image dst = get_img(h);                                                    \
    if (h >= 0 && h < g_img_n) { __VA_ARGS__; g_img_h[h] = dst; }              \
    return 0.0;                                                                \
}
GCL_IMG_DRAW(ImageClearBackground, ImageClearBackground(&dst, ci(argv, 1)))
GCL_IMG_DRAW(ImageDrawPixel, ImageDrawPixel(&dst, ii(argv[1]), ii(argv[2]), ci(argv, 3)))
GCL_IMG_DRAW(ImageDrawPixelV, ImageDrawPixelV(&dst, v2_arg(argv, 1), ci(argv, 3)))
GCL_IMG_DRAW(ImageDrawLine, ImageDrawLine(&dst, ii(argv[1]), ii(argv[2]),
                                          ii(argv[3]), ii(argv[4]), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawLineV, ImageDrawLineV(&dst, v2_arg(argv, 1), v2_arg(argv, 3), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawLineEx, ImageDrawLineEx(&dst, v2_arg(argv, 1), v2_arg(argv, 3),
                                              ii(argv[5]), ci(argv, 6)))
GCL_IMG_DRAW(ImageDrawTriangle, ImageDrawTriangle(&dst, v2_arg(argv, 1), v2_arg(argv, 3),
                                                  v2_arg(argv, 5), ci(argv, 7)))
GCL_IMG_DRAW(ImageDrawTriangleGradient,
             ImageDrawTriangleGradient(&dst, v2_arg(argv, 1), v2_arg(argv, 3), v2_arg(argv, 5),
                                       ci(argv, 7), ci(argv, 8), ci(argv, 9)))
GCL_IMG_DRAW(ImageDrawTriangleLines, ImageDrawTriangleLines(&dst, v2_arg(argv, 1), v2_arg(argv, 3),
                                                            v2_arg(argv, 5), ci(argv, 7)))
GCL_IMG_DRAW(ImageDrawRectangle, ImageDrawRectangle(&dst, ii(argv[1]), ii(argv[2]),
                                                    ii(argv[3]), ii(argv[4]), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawRectangleV, ImageDrawRectangleV(&dst, v2_arg(argv, 1), v2_arg(argv, 3), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawRectangleRec, ImageDrawRectangleRec(&dst, rect_arg(argv, 1), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawRectanglePro, ImageDrawRectanglePro(&dst, rect_arg(argv, 1),
                                                          v2_arg(argv, 5), ff(argv[7]), ci(argv, 8)))
GCL_IMG_DRAW(ImageDrawRectangleLines, ImageDrawRectangleLines(&dst, ii(argv[1]), ii(argv[2]),
                                                              ii(argv[3]), ii(argv[4]), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawRectangleLinesEx, ImageDrawRectangleLinesEx(&dst, rect_arg(argv, 1),
                                                                  ii(argv[5]), ci(argv, 6)))
GCL_IMG_DRAW(ImageDrawRectangleGradientEx,
             ImageDrawRectangleGradientEx(&dst, rect_arg(argv, 1), ci(argv, 5), ci(argv, 6),
                                          ci(argv, 7), ci(argv, 8)))
GCL_IMG_DRAW(ImageDrawCircle, ImageDrawCircle(&dst, ii(argv[1]), ii(argv[2]), ii(argv[3]), ci(argv, 4)))
GCL_IMG_DRAW(ImageDrawCircleV, ImageDrawCircleV(&dst, v2_arg(argv, 1), ii(argv[3]), ci(argv, 4)))
GCL_IMG_DRAW(ImageDrawCircleLines, ImageDrawCircleLines(&dst, ii(argv[1]), ii(argv[2]),
                                                        ii(argv[3]), ci(argv, 4)))
GCL_IMG_DRAW(ImageDrawCircleLinesV, ImageDrawCircleLinesV(&dst, v2_arg(argv, 1), ii(argv[3]), ci(argv, 4)))
GCL_IMG_DRAW(ImageDrawCircleGradient, ImageDrawCircleGradient(&dst, v2_arg(argv, 1), ff(argv[3]),
                                                              ci(argv, 4), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawImage, ImageDrawImage(&dst, get_img(ii(argv[1])),
                                            ii(argv[2]), ii(argv[3]), ci(argv, 4)))
GCL_IMG_DRAW(ImageDrawImageEx, ImageDrawImageEx(&dst, get_img(ii(argv[1])), v2_arg(argv, 2),
                                                ff(argv[4]), ff(argv[5]), ci(argv, 6)))
GCL_IMG_DRAW(ImageDrawImageRec, ImageDrawImageRec(&dst, get_img(ii(argv[1])), rect_arg(argv, 2),
                                                  v2_arg(argv, 6), ci(argv, 8)))
GCL_IMG_DRAW(ImageDrawImagePro, ImageDrawImagePro(&dst, get_img(ii(argv[1])), rect_arg(argv, 2),
                                                  rect_arg(argv, 6), v2_arg(argv, 10),
                                                  ff(argv[12]), ci(argv, 13)))
GCL_IMG_DRAW(ImageDrawText, ImageDrawText(&dst, ss(argv[1]), ii(argv[2]), ii(argv[3]),
                                          ii(argv[4]), ci(argv, 5)))
GCL_IMG_DRAW(ImageDrawTextEx, ImageDrawTextEx(&dst, get_font(ii(argv[1])), ss(argv[2]),
                                              v2_arg(argv, 3), ff(argv[5]), ff(argv[6]), ci(argv, 7)))
GCL_IMG_DRAW(ImageDrawTextPro, ImageDrawTextPro(&dst, get_font(ii(argv[1])), ss(argv[2]),
                                                v2_arg(argv, 3), v2_arg(argv, 5), ff(argv[7]),
                                                ff(argv[8]), ff(argv[9]), ci(argv, 10)))
#undef GCL_IMG_DRAW

/* The three point-list variants take the destination handle, the point count and
   then the flattened coordinates: (imageHandle, pointCount, x0,y0, ..., Color). */
static double fn_ImageDrawLineStrip(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int h = (argc > 0) ? ii(argv[0]) : -1;
    int n = gcl_read_points2(argc, argv, 1, pts, GCL_MAX_POINTS);
    Image dst = get_img(h);
    if (h >= 0 && h < g_img_n && n > 1) {
        ImageDrawLineStrip(&dst, pts, n, ci(argv, 2 + n * 2));
        g_img_h[h] = dst;
    }
    return 0.0;
}
static double fn_ImageDrawTriangleFan(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int h = (argc > 0) ? ii(argv[0]) : -1;
    int n = gcl_read_points2(argc, argv, 1, pts, GCL_MAX_POINTS);
    Image dst = get_img(h);
    if (h >= 0 && h < g_img_n && n > 2) {
        ImageDrawTriangleFan(&dst, pts, n, ci(argv, 2 + n * 2));
        g_img_h[h] = dst;
    }
    return 0.0;
}
static double fn_ImageDrawTriangleStrip(int argc,const char**argv){
    static Vector2 pts[GCL_MAX_POINTS];
    int h = (argc > 0) ? ii(argv[0]) : -1;
    int n = gcl_read_points2(argc, argv, 1, pts, GCL_MAX_POINTS);
    Image dst = get_img(h);
    if (h >= 0 && h < g_img_n && n > 2) {
        ImageDrawTriangleStrip(&dst, pts, n, ci(argv, 2 + n * 2));
        g_img_h[h] = dst;
    }
    return 0.0;
}

/* texture */
static double fn_LoadTexture(int argc,const char**argv){ g_last_handle=reg_tex(LoadTexture(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadTextureFromImage(int argc,const char**argv){ g_last_handle=reg_tex(LoadTextureFromImage(get_img(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadTextureCubemap(int argc,const char**argv){ g_last_handle=reg_tex(LoadTextureCubemap(get_img(ii(argv[0])),ii(argv[1]))); return (double)g_last_handle; }
static double fn_LoadRenderTexture(int argc,const char**argv){ g_last_handle=reg_rt(LoadRenderTexture(ii(argv[0]),ii(argv[1]))); return (double)g_last_handle; }
static double fn_IsTextureValid(int argc,const char**argv){ return IsTextureValid(get_tex(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadTexture(int argc,const char**argv){ UnloadTexture(get_tex(ii(argv[0]))); return 0.0; }
static double fn_IsRenderTextureValid(int argc,const char**argv){ return IsRenderTextureValid(get_rt(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadRenderTexture(int argc,const char**argv){ UnloadRenderTexture(get_rt(ii(argv[0]))); return 0.0; }
/* UpdateTexture(textureHandle, pixels): `pixels` is the raw byte block as a GCL
   string (see the byte/text bridge note above the file-system section). */
static double fn_UpdateTexture(int argc,const char**argv){
    Texture2D t = get_tex((argc > 0) ? ii(argv[0]) : -1);
    if (argc > 1 && argv[1]) UpdateTexture(t, (const void *)argv[1]);
    return 0.0;
}
/* UpdateTextureRec(textureHandle, x, y, width, height, pixels) */
static double fn_UpdateTextureRec(int argc,const char**argv){
    Texture2D t = get_tex((argc > 0) ? ii(argv[0]) : -1);
    if (argc > 5 && argv[5]) UpdateTextureRec(t, rect_arg(argv, 1), (const void *)argv[5]);
    return 0.0;
}
/* GenTextureMipmaps(textureHandle): raylib takes a Texture2D* and writes the new
   mipmap count back into the struct, so the registry entry is refreshed. */
static double fn_GenTextureMipmaps(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h >= 0 && h < g_tex_n) {
        Texture2D t = g_tex_h[h];
        GenTextureMipmaps(&t);
        g_tex_h[h] = t;
    }
    return 0.0;
}
static double fn_SetTextureFilter(int argc,const char**argv){ SetTextureFilter(get_tex(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_SetTextureWrap(int argc,const char**argv){ SetTextureWrap(get_tex(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_DrawTexture(int argc,const char**argv){ DrawTexture(get_tex(ii(argv[0])),ii(argv[1]),ii(argv[2]),ci(argv,3)); return 0.0; }
static double fn_DrawTextureV(int argc,const char**argv){ DrawTextureV(get_tex(ii(argv[0])),v2_arg(argv,1),ci(argv,3)); return 0.0; }
static double fn_DrawTextureEx(int argc,const char**argv){ DrawTextureEx(get_tex(ii(argv[0])),v2_arg(argv,1),ff(argv[3]),ff(argv[4]),ci(argv,5)); return 0.0; }
static double fn_DrawTextureRec(int argc,const char**argv){ DrawTextureRec(get_tex(ii(argv[0])),rect_arg(argv,1),v2_arg(argv,5),ci(argv,7)); return 0.0; }
static double fn_DrawTexturePro(int argc,const char**argv){ DrawTexturePro(get_tex(ii(argv[0])),rect_arg(argv,1),rect_arg(argv,5),v2_arg(argv,9),ff(argv[11]),ci(argv,12)); return 0.0; }
/* DrawTextureNPatch(textureHandle, nPatchX,nPatchY,nPatchW,nPatchH,left,top,
                     right,bottom,layout, dstX,dstY,dstW,dstH,
                     originX,originY, rotation, Color) */
static double fn_DrawTextureNPatch(int argc,const char**argv){
    Texture2D t = get_tex((argc > 0) ? ii(argv[0]) : -1);
    NPatchInfo np = (NPatchInfo){ rect_arg(argv, 1), ii(argv[5]), ii(argv[6]),
                                  ii(argv[7]), ii(argv[8]), ii(argv[9]) };
    DrawTextureNPatch(t, np, rect_arg(argv, 10), v2_arg(argv, 14),
                      ff(argv[16]), ci(argv, 17));
    return 0.0;
}

/* =========================================================================
   TEXT / FONT
   ========================================================================= */
/* ---------- Kod noktası / metin köprüleri ----------
   raylib'in `const int *codepoints` alan API'leri GCL'den doğrudan
   çağrılamaz (GCL'de tamsayı dizisi değeri yoktur). Modül diziyi çağıran
   adına kurar:
     * metinden -> LoadCodepoints   (Raylib.LoadCodepoints("AB") = "65,66")
     * sayıdan  -> "65,66" biçimindeki GCL metni (Raylib.LoadUTF8("65,66"))
   Bayt ofseti alan varyantlar (GetCodepoint/Next/Previous) kod noktasını
   döndürür ve tükettiği bayt sayısını g_last_int'te yayınlar
   (GCL tarafında Raylib.LastInt()).
   Diziyi modül kurup yine modül bıraktığı için UnloadUTF8 /
   UnloadCodepoints / UnloadTextLines bilinçli olarak no-op'tur: GCL tarafında
   bırakılacak bir tahsis yoktur. */

/* "65,66,67" -> int dizisi. out NULL ise yalnızca sayar. Dönen: öğe sayısı. */
static int gcl_parse_int_list(const char *s, int *out, int max){
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p) {
        char *end = NULL;
        long v;
        while (*p && (*p == ',' || *p == ' ' || *p == ';' || *p == '\t')) p++;
        if (!*p) break;
        v = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        if (out && n < max) out[n] = (int)v;
        n++;
        p = end;
    }
    return n;
}

/* int dizisini "65,66,67" biçiminde g_str'ye yazar. Dönen: yazılan uzunluk. */
static int gcl_int_list_to_str(const int *v, int count){
    int n = 0;
    if (!v || count <= 0) { g_str[0] = 0; return 0; }
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + n, sizeof(g_str) - (size_t)n,
                             "%s%d", (i ? "," : ""), v[i]);
        if (wrote < 0 || (size_t)(n + wrote) >= sizeof(g_str)) break;
        n += wrote;
    }
    return n;
}

/* Metni kod noktası dizisine çevirir (UnloadCodepoints ile bırakılır). */
static int *gcl_text_codepoints(const char *text, int *count){
    int n = 0;
    int *cp;
    if (count) *count = 0;
    if (!text || !text[0]) return NULL;
    cp = LoadCodepoints(text, &n);
    if (count) *count = n;
    return cp;
}

static double fn_GetFontDefault(int argc,const char**argv){ g_last_handle=reg_font(GetFontDefault()); return (double)g_last_handle; }
static double fn_LoadFont(int argc,const char**argv){ g_last_handle=reg_font(LoadFont(ss(argv[0]))); return (double)g_last_handle; }
/* LoadFontEx(fileName, fontSize[, "cp0,cp1,..."]): 3. argüman kod noktası
   listesidir. Eski sürüm her zaman NULL/0 geçiyordu, yani özel karakter
   setiyle font yüklemek imkânsızdı (todo P3). */
static double fn_LoadFontEx(int argc,const char**argv){
    int cpBuf[1024];
    int cpCount = 0;
    if (argc > 2 && argv[2] && argv[2][0]) {
        int n = gcl_parse_int_list(argv[2], cpBuf, 1024);
        cpCount = (n > 1024) ? 1024 : n;
    }
    g_last_handle = reg_font(LoadFontEx(ss(argv[0]), (argc > 1) ? ii(argv[1]) : 16,
                                        (cpCount > 0) ? cpBuf : NULL, cpCount));
    return (double)g_last_handle;
}
/* LoadFontFromImage(imageHandle, keyColor, firstChar): önceden boş bir Font
   kaydediyordu, yani çağrı hiçbir şey yüklemiyordu. */
static double fn_LoadFontFromImage(int argc,const char**argv){
    g_last_handle = reg_font(LoadFontFromImage(get_img((argc > 0) ? ii(argv[0]) : -1),
                                               ci(argv, 1),
                                               (argc > 2) ? ii(argv[2]) : 32));
    return (double)g_last_handle;
}
/* LoadFontFromMemory(fileName, fontSize): raylib sürümü dosya BAYTLARINI
   ister; modül dosyayı okur ve uzantıyı fileType olarak geçirir. Böylece
   LoadFont'un yüklemediği boyut/glif seti seçilebilir. */
static double fn_LoadFontFromMemory(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    int fontSize = (argc > 1) ? ii(argv[1]) : 16;
    int dataSize = 0;
    unsigned char *data = LoadFileData(file, &dataSize);
    Font f = (Font){0};
    if (data) {
        f = LoadFontFromMemory(ss(GetFileExtension(file)), data, dataSize, fontSize, NULL, 0);
        UnloadFileData(data);
    }
    g_last_handle = reg_font(f);
    return (double)g_last_handle;
}
static double fn_IsFontValid(int argc,const char**argv){ return IsFontValid(get_font(ii(argv[0])))?1.0:0.0; }
/* LoadFontData(fileName, fontSize, type[, "cp0,cp1,..."]) -> glyph-set indeksi.
   raylib `const unsigned char *fileData` alır ve `GlyphInfo *` döndürür: modül
   dosyayı kendisi okur, diziyi kendi tutar, GCL'e indeks verir ve glif sayısını
   g_last_int'te yayınlar (Raylib.LastInt()). Seti GenImageFontAtlas tüketir. */
static double fn_LoadFontData(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    int fontSize = (argc > 1) ? ii(argv[1]) : 16;
    int type     = (argc > 2) ? ii(argv[2]) : FONT_DEFAULT;
    int cpBuf[1024];
    int cpCount = 0;
    int dataSize = 0;
    int glyphCount = 0;
    int slot = -1;
    unsigned char *data;
    GlyphInfo *glyphs;
    if (argc > 3 && argv[3] && argv[3][0]) {
        int n = gcl_parse_int_list(argv[3], cpBuf, 1024);
        cpCount = (n > 1024) ? 1024 : n;
    }
    data = LoadFileData(file, &dataSize);
    if (!data) { g_last_int = 0; return -1.0; }
    glyphs = LoadFontData(data, dataSize, fontSize,
                          (cpCount > 0) ? cpBuf : NULL, cpCount, type, &glyphCount);
    UnloadFileData(data);
    g_last_int = glyphCount;
    if (!glyphs || glyphCount <= 0) return -1.0;
    for (int i = 0; i < GCL_MAX_GLYPH_SETS; i++) { if (!g_glyphset[i]) { slot = i; break; } }
    if (slot < 0) { UnloadFontData(glyphs, glyphCount); return -1.0; }
    g_glyphset[slot] = glyphs;
    g_glyphset_count[slot] = glyphCount;
    return (double)slot;
}
/* GenImageFontAtlas(glyphSet, fontSize, padding, packMethod) -> Image handle.
   raylib `Rectangle **glyphRecs` out-parametresini modül için ayırır; dizi
   glyph-set slotunda saklanır ve UnloadFontData ile bırakılır. */
static double fn_GenImageFontAtlas(int argc,const char**argv){
    int slot = (argc > 0) ? ii(argv[0]) : -1;
    Rectangle *recs = NULL;
    Image img;
    if (slot < 0 || slot >= GCL_MAX_GLYPH_SETS || !g_glyphset[slot]) {
        g_last_handle = -1; return -1.0;
    }
    img = GenImageFontAtlas(g_glyphset[slot], &recs, g_glyphset_count[slot],
                            (argc > 1) ? ii(argv[1]) : 16,
                            (argc > 2) ? ii(argv[2]) : 0,
                            (argc > 3) ? ii(argv[3]) : 0);
    if (g_glyphset_recs[slot]) MemFree(g_glyphset_recs[slot]);
    g_glyphset_recs[slot] = recs;
    g_last_handle = reg_img(img);
    return (double)g_last_handle;
}
/* UnloadFontData(glyphSet): LoadFontData'nın glif dizisini ve
   GenImageFontAtlas'ın ürettiği glyph dikdörtgenlerini bırakır. */
static double fn_UnloadFontData(int argc,const char**argv){
    int slot = (argc > 0) ? ii(argv[0]) : -1;
    if (slot < 0 || slot >= GCL_MAX_GLYPH_SETS) return 0.0;
    if (g_glyphset[slot]) {
        UnloadFontData(g_glyphset[slot], g_glyphset_count[slot]);
        g_glyphset[slot] = NULL;
    }
    if (g_glyphset_recs[slot]) { MemFree(g_glyphset_recs[slot]); g_glyphset_recs[slot] = NULL; }
    g_glyphset_count[slot] = 0;
    return 0.0;
}
static double fn_UnloadFont(int argc,const char**argv){ UnloadFont(get_font(ii(argv[0]))); return 0.0; }
static double fn_ExportFontAsCode(int argc,const char**argv){ return ExportFontAsCode(get_font(ii(argv[0])),ss(argv[1]))?1.0:0.0; }
static double fn_DrawFPS(int argc,const char**argv){ DrawFPS(ii(argv[0]),ii(argv[1])); return 0.0; }
static double fn_DrawText(int argc,const char**argv){ DrawText(ss(argv[0]),ii(argv[1]),ii(argv[2]),ii(argv[3]),ci(argv,4)); return 0.0; }
static double fn_DrawTextEx(int argc,const char**argv){ DrawTextEx(get_font(ii(argv[0])),ss(argv[1]),v2_arg(argv,2),ff(argv[4]),ff(argv[5]),ci(argv,6)); return 0.0; }
static double fn_DrawTextPro(int argc,const char**argv){ DrawTextPro(get_font(ii(argv[0])),ss(argv[1]),v2_arg(argv,2),v2_arg(argv,4),ff(argv[6]),ff(argv[7]),ff(argv[8]),ci(argv,9)); return 0.0; }
static double fn_DrawTextCodepoint(int argc,const char**argv){ DrawTextCodepoint(get_font(ii(argv[0])),ii(argv[1]),v2_arg(argv,2),ff(argv[4]),ci(argv,5)); return 0.0; }
/* DrawTextCodepoints(fontHandle, text, x, y, fontSize, spacing, Color).
   raylib `const int *codepoints` ister; modül metni kod noktalarına çevirir
   (LoadCodepoints) ve diziyi çağrı dönüşünde bırakır. */
static double fn_DrawTextCodepoints(int argc,const char**argv){
    int count = 0;
    int *cps = gcl_text_codepoints(ss((argc > 1) ? argv[1] : ""), &count);
    if (cps && argc >= 6) {
        DrawTextCodepoints(get_font(ii(argv[0])), cps, count,
                           (Vector2){ ff(argv[2]), ff(argv[3]) },
                           ff(argv[4]), ff(argv[5]), ci(argv, 6));
    }
    if (cps) UnloadCodepoints(cps);
    return 0.0;
}
static double fn_SetTextLineSpacing(int argc,const char**argv){ SetTextLineSpacing(ii(argv[0])); return 0.0; }
static double fn_MeasureText(int argc,const char**argv){ return (double)MeasureText(ss(argv[0]),ii(argv[1])); }
static double fn_MeasureTextEx(int argc,const char**argv){ g_last_v2=MeasureTextEx(get_font(ii(argv[0])),ss(argv[1]),ff(argv[2]),ff(argv[3])); return 0.0; }
/* MeasureTextCodepoints(fontHandle, text, fontSize, spacing) -> g_last_v2
   (Raylib.LastV2X / LastV2Y). raylib `const int *codepoints` + length ister;
   modül metni kod noktalarına çevirir. */
static double fn_MeasureTextCodepoints(int argc,const char**argv){
    int count = 0;
    int *cps = gcl_text_codepoints(ss((argc > 1) ? argv[1] : ""), &count);
    g_last_v2 = (Vector2){0};
    if (cps) {
        g_last_v2 = MeasureTextCodepoints(get_font((argc > 0) ? ii(argv[0]) : -1),
                                          cps, count,
                                          (argc > 2) ? ff(argv[2]) : 10.0f,
                                          (argc > 3) ? ff(argv[3]) : 0.0f);
        UnloadCodepoints(cps);
    }
    return 0.0;
}
static double fn_GetGlyphIndex(int argc,const char**argv){ return (double)GetGlyphIndex(get_font(ii(argv[0])),ii(argv[1])); }
static double fn_GetGlyphInfo(int argc,const char**argv){ g_last_glyph=GetGlyphInfo(get_font(ii(argv[0])),ii(argv[1])); return 0.0; }
static double fn_GetGlyphAtlasRec(int argc,const char**argv){ g_last_rect=GetGlyphAtlasRec(get_font(ii(argv[0])),ii(argv[1])); return 0.0; }
/* LoadUTF8("65,66,67") -> UTF-8 metni g_str'de; dönen: bayt sayısı.
   raylib `const int *codepoints` ister, GCL sayı listesini metin olarak verir. */
static double fn_LoadUTF8(int argc,const char**argv){
    int cps[512];
    int n = gcl_parse_int_list(ss((argc > 0) ? argv[0] : ""), cps, 512);
    char *s;
    if (n > 512) n = 512;
    if (n <= 0) { g_str[0] = 0; return 0.0; }
    s = LoadUTF8(cps, n);
    if (s) { snprintf(g_str, sizeof(g_str), "%s", s); UnloadUTF8(s); }
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
/* UnloadUTF8(text): dönüşümü modül yaptığı için bırakılacak tahsis yoktur. */
static double fn_UnloadUTF8(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
/* LoadCodepoints("AB") -> kod noktaları "65,66" (g_str); dönen: kod noktası
   sayısı (ayrıca g_last_int). LoadUTF8 ile geri çevrilebilir. */
static double fn_LoadCodepoints(int argc,const char**argv){
    int count = 0;
    int *cps = gcl_text_codepoints(ss((argc > 0) ? argv[0] : ""), &count);
    g_last_int = count;
    if (cps) { gcl_int_list_to_str(cps, count); UnloadCodepoints(cps); }
    else g_str[0] = 0;
    return (double)count;
}
static double fn_UnloadCodepoints(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_GetCodepointCount(int argc,const char**argv){ return (double)GetCodepointCount(ss(argv[0])); }
/* GetCodepoint(text[, byteOffset]) -> kod noktası; tüketilen bayt sayısı
   g_last_int'te (Raylib.LastInt()). */
static double fn_GetCodepoint(int argc,const char**argv){
    const char *text = ss((argc > 0) ? argv[0] : "");
    int len = (int)strlen(text);
    int off = (argc > 1) ? ii(argv[1]) : 0;
    int size = 1;
    int cp;
    if (off < 0) off = 0;
    if (off >= len) { g_last_int = 0; return 0.0; }
    cp = GetCodepoint(text + off, &size);
    g_last_int = size;
    return (double)cp;
}
static double fn_GetCodepointNext(int argc,const char**argv){
    const char *text = ss((argc > 0) ? argv[0] : "");
    int len = (int)strlen(text);
    int off = (argc > 1) ? ii(argv[1]) : 0;
    int size = 1;
    int cp;
    if (off < 0) off = 0;
    if (off >= len) { g_last_int = 0; return 0.0; }
    cp = GetCodepointNext(text + off, &size);
    g_last_int = size;
    return (double)cp;
}
/* GetCodepointPrevious(text[, byteOffset]): byteOffset'tan ÖNCEKİ kod noktası;
   bayt sayısı g_last_int'te. Offset verilmezse metnin sonundan başlar. */
static double fn_GetCodepointPrevious(int argc,const char**argv){
    const char *text = ss((argc > 0) ? argv[0] : "");
    int len = (int)strlen(text);
    int off = (argc > 1) ? ii(argv[1]) : len;
    int size = 1;
    int cp;
    if (off > len) off = len;
    if (off <= 0) { g_last_int = 0; return 0.0; }
    cp = GetCodepointPrevious(text + off, &size);
    g_last_int = size;
    return (double)cp;
}
/* CodepointToUTF8(codepoint) -> UTF-8 baytları g_str'de; dönen: bayt sayısı.
   raylib static bir tampon döndürür, bu yüzden hemen kopyalanır. */
static double fn_CodepointToUTF8(int argc,const char**argv){
    int size = 0;
    const char *u = CodepointToUTF8((argc > 0) ? ii(argv[0]) : 0, &size);
    if (u && size > 0) {
        int n = (size < (int)sizeof(g_str)) ? size : (int)sizeof(g_str) - 1;
        memcpy(g_str, u, (size_t)n);
        g_str[n] = 0;
    } else { g_str[0] = 0; size = 0; }
    return (double)size;
}
/* LoadTextLines(text) -> satır sayısı (g_last_int); satırlar g_str'de '\n' ile
   ayrılmış. raylib `char **` döndürdüğü için dizi modülde kalır ve hemen
   bırakılır. */
static double fn_LoadTextLines(int argc,const char**argv){
    int count = 0;
    char **lines = LoadTextLines(ss((argc > 0) ? argv[0] : ""), &count);
    int pos = 0;
    g_last_int = count;
    g_str[0] = 0;
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + pos, sizeof(g_str) - (size_t)pos, "%s%s",
                             (i ? "\n" : ""), lines[i] ? lines[i] : "");
        if (wrote < 0 || (size_t)(pos + wrote) >= sizeof(g_str)) break;
        pos += wrote;
    }
    if (lines) UnloadTextLines(lines, count);
    return (double)count;
}
static double fn_UnloadTextLines(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_TextCopy(int argc,const char**argv){ return (double)TextCopy((char*)argv[0],ss(argv[1])); }
static double fn_TextIsEqual(int argc,const char**argv){ return TextIsEqual(ss(argv[0]),ss(argv[1]))?1.0:0.0; }
static double fn_TextLength(int argc,const char**argv){ return (double)TextLength(ss(argv[0])); }
static double fn_TextFormat(int argc,const char**argv){ const char *s=TextFormat(ss(argv[0]),ss(argv[1]),ss(argv[2]),ss(argv[3]),ss(argv[4]),ss(argv[5]),ss(argv[6]),ss(argv[7]),ss(argv[8]),ss(argv[9]),ss(argv[10]),ss(argv[11]),ss(argv[12]),ss(argv[13]),ss(argv[14]),ss(argv[15])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextSubtext(int argc,const char**argv){ const char *s=TextSubtext(ss(argv[0]),ii(argv[1]),ii(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextRemoveSpaces(int argc,const char**argv){ const char *s=TextRemoveSpaces(ss(argv[0])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_GetTextBetween(int argc,const char**argv){ char *s=GetTextBetween(ss(argv[0]),ss(argv[1]),ss(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextReplace(int argc,const char**argv){ char *s=TextReplace(ss(argv[0]),ss(argv[1]),ss(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
/* ---------- Metin yardımcıları ----------
   raylib'in bir kısmı yerinde yazar (char *text), bir kısmı static/ayrılmış
   tampon döndürür. GCL metinleri salt-okunurdur, bu yüzden sonuç her zaman
   g_str'ye kopyalanır ve fonksiyon yeni uzunluğu döndürür; Alloc varyantları
   ayrılan tamponu hemen bırakır (sızıntı yok). */

static double fn_TextReplaceAlloc(int argc,const char**argv){
    char *s = TextReplaceAlloc(ss(argv[0]), ss(argv[1]), ss(argv[2]));
    if (s) { snprintf(g_str, sizeof(g_str), "%s", s); free(s); }
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
static double fn_TextReplaceBetween(int argc,const char**argv){
    const char *s = TextReplaceBetween(ss(argv[0]), ss(argv[1]), ss(argv[2]), ss(argv[3]));
    if (s) snprintf(g_str, sizeof(g_str), "%s", s);
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
static double fn_TextReplaceBetweenAlloc(int argc,const char**argv){
    char *s = TextReplaceBetweenAlloc(ss(argv[0]), ss(argv[1]), ss(argv[2]), ss(argv[3]));
    if (s) { snprintf(g_str, sizeof(g_str), "%s", s); free(s); }
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
static double fn_TextInsert(int argc,const char**argv){ char *s=TextInsert(ss(argv[0]),ss(argv[1]),ii(argv[2])); if(s){snprintf(g_str,sizeof(g_str),"%s",s);} return 0.0; }
static double fn_TextInsertAlloc(int argc,const char**argv){
    char *s = TextInsertAlloc(ss(argv[0]), ss(argv[1]), (argc > 2) ? ii(argv[2]) : 0);
    if (s) { snprintf(g_str, sizeof(g_str), "%s", s); free(s); }
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
/* TextJoin(delimiter, item0, item1, ...): raylib `char **textList, int count`
   ister; GCL değişken sayıda argümanla çağırır (ilk argüman ayırıcı). */
static double fn_TextJoin(int argc,const char**argv){
    static char *items[64];
    const char *delim = (argc > 0) ? ss(argv[0]) : "";
    int n = argc - 1;
    const char *s;
    if (n < 0) n = 0;
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) items[i] = (char *)ss(argv[1 + i]);
    s = TextJoin(items, n, delim);
    if (s) snprintf(g_str, sizeof(g_str), "%s", s);
    else g_str[0] = 0;
    return (double)strlen(g_str);
}
/* TextSplit(text[, delimiter]) -> parça sayısı (g_last_int); parçalar g_str'de
   '\n' ile ayrılmış olarak (raylib `char **` döndürür). Ayırıcı verilmezse
   ',' kullanılır. */
static double fn_TextSplit(int argc,const char**argv){
    char delim = (argc > 1 && argv[1] && argv[1][0]) ? argv[1][0] : ',';
    int count = 0;
    char **parts = TextSplit(ss(argv[0]), delim, &count);
    int pos = 0;
    g_last_int = count;
    g_str[0] = 0;
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + pos, sizeof(g_str) - (size_t)pos, "%s%s",
                             (i ? "\n" : ""), parts[i] ? parts[i] : "");
        if (wrote < 0 || (size_t)(pos + wrote) >= sizeof(g_str)) break;
        pos += wrote;
    }
    return (double)count;
}
/* TextAppend(text, append) -> birleşik metin g_str'de; dönen: yeni uzunluk.
   raylib yerinde yazar; GCL metni salt-okunur olduğu için modül bir tamponda
   birleştirir. */
static double fn_TextAppend(int argc,const char**argv){
    char buf[4096];
    int pos = 0;
    snprintf(buf, sizeof(buf), "%s", ss((argc > 0) ? argv[0] : ""));
    pos = (int)strlen(buf);
    TextAppend(buf, ss((argc > 1) ? argv[1] : ""), &pos);
    snprintf(g_str, sizeof(g_str), "%s", buf);
    return (double)pos;
}
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
/* DrawTriangleStrip3D(count, x0,y0,z0, x1,y1,z1, ..., Color)
   raylib `const Vector3 *points, int pointCount` ister; GCL sayı listesini
   sırayla verir (count önce — modülün diğer dizi köprüleriyle aynı düzen). */
static double fn_DrawTriangleStrip3D(int argc,const char**argv){
    static Vector3 pts[256];
    int count = (argc > 0) ? ii(argv[0]) : 0;
    Color tint = (Color){255,255,255,255};
    if (count < 0) count = 0;
    if (count > 256) count = 256;
    for (int i = 0; i < count; i++) {
        int a = 1 + i * 3;
        pts[i] = (Vector3){ (a < argc) ? ff(argv[a]) : 0.0f,
                            (a + 1 < argc) ? ff(argv[a + 1]) : 0.0f,
                            (a + 2 < argc) ? ff(argv[a + 2]) : 0.0f };
    }
    if (1 + count * 3 < argc) tint = ci(argv, 1 + count * 3);
    if (count >= 3) DrawTriangleStrip3D(pts, count, tint);
    return 0.0;
}
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

/* UploadMesh(meshHandle[, dynamic]): raylib `Mesh *` alır; modül kayıt
   tablosundaki girdiyi günceller (vaoId/vboId mesh'e yazılır). */
static double fn_UploadMesh(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h < 0 || h >= g_mesh_n) return 0.0;
    UploadMesh(&g_mesh_h[h], (argc > 1) ? (ii(argv[1]) != 0) : false);
    return 0.0;
}
/* UpdateMeshBuffer(meshHandle, bufferIndex, hexData[, dataSize[, offset]]):
   raylib `const void *data` ister; GCL baytları HEX metin olarak verir
   (mevcut hex köprüsü — bkz. CompressData/LoadImageFromMemory). */
static double fn_UpdateMeshBuffer(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    int index = (argc > 1) ? ii(argv[1]) : 0;
    const char *hex = ss((argc > 2) ? argv[2] : "");
    int binLen = 0;
    unsigned char *bin;
    if (h < 0 || h >= g_mesh_n) return 0.0;
    bin = gcl_hex_to_bytes(hex, (int)strlen(hex), &binLen);
    if (!bin) return 0.0;
    UpdateMeshBuffer(g_mesh_h[h], index, bin,
                     (argc > 3) ? ii(argv[3]) : binLen,
                     (argc > 4) ? ii(argv[4]) : 0);
    free(bin);
    return 0.0;
}
static double fn_UnloadMesh(int argc,const char**argv){ UnloadMesh(get_mesh(ii(argv[0]))); return 0.0; }
static double fn_DrawMesh(int argc,const char**argv){ DrawMesh(get_mesh(ii(argv[0])),get_mat(ii(argv[1])),g_last_mat); return 0.0; }
/* DrawMeshInstanced(meshHandle, materialHandle, instanceCount, m0[16] float,
                     m1[16] float, ...): raylib `const Matrix *transforms`
   ister; GCL matrisleri düz 16'şar float olarak verir. */
static double fn_DrawMeshInstanced(int argc,const char**argv){
    static Matrix tr[64];
    int mh   = (argc > 0) ? ii(argv[0]) : -1;
    int matH = (argc > 1) ? ii(argv[1]) : -1;
    int n    = (argc > 2) ? ii(argv[2]) : 0;
    if (mh < 0 || mh >= g_mesh_n) return 0.0;
    if (n < 0) n = 0;
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        float *f = &tr[i].m0;
        for (int k = 0; k < 16; k++) {
            int a = 3 + i * 16 + k;
            f[k] = (a < argc) ? ff(argv[a]) : 0.0f;
        }
    }
    if (n > 0) DrawMeshInstanced(g_mesh_h[mh], get_mat(matH), tr, n);
    return 0.0;
}
static double fn_GetMeshBoundingBox(int argc,const char**argv){ g_last_bb=GetMeshBoundingBox(get_mesh(ii(argv[0]))); return 0.0; }
/* GenMeshTangents(meshHandle): mesh'i yerinde günceller. */
static double fn_GenMeshTangents(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h < 0 || h >= g_mesh_n) return 0.0;
    GenMeshTangents(&g_mesh_h[h]);
    return 0.0;
}
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

/* LoadMaterials(fileName) -> İLK malzemenin handle'ı; malzeme sayısı
   g_last_int'te (Raylib.LastInt()). raylib `Material *` dizisi döndürür;
   modül her malzemeyi kendi kayıt tablosuna alır, sadece diziyi bırakır
   (malzemelerin map'leri UnloadMaterial çağrılana kadar yaşar). */
static double fn_LoadMaterials(int argc,const char**argv){
    int count = 0;
    Material *mats = LoadMaterials(ss((argc > 0) ? argv[0] : ""), &count);
    int first = -1;
    g_last_int = count;
    if (!mats) return -1.0;
    for (int i = 0; i < count; i++) {
        int h = reg_mat(mats[i]);
        if (i == 0) first = h;
    }
    MemFree(mats);
    return (double)first;
}
static double fn_LoadMaterialDefault(int argc,const char**argv){ g_last_handle=reg_mat(LoadMaterialDefault()); return (double)g_last_handle; }
static double fn_IsMaterialValid(int argc,const char**argv){ return IsMaterialValid(get_mat(ii(argv[0])))?1.0:0.0; }
static double fn_UnloadMaterial(int argc,const char**argv){ UnloadMaterial(get_mat(ii(argv[0]))); return 0.0; }
/* SetMaterialTexture(materialHandle, mapType, textureHandle): raylib
   `Material *` ister; modül kayıt tablosundaki girdiyi yerinde günceller. */
static double fn_SetMaterialTexture(int argc,const char**argv){
    int h  = (argc > 0) ? ii(argv[0]) : -1;
    int mt = (argc > 1) ? ii(argv[1]) : MATERIAL_MAP_DIFFUSE;
    int th = (argc > 2) ? ii(argv[2]) : -1;
    if (h < 0 || h >= g_mat_n) return 0.0;
    SetMaterialTexture(&g_mat_h[h], mt, get_tex(th));
    return 0.0;
}
/* SetModelMeshMaterial(modelHandle, meshId, materialId) */
static double fn_SetModelMeshMaterial(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h < 0 || h >= g_model_n) return 0.0;
    SetModelMeshMaterial(&g_model_h[h],
                         (argc > 1) ? ii(argv[1]) : 0,
                         (argc > 2) ? ii(argv[2]) : 0);
    return 0.0;
}
/* LoadModelAnimations(fileName) -> İLK animasyon indeksi; animasyon sayısı
   g_last_int'te. raylib `ModelAnimation *` dizisi döndürür; modül diziyi
   g_anim_h tablosuna alır, UpdateModelAnimation(Ex) bu indekslerle çalışır. */
static double fn_LoadModelAnimations(int argc,const char**argv){
    int count = 0;
    ModelAnimation *anims = LoadModelAnimations(ss((argc > 0) ? argv[0] : ""), &count);
    int first = -1;
    g_last_int = count;
    if (!anims) return -1.0;
    g_anim_sets++;
    for (int i = 0; i < count; i++) {
        int slot = -1;
        for (int s = 0; s < g_anim_n; s++) { if (!g_anim_live[s]) { slot = s; break; } }
        if (slot < 0) {
            if (g_anim_n >= GCL_MAX_ANIMS) break;
            slot = g_anim_n++;
        }
        g_anim_h[slot]    = anims[i];
        g_anim_set[slot]  = g_anim_sets;
        g_anim_live[slot] = 1;
        if (i == 0) first = slot;
    }
    MemFree(anims);
    return (double)first;
}
/* UpdateModelAnimation(modelHandle, animationIndex, frame) */
static double fn_UpdateModelAnimation(int argc,const char**argv){
    int mh    = (argc > 0) ? ii(argv[0]) : -1;
    int ai    = (argc > 1) ? ii(argv[1]) : -1;
    int frame = (argc > 2) ? ii(argv[2]) : 0;
    if (mh < 0 || mh >= g_model_n) return 0.0;
    if (ai < 0 || ai >= g_anim_n || !g_anim_live[ai]) return 0.0;
    UpdateModelAnimation(g_model_h[mh], g_anim_h[ai], (float)frame);
    return 0.0;
}
/* UpdateModelAnimationEx(model, animA, frameA, animB, frameB, blend) */
static double fn_UpdateModelAnimationEx(int argc,const char**argv){
    int mh    = (argc > 0) ? ii(argv[0]) : -1;
    int aa    = (argc > 1) ? ii(argv[1]) : -1;
    float fa  = (argc > 2) ? ff(argv[2]) : 0.0f;
    int ab    = (argc > 3) ? ii(argv[3]) : -1;
    float fb  = (argc > 4) ? ff(argv[4]) : 0.0f;
    float mix = (argc > 5) ? ff(argv[5]) : 0.0f;
    if (mh < 0 || mh >= g_model_n) return 0.0;
    if (aa < 0 || aa >= g_anim_n || !g_anim_live[aa]) return 0.0;
    if (ab < 0 || ab >= g_anim_n || !g_anim_live[ab]) return 0.0;
    UpdateModelAnimationEx(g_model_h[mh], g_anim_h[aa], fa, g_anim_h[ab], fb, mix);
    return 0.0;
}
/* UnloadModelAnimations(): modülün tuttuğu TÜM animasyonları bırakır.
   raylib sürümü `dizi + sayı` ister ve diziyi RL_FREE ile serbest bırakır;
   modülün dizisi statik olduğu için geçici bir kopya üzerinden çağrılır. */
static double fn_UnloadModelAnimations(int argc,const char**argv){
    int live = 0;
    (void)argc; (void)argv;
    for (int i = 0; i < g_anim_n; i++) { if (g_anim_live[i]) live++; }
    if (live > 0) {
        ModelAnimation *tmp =
            (ModelAnimation *)MemAlloc((unsigned int)(live * (int)sizeof(ModelAnimation)));
        if (tmp) {
            int n = 0;
            for (int i = 0; i < g_anim_n; i++) { if (g_anim_live[i]) tmp[n++] = g_anim_h[i]; }
            UnloadModelAnimations(tmp, n);
        }
    }
    for (int i = 0; i < GCL_MAX_ANIMS; i++) {
        g_anim_live[i] = 0;
        g_anim_set[i]  = 0;
        memset(&g_anim_h[i], 0, sizeof(g_anim_h[i]));
    }
    g_anim_n = 0;
    return 0.0;
}
/* IsModelAnimationValid(modelHandle, animationIndex): önceki sürüm her zaman
   boş bir `ModelAnimation{0}` ile çağırıyordu, yani daima 0 dönüyordu. */
static double fn_IsModelAnimationValid(int argc,const char**argv){
    int mh = (argc > 0) ? ii(argv[0]) : -1;
    int ai = (argc > 1) ? ii(argv[1]) : -1;
    if (mh < 0 || mh >= g_model_n) return 0.0;
    if (ai < 0 || ai >= g_anim_n || !g_anim_live[ai]) return 0.0;
    return IsModelAnimationValid(g_model_h[mh], g_anim_h[ai]) ? 1.0 : 0.0;
}

static double fn_CheckCollisionSpheres(int argc,const char**argv){ return CheckCollisionSpheres(v3_arg(argv,0),ff(argv[3]),v3_arg(argv,4),ff(argv[7]))?1.0:0.0; }
/* CheckCollisionBoxes(min1 xyz, max1 xyz, min2 xyz, max2 xyz): önceki sürüm
   iki argümanı da son `g_last_bb`'den alıyordu, yani GCL'den kutu geçirmenin
   yolu yoktu ve sonuç anlamsızdı. */
static double fn_CheckCollisionBoxes(int argc,const char**argv){
    BoundingBox b1, b2;
    if (argc < 12) return 0.0;
    b1 = (BoundingBox){ v3_arg(argv, 0), v3_arg(argv, 3) };
    b2 = (BoundingBox){ v3_arg(argv, 6), v3_arg(argv, 9) };
    return CheckCollisionBoxes(b1, b2) ? 1.0 : 0.0;
}
/* CheckCollisionBoxSphere(min xyz, max xyz, center xyz, radius) */
static double fn_CheckCollisionBoxSphere(int argc,const char**argv){
    BoundingBox b;
    if (argc < 10) return 0.0;
    b = (BoundingBox){ v3_arg(argv, 0), v3_arg(argv, 3) };
    return CheckCollisionBoxSphere(b, v3_arg(argv, 6), ff(argv[9])) ? 1.0 : 0.0;
}
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
/* LoadWaveFromMemory(fileName, fileType) -> Wave handle. raylib `fileData`
   bayt dizisi ister; modül dosyayı okur ve dosya tipini açıkça kullanır. */
static double fn_LoadWaveFromMemory(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    const char *dot = strrchr(file, '.');
    const char *type = (argc > 1 && argv[1] && argv[1][0]) ? ss(argv[1]) : (dot ? dot : "");
    int dataSize = 0;
    Wave w = (Wave){0};
    unsigned char *data = LoadFileData(file, &dataSize);
    if (data) {
        w = LoadWaveFromMemory(type, data, dataSize);
        UnloadFileData(data);
    }
    g_last_handle = reg_wave(w);
    return (double)g_last_handle;
}
static double fn_IsWaveValid(int argc,const char**argv){ return IsWaveValid(get_wave(ii(argv[0])))?1.0:0.0; }
static double fn_LoadSound(int argc,const char**argv){ g_last_handle=reg_snd(LoadSound(ss(argv[0]))); return (double)g_last_handle; }
static double fn_LoadSoundFromWave(int argc,const char**argv){ g_last_handle=reg_snd(LoadSoundFromWave(get_wave(ii(argv[0])))); return (double)g_last_handle; }
static double fn_LoadSoundAlias(int argc,const char**argv){ g_last_handle=reg_snd(LoadSoundAlias(get_snd(ii(argv[0])))); return (double)g_last_handle; }
static double fn_IsSoundValid(int argc,const char**argv){ return IsSoundValid(get_snd(ii(argv[0])))?1.0:0.0; }
/* UpdateSound(soundHandle, hexData[, frameCount]): raylib `const void *data`
   ister; GCL baytları HEX olarak verir. frameCount verilmezse bayt sayısı / 4
   (32-bit float çerçeve) kullanılır. */
static double fn_UpdateSound(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    const char *hex = ss((argc > 1) ? argv[1] : "");
    int binLen = 0;
    unsigned char *bin;
    if (h < 0 || h >= g_snd_n) return 0.0;
    bin = gcl_hex_to_bytes(hex, (int)strlen(hex), &binLen);
    if (!bin) return 0.0;
    UpdateSound(g_snd_h[h], bin, (argc > 2) ? ii(argv[2]) : (binLen / 4));
    free(bin);
    return 0.0;
}
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
/* WaveCrop(waveHandle, initFrame, finalFrame): dalgayı yerinde kırpar. */
static double fn_WaveCrop(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h < 0 || h >= g_wave_n) return 0.0;
    WaveCrop(&g_wave_h[h], (argc > 1) ? ii(argv[1]) : 0, (argc > 2) ? ii(argv[2]) : 0);
    return 0.0;
}
/* WaveFormat(waveHandle, sampleRate, sampleSize, channels): dalgayı yerinde
   yeniden biçimlendirir. */
static double fn_WaveFormat(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    if (h < 0 || h >= g_wave_n) return 0.0;
    WaveFormat(&g_wave_h[h],
               (argc > 1) ? ii(argv[1]) : 44100,
               (argc > 2) ? ii(argv[2]) : 16,
               (argc > 3) ? ii(argv[3]) : 2);
    return 0.0;
}
/* LoadWaveSamples(waveHandle) -> örnek sayısı (kare × kanal); değerler g_str'de
   virgülle ayrılmış float olarak (raylib `float *` döndürür, dizi modülde
   kalır ve hemen bırakılır — LoadRandomSequence ile aynı desen). */
static double fn_LoadWaveSamples(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    Wave w;
    float *samples;
    int count = 0;
    int pos = 0;
    g_str[0] = 0;
    if (h < 0 || h >= g_wave_n) return 0.0;
    w = g_wave_h[h];
    samples = LoadWaveSamples(w);
    if (!samples) return 0.0;
    count = (int)(w.frameCount * w.channels);
    if (count < 0) count = 0;
    for (int i = 0; i < count; i++) {
        int wrote = snprintf(g_str + pos, sizeof(g_str) - (size_t)pos,
                             "%s%.6g", (i ? "," : ""), (double)samples[i]);
        if (wrote < 0 || (size_t)(pos + wrote) >= sizeof(g_str)) break;
        pos += wrote;
    }
    UnloadWaveSamples(samples);
    return (double)count;
}
/* UnloadWaveSamples(): örnekleri g_str'ye kopyalayıp hemen bıraktığımız için
   GCL tarafında serbest bırakılacak bir şey yoktur. */
static double fn_UnloadWaveSamples(int argc,const char**argv){ (void)argc;(void)argv; return 0.0; }
static double fn_LoadMusicStream(int argc,const char**argv){ g_last_handle=reg_mus(LoadMusicStream(ss(argv[0]))); return (double)g_last_handle; }
/* LoadMusicStreamFromMemory(fileName, fileType) -> Music handle. raylib
   `unsigned char *data` ister; modül dosyayı okur ve tipi açıkça kullanır. */
static double fn_LoadMusicStreamFromMemory(int argc,const char**argv){
    const char *file = ss((argc > 0) ? argv[0] : "");
    const char *dot = strrchr(file, '.');
    const char *type = (argc > 1 && argv[1] && argv[1][0]) ? ss(argv[1]) : (dot ? dot : "");
    int dataSize = 0;
    Music m = (Music){0};
    unsigned char *data = LoadFileData(file, &dataSize);
    if (data) {
        m = LoadMusicStreamFromMemory(type, data, dataSize);
        UnloadFileData(data);
    }
    g_last_handle = reg_mus(m);
    return (double)g_last_handle;
}
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
/* UpdateAudioStream(streamHandle, hexData[, frameCount]): raylib
   `const void *data` ister; GCL baytları HEX olarak verir. */
static double fn_UpdateAudioStream(int argc,const char**argv){
    int h = (argc > 0) ? ii(argv[0]) : -1;
    const char *hex = ss((argc > 1) ? argv[1] : "");
    int binLen = 0;
    unsigned char *bin;
    if (h < 0 || h >= g_ast_n) return 0.0;
    bin = gcl_hex_to_bytes(hex, (int)strlen(hex), &binLen);
    if (!bin) return 0.0;
    UpdateAudioStream(g_ast_h[h], bin, (argc > 2) ? ii(argv[2]) : (binLen / 4));
    free(bin);
    return 0.0;
}
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
/* Bu beş üye bir C `AudioCallback` fonksiyon işaretçisi alır
   (SetAudioStreamCallback / Attach(Audio|Detach)...Processor). GCL bir
   fonksiyonu C'ye geçiremediği için bilinçli no-op'tur: çağrı sessizce hiçbir
   şey yapmaz, program çökmez. Üyeler tabloda kalır ki `Raygui`/`Raylib`
   yüzeyi raylib ile eşleşsin ve "unknown member" hatası çıkmasın
   (bkz. SetTraceLogCallback — aynı gerekçe). */
/* ---------- Ses callback köprüsü (gerçek zamanlı güvenli) ----------

   raylib ses callback'ini AYRI bir iş parçacığında çağırır; yorumlayıcının
   ortamı paylaşılan durum olduğu için orada GCL ÇALIŞTIRILAMAZ (veri yarışı).
   Bu yüzden tek üretici/tek tüketici bir HALK TAMPON kullanılır:

     • ANA iş parçacığı (gcl_module_tick): tampon azaldığında GCL üretici
       fonksiyonu host->call_gcl ile çağrılır; fonksiyon Raylib.AudioPush(v)
       ile örnek basar.
     • SES iş parçacığı (trambolin): yalnızca tampondan örnek ÇEKER; GCL'e
       hiç girmez, kilit kullanmaz — bu yüzden gerçek zamanlı güvenlidir.
   Tampon boşsa sessizlik (0.0) üretilir, çatlama olmaz. */
#define GCL_AUDIO_RING     16384
#define GCL_AUDIO_LOW       2048   /* bu seviyenin altında üretim tetiklenir */
#define GCL_AUDIO_HIGH      8192   /* hedeflenen doluluk */
#define GCL_AUDIO_MAX_GEN   4096   /* tek tick'te en çok üretilecek örnek */

static float g_aring[GCL_AUDIO_RING];
static int   g_ahead = 0;             /* yazar: ana iş parçacığı */
static int   g_atail = 0;             /* okur: ses iş parçacığı */
static char  g_audio_gen[128] = "";   /* üretecek GCL fonksiyonunun adı */
static int   g_audio_mode  = 0;       /* 0: buffer'ı YAZ, 1: üstüne EKLE */
static int   g_audio_ch    = 1;       /* kanal sayısı (kurulumda alınır) */

static int a_count(void){
    int h = __atomic_load_n(&g_ahead, __ATOMIC_ACQUIRE);
    int t = __atomic_load_n(&g_atail, __ATOMIC_ACQUIRE);
    return h - t;
}
static int a_push(float v){
    int h = __atomic_load_n(&g_ahead, __ATOMIC_RELAXED);
    int t = __atomic_load_n(&g_atail, __ATOMIC_ACQUIRE);
    if (h - t >= GCL_AUDIO_RING) return 0;          /* dolu: örnek atılır */
    g_aring[h % GCL_AUDIO_RING] = v;
    __atomic_store_n(&g_ahead, h + 1, __ATOMIC_RELEASE);
    return 1;
}
static int a_pop(float *v){
    int t = __atomic_load_n(&g_atail, __ATOMIC_RELAXED);
    int h = __atomic_load_n(&g_ahead, __ATOMIC_ACQUIRE);
    if (h - t <= 0) return 0;
    *v = g_aring[t % GCL_AUDIO_RING];
    __atomic_store_n(&g_atail, t + 1, __ATOMIC_RELEASE);
    return 1;
}

/* SES iş parçacığı — yalnızca tampon okur, GCL'e girmez. */
static void gcl_audio_trampoline(void *bufferData, unsigned int frames){
    float *out = (float *)bufferData;
    if (!out) return;
    int ch = (g_audio_ch > 0) ? g_audio_ch : 1;
    for (unsigned int f = 0; f < frames; f++) {
        float v = 0.0f;
        if (!a_pop(&v)) v = 0.0f;                   /* tampon boş → sessizlik */
        for (int c = 0; c < ch; c++) {
            size_t idx = (size_t)f * (size_t)ch + (size_t)c;
            if (g_audio_mode) out[idx] += v;         /* processor: mevcut sesin üstüne */
            else              out[idx]  = v;
        }
    }
}

/* ANA iş parçacığı — ertelenmiş ses üretimi. Runtime bunu her native modül
   çağrısından önce çağırır; bir oyun döngüsünde bu her karede olur. */
GCL_EXPORT void gcl_module_tick(const GclHostApi *api){
    if (!api || !api->call_gcl) return;
    if (!g_audio_gen[0]) return;
    int q = a_count();
    if (q > GCL_AUDIO_LOW) return;                  /* yeterli örnek var */
    int need = GCL_AUDIO_HIGH - q;
    if (need > GCL_AUDIO_MAX_GEN) need = GCL_AUDIO_MAX_GEN;
    if (need < 1) return;
    GclHostArg a[1];
    a[0].is_string = 0; a[0].num = (double)need; a[0].str = NULL;
    api->call_gcl(api->host, g_audio_gen, 1, a, NULL);
}

/* Ayırıcılar aşağıda tanımlı ama takıcılar onlara düşebiliyor (boş ad =
   ayır); C99 örtük bildirime izin vermediği için önden bildirilirler. */
static double fn_DetachAudioStreamProcessor(int argc,const char**argv);
static double fn_DetachAudioMixedProcessor(int argc,const char**argv);

/* SetAudioStreamCallback(streamHandle, "onAudio") — callback bu akışa takılır
   ve buffer'ı YAZAR. Gen adı boş verilirse callback kaldırılır. */
static double fn_SetAudioStreamCallback(int argc,const char**argv){
    AudioStream st = get_ast((argc > 0) ? ii(argv[0]) : -1);
    if (argc < 2 || !argv[1] || !argv[1][0]) {
        snprintf(g_audio_gen, sizeof(g_audio_gen), "");
        g_audio_mode = 0;
        SetAudioStreamCallback(st, NULL);
        return 0.0;
    }
    snprintf(g_audio_gen, sizeof(g_audio_gen), "%s", argv[1]);
    g_audio_mode = 0;
    g_audio_ch   = (st.channels > 0) ? st.channels : 1;
    SetAudioStreamCallback(st, gcl_audio_trampoline);
    return 0.0;
}
/* AttachAudioStreamProcessor(streamHandle, "onAudio") — mevcut sesin ÜSTÜNE
   ekler (raylib processor semantiği). Ad verilmezse ayırır. */
static double fn_AttachAudioStreamProcessor(int argc,const char**argv){
    AudioStream st = get_ast((argc > 0) ? ii(argv[0]) : -1);
    if (argc < 2 || !argv[1] || !argv[1][0]) return fn_DetachAudioStreamProcessor(argc, argv);
    snprintf(g_audio_gen, sizeof(g_audio_gen), "%s", argv[1]);
    g_audio_mode = 1;
    g_audio_ch   = (st.channels > 0) ? st.channels : 1;
    AttachAudioStreamProcessor(st, gcl_audio_trampoline);
    return 0.0;
}
static double fn_DetachAudioStreamProcessor(int argc,const char**argv){
    AudioStream st = get_ast((argc > 0) ? ii(argv[0]) : -1);
    snprintf(g_audio_gen, sizeof(g_audio_gen), "");
    DetachAudioStreamProcessor(st, gcl_audio_trampoline);
    return 0.0;
}
/* Karma (mixed) işlemciler tüm ses çıkışına eklenir; akış argümanı yoktur. */
static double fn_AttachAudioMixedProcessor(int argc,const char**argv){
    if (argc < 1 || !argv[0] || !argv[0][0]) return fn_DetachAudioMixedProcessor(argc, argv);
    snprintf(g_audio_gen, sizeof(g_audio_gen), "%s", argv[0]);
    g_audio_mode = 1;
    g_audio_ch   = 2;
    AttachAudioMixedProcessor(gcl_audio_trampoline);
    return 0.0;
}
static double fn_DetachAudioMixedProcessor(int argc,const char**argv){
    (void)argc;(void)argv;
    snprintf(g_audio_gen, sizeof(g_audio_gen), "");
    DetachAudioMixedProcessor(gcl_audio_trampoline);
    return 0.0;
}

/* Üretici tarafı erişimcileri: GCL callback'i bunlarla örnek basar/okur. */
static double fn_AudioPush(int argc,const char**argv){
    return a_push((argc > 0) ? ff(argv[0]) : 0.0f) ? 1.0 : 0.0;
}
static double fn_AudioQueued(int argc,const char**argv){
    (void)argc;(void)argv;
    return (double)a_count();
}

/* =========================================================================
   Fonksiyon tablosu
   ========================================================================= */
/* =========================================================================
   RAYLIB 6.1 — EKSİK API FONKSİYONLARI
   ========================================================================= */
static double fn_IsFileHidden(int argc,const char**argv){ return IsFileHidden(ss(argv[0]))?1.0:0.0; }
static double fn_DrawTriangleGradient(int argc,const char**argv){
    DrawTriangleGradient(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ci(argv,6),ci(argv,7),ci(argv,8)); return 0.0;
}
static double fn_DrawTriangleLinesEx(int argc,const char**argv){
    DrawTriangleLinesEx(v2_arg(argv,0),v2_arg(argv,2),v2_arg(argv,4),ff(argv[6]),ci(argv,7)); return 0.0;
}
static double fn_LoadRenderTextureEx(int argc,const char**argv){
    g_last_handle=reg_rt(LoadRenderTextureEx(ii(argv[0]),ii(argv[1]),ii(argv[2]))); return (double)g_last_handle;
}

/* =========================================================================
   RAYLIB 6.1 — TİP KURUCULARI (değer tipleri g_* global'ine yazar)
   ========================================================================= */
/* g_last_quat / g_last_transform / g_last_skeleton / g_last_manim /
   g_last_vrdev / g_last_matmap / g_last_bone dosyanın BAŞINDA, diğer g_last_*
   global'leriyle birlikte tanımlıdır (READ-BACK listesi onları orada kullanır). */

/* Vector4 alias: Quaternion */
static double fn_Quaternion(int argc,const char**argv){(void)argc;(void)argv;g_last_quat=(Quaternion){ff(argv[0]),ff(argv[1]),ff(argv[2]),ff(argv[3])};return 0.0;}
/* Color: ABI'de 32-bit packed uint olarak taşınır (zincirleme yok) */
static double fn_Color(int argc,const char**argv){
    Color c; c.r=(unsigned char)ii(argv[0]); c.g=(unsigned char)ii(argv[1]);
    c.b=(unsigned char)ii(argv[2]); c.a=(unsigned char)ii(argv[3]);
    return (double)color_to_uint(c);
}
/* Camera3D (Camera ile aynı alanlar) */
static double fn_Camera3D(int argc,const char**argv){(void)argc;(void)argv;g_last_cam=(Camera3D){v3_arg(argv,0),v3_arg(argv,3),v3_arg(argv,6),ff(argv[9]),ii(argv[10])};return 0.0;}
/* RayCollision */
static double fn_RayCollision(int argc,const char**argv){(void)argc;(void)argv;g_last_raycol=(RayCollision){(ii(argv[0])!=0),ff(argv[1]),v3_arg(argv,2),v3_arg(argv,5)};return 0.0;}
/* Transform */
static double fn_Transform(int argc,const char**argv){(void)argc;(void)argv;g_last_transform=(Transform){v3_arg(argv,0),(Quaternion){ff(argv[3]),ff(argv[4]),ff(argv[5]),ff(argv[6])},v3_arg(argv,7)};return 0.0;}
/* BoneInfo */
static double fn_BoneInfo(int argc,const char**argv){(void)argc;(void)argv;memset(&g_last_bone,0,sizeof(g_last_bone)); snprintf(g_last_bone.name,sizeof(g_last_bone.name),"%s",ss(argv[0])); g_last_bone.parent=ii(argv[1]); return 0.0;}
/* ModelSkeleton / ModelAnimation / MaterialMap / VrDeviceInfo: sıfırlanmış değer */
/* ModelSkeleton / ModelAnimation: bones ve keyframePoses dizilerinin sahibi
   raylib'dir, GCL'den kurulamazlar, bu yuzden kurucular sifirlar (IDE bu
   tipler icin parametre gostermez). Animasyon kullanimi handle tabanlidir:
   LoadModelAnimations -> UpdateModelAnimation(Ex), sayilar Raylib.LastInt(). */
static double fn_ModelSkeleton(int argc,const char**argv){(void)argc;(void)argv;memset(&g_last_skeleton,0,sizeof(g_last_skeleton));return 0.0;}
static double fn_ModelAnimation(int argc,const char**argv){(void)argc;(void)argv;memset(&g_last_manim,0,sizeof(g_last_manim));return 0.0;}
static double fn_MaterialMap(int argc,const char**argv){(void)argc;(void)argv;g_last_matmap=(MaterialMap){get_tex(ii(argv[0])),ci(argv,1),ff(argv[2])};return 0.0;}
/* VrDeviceInfo(hResolution, vResolution, hScreenSize, vScreenSize,
               eyeToScreenDistance, lensSeparationDistance, interpupillaryDistance
               [, lensDistortion0..3][, chromaAbCorrection0..3])
   IDE'nin imza yardiminda gosterdigi 7 argümanin aynisi; degerler gercekten
   yazilir ve LastVrDev*() ile geri okunur. Argumansiz cagri sifirlar. */
static double fn_VrDeviceInfo(int argc,const char**argv){
    memset(&g_last_vrdev,0,sizeof(g_last_vrdev));
    if (argc > 0) g_last_vrdev.hResolution            = ii(argv[0]);
    if (argc > 1) g_last_vrdev.vResolution            = ii(argv[1]);
    if (argc > 2) g_last_vrdev.hScreenSize            = ff(argv[2]);
    if (argc > 3) g_last_vrdev.vScreenSize            = ff(argv[3]);
    if (argc > 4) g_last_vrdev.eyeToScreenDistance    = ff(argv[4]);
    if (argc > 5) g_last_vrdev.lensSeparationDistance = ff(argv[5]);
    if (argc > 6) g_last_vrdev.interpupillaryDistance = ff(argv[6]);
    for (int i = 0; i < 4; i++) {
        if (argc > 7 + i)  g_last_vrdev.lensDistortionValues[i] = ff(argv[7 + i]);
        if (argc > 11 + i) g_last_vrdev.chromaAbCorrection[i]   = ff(argv[11 + i]);
    }
    return 0.0;
}

/* =========================================================================
   RAYLIB 6.1 — TİP KURUCULARI (handle tipleri: boş/geçerli handle üretir)
   ========================================================================= */
static double fn_Image(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_img((Image){0});return (double)g_last_handle;}
static double fn_Texture(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_tex((Texture){0});return (double)g_last_handle;}
static double fn_Texture2D(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_tex((Texture2D){0});return (double)g_last_handle;}
static double fn_TextureCubemap(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_tex((TextureCubemap){0});return (double)g_last_handle;}
static double fn_RenderTexture(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_rt((RenderTexture){0});return (double)g_last_handle;}
static double fn_RenderTexture2D(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_rt((RenderTexture2D){0});return (double)g_last_handle;}
static double fn_Font(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_font((Font){0});return (double)g_last_handle;}
static double fn_Shader(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_shader((Shader){0});return (double)g_last_handle;}
static double fn_Mesh(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_mesh((Mesh){0});return (double)g_last_handle;}
static double fn_Material(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_mat((Material){0});return (double)g_last_handle;}
static double fn_Model(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_model((Model){0});return (double)g_last_handle;}
static double fn_Wave(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_wave((Wave){0});return (double)g_last_handle;}
static double fn_AudioStream(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_ast((AudioStream){0});return (double)g_last_handle;}
static double fn_Sound(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_snd((Sound){0});return (double)g_last_handle;}
static double fn_Music(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_mus((Music){0});return (double)g_last_handle;}
static double fn_FilePathList(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_fpl((FilePathList){0});return (double)g_last_handle;}
static double fn_AutomationEventList(int argc,const char**argv){(void)argc;(void)argv;g_last_handle=reg_aevl((AutomationEventList){0});return (double)g_last_handle;}

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
    E(FileDataReset),E(FileDataPush),E(FileDataSize),E(FileDataAt),
    E(FileRename),E(FileRemove),E(FileCopy),E(FileMove),E(FileTextReplace),E(FileTextFindIndex),E(FileExists),
    E(DirectoryExists),E(IsFileExtension),E(IsFileHidden),E(GetFileLength),E(GetFileModTime),E(GetFileExtension),E(GetFileName),

    /* Types — raylib 6.1 tüm public tipleri (kurucular) */
    E(Quaternion),E(Color),E(Camera3D),E(RayCollision),E(Transform),E(BoneInfo),
    E(Image),E(Texture),E(Texture2D),E(TextureCubemap),E(RenderTexture),E(RenderTexture2D),
    E(Font),E(Shader),E(Mesh),E(MaterialMap),E(Material),
    E(ModelSkeleton),E(Model),E(ModelAnimation),
    E(Wave),E(AudioStream),E(Sound),E(Music),
    E(VrDeviceInfo),E(FilePathList),E(AutomationEventList),
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

    /* Config flags */
    E(FLAG_VSYNC_HINT),E(FLAG_FULLSCREEN_MODE),E(FLAG_WINDOW_RESIZABLE),E(FLAG_WINDOW_UNDECORATED),
    E(FLAG_WINDOW_HIDDEN),E(FLAG_WINDOW_MINIMIZED),E(FLAG_WINDOW_MAXIMIZED),E(FLAG_WINDOW_UNFOCUSED),
    E(FLAG_WINDOW_TOPMOST),E(FLAG_WINDOW_ALWAYS_RUN),E(FLAG_WINDOW_TRANSPARENT),E(FLAG_WINDOW_HIGHDPI),
    E(FLAG_WINDOW_MOUSE_PASSTHROUGH),E(FLAG_BORDERLESS_WINDOWED_MODE),E(FLAG_MSAA_4X_HINT),E(FLAG_INTERLACED_HINT),

    /* Trace log levels */
    E(LOG_ALL),E(LOG_TRACE),E(LOG_DEBUG),E(LOG_INFO),E(LOG_WARNING),E(LOG_ERROR),E(LOG_FATAL),E(LOG_NONE),

    /* Camera mode */
    E(CAMERA_CUSTOM),

    /* Keyboard keys */
    E(KEY_NULL),E(KEY_APOSTROPHE),E(KEY_COMMA),E(KEY_MINUS),E(KEY_PERIOD),E(KEY_SLASH),
    E(KEY_ZERO),E(KEY_ONE),E(KEY_TWO),E(KEY_THREE),E(KEY_FOUR),E(KEY_FIVE),E(KEY_SIX),E(KEY_SEVEN),E(KEY_EIGHT),E(KEY_NINE),
    E(KEY_SEMICOLON),E(KEY_EQUAL),
    E(KEY_B),E(KEY_C),E(KEY_E),E(KEY_F),E(KEY_G),E(KEY_H),E(KEY_I),E(KEY_J),E(KEY_K),E(KEY_L),E(KEY_M),
    E(KEY_N),E(KEY_O),E(KEY_P),E(KEY_Q),E(KEY_R),E(KEY_T),E(KEY_U),E(KEY_V),E(KEY_X),E(KEY_Y),
    E(KEY_LEFT_BRACKET),E(KEY_BACKSLASH),E(KEY_RIGHT_BRACKET),E(KEY_GRAVE),
    E(KEY_ENTER),E(KEY_TAB),E(KEY_BACKSPACE),E(KEY_INSERT),E(KEY_DELETE),
    E(KEY_RIGHT),E(KEY_LEFT),E(KEY_DOWN),E(KEY_UP),E(KEY_PAGE_UP),E(KEY_PAGE_DOWN),E(KEY_HOME),E(KEY_END),
    E(KEY_CAPS_LOCK),E(KEY_SCROLL_LOCK),E(KEY_NUM_LOCK),E(KEY_PRINT_SCREEN),E(KEY_PAUSE),
    E(KEY_F1),E(KEY_F2),E(KEY_F3),E(KEY_F4),E(KEY_F5),E(KEY_F6),E(KEY_F7),E(KEY_F8),E(KEY_F9),E(KEY_F10),E(KEY_F11),E(KEY_F12),
    E(KEY_LEFT_SHIFT),E(KEY_LEFT_CONTROL),E(KEY_LEFT_ALT),E(KEY_LEFT_SUPER),
    E(KEY_RIGHT_SHIFT),E(KEY_RIGHT_CONTROL),E(KEY_RIGHT_ALT),E(KEY_RIGHT_SUPER),E(KEY_KB_MENU),
    E(KEY_KP_0),E(KEY_KP_1),E(KEY_KP_2),E(KEY_KP_3),E(KEY_KP_4),E(KEY_KP_5),E(KEY_KP_6),E(KEY_KP_7),
    E(KEY_KP_8),E(KEY_KP_9),E(KEY_KP_DECIMAL),E(KEY_KP_DIVIDE),E(KEY_KP_MULTIPLY),E(KEY_KP_SUBTRACT),
    E(KEY_KP_ADD),E(KEY_KP_ENTER),E(KEY_KP_EQUAL),
    E(KEY_BACK),E(KEY_MENU),E(KEY_VOLUME_UP),E(KEY_VOLUME_DOWN),

    /* Mouse buttons / cursors */
    E(MOUSE_BUTTON_SIDE),E(MOUSE_BUTTON_EXTRA),E(MOUSE_BUTTON_FORWARD),E(MOUSE_BUTTON_BACK),
    E(MOUSE_LEFT_BUTTON),E(MOUSE_RIGHT_BUTTON),E(MOUSE_MIDDLE_BUTTON),
    E(MOUSE_CURSOR_DEFAULT),E(MOUSE_CURSOR_ARROW),E(MOUSE_CURSOR_IBEAM),E(MOUSE_CURSOR_CROSSHAIR),
    E(MOUSE_CURSOR_POINTING_HAND),E(MOUSE_CURSOR_RESIZE_EW),E(MOUSE_CURSOR_RESIZE_NS),
    E(MOUSE_CURSOR_RESIZE_NWSE),E(MOUSE_CURSOR_RESIZE_NESW),E(MOUSE_CURSOR_RESIZE_ALL),E(MOUSE_CURSOR_NOT_ALLOWED),

    /* Gamepad buttons / axes */
    E(GAMEPAD_BUTTON_UNKNOWN),E(GAMEPAD_BUTTON_LEFT_FACE_UP),E(GAMEPAD_BUTTON_LEFT_FACE_RIGHT),
    E(GAMEPAD_BUTTON_LEFT_FACE_DOWN),E(GAMEPAD_BUTTON_LEFT_FACE_LEFT),E(GAMEPAD_BUTTON_RIGHT_FACE_UP),
    E(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT),E(GAMEPAD_BUTTON_RIGHT_FACE_DOWN),E(GAMEPAD_BUTTON_RIGHT_FACE_LEFT),
    E(GAMEPAD_BUTTON_LEFT_TRIGGER_1),E(GAMEPAD_BUTTON_LEFT_TRIGGER_2),E(GAMEPAD_BUTTON_RIGHT_TRIGGER_1),
    E(GAMEPAD_BUTTON_RIGHT_TRIGGER_2),E(GAMEPAD_BUTTON_MIDDLE_LEFT),E(GAMEPAD_BUTTON_MIDDLE),
    E(GAMEPAD_BUTTON_MIDDLE_RIGHT),E(GAMEPAD_BUTTON_LEFT_THUMB),E(GAMEPAD_BUTTON_RIGHT_THUMB),
    E(GAMEPAD_AXIS_LEFT_X),E(GAMEPAD_AXIS_LEFT_Y),E(GAMEPAD_AXIS_RIGHT_X),E(GAMEPAD_AXIS_RIGHT_Y),
    E(GAMEPAD_AXIS_LEFT_TRIGGER),E(GAMEPAD_AXIS_RIGHT_TRIGGER),

    /* Material maps */
    E(MATERIAL_MAP_ALBEDO),E(MATERIAL_MAP_METALNESS),E(MATERIAL_MAP_NORMAL),E(MATERIAL_MAP_ROUGHNESS),
    E(MATERIAL_MAP_OCCLUSION),E(MATERIAL_MAP_EMISSION),E(MATERIAL_MAP_HEIGHT),E(MATERIAL_MAP_CUBEMAP),
    E(MATERIAL_MAP_IRRADIANCE),E(MATERIAL_MAP_PREFILTER),E(MATERIAL_MAP_BRDF),
    E(MATERIAL_MAP_DIFFUSE),E(MATERIAL_MAP_SPECULAR),

    /* Shader locations */
    E(SHADER_LOC_VERTEX_POSITION),E(SHADER_LOC_VERTEX_TEXCOORD01),E(SHADER_LOC_VERTEX_TEXCOORD02),
    E(SHADER_LOC_VERTEX_NORMAL),E(SHADER_LOC_VERTEX_TANGENT),E(SHADER_LOC_VERTEX_COLOR),
    E(SHADER_LOC_MATRIX_MVP),E(SHADER_LOC_MATRIX_VIEW),E(SHADER_LOC_MATRIX_PROJECTION),
    E(SHADER_LOC_MATRIX_MODEL),E(SHADER_LOC_MATRIX_NORMAL),E(SHADER_LOC_VECTOR_VIEW),
    E(SHADER_LOC_COLOR_DIFFUSE),E(SHADER_LOC_COLOR_SPECULAR),E(SHADER_LOC_COLOR_AMBIENT),
    E(SHADER_LOC_MAP_ALBEDO),E(SHADER_LOC_MAP_METALNESS),E(SHADER_LOC_MAP_NORMAL),E(SHADER_LOC_MAP_ROUGHNESS),
    E(SHADER_LOC_MAP_OCCLUSION),E(SHADER_LOC_MAP_EMISSION),E(SHADER_LOC_MAP_HEIGHT),E(SHADER_LOC_MAP_CUBEMAP),
    E(SHADER_LOC_MAP_IRRADIANCE),E(SHADER_LOC_MAP_PREFILTER),E(SHADER_LOC_MAP_BRDF),
    E(SHADER_LOC_VERTEX_BONEIDS),E(SHADER_LOC_VERTEX_BONEWEIGHTS),E(SHADER_LOC_MATRIX_BONETRANSFORMS),
    E(SHADER_LOC_VERTEX_INSTANCETRANSFORM),E(SHADER_LOC_MAP_DIFFUSE),E(SHADER_LOC_MAP_SPECULAR),

    /* Shader uniform / attribute types */
    E(SHADER_UNIFORM_FLOAT),E(SHADER_UNIFORM_VEC2),E(SHADER_UNIFORM_VEC3),E(SHADER_UNIFORM_VEC4),
    E(SHADER_UNIFORM_INT),E(SHADER_UNIFORM_IVEC2),E(SHADER_UNIFORM_IVEC3),E(SHADER_UNIFORM_IVEC4),
    E(SHADER_UNIFORM_UINT),E(SHADER_UNIFORM_UIVEC2),E(SHADER_UNIFORM_UIVEC3),E(SHADER_UNIFORM_UIVEC4),
    E(SHADER_UNIFORM_SAMPLER2D),
    E(SHADER_ATTRIB_FLOAT),E(SHADER_ATTRIB_VEC2),E(SHADER_ATTRIB_VEC3),E(SHADER_ATTRIB_VEC4),

    /* Pixel formats */
    E(PIXELFORMAT_UNCOMPRESSED_GRAYSCALE),E(PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA),E(PIXELFORMAT_UNCOMPRESSED_R5G6B5),
    E(PIXELFORMAT_UNCOMPRESSED_R8G8B8),E(PIXELFORMAT_UNCOMPRESSED_R5G5B5A1),E(PIXELFORMAT_UNCOMPRESSED_R4G4B4A4),
    E(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8),E(PIXELFORMAT_UNCOMPRESSED_R32),E(PIXELFORMAT_UNCOMPRESSED_R32G32B32),
    E(PIXELFORMAT_UNCOMPRESSED_R32G32B32A32),E(PIXELFORMAT_UNCOMPRESSED_R16),E(PIXELFORMAT_UNCOMPRESSED_R16G16B16),
    E(PIXELFORMAT_UNCOMPRESSED_R16G16B16A16),E(PIXELFORMAT_COMPRESSED_DXT1_RGB),E(PIXELFORMAT_COMPRESSED_DXT1_RGBA),
    E(PIXELFORMAT_COMPRESSED_DXT3_RGBA),E(PIXELFORMAT_COMPRESSED_DXT5_RGBA),E(PIXELFORMAT_COMPRESSED_ETC1_RGB),
    E(PIXELFORMAT_COMPRESSED_ETC2_RGB),E(PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA),E(PIXELFORMAT_COMPRESSED_PVRT_RGB),
    E(PIXELFORMAT_COMPRESSED_PVRT_RGBA),E(PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA),E(PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA),

    /* Texture filter / wrap / cubemap / font */
    E(TEXTURE_FILTER_POINT),E(TEXTURE_FILTER_BILINEAR),E(TEXTURE_FILTER_TRILINEAR),
    E(TEXTURE_FILTER_ANISOTROPIC_4X),E(TEXTURE_FILTER_ANISOTROPIC_8X),E(TEXTURE_FILTER_ANISOTROPIC_16X),
    E(TEXTURE_WRAP_REPEAT),E(TEXTURE_WRAP_CLAMP),E(TEXTURE_WRAP_MIRROR_REPEAT),E(TEXTURE_WRAP_MIRROR_CLAMP),
    E(CUBEMAP_LAYOUT_AUTO_DETECT),E(CUBEMAP_LAYOUT_LINE_VERTICAL),E(CUBEMAP_LAYOUT_LINE_HORIZONTAL),
    E(CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR),E(CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE),
    E(FONT_DEFAULT),E(FONT_BITMAP),E(FONT_SDF),

    /* Blend modes */
    E(BLEND_ALPHA),E(BLEND_ADDITIVE),E(BLEND_MULTIPLIED),E(BLEND_ADD_COLORS),
    E(BLEND_SUBTRACT_COLORS),E(BLEND_ALPHA_PREMULTIPLY),E(BLEND_CUSTOM),E(BLEND_CUSTOM_SEPARATE),

    /* Gestures */
    E(GESTURE_NONE),E(GESTURE_TAP),E(GESTURE_DOUBLETAP),E(GESTURE_HOLD),E(GESTURE_DRAG),
    E(GESTURE_SWIPE_RIGHT),E(GESTURE_SWIPE_LEFT),E(GESTURE_SWIPE_UP),E(GESTURE_SWIPE_DOWN),
    E(GESTURE_PINCH_IN),E(GESTURE_PINCH_OUT),

    /* N-patch layout */
    E(NPATCH_NINE_PATCH),E(NPATCH_THREE_PATCH_VERTICAL),E(NPATCH_THREE_PATCH_HORIZONTAL),
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
    E(DrawTriangle),E(DrawTriangleLines),E(DrawTriangleGradient),E(DrawTriangleLinesEx),
    E(DrawTriangleFan),E(DrawTriangleStrip),
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
    E(LoadTexture),E(LoadTextureFromImage),E(LoadTextureCubemap),E(LoadRenderTexture),E(LoadRenderTextureEx),
    E(IsTextureValid),E(UnloadTexture),
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
    E(AudioPush),E(AudioQueued),

    /* Color helpers */
    E(Fade),E(ColorToInt),E(ColorNormalize),E(ColorFromNormalized),E(ColorToHSV),E(ColorFromHSV),E(ColorTint),E(ColorBrightness),
    E(ColorContrast),E(ColorAlpha),E(ColorAlphaBlend),E(ColorLerp),E(GetColor),E(GetPixelColor),E(SetPixelColor),
    E(GetPixelDataSize),E(ColorIsEqual),

    /* Shapes constructors */
    E(Rectangle),E(Vector2),E(Vector3),E(Vector4),E(Matrix),E(Camera),E(Camera2D),E(Ray),E(BoundingBox),
    E(NPatchInfo),E(GlyphInfo),E(VrStereoConfig),E(AutomationEvent),

    /* Read-back — g_last_* scratch değerlerini okuma erişimcileri.
       Liste tek yerde tanımlı (GCL_RAYLIB_LAST_LIST); gövdeler ve bu satırlar
       aynı kaynaktan üretilir. */
#define GCL_LAST_ENTRY(NAME, EXPR) E(Last##NAME),
    GCL_RAYLIB_LAST_LIST(GCL_LAST_ENTRY)
#undef GCL_LAST_ENTRY
    E(LastMatrix),E(LastStringLen),E(LastStringByte),
    /* Parametreli okuma erisimcileri (indeks alirlar) */
    E(LastVrDevLensDistortion),E(LastVrDevChromaAb),
    E(LastVrProjection),E(LastVrViewOffset),E(LastVrPair),
};

GCL_EXPORT const GclNativeEntry *gcl_raylib_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
