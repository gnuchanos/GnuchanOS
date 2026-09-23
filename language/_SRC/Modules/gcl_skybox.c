/*
#native <RaylibSKYBOX>


# simple cartoon build in module generate skybox
RaylibSKYBOX.Skybox sky = RaylibSKYBOX.CreateSimpleSkybox();
sky.DailyCycle = RaylibSKYBOX.night;


# simple cloud and start moves
# simple sun only moves DailyCycle

night
day
morning
noon
evening
midnight
dawn
sunset
sunrise
twilight

# ek cartoon bulut katmani (gun/gece rengine gore)
sky.cloudON = true;

# bulut akis hizi (varsayilan 1; 0 = durur)
sky.CloudSpeed = 1;

# call

sky.Update();
sky.Draw();
*/

/* Backend.

   Gokyuzu TAMAMEN SHADER ile uretilir: modul HICBIR doku (PNG) tasimaz,
   yalnizca bir kubbe mesh'i kurar ve fragment shader'a "bakis YONU" verir.

   NEDEN SHADER: iki sey doku ile ifade edilemez.

     1. Gok gradyani kameraya YAPISIK olmali. Kubbe her karede kameraya
        otelendigi icin tepedeki gradyan kamerayla birlikte hareket eder;
        doku tabanli bir gokyuzu ise donuyor gibi gorunur.
     2. Gunesin yeri calisma aninda, RaylibSimpleLight'in kendi aci
        kuralindan (Rotate.z = yukseklik, Rotate.y = azimut) gelir. Gunesi
        isiklandirmaya birakmak yerine gokyuzu gunesi/dogal olarak ayi KENDI
        cizer; boylece gunes ufkun altindayken de gokyuzu gece rengini korur.

   CIZIM SIRASI (script'in yaptigi gibi):

       Raylib.ClearBackground(Raylib.BLUE);   // artik gereksiz, ama zararsiz
       Raylib.BeginMode3D(CurrentCamera);
           sky.Draw();                        // ONCE gokyuzu
           Raylib.BeginShaderMode(shader);
               Terrain.Draw(); CUBE.Draw();
           Raylib.EndShaderMode();
       Raylib.EndMode3D();

   Draw() gokyuzunu kameraya oteler, DERINLIK YAZMASINI kapatir (her seyin
   arkasinda kalir) ve YUZEY ELEMESINI kapatir (kubbe ic yuzunden gorunur).
   Cagri bitince uc durum de geri alinir; arazinin cizimi etkilenmez.

   SLOT TABLOSU (SharedPipeline/gcl_native_types.c'deki `Skybox` alan listesi
   ve GCL/SimpleRunner/gcl_runner.c'deki g_native_slot_maps[] sayaciyla BIREBIR
   ayni olmak zorundadir):

       0 Handle           1 DailyCycle        2 Time
       3 DayLength        4 CloudAmount       5 CloudSpeed
       6 StarAmount       7 SunSize           8 SunBrightness
       9 CloudON

   CloudON (0/1) EK BULUT KATMANINI acar: var olan yumusak bulutlarin USTUNDE,
   keskin kenarli ve duz renkli (cartoon) ikinci bir katman. Rengi gunesin
   yuksekligine gore degisir — gunduz beyaz, gece koyu mavi-gri. Varsayilan 0.

   CloudSpeed bulutlarin AKIS HIZIDIR (varsayilan 1, 0 = durur). Bulutlarin
   kendi saati (`cloud_time`) her karede bu degerle ilerler; gok saatinden AYRI
   tutulur, cunku sabit gorunumlerde (night, day, ...) gok saati hic ilerlemez
   ve bulutlar donardi.

   GUNES BURADAN YAYILIR: hesaplanan gunes yonu, rengi ve gucu
   (gcl_skybox_sun_direction / _color / _power, dosyanin sonunda) disa acilir.
   RaylibSimpleLight.dll varsa gunesini buradan alir; boylece EKRANDA GORUNEN
   gunes ile AYDINLATILAN yuz ayni yeri gosterir. Kendi acisini hesaplayan bir
   isik, gokyuzundeki diskle hizasiz kalirdi.

   Sabitler (RaylibSKYBOX.night gibi) hem `sky.DailyCycle = RaylibSKYBOX.night;`
   yazimini hem de dogrudan `sky.DailyCycle = 0;` yazimini mumkun kilar; ikisi
   de ayni tam sayiyi tasir. Deger donduren birer modul uyesidirler, yani
   raylib'in renk sabitleriyle ayni yoldan cozulur — ayri bir mekanizma yok. */

#include "gcl_module.h"

/* windows.h, wingdi.h/winuser.h'yi cekmemeli: onlarin Rectangle/CloseWindow
   adlari raylib'in adlariyla catisir. Burada yalnizca yukleyici API'si ve
   GetFrameTime icin GetProcAddress lazim. */
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
   `inline`'a acilir (raymath.h:83); o durumda MatrixTranslate'in disaridan
   tanimi olmadigi icin LINK asamasinda patlar. Belgelendirilmis kullanim,
   kutuphane tuketicisini static-inline moduna sabitlemektir. */
#define RAYMATH_STATIC_INLINE
#include "raymath.h"

/* Yuzey elemesi ve derinlik maskesi rlgl'nin kendi cagrilariyla kapatilir.
   rlgl header-only'dir: RLGL_IMPLEMENTATION tanimlanmadigi icin burada yalnizca
   bildirimler gelir, GL baglamini zaten raylib yonetir. */
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcl_skybox_shader.h"

/* ---------- olculer ---------- */

#define SKY_SLOT_COUNT     10
#define SKY_MAX            8
#define SKY_DEG2RAD        0.017453292519943295
#define SKY_DOME_RADIUS    120.0f
#define SKY_DOME_RINGS     12
#define SKY_DOME_SEGMENTS  24
#define SKY_MAX_STEP       0.25   /* tek karede ilerleyebilecek en buyuk gun orani */
#define SKY_DEFAULT_DAY_LENGTH 120.0

/* Gun rengi gunesin yuksekliginden turetilir: yukseklik bu bandin ustundeyse
   gunduz sayilir. Alacakaranlik gecisi bu bantta yasanir. */
#define SKY_DAY_BAND       0.35
/* Ufuk sisu genisligi (shader'daki fog ile ayni sayi olmali). */
#define SKY_HORIZON_BAND   0.22

enum {
    S_HANDLE = 0, S_DAILY_CYCLE, S_TIME, S_DAY_LENGTH,
    S_CLOUD_AMOUNT, S_CLOUD_SPEED, S_STAR_AMOUNT, S_SUN_SIZE, S_SUN_BRIGHTNESS,
    S_CLOUD_ON
};

/* ---------- hazir donguler ---------- */

enum {
    SKY_NIGHT = 0, SKY_DAY, SKY_MORNING, SKY_NOON, SKY_EVENING,
    SKY_MIDNIGHT, SKY_DAWN, SKY_SUNSET, SKY_SUNRISE, SKY_TWILIGHT,
    SKY_CYCLE, SKY_CYCLE_COUNT
};
/* Ilk 10'u SABIT gorunumdur (gunes yeri tablodan gelir, zaman akmaz);
   SKY_CYCLE gun boyunca ilerler. */
#define SKY_FIXED_COUNT SKY_CYCLE

