/*

#native <RaylibSimpleWater>

RaylibSimpleWater.Water water = RaylibSimpleWater.CreateSimpleWaterFromOBJ("water.obj");
water.Color = Raylib.WHITE; # default
water.Position.x = 0; # default
water.Position.y = 0; # default
water.Position.z = 0; # default

water.Scale.x = 1; # default
water.Scale.y = 1; # default
water.Scale.z = 1; # default

water.Rotate.x = 0; # default
water.Rotate.y = 0; # default
water.Rotate.z = 0; # default

water.Update(); # update shader
water.Draw();   # 3B yuzey. BeginMode3D'nin ICINDE cagrilir.


#check
water.FPS_InWater();
water.FPS_IsSwimming();

# extra parameter
water.Alpha = 0.5f;
water.Reflection = 0.5f;   # skybox yansimasi
water.Refraction = 0.2f;
water.Fresnel = 0.5f;
water.WaveStrength = 1.0f;
water.WaveSpeed = 1.0f;
water.WaveScale = 1.0f;
water.Foam = 0.0f;

RaylibSimpleWater.UnloadSimpleWaterFromOBJ(water);

*/

/* Backend.

   IKI AYRI IS:

     1. YUZEY (Draw) - 3B su mesh'ini dalga shader'iyla cizer
        (bkz. gcl_water_shader.h). BeginMode3D'nin ICINDE cagrilir. Suyun
        ustunden bakildiginda gokyuzu yansimasi (bulutlar + gunes dahil),
        Fresnel ve kopuk bu asamadadir.

     2. DOLGU (DrawUnderwaterEffect) - BeginMode3D KAPANDIKTAN sonra cagrilir
        (bkz. gcl_underwater_shader.h). EKRANI DUZ MAVIYE BOYAMAZ; her piksel
        icin kameradan bir isin atar ve o isinin GERCEK su yuzeyini vurup
        vurmadigini sorar:
           * Vuruyorsa piksel su ile boyanir.
           * Vurmuyorsa (isin suyu iskalayip gokyuzune kacmistir) piksel
             ATILIR: orada zaten cizilmis gokyuzu/arazi/su yuzeyi aynen kalir.
        Ekrandaki su siniri boylece UYDURMA bir dalga degil, 3B mesh'in
        kameradan gecirilmis izdusumudur; yukaridan gorunen sirt ile asagidan
        gorunen sinir AYNI suya aittir.
        Ayrica suyun icinden YUKARI bakildiginda 3B yuzeyin ALTI farkli
        cizilir: Snell penceresi (gokyuzu), toplam ic yansima, gunes
        parlamasi. Yukaridan gorunus ile asagidan gorunus boylece ayrilir.

   SU BIR HACIMDIR. `water.obj` duz bir levha degil, kapali bir hacimdir;
   "suda miyim" sorusu YUKSEKLIK karsilastirmasiyla degil, hacmin ICINDE
   olmakla yanitlanir (bkz. water_raycast). Duz levha sayan bir sorgu, suyun
   DISINDA, arazinin cukurunda duran oyuncuyu da "suda" sayardi.

   SLOT TABLOSU (SharedPipeline/gcl_native_types.c'deki `f_water`,
   GCL/SimpleRunner/gcl_runner.c'deki `last_water[]` ve `g_native_slot_maps[]`
   ile BIREBIR ayni olmak zorundadir; WATER_SLOT_COUNT = 19):

       0  Color          1  Handle
       2  PosX           3  PosY           4  PosZ
       5  RotX           6  RotY           7  RotZ
       8  ScaleX         9  ScaleY        10  ScaleZ
      11  Alpha         12  Reflection    13  Refraction
      14  Fresnel       15  WaveStrength  16  WaveSpeed
      17  WaveScale     18  Foam

   CAGRI SOZLESMESI: runner, bir uye cagrisinda degiskenin 19 skaler yapragini
   argv olarak GECIRIR (`water.Draw()` -> argc == 19, argv[i] = slot i).
   `CreateSimpleWaterFromOBJ` bir uye DEGILDIR: argv[0] dosya adidir. Bu yuzden
   okuyucu, ancak TAM 19 arguman geldiginde yuvalari tazeler.

   Bu tip YALNIZCA SUYU TANIMLAR: yuzme esigi, kapsul olcusu ya da "yuzuyor
   sayilma" karari BURADA YASAMAZ. Karar tek bir yerde, oyuncunun kendi
   modulunde verilir (bkz. gcl_raylib_fps.c) ve bu modul yalnizca SORAR. */

#include "gcl_module.h"

/* windows.h'den yalnizca yukleyici API'si gerekir; NOGDI/NOUSER wingdi.h ve
   winuser.h'yi disarida tutar, cunku onlarin Rectangle/CloseWindow adlari
   raylib'in adlariyla catisir. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include "raylib.h"

/* raymath header-only'dir ve IKI mod makrosundan biri olmadan RMAPI duz C99
   `inline`'a acilir; o durumda MatrixMultiply'in disaridan tanimi olmadigi
   icin LINK asamasinda patlar. */
#define RAYMATH_STATIC_INLINE
#include "raymath.h"

#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcl_water_shader.h"
#include "gcl_underwater_shader.h"

#define WATER_SLOT_COUNT 19
#define WATER_MAX        64
#define WATER_NAME_MAX   512
#define WATER_DEG2RAD    0.017453292519943295f
#define WATER_TWO_PI     6.283185307179586f

/* Suyun kapsul ayak izinin yaricapi. Oyuncu kapsulu (bkz.
   gcl_SimpleCollision.c: COL_PLAYER_RADIUS) ile AYNI sayidir. */
#define WATER_BODY_RADIUS 0.3f
#define WATER_RING_SAMPLES 4

/* Ust yuz ile taban arasindaki en kucuk fark. Bundan ince bir kafes ACIK
   kabul edilir (derinlik sinirsiz): tek yuzlu bir "water.obj" de calissin. */
#define WATER_PAIR_MIN 0.02f
#define WATER_RAY_TOP  1000.0f

/* Dalga parametreleri MODELIN KENDI boyutuna gore turetilir; ikisi de SABIT
   OLAMAZ. Oran secildiginde her boyuttaki su AYNI gorunur. */
#define WATER_WAVE_CRESTS    6.0f
#define WATER_WAVE_AMP_RATIO 0.018f

/* ---------- DALGA: MUTLAK DUNYA OLCEGI ----------
   Genlik ve frekans MODELIN genisliginden turetiliyordu
   (`extent*0.018`, `2*PI*6/extent`). Genis bir levhada dalgalar metrelerce
   yuksek ve onlarca metre aralikli oluyor, normaller yatay cikiyor ve yuzey
   her yerde gokyuzunu yansitip DUZ BEYAZ gorunuyordu. Ayrica dalga boyu
   modelin boyutuna bagli oldugu icin kameranin gordugu dalgayla ilgisi
   kalmiyordu.

   Dalga suyun KENDI olcegidir; model ne kadar buyuk olursa olsun aynidir. */
#define WATER_WAVE_AMP_WORLD 0.16f   /* metre: tepe genligi (WaveStrength ile olceklenir) */
#define WATER_WAVE_LEN_WORLD 4.00f   /* metre: tepe-tepe uzaklik (WaveScale ile olceklenir) */

#define WATER_DEF_ALPHA      0.5
#define WATER_DEF_REFLECT    0.5
#define WATER_DEF_REFRACT    0.2
#define WATER_DEF_FRESNEL    0.5
#define WATER_DEF_WAVE_STR   1.0
#define WATER_DEF_WAVE_SPEED 1.0
#define WATER_DEF_WAVE_SCALE 1.0
#define WATER_DEF_FOAM       0.0

/* Taban su renkleri. `Color` bunlarin uzerine CARPILIR; varsayilan
   Raylib.WHITE oldugu icin hicbir sey yazilmazsa bu renkler aynen gorunur. */
#define WATER_BASE_SHALLOW_R 0.16f
#define WATER_BASE_SHALLOW_G 0.42f
#define WATER_BASE_SHALLOW_B 0.50f
#define WATER_BASE_DEEP_R    0.03f
#define WATER_BASE_DEEP_G    0.12f
#define WATER_BASE_DEEP_B    0.22f

/* Skybox yuklu degilse kullanilan gokyuzu gradyani ve gunes rengi. */
#define WATER_FALLBACK_TOP_R 0.29f
#define WATER_FALLBACK_TOP_G 0.56f
#define WATER_FALLBACK_TOP_B 0.89f
#define WATER_FALLBACK_HOR_R 0.66f
#define WATER_FALLBACK_HOR_G 0.83f
#define WATER_FALLBACK_HOR_B 0.96f
#define WATER_FALLBACK_SUN_R 1.00f
#define WATER_FALLBACK_SUN_G 0.98f
#define WATER_FALLBACK_SUN_B 0.93f

/* ---------- DOLGUNUN SABITLERI ----------

   UW_FADE_RISE / UW_FADE_FALL: doldurmanin acilma/kapanma hizi (/sn). Ani
   ACMA/KAPAMA yoktur; su cizgisini gecerken ekran bir karede kapanmaz.

   UW_FILL_MIN: dolgu ancak bu siddetin uzerinde cizilir. Tamamen kapaliyken
   ikinci bir cizim cagrisi yapmanin anlami yoktur.

   UW_EYE_SUBMERGE / UW_EYE_LEAD: dolgunun DAYANAGI MERCEKTIR, GOVDE DEGIL.

   Onceki surum govdenin batma oranina bakiyordu ve bu yanlisti: diz boyu suda
   duran oyuncunun govdesinin yarisi "batmis" sayiliyor, ekranin yarisi su ile
   kaplaniyor, oysa kamera suyun cok USTUNDE duruyor ve oyuncu onunu gormek
   istiyor. Ekran kaplamasi, ekrani kaplayan seyin kendisine - mercege -
   baglidir.

   UW_EYE_LEAD: mercek yuzeye bu kadar yaklasinca dolgu ACILMAYA baslar. Suda
   yuruyen oyuncunun gozu yuzeyin cok ustunde kaldigi icin ekran tertemiz
   kalir; ancak dalip yuzeye yaklasinca su gorunur.

   UW_EYE_SUBMERGE: mercek yuzeyin bu kadar ALTINA indiginde ekran tamamen
   dolar. Ikisi birlikte "suya yavas yavas girdikce" olcusunu verir.

   Ekrandaki su siniri ARTIK BURADA SECILMEZ: onu fragment shader, isini
   gercek dalga yuzeyiyle kesistirerek bulur (bkz. gcl_underwater_shader.h).
   Modul yalnizca kamerayi, yuzeyi, dalga alanini ve suyun DUNYA kutusunu
   besler; cizginin sekli bu sayilarin dogal sonucudur. */
#define UW_FADE_RISE     3.2f
#define UW_FADE_FALL     4.0f
#define UW_FILL_MIN      0.004f
/* Perdenin TAM SIDDETE ulastigi derinlik (metre), mercek yuzeyin ALTINA
   indikten SONRA. Esik KESKINDIR: mercek yuzeyin USTUNDE oldugu surece perde
   HIC acilmaz.

   Eski UW_EYE_LEAD 0.35 yuzeyin USTUNDE aciyordu ve YANLISTI: yarim batmis
   oyuncuda 3B dalga yuzeyi de hala cizildigi icin ekranda ALT ALTA IKI SU
   olusuyordu ("dalgalar havada duruyor", "bacak suya girince ekran mavi"). */
