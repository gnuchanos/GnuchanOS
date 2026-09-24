/*

#native <RaylibSimpleLight>

RaylibSimpleLight.SunLight SUN = RaylibSimpleLight.CreateSun(Raylib.YELLOW);
SUN.Rotate.x = 0;
SUN.Rotate.z = 90;
SUN.Rotate.y = 0;
SUN.Update();

RaylibSimpleLight.SunLight _0_point_light = RaylibSimpleLight.CreatePointLight(Raylib.YELLOW, SUN, 0, 1, 0);
# light color, sun light, x, y, z

_0_point_light.Position.x = 0;
_0_point_light.Position.y = 0;
_0_point_light.Position.z = 0;

_0_point_light.Sun = SUN; # if sun exist sun + point light ve balance
_0_point_light.Update();
_0_point_light.Draw(); # draw glow cube for check where is the light
*/

/* Backend.

   IKI ISIK TURU, TEK STRUCT.

       GUNES (directional) : yonu vardir      -> `Rotate` kullanilir
       NOKTA (point)       : konumu vardir    -> `Position` kullanilir

   Ayrim TUTAMAC ARALIGIYLA yapilir: gunes 0..SUN_MAX-1, nokta isiklar
   PLIGHT_HANDLE_BASE'ten baslar.

   YAPRAK SIRASI (`f_sunlight[]`, `last_sunlight[]` ve `g_native_slot_maps[]`
   ile BIREBIR ayni; toplami 8'dir):

       0 Handle
       1 PosX     2 PosY     3 PosZ
       4 RotX     5 RotY     6 RotZ
       7 Sun

   ---------- CAGRI SOZLESMESI ----------

   Runner, bir uye cagrisinda degiskenin TUM yapraklarini argv olarak gecirir:
   `light.Update()` -> argc == SUN_SLOT_COUNT, argv[i] = yaprak i.

   `CreateSun` ve `CreatePointLight` UYE DEGILDIR; argumanlari kendi
   duzenindedir ve konum `1 + SUN_SLOT_COUNT` TEMELINDEN okunur.

   ---------- RENKLER `color_bits()` ILE OKUNUR, `strtoul` ILE DEGIL ----------

   Runner skalerleri `%.17g` ile yazar. 0xFF6464FF (Raylib.YELLOW) buraya
   "4.294965245e+09" gibi BILIMSEL GOSTERIMLE ulasir. `strtoul` o dizede
   yalnizca bastaki "4"u okur ve renk (4,0,0) - yani SIYAH - cikar. Isik kupu
   tam da bu yuzden kapkara gorunuyordu. `atof` tum degeri okur, `color_bits`
   isaretli tamsayiya cevirip 32 biti maskeler; paket deseni aynen korunur.

   ---------- GUNES ARTIK SKYBOX'TAN GELIR ----------

   RaylibSKYBOX.dll gunesi HEM CIZER hem hesaplar. Yuklu ise isik YONUNU,
   RENGINI ve SIDDETINI oradan alir; degilse `Rotate.*`'tan hesaplanir.

   ---------- IKI AYRI ISIK YOLU: DOGRUDAN ve DOLAYLI ----------

       dogrudan : lights[i].color.rgb * max(dot(normal, light), 0)
       dolayli  : ambient.rgb

   Gunes tam tepedeyken yalnizca ust yuzu aydinlatir; yan yuzler `NdotL = 0`
   alip simsiyah kalirdi - DOLAYLI terim bu yuzden vardir.

   DOGRUDAN terim SUN_DIRECT_MAX ile sinirlidir: sinirsiz birakilinca
   `dogrudan + dolayli` 1'i asar ve gunese bakan yuz BEYAZA kirpilir.

   ---------- NOKTA ISIGI ----------

   Nokta isigin YONU yoktur: KONUMU ve MESAFE SONUMU vardir. Sonum shader'da
   hesaplanir (bkz. assets/lighting.fs: POINT_ATTEN_K1/K2); bu modul yalnizca
   konumu ve TAM parlakliktaki rengi yazar.

   Gunes YOKKEN (SUN = null) dunya kapkara olur ve geriye yalnizca nokta
   isiklar kalir - bkz. fn_update'teki gunes-yok dali. */

#include "gcl_module.h"

/* windows.h must not drag in wingdi.h/winuser.h: their Rectangle/CloseWindow
   clash with raylib's names. Only the loader API is needed here. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include "raylib.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SUN_SLOT_COUNT   8
#define SUN_MAX          4
#define SUN_DISTANCE     1000.0f
#define SUN_DEG2RAD      0.017453292519943295

/* Cekirdek kutunun kenar uzunlugu (dunya birimi). Isiga etkisi yoktur. */
#define SUN_DEBUG_CUBE   0.50f

/* Cekirdegin etrafina cizilen ic ice kabuk sayisi, en dis kabugun cekirdege
   gore buyume katsayisi ve kabuklarin tepe katki siddeti. Kabuklar buyurken
   alfasi duser: disa dogru sonumlenen yumusak bir hale olusur - "glow". */
#define GLOW_SHELLS      8
#define GLOW_GROW        3.0f
#define GLOW_ALPHA       16