/* (gun orani, yukseklik) — CYCLE'dan sabit bir gorunume gecerken gunesin yeri
   sicramasin diye tablo CYCLE icin de bir baslangic noktasi tasir. */
typedef struct { double phase; double elevation; } SkyPhase;

static const SkyPhase SKY_PHASES[SKY_CYCLE_COUNT] = {
    {0.00, -0.35},  /* night    */
    {0.50,  1.00},  /* day      */
    {0.28,  0.45},  /* morning  */
    {0.50,  1.00},  /* noon     */
    {0.72,  0.20},  /* evening  */
    {0.00, -0.35},  /* midnight */
    {0.26,  0.20},  /* dawn     */
    {0.74,  0.05},  /* sunset   */
    {0.26,  0.20},  /* sunrise  */
    {0.78, -0.05},  /* twilight */
    {0.50,  1.00}   /* cycle    */
};

/* ---------- renk paleti ----------
   Her hazir gorunum icin 4 satir: gunduz tepesi, gunduz ufku, gece tepesi,
   gece ufku. (0,0,0) "bu satir bos" demektir; hesaplama bos yeri ayni
   gorunumun oteki yarim kuresinden devralir (bkz. palette_row). */
#define SKY_RGB(R, G, B) { (float)(R) / 255.0f, (float)(G) / 255.0f, (float)(B) / 255.0f }

enum { SKY_LIT_TOP = 0, SKY_LIT_HORIZON, SKY_DARK_TOP, SKY_DARK_HORIZON, SKY_ROW_COUNT };

static const float SKY_PALETTE[SKY_CYCLE_COUNT][SKY_ROW_COUNT][3] = {
    /* night    */ { {0,0,0}, {0,0,0}, SKY_RGB( 8, 12, 34), SKY_RGB( 18, 26, 58) },
    /* day      */ { SKY_RGB( 74,144,226), SKY_RGB(168,212,246), {0,0,0}, {0,0,0} },
    /* morning  */ { SKY_RGB(104,164,232), SKY_RGB(226,206,178), {0,0,0}, {0,0,0} },
    /* noon     */ { SKY_RGB( 58,132,222), SKY_RGB(190,224,248), {0,0,0}, {0,0,0} },
    /* evening  */ { SKY_RGB( 92,116,186), SKY_RGB(246,168,114), {0,0,0}, {0,0,0} },
    /* midnight */ { {0,0,0}, {0,0,0}, SKY_RGB( 4,  6, 22), SKY_RGB( 12, 18, 44) },
    /* dawn     */ { SKY_RGB(126,140,208), SKY_RGB(244,198,154), {0,0,0}, {0,0,0} },
    /* sunset   */ { SKY_RGB(118,108,190), SKY_RGB(250,158,100), {0,0,0}, {0,0,0} },
    /* sunrise  */ { SKY_RGB(122,152,220), SKY_RGB(248,190,142), {0,0,0}, {0,0,0} },
    /* twilight */ { SKY_RGB( 62, 70,132), SKY_RGB(244,148,104), {0,0,0}, {0,0,0} },
    /* cycle    */ { SKY_RGB( 74,144,226), SKY_RGB(168,212,246),
                     SKY_RGB(  8, 12, 34), SKY_RGB( 18, 26, 58) }
};

/* Alacakaranlik tonu. `SKY_SUNSET` bir SABIT GORUNUM adidir (enum); renk
   sabitine ayni adi vermek ikisini catistirirdi, bu yuzden ayri ad tasir. */
static const float SKY_DUSK[3]      = SKY_RGB(255, 136, 56);
static const float SKY_SUN_DAY[3]   = SKY_RGB(255, 251, 238);
static const float SKY_SUN_NIGHT[3] = SKY_RGB(214, 224, 246);

/* Varsayilanlar: dokumandaki `CreateSimpleSkybox()` cagrisi bunlari kurar. */
#define SKY_DEF_CYCLE        SKY_DAY
#define SKY_DEF_TIME         0.50
#define SKY_DEF_DAY_LENGTH   SKY_DEFAULT_DAY_LENGTH
#define SKY_DEF_CLOUD_AMOUNT 45.0
#define SKY_DEF_CLOUD_SPEED  1.0
#define SKY_DEF_STAR_AMOUNT  100.0
#define SKY_DEF_SUN_SIZE     1.4
#define SKY_DEF_SUN_BRIGHT   1.0

/* ---------- modul durumu ---------- */

typedef struct {
    int    used;
    int    daily_cycle;
    double phase;     /* 0..1, CYCLE'in kendi gun saati */
    double prev_time; /* script'in Time'inda en son gorulen deger */
    /* Bulutlarin KENDI saati (saniye). Gok saatinden (`phase`) AYRI tutulur:
       sabit gorunumlerde (night, day, ...) `phase` hic ilerlemez, bu yuzden
       gok saatine bagli bir bulut akisi o gorunumlerde DONARDI. Bu sayac her
       karede CloudSpeed ile artar. */
    double cloud_time;
} SkyStore;

static SkyStore      g_sky[SKY_MAX];
static Mesh          g_dome;
static Material      g_dome_material;
static int           g_shader_ready = 0;

/* Kubbeye has GLSL program. RaylibShader'in programindan AYRI tutulur, cunku
   gokyuzu BeginShaderMode blogunun DISINDA cizilir (bkz. basliktaki cizim
   sirasi); DrawMesh zaten materyalin shader'ini kendisi baglar. */
static Shader        g_sky_shader;
static int           g_loc_top = -1, g_loc_horizon = -1, g_loc_bottom = -1;
static int           g_loc_sun_dir = -1, g_loc_sun_color = -1, g_loc_sun_params = -1;
static int           g_loc_cloud = -1, g_loc_cloud_shade = -1, g_loc_cloud_p = -1;
static int           g_loc_star = -1, g_loc_star_p = -1, g_loc_sky_p = -1;
static int           g_loc_band_p = -1;

/* Son Update/Draw cagrisinin urettigi degerler; okuma erisimcileri ve
   LastSlot bunlari dondurur. */
static double        g_slot[SKY_SLOT_COUNT];

/* ---------- KARE BASINA BIR KEZ ILERLEME ----------

   Script `sky.Update(); sky.Draw();` ikilisini cagirir (modulun belgeledigi
   kullanim). Iki cagri da fazi ilerletirse GUN IKI KAT HIZLI AKAR: DayLength
   = 120 gercekte 60 saniye surer. Bu bir performans degil DOGRULUK hatasidir.

   Cozum: ilk cagri ilerletir, ayni karedeki ikinci cagri ilerletmez. "Ayni
   kare" modulun kendi bayragiyla izlenir; Update bayragi kaldirir, Draw
   indirir. Yani kare basina EN FAZLA BIR ilerleme olur.

   BELGELENEN SIRA ONEMLIDIR: Update once, Draw sonra. Sira ters cevrilirse
   iki cagri da ilerletir (Draw indirdigi icin Update yeniden ilerletir).
   Modulun ornegi ve main.gcsf bu sirayi kullanir. */
static int g_advanced_this_frame = 0;

/* ---------- Gun durumu (RaylibSimpleLight buradan beslenir) ----------

   Gunesin yeri ve rengi bu modulde hesaplanir. Isik modulu kendi acisindan
   bagimsiz bir yon hesaplarsa EKRANDA GORUNEN gunes ile AYDINLATILAN yuz ayni
   yeri gostermez. Bu yuzden cizimde kullanilan degerler burada saklanir ve
   dosyanin sonundaki gcl_skybox_sun_* sembolleriyle disa acilir. */
