/*

#native <RaylibRender>

Raylib.InitWindow(1280, 720, "GCL Project");

RaylibRender.FullScreenRenderResolution(320, 240);

while (!Raylib.WindowShouldClose()) {
    Raylib.BeginDrawing();
    Raylib.ClearBackground(Raylib.BLUE);
    ...                        # 320x240'ta cizilir
    Raylib.EndDrawing();       # ekrana gerilir
}

Bu modul, cizimi DUSUK COZUNURLUKTE yapip sonucu ekranin TAMAMINA gerer -
bir PS2 emulatorunun "internal resolution" ayari gibi. Amac performans:
fragment maliyeti cozunurlugun KARESIYLE artar, yani 1920x1080'de cizilen
bir kare 320x240'ta ~27 kat daha az is yapar. Zayif bir dizustu icin
"kabul edilebilir" ile "oynanamaz" arasindaki fark budur.

NEDEN CALISIR: isin pahali kismi (arazi, golgelendirme, isik, su) dusuk
cozunurlukte kosar; ekrana gerdirme ise TEK bir doku cizimidir - dortgen
basina bir fragment, yani ihmal edilebilir. Emulatorlerin yaptigi tam
olarak budur.

---- BURADA YAPILAN, RAYLIB'IN KENDI YOLUYLA YAPILAMAZ ----

raylib'in `BeginTextureMode`/`EndTextureMode` cifti vardir ve script bunlari
kendisi kullanabilir. Ama o zaman script'in HER cizim blogunu elle sarmasi
gerekir; modulun varlik sebebi bunu TEK SATIRA indirmektir. Bu yuzden:

  Raylib.BeginDrawing()  ->  modul ACIKSA  BeginTextureMode(rt)
  Raylib.EndDrawing()    ->  modul ACIKSA  EndTextureMode + ekrana ger + bitir

Bunun icin `gcl_raylib.c` iki yerde bu modulu ARAR (bkz. gcl_raylib.c:
`render_hook_begin` / `render_hook_end`). Modul yuklu DEGILSE sembol
bulunamaz ve her sey raylib'in normal yoluyla isler - yani bu modulu
kullanmayan hicbir program etkilenmez.

---- TUM RAYLIB CAGRILARI DLL'DEN COZULUR ----

Bu, dosyanin en onemli kurali. Moduller raylib TIPLERI icin kendi
kopyalarini tasir ama GL DURUMU (matris yigini, bagli framebuffer, viewport)
surec genelindedir ve TEK olmalidir. Statik `libraylib.a` uzerinden cagrilan
bir `BeginTextureMode`, Raylib.dll'in actigi pencereden HABERSIZ ayri bir
rlgl durumuna yazar; sonuc "hicbir sey degismedi" ya da "ekran karardi"
olur. `gcl_SimpleLight.c` bunu bir kez tam olarak yasamistir; ayni tuzaga
dusmemek icin buradaki her fonksiyon `gcl_module_symbol("Raylib.dll", ...)`
ile cozulur.

---- TAM EKRAN: BORDERLESS, EXCLUSIVE DEGIL ----

`FullScreenRenderResolution` pencereyi monitor boyutuna getirir ama bunu
`FLAG_BORDERLESS_WINDOWED_MODE` ile yapar.

NEDEN `FLAG_FULLSCREEN_MODE` DEGIL: o bayrak GLFW'de EXCLUSIVE fullscreen'dir
ve `glfwSetWindowMonitor(handle, monitor, ..., mode->refreshRate)` cagirir
(rcore_desktop_glfw.c). Monitorun VIDEO MODU degisir; Windows'ta bu,
alt-tab ile baska pencereye gecmeyi KILITLER - oyun arka planda kalir ve
kullanicinin masaustune donmesi engellenir.

`FLAG_BORDERLESS_WINDOWED_MODE` ise Windows'ta
`glfwSetWindowMonitor(handle, NULL, ...)` kullanir (ayni dosya, `#if
defined(_WIN32)` dali): pencere cercevesiz olur, monitor boyutuna oturur ve
video modu DEGISMEZ. Gorunum aynidir (ekran dolar), ama alt-tab normal
calisir.

SIRA ONEMLIDIR: raylib, borderless'i `FLAG_FULLSCREEN_MODE`'dan ONCE isler
ve gerekiyorsa once `ToggleFullscreen()` ile exclusive'den CIKAR. Yani
script zaten tam ekrandaysa gecis yine dogru yapilir.

---- FARE ----

Dusuk cozunurlukte cizerken pencere hala 1920 piksel genistir: `GetMouseX()`
0..1919 dondurur ama oyun 0..319 bekler. Bu yuzden fare olcegi
`SetMouseScale()` ile RT boyutuna eslenir. Bu olmadan fareden gelen her
koordinat yanlis olur - modul "ciziyor ama oynanmiyor" haline duserdi.

---- GETSCREENWIDTH NE DER ----

Modul acikken `Raylib.GetScreenWidth()`/`GetScreenHeight()` ve
`GetRenderWidth()`/`GetRenderHeight()` RT boyutunu dondurur (bkz.
gcl_raylib.c). Dogru olan budur: cizim yuzeyi artik 320x240'tir, ve
yerlesimini ekrana gore hesaplayan bir arayuz aksi halde yanlis yerlere
cizer.

---- EN-BOY ORANI: EKRANI DOLDURUR, ORANI KORUMAZ (STRETCH) ----

`composite()` RT'yi ekranin TAMAMINA esler: hedef dortgen `{0, 0,
ekran_w, ekran_h}`, kaynak ise RT'nin kendisidir. Ikisinin orani AYNI
olmadiginda goruntu YATAY ve DKEY olarak FARKLI oranlarda olceklenir -
yani GERILIR.

Ornek: RT 320x240 (4:3), monitor 1920x1080 (16:9).
    yatay olcek  = 1920 / 320 = 6.00
    dikey olcek  = 1080 / 240 = 4.50
    fark         = 6.00 / 4.50 = 1.333
Goruntu yatayda dikeyden ~%33 daha fazla buyur; DAireLER ELIPS olur,
kareler dikdortgen. Kenar bandi (letterbox) YOKTUR.

BU BILINCLI BIR SECIMDIR, bir gozden kacma degil:
  * ekranin tamami dolar - siyah bant yok, "dusuk cozunurluk" hissi
    azalir ve oyun alani hicbir piksel kaybetmez;
  * maliyeti degistirmez - gerdirme zaten TEK doku cizimidir, dortgen
    basina bir fragment. Kaynak/hedef oranini olcmek de ek is yapmaz;
  * fare olcegi AYNI oranla eslenir (`SetMouseScale`, g_w/ekran_w ve
    g_h/ekran_h). Yani imlec ile cizilen sey AYNI sekilde gerilir:
    bozulma rahatsiz edici olsa da TIKLAMALAR yanlis yere gitmez.

BOZULMA ISTEMIYORSAN: ic cozunurlugu hedefin oraniyla ayni sec. 16:9 bir
ekran icin 320x240 yerine 480x270 veya 384x216 kullan. Oran esitse
gerdirme zaten esit olceklenir ve bozulma tam olarak sifir olur - ayri
bir "letterbox modu" gerekmez.

ORANI KORUYAN (letterbox) BIR MOD YOKTUR ve bilerek eklenmedi: gerekli
oldugunda hedef dortgeni yeniden hesaplamak yeterlidir, ama o zaman
ekranin bir bolumu bos kalir ve kenar bandinin rengini/karartmasini
secmek gibi ikinci bir karar dogar. Ihtiyac netlesirse
`composite()` icindeki `dest` dortgeni bu kurala gore daraltilir.

---- BILINEN SINIR ----

`Raylib.GetMonitorWidth/Height` pencerenin gercek boyutunu vermeye devam
eder; olceklendirilmemistir. Ekranin TAMAMI hedeflendiginde ikisi ayni
seyi anlatir, ama pencere kucultulurse ayrilirlar.

Ikinci sinir: `composite()` ekran boyutunu HER karede yeniden okur, ama
RT boyutu `FullScreenRenderResolution` cagrildigi anda sabitlenir. Pencere
sonradan elle boyutlandirilirsa (tam ekrandan cikip kucultulurse) goruntu
YENI ekrana gerilmeye devam eder - bu istenen davranistir, ama artik
RT'nin orani ile ekranin orani buyuk olcude ayrismis olabilir.
*/