/* CEKIRDEK KUTUNUN TOPLAMALI KATKISI. 255 IDI VE YANLISTI.

   Toplamali karisimda alfa EKLENEN miktardir; katkilar ust uste TOPLANIR.
   Eski degerlerle toplam 255'i fersah fersah asiyordu:

       kabuklar : GLOW_ALPHA*(0.875^2 + 0.75^2 + ...) = 70*2.19  ~ 153
       cekirdek : alpha = 255 (tam renk)                          ~ 255
       ----------------------------------------------------------------
       toplam                                                     ~ 408

   Yani isigin merkezi HER ZAMAN saf beyaza doyuyordu: 255'in uzerine cikan
   her sey kirpilir ve ne isik rengi ne de kutu sekli kalir - "isik bir
   beyazlik gibi patliyor" goruntusunun sebebi budur.

   Butce 255'in ALTINDA tutulur; PARLAK bir sahnenin (gokyuzu, su) uzerine
   binen pay da hesaba katilir. Yeni toplam ~90/255: karanlik sahnede net bir
   hale, aydinlik sahnede bile BEYAZA VARMAYAN bir katki. */
#define GLOW_ALPHA       16
#define GLOW_CORE_ALPHA  55

/* Gece dogrudan isigin sonme bandi. `sunDir`in Y bileseni gece negatiftir;
   negatif isik vektoru taban yuzunde `dot(normal, light) > 0` yapar ve kubun
   ALT YUZU aydinlanir. Isik bu yuzden yalnizca gunes ufkun USTUNDEYKEN
   acilir. */
#define SUN_HORIZON_FADE 0.20f

/* Dolayli isik. GUNDUZ > GECE. */
#define SUN_AMBIENT_DAY   0.18f
#define SUN_AMBIENT_NIGHT 0.05f

/* Gokyuzu parlakliginin ambient'e giren EN AZ payi: gunes ufka indiginde de
   dolgu ayakta kalsin, gece tamamen siyah olmasin. */
#define SUN_AMBIENT_SKY_FLOOR 0.35f

/* Dogrudan isigin tavani: `dogrudan + dolayli` toplami 1'i asmasin. */
#define SUN_DIRECT_MAX 0.80f

/* raylib's Light.type values (raylib.h) */
#define SUN_LIGHT_DIRECTIONAL 0
#define SUN_LIGHT_POINT       1

/* Noktasal isiklar guneslerle AYNI GL slotlarini paylasir. */
#define PLIGHT_MAX        8
#define PLIGHT_GL_FIRST   1

/* TUTAMAC ARALIGI: nokta isiklar bu tabandan baslar. */
#define PLIGHT_HANDLE_BASE 1000

/* YAPRAK SIRASI - f_sunlight[] ve last_sunlight[] ile BIREBIR ayni olmali. */
enum {
    S_HANDLE = 0,
    S_POS_X, S_POS_Y, S_POS_Z,
    S_ROT_X, S_ROT_Y, S_ROT_Z,
    S_SUN
};

static double       g_slot[SUN_SLOT_COUNT] = {
    -1.0,           /* Handle: isik yok */
     0.0, 0.0, 0.0,  /* Position */
     0.0, 0.0, 90.0, /* Rotate: elevation 90 = tepede, makul varsayilan */
    -1.0            /* Sun: bagimsiz */
};
static unsigned int g_sun_color[SUN_MAX]   = {0};
static int          g_sun_count            = 0;
static int          g_last_sun             = -1;

typedef struct {
    int          used;
    unsigned int color;
    float        x, y, z;
    int          sun;
} PointLight;

static PointLight g_plight[PLIGHT_MAX];
static int        g_plight_count = 0;

/* Tutamac -> kayit indeksi; gunes/gecersiz tutamac -1 doner. */
static int plight_index(int handle) {
    int i;
    if (handle < PLIGHT_HANDLE_BASE) return -1;
    i = handle - PLIGHT_HANDLE_BASE;
    if (i < 0 || i >= PLIGHT_MAX) return -1;
    if (!g_plight[i].used) return -1;
    return i;
}

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* Paketlenmis renk -> Color.

   `Color` GCL tarafinda ISARETLI int olarak tasinir; Raylib.WHITE
   (0xFFFFFFFF) buraya -1 olarak ulasir. Double'i dogrudan unsigned'a cevirmek
   negatifte TANIMSIZ davranistir; isaretli tamsayi uzerinden gidip 32 biti
   maskeledigimizde desen aynen korunur (bkz. gcl_SimpleMesh.c: color_bits). */
static unsigned int color_bits(double v) {
    long long i = (long long)v;
    return (unsigned int)(i & 0xFFFFFFFFLL);
}

static Color unpack_color(unsigned int v) {
    Color c;
    c.r = (unsigned char)(v & 0xFF);
    c.g = (unsigned char)((v >> 8) & 0xFF);
    c.b = (unsigned char)((v >> 16) & 0xFF);
    c.a = (unsigned char)((v >> 24) & 0xFF);
    return c;
}