static float g_sun_dir[3] = {0.0f, 1.0f, 0.0f};
static float g_sun_rgb[3] = {1.0f, 1.0f, 1.0f};
static float g_sun_power  = 1.0f;   /* 0 = gece, 1 = tam gunduz */
static int   g_sun_valid  = 0;

/* Son karede CIZILEN gokyuzu gradyani (zenit ve ufuk renkleri). Su modulu
   yansimasini bundan alir; kendi sabit rengini uyduran bir su, ekrandaki
   gokyuzuyle tutmayan bir gradyan yansitirdi. */
static float g_sky_top[3] = {0.0f, 0.0f, 0.0f};
static float g_sky_hor[3] = {0.0f, 0.0f, 0.0f};
static int   g_sky_colors_valid = 0;

/* GOKYUZUNUN TAM DURUMU — cizimde shader'a gonderilen BUTUN uniform'lar.

   Neden gerekli: gokyuzu PROSEDUREL'dir; rengi yalnizca bir gradyan degil,
   ufuk isimasi + samanyolu + uc katman yildiz + gunes diski ve isimasi +
   yumusak bulut katmani + cartoon bulut katmani'ndan olusur. Su yalnizca
   gradyani yansitirsa EKRANDAKI gokyuzunu degil, BASKA bir gokyuzunu
   yansitir; aradaki fark da tam olarak gozun fark ettigi seydir: bulutlar ve
   gunes.

   Bu yuzden cizimde kullanilan 13 uniform burada saklanir ve
   `gcl_skybox_uniform_state` ile disa acilir. Su, ayni degerlerle paylasilan
   `gclSkyColor()` fonksiyonunu cagirir; boylece yansittigi gok ile cizilen
   gok BIREBIR ayni olur — iki modul tek bir gok tanimi paylasir.

   SIRA SOZLESMESI (45 float; su modulu bu sirayla okur):
        0.. 2  skyTop      3.. 5  skyHorizon    6.. 8  skyBottom
        9..11  sunDir     12..14  sunColor     15..18  sunParams
       19..21  cloudColor 22..24  cloudShade   25..28  cloudParams
       29..31  starColor  32..35  starParams   36..39  skyParams
       40..44  bandParams
   Adlar gcl_sky_shared.h'deki SKY_U_* sabitleriyle birebir aynidir. */
#define SKY_STATE_FLOATS 45
static float g_sky_state[SKY_STATE_FLOATS];
static int   g_sky_state_valid = 0;

/* ---------- Raylib.dll: ayni surecteki raylib durumu ---------- */

typedef float (*GclRaylibFrameTimeFn)(void);
typedef int   (*GclRaylibCameraPosFn)(float *out3);

#ifdef _WIN32
static void *raylib_symbol(const char *name) {
    HMODULE h = GetModuleHandleA("Raylib.dll");
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *raylib_symbol(const char *name) { (void)name; return NULL; }
#endif

/* Kare suresi raylib'in KENDI saatiyle olculur: bir tam gun boylece
   Raylib.SetTargetFPS() ile ayni hizda akar. Sembol yoksa (Raylib.dll
   yuklenmemis) sabit 1/60 varsayilir. */
static double frame_seconds(void) {
    static GclRaylibFrameTimeFn fn = NULL;
    static int tried = 0;
    double dt;
    if (!tried) { tried = 1; fn = (GclRaylibFrameTimeFn)raylib_symbol("GetFrameTime"); }
    dt = fn ? (double)fn() : 1.0 / 60.0;
    if (!(dt > 0.0) || dt > 0.5) dt = 1.0 / 60.0;   /* ilk kare / pencere tasima */
    return dt;
}

/* Raylib.BeginMode3D()'ye verilen kameranin goz noktasi; kubbe buraya otelenir.
   Sembol yoksa 0 doner: gokyuzu yine cizilir, yalnizca cok buyuk dunyalarda
   kubbenin merkezi kameradan kayabilir. */
static int camera_eye(float out3[3]) {
    static GclRaylibCameraPosFn fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclRaylibCameraPosFn)raylib_symbol("gcl_raylib_camera_pos"); }
    if (!fn) return 0;
    return fn(out3);
}

/* ---------- kucuk yardimcilar ---------- */

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static double wrap01(double v) {
    v = fmod(v, 1.0);
    return v < 0.0 ? v + 1.0 : v;
}

static double lerp(double a, double b, double t) { return a + (b - a) * t; }

/* Gun orani -> gunes yuksekligi. 0'da gunes ufkun ALTINDA (gece yarisi),
   0.25'te dogar, 0.50'de tepe noktasi, 0.75'te batar. */
static double phase_elevation(double phase) {
    return -cos(phase * 6.283185307179586);
}

/* Yildiz/samanyolu gorunurlugu — YALNIZCA gercek gecede.

   DUZELTILEN HATA: once dogrudan `night = 1 - day` kullaniliyordu. `day`
   gunesin YUKSEKLIGINDEN geldigi icin twilight'ta night=0.70, sunset'ta 0.53
   cikiyordu; yani AKŞAM, GECE YARISI ve ŞAFAK gorunumlerinin HEPSI yildiz
   doluydu ve birbirinin aynisi gibi gorunuyordu. Yildiz artik cok daha dik bir
   egriyle gelir: gunes ufkun belirgin sekilde altina inmeden yildiz yoktur. */
static double star_visibility(double night) {
    double t = (night - 0.62) / 0.28;   /* 0.62 altinda yok, 0.90 ustunde tam */
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);     /* smoothstep */
}

/* Gunesin aci vektoru; isik moduluyle AYNI kural (bkz. gcl_SimpleLight.c):
   Rotate.z = yukseklik, Rotate.y = azimut ve
   direction = (sin(az)*cos(el), sin(el), cos(az)*cos(el)).

   AZIMUT GUN BOYUNCA DONER. Once sabit 25 dereceydi; bu yuzden morning ile
   evening ve dawn ile sunset BIRBIRININ AYNISI goruntuyordu — gunes her zaman
   ayni yerden dogup ayni yere batiyordu. Artik dogudan (phase 0.25) tepeye,
   oradan batiya (phase 0.75) gider. */
static void sun_direction(double elevation, double phase, float out3[3]) {
    double el = elevation * 1.5707963267948966;   /* -1..1 -> -90..90 derece */
    double az = (90.0 + (phase - 0.25) * 360.0) * SKY_DEG2RAD;
    out3[0] = (float)(sin(az) * cos(el));
    out3[1] = (float)sin(el);
    out3[2] = (float)(cos(az) * cos(el));
}

/* ---------- gokyuzu kubbesi ---------- */

static void sky_dome_free(Mesh *m) {
    if (!m) return;
    if (m->vertices)  { free(m->vertices);  m->vertices = NULL; }
    if (m->texcoords) { free(m->texcoords); m->texcoords = NULL; }
    if (m->indices)   { free(m->indices);   m->indices = NULL; }
    m->vertexCount = m->triangleCount = 0;
}