#include "gcl_module.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- sabitler ---------- */

#define RAYLIB_DLL "Raylib.dll"

#define DEFAULT_WIDTH  320
#define DEFAULT_HEIGHT 240

/* Bir doku boyutu icin alt ve ust sinir. Alt sinir: 16x16'nin altinda
   cizilecek bir sahne yoktur, ve 0 bir bolme hatasina goturur (fare
   olcegi g_w/screen_w). Ust sinir: ekrandan buyuk bir RT "dusuk
   cozunurluk" degildir, yanlislikla yazilmis bir sayidir. */
#define MIN_SIDE 16
#define MAX_SIDE 8192

/* ---------- raylib API'si, DLL'den cozulmus ---------- */

/* Hicbiri statik link EDILMEZ. Gerekcesi dosya basindaki "TUM RAYLIB
   CAGRILARI DLL'DEN COZULUR" notunda. */
typedef struct RenderApi {
    bool (*is_window_ready)(void);
    int  (*get_current_monitor)(void);
    int  (*get_monitor_width)(int);
    int  (*get_monitor_height)(int);
    int  (*get_screen_width)(void);
    int  (*get_screen_height)(void);

    void (*set_window_size)(int, int);
    void (*set_window_state)(unsigned int);
    void (*clear_window_state)(unsigned int);
    bool (*is_window_state)(unsigned int);

    RenderTexture2D (*load_render_texture)(int, int);
    void (*unload_render_texture)(RenderTexture2D);
    bool (*is_render_texture_valid)(RenderTexture2D);
    void (*begin_texture_mode)(RenderTexture2D);
    void (*end_texture_mode)(void);

    void (*begin_drawing)(void);
    void (*end_drawing)(void);
    void (*draw_texture_pro)(Texture2D, Rectangle, Rectangle, Vector2,
                             float, Color);
    void (*set_texture_filter)(Texture2D, int);
    void (*set_mouse_scale)(float, float);
} RenderApi;