#define UW_EYE_RAMP      0.35f

/* Dalga alaninin tepe-tepe toplami: a1+a2+a3 = 1.96
   (bkz. gcl_water_shader.h: gclWaterWave - 1.00 + 0.62 + 0.34).

   Yuzey shader'i `world.y += h*amp` yapar, yani GORUNEN su yuzeyi model
   duzleminden +-1.96*amp araliginda gezinir. Bu sayi BURADA gerekli cunku
   gecis bandi dalga araligindan DAR OLAMAZ: dar olursa iki hata birden
   cikar - (1) dalga tepesi duzlemin ustune tastigi icin goz suyun icindeyken
   perde kapali kalir ("dalgalar havada duruyor"), (2) dalga kameranin onunden
   gecerken durum kare kare acilip kapanir. */
#define WATER_WAVE_SUM   1.96f

/* ---------- EKRAN PERDESININ ACTIGI GOVDE ORANI ----------

   OLCU GOVDENIN BATMA ORANIDIR: (yuzey - ayak) / boy.

   Neden GOZ DEGIL: yuzme modunda KAFA YUZEYE KILITLIDIR (bkz.
   gcl_raylib_fps.c: swim_move -> `max_feet = surface - Height`). Yani goz
   yuzeyin ALTINA hicbir zaman inmez; "yuzey - goz" ifadesi yuzme boyunca
   TAM 0 kalir. Gozle olcen bir perde bu yuzden hicbir zaman acilmaz - kod
   calisir ama gorunmez ("suyun altinda shader yok, silmissin"). Goz olcusu
   yalnizca serbest kamera icin dogrudur ve YEDEK yol olarak durur.

   ESIKLER: diz boyu suda ekran maviye donmemeli (eski sikayet), ama yuzme
   baslar baslamaz su goruntuyu kaplamalidir. FPS yuzme esigi 0.60'tir
   (gcl_raylib_fps.c: FPS_SWIM_SUBMERGE); perde oradan once acilmaya baslar
   ve tam orada doyar, boylece ekranin "suya girdim" dedigi an ile fizigin
   dedigi an cakisir. */
#define UW_BODY_START    0.45f
#define UW_BODY_FULL     0.65f

enum {
    W_COLOR = 0, W_HANDLE,
    W_POS_X, W_POS_Y, W_POS_Z,
    W_ROT_X, W_ROT_Y, W_ROT_Z,
    W_SCALE_X, W_SCALE_Y, W_SCALE_Z,
    W_ALPHA, W_REFLECTION, W_REFRACTION, W_FRESNEL,
    W_WAVE_STRENGTH, W_WAVE_SPEED, W_WAVE_SCALE, W_FOAM
};

/* DOLGUNUN O ANKI SIDDETI (dosya geneli). Su kaydindan BAGIMSIZDIR: dolgu
   ekranin tamamina uygulanir, tek bir su nesnesinin ozelligi degildir. */
static float g_uw_fade = 0.0f;   /* 0 = kapali, 1 = tam siddet */

typedef struct {
    char   file[WATER_NAME_MAX];
    Model  model;
    /* Dosyanin KENDI tasidigi donusum. Script'in Position/Rotate/Scale'i her
       karede bunun UZERINE kurulur. */
    Matrix base_transform;
    /* Modelin KENDI yatay genisligi (dalga olcegi). */
    float  extent;
    int    extent_ready;
    /* Dalganin KENDI saati (saniye). */
    double wave_time;
    int    used;
} WaterEntry;

static WaterEntry g_water[WATER_MAX];
static double     g_slot[WATER_SLOT_COUNT];

/* ---------- YUZEY SHADER'I ---------- */
static Shader g_shader;
static int    g_shader_ready = 0;
static int    g_loc_time = -1, g_loc_viewpos = -1;
static int    g_loc_shallow = -1, g_loc_deep = -1;
static int    g_loc_wave = -1, g_loc_surf = -1, g_loc_foam = -1;
static int    g_loc_sky_top = -1, g_loc_sky_hor = -1, g_loc_sky_bot = -1;
static int    g_loc_sun_dir = -1, g_loc_sun_color = -1, g_loc_sun_params = -1;
static int    g_loc_cloud = -1, g_loc_cloud_shade = -1, g_loc_cloud_p = -1;
static int    g_loc_star = -1, g_loc_star_p = -1, g_loc_sky_p = -1, g_loc_band_p = -1;
static int    g_loc_fog_color = -1, g_loc_fog_params = -1, g_loc_fog_shape = -1;

/* ---------- DOLGU SHADER'I ---------- */
static Shader g_uw_shader;
static int    g_uw_ready = 0;
static int uw_loc_res = -1, uw_loc_time = -1;
static int uw_loc_shallow = -1, uw_loc_deep = -1;
static int uw_loc_submerge = -1;
static int uw_loc_campos = -1, uw_loc_camfwd = -1;
static int uw_loc_camright = -1, uw_loc_camup = -1;
static int uw_loc_tanhalf = -1, uw_loc_aspect = -1;
static int uw_loc_surfacey = -1;
static int uw_loc_amp = -1, uw_loc_freq = -1, uw_loc_speed = -1;

/* ---------- CIZILMIS KARE DOKUSU ----------

   Perde CIZILMIS SAHNEYI orneklemek zorundadir; bulaniklik, kirilma ve
   sogurma ancak boyle anlamli olur. Eski surum sahneyi hic okumuyordu,
   yalnizca uzerine koyu bir renk biniyordu - "blur yok" sikayetinin sebebi.

   Kare, raylib'in KENDI ekran okumasIyla alinir (LoadImageFromScreen) ve
   ONCEDEN OLUSTURULMUS bir dokuya yuklenir (UpdateTexture). Doku her karede
   YENIDEN OLUSTURULMAZ: yalnizca boyutu degistiginde kurulur, boylece kare
   basina bir VRAM ayirma/tahliye olmaz.

   SAHNE OKUMASI PAHALIDIR (glReadPixels senkron bir okumadir) ve bu yuzden
   YALNIZCA perde gorunurken yapilir - su altinda degilken hicbir maliyeti
   yoktur.

   DONDURME GEREKMEZ - KAYNAKTAN DOGRULANDI. `rlReadScreenPixels` (bkz.
   rlgl.h) glReadPixels'in ALT-SOL kokenli ciktisini KENDISI cevirir, yani
   `LoadImageFromScreen` goruntuyu UST-SOL kokenli dondurur: ilk satir
   ekranin USTUDUR. glTexImage2D ilk satiri V=0 yapar ve `DrawTexturePro`
   V=0'i dortgenin UST kenarina baglar; iki uc birbirini tutar. Shader bu
   yuzden ornegi HAM `fragTexCoord` ile alir; buraya elle bir `1.0 - v`
   eklemek goruntuyu TERS cevirir - "kamera ters donmus gibi" hatasi
   tam olarak bundan cikmisti. */
static Texture2D g_uw_frame = {0};
static int       g_uw_frame_w = 0;
static int       g_uw_frame_h = 0;

/* Kareyi dokuya tazele. Doku hazir degilse/boyut degistiyse yeniden kurar.
   Donus: 1 = doku kullanilabilir. */
static int uw_frame_refresh(int w, int h) {
    Image img;

    if (w <= 0 || h <= 0) return 0;

    /* Kare HER ZAMAN once okunur ve GECERLILIGI KONTROL EDILIR.

       Eski surum iki ayri hata tasiyordu ve ikisi de ekranda goruluyordu:

         * Ilk dalda `LoadImageFromScreen()` dogrudan `LoadTextureFromImage`e
           veriliyordu; okuma basarisizsa (pencere simge durumunda, GL baglami
           hazir degil) BEYAZ bir doku olusuyor ve perde o kareyi ekrana
           bastigi icin goruntu duz beyaza donuyordu. Ustelik o `Image` hic
           serbest birakilmiyordu.

         * Okuma basarisiz oldugunda fonksiyon 0 doner, cagiran da cizimi
           atlar; bu yuzden perde "bazen hic devreye girmiyor" gibi gorunuyordu.

       Simdi gecersiz kare dokuya hic yazilmaz: once dogrulanir, sonra
       kullanilir. Doku zaten kareyi tutuyorsa bir onceki gecerli kare kalir -
       ekran beyaza boyanmaz, yalnizca bir kare gecikir. */
    img = LoadImageFromScreen();
    if (!img.data || img.width <= 0 || img.height <= 0) {
        if (img.data) UnloadImage(img);
        return 0;
    }

    if (g_uw_frame.id == 0 || g_uw_frame_w != img.width || g_uw_frame_h != img.height) {
        if (g_uw_frame.id != 0) UnloadTexture(g_uw_frame);
        g_uw_frame = LoadTextureFromImage(img);
        UnloadImage(img);   /* dokuya kopyalandi; Image artik gerekmez */
        if (g_uw_frame.id == 0) return 0;
        g_uw_frame_w = img.width;
        g_uw_frame_h = img.height;
        return 1;
    }

    UpdateTexture(g_uw_frame, img.data);
    UnloadImage(img);
    return 1;
}

/* ---------- GOKYUZU DURUMU (PAYLASILAN) ----------

   Yuzey de, dolgu da AYNI gokyuzunu kullanmak zorundadir: biri kendi
   gradyanini uydurursa yansima ile pencereden gorunen gok birbirini tutmaz.
   Bu yuzden gokyuzu uniform'lari TEK bir yerden, sky_uniform_state()'ten
   gelir; iki shader'a da o diziden yazilir. */
enum {
    SS_TOP = 0, SS_HOR = 3, SS_BOT = 6,
    SS_SUN_DIR = 9, SS_SUN_COLOR = 12, SS_SUN_PARAMS = 15,
    SS_CLOUD_COLOR = 19, SS_CLOUD_SHADE = 22, SS_CLOUD_PARAMS = 25,
    SS_STAR_COLOR = 29, SS_STAR_PARAMS = 32,
    SS_SKY_PARAMS = 36, SS_BAND_PARAMS = 40,
    SS_COUNT = 45
};

/* Gokyuzu uniform ADLARI, iki shader icin TEK liste. */
enum {
    SKY_LOC_TOP = 0, SKY_LOC_HOR, SKY_LOC_BOT, SKY_LOC_SUN_DIR,
    SKY_LOC_SUN_COLOR, SKY_LOC_SUN_PARAMS, SKY_LOC_CLOUD_LIT, SKY_LOC_CLOUD_DARK,
    SKY_LOC_CLOUD_P, SKY_LOC_STAR_COLOR, SKY_LOC_STAR_P, SKY_LOC_SKY_P,
    SKY_LOC_BAND_P, SKY_LOC_COUNT
};

static const char *const g_sky_uniform_names[SKY_LOC_COUNT] = {
    SKY_U_TOP, SKY_U_HORIZON, SKY_U_BOTTOM, SKY_U_SUN_DIR,
    SKY_U_SUN_COLOR, SKY_U_SUN_PARAMS, SKY_U_CLOUD_LIT, SKY_U_CLOUD_DARK,
    SKY_U_CLOUD_P, SKY_U_STAR_COLOR, SKY_U_STAR_P, SKY_U_SKY_P, SKY_U_BAND_P
};


/* ---------- kucuk yardimcilar ---------- */