/* ================= CIZIM: Raylib.dll SEMBOLLERI =================

   EN ONEMLI NOT BU DOSYADA BUDUR.

   Bu modul, RAYLIB TIPLERI icin kendi statik kopyasini link eder
   (`libraylib.a`: Model, Matrix, Color...). Ama o kopyanin rlgl MATRIS
   YIGINI ve GL DURUMU AYRIDIR ve BOSTUR (identity).

   Script ise kamerayi `Raylib.BeginMode3D(...)` ile **Raylib.dll**'e kurar.
   Modulun statik `DrawCube`'u DLL'in kamerasini GORMEDIGI icin cizdigi kutu
   NDC uzayinda (0,1,0) - yani ekranin TEPE KENARINDA, tek bir noktaya -
   duser ve GORUNMEZ. "Sahnede kup yok, sadece isik var" halinin tek sebebi
   budur ve bir kez tam olarak bu yasanmistir.

   COZUM: GL'e dokunan HER fonksiyon DLL'den cozulur. `CUBE.Draw()`'in
   calismasinin sebebi de budur (bkz. Modules/gcl_SimpleMesh.c, ayni desen).

   Semboller tembel cozulur; DLL yoksa statik kopyaya dusulur ki pencere
   acmadan calisan bir program bozulmasin. */
#ifdef _WIN32
static void *raylib_symbol(const char *name) {
    HMODULE h = GetModuleHandleA("Raylib.dll");
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *raylib_symbol(const char *name) { (void)name; return NULL; }
#endif

static void draw_cube(Vector3 at, float side, Color c) {
    static void (*fn)(Vector3, float, float, float, Color) = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (void (*)(Vector3, float, float, float, Color))raylib_symbol("DrawCube"); }
    if (fn) fn(at, side, side, side, c);
    else    DrawCube(at, side, side, side, c);
}

/* ---------- PARLAMA (GLOW): UC SART, UCU DE ZORUNLU ----------

   Kabuklar AYNI merkeze ust uste cizilir. Asagidakilerden biri eksikse glow
   ya gorunmez ya da RENGINI KAYBEDER - ucu de ayri ayri yasandi:

     1. VARSAYILAN SHADER. Sahne arazinin KENDI programiyla cizilir
        (assets/lighting.fs) ve SIS o programda uygulanir (bkz. gcl_fog.c:
        "Karisim arazinin KENDI fragment shader'inda olur"). Glow ayni
        programdan gecerse sise boyanir: sisli bolgeye, yani ufka bakinca
        glow sise karisip gorunmez olur - sikayetin TAM sebebi budur. Glow
        bu yuzden VARSAYILAN shader ile cizilir: ne sis, ne isiklandirma,
        yalnizca kendi rengi.

     2. DERINLIK MASKESI KAPALI, TESTI ACIK. Maske acikken en dis kabuk
        derinlik yazar ve icteki kabuklarin HEPSI elenir; geriye duz, tek bir
        kutu kalir - "glow yok" goruntusu budur. Test ise ACIK kalmalidir ki
        dunya geometrisi glow'u dogru sekilde kapatsin: bir tepenin arkasinda
        kalan isik gorunmemeli.

     3. TOPLAMALI KARISIM. Normal alfa karisiminda ust uste binen saydam
        yuzeyler birbirini ORTER ve sonuc gri bulamac olur; parlaklik
        ARTMAZ. Toplamali karisimda her kabuk oncekine EKLENIR ve merkez
        doyarak gercekten PARLAR. Toplamali karisimda alfa dogrudan EKLENEN
        miktardir; alfa = 0 hicbir sey eklemez, sonum bu yuzden bedava gelir.

   SIRA ONEMLIDIR: once birikmis cizim BOSALTILIR. Bekleyen vertex'ler
   (arazi, su, kutu) sahne programina aittir ve shader degistirilmeden ONCE
   cizilmeleri gerekir; aksi halde onlar da varsayilan shader'la cizilir ve
   sahne bozulur. raylib bir cizim listesi tutar ve shader degistirmek
   listeyi kendiliginden bosaltmaz.

   Modul, sectigi shader'i glow_end()te GERI KOYAR: cagiran icin gorunmez
   olmali, aksi halde script'in BeginShaderMode blogundaki sonraki cizimler
   ve fogy.Update()in hedefledigi program sasardi. aktif sahne shader'inin
   tutamaci `gcl_raylib_light_shader()` ile ogrenilir (bkz. gcl_raylib.c);
   -1 ise geri koyacak bir program yoktur ve varsayilan shader yeter. */
static int g_scene_shader = -1;