static RenderApi g_api;
static int g_api_loaded = 0;

/* Bir sembolu cozerken `tried` bayragi tutulur: dlsym/GetProcAddress her
   karede cagrilacak kadar ucuz degildir ve sonuc zaten degismez. */
#define LOAD(field, name) \
    g_api.field = (void *)gcl_module_symbol(RAYLIB_DLL, name)

static void load_api(void) {
    if (g_api_loaded) return;
    g_api_loaded = 1;

    memset(&g_api, 0, sizeof(g_api));

    LOAD(is_window_ready,        "IsWindowReady");
    LOAD(get_current_monitor,    "GetCurrentMonitor");
    LOAD(get_monitor_width,      "GetMonitorWidth");
    LOAD(get_monitor_height,     "GetMonitorHeight");
    LOAD(get_screen_width,       "GetScreenWidth");
    LOAD(get_screen_height,      "GetScreenHeight");

    LOAD(set_window_size,        "SetWindowSize");
    LOAD(set_window_state,       "SetWindowState");
    LOAD(clear_window_state,     "ClearWindowState");
    LOAD(is_window_state,        "IsWindowState");

    LOAD(load_render_texture,    "LoadRenderTexture");
    LOAD(unload_render_texture,  "UnloadRenderTexture");
    LOAD(is_render_texture_valid,"IsRenderTextureValid");
    LOAD(begin_texture_mode,     "BeginTextureMode");
    LOAD(end_texture_mode,       "EndTextureMode");

    LOAD(begin_drawing,          "BeginDrawing");
    LOAD(end_drawing,            "EndDrawing");
    LOAD(draw_texture_pro,       "DrawTexturePro");
    LOAD(set_texture_filter,     "SetTextureFilter");
    LOAD(set_mouse_scale,        "SetMouseScale");
}

/* ---------- durum ---------- */

static int g_enabled = 0;      /* cizim RT'ye yonlendiriliyor mu */
static int g_in_frame = 0;     /* Begin gordük, End bekleniyor */
static int g_width = 0;        /* ic cozunurluk */
static int g_height = 0;
static RenderTexture2D g_target;
static int g_target_valid = 0;

/* Modulun DEGISTIRDIGI pencere durumu; Disable() bunlari geri koyar. */
static int g_saved_screen_w = 0;
static int g_saved_screen_h = 0;
static int g_made_fullscreen = 0;