static const char *str_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return "";
    return argv[i];
}

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* Paketlenmis renk -> 32-bit desen.

   `Color` GCL tarafinda `int` olarak bildirilmistir, bu yuzden runner ISARETLI
   32-bit degeri gonderir: Raylib.WHITE (0xFFFFFFFF) buraya -1 olarak ulasir.
   Double'i dogrudan `unsigned int`e cevirmek negatif degerlerde TANIMSIZ
   davranistir; isaretli tamsayi uzerinden gidip 32 biti maskeledigimizde desen
   aynen korunur (bkz. gcl_SimpleMesh.c: color_bits). */
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

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

/* ---------- ayni surecteki raylib / komsu modul durumu ----------

   Semboller TEMBEL cozulur: bu modul Raylib.dll yuklenmeden de yuklenmelidir.

     RaylibSkybox.dll  -> gcl_skybox_uniform_state (gokyuzu, 45 float)
     RaylibFPS.dll     -> gcl_water_fps_state (oyuncunun batma orani)
     Raylib.dll        -> kamera, kare suresi, varlik yolu

   Yalnizca Raylib.dll'e bakilsaydi gokyuzu ve FPS sembolleri hicbir zaman
   bulunamazdi: su kendi yedek gradyanini yansitir ve dolgu hic inmezdi. */
#ifdef _WIN32
static void *symbol_in(const char *dll, const char *name) {
    HMODULE h = GetModuleHandleA(dll);
    return h ? (void *)GetProcAddress(h, name) : NULL;
}

static void *raylib_symbol(const char *name) {
    void *p = symbol_in("RaylibSkybox.dll", name);
    if (p) return p;
    p = symbol_in("RaylibFPS.dll", name);
    if (p) return p;
    return symbol_in("Raylib.dll", name);
}
#else
static void *raylib_symbol(const char *name) { (void)name; return NULL; }
#endif

typedef float (*GclFrameTimeFn)(void);
typedef int   (*GclCameraPosFn)(float *out3);
typedef int   (*GclCameraStateFn)(float *out11);
typedef const char *(*GclAssetPathFn)(const char *);
typedef int   (*GclFpsStateFn)(float *out8);
typedef int   (*GclGetFogFn)(float *out11);

/* Kare suresi raylib'in KENDI saatiyle olculur; boylece dalgalar
   SetTargetFPS ile ayni hizda akar. */
static double frame_seconds(void) {
    static GclFrameTimeFn fn = NULL;
    static int tried = 0;
    double dt;
    if (!tried) { tried = 1; fn = (GclFrameTimeFn)raylib_symbol("GetFrameTime"); }
    dt = fn ? (double)fn() : 1.0 / 60.0;
    if (!(dt > 0.0) || dt > 0.5) dt = 1.0 / 60.0;
    return dt;
}

static int camera_eye(float out3[3]) {
    static GclCameraPosFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclCameraPosFn)raylib_symbol("gcl_raylib_camera_pos"); }
    if (!fn) return 0;
    return fn(out3);
}

/* Kameranin TAM durumu: pos(3), target(3), up(3), fovy(1), projection(1).
   Dolgu, 3B su noktasini ekrana izdusurmek icin kameranin TABANINI (ileri,
   sag, yukari) ve gorus acisini bilmek zorundadir; konum tek basina yetmez. */
static int camera_state(float *out11) {
    static GclCameraStateFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclCameraStateFn)raylib_symbol("gcl_raylib_camera_state"); }
    if (!fn) return 0;
    return fn(out11);
}

/* OYUNCUNUN SUYA NE KADAR GIRDIGI, RaylibFPS.dll'den OKUNUR.

   Neden bu modul kendi olcmuyor: yuzme modunda KAFA YUZEYE KILITLIDIR
   (bkz. gcl_raylib_fps.c: swim_move), yani goz yuzeyin ALTINA hic inmez.
   "Goz suyun altinda mi" sorusu bu oyunda HER ZAMAN "hayir" cevabini verir ve
   dolgu icin tek basina yetersizdir. Dogru olcu GOVDENIN batma oranidir ve o
   olcum zaten FPS modulunde her karede yapilir.

   Donus: 1 = out8 yazildi. Sira: surface, bottom, feet, eye, height, depth,
   body, swim. FPS yuklu degilse 0 doner ve cagiran kendi olcumune duser. */
#define UW_FPS_SURFACE 0
#define UW_FPS_EYE     3
#define UW_FPS_HEIGHT  4
#define UW_FPS_DEPTH   5

static int fps_water_state(float *out8) {
    static GclFpsStateFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclFpsStateFn)raylib_symbol("gcl_water_fps_state"); }
    if (!fn) return 0;
    return fn(out8);
}

static const char *resolve_file(const char *file) {
    static GclAssetPathFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclAssetPathFn)raylib_symbol("gcl_raylib_asset_path"); }
    if (!fn || !file) return file ? file : "";
    return fn(file);
}

/* Gokyuzu durumu (RaylibSKYBOX). Yansima ve pencereden gorunen gok
   GOKYUZUNDEN gelir; bu modul onu HESAPLAMAZ, OKUR. Sembol yoksa ya da skybox
   henuz bir kare cizmediyse asagidaki BAGIMSIZ varsayilan kurulur; modul tek
   basina da calisir. */
static int sky_uniform_state(float *out) {
    static int (*fn)(float *) = NULL;
    static int tried = 0;
    int i;

    if (!tried) { tried = 1; fn = (int (*)(float *))raylib_symbol("gcl_skybox_uniform_state"); }
    if (fn && fn(out)) return 1;   /* skybox canli: aynen kullan */

    for (i = 0; i < SS_COUNT; i++) out[i] = 0.0f;

    out[SS_TOP + 0] = WATER_FALLBACK_TOP_R;
    out[SS_TOP + 1] = WATER_FALLBACK_TOP_G;
    out[SS_TOP + 2] = WATER_FALLBACK_TOP_B;
    out[SS_HOR + 0] = WATER_FALLBACK_HOR_R;
    out[SS_HOR + 1] = WATER_FALLBACK_HOR_G;
    out[SS_HOR + 2] = WATER_FALLBACK_HOR_B;
    out[SS_BOT + 0] = WATER_FALLBACK_HOR_R * 0.72f;
    out[SS_BOT + 1] = WATER_FALLBACK_HOR_G * 0.72f;
    out[SS_BOT + 2] = WATER_FALLBACK_HOR_B * 0.72f;
    out[SS_SUN_DIR + 1] = 1.0f;
    out[SS_SUN_COLOR + 0] = WATER_FALLBACK_SUN_R;
    out[SS_SUN_COLOR + 1] = WATER_FALLBACK_SUN_G;
    out[SS_SUN_COLOR + 2] = WATER_FALLBACK_SUN_B;
    out[SS_SUN_PARAMS + 0] = 0.9999f;
    out[SS_SUN_PARAMS + 1] = 0.9986f;
    out[SS_SUN_PARAMS + 2] = 0.45f;
    out[SS_SUN_PARAMS + 3] = 0.0f;
    out[SS_CLOUD_COLOR + 0] = WATER_FALLBACK_HOR_R;
    out[SS_CLOUD_COLOR + 1] = WATER_FALLBACK_HOR_G;
    out[SS_CLOUD_COLOR + 2] = WATER_FALLBACK_HOR_B;
    out[SS_CLOUD_SHADE + 0] = WATER_FALLBACK_HOR_R * 0.72f;
    out[SS_CLOUD_SHADE + 1] = WATER_FALLBACK_HOR_G * 0.72f;
    out[SS_CLOUD_SHADE + 2] = WATER_FALLBACK_HOR_B * 0.72f;
    out[SS_CLOUD_PARAMS + 0] = 0.42f;
    out[SS_CLOUD_PARAMS + 1] = 0.22f;
    out[SS_CLOUD_PARAMS + 2] = 3.00f;
    out[SS_CLOUD_PARAMS + 3] = 0.45f;
    out[SS_STAR_COLOR + 0] = WATER_FALLBACK_SUN_R;
    out[SS_STAR_COLOR + 1] = WATER_FALLBACK_SUN_G;
    out[SS_STAR_COLOR + 2] = WATER_FALLBACK_SUN_B;
    out[SS_STAR_PARAMS + 1] = 60.0f;
    out[SS_STAR_PARAMS + 3] = 2.00f;
    out[SS_SKY_PARAMS + 1] = 1.00f;
    out[SS_SKY_PARAMS + 3] = 0.30f;
    out[SS_BAND_PARAMS + 0] = 0.55f;
    out[SS_BAND_PARAMS + 1] = 26.0f;
    out[SS_BAND_PARAMS + 3] = 0.22f;
    return 0;
}

/* Sis durumu (Raylib.dll). RaylibFOG hicbir sey CIZMEZ: yalnizca cizilmekte
   olan sahnenin shader'ina fog uniform'larini yazar. Su kendi GLSL programini
   kullandigi icin o uniform'lar ona HIC ULASMAZ ve su, sisli bir arazinin
   uzerinde cam gibi durur. Bu modul sisi HESAPLAMAZ, OKUR: ayni sayilari
   alir, kendi shader'indaki kopyalarina yazar ve paylasilan formulu
   uygular (bkz. gcl_water_shader.h: gclWaterFogAmount). */
static int water_fog_state(float *out11) {
    static GclGetFogFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclGetFogFn)raylib_symbol("gcl_raylib_shader_get_fog"); }
    if (!fn) return 0;
    return fn(out11);
}

/* ---------- shader kurulumu ---------- */

static Shader build_shader(const char *vs_src, const char *fs_src, int glsl) {
    size_t vl = strlen(vs_src), fl = strlen(fs_src);
    char  *vs = (char *)malloc(vl + 32);
    char  *fs = (char *)malloc(fl + 32);
    Shader shader = {0};

    if (!vs || !fs) { free(vs); free(fs); return shader; }
    snprintf(vs, vl + 32, "#version %d\n%s", glsl, vs_src);
    snprintf(fs, fl + 32, "#version %d\n%s", glsl, fs_src);
    shader = LoadShaderFromMemory(vs, fs);
    free(vs);
    free(fs);
    return shader;
}

static void water_shader_locations(void) {
    g_loc_time       = GetShaderLocation(g_shader, WATER_U_TIME);
    g_loc_viewpos    = GetShaderLocation(g_shader, WATER_U_VIEWPOS);
    g_loc_shallow    = GetShaderLocation(g_shader, WATER_U_SHALLOW);
    g_loc_deep       = GetShaderLocation(g_shader, WATER_U_DEEP);
    g_loc_wave       = GetShaderLocation(g_shader, WATER_U_WAVE);
    g_loc_surf       = GetShaderLocation(g_shader, WATER_U_SURF);
    g_loc_foam       = GetShaderLocation(g_shader, WATER_U_FOAM);
    g_loc_sky_top    = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_TOP]);
    g_loc_sky_hor    = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_HOR]);
    g_loc_sky_bot    = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_BOT]);
    g_loc_sun_dir    = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_SUN_DIR]);
    g_loc_sun_color  = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_SUN_COLOR]);
    g_loc_sun_params = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_SUN_PARAMS]);
    g_loc_cloud      = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_CLOUD_LIT]);
    g_loc_cloud_shade= GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_CLOUD_DARK]);
    g_loc_cloud_p    = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_CLOUD_P]);
    g_loc_star       = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_STAR_COLOR]);
    g_loc_star_p     = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_STAR_P]);
    g_loc_sky_p      = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_SKY_P]);
    g_loc_band_p     = GetShaderLocation(g_shader, g_sky_uniform_names[SKY_LOC_BAND_P]);
    g_loc_fog_color  = GetShaderLocation(g_shader, WATER_U_FOG_COLOR);
    g_loc_fog_params = GetShaderLocation(g_shader, WATER_U_FOG_PARAMS);
    g_loc_fog_shape  = GetShaderLocation(g_shader, WATER_U_FOG_SHAPE);
}