static void glow_begin(void) {
    typedef void         (*GclVoidFn)(void);
    typedef void         (*GclIntFn)(int);
    typedef unsigned int (*GclGetIdFn)(void);
    typedef int *        (*GclGetLocsFn)(void);
    typedef void         (*GclSetShaderFn)(unsigned int, int *);
    typedef int          (*GclCurrentFn)(void);

    static GclVoidFn      flush          = NULL;
    static GclSetShaderFn set_shader     = NULL;
    static GclGetIdFn     default_id     = NULL;
    static GclGetLocsFn   default_locs   = NULL;
    static GclVoidFn      depth_mask_off = NULL;
    static GclVoidFn      depth_test_on  = NULL;
    static GclIntFn       set_blend      = NULL;
    static GclCurrentFn   scene_current  = NULL;
    static int tried = 0;

    if (!tried) {
        tried          = 1;
        flush          = (GclVoidFn)      raylib_symbol("rlDrawRenderBatchActive");
        set_shader     = (GclSetShaderFn) raylib_symbol("rlSetShader");
        default_id     = (GclGetIdFn)     raylib_symbol("rlGetShaderIdDefault");
        default_locs   = (GclGetLocsFn)   raylib_symbol("rlGetShaderLocsDefault");
        depth_mask_off = (GclVoidFn)      raylib_symbol("rlDisableDepthMask");
        depth_test_on  = (GclVoidFn)      raylib_symbol("rlEnableDepthTest");
        set_blend      = (GclIntFn)       raylib_symbol("rlSetBlendMode");
        scene_current  = (GclCurrentFn)   raylib_symbol("gcl_raylib_light_shader");
    }

    if (flush) flush();

    g_scene_shader = scene_current ? scene_current() : -1;

    /* VARSAYILAN PROGRAM — `rlSetShader` ILE, `rlEnableShader` ILE DEGIL.

       `rlEnableShader` YALNIZCA `glUseProgram` cagirir; rlgl'nin
       `RLGL.State.currentShaderId` degerine DOKUNMAZ (rlgl.h:1824). Birikmis
       vertex'leri cizen `rlDrawRenderBatch` ise program olarak AYNI
       `currentShaderId` degerini kullanir (rlgl.h:3036). Yani
       `rlEnableShader(default_id())` glow'un HANGI programla cizilecegini
       DEGISTIRMIYORDU: kutular sahnenin (assets/lighting.fs) programiyla
       ciziliyor ve SIS orada, fragment gecidinde uygulaniyordu - glow'un
       sise karismasinin TAM sebebi budur.

       Durumu GERCEKTEN degistiren API `rlSetShader`tir: bekleyen partiyi
       once mevcut programla bosaltir, sonra `currentShaderId`i degistirir
       (rlgl.h:4592). Glow artik varsayilan programla cizilir: ne sis, ne
       isiklandirma, yalnizca kendi rengi. */
    if (set_shader && default_id && default_locs) set_shader(default_id(), default_locs());

    /* DERINLIK TESTI ACIKCA ACILIR. Once glow sahnenin ARKASINDA ciziliyordu
       ve derinlik tamponu bostu; simdi sahne ONCE cizilir (bkz. glow_end), o
       yuzden glow'un dunya geometrisi tarafindan dogru sekilde KAPATILMASI
       bu testin acik olmasina baglidir: son cizim (su) saydamligi icin testi
       kapatmissa glow tepelerin uzerine tasardi. */
    if (depth_test_on)  depth_test_on();
    if (depth_mask_off) depth_mask_off();

    if (set_blend) set_blend(BLEND_ADDITIVE);
    else           BeginBlendMode(BLEND_ADDITIVE);
}

static void glow_end(void) {
    typedef void         (*GclVoidFn)(void);
    typedef void         (*GclIntFn)(int);
    typedef void         (*GclSetShaderFn)(unsigned int, int *);
    typedef Shader       (*GclShaderGetFn)(int);

    static GclVoidFn      flush         = NULL;
    static GclSetShaderFn set_shader    = NULL;
    static GclIntFn       set_blend     = NULL;
    static GclVoidFn      depth_mask_on = NULL;
    static GclShaderGetFn shader_get    = NULL;
    static int tried = 0;

    if (!tried) {
        tried         = 1;
        flush         = (GclVoidFn)      raylib_symbol("rlDrawRenderBatchActive");
        set_shader    = (GclSetShaderFn) raylib_symbol("rlSetShader");
        set_blend     = (GclIntFn)       raylib_symbol("rlSetBlendMode");
        depth_mask_on = (GclVoidFn)      raylib_symbol("rlEnableDepthMask");
        shader_get    = (GclShaderGetFn) raylib_symbol("gcl_raylib_shader_get");
    }

    /* ---------- BU BOSALTMA ZORUNLUDUR ----------

       Birikmis kabuklar BURADA, hala VARSAYILAN shader ve TOPLAMALI karisim
       bagliyken cizilmelidir.

       `rlSetBlendMode` ve `rlEnableShader` bekleyen vertex'leri KENDILIGINDEN
       bosaltmaz; yalnizca GL durumunu degistirirler. Asagidaki uc cagri
       (karisim -> sahne programi -> derinlik) once yapilirsa kuyruktaki kutu
       vertex'leri bir SONRAKI cizimle, yani SAHNE programiyla birlikte
       bosalir. Sonuc: glow `assets/lighting.fs`ten gecer, SIS ona da boyanir
       ve ufka - yani sisin oldugu yere - bakinca glow sise karisip KAYBOLUR.
       Bildirilen hatanin tam sebebi budur; dosyanin basindaki 1. sart
       ("VARSAYILAN SHADER") bu bosaltma olmadan GECERSIZDIR.

       Once bosalt, sonra durumu geri koy. */
    if (flush) flush();

    if (set_blend) set_blend(BLEND_ALPHA);
    else           BeginBlendMode(BLEND_ALPHA);

    /* Sahne programini GERI KOY — yine `rlSetShader` ile. Yalnizca `id`
       yetmez: uniform KONUMLARI da (`locs`) geri yazilmalidir, yoksa
       sonraki cizimler eski programin konumlarina uniform yollar. */
    if (set_shader && shader_get && g_scene_shader >= 0) {
        Shader s = shader_get(g_scene_shader);
        set_shader(s.id, s.locs);
    }
    g_scene_shader = -1;

    if (depth_mask_on) depth_mask_on();
}

/* ---------- uniforms ---------- */