/* Farenin en son hangi ekran boyutuna gore olceklendigi. Pencere
   boyutu degistiginde olcek yeniden uygulanir. */
static int g_mouse_scaled_w = 0;
static int g_mouse_scaled_h = 0;

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* ---------- pencere ---------- */

static int clamp_side(double v, int fallback) {
    int side = (v > 0.0) ? (int)(v + 0.5) : fallback;
    if (side < MIN_SIDE) side = MIN_SIDE;
    if (side > MAX_SIDE) side = MAX_SIDE;
    return side;
}

/* Pencereyi ekranin tamamina ac - BORDERLESS, EXCLUSIVE DEGIL.

   `SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE)` cagrilir ve bayrak
   YALNIZCA gerekliyse degisir; zaten borderless ise hicbir sey olmaz.
   `ToggleFullscreen` bilerek kullanilmaz: o bir AC/KAPA anahtaridir,
   bir hedef degil.

   NEDEN `FLAG_FULLSCREEN_MODE` DEGIL - gerekcesi dosya basindaki "TAM
   EKRAN: BORDERLESS, EXCLUSIVE DEGIL" notunda: exclusive fullscreen
   monitorun video modunu degistirir ve alt-tab'i kilitler.

   Pencere boyutu ELLE AYARLANMAZ. `ToggleBorderlessWindowed()` zaten
   monitorun video modu boyutuna oturur (rcore_desktop_glfw.c) ve onceki
   boyutu `CORE.Window.previousScreen`e yazar. Burada `SetWindowSize`
   cagirmak o kaydi MONITOR boyutuyla kirletirdi: `Disable()` sonrasi
   pencere, kullanicinin actigi 320x240 yerine 1920x1080 olarak geri
   gelirdi. */
static void go_fullscreen(void) {
    g_made_fullscreen = 0;

    if (g_api.is_window_state && g_api.set_window_state &&
        !g_api.is_window_state(FLAG_BORDERLESS_WINDOWED_MODE)) {
        g_api.set_window_state(FLAG_BORDERLESS_WINDOWED_MODE);
        g_made_fullscreen = 1;
    }
}

static void restore_window(void) {
    if (g_made_fullscreen && g_api.clear_window_state) {
        g_api.clear_window_state(FLAG_BORDERLESS_WINDOWED_MODE);
    }
    /* Pencere boyutu YALNIZCA tam ekrandan cikarken geri konur; aksi halde
       FullScreenRenderResolution'dan sonra elle boyut veren bir script'in
       karari ezilirdi. */
    if (g_made_fullscreen && g_saved_screen_w > 0 && g_saved_screen_h > 0 &&
        g_api.set_window_size) {
        g_api.set_window_size(g_saved_screen_w, g_saved_screen_h);
    }
    g_made_fullscreen = 0;
    g_saved_screen_w = 0;
    g_saved_screen_h = 0;
}

/* ---------- cevreleme ---------- */

static void release_target(void) {
    if (g_target_valid && g_api.unload_render_texture) {
        g_api.unload_render_texture(g_target);
    }
    g_target_valid = 0;
    memset(&g_target, 0, sizeof(g_target));
}

static void release_all(void) {
    release_target();
    g_enabled = 0;
    g_in_frame = 0;
    g_width = 0;
    g_height = 0;
    g_mouse_scaled_w = 0;
    g_mouse_scaled_h = 0;
    if (g_api.set_mouse_scale) g_api.set_mouse_scale(1.0f, 1.0f);
}

/* Farenin olcegini RT boyutuna esle - gerekcesi dosya basindaki "FARE"
   notunda. Pencere boyutu degistiginde yeniden uygulanir. */
static void apply_mouse_scale(void) {
    int sw = g_api.get_screen_width ? g_api.get_screen_width() : 0;
    int sh = g_api.get_screen_height ? g_api.get_screen_height() : 0;
    if (sw <= 0 || sh <= 0 || !g_api.set_mouse_scale) return;
    if (sw == g_mouse_scaled_w && sh == g_mouse_scaled_h) return;

    g_api.set_mouse_scale((float)g_width / (float)sw,
                          (float)g_height / (float)sh);
    g_mouse_scaled_w = sw;
    g_mouse_scaled_h = sh;
}