static void underwater_locations(void) {
    uw_loc_res      = GetShaderLocation(g_uw_shader, UW_U_RES);
    uw_loc_time     = GetShaderLocation(g_uw_shader, UW_U_TIME);
    uw_loc_shallow  = GetShaderLocation(g_uw_shader, UW_U_SHALLOW);
    uw_loc_deep     = GetShaderLocation(g_uw_shader, UW_U_DEEP);
    uw_loc_submerge = GetShaderLocation(g_uw_shader, UW_U_SUBMERGE);
    uw_loc_campos   = GetShaderLocation(g_uw_shader, UW_U_CAMPOS);
    uw_loc_camfwd   = GetShaderLocation(g_uw_shader, UW_U_CAMFWD);
    uw_loc_camright = GetShaderLocation(g_uw_shader, UW_U_CAMRIGHT);
    uw_loc_camup    = GetShaderLocation(g_uw_shader, UW_U_CAMUP);
    uw_loc_tanhalf  = GetShaderLocation(g_uw_shader, UW_U_TANHALF);
    uw_loc_aspect   = GetShaderLocation(g_uw_shader, UW_U_ASPECT);
    uw_loc_surfacey = GetShaderLocation(g_uw_shader, UW_U_SURFACEY);
    uw_loc_amp      = GetShaderLocation(g_uw_shader, UW_U_AMP);
    uw_loc_freq     = GetShaderLocation(g_uw_shader, UW_U_FREQ);
    uw_loc_speed    = GetShaderLocation(g_uw_shader, UW_U_SPEED);
}

/* Shader'lar yalnizca BIR KEZ, ilk cizimde kurulur: o anda GL baglami zaten
   aciktir, cunku su ancak pencerenin icinde cizilebilir. Derlenemezse 0 doner
   ve cagri sessizce cikar (pencere disi bir cizim cokmemeli). */
static int water_shader_ready(void) {
    if (g_shader_ready) return 1;
    g_shader = build_shader(WATER_VERTEX_SRC, WATER_FRAGMENT_SRC, WATER_DEFAULT_GLSL);
    if (g_shader.id == 0) return 0;
    water_shader_locations();
    g_shader_ready = 1;
    return 1;
}

static int underwater_ready(void) {
    if (g_uw_ready) return 1;
    g_uw_shader = build_shader(UNDERWATER_VERTEX_SRC, UNDERWATER_FRAGMENT_SRC,
                               UNDERWATER_DEFAULT_GLSL);
    if (g_uw_shader.id == 0) return 0;
    underwater_locations();
    g_uw_ready = 1;
    return 1;
}

/* Gokyuzu degerlerini VERILEN programin konumlarina yazar. Iki shader da
   ayni sayilari okur; tek bir yerde toplanmalarinin nedeni budur. */
static void water_push_sky(const float *sky) {
    SetShaderValue(g_shader, g_loc_sky_top,     sky + SS_TOP,         SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_sky_hor,     sky + SS_HOR,         SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_sky_bot,     sky + SS_BOT,         SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_sun_dir,     sky + SS_SUN_DIR,     SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_sun_color,   sky + SS_SUN_COLOR,   SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_sun_params,  sky + SS_SUN_PARAMS,  SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_cloud,       sky + SS_CLOUD_COLOR, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_cloud_shade, sky + SS_CLOUD_SHADE, SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_cloud_p,     sky + SS_CLOUD_PARAMS,SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_star,        sky + SS_STAR_COLOR,  SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_star_p,      sky + SS_STAR_PARAMS, SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_sky_p,       sky + SS_SKY_PARAMS,  SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_band_p,      sky + SS_BAND_PARAMS, SHADER_UNIFORM_VEC4);
}


/* ---------- kayit defteri ---------- */

static WaterEntry *entry_of(int handle) {
    if (handle < 0 || handle >= WATER_MAX) return NULL;
    if (!g_water[handle].used) return NULL;
    return &g_water[handle];
}

/* Bos yuva bul; ayni dosya zaten yuklu ise onu PAYLAS. Ikinci bir kopya
   yuklemek GPU'da yer birakir ve iki ayri dalga saati yaratirdi. */
static int take_entry(const char *file) {
    int i;
    for (i = 0; i < WATER_MAX; i++) {
        if (g_water[i].used && strncmp(g_water[i].file, file, WATER_NAME_MAX) == 0)
            return i;
    }
    for (i = 0; i < WATER_MAX; i++) {
        if (!g_water[i].used) return i;
    }
    return -1;
}

/* Modelin YATAY genisligi; dalga olcegi buradan turetilir. Vertex verisi
   yuklemeden sonra degismedigi icin bir kez hesaplanir. YALNIZCA yatay:
   kisa bir kalin blok, uzun bir ince levhadan daha buyuk dalga almamalidir. */
static void entry_measure(WaterEntry *e) {
    float lo[3], hi[3];
    int   seen = 0;
    int   i;

    if (!e || e->extent_ready) return;
    for (i = 0; i < e->model.meshCount; i++) {
        Mesh m = e->model.meshes[i];
        const float *v = (const float *)m.vertices;
        int k;
        if (!v || m.vertexCount <= 0) continue;
        for (k = 0; k < m.vertexCount; k++) {
            float x = v[k * 3 + 0];
            float y = v[k * 3 + 1];
            float z = v[k * 3 + 2];
            if (!seen) {
                lo[0] = hi[0] = x; lo[1] = hi[1] = y; lo[2] = hi[2] = z;
                seen = 1;
            } else {
                if (x < lo[0]) lo[0] = x; else if (x > hi[0]) hi[0] = x;
                if (y < lo[1]) lo[1] = y; else if (y > hi[1]) hi[1] = y;
                if (z < lo[2]) lo[2] = z; else if (z > hi[2]) hi[2] = z;
            }
        }
    }
    if (!seen) {
        e->extent = 1.0f;
    } else {
        float sx = hi[0] - lo[0];
        float sz = hi[2] - lo[2];
        float s  = (sx > sz) ? sx : sz;
        e->extent = (s < 0.001f) ? 1.0f : s;
    }
    e->extent_ready = 1;
}

static float entry_extent(WaterEntry *e) {
    if (!e) return 1.0f;
    entry_measure(e);
    return e->extent;
}

/* Suyun DUNYA uzayindaki eksen hizali kutusu.

   Dolgu shader'i, kameradan attigi isinin suyu vurup vurmadigini bu kutuyla
   karar verir: vurus noktasi kutunun ICINDE degilse isin suyu iskalayip
   gokyuzune kacmistir ve piksel atilir. Kutu, mesh'in TUM koselerinin
   model.transform'tan gecirilmis halidir; donusum guncel oldugu surece
   dogrudur (bkz. water_apply_transform). */
static void water_world_aabb(WaterEntry *e, float *lo, float *hi) {
    int seen = 0, i, k;
    Vector3 mn = {0.0f, 0.0f, 0.0f}, mx = {0.0f, 0.0f, 0.0f};

    if (!e) { lo[0]=lo[1]=lo[2]=0.0f; hi[0]=hi[1]=hi[2]=0.0f; return; }
    for (i = 0; i < e->model.meshCount; i++) {
        Mesh m = e->model.meshes[i];
        const float *v = (const float *)m.vertices;
        if (!v || m.vertexCount <= 0) continue;
        for (k = 0; k < m.vertexCount; k++) {
            Vector3 w = Vector3Transform(
                (Vector3){v[k*3+0], v[k*3+1], v[k*3+2]}, e->model.transform);
            if (!seen) { mn = mx = w; seen = 1; }
            else {
                if (w.x < mn.x) mn.x = w.x; else if (w.x > mx.x) mx.x = w.x;
                if (w.y < mn.y) mn.y = w.y; else if (w.y > mx.y) mx.y = w.y;
                if (w.z < mn.z) mn.z = w.z; else if (w.z > mx.z) mx.z = w.z;
            }
        }
    }
    if (!seen) { mn = (Vector3){-1.0f, -1.0f, -1.0f}; mx = (Vector3){1.0f, 1.0f, 1.0f}; }
    lo[0] = mn.x; lo[1] = mn.y; lo[2] = mn.z;
    hi[0] = mx.x; hi[1] = mx.y; hi[2] = mx.z;
}

/* ---------- SU BIR HACIMDIR (KAFES) ----------

   TEST: suyun uzerinden ASAGI dogru bir isin atilir (raylib'in kendi ucgen
   testiyle, model.transform uzerinden). Isin kafesi delmiyorsa nokta su
   alaninin DISINDADIR. Deliyorsa:

       en yakin vurus = su YUZEYI  (kafesin ust yuzu)
       en uzak  vurus = su TABANI  (kafesin alt yuzu)

   Isin tabanli test, duzgun olmayan ve dondurulmus kafeslerde de tam
   dogrudur; kutu kose testi (AABB) egik/duzensiz bir alanda yanlis cevap
   verirdi. */
static int water_raycast(WaterEntry *e, float x, float z,
                         float *out_top, float *out_bottom) {
    Ray ray;
    RayCollision hit_near = {0}, hit_far = {0};
    int found = 0, hits = 0;
    int i;

    if (!e) return 0;
    ray.position  = (Vector3){x, WATER_RAY_TOP, z};
    ray.direction = (Vector3){0.0f, -1.0f, 0.0f};
    hit_near.distance = 1.0e30f;
    hit_far.distance  = -1.0e30f;

    for (i = 0; i < e->model.meshCount; i++) {
        RayCollision hit = GetRayCollisionMesh(ray, e->model.meshes[i], e->model.transform);
        if (!hit.hit) continue;
        if (!found || hit.distance < hit_near.distance) hit_near = hit;
        if (!found || hit.distance > hit_far.distance)  hit_far  = hit;
        found = 1;
        hits++;
    }
    if (!found) return 0;

    /* Ince bir kafes (tek yuzlu levha) ACIK sayilir: taban yoktur, derinlik
       sinirsizdir. Bu yuzden taban, yuzeyin cok altinda kabul edilir; aksi
       halde levhanin altinda kalan oyuncu "sudan cikmis" olurdu. */
    if (hits < 2 || (hit_near.point.y - hit_far.point.y) <= WATER_PAIR_MIN)
        hit_far.point.y = -1.0e9f;

    if (out_top)    *out_top = hit_near.point.y;
    if (out_bottom) *out_bottom = hit_far.point.y;
    return 1;
}

static int water_raycast_any(float x, float z, float *out_top, float *out_bottom) {
    int i;
    for (i = 0; i < WATER_MAX; i++) {
        if (!g_water[i].used) continue;
        if (water_raycast(&g_water[i], x, z, out_top, out_bottom)) return 1;
    }
    return 0;
}

/* DISA ACILAN SU SORGUSU. RaylibFPS.dll bunu "gcl_water_volume_probe" adiyla
   arar (bkz. gcl_raylib_fps.c: water_probe_fn) ve imza TAM olarak budur:

       int gcl_water_volume_probe(float x, float z, float *surface, float *bottom)

   Modul, model.transform GUNCEL oldugu surece dogru cevap verir; bu yuzden
   donusum hem Update() hem Draw() icinde kurulur.

   Donus: 1 = su var (iki isaretci de yazildi), 0 = yok. */
