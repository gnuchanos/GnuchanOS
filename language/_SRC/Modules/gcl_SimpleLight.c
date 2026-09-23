/*

#native <RaylibSimpleLight>

RaylibSimpleLight.SunLight SUN = RaylibSimpleLight.CreateSun(Raylib.YELLOW);
SUN.Rotate.x = 0;
SUN.Rotate.z = 90;
SUN.Rotate.y = 0;
SUN.Update();

*/

/* Backend.

   CreateSun(color) registers a directional light (raylib's LIGHT_DIRECTIONAL)
   and returns its index, which the script keeps in `SunLight.Handle`.

   The runtime flattens a native struct argument into numbers in DECLARATION
   order and publishes values back through the LastSlot channel (see
   SharedPipeline/gcl_native_types.c). Slot table:

       0 Handle   1 Rotate.x   2 Rotate.y   3 Rotate.z

   ---------- GUNES ARTIK SKYBOX'TAN GELIR ----------

   RaylibSKYBOX.dll gunesi HEM CIZER hem hesaplar: yonu gunun saatinden
   (phase), rengi paletten turer. Iki modul kendi acisini bagimsiz hesaplarsa
   EKRANDA GORUNEN gunes ile AYDINLATILAN yuz hizasiz kalir — sahne, gunesi
   baska yerde olan bir dunya gibi gorunur.

   Bu yuzden RaylibSkybox.dll yuklu ise isik YONUNU, RENGINI ve SIDDETINI
   oradan alir (gcl_skybox_sun_direction / _color / _power). Yuklu degilse
   eski davranis aynen surer: yon script'in `Rotate.*` alanlarindan hesaplanir.

   SONUC: skybox kullanilan bir sahnede `SUN.Rotate.*` ARTIK YONU BELIRLEMEZ;
   gunes nerede ise isik oradadir. `SUN.Update()` cagrilmaya devam etmelidir —
   uniform'lari o yazar. Renk de skybox'tan gelir; `CreateSun(...)`'a verilen
   renk yalnizca skybox YOKKEN kullanilir.

   ---------- IKI AYRI ISIK YOLU: DOGRUDAN ve DOLAYLI ----------

   Bir yuzun parlakligi IKI terimin toplamidir (bkz. assets/lighting.fs):

       dogrudan : lights[i].color.rgb * max(dot(normal, light), 0)
       dolayli  : ambient.rgb   (agirlik MODULDE islenmistir, shader'da yok)

   DOGRUDAN terim YONE baglidir ve asil parlakligi tasir. Gunes tam tepedeyken
   yalnizca ust yuzu aydinlatir; yan yuzler `NdotL = 0` alir ve simsiyah
   kalirdi — bu yuzden DOLAYLI terim vardir. Ambient normal vektoru
   KULLANMADIGI icin hicbir yuzu digerinden ayiramaz: yan yuzleri esit
   yukseltir, alttan parlama yaratamaz.

    TUZAK — AYNI DEGERI IKI KEZ OLCEKLEMEK: ambient'in nihai siddeti UC yerde
    belirlenir ve ucu de tutarli olmak zorundadir:

        1. modul  : taban k = SUN_AMBIENT_NIGHT..SUN_AMBIENT_DAY   (nihai k)
        2. modul  : SUN_AMBIENT_SKY_FLOOR — gunes ufuktayken taban k
        3. shader : ambient.rgb OLDUGU GIBI — ek carpan YOK

    3. madde bir kez ihlal edildi: shader `ambient.rgb * 0.1` yapiyordu, yani
    modulden gelen deger IKINCI KEZ kucultuluyordu. Efektif dolgu gokyuzu
    renginin %1-5'i kadar kaliyordu. Sonuc: gunese bakmayan yuzler ve butun
    safak/gun batimi sahnesi kapkaraydi — ustunde parlak bir gokyuzu varken.
    Ikinci carpan KALDIRILDI (bkz. assets/lighting.fs).

    Ayrica DOGRUDAN terim SUN_DIRECT_MAX ile sinirlidir. Sinirsiz birakilinca
    (1.0) `dogrudan + dolayli` toplami 1'i asiyor ve gunese tam bakan yuz
    BEYAZA kirpiliyordu: dokudaki renk kayboluyor, sahne "yikanmis" gorunuyordu.

   THE SUN ANGLE, skybox YOKKEN gecerli olan kural:

       Rotate.z = elevation, 0 = on the horizon, 90 = straight overhead (noon)
       Rotate.y = azimuth, 0 = +Z, 90 = +X

       direction = ( sin(az)*cos(el), sin(el), cos(az)*cos(el) )
       position  = direction * SUN_DISTANCE,  target = the world origin

   A directional light has no meaningful distance, so the far position only
   exists because raylib's Light struct carries position/target; the shader
   rebuilds the direction itself with `-normalize(target - position)`.

   Nothing here touches GL: the uniforms are written through Raylib.dll's
   exported helpers, and the shader they belong to is the one RaylibShader.dll
   created last (`gcl_shader_current`). Both modules run in the same process. */

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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SUN_SLOT_COUNT   4
#define SUN_MAX          4
#define SUN_DISTANCE     1000.0f
#define SUN_DEG2RAD      0.017453292519943295