typedef void (*GclShaderSetLightFn)(int handle, int index, int enabled, int type,
                                    float px, float py, float pz,
                                    float tx, float ty, float tz,
                                    unsigned int color);
typedef void (*GclShaderSetAmbientFn)(int handle, unsigned int ambient);

static GclShaderSetLightFn shader_set_light(void) {
    static GclShaderSetLightFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclShaderSetLightFn)raylib_symbol("gcl_raylib_shader_set_light"); }
    return fn;
}

static GclShaderSetAmbientFn shader_set_ambient(void) {
    static GclShaderSetAmbientFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclShaderSetAmbientFn)raylib_symbol("gcl_raylib_shader_set_ambient"); }
    return fn;
}

/* ---------- RaylibSkybox.dll: gunesin TEK kaynagi ---------- */
typedef int    (*GclSkyboxSunVecFn)(float *out3);
typedef double (*GclSkyboxSunPowerFn)(void);

#ifdef _WIN32
static void *skybox_symbol(const char *name) {
    HMODULE h = GetModuleHandleA("RaylibSkybox.dll");
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *skybox_symbol(const char *name) { (void)name; return NULL; }
#endif

static GclSkyboxSunVecFn skybox_sun_direction(void) {
    static GclSkyboxSunVecFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclSkyboxSunVecFn)skybox_symbol("gcl_skybox_sun_direction"); }
    return fn;
}

static GclSkyboxSunVecFn skybox_sun_color(void) {
    static GclSkyboxSunVecFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclSkyboxSunVecFn)skybox_symbol("gcl_skybox_sun_color"); }
    return fn;
}

static GclSkyboxSunPowerFn skybox_sun_power(void) {
    static GclSkyboxSunPowerFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclSkyboxSunPowerFn)skybox_symbol("gcl_skybox_sun_power"); }
    return fn;
}

/* ---------- hedef shader ---------- */
typedef int (*GclShaderCurrentFn)(void);

#ifdef _WIN32
static void *shader_symbol(const char *dll, const char *name) {
    HMODULE h = GetModuleHandleA(dll);
    return h ? (void *)GetProcAddress(h, name) : NULL;
}

/* Raylib.dll remembers the shader Raylib.BeginShaderMode() selected, and that
   value survives EndShaderMode(). This is the ONLY correct target: the lights
   must land in the same GL program the model is drawn with. */
static GclShaderCurrentFn raylib_light_shader(void) {
    static GclShaderCurrentFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclShaderCurrentFn)shader_symbol("Raylib.dll", "gcl_raylib_light_shader"); }
    return fn;
}

static GclShaderCurrentFn shader_current(void) {
    static GclShaderCurrentFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclShaderCurrentFn)shader_symbol("RaylibShader.dll", "gcl_shader_current"); }
    return fn;
}
#else
static GclShaderCurrentFn raylib_light_shader(void) { return NULL; }
static GclShaderCurrentFn shader_current(void) { return NULL; }
#endif

/* The shader whose lights are being fed, or -1 when there is none.

   Order matters: the shader being DRAWN with wins, and "the last one created"
   is only the fallback. */
static int target_shader(void) {
    GclShaderCurrentFn selected = raylib_light_shader();
    int handle = selected ? selected() : -1;
    GclShaderCurrentFn created;
    if (handle >= 0) return handle;
    created = shader_current();
    return created ? created() : -1;
}

/* ---------- colour helpers ---------- */

/* Packed rengi bir katsayiyla olcekle; alfa korunur. */
static unsigned int scale_color(unsigned int color, float k) {
    unsigned int r, g, b;
    if (!(k > 0.0f)) k = 0.0f;
    if (k > 1.0f)    k = 1.0f;
    r = (unsigned int)((float)( color        & 0xFF) * k);
    g = (unsigned int)((float)((color >>  8) & 0xFF) * k);
    b = (unsigned int)((float)((color >> 16) & 0xFF) * k);
    return r | (g << 8) | (b << 16) | (color & 0xFF000000u);
}

/* Dolayli isik dolgusu. `day` 0 = gece, 1 = gunduz. */
static unsigned int ambient_fill(unsigned int color, float day) {
    float k;
    if (!(day > 0.0f)) day = 0.0f;
    if (day > 1.0f)    day = 1.0f;
    k = SUN_AMBIENT_NIGHT + (SUN_AMBIENT_DAY - SUN_AMBIENT_NIGHT)*day;
    return scale_color(color, k);
}

/* A 0..1 float triple -> the same packed layout scale_color() reads. */
static unsigned int pack_rgb(const float rgb[3], float scale) {
    int r = (int)(rgb[0] * scale * 255.0f + 0.5f);
    int g = (int)(rgb[1] * scale * 255.0f + 0.5f);
    int b = (int)(rgb[2] * scale * 255.0f + 0.5f);
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (unsigned int)r | ((unsigned int)g << 8) | ((unsigned int)b << 16) | 0xFF000000u;
}

/* ---------- members ---------- */

/* CreatePointLight(color, sun, x, y, z) -> noktasal isik tutamaci.

   ARGUMAN DUZENI: `SUN` bir struct'tir ve runner onu BILDIRIM SIRASINDA
   skaler yapraklara acar. Konum bu yuzden `1 + SUN_SLOT_COUNT` TEMELINDEN
   okunur; sabit indeks yazmak, struct'a bir alan eklendiginde sessizce
   yanlis yapragi konum sanmak demekti. */