GCL_EXPORT int gcl_water_volume_probe(float x, float z,
                                      float *out_surface, float *out_bottom) {
    return water_raycast_any(x, z, out_surface, out_bottom);
}

/* Oyuncunun kapladigi ALANI tarayan sorgu (merkez + halka). Su kenarinda
   duran oyuncu bir ayagi islakken "karada" sayilmaz. */
static int water_area_at(float x, float z, float radius,
                         float *out_surface, float *out_bottom) {
    float best_top = 0.0f, best_bottom = 0.0f;
    int   found = 0;
    int   i;

    if (radius < 0.0f) radius = 0.0f;
    for (i = 0; i < WATER_RING_SAMPLES; i++) {
        float a  = (float)i * (WATER_TWO_PI / (float)WATER_RING_SAMPLES);
        float top, bottom;
        if (!water_raycast_any(x + cosf(a)*radius, z + sinf(a)*radius, &top, &bottom))
            continue;
        if (!found || top > best_top) { best_top = top; best_bottom = bottom; }
        found = 1;
    }
    {
        float top, bottom;
        if (water_raycast_any(x, z, &top, &bottom)) {
            if (!found || top > best_top) { best_top = top; best_bottom = bottom; }
            found = 1;
        }
    }
    if (!found) return 0;
    if (out_surface) *out_surface = best_top;
    if (out_bottom)  *out_bottom  = best_bottom;
    return 1;
}

/* Script'in konumlandirmasi, raylib'in DrawModelEx ile AYNI sirada
   (rmodels.c: matScale x matRotation x matTranslation) kurulur. Boylece bir su
   tam olarak DrawModelEx'in koyacagi yere oturur; Rotate DERECEDIR. */
static Matrix compose_transform(double px, double py, double pz,
                                double rx, double ry, double rz,
                                double sx, double sy, double sz) {
    Matrix mat_scale = MatrixScale((float)sx, (float)sy, (float)sz);
    Matrix mat_rot   = MatrixRotateXYZ((Vector3){
        (float)(rx * WATER_DEG2RAD),
        (float)(ry * WATER_DEG2RAD),
        (float)(rz * WATER_DEG2RAD)
    });
    Matrix mat_move  = MatrixTranslate((float)px, (float)py, (float)pz);
    return MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_move);
}

/* Suyun KONUMUNU modele yazar.

   Neden ayri bir fonksiyon: hacim sorgusu yuzey yuksekligini donusum
   matrisinden okur. Bu satir yalnizca Draw() icinde dururken,
   `water.Update(); if (water.FPS_InWater()) ...` yazan bir script SORGUDAN
   ONCE hicbir guncelleme yapmamis olur ve yuzey 0'da kalir - yani oyuncu
   suyun icindeyken "suda degil" cevabini alir. Bu yuzden donusum TEK bir
   yerde kurulur ve hem Update() hem Draw() onu cagirir. */
/* ---------- MERCEGIN SUYA GIRME SIDDETI (0 = kuru, 1 = tam su altinda) ----------

   TEK KAYNAK. Iki katman da bu sayiyi kullanir:

     * ekran PERDESI bu oranda GUC LENIR (bkz. fn_underwater_effect)
     * dolgu shader'inin `uwSubmerge` degeri budur

   YUZEYI BU ORANLA SAYDAMLASMAK DENENDI VE YANLISTI: yuzme esiginde oran
   1.0'a ulastigi icin alfa sifira iniyor ve su tamamen SAYDAM oluyordu
   ("suyu gorunmez mi yaptin"). Su bir hacimdir; icindeyken de gorunen sey
   odur. Yuzey HER ZAMAN cizilir (bkz. fn_draw).

   OLCU GOVDENIN BATMA ORANIDIR; `amp` yalnizca YEDEK yol icin gerekir. */
static float water_eye_depth01(float amp) {
    float st[8];
    float eye[3];
    float surface = 0.0f;
    float eye_y;
    float ramp;
    float depth, height, body, body_d = 0.0f, eye_d = 0.0f, d;

    /* ---------- ANA YOL: GOVDENIN BATMA ORANI ----------

       FPS modulu her karede suya ne kadar girildigini zaten hesaplar
       (gcl_raylib_fps.c: water_classify) ve PAYLASIR. Bu olcu yuzme
       boyunca da gecerlidir, cunku ayaklar yuzeyin altindadir.

       Gozle olcmek bu oyunda imkansizdir: kafa yuzeye kilitlidir, fark
       her zaman 0 cikar ve perde hic acilmaz. */
    if (fps_water_state(st)) {
        eye_y   = st[UW_FPS_EYE];
        surface = st[UW_FPS_SURFACE];
        height  = st[UW_FPS_HEIGHT];
        depth   = st[UW_FPS_DEPTH];
        /* GOVDE OLCUSU KALDIRILDI.

           Eskiden burada `depth/height` orani hesaplanip FONKSIYON BURADA
           KESIP DONUYORDU. Esikler UW_BODY_START=0.45 / UW_BODY_FULL=0.65
           oldugu icin efekt ancak govdenin YARISI suya girdiginde aciliyordu.
           KAFA suya girdiginde govde hala ayaktadir (batma orani ~0.1), olcu
           esigin ALTINDA kalir, fonksiyon 0 doner ve ekran DUZ BOS gorunur.
           Bildirilen "kamera az suya giriyor ama suyun alti bos" hali TAM
           OLARAK budur.

           Dogru olcu MERCEK ile SU YUZEYI arasidir ve asagida zaten
           hesaplaniyor: `surface - eye_y`. Erken donus kaldirildigi icin
           artik o kod calisir: kafa yuzeyin altina indigi an perde acilir,
           ciktigi an kapanir. */
    } else {
        /* Yedek yol: FPS modulu yoksa hacmi dogrudan sorgula. */
        if (!camera_eye(eye)) return 0.0f;
        eye_y = eye[1];
        if (!water_area_at(eye[0], eye[2], 0.0f, &surface, NULL)) return 0.0f;
    }

    /* YEDEK YOL: MERCEGIN DERINLIGI.

       Goz, ortalama su duzlemiyle karsilastirilir. Gercek su yuzeyi o
       duzlemin uzerinde +-WATER_WAVE_SUM*amp kadar gezinir (bkz.
       gcl_water_shader.h: `world.y += h*waterWave.x`). Bant bu zarfin IKI
       katidir; dar olursa dalga kameranin onunden gecerken durum kare kare
       acilip kapanir. */
    ramp = 2.0f * WATER_WAVE_SUM * amp;
    if (ramp < UW_EYE_RAMP) ramp = UW_EYE_RAMP;

    eye_d = (surface - eye_y) / ramp;
    if (eye_d > 1.0f) eye_d = 1.0f;
    if (eye_d < 0.0f) eye_d = 0.0f;

    /* GOVDE BATMA ORANI. Yuzerken perdeyi acan olcu BUDUR.

       Yuzme modunda kafa yuzeye KILITLIDIR (bkz. gcl_raylib_fps.c:
       swim_move -> max_feet = surface - Height), yani eye == surface TAM
       olarak saglanir ve eye_d yuzme boyunca TAM 0 kalir. Tek basina goz
       olcusune bakan perde HIC ACILMAZ. */

    /* ANA OLCU: GOVDENIN BATMA ORANI. Ayaklar yuzeyin altinda oldugu icin
       `depth / height` yuzme boyunca GECERLIDIR. Esikler: 0.45 -> 0.65. */
    body   = (height > 0.01f && depth > 0.0f) ? (depth / height) : 0.0f;
    body_d = (body - UW_BODY_START) / (UW_BODY_FULL - UW_BODY_START);
    if (body_d > 1.0f) body_d = 1.0f;
    if (body_d < 0.0f) body_d = 0.0f;

    /* BUYUK OLAN KAZANIR. Govde batmasi YUZMEYI, mercek derinligi serbest
       kamerayi ve kafa suya girdiginde yurumeyi kapsar. Ikisi bir arada her
       durumu yakalar. */
    d = (body_d > eye_d) ? body_d : eye_d;
    if (d > 1.0f) d = 1.0f;
    if (d < 0.0f) d = 0.0f;
    return d;
}

static void water_apply_transform(int index) {
    WaterEntry *e = entry_of(index);
    if (!e) return;
    e->model.transform = MatrixMultiply(e->base_transform,
        compose_transform(g_slot[W_POS_X], g_slot[W_POS_Y], g_slot[W_POS_Z],
                          g_slot[W_ROT_X], g_slot[W_ROT_Y], g_slot[W_ROT_Z],
                          g_slot[W_SCALE_X], g_slot[W_SCALE_Y], g_slot[W_SCALE_Z]));
}

/* ---------- slot <-> durum ----------

   IKI AYRI OKUMA YOLU VARDIR ve karistirilmamalidir:

   1. read_slots(argc, argv) - runner'in gonderdigi 19 yaprak. Uye cagrilarinda
      (Draw/Update) argc == 19'dur ve argv[i] dogrudan slot i'dir. DAHA AZ
      arguman gelirse yuvalar TAZELENMEZ: modulun kendi durumu korunur.

   2. reset_slots() - BELGELENMIS varsayilanlar. Yalnizca yukleme aninda
      cagrilir: `Create...` bir uye degildir ve yayinlanan durumu gecerli
      yapmasi gerekir. Dokunulmayan bir yuvayi static sifirda birakmak Scale'i
      (0,0,0) yapardi: donusum her ekseni 0 ile carpar, su bir noktaya coker ve
      hicbir sey gorunmez. */
static void reset_slots(void) {
    g_slot[W_COLOR]         = (double)0xFFFFFFFFu;   /* Raylib.WHITE */
    g_slot[W_HANDLE]        = -1.0;
    g_slot[W_POS_X]         = 0.0;
    g_slot[W_POS_Y]         = 0.0;
    g_slot[W_POS_Z]         = 0.0;
    g_slot[W_ROT_X]         = 0.0;
    g_slot[W_ROT_Y]         = 0.0;
    g_slot[W_ROT_Z]         = 0.0;
    g_slot[W_SCALE_X]       = 1.0;
    g_slot[W_SCALE_Y]       = 1.0;
    g_slot[W_SCALE_Z]       = 1.0;
    g_slot[W_ALPHA]         = WATER_DEF_ALPHA;
    g_slot[W_REFLECTION]    = WATER_DEF_REFLECT;
    g_slot[W_REFRACTION]    = WATER_DEF_REFRACT;
    g_slot[W_FRESNEL]       = WATER_DEF_FRESNEL;
    g_slot[W_WAVE_STRENGTH] = WATER_DEF_WAVE_STR;
    g_slot[W_WAVE_SPEED]    = WATER_DEF_WAVE_SPEED;
    g_slot[W_WAVE_SCALE]    = WATER_DEF_WAVE_SCALE;
    g_slot[W_FOAM]          = WATER_DEF_FOAM;
}

static void read_slots(int argc, const char **argv) {
    int i;
    if (argc < WATER_SLOT_COUNT) return;
    for (i = 0; i < WATER_SLOT_COUNT; i++) g_slot[i] = num_arg(argc, argv, i);
}

/* ---------- dalga parametreleri ----------

   TEK kaynak. Hem yuzey shader'i hem dolgu bunlari kullanir; iki ayri formul
   su ile ekrandaki cizgiyi ayristirir ve cizgi suyu tutmaz. */