/* Yaricapi SKY_DOME_RADIUS olan, guney kutbundan baslayip kuzey kutbunda biten
   kure. Merkez (0,0,0): model matrisi YALNIZCA oteler, bu yuzden vertex
   konumu ayni zamanda merkezden dunya yonudur (shader bunu boyle kullanir).

   DOKU KOORDINATI YOKTUR — ve olmamalidir. Butun gokyuzu `vDir`'den, yani
   YONDEN uretilir; shader hicbir doku orneklemez. Bir zamanlar burada UV
   uretiliyor ve bir CPU gurultu makinesiyle bozuluyordu; hicbir yerde
   okunmadigi icin kaldirildi (bkz. optimizasyon_todo.md). */
static int sky_dome_build(void) {
    const int outer = SKY_DOME_RINGS * SKY_DOME_SEGMENTS;   /* halka noktalari */
    const int verts = outer + 1;                            /* + guney kutbu   */
    const int tris  = SKY_DOME_SEGMENTS * SKY_DOME_RINGS * 2;
    int vi = 1, ii = 0;

    memset(&g_dome, 0, sizeof(g_dome));
    g_dome.vertexCount   = verts;
    g_dome.triangleCount = tris;
    g_dome.vertices  = (float *)calloc((size_t)verts * 3, sizeof(float));
    g_dome.indices   = (unsigned short *)calloc((size_t)tris * 3, sizeof(unsigned short));
    if (!g_dome.vertices || !g_dome.indices) {
        sky_dome_free(&g_dome);
        return 0;
    }

    /* guney kutbu: uretilen tek nokta */
    g_dome.vertices[0] = 0.0f;
    g_dome.vertices[1] = -SKY_DOME_RADIUS;
    g_dome.vertices[2] = 0.0f;

    for (int r = 1; r <= SKY_DOME_RINGS; r++) {
        float height = -1.0f + 2.0f * (float)r / (float)SKY_DOME_RINGS;   /* -1..1 */
        float ring_radius = sqrtf(fmaxf(0.0f, 1.0f - height * height));
        for (int seg = 0; seg < SKY_DOME_SEGMENTS; seg++) {
            float az = 6.283185307179586f * (float)seg / (float)SKY_DOME_SEGMENTS;
            int   at = vi * 3;
            g_dome.vertices[at + 0] = cosf(az) * ring_radius * SKY_DOME_RADIUS;
            g_dome.vertices[at + 1] = height * SKY_DOME_RADIUS;
            g_dome.vertices[at + 2] = sinf(az) * ring_radius * SKY_DOME_RADIUS;
            vi++;
        }
    }

    /* guney kapagi: kutup noktasi + ilk halka */
    for (int seg = 0; seg < SKY_DOME_SEGMENTS; seg++) {
        int b = 1 + seg;
        int c = 1 + (seg + 1) % SKY_DOME_SEGMENTS;
        g_dome.indices[ii++] = 0;
        g_dome.indices[ii++] = (unsigned short)c;
        g_dome.indices[ii++] = (unsigned short)b;
    }
    /* halkalar arasi dortgenler */
    for (int r = 0; r + 1 < SKY_DOME_RINGS; r++) {
        int lo = 1 + r * SKY_DOME_SEGMENTS;
        int hi = lo + SKY_DOME_SEGMENTS;
        for (int seg = 0; seg < SKY_DOME_SEGMENTS; seg++) {
            int n = (seg + 1) % SKY_DOME_SEGMENTS;
            int v00 = lo + seg, v01 = lo + n, v10 = hi + seg, v11 = hi + n;
            g_dome.indices[ii++] = (unsigned short)v00;
            g_dome.indices[ii++] = (unsigned short)v11;
            g_dome.indices[ii++] = (unsigned short)v10;
            g_dome.indices[ii++] = (unsigned short)v00;
            g_dome.indices[ii++] = (unsigned short)v01;
            g_dome.indices[ii++] = (unsigned short)v11;
        }
    }
    return 1;
}

/* ---------- shader ---------- */

/* Iki GLSL kaynagini, baslarina `#version N` ekleyerek derler —
   gcl_Shader.c'nin dosya tabanli shader'larda kullandigi sozlesmenin aynisi. */
static Shader load_sky_shader(int glsl_version) {
    size_t vs_len = strlen(SKY_VERTEX_SRC), fs_len = strlen(SKY_FRAGMENT_SRC);
    char  *vs = (char *)malloc(vs_len + 32);
    char  *fs = (char *)malloc(fs_len + 32);
    Shader shader = {0};

    if (!vs || !fs) { free(vs); free(fs); return shader; }
    snprintf(vs, vs_len + 32, "#version %d\n%s", glsl_version, SKY_VERTEX_SRC);
    snprintf(fs, fs_len + 32, "#version %d\n%s", glsl_version, SKY_FRAGMENT_SRC);

    shader = LoadShaderFromMemory(vs, fs);
    free(vs);
    free(fs);
    return shader;
}

static void sky_shader_cache_locations(void) {
    g_loc_top         = GetShaderLocation(g_sky_shader, SKY_U_TOP);
    g_loc_horizon     = GetShaderLocation(g_sky_shader, SKY_U_HORIZON);
    g_loc_bottom      = GetShaderLocation(g_sky_shader, SKY_U_BOTTOM);
    g_loc_sun_dir     = GetShaderLocation(g_sky_shader, SKY_U_SUN_DIR);
    g_loc_sun_color   = GetShaderLocation(g_sky_shader, SKY_U_SUN_COLOR);
    g_loc_sun_params  = GetShaderLocation(g_sky_shader, SKY_U_SUN_PARAMS);
    g_loc_cloud       = GetShaderLocation(g_sky_shader, SKY_U_CLOUD_LIT);
    g_loc_cloud_shade = GetShaderLocation(g_sky_shader, SKY_U_CLOUD_DARK);
    g_loc_cloud_p     = GetShaderLocation(g_sky_shader, SKY_U_CLOUD_P);
    g_loc_star        = GetShaderLocation(g_sky_shader, SKY_U_STAR_COLOR);
    g_loc_star_p      = GetShaderLocation(g_sky_shader, SKY_U_STAR_P);
    g_loc_sky_p       = GetShaderLocation(g_sky_shader, SKY_U_SKY_P);
    g_loc_band_p      = GetShaderLocation(g_sky_shader, SKY_U_BAND_P);
}

/* Shader + materyal + GPU yuklemesi yalnizca BIR KEZ yapilir; ilk cizimde GL
   baglami zaten aciktir, cunku gokyuzu ancak pencerenin icinde cizilebilir.

   UploadMesh() SART: kubbe CPU'da elle kuruluyor (vertices/indices
   doldurulur), ama DrawMesh cizimi VAO/VBO uzerinden yapar. UploadMesh
   cagrilmazsa mesh.vaoId 0 kalir ve DrawMesh HICBIR SEY cizmez — ekranda
   yalnizca ClearBackground'in rengi gorunur. */
static int sky_ready(void) {
    if (g_shader_ready) return 1;
    if (sky_dome_build() == 0) return 0;

    /* CPU mesh'i -> GPU. Ikinci arguman 0: kubbe her karede ayni, dinamik
       guncelleme yok. */
    UploadMesh(&g_dome, false);

    g_sky_shader = load_sky_shader(SKY_DEFAULT_GLSL);
    if (g_sky_shader.id == 0) {
        sky_dome_free(&g_dome);
        return 0;
    }
    sky_shader_cache_locations();

    g_dome_material = LoadMaterialDefault();
    g_dome_material.shader = g_sky_shader;
    g_dome_material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    g_shader_ready = 1;
    return 1;
}