static double fn_create_point_light(int argc, const char **argv) {
    unsigned int color = 0xFFFFFFFFu;   /* Raylib.WHITE */
    int   sun = -1;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    int   i = g_plight_count;
    int   base = 1 + SUN_SLOT_COUNT;

    /* Renk `color_bits(atof(...))` ile okunur - gerekcesi dosya basindaki
       "RENKLER" notunda. `strtoul` bilimsel gosterimi kesip SIYAH verirdi. */
    if (argc > 0 && argv[0]) color = color_bits(atof(argv[0]));
    if (argc > 1 && argv[1]) sun   = (int)atof(argv[1]);
    if (argc >= base + 3) {
        x = (float)num_arg(argc, argv, base + 0);
        y = (float)num_arg(argc, argv, base + 1);
        z = (float)num_arg(argc, argv, base + 2);
    }

    if (i < 0 || i >= PLIGHT_MAX) return -1.0;
    g_plight[i].used  = 1;
    g_plight[i].color = color;
    g_plight[i].x     = x;
    g_plight[i].y     = y;
    g_plight[i].z     = z;
    g_plight[i].sun   = sun;
    g_plight_count++;

    g_slot[S_HANDLE] = (double)(PLIGHT_HANDLE_BASE + i);
    g_slot[S_POS_X]  = (double)x;
    g_slot[S_POS_Y]  = (double)y;
    g_slot[S_POS_Z]  = (double)z;
    g_slot[S_SUN]    = (double)sun;
    return (double)(PLIGHT_HANDLE_BASE + i);
}

/* CreateSun(color) -> sun index (the script keeps it in SunLight.Handle). */
static double fn_create_sun(int argc, const char **argv) {
    /* Renk `color_bits(atof(...))` ile okunur - gerekcesi fn_create_point_light
       basindaki notta ve dosya basindaki "RENKLER" notunda. */
    unsigned int color = (argc > 0 && argv[0]) ? color_bits(atof(argv[0]))
                                               : 0xFFFFFFFFu;   /* Raylib.WHITE */
    int index = g_sun_count;

    if (index >= SUN_MAX) return -1.0;
    g_sun_color[index] = color;
    g_sun_count++;
    g_last_sun = index;

    g_slot[S_HANDLE] = (double)index;
    g_slot[S_POS_X]  = 0.0;
    g_slot[S_POS_Y]  = 0.0;
    g_slot[S_POS_Z]  = 0.0;
    g_slot[S_ROT_X]  = 0.0;
    g_slot[S_ROT_Y]  = 0.0;
    g_slot[S_ROT_Z]  = 90.0;   /* noon is the sane default */
    g_slot[S_SUN]    = -1.0;
    return (double)index;
}

/* Update() - feed the shader.

   UC YOL: nokta isigi / gunes yok (dunya kapkara) / gunes. */