static void wave_params(WaterEntry *e, float *amp, float *freq, float *speed) {
    float ext = entry_extent(e);
    if (amp) {
        float a   = WATER_WAVE_AMP_WORLD*(float)g_slot[W_WAVE_STRENGTH];
        float cap = ext*0.05f;   /* cok kucuk levhada dalga ondan buyuk olmasin */
        if (cap > 0.0f && a > cap) a = cap;
        *amp = a;
    }
    if (freq)  *freq  = (WATER_TWO_PI / WATER_WAVE_LEN_WORLD)
                      * (float)g_slot[W_WAVE_SCALE];
    if (speed) *speed = (float)g_slot[W_WAVE_SPEED];
}

/* Suyun taban renkleri `Color` ile CARPILIR: varsayilan beyaz oldugu icin
   taban renkler aynen gorunur, kirmizi verilirse su kirmizilasir. */
static void water_base_colors(float shallow[3], float deep[3]) {
    Color tint = unpack_color(color_bits(g_slot[W_COLOR]));
    shallow[0] = WATER_BASE_SHALLOW_R*(float)tint.r/255.0f;
    shallow[1] = WATER_BASE_SHALLOW_G*(float)tint.g/255.0f;
    shallow[2] = WATER_BASE_SHALLOW_B*(float)tint.b/255.0f;
    deep[0]    = WATER_BASE_DEEP_R*(float)tint.r/255.0f;
    deep[1]    = WATER_BASE_DEEP_G*(float)tint.g/255.0f;
    deep[2]    = WATER_BASE_DEEP_B*(float)tint.b/255.0f;
}

/* Dalga saatini FLOAT'a cevirir.

   rlgl float uniform'u `glUniform1fv(loc, count, (float*)value)` ile okur -
   gecirilen isaretciyi FLOAT sanip bastan yorumlar. Bir `double`in adresi
   verilirse yalnizca ILK 4 BAYTI okunur; kucuk endian'da bir double'in alt 4
   bayti genellikle 0'dir, yani zaman her karede 0.0 gider ve dalgalar HIC
   ilerlemez - statik bir mavi levha. */
static float wave_clock(WaterEntry *e) {
    return (float)fmod(e->wave_time, 1000.0);
}

/* ---------- yukleme / serbest birakma ---------- */

static double fn_create_water(int argc, const char **argv) {
    const char *file = str_arg(argc, argv, 0);
    int slot;
    Model model;

    if (!file[0]) { g_slot[W_HANDLE] = -1.0; return -1.0; }
    /* Yayinlanan durum YUKLEME ANINDA gecerli olmali: cagri dondukten sonra
       runner LastSlot kanalini okuyup TUM yuvalari degiskene yazar. */
    reset_slots();
    slot = take_entry(file);
    if (slot < 0) { g_slot[W_HANDLE] = -1.0; return -1.0; }

    if (g_water[slot].used) {
        g_slot[W_HANDLE] = (double)slot;
        return (double)slot;
    }

    model = LoadModel(resolve_file(file));
    if (!model.meshCount) { g_slot[W_HANDLE] = -1.0; return -1.0; }

    g_water[slot].model = model;
    g_water[slot].base_transform = model.transform;
    g_water[slot].extent = 1.0f;
    g_water[slot].extent_ready = 0;
    g_water[slot].wave_time = 0.0;
    g_water[slot].used = 1;
    snprintf(g_water[slot].file, WATER_NAME_MAX, "%s", file);
    entry_measure(&g_water[slot]);
    g_slot[W_HANDLE] = (double)slot;
    return (double)slot;
}

static void release_entry(int handle) {
    if (!entry_of(handle)) return;
    UnloadModel(g_water[handle].model);
    g_water[handle].model = (Model){0};
    g_water[handle].used = 0;
    g_water[handle].file[0] = 0;
    g_water[handle].extent_ready = 0;
}

/* UnloadSimpleWaterFromOBJ(struct) - serbest birakilacak kaydi duzlesmis
   struct'in Handle yapragindan (slot 1) alir. Ciplak bir tutamac da kabul
   edilir: UnloadSimpleWaterFromOBJ(water) ve (handle) ayni sekilde calisir. */
static double fn_unload_water(int argc, const char **argv) {
    int handle;
    read_slots(argc, argv);
    handle = (int)g_slot[W_HANDLE];
    if (!entry_of(handle)) handle = (int)num_arg(argc, argv, 0);
    release_entry(handle);
    return 0.0;
}

/* ---------- Update ----------

   Update() - dalga saatini ILERLETMEZ, yalnizca durumu okur. Belgedeki cagri
   sirasi `Update(); Draw();` oldugu icin iki cagri da ilerletseydi su iki kat
   hizli akardi. Zaman tek bir noktada, Draw'da akar. */
static double fn_update(int argc, const char **argv) {
    read_slots(argc, argv);
    /* Yerlesim BURADA da kurulur: sorgudan once guncelleme yapilmissa yuzey
       dogru okunur (bkz. water_apply_transform). */
    water_apply_transform((int)g_slot[W_HANDLE]);
    return 0.0;
}

/* ---------- Draw ----------

   Suyu 3B olarak cizer.

   DONUSUM: DrawModel()'a kimlik konum ve 1 olcek verilir, cunku yerlesim ZATEN
   model.transform'tedir; raylib aksi halde kendi otelemesini bunun uzerine
   carpar ve su iki kez otelerdi.

   SHADER MATERYALE KONUR: raylib'in DrawMesh'i kosulsuz `material.shader`i
   baglar, yani arada BeginShaderMode() cagirmak YOK SAYILIR. Bu yuzden su,
   script onu bir shader blogunun icinde cagirsa bile KENDI shader'iyla
   cizilir.

   ARKA YUZEY KAPATMA KAPATILIR ve bu BIR ZORUNLULUKTUR: su bir hacimdir.
   rlgl eleme acikken, oyuncu suyun ICINE girdigi anda bakilan her duvar ve
   taban "arka yuz" olur, hepsi elenir ve geriye yuzeyin ince bir yapragi
   kalir - ekranda "su yok, sadece yarim saydam bir levha var" gorunur ve
   suyun ALTINDAKI bosluk aciga cikar. Durum cagri sonunda GERI ALINIR.

   KARISIM: su yarim saydamdir; alfa karisimi acikken cizilir. */
/* On bildirim: ekran gecisi `fn_draw`in ICINDEN cagrilir ama tanimi
   dosyada SONRA gelir (bkz. fn_draw sonundaki birlesik cagri). */
static double fn_underwater_effect(int argc, const char **argv);