/* Gece dogrudan isigin sonme bandi.

   NEDEN GEREKLI: gece `sunDir`in Y bileseni NEGATIFTIR (gunes ufkun altinda).
   Isik vektoru asagiyi gosterince `dot(normal, light)` taban yuzunde POZITIF
   cikar ve kubun ALT YUZU aydinlanir: "gece alttan parliyor" goruntusunun tek
   sebebi budur. Dogrudan isik bu yuzden yalnizca gunes ufkun USTUNDEYKEN
   acilir; `dir_y = 0` (tam ufuk) sonmus, `0.20` (~11.5 derece) tamdir. Arada
   yumusak gecis, gun batiminda sicrama olmaz. */
#define SUN_HORIZON_FADE 0.20f

/* Gunesin vurmadigi yuzleri tasiyan dolayli isik. `k` 0..1'dir ve rengin
   kanallarina carpilir. GUNDUZ > GECE — bu bir tercih degil, isigin
   fizigidir: gunduz gokyuzu aydinlik (guclu sacilma), gece karanliktir.

   BUNLAR ARTIK NIHAI DEGERLERDIR: shader bu sayilari OLDUGU GIBI kullanir
   (lighting.fs: `lit += texelLinear*ambient.rgb`). Onceden shader ayrica
   `*0.1` yapiyordu, yani ayni deger IKI KEZ kucultuluyordu; efektif dolgu
   gokyuzu renginin %1-5'i kadar kaliyordu. Sonuc: gunese bakmayan yuzler ve
   butun safak/gun batimi sahnesi kapkaraydi — oysa gokyuzu o an parlaktir.
   Ikinci olcek KALDIRILDI; denge bu iki sayiyla kurulur.

   Oran ~3.6:1. Ters kurulursa yan yuzler (NdotL = 0) gece gunduzden daha
   parlak gorunur; bir kez tam boyle oldu, bkz. basliktaki "IKI AYRI ISIK
   YOLU" notu. */
#define SUN_AMBIENT_DAY   0.18f
#define SUN_AMBIENT_NIGHT 0.05f

/* Gokyuzu parlakliginin ambient'e giren EN AZ payi.

   Ambient yalnizca `sun_power` (gunesin yuksekligi) ile olceklenseydi, gunes
   ufka indiginDE (power ~0.5) dolgu yariya inerdi; oysa o anda gokyuzu HALA
   aydinliktir (ufuk turuncu). Sahne, ustunde parlak bir gokyuzu varken
   karanliga gomulurdu. Taban 0.35 gece gorunumune de ay isigini tasir: gece
   tamamen siyah olmaz, yalnizca karanlik olur. */
#define SUN_AMBIENT_SKY_FLOOR 0.35f

/* Dogrudan isigin TAVANI.

   Nihai parlaklik `dogrudan + dolayli` oldugu icin tavansiz bir dogrudan
   terim (1.0), ambient ile toplandiginda 1'i asar ve gunese tam bakan yuzler
   BEYAZA KIRPILIR — dokudaki renk kaybolur ("yikanmis" gorunum). 0.80 ile
   aydinlik yuz 0.80 + 0.18 = 0.98'de kalir: kirpma yok, kontrast var.
   Rengin kanallarina islenir, cunku raylib'in isik uniform'unda ayri bir
   intensity alani yoktur. */
#define SUN_DIRECT_MAX 0.80f

/* raylib's Light.type values (raylib.h) */
#define SUN_LIGHT_DIRECTIONAL 0

enum { S_HANDLE = 0, S_ROT_X, S_ROT_Y, S_ROT_Z };

static double       g_slot[SUN_SLOT_COUNT] = { -1.0, 0.0, 0.0, 0.0 };
static unsigned int g_sun_color[SUN_MAX]   = {0};
static int          g_sun_count            = 0;
static int          g_last_sun             = -1;

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* ---------- Raylib.dll: uniforms ---------- */

typedef void (*GclShaderSetLightFn)(int handle, int index, int enabled, int type,
                                    float px, float py, float pz,
                                    float tx, float ty, float tz,
                                    unsigned int color);
typedef void (*GclShaderSetAmbientFn)(int handle, unsigned int ambient);