static double fn_update(int argc, const char **argv) {
    int   index = (int)num_arg(argc, argv, S_HANDLE);
    float el    = (float)(num_arg(argc, argv, S_ROT_Z) * SUN_DEG2RAD);
    float az    = (float)(num_arg(argc, argv, S_ROT_Y) * SUN_DEG2RAD);
    float dir_x = sinf(az) * cosf(el);
    float dir_y = sinf(el);
    float dir_z = cosf(az) * cosf(el);
    float sky_dir[3];
    float sky_rgb[3];
    int   enabled = 1;
    float ambient_day = 1.0f;
    unsigned int color;
    unsigned int light_color;
    unsigned int ambient_color;
    GclShaderSetLightFn   set_light   = shader_set_light();
    GclShaderSetAmbientFn set_ambient = shader_set_ambient();
    int shader = target_shader();

    /* ---------- YOL 1: NOKTA ISIGI ---------- */
    {
        int pi = plight_index(index);
        if (pi >= 0) {
            int   gl = PLIGHT_GL_FIRST + pi;
            PointLight *P = &g_plight[pi];
            float px = (float)num_arg(argc, argv, S_POS_X);
            float py = (float)num_arg(argc, argv, S_POS_Y);
            float pz = (float)num_arg(argc, argv, S_POS_Z);
            int   sun_ref = (int)num_arg(argc, argv, S_SUN);

            P->x = px; P->y = py; P->z = pz;
            P->sun = sun_ref;

            g_slot[S_HANDLE] = (double)index;
            g_slot[S_POS_X]  = px;
            g_slot[S_POS_Y]  = py;
            g_slot[S_POS_Z]  = pz;
            g_slot[S_SUN]    = (double)sun_ref;

            /* UST SINIR SHADER'IN `lights[]` BOYUTUDUR. 4 iken 4. nokta
               isik (slot 4) hicbir uniform'a yazilmiyordu: `_3_point_light`
               SESSIZCE olu kaliyordu ve konumunu PLAYER'a esitlemek de ise
               yaramiyordu - isik shader'a hic ULASMIYOR. Uc yerdeki sayi AYNI
               olmak zorundadir: burasi, gcl_raylib.c: GCL_LIGHT_SLOTS ve
               assets/lighting.fs: MAX_LIGHTS. */
            if (gl < 5 && shader >= 0 && set_light) {
                set_light(shader, gl, 1, SUN_LIGHT_POINT,
                          px, py, pz,
                          0.0f, 0.0f, 0.0f,  /* hedef nokta isikta kullanilmaz */
                          P->color);
            }
            return 0.0;
        }
    }

    /* ---------- YOL 2: GUNES YOK -> DUNYA KAPKARA ----------

       Gecersiz tutamac -1, "gunes yok" demektir. Eskiden buradan sessizce
       cikiliyordu ve IKI sey yanlis kaliyordu: slot 0 ACIK kaliyordu (son
       karede yazilan yon uniform'da duruyordu) ve ambient son degeri
       tasiyordu, bu yuzden dunya kapkara OLMAZDI.

       Ikisi de burada temizlenir. Noktasal slotlara DOKUNULMAZ; silinen
       gunestir, onlar degil. Ambient'i sifirlamak nokta isiklarin aydinlattigi
       yeri karartmaz: onlarin katkisi `ambient` teriminden AYRI toplanir. */
    if (index < 0 || index >= SUN_MAX) {
        if (shader >= 0 && set_light) {
            set_light(shader, 0, 0, SUN_LIGHT_DIRECTIONAL,
                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u);
        }
        if (shader >= 0 && set_ambient) set_ambient(shader, 0u);
        return 0.0;
    }

    /* ---------- YOL 3: GUNES ---------- */
    color = g_sun_color[index];

    g_slot[S_HANDLE] = (double)index;
    g_slot[S_ROT_X]  = num_arg(argc, argv, S_ROT_X);
    g_slot[S_ROT_Y]  = num_arg(argc, argv, S_ROT_Y);
    g_slot[S_ROT_Z]  = num_arg(argc, argv, S_ROT_Z);
    g_slot[S_SUN]    = -1.0;
    g_last_sun       = index;

    {
        GclSkyboxSunVecFn   get_dir   = skybox_sun_direction();
        GclSkyboxSunVecFn   get_color = skybox_sun_color();
        GclSkyboxSunPowerFn get_power = skybox_sun_power();
        float intensity = SUN_DIRECT_MAX;

        if (get_dir && get_dir(sky_dir)) {
            dir_x = sky_dir[0];
            dir_y = sky_dir[1];
            dir_z = sky_dir[2];
        }
        if (get_color && get_color(sky_rgb)) {
            color = pack_rgb(sky_rgb, 1.0f);
        }
        /* Guc NEGATIF ise skybox henuz bir kare cizmedi (bkz. gcl_skybox.c:
           `g_sun_valid`). O durumda modulun varsayilanlari korunur: gece
           varsaymak sahneyi haksiz yere karartirdi. */
        if (get_power) {
            float power = (float)get_power();
            if (power >= 0.0f) {
                if (power > 1.0f) power = 1.0f;
                intensity = SUN_DIRECT_MAX * (0.72f + 0.28f * power);
                ambient_day = SUN_AMBIENT_SKY_FLOOR +
                              (1.0f - SUN_AMBIENT_SKY_FLOOR) * power;
            }
        }

        /* GUNES UFKUN ALTINDAYKEN DOGRUDAN ISIK KAPANIR: negatif isik vektoru
           taban yuzunde `dot(normal, light) > 0` yapar ve kubun ALT YUZU
           aydinlanir. Ufka yaklastikca yumusak soner. */
        if (dir_y > 0.0f) {
            float fade = dir_y / SUN_HORIZON_FADE;
            if (fade > 1.0f) fade = 1.0f;
            light_color = scale_color(color, intensity * fade);
        } else {
            enabled = 0;
            light_color = scale_color(color, 0.0f);
        }

        /* Ambient, DOGRUDAN ISIK renginden AYRI hesaplanir - gece
           `light_color` 0'a cekilirken ambient ay isigini tasimaya devam
           eder. */
        ambient_color = ambient_fill(color, ambient_day);
    }

    if (shader < 0 || !set_light) return 0.0;

    set_light(shader, index, enabled, SUN_LIGHT_DIRECTIONAL,
              dir_x * SUN_DISTANCE, dir_y * SUN_DISTANCE, dir_z * SUN_DISTANCE,
              0.0f, 0.0f, 0.0f,                     /* target: the world origin */
              light_color);

    if (set_ambient) set_ambient(shader, ambient_color);
    return 0.0;
}

/* ---------- girisi `g_slot`a geri yaz ----------

   NEDEN ZORUNLU: runner bir struct metot cagrisindan SONRA degiskenin TUM
   yapraklarini `LastSlot(i)` kanalindan GERI YAZAR (bkz. gcl_runner.c:
   store_slots_into_var). Modul `g_slot`u tazelemezse, geri yazim modulun SON
   isleminden kalan degerleri degiskene kopyalar.

   Uc nokta isikta sonuc soyleydi: `_0_point_light.Update()` `g_slot`u 1000
   yapar, sonra `_0_point_light.Draw()` geri yaziminda onu EZER; uc degisken
   de en son islenen isigin tutamacina ve KONUMUNA coker. Yani uc isik
   bildirilse de ekranda TEK isik cizilir - bildirilen hata tam budur.

   Her modulun Draw/Update yolu bu yuzden GIRISI AYNEN GERI YAZAR
   (bkz. gcl_SimpleMesh.c: read_mesh, gcl_fog.c/gcl_skybox.c: read_slots).
   `fn_update` bunu zaten yapiyordu; eksik olan `fn_draw`di. */