/* ---------- renk hesabi ---------- */

static int palette_present(const float rgb[3]) {
    return rgb[0] > 0.0f || rgb[1] > 0.0f || rgb[2] > 0.0f;
}

static void palette_row(int cycle, int row, float out3[3]) {
    const float (*p)[3];
    if (cycle < 0 || cycle >= SKY_CYCLE_COUNT) cycle = SKY_DEF_CYCLE;
    p = SKY_PALETTE[cycle];
    if (palette_present(p[row])) {
        out3[0] = p[row][0]; out3[1] = p[row][1]; out3[2] = p[row][2];
        return;
    }
    /* Bos satir ayni gorunumun oteki yarim kuresinden devralinir: gece
       gorunumlerinde gunduz satiri, gunduz gorunumlerinde gece satiri. */
    if (row == SKY_LIT_TOP)          { palette_row(cycle, SKY_DARK_TOP, out3); return; }
    if (row == SKY_LIT_HORIZON)      { palette_row(cycle, SKY_DARK_HORIZON, out3); return; }
    if (row == SKY_DARK_TOP)         { palette_row(cycle, SKY_LIT_TOP, out3); return; }
    palette_row(cycle, SKY_LIT_HORIZON, out3);
}

/* Bir hazir gorunumun nihai renkleri. `day` 0..1 gunesin ne kadar yukarida
   oldugu, `dawn` 0..1 ufka ne kadar yakin oldugudur. */
static void sky_colors(int cycle, double day, double dawn, int is_cycle,
                       float top[3], float horizon[3], float bottom[3], float sun[3]) {
    float lit_top[3], lit_hor[3], dark_top[3], dark_hor[3];
    double sun_base[3];

    /* Iki satir da palette_row'dan gelir; BOS satir ayni gorunumun oteki
       yarim kuresinden devralinir (bkz. palette_row). Bu yuzden burada EK bir
       karartma YOKTUR: gece gorunumunun rengi zaten paletin kendi koyu
       tonudur ve uzerine bir carpan daha uygulamak gokyuzunu simsiyah
       yapiyordu ("night" secilince ekran bos gorunuyordu). */
    palette_row(cycle, SKY_LIT_TOP, lit_top);
    palette_row(cycle, SKY_LIT_HORIZON, lit_hor);
    palette_row(cycle, SKY_DARK_TOP, dark_top);
    palette_row(cycle, SKY_DARK_HORIZON, dark_hor);
    (void)is_cycle;

    for (int c = 0; c < 3; c++) {
        double h = lerp((double)dark_hor[c], (double)lit_hor[c], day);
        top[c]     = (float)lerp((double)dark_top[c], (double)lit_top[c], day);
        /* ufuk, gunes ufka yaklastikca isinir (alacakaranlik) */
        horizon[c] = (float)clamp01(h + (double)SKY_DUSK[c] * dawn * 0.55);
        bottom[c]  = (float)clamp01((double)horizon[c] * 0.72);
    }

    /* Gunes diski: gunduz beyazi, gece ayi, ufukta turuncu. */
    for (int c = 0; c < 3; c++)
        sun_base[c] = day > 0.5 ? (double)SKY_SUN_DAY[c] : (double)SKY_SUN_NIGHT[c];
    for (int c = 0; c < 3; c++)
        sun[c] = (float)lerp(sun_base[c], (double)SKY_DUSK[c], dawn * 0.75);
}

/* ---------- slot <-> durum ---------- */

/* Gelen skaler yuvayi modulun gorunum degerlerine cevirir. Eksik yuvayi
   (argc kisa ise) belgelenmis varsayilana getirir; boylece hem tam hem kisa
   cagri bicimi ayni durumu uretir. */
static void read_slots(int argc, const char **argv, int index,
                       double *time_out, double *cloud_out, double *star_out,
                       double *sun_size_out, double *sun_bright_out, double *speed_out,
                       double *cloud_on_out) {
    double dc;
    if (index < 0 || index >= SKY_MAX) index = 0;
    dc = (argc > S_DAILY_CYCLE) ? num_arg(argc, argv, S_DAILY_CYCLE)
                                : (double)g_sky[index].daily_cycle;

    *time_out       = (argc > S_TIME)         ? num_arg(argc, argv, S_TIME)         : SKY_DEF_TIME;
    *cloud_out      = (argc > S_CLOUD_AMOUNT) ? num_arg(argc, argv, S_CLOUD_AMOUNT) : SKY_DEF_CLOUD_AMOUNT;
    *speed_out      = (argc > S_CLOUD_SPEED)  ? num_arg(argc, argv, S_CLOUD_SPEED)  : SKY_DEF_CLOUD_SPEED;
    *star_out       = (argc > S_STAR_AMOUNT)  ? num_arg(argc, argv, S_STAR_AMOUNT)  : SKY_DEF_STAR_AMOUNT;
    *sun_size_out   = (argc > S_SUN_SIZE)     ? num_arg(argc, argv, S_SUN_SIZE)     : SKY_DEF_SUN_SIZE;
    *sun_bright_out = (argc > S_SUN_BRIGHTNESS) ? num_arg(argc, argv, S_SUN_BRIGHTNESS) : SKY_DEF_SUN_BRIGHT;
    /* CloudON bir ANAHTARDIR, olcek degil: script `true` de yazsa `1` de yazsa
       ayni katman acilir. 0 (ya da hic verilmemis) ise kapali kalir. */
    *cloud_on_out   = (argc > S_CLOUD_ON)     ? (num_arg(argc, argv, S_CLOUD_ON) != 0.0 ? 1.0 : 0.0) : 0.0;

    g_slot[S_HANDLE]         = (double)index;
    g_slot[S_DAILY_CYCLE]    = dc;
    g_slot[S_TIME]           = *time_out;
    g_slot[S_DAY_LENGTH]     = (argc > S_DAY_LENGTH) ? num_arg(argc, argv, S_DAY_LENGTH) : SKY_DEF_DAY_LENGTH;
    g_slot[S_CLOUD_AMOUNT]   = *cloud_out;
    g_slot[S_CLOUD_SPEED]    = *speed_out;
    g_slot[S_STAR_AMOUNT]    = *star_out;
    g_slot[S_SUN_SIZE]       = *sun_size_out;
    g_slot[S_SUN_BRIGHTNESS] = *sun_bright_out;
    g_slot[S_CLOUD_ON]       = *cloud_on_out;
}

/* Dongu/gun ilerlemesini hesaplar, modul durumunu gunceller ve gunesin
   yuksekligini dondurur.

   KARE KORUMASI: sabit gorunumlerde zaman akmaz, o yuzden koruma yalnizca
   CYCLE dalini ilgilendirir. Ayni karede ikinci cagri `dt` yerine 0 kullanir;
   boylece `Update()` + `Draw()` gunu IKI KAT hizlandirmaz. */