/* RT icerigini ekranin TAMAMINA ciz.

   KAYNAK DORTGENIN YUKSEKLIGI NEGATIFTIR ve bu bir yazim hatasi degildir:
   raylib'de render texture'lar ALT-SOL kokenlidir, yani oldugu gibi
   cizilirse goruntu TERS gelir. Negatif yukseklik dikey ekseni cevirir -
   raylib'in kendi orneklerinde de kullanilan yol budur. */
static void composite(void) {
    int sw = g_api.get_screen_width ? g_api.get_screen_width() : 0;
    int sh = g_api.get_screen_height ? g_api.get_screen_height() : 0;
    if (sw <= 0 || sh <= 0 || !g_api.draw_texture_pro) return;

    Rectangle source = { 0.0f, 0.0f,
                         (float)g_target.texture.width,
                        -(float)g_target.texture.height };
    Rectangle dest   = { 0.0f, 0.0f, (float)sw, (float)sh };
    Vector2   origin = { 0.0f, 0.0f };

    g_api.draw_texture_pro(g_target.texture, source, dest, origin, 0.0f,
                           WHITE);
}

/* ---------- gcl_raylib.c'nin CAGRIDIGI KAPILAR ----------

   Bu uc sembol modulun KENDI icin degil, Raylib modulu icin vardir:
   `Raylib.BeginDrawing()` ve `Raylib.EndDrawing()` cerceveyi buraya
   devreder (bkz. gcl_raylib.c: render_hook_begin/render_hook_end). */

/* 1 dondururse cagiran `BeginDrawing()` CAGRMAZ - cerceve RT icinde
   baslamistir.

   SIRA `BeginDrawing` SONRA `BeginTextureMode`DIR ve bu, raylib'in kendi
   render-texture ornegiyle (core_render_texture.c) aynidir. Ikisi yer
   degistirirse cerceve calismaz:

     * `BeginDrawing` `CORE.Window.rendering` bayragini kurar, pencere
       viewport'unu ve ekran olcegini hazirlar. `BeginTextureMode` BU
       BAYRAGI KURMAZ.
     * Bu yuzden yalnizca `BeginTextureMode` cagrilirsa `EndDrawing` "once
       BeginDrawing cagir" diye uyarip HICBIR SEY cizmez ve ekran hic
       guncellenmez - pencere bos ya da donuk kalir.

   Once pencere cercevesi acilir, sonra cizim yuzeyi RT'ye cevrilir; RT
   kapaninca cerceve HALA aciktir ve geri germe ayni cercevenin icine
   yapilir. */
GCL_EXPORT int gcl_render_begin(void) {
    load_api();
    if (!g_enabled || !g_target_valid || !g_api.begin_texture_mode) return 0;
    if (g_in_frame) return 1;      /* zaten RT icinde: ikinci Begin zararsiz */
    if (!g_api.begin_drawing) return 0;

    g_api.begin_drawing();
    g_api.begin_texture_mode(g_target);
    g_in_frame = 1;
    return 1;
}

/* 1 dondururse cagiran `EndDrawing()` CAGRMAZ - cerceve burada bitmistir.
   0 dondururse modul devrede degildir ve raylib'in normal yolu isler. */
GCL_EXPORT int gcl_render_end(void) {
    load_api();
    if (!g_enabled || !g_target_valid || !g_in_frame) return 0;
    if (!g_api.end_texture_mode || !g_api.begin_drawing || !g_api.end_drawing) {
        g_in_frame = 0;
        return 0;
    }

    /* Cizim yuzeyini pencereye geri cevir. Cerceve KAPANMAZ: `BeginDrawing`
       bir kez, yukarida cagrildi. */
    g_api.end_texture_mode();
    g_in_frame = 0;

    /* Ayni cercevenin ikinci yarisi: RT'yi ekrana ger ve cerceveyi kapat.
       `composite` cagrilmadan once fare olcegi tazelenir, cunku bu kare
       icin okunacak fare konumu RT uzayinda olmalidir. */
    apply_mouse_scale();
    composite();
    g_api.end_drawing();
    return 1;
}

/* RT boyutu, modul acikken; degilse `fallback` aynen geri verilir. */
GCL_EXPORT int gcl_render_screen_width(int fallback) {
    return g_enabled ? g_width : fallback;
}