static double fn_draw(int argc, const char **argv) {
    WaterEntry *e;
    float eye[3];
    double dt = frame_seconds();
    float sky[SS_COUNT], fog[11];
    float shallow[3], deep[3], wave[4], surf[4], foam[4];
    float amp = 0.0f, freq = 1.0f, speed = 1.0f;
    float t;
    int    index;

    read_slots(argc, argv);
    index = (int)g_slot[W_HANDLE];
    e = entry_of(index);
    if (!e) return 0.0;
    if (!water_shader_ready()) return 0.0;

    /* Dalga saati HER KAREDE, TEK noktadan ilerler. Dolgu ayni degeri okur. */
    e->wave_time += dt * g_slot[W_WAVE_SPEED];
    water_apply_transform(index);

    sky_uniform_state(sky);
    water_base_colors(shallow, deep);
    wave_params(e, &amp, &freq, &speed);
    t = wave_clock(e);

    wave[0] = amp;
    wave[1] = freq;
    wave[2] = speed;
    wave[3] = (float)clamp01(0.55 * g_slot[W_WAVE_STRENGTH]);

    surf[0] = (float)clamp01(g_slot[W_ALPHA]);
    surf[1] = (float)clamp01(g_slot[W_REFLECTION]);
    surf[2] = (float)clamp01(g_slot[W_REFRACTION]);
    {
        double f = g_slot[W_FRESNEL];
        if (f < 0.5) f = 0.5;
        if (f > 8.0) f = 8.0;
        surf[3] = (float)f;
    }
    foam[0] = (float)clamp01(g_slot[W_FOAM]);
    foam[1] = 0.10f;
    foam[2] = 0.0f;
    foam[3] = 0.0f;

    if (!camera_eye(eye)) { eye[0] = eye[1] = eye[2] = 0.0f; }

    SetShaderValue(g_shader, g_loc_time,      &t,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_shader, g_loc_viewpos,   eye,       SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_shallow,   shallow,   SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_deep,      deep,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_shader, g_loc_wave,      wave,      SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_surf,      surf,      SHADER_UNIFORM_VEC4);
    SetShaderValue(g_shader, g_loc_foam,      foam,      SHADER_UNIFORM_VEC4);

    water_push_sky(sky);

    /* SIS. Sis kurulmamisSA HIC yazilmaz: uniform'lar 0 kalir ve mix tamamen
       kapanir - yani su, sis yokken de dogru cizilir. */
    if (water_fog_state(fog)) {
        SetShaderValue(g_shader, g_loc_fog_color,  fog + 0, SHADER_UNIFORM_VEC3);
        SetShaderValue(g_shader, g_loc_fog_params, fog + 3, SHADER_UNIFORM_VEC4);
        SetShaderValue(g_shader, g_loc_fog_shape,  fog + 7, SHADER_UNIFORM_VEC4);
    }

    /* ---------- GOZ SUYUN ALTINDAYKEN 3B YUZEY CIZILMEZ ----------

       Iki katman ayni karede acik kalirsa ekranda ALT ALTA IKI SU gorunur:
       3B dalga yuzeyi bir kez, ekran perdesi bir kez. "Dalgalar havada
       duruyor gibi" ve "bacaklar suya girince ekran maviye donuyor" diye
       bildirilen hal budur. Ikisi BIRBIRINI DISLAR: mercek yuzeyin USTUNDE
       -> 3B YUZEY (yansima + Fresnel + kopuk), ALTINDA -> EKRAN PERDESI.

       Saat yukarida yine ilerletilir, yani suya cikinca dalga kaldigi yerden
       akar ve sicramaz. Karar TEK yerden gelir: g_uw_fade, DrawUnderwater
       Effect ile AYNI yumusatilmis siddettir. */
    /* ---------- YUZEY HER ZAMAN CIZILIR ----------

       BURADA ALFA DUSURULMEZ ve BURADAN ERKEN DONULMEZ. Ikisi de denendi ve
       ikisi de YANLISTI:

         * `if (goz yuzeyin altinda) return 0.0;` -> suya girerken 3B su bir
           karede tamamen kayboluyordu ("su toptan gidiyor").
         * `surf[0] = Alpha * (1 - batma_orani)` -> yuzme esiginde oran 1.0
           oldugu icin alfa SIFIRA iniyordu; su tamamen SAYDAM oluyordu
           ("suyu gorunmez mi yaptin").

       Dogrusu: su BIR HACIMDIR ve icindeyken de gorunen sey ODUR. Arka yuz
       eleme kapali oldugu icin kameradan hacmin ic duvarlari ve yuzeyin ALTI
       cizilir; oyuncuyu saran su budur. Ekran perdesi onun YERINE gecmez,
       yalnizca derinlik sisine katkida bulunur (bkz. fn_underwater_effect).

       Yani iki katman BIRBIRINI DISLAMAZ, ust uste biner - gercek suda da
       oyle olur. `surf[0]` yalnizca script'in verdigi `Alpha` degeridir. */

    /* YUZEY. Kendi dalga shader'iyla: gokyuzu yansimasi, Fresnel, kopuk.
       Dolgu BURADA YAPILMAZ: o, BeginMode3D kapandiktan sonra ekran
       uzayinda calisir (bkz. fn_underwater_effect). */
    if (e->model.materialCount > 0) e->model.materials[0].shader = g_shader;

    rlDrawRenderBatchActive();   /* birikmis cizimleri bosalt: karisim sirasi */
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    DrawModel(e->model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    rlEnableBackfaceCulling();
    EndBlendMode();

    /* ---------- SU ALTI EKRAN GECISI: AYNI CAGRI ICINDE ----------

       Eskiden bu is AYRI bir uye cagrisiydi (`DrawUnderwaterEffect`) ve
       script onu `EndMode3D()`ten SONRA cagirmak zorundaydi. Ayri cagri
       olmasi iki sorun uretiyordu:

         * Cagriyi UNUTMAK mumkundu ve efekt sessizce kaybolurdu.
         * Iki cagri ARASINDA kalan her sey ile efekt arasinda bir kare
           kaymasi olusabiliyordu - "gecikmeli" gorunum.

       Artik ekran gecisi `Draw()`in ICINDE, yuzey cizildikten hemen sonra
       calisir. Ayri cagri YOKTUR.

       ORTHO GECISI: `Draw()` normalde BeginMode3D'nin ICINDEDIR, yani o an
       aktif matrisler KAMERA matrisleridir. Ekran gecisi ise 2B'dir; 3B
       matrislerle cizilirse dortgen kameraya gore yerlesir ve ekrani
       kaplamaz. Bu yuzden once matrisler SAKLANIR, 2B ortho kurulur, gecis
       calistirilir, sonra matrisler AYNEN GERI KONUR.

       GERI KOYMAK ZORUNLUDUR: script `EndMode3D()`ten sonra UI cizmeye devam
       eder; matrisler geri konmazsa UI yanlis yere duser. */
    {
        Matrix saved_mv   = rlGetMatrixModelview();
        Matrix saved_proj = rlGetMatrixProjection();
        int    rw = GetRenderWidth();
        int    rh = GetRenderHeight();

        if (rw > 0 && rh > 0) {
            rlOrtho(0.0, (double)rw, (double)rh, 0.0, 0.0, 1.0);
            rlSetMatrixModelview(MatrixIdentity());
            /* argc = 0: ekran gecisi yuvalari YENIDEN OKUMAZ; `Draw()`in
               az once okudugu degerleri kullanir. */
            (void)fn_underwater_effect(0, NULL);
        }

        rlSetMatrixProjection(saved_proj);
        rlSetMatrixModelview(saved_mv);
    }
    return 0.0;
}

/* ---------- DrawUnderwaterEffect ----------

   BeginMode3D KAPANDIKTAN SONRA, HER KAREDE cagrilir (bkz. gcl_underwater_shader.h).

   NEDEN DUZ RENK DEGIL: ekranin yarisina mavi bir dikdortgen basmak suyun
   yuzeyini ANLATMAZ - ne dalga vardir, ne derinlik, ne de asagidan bakisin
   farki. Gorsel olarak "mavi bir cam"dan ibaret kalir.

   NEDEN GENLIK MERCEKTEN OLULUR: ekran kaplamasi, ekrani kaplayan seyin
   kendisine - kameraya - baglidir. Onceki surum GOVDENIN batma oranina
   bakiyordu ve bu yanlisti: diz boyu suda duran oyuncunun govdesinin yarisi
   batmis sayiliyor, ekranin yarisi su ile kaplaniyor, oysa kamera suyun cok
   ustunde duruyor ve oyuncu onunu gormek istiyor. Mercek olcusu, "suya
   ayagimi soktum" ile "su beni kapladi" arasindaki farki korur. FPS yuklu
   degilse (serbest kamera) ayni olcu dogrudan su hacminden okunur.

   YUMUSATMA: ani ACMA/KAPAMA yoktur; su cizgisini gecerken ekran bir karede
   kapanmaz. Tamamen kapaliyken HIC cizilmez.

   SU YUZEYINE HIC DOKUNMAZ: yalnizca 2B ekranda, yuzeyin ALTINDA kalan
   pikselleri boyar; yuzeyin gokyuzu yansimasi, Fresnel ve kopugu bozulmaz. */
static double fn_underwater_effect(int argc, const char **argv) {
    /* ---------- ERKEN DONUS KALDIRILDI ----------

       Burada eskiden kosulsuz bir `return 0.0;` vardi: efektin TAMAMI olu
       koddur. Sonuc, suya girildiginde ekranda HICBIR sey degismemesiydi -
       ne blur, ne kirilma, ne de su tonu. Asagidaki govde artik GERCEKTEN
       calisir.

       Efektin isi: cizilmis kareyi okuyup (uw_frame_refresh), uzerinde
       kirilma + blur + kromatik sapma + sogurma + kursagi/caustic
       uygulamak. Sahne okumasi pahali oldugu icin yalnizca perde
       gorunurken yapilir. */
    float shallow[3], deep[3];
    float target, fade, amp = 0.0f, freq = 1.0f, speed = 1.0f;
    float res[2];
    float t, dt;
    float cam[11];
    float pos[3], fwd[3], right[3], upv[3];
    float aspect = 1.0f, tan_half = 0.5f, surface = 0.0f;
    int   sw, sh;
    WaterEntry *e;

    read_slots(argc, argv);
    e = entry_of((int)g_slot[W_HANDLE]);
    if (!e) return 0.0;

    /* FRAMEBUFFER OLCUSU, PENCERE OLCUSU DEGIL.

       `GetScreenWidth/Height` PENCEREYI verir; `GetRenderWidth/Height` ise
       gercekte cizilen framebuffer'i. Ikisi (DPI olceklemesi ya da hedef
       doku kullanildiginda) FARKLI olur ve yanlis olani secmek iki hatayi
       birden uretir:

         * `LoadImageFromScreen` framebuffer'i okur; beklenen boyut pencere
           boyutundan farkliysa okunan goruntu ile blit uyusmaz ve goruntu
           BOZULUR (gecikmeli/yanlis kare izlenimi).
         * Blit hedefi yanlis olcekte kalir; perde ekrani tam kaplamaz.

       Ekran uzayinda calisan bir efekt icin dogru olcu her zaman RENDER
       olcusudur - shader'daki `uwRes` de ayni sayiyi almali. */
    sw = GetRenderWidth();
    sh = GetRenderHeight();
    if (sw <= 0 || sh <= 0) return 0.0;
    /* Uyari: kameranin KONUMU artik gerekmez. Kursak shader'i ekran uzayinda
       calisir (kaynak `underwater.glsl` de oyle); dunya uzayindan izdusurme
       YOKTUR. Tek gereken ekran olcusu, zaman ve batma siddetidir. */

    /* ---------- EFEKT SIDDETI ----------

       Kaynak shader'in kendi zaman ilerlemesi yoktur; isinlar `iTime` ile
       akar. SIDDET ise oyuncunun suya ne kadar girdiginden gelir ve FPS
       modulunden OKUNUR (bkz. water_eye_depth01): yuzme sirasinda kafa
       yuzeye kilitli oldugu icin "goz suyun altinda mi" sorusu bu oyunda
       her zaman "hayir" doner ve o olcu tek basina yetersizdir.

       Dalga genligi de bu olcuye girer cunku gecis bandi dalga zarfindan
       dar olamaz. */
    wave_params(e, &amp, &freq, &speed);
    target = water_eye_depth01(amp);
    if (target > 1.0f) target = 1.0f;
    if (target < 0.0f) target = 0.0f;

    /* ---------- SU YUZEYININ DUNYA YUKSEKLIGI ----------
       Sinir PIKSEL BASINA bu duzleme gore hesaplanir; bu yuzden modul
       burada yalnizca TEK bir sayi besler. Once FPS modulu sorulur
       (govde icin dogru olan yuzey), yoksa hacim dogrudan taranir. */
    {
        float st[8];
        if (fps_water_state(st)) {
            surface = st[UW_FPS_SURFACE];
        } else {
            float eye3[3];
            if (camera_eye(eye3)) {
                float top, bottom;
                if (water_area_at(eye3[0], eye3[2], 0.0f, &top, &bottom)) surface = top;
            }
        }
    }

    /* YUMUSATMA */
    dt = (float)frame_seconds();
    {
        float k = (target > g_uw_fade ? UW_FADE_RISE : UW_FADE_FALL) * dt;
        if (k > 1.0f) k = 1.0f;
        if (k < 0.0f) k = 0.0f;
        g_uw_fade += (target - g_uw_fade) * k;
        if (g_uw_fade < UW_FILL_MIN) g_uw_fade = 0.0f;
    }
    fade = g_uw_fade;
    if (fade <= UW_FILL_MIN) return 0.0;
    if (!underwater_ready()) return 0.0;

    /* Renkler yuzeyle AYNI paletten: suyun ustu ile ekrandaki su ayni suya
       ait gorunmeli. */
    water_base_colors(shallow, deep);
    t = wave_clock(e);   /* YUZEYIN saatiyle AYNI deger; faz ayrisamaz */

    res[0] = (float)sw;
    res[1] = (float)sh;
    aspect = (float)sw/(float)sh;

    /* ---------- KAMERA TABANI ----------
       Piksel basina isin yonu buradan cikar; su cizgisinin sekli tamamen
       bu tabana ve `surface`e baglidir. Semboller yoksa kimlik kalir ve
       efekt kapali sayilir. */
    pos[0] = pos[1] = pos[2] = 0.0f;
    fwd[0] = 0.0f; fwd[1] = 0.0f; fwd[2] = -1.0f;
    right[0] = 1.0f; right[1] = right[2] = 0.0f;
    upv[0] = upv[2] = 0.0f; upv[1] = 1.0f;
    if (camera_state(cam)) {
        float fx, fy, fz, rx, ry, rz, len;
        pos[0] = cam[0]; pos[1] = cam[1]; pos[2] = cam[2];
        fx = cam[3] - cam[0]; fy = cam[4] - cam[1]; fz = cam[5] - cam[2];
        len = sqrtf(fx*fx + fy*fy + fz*fz);
        if (len > 1.0e-6f) { fx /= len; fy /= len; fz /= len; }
        fwd[0] = fx; fwd[1] = fy; fwd[2] = fz;
        /* sag = ileri x yukari; sonra yukari = sag x ileri (diklestirme) */
        rx = fy*cam[8] - fz*cam[7];
        ry = fz*cam[6] - fx*cam[8];
        rz = fx*cam[7] - fy*cam[6];
        len = sqrtf(rx*rx + ry*ry + rz*rz);
        if (len > 1.0e-6f) {
            rx /= len; ry /= len; rz /= len;
            right[0] = rx; right[1] = ry; right[2] = rz;
            upv[0] = ry*fz - rz*fy;
            upv[1] = rz*fx - rx*fz;
            upv[2] = rx*fy - ry*fx;
        }
        tan_half = ((cam[9] > 0.0f) ? cam[9] : 60.0f)*0.5f*WATER_DEG2RAD;
        tan_half = tanf(tan_half);
        if (tan_half < 1.0e-6f) tan_half = 1.0e-6f;
    }

    SetShaderValue(g_uw_shader, uw_loc_res,      res,      SHADER_UNIFORM_VEC2);
    SetShaderValue(g_uw_shader, uw_loc_time,     &t,       SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_shallow,  shallow,  SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_deep,     deep,     SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_campos,   pos,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_camfwd,   fwd,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_camright, right,    SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_camup,    upv,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_uw_shader, uw_loc_tanhalf,  &tan_half,SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_aspect,   &aspect,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_surfacey, &surface, SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_amp,      &amp,     SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_freq,     &freq,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_uw_shader, uw_loc_speed,    &speed,   SHADER_UNIFORM_FLOAT);
    /* `uwSubmerge` YUMUSATILMIS siddettir: su, oyuncu indikce KADEMELI
       yukselir, bir karede belirmez. */
    SetShaderValue(g_uw_shader, uw_loc_submerge, &fade,   SHADER_UNIFORM_FLOAT);

    /* 2B dolgu. Ekran uzayinda calisir; kamera matrisi kullanilmaz.

       DERINLIK TESTI KAPATILIR ve bu BIR ZORUNLULUKTUR.

       Neden: raylib'in 2B toplu ciziminde (DrawRectangle) dortgen ekran
       uzayindadir ve NDC z'si 0'dir - pencere derinliginde 0.5. Derinlik
       testi acikken onunden gecen HER 3B geometri (arazi duvari, zemin, hatta
       su yuzeyinin kendisi) 0.5'ten DAHA YAKIN bir derinlige sahiptir ve
       dolguyu EZER. Kamera suyun icindeyken bakilan yonde su olmasi
       gerekirken arkadaki arazi ve gokyuzu gorunur; "su beni kaplamiyor,
       suyun yukarisini goruyorum" hatasi tam olarak budur.

       Fiziksel gerekce: mercek suyun ICINDE iken baktigi her yonde SU vardir.
       Onunden daha yakin bir geometri gecemez; dolgu bu yuzden kosulsuz
       ustune yazmalidir.

       DERINLIK YAZIMI DA KAPATILIR: dolgu, kendisinden sonra cizilecek
       hicbir seyin derinligini bozmaz. Iki durum da cagri sonunda GERI ALINIR.

       ATILAN PIKSEL YINE ATILIR: derinlik testi kapali olsa bile shader'in
       `discard`i orada hicbir sey yazmaz. Kamera suyun DISINDAYKEN veya isin
       suyu iskalayip gokyuzune kactiginda piksel bos kalir; su kenarinda ekran
       gereksizce maviye boyanmaz. */
    rlDrawRenderBatchActive();
    /* DERINLIK TESTI KAPATILIR ve bu DOGRUDUR: perde artik SAHNENIN KENDISINI
       tasir (kare dokuya okunur, shader onu bulaniklastirip tonlar). Sahne
       zaten `col`in icindedir, dolayisiyla ekran derinligine gore kirpmak
       yalnizca rastgele bir bolgeyi acikta birakir. Suyun icindeyken tum
       goruntu ayni su tonundan gecmelidir. */
    rlDisableDepthTest();
    BeginBlendMode(BLEND_ALPHA);
    /* DERINLIK TESTI ACIK KALIR - ONCEDEN KAPATILIYORDU VE YANLISTI.

       Kapatildiginda perde her pikselin ustune KOSULSUZ yaziyordu: 2 m
       otedeki magara tavani ile 30 m otedeki su yuzeyi perde icin AYNIYDI.
       Perde tavani eziyor, magara duvarlari gorunmuyordu.

       Perde 2B bir dortgendir; NDC z'si 0 yani derinlik 0.5'tir. Test
       acikken:
         * YAKIN geometri (magara tavani/duvari, kaya) perdeyi EZER ve
           gorunur kalir.
         * UZAK geometri (acik deniz, gokyuzu) perdeye YENILIR ve suya doner.
       Boylece magara ici okunur ama acik suda ekran su ile kaplanir.

       YAZMA KAPALI KALIR: perde kendisinden sonra cizilecek hicbir seyin
       derinligini bozmaz. */
    /* ---------- DERINLIK TESTI ACIK ----------

       EndMode3D() derinlik testini KAPATIR. Bu yuzden perde, varsayilan
       haldeyken her pikselin ustune KOSULSUZ yaziyordu: 2 m otedeki magara
       tavani ile 30 m otedeki su yuzeyi perde icin AYNIYDI. Perde tavani
       ezip magara duvarlarini gorunmez yapiyordu.

       Test acilinca siralama su olur (dortgen, tasiyici asamada arka
       duzleme itilir - bkz. gcl_underwater_shader.h: UNDERWATER_VERTEX_SRC):

         * YAKIN geometri (magara tavani/duvari) perdeyi EZER -> gorunur kalir.
         * BOS/zemin pikseller perdeye YENILIR -> su rengi orayi kaplar.

       Koprunun altinda YILDIZ gorunmesinin sebebi de buydu: orada geometri
       yoktur, perde atilir ve arkadaki gokyuzu aciga cikar. Perde arka
       duzleme itildigi icin artik o boslugu da SU doldurur.

       YAZMA KAPALI KALIR (rlDisableDepthMask): perde, kendisinden sonra
       cizilecek hicbir seyin derinligini bozmaz. Test, cagri sonunda 2B
       varsayilanina GERI ALINIR ki UI ve DrawFPS etkilenmesin. */
    rlDisableDepthMask();
    /* KAREYI OKU ve PERDEYI O KARENIN UZERINDEN GECIR.
       Artik cizilen sey duz bir dikdortgen degil, CIZILMIS KARENIN
       KENDISIDIR: shader onu bulaniklastirir, dalgayla buker ve sogurur.
       `DrawTexturePro` partinin doku birimini otomatik baglar, shader da onu
       `texture0` adiyla okur (bkz. gcl_underwater_shader.h). */
    if (uw_frame_refresh(sw, sh)) {
        /* KAYNAK DORTGEN DOKUNUN KENDI OLCUSUDUR, ekranin degil.

           Doku, yakalanan karenin boyutuyla olusur; `sw/sh` ise cizim
           hedefidir. Ikisi (pencere yeniden boyutlandirildiginda ya da
           pencere olcusu ile cizim olcusu ayristiginda) FARKLI olabilir ve o
           anda kaynak dortgen doku sinirlarinin DISINA tasar; shader gecerli
           kare yerine kenar/tasma ornekler ve ekran bozulur ("bazen shader
           devreye girmiyor" goruntusu). Doku olcusunu kullanmak bu uyusmayi
           tamamen ortadan kaldirir. */
        float fw = (float)g_uw_frame.width;
        float fh = (float)g_uw_frame.height;
        BeginShaderMode(g_uw_shader);
        DrawTexturePro(g_uw_frame,
                       (Rectangle){0.0f, 0.0f, fw, fh},
                       (Rectangle){0.0f, 0.0f, (float)sw, (float)sh},
                       (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        EndShaderMode();
    }
    rlEnableDepthMask();
    rlEnableDepthTest();
    EndBlendMode();
    return 0.0;
}

/* ---------- OKUMA ERISIMCILERI ----------

   Runner, bir struct degiskenini bu adlardan doldurur (bkz. gcl_runner.c'deki
   last_water[] ve g_native_last_maps[]). Adlar script'e BAKAN adlardir ve
   sira f_water[] ile ayni olmak zorundadir. */
static double slot_get(int i) { return g_slot[i]; }

static double fn_color(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_COLOR); }
static double fn_handle(int argc, const char **argv)     { (void)argc; (void)argv; return slot_get(W_HANDLE); }
static double fn_pos_x(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_POS_X); }
static double fn_pos_y(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_POS_Y); }
static double fn_pos_z(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_POS_Z); }
static double fn_rot_x(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_ROT_X); }
static double fn_rot_y(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_ROT_Y); }
static double fn_rot_z(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_ROT_Z); }
static double fn_scale_x(int argc, const char **argv)    { (void)argc; (void)argv; return slot_get(W_SCALE_X); }
static double fn_scale_y(int argc, const char **argv)    { (void)argc; (void)argv; return slot_get(W_SCALE_Y); }
static double fn_scale_z(int argc, const char **argv)    { (void)argc; (void)argv; return slot_get(W_SCALE_Z); }
static double fn_alpha(int argc, const char **argv)      { (void)argc; (void)argv; return slot_get(W_ALPHA); }
static double fn_reflection(int argc, const char **argv) { (void)argc; (void)argv; return slot_get(W_REFLECTION); }
static double fn_refraction(int argc, const char **argv) { (void)argc; (void)argv; return slot_get(W_REFRACTION); }
static double fn_fresnel(int argc, const char **argv)    { (void)argc; (void)argv; return slot_get(W_FRESNEL); }
static double fn_wave_strength(int argc, const char **argv){ (void)argc; (void)argv; return slot_get(W_WAVE_STRENGTH); }
static double fn_wave_speed(int argc, const char **argv) { (void)argc; (void)argv; return slot_get(W_WAVE_SPEED); }
static double fn_wave_scale(int argc, const char **argv) { (void)argc; (void)argv; return slot_get(W_WAVE_SCALE); }
static double fn_foam(int argc, const char **argv)       { (void)argc; (void)argv; return slot_get(W_FOAM); }