static double sky_advance(int index, double dt, int *cycle_out, double *day_out) {
    SkyStore *s;
    int    cycle;
    double phase, elev, day;

    if (index < 0 || index >= SKY_MAX) index = 0;
    s = &g_sky[index];
    cycle = (int)g_slot[S_DAILY_CYCLE];
    if (cycle < 0 || cycle >= SKY_CYCLE_COUNT) cycle = SKY_DEF_CYCLE;
    g_slot[S_DAILY_CYCLE] = (double)cycle;
    g_slot[S_TIME] = wrap01(g_slot[S_TIME]);
    s->daily_cycle = cycle;

    if (cycle < SKY_FIXED_COUNT) {
        /* Sabit gorunum: gunes yeri tablodan gelir, zaman akmaz. */
        phase = SKY_PHASES[cycle].phase;
        s->phase = phase;
        s->prev_time = g_slot[S_TIME];
    } else {
        double day_length = g_slot[S_DAY_LENGTH];
        double dtime = g_slot[S_TIME] - s->prev_time;
        double step  = g_advanced_this_frame ? 0.0 : dt;   /* kare basina BIR kez */
        if (day_length < 1.0) day_length = SKY_DEF_DAY_LENGTH;
        if (dtime < 0.0) dtime += 1.0;               /* script saati geri sardi */
        if (dtime > SKY_MAX_STEP) dtime = SKY_MAX_STEP;
        s->phase = wrap01(s->phase + dtime + step / day_length);
        s->prev_time = g_slot[S_TIME];
        phase = s->phase;
    }
    g_advanced_this_frame = 1;

    elev = phase_elevation(phase);
    if (elev > 1.0) elev = 1.0; else if (elev < -1.0) elev = -1.0;
    day = clamp01((elev + SKY_HORIZON_BAND) / (SKY_DAY_BAND + SKY_HORIZON_BAND));
    *cycle_out = cycle;
    *day_out   = day;
    return elev;
}

/* ---------- cizim ---------- */

static double fn_draw(int argc, const char **argv) {
    double time, cloud, star, sun_size, sun_bright, speed, cloud_on;
    double dt = frame_seconds();
    double phase, elev, day, dawn, night;
    int    index = (int)num_arg(argc, argv, S_HANDLE);
    int    cycle = SKY_DEF_CYCLE;
    float  top[3], horizon[3], bottom[3], sun[3], sun_dir[3], eye[3];
    float  sun_params[4], cloud_params[4], star_params[4], sky_params[4];
    float  band_params[4];

    if (index < 0 || index >= SKY_MAX) index = 0;
    read_slots(argc, argv, index, &time, &cloud, &star, &sun_size, &sun_bright, &speed, &cloud_on);
    elev = sky_advance(index, dt, &cycle, &day);
    phase = g_sky[index].phase;
    g_sky[index].used = 1;

    dawn  = clamp01(1.0 - fabs(elev) / SKY_DAY_BAND);
    night = 1.0 - day;

    sky_colors(cycle, day, dawn, cycle == SKY_CYCLE, top, horizon, bottom, sun);
    sun_direction(elev, phase, sun_dir);   /* azimut gunun saatine gore doner */

    /* Cizimde kullanilan gradyan SU MODULU icin saklanir
       (bkz. dosyanin sonundaki gcl_skybox_sky_colors). */
    g_sky_top[0] = top[0]; g_sky_top[1] = top[1]; g_sky_top[2] = top[2];
    g_sky_hor[0] = horizon[0]; g_sky_hor[1] = horizon[1]; g_sky_hor[2] = horizon[2];
    g_sky_colors_valid = 1;

    /* Gunesin DUNYA YONU, rengi ve gucu saklanir; RaylibSimpleLight.dll
       bunlari gcl_skybox_sun_* sembolleriyle okur. Cizimde kullanilan
       degerlerin AYNISI yazilir, boylece ekrandaki disk ile aydinlatilan
       yuz ayni yeri gosterir. */
    g_sun_dir[0] = sun_dir[0];
    g_sun_dir[1] = sun_dir[1];
    g_sun_dir[2] = sun_dir[2];
    g_sun_rgb[0] = sun[0];
    g_sun_rgb[1] = sun[1];
    g_sun_rgb[2] = sun[2];
    g_sun_power  = (float)day;
    g_sun_valid  = 1;

    /* Penceresiz calisma da guvenli olmali: hazirlik basarisizsa (GL baglami
       yok ya da shader derlenmedi) sessizce cikilir. */
    if (!sky_ready()) return 0.0;

    g_slot[S_TIME] = phase;
    if (!camera_eye(eye)) { eye[0] = eye[1] = eye[2] = 0.0f; }

    sun_params[0] = (float)cos(sun_size * SKY_DEG2RAD * 0.5);
    sun_params[1] = (float)cos(sun_size * SKY_DEG2RAD * 2.0);
    sun_params[2] = (float)(0.45 * clamp01(sun_bright));
    /* Yildiz/samanyolu kapisi: TWILIGHT ve SUNSET artik yildiz gostermez
       (bkz. star_visibility). Bulut aydinlatmasi da ayni degeri kullanir;
       alacakaranlikta bulutlarin hala aydinlik gorunmesi DOGRU sonuctur. */
    sun_params[3] = (float)star_visibility(night);

    /* Esik `gclFbm`in ORTALAMASININ (0.484) altinda tutulur; ustunde secilirse
       bulut ancak gurultu ortalamayi astigi anda cikar ve gokyuzu pratikte bos
       kalir (bkz. gcl_skybox_shader.h). */
    cloud_params[0] = 0.42f;
    cloud_params[1] = 0.22f;
    cloud_params[2] = 3.0f;
    cloud_params[3] = (float)clamp01(cloud / 100.0);

    /* starParams.y GOK KURESINDEKI hucre sikligidir; uc katman bunun
       0.62 / 1.45 / 3.05 katiyla orneklenir (bkz. gcl_skybox_shader.h).
       60 ile kure basina ~37 / 87 / 183 hucrelik katmanlar olusur: gercek bir
       gece gokyuzu yogunlugu. */
    star_params[0] = 0.0f;
    star_params[1] = 60.0f;
    star_params[2] = (float)clamp01(star / 100.0);
    star_params[3] = 2.0f;                                    /* pirliti hizi     */

    /* Samanyolu banti: siddet (x), bant genisligi (y), EK CARTOON BULUT
       katmani anahtari (z), ufuk isimasi (w). Siddet geceyle olceklenir;
       gunduz gokyuzunde boyle bir bant gorunmez. */
    band_params[0] = 0.55f;
    band_params[1] = 26.0f;
    band_params[2] = (float)clamp01(cloud_on);                /* CloudON anahtari */
    band_params[3] = 0.22f;

    /* Bulut saati HER KAREDE ilerler; hizi CloudSpeed belirler (varsayilan 1).
       Sabit gorunumlerde de bulutlar aksin diye gok saatine baglanmaz. */
    g_sky[index].cloud_time += dt * speed;

    sky_params[0] = (float)(phase * 1000.0);                  /* pirliti saati    */
    sky_params[1] = 1.0f;                                     /* gradyan ussu     */
    /* fmod sart: saat saatlerce akinca gurultu koordinati buyur ve `sin`
       hash'i hassasiyetini yitirir. 1000 sn'de bir basa doner. */
    sky_params[2] = (float)fmod(g_sky[index].cloud_time, 1000.0); /* bulut saati  */
    sky_params[3] = 0.30f;                                    /* ufuk sisu        */

    SetShaderValue(g_sky_shader, g_loc_top,        top,          SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_horizon,    horizon,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_bottom,     bottom,       SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_sun_dir,    sun_dir,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_sun_color,  sun,          SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_sun_params, sun_params,   SHADER_UNIFORM_VEC4);
    SetShaderValue(g_sky_shader, g_loc_cloud,      horizon,      SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_cloud_shade,bottom,       SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_cloud_p,    cloud_params, SHADER_UNIFORM_VEC4);
    SetShaderValue(g_sky_shader, g_loc_star,       sun,          SHADER_UNIFORM_VEC3);
    SetShaderValue(g_sky_shader, g_loc_star_p,     star_params,  SHADER_UNIFORM_VEC4);
    SetShaderValue(g_sky_shader, g_loc_sky_p,      sky_params,   SHADER_UNIFORM_VEC4);
    SetShaderValue(g_sky_shader, g_loc_band_p,     band_params,  SHADER_UNIFORM_VEC4);

    /* Cizimde kullanilan BUTUN uniform'lar saklanir; su modulu bunlarla ayni
       `gclSkyColor()` fonksiyonunu cagirir ve boylece ekrandaki gokyuzunun
       BIREBIR aynisini yansitir. Yalnizca gradyani vermek yetmezdi: bulutlar,
       gunes diski, samanyolu ve yildizlar da gokun parcasi (bkz. g_sky_state
       notu ve dosyanin sonundaki gcl_skybox_uniform_state). */
    {
        float *s = g_sky_state;
        int i;
        for (i = 0; i < 3; i++) s[ 0+i] = top[i];
        for (i = 0; i < 3; i++) s[ 3+i] = horizon[i];
        for (i = 0; i < 3; i++) s[ 6+i] = bottom[i];
        for (i = 0; i < 3; i++) s[ 9+i] = sun_dir[i];
        for (i = 0; i < 3; i++) s[12+i] = sun[i];
        for (i = 0; i < 4; i++) s[15+i] = sun_params[i];
        for (i = 0; i < 3; i++) s[19+i] = horizon[i];      /* cloudColor */
        for (i = 0; i < 3; i++) s[22+i] = bottom[i];       /* cloudShade */
        for (i = 0; i < 4; i++) s[25+i] = cloud_params[i];
        for (i = 0; i < 3; i++) s[29+i] = sun[i];          /* starColor  */
        for (i = 0; i < 4; i++) s[32+i] = star_params[i];
        for (i = 0; i < 4; i++) s[36+i] = sky_params[i];
        for (i = 0; i < 4; i++) s[40+i] = band_params[i];
        g_sky_state_valid = 1;
    }

    /* Kubbe: kameraya otelenmis, 1:1 olcekli, donmesiz. Derinlik YAZILMAZ
       (gokyuzu her seyin arkasinda kalir), yuzey elemesi kapatilir (kubbe ic
       yuzunden gorunur). Uc durum da cagri sonunda geri alinir. */
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawMesh(g_dome, g_dome_material, MatrixTranslate(eye[0], eye[1], eye[2]));
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    /* Kare bitti: bir sonraki Update yeniden ilerletebilir (bkz.
       g_advanced_this_frame). */
    g_advanced_this_frame = 0;
    return 0.0;
}