GCL_EXPORT int gcl_render_screen_height(int fallback) {
    return g_enabled ? g_height : fallback;
}

/* Pencere kapanmadan once cagrilir (bkz. gcl_raylib.c: fn_CloseWindow).
   RT'nin GL kaynaklari pencere yokken serbest birakilamaz. */
GCL_EXPORT void gcl_render_shutdown(void) {
    load_api();
    release_all();
}

/* ---------- uyeler ---------- */

/* FullScreenRenderResolution(width, height)

   Pencereyi ekranin tamamina acar ve bundan sonra HER kareyi `width` x
   `height` cozunurlugunde cizip ekrana gerer. Cozunurluk verilmezse
   320x240 kullanilir - PS2 donemi icin tipik ve zayif bir dizustunde
   akici kalan bir deger. */
static double fn_fullscreen_resolution(int argc, const char **argv) {
    int want_w, want_h;
    load_api();

    if (!g_api.is_window_ready || !g_api.is_window_ready()) {
        fprintf(stderr,
                "RaylibRender.FullScreenRenderResolution: window not ready "
                "(call Raylib.InitWindow first) - call skipped\n");
        return 0.0;
    }
    if (!g_api.load_render_texture) {
        fprintf(stderr,
                "RaylibRender.FullScreenRenderResolution: Raylib module is not "
                "loaded - call skipped\n");
        return 0.0;
    }

    want_w = clamp_side(num_arg(argc, argv, 0), DEFAULT_WIDTH);
    want_h = clamp_side(num_arg(argc, argv, 1), DEFAULT_HEIGHT);

    /* Onceki hedefi birak: RT boyutu degisirse eskisi sizar (yeni bir
       doku ayirmak gerekir) ve pencere durumu zaten ayarlanmistir. */
    release_target();

    /* Ayarlanmadan ONCE kaydet - Disable() bunlari geri koyar. Yalnizca
       ilk cagrida: ikinci bir cagri kaydedilmis degeri ezmemelidir. */
    if (!g_enabled && g_api.get_screen_width && g_api.get_screen_height) {
        g_saved_screen_w = g_api.get_screen_width();
        g_saved_screen_h = g_api.get_screen_height();
    }

    go_fullscreen();

    g_target = g_api.load_render_texture(want_w, want_h);

    if (g_api.is_render_texture_valid && !g_api.is_render_texture_valid(g_target)) {
        fprintf(stderr,
                "RaylibRender.FullScreenRenderResolution: could not make a "
                "%dx%d render target - call skipped\n", want_w, want_h);
        release_target();
        restore_window();
        return 0.0;
    }

    /* NOKTA SUZME. Emulator gorunumu budur: RT ekrandan BUYUK oldugu icin
       dogrusal suzme her pikseli bulandirir; nokta suzme kare pikseli net
       tutar. GL_LINEAR isteyen bir script SetTextureFilter ile
       degistirebilir - doku tutamaci Raylib.LastSlot/Handle ile degil,
       dogrudan `RaylibRender` altinda verilir. */
    if (g_api.set_texture_filter) {
        g_api.set_texture_filter(g_target.texture, TEXTURE_FILTER_POINT);
    }

    g_target_valid = 1;
    g_width = want_w;
    g_height = want_h;
    g_enabled = 1;
    g_in_frame = 0;

    /* Ekran boyutu az once degistigi icin olcegi hemen tazele. */
    g_mouse_scaled_w = 0;
    g_mouse_scaled_h = 0;
    apply_mouse_scale();

    return 0.0;
}

/* Disable() - normal cizime don. Pencere ve fare olcegi geri konur. */
static double fn_disable(int argc, const char **argv) {
    (void)argc; (void)argv;
    load_api();

    if (g_enabled) {
        release_all();
        restore_window();
    }
    return 0.0;
}

static double fn_enabled(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_enabled ? 1.0 : 0.0;
}

static double fn_internal_width(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)g_width;
}

static double fn_internal_height(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)g_height;
}

#define E(NAME, STR) { STR, fn_##NAME }

static const GclNativeEntry g_entries[] = {
    E(fullscreen_resolution, "FullScreenRenderResolution"),
    E(disable,               "Disable"),
    E(enabled,               "Enabled"),
    E(internal_width,        "InternalWidth"),
    E(internal_height,       "InternalHeight"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibrender_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