/* LastSlot(): argumansiz -> kac yuva yayinlanir; LastSlot(i) -> yuva i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)WATER_SLOT_COUNT;
    {
        int i = (int)atof(argv[0]);
        if (i < 0 || i >= WATER_SLOT_COUNT) return 0.0;
        return g_slot[i];
    }
}

/* ---------- rahatlik sorgulari ----------

   Bunlar SUYU TANIMLAMAZ, yalnizca oyuncunun durumunu FPS modulunden okur ve
   cevirir. Esik TUTMAZLAR: karar tek bir yerde (gcl_raylib_fps.c) verilir. */
static double fn_fps_in_water(int argc, const char **argv) {
    float st[8];
    (void)argc; (void)argv;
    if (!fps_water_state(st)) return 0.0;
    return (st[UW_FPS_DEPTH] > 0.0f) ? 1.0 : 0.0;
}

static double fn_fps_is_swimming(int argc, const char **argv) {
    float st[8];
    (void)argc; (void)argv;
    if (!fps_water_state(st)) return 0.0;
    return (st[7] > 0.5f) ? 1.0 : 0.0;
}

#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_water,        "CreateSimpleWaterFromOBJ"),
    E(unload_water,        "UnloadSimpleWaterFromOBJ"),
    E(update,              "Update"),
    E(draw,                "Draw"),
    E(underwater_effect,   "DrawUnderwaterEffect"),
    E(fps_in_water,        "FPS_InWater"),
    E(fps_is_swimming,     "FPS_IsSwimming"),
    E(color,               "Color"),
    E(handle,              "Handle"),
    E(pos_x,               "PositionX"),
    E(pos_y,               "PositionY"),
    E(pos_z,               "PositionZ"),
    E(rot_x,               "RotateX"),
    E(rot_y,               "RotateY"),
    E(rot_z,               "RotateZ"),
    E(scale_x,             "ScaleX"),
    E(scale_y,             "ScaleY"),
    E(scale_z,             "ScaleZ"),
    E(alpha,               "Alpha"),
    E(reflection,          "Reflection"),
    E(refraction,          "Refraction"),
    E(fresnel,             "Fresnel"),
    E(wave_strength,       "WaveStrength"),
    E(wave_speed,          "WaveSpeed"),
    E(wave_scale,          "WaveScale"),
    E(foam,                "Foam"),
    E(last_slot,           "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplewater_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