/* ---------- modul uyeleri ---------- */

/* CreateSimpleSkybox() -> kayit indeksi (script bunu Skybox.Handle'da tutar).
   Her cagri YENI bir yuva alir; boylece ayni anda birden fazla gokyuzu
   (ornegin gecis efekti icin) kurulabilir. */
static double fn_create_simple_skybox(int argc, const char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < SKY_MAX; i++) {
        if (g_sky[i].used) continue;
        memset(&g_sky[i], 0, sizeof(g_sky[i]));
        g_sky[i].used        = 1;
        g_sky[i].daily_cycle = SKY_DEF_CYCLE;
        g_sky[i].phase       = SKY_PHASES[SKY_DEF_CYCLE].phase;
        g_sky[i].prev_time   = SKY_DEF_TIME;

        g_slot[S_HANDLE]         = (double)i;
        g_slot[S_DAILY_CYCLE]    = (double)SKY_DEF_CYCLE;
        g_slot[S_TIME]           = SKY_DEF_TIME;
        g_slot[S_DAY_LENGTH]     = SKY_DEF_DAY_LENGTH;
        g_slot[S_CLOUD_AMOUNT]   = SKY_DEF_CLOUD_AMOUNT;
        g_slot[S_CLOUD_SPEED]    = SKY_DEF_CLOUD_SPEED;
        g_slot[S_STAR_AMOUNT]    = SKY_DEF_STAR_AMOUNT;
        g_slot[S_SUN_SIZE]       = SKY_DEF_SUN_SIZE;
        g_slot[S_SUN_BRIGHTNESS] = SKY_DEF_SUN_BRIGHT;
        g_slot[S_CLOUD_ON]       = 0.0;   /* ek katman varsayilan KAPALI */
        return (double)i;
    }
    g_slot[S_HANDLE] = -1.0;
    return -1.0;
}

/* Update() — gokyuzunun saatini ilerletir ve rengini hesaplar; EKRANA BIR SEY
   CIZMEZ (dokumandaki `sky.Update(); sky.Draw();` ikilisi bunu gerektirir).

   Kare korumasi içindedir: Update ilerlettikten sonra ayni karedeki Draw
   ilerletmez (bkz. g_advanced_this_frame). */
static double fn_update(int argc, const char **argv) {
    double time, cloud, star, sun_size, sun_bright, speed, cloud_on;
    double dt = frame_seconds();
    double elev, day;
    int    index = (int)num_arg(argc, argv, S_HANDLE);
    int    cycle = SKY_DEF_CYCLE;

    if (index < 0 || index >= SKY_MAX) index = 0;
    read_slots(argc, argv, index, &time, &cloud, &star, &sun_size, &sun_bright, &speed, &cloud_on);
    elev = sky_advance(index, dt, &cycle, &day);
    g_slot[S_TIME] = g_sky[index].phase;
    g_sky[index].used = 1;
    (void)elev;
    return 0.0;
}

/* Okuma erisimcileri: runner bir struct degiskenini bu adlardan doldurur
   (bkz. SimpleRunner'daki g_native_last_maps[] ve store_slots_into_var). */
static double fn_handle(int argc, const char **argv)         { (void)argc; (void)argv; return g_slot[S_HANDLE]; }
static double fn_daily_cycle(int argc, const char **argv)    { (void)argc; (void)argv; return g_slot[S_DAILY_CYCLE]; }
static double fn_time(int argc, const char **argv)           { (void)argc; (void)argv; return g_slot[S_TIME]; }
static double fn_day_length(int argc, const char **argv)     { (void)argc; (void)argv; return g_slot[S_DAY_LENGTH]; }
static double fn_cloud_amount(int argc, const char **argv)   { (void)argc; (void)argv; return g_slot[S_CLOUD_AMOUNT]; }
static double fn_cloud_speed(int argc, const char **argv)    { (void)argc; (void)argv; return g_slot[S_CLOUD_SPEED]; }
static double fn_star_amount(int argc, const char **argv)    { (void)argc; (void)argv; return g_slot[S_STAR_AMOUNT]; }
static double fn_sun_size(int argc, const char **argv)       { (void)argc; (void)argv; return g_slot[S_SUN_SIZE]; }
static double fn_sun_brightness(int argc, const char **argv) { (void)argc; (void)argv; return g_slot[S_SUN_BRIGHTNESS]; }
static double fn_cloud_on(int argc, const char **argv)       { (void)argc; (void)argv; return g_slot[S_CLOUD_ON]; }