static void sync_slots_from_args(int argc, const char **argv) {
    int i;
    for (i = 0; i < SUN_SLOT_COUNT; i++) {
        if (argv && i < argc && argv[i]) g_slot[i] = atof(argv[i]);
    }
}

/* ---------- Draw ----------

   ISIGIN YERINI GOSTEREN PARLAYAN KUP.

   Yalnizca GORSEL bir dogrulama aracidir: isiga etkisi yoktur, uniform'lara
   yazmaz. Cizimin uc on sarti `glow_begin()` notunda; ozetle: VARSAYILAN
   shader (sis ve isiklandirma devre disi), derinlik maskesi KAPALI / testi
   ACIK, TOPLAMALI karisim. Rengi isigin KENDI rengidir.

   BeginMode3D'nin ICINDE cagrilmalidir, cunku dunya uzayi koordinatlari
   kullanir. */
static double fn_draw(int argc, const char **argv) {
    int     pi;
    Color   c;
    Vector3 at;

    /* GIRISI AYNEN GERI YAZ - gerekcesi `sync_slots_from_args` notunda.
       Bu olmadan `Draw()`, degiskenin Handle/Position/Sun yapraklarini
       modulun son degerleriyle ezip butun nokta isiklarini TEK isiga
       cokertiyordu. */
    sync_slots_from_args(argc, argv);

    pi = plight_index((int)g_slot[S_HANDLE]);
    if (pi < 0) return 0.0;   /* gunes veya gecersiz tutamac: cizilecek yer yok */

    at = (Vector3){ g_plight[pi].x, g_plight[pi].y, g_plight[pi].z };
    c  = unpack_color(g_plight[pi].color);

    /* ---------- GLOW: ic ice buyuyen, sonumlenen kabuklar ----------

       Dis kabuktan ice dogru: her kabuk bir oncekinden KUCUK ve DAHA PARLAK.
       Toplamali karisimda katkilar ust uste binince merkez doyar ve isik
       gercekten PARLAR.

       ALFA SONUMU: toplamali karisimda alfa dogrudan EKLENEN miktardir;
       alfa = 0 hicbir sey eklemez. KARESI alinir, cunku gercek isikta siddet
       merkezden uzaklastikca hizla duser - dogrusal sonum yapay bir "kure"
       gibi gorunurdu. */
    glow_begin();

    for (int i = GLOW_SHELLS; i >= 1; i--) {
        float t     = (float)i / (float)GLOW_SHELLS;  /* 1 = en dis kabuk */
        float side  = SUN_DEBUG_CUBE * (1.0f + (GLOW_GROW - 1.0f) * t);
        float k     = 1.0f - t;                       /* 1 = cekirdek */
        int   a     = (int)((float)GLOW_ALPHA * k * k);
        Color shell = c;

        if (a <= 0) continue;
        shell.a = (unsigned char)(a > 255 ? 255 : a);
        draw_cube(at, side, shell);
    }

    /* CEKIRDEK: kabuklarin toplandigi merkez. Isigin KONUMUNU gosterir.
       TAM OPAK (alpha 255) DEGILDIR - gerekcesi GLOW_CORE_ALPHA notunda:
       toplamali karismda 255, kabuklarin uzerine binince merkezi saf beyaza
       doyurur ve geriye ne isik rengi ne de kutu sekli kalir. */
    c.a = (unsigned char)GLOW_CORE_ALPHA;
    draw_cube(at, SUN_DEBUG_CUBE, c);

    glow_end();
    return 0.0;
}

/* LastColor() -> the packed colour of the sun created/updated last. */
static double fn_last_color(int argc, const char **argv) {
    (void)argc; (void)argv;
    if (g_last_sun < 0 || g_last_sun >= SUN_MAX) return 0.0;
    return (double)g_sun_color[g_last_sun];
}

/* Named read-back accessors: the runtime's declaration path fills a struct from
   the module through THESE names (see fill_native_from_last_slot). */
static double fn_handle(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_HANDLE];
}
static double fn_pos_x(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_POS_X];
}
static double fn_pos_y(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_POS_Y];
}
static double fn_pos_z(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_POS_Z];
}
static double fn_rot_x(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_ROT_X];
}
static double fn_rot_y(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_ROT_Y];
}
static double fn_rot_z(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_ROT_Z];
}
static double fn_sun(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_SUN];
}

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)SUN_SLOT_COUNT;
    {
        int i = (int)atof(argv[0]);
        if (i < 0 || i >= SUN_SLOT_COUNT) return 0.0;
        return g_slot[i];
    }
}

/* GCL scripts address these members by the names the module documents. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_sun,         "CreateSun"),
    E(create_point_light, "CreatePointLight"),
    E(update,             "Update"),
    E(draw,               "Draw"),
    E(handle,             "Handle"),
    E(pos_x,              "PosX"),
    E(pos_y,              "PosY"),
    E(pos_z,              "PosZ"),
    E(rot_x,              "RotX"),
    E(rot_y,              "RotY"),
    E(rot_z,              "RotZ"),
    E(sun,                "Sun"),
    E(last_color,         "LastColor"),
    E(last_slot,          "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplelight_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