#ifdef _WIN32
static void *raylib_symbol(const char *name) {
    HMODULE h = GetModuleHandleA("Raylib.dll");
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *raylib_symbol(const char *name) { (void)name; return NULL; }
#endif

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

/* ---------- RaylibSkybox.dll: gunesin TEK kaynagi ----------

   Semboller tembel cozulur: skybox yuklu degilse hepsi NULL kalir ve modul
   eski davranisina (script'in Rotate alanlari) duser. */
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

/* ---------- RaylibShader.dll: the shader in use ---------- */

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

/* Fallback for programs that never call BeginShaderMode(): RaylibShader.dll
   answers the shader CreateSimpleShader() returned last. */
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

   Order matters: the shader being DRAWN with wins (Raylib.dll's answer), and
   "the last one created" is only the fallback. Targeting the last created
   program instead would feed the uniforms of a shader that is not on screen,
   which renders the drawn model BLACK — every light reads as disabled. */
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

/* Dolayli isik dolgusu. `day` 0 = gece, 1 = gunduz. GUNDUZ DAHA GUCLU —
   gerekcesi SUN_AMBIENT_DAY taniminda. Skybox yoksa `day` 1 verilir ve
   davranis eskisi gibi kalir. */
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

/* CreateSun(color) -> sun index (the script keeps it in SunLight.Handle). */
static double fn_create_sun(int argc, const char **argv) {
    unsigned int color = (argc > 0 && argv[0]) ? (unsigned int)strtoul(argv[0], NULL, 10)
                                               : 0xFFFFFFFFu;   /* Raylib.WHITE */
    int index = g_sun_count;

    if (index >= SUN_MAX) return -1.0;
    g_sun_color[index] = color;
    g_sun_count++;
    g_last_sun = index;

    g_slot[S_HANDLE] = (double)index;
    g_slot[S_ROT_X]  = 0.0;
    g_slot[S_ROT_Y]  = 0.0;
    g_slot[S_ROT_Z]  = 90.0;   /* midnight is dark; noon is the sane default */
    return (double)index;
}

/* Update() — feed the shader.

   YON  : RaylibSkybox.dll yuklu ve bir kare cizilmisse oradan (gunes nerede
          ise isik oradadir), yoksa script'in `Rotate.*` alanlarindan.
   RENK : skybox varsa onun gunes rengi (gun batiminda turuncu, gece ay
          beyazi), yoksa CreateSun()'a verilen renk.
   ACIK : skybox varsa ve gunes ufkun USTUNDEyse acilir. Gece kapanir; aksi
          halde negatif isik vektoru tabani aydinlatir (bkz. SUN_HORIZON_FADE).
   AMBIENT: her zaman yazilir; gunesin vurmadigi yuzleri tasiyan tek sey budur.

   Arguments are the flattened SunLight slots (see the header): the rotation is
   taken from them when the skybox is absent. A missing shader is not an error:
   the light simply has nowhere to write its uniforms. */
static double fn_update(int argc, const char **argv) {
    int   index = (int)num_arg(argc, argv, S_HANDLE);
    float el    = (float)(num_arg(argc, argv, S_ROT_Z) * SUN_DEG2RAD);
    float az    = (float)(num_arg(argc, argv, S_ROT_Y) * SUN_DEG2RAD);
    float dir_x = sinf(az) * cosf(el);
    float dir_y = sinf(el);
    float dir_z = cosf(az) * cosf(el);
    float sky_dir[3];
    float sky_rgb[3];
    int   enabled = 1;          /* skybox yoksa eski davranis: isik acik */
    float ambient_day = 1.0f;   /* skybox yoksa tam gunduz ambienti */
    unsigned int color;
    /* Isik ve ambient renkleri AYRI tutulur: gece dogrudan isik 0'a cekilir,
       AMA ambient hala ay isigini tasimalidir. Ikisi tek degiskeni paylassaydi
       gece sahnesi kapkara olurdu. */
    unsigned int light_color;
    unsigned int ambient_color;
    GclShaderSetLightFn   set_light   = shader_set_light();
    GclShaderSetAmbientFn set_ambient = shader_set_ambient();
    int shader;

    if (index < 0 || index >= SUN_MAX) return 0.0;
    color  = g_sun_color[index];
    shader = target_shader();

    /* Keep the module's own view of the sun current, so Handle/RotX/RotY/RotZ
       and LastSlot() report what the script last drew with. */
    g_slot[S_HANDLE] = (double)index;
    g_slot[S_ROT_X]  = num_arg(argc, argv, S_ROT_X);
    g_slot[S_ROT_Y]  = num_arg(argc, argv, S_ROT_Y);
    g_slot[S_ROT_Z]  = num_arg(argc, argv, S_ROT_Z);
    g_last_sun       = index;

    /* ---------- skybox varsa yon, renk ve guc oradan ---------- */
    {
        GclSkyboxSunVecFn   get_dir   = skybox_sun_direction();
        GclSkyboxSunVecFn   get_color = skybox_sun_color();
        GclSkyboxSunPowerFn get_power = skybox_sun_power();
        /* Tavan skybox YOKKEN de gecerlidir: ayni `dogrudan + dolayli`
           toplami burada da kurulur. Tavansiz 1.0, ambient ile toplanip 1'i
           asar ve yuzu beyaza kirpardi. */
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
                /* DOGRUDAN isik SUN_DIRECT_MAX ile sinirlidir: 1.0 olsaydi
                   ambient ile toplandiginda 1'i asar ve gunese bakan yuz
                   BEYAZA kirpilirdi (bkz. SUN_DIRECT_MAX). Gun batiminda
                   (power ~0.5) hafif sonuk kalir, gece tamamen soner. */
                intensity = SUN_DIRECT_MAX * (0.72f + 0.28f * power);
                /* AMBIENT gokyuzunun parlakligini izler, gunesin yuksekligini
                   DEGIL: SUN_AMBIENT_SKY_FLOOR tabani sayesinde gunes ufka
                   indiginde de dolgu ayakta kalir. Aksi halde ustunde turuncu
                   bir gokyuzu varken sahne karanliga gomulurdu — "gun batimi
                   kapkara" hatasinin ikinci yarisi tam buydu. */
                ambient_day = SUN_AMBIENT_SKY_FLOOR +
                              (1.0f - SUN_AMBIENT_SKY_FLOOR) * power;
            }
        }

        /* ---------- GUNES UFKUN ALTINDAYKEN DOGRUDAN ISIK KAPANIR ----------

           `dir_y` gunesin yuksekliginin sinusudur: gece negatiftir. Negatif
           bir isik vektoru taban yuzunde `dot(normal, light) > 0` yapar ve
           kubun ALT YUZU aydinlanir — "gece alttan parliyor" goruntusunun
           tek sebebi budur. Isik bu yuzden yalnizca gunes ufkun USTUNDEYKEN
           acilir; ufka yaklastikca yumusak soner ki gun batiminda sicrama
           olmasin. */
        if (dir_y > 0.0f) {
            float fade = dir_y / SUN_HORIZON_FADE;
            if (fade > 1.0f) fade = 1.0f;
            /* Siddet rengin kanallarina islenir: raylib'in isik uniform'unda
               ayri bir "intensity" alani yok, renk zaten aydinlatma
               carpanidir. */
            light_color = scale_color(color, intensity * fade);
        } else {
            enabled = 0;
            /* Renk yine de 0'a cekilir: `enabled` bayragini yok sayan bir
               yol kalirsa bile taban aydinlanmasin. */
            light_color = scale_color(color, 0.0f);
        }

        /* Ambient, DOGRUDAN ISIK renginden AYRI hesaplanir — gece
           `light_color` 0'a cekilirken ambient ay isigini tasimaya devam
           eder. */
        ambient_color = ambient_fill(color, ambient_day);
    }

    if (shader < 0 || !set_light) return 0.0;

    set_light(shader, index, enabled, SUN_LIGHT_DIRECTIONAL,
              dir_x * SUN_DISTANCE, dir_y * SUN_DISTANCE, dir_z * SUN_DISTANCE,
              0.0f, 0.0f, 0.0f,                     /* target: the world origin */
              light_color);

    /* Ambient her zaman yazilir: gunesin vurmadigi yuzleri tasiyan tek sey
       budur ve normal vektoru kullanmadigi icin hicbir yuzu digerinden
       ayirmaz. */
    if (set_ambient) set_ambient(shader, ambient_color);
    return 0.0;
}

/* LastColor() -> the packed colour of the sun created/updated last.

   The colour arrives as a STRING from the runner (`%.17g` of the packed
   0..255 value), so this member is the way to check that what the script wrote
   is what the module actually holds — the declaration read-back channel
   (Handle/RotX/RotY/RotZ) has no room for it.

   0 means "no sun yet"; note that 0 is also a valid (fully transparent) packed
   colour, so call CreateSun first. */
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

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)SUN_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= SUN_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* GCL scripts address these members by the names the module documents
   (RaylibSimpleLight.CreateSun, SUN.Update). Handle/RotX/RotY/RotZ are the
   declaration read-back accessors and LastSlot the generic slot channel; the
   explicit strings keep the script-facing spelling independent of the C name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_sun, "CreateSun"),
    E(update,     "Update"),
    E(handle,     "Handle"),
    E(rot_x,      "RotX"),
    E(rot_y,      "RotY"),
    E(rot_z,      "RotZ"),
    E(last_color, "LastColor"),
    E(last_slot,  "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplelight_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