/* LastSlot(): argumansiz -> yuva sayisi; LastSlot(i) -> i. yuva.
   Struct metot cagrilarindan sonra (sky.Update(), sky.Draw()) runner
   degiskenin alanlarini bu kanaldan tazeler. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)SKY_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= SKY_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* Sabit adlari: `sky.DailyCycle = RaylibSKYBOX.night;` yazimini mumkun kilar.
   Degeri dogrudan donduren birer uyedir, yani raylib'in renk sabitleriyle AYNI
   yoldan cozulur — ayri bir mekanizma gerekmez. */
static double fn_const_night(int argc, const char **argv)    { (void)argc; (void)argv; return (double)SKY_NIGHT; }
static double fn_const_day(int argc, const char **argv)      { (void)argc; (void)argv; return (double)SKY_DAY; }
static double fn_const_morning(int argc, const char **argv)  { (void)argc; (void)argv; return (double)SKY_MORNING; }
static double fn_const_noon(int argc, const char **argv)     { (void)argc; (void)argv; return (double)SKY_NOON; }
static double fn_const_evening(int argc, const char **argv)  { (void)argc; (void)argv; return (double)SKY_EVENING; }
static double fn_const_midnight(int argc, const char **argv) { (void)argc; (void)argv; return (double)SKY_MIDNIGHT; }
static double fn_const_dawn(int argc, const char **argv)     { (void)argc; (void)argv; return (double)SKY_DAWN; }
static double fn_const_sunset(int argc, const char **argv)   { (void)argc; (void)argv; return (double)SKY_SUNSET; }
static double fn_const_sunrise(int argc, const char **argv)  { (void)argc; (void)argv; return (double)SKY_SUNRISE; }
static double fn_const_twilight(int argc, const char **argv) { (void)argc; (void)argv; return (double)SKY_TWILIGHT; }
static double fn_const_cycle(int argc, const char **argv)    { (void)argc; (void)argv; return (double)SKY_CYCLE; }

/* Kac hazir gorunum var? (IDE'nin tamamlama listesi ve script tarafi icin.) */
static double fn_cycle_count(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)SKY_CYCLE_COUNT;
}

/* GCL scriptleri bu uyeleri modulun belgeledigi adlarla cagirir
   (RaylibSKYBOX.CreateSimpleSkybox, sky.Update, ...); nesne yonlendirme
   dizeleri script'e bakan yazimi C fonksiyon adindan bagimsiz tutar. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_simple_skybox, "CreateSimpleSkybox"),
    E(update,               "Update"),
    E(draw,                 "Draw"),
    E(handle,               "Handle"),
    E(daily_cycle,          "DailyCycle"),
    E(time,                 "Time"),
    E(day_length,           "DayLength"),
    E(cloud_amount,         "CloudAmount"),
    E(cloud_speed,          "CloudSpeed"),
    E(star_amount,          "StarAmount"),
    E(sun_size,             "SunSize"),
    E(sun_brightness,       "SunBrightness"),
    E(cloud_on,             "CloudON"),
    E(last_slot,            "LastSlot"),
    E(const_night,          "night"),
    E(const_day,            "day"),
    E(const_morning,        "morning"),
    E(const_noon,           "noon"),
    E(const_evening,        "evening"),
    E(const_midnight,       "midnight"),
    E(const_dawn,           "dawn"),
    E(const_sunset,         "sunset"),
    E(const_sunrise,        "sunrise"),
    E(const_twilight,       "twilight"),
    E(const_cycle,          "cycle"),
    E(cycle_count,          "CycleCount"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibskybox_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}

/* ---------- Gunes durumu, RaylibSimpleLight.dll icin ----------

   Bu modul gunesi HEM CIZER hem hesaplar. Isik modulu ayni gunesi
   aydinlatmak icin kullanacagi icin yon, renk ve guc burada disa acilir;
   boylece iki modul TEK bir gunes tanimi paylasir ve ekranda gorunen disk ile
   aydinlatilan yuz hizasiz kalmaz.

   Donus sozlesmesi: 0 = henuz bir kare cizilmedi (deger yok, cagiran kendi
   varsayilanini kullanmali), 1 = out yazildi. Bunlar GCL uyesi DEGILDIR;
   script'ten gorunmezler, yalnizca DLL'ler arasi kanaldir. */
GCL_EXPORT int gcl_skybox_sun_direction(float *out3) {
    if (!out3 || !g_sun_valid) return 0;
    out3[0] = g_sun_dir[0];
    out3[1] = g_sun_dir[1];
    out3[2] = g_sun_dir[2];
    return 1;
}

GCL_EXPORT int gcl_skybox_sun_color(float *out3) {
    if (!out3 || !g_sun_valid) return 0;
    out3[0] = g_sun_rgb[0];
    out3[1] = g_sun_rgb[1];
    out3[2] = g_sun_rgb[2];
    return 1;
}

/* 0 = gunes ufkun altinda (gece), 1 = tam gunduz. Isigin siddetini olceklemek
   icin: gece gunes kapali olmali, yoksa sahne alttan aydinlanir. */
GCL_EXPORT double gcl_skybox_sun_power(void) {
    if (!g_sun_valid) return 0.0;
    return (double)g_sun_power;
}

/* Son karede CIZILEN gokyuzu gradyani: out6[0..2] = zenit, out6[3..5] = ufuk.

   Su modulu bunu OKUR (gcl_SimpleWater.c: sky_gradient), boylece suyun
   yansittigi gradyan ekrandaki gokyuzuyle AYNI olur — iki modul tek bir
   gokyuzu tanimi paylasir. Donus sozlesmesi sun_* ile aynidir: 0 = henuz bir
   kare cizilmedi (cagiran kendi varsayilanini kullanmali), 1 = out yazildi.
   GCL uyesi DEGILDIR; script'ten gorunmez, yalnizca DLL'ler arasi kanaldir. */
GCL_EXPORT int gcl_skybox_sky_colors(float *out6) {
    if (!out6 || !g_sky_colors_valid) return 0;
    out6[0] = g_sky_top[0]; out6[1] = g_sky_top[1]; out6[2] = g_sky_top[2];
    out6[3] = g_sky_hor[0]; out6[4] = g_sky_hor[1]; out6[5] = g_sky_hor[2];
    return 1;
}

/* GOKYUZUNUN BUTUN UNIFORM DURUMU — 45 float, g_sky_state'deki sirayla.

   Su modulu bunu okur ve AYNI degerlerle paylasilan `gclSkyColor()` cagirir;
   boylece suyun yansittigi gok ile ekranda cizilen gok BIREBIR ayni olur.
   Yalnizca gradyani (gcl_skybox_sky_colors) vermek yetmez: gokyuzunun
   karakterini veren sey bulutlar, gunes diski ve isimasidir; gradyan tek
   basina duz bir mavi levha uretir ve su "transparan mavi kaplama" gibi
   gorunur — bildirilen hata tam olarak buydu.

   Donus sozlesmesi: 0 = henuz bir kare cizilmedi (cagiran kendi yedegine
   dusmeli), 1 = out yazildi. GCL uyesi DEGILDIR; script'ten gorunmez,
   yalnizca DLL'ler arasi kanaldir. */
GCL_EXPORT int gcl_skybox_uniform_state(float *out45) {
    int i;
    if (!out45 || !g_sky_state_valid) return 0;
    for (i = 0; i < SKY_STATE_FLOATS; i++) out45[i] = g_sky_state[i];
    return 1;
}
