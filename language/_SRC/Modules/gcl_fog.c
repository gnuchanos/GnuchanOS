/*
#native <RaylibFOG>

#// SIS: sahneyi mesafeyle yogunlasan tek bir renge dogru karistirir.
#// Update() ve Draw() AYNI isi yapar: shader'in uniform'larini doldurur.
#// Ekrana HICBIR SEY cizilmez; gokyuzu etkilenmez.
RaylibFOG.FOG fogy = RaylibFOG.CreateFog(Raylib.WHITE, 0.5f, 10.0f, 120.0f, true, false);

fogy.Color    = Raylib.WHITE;      #// sis rengi (gokyuzuyle ayni tut)
fogy.Start    = 10.0f;             #// sisin basladigi mesafe
fogy.End      = 120.0f;            #// sisin TAMAMINA dondugu mesafe
fogy.Density  = 0.9f;              #// yogunluk (exponential modlarda egriyi surer)
fogy.Mode     = RaylibFOG.ExponentialSquared;  #// Linear / Exponential / ExponentialSquared
fogy.Height   = 40.0f;             #// yukseklik sisi tavan yuksekligi
fogy.Falloff  = 0.25f;             #// yukseklik bandinin yumusakligi
fogy.Alpha    = 1.0f;              #// genel siddet (0 = sis kapali)
fogy.HeightFog = false;            #// yukseklik sisi acilsin mi
fogy.Enabled  = true;              #// false = sis tamamen kapali

fogy.Update();
fogy.Draw();

*/

/* Backend.

   RaylibFOG KENDI BASINA HICBIR SEY CIZMEZ. Ne mesh, ne GLSL programi, ne de
   ikinci bir cizim gecisi tutar. Yaptigi tek sey, sahnenin ZATEN cizildigi
   shader'in uc uniform'unu doldurmaktir:

       fogColor    sisi rengi        (0..1)
       fogParams   start, end, density, mode
       fogShape    strength, heightFog, height, falloff

   Karisim arazinin KENDI fragment shader'inda olur (assets/lighting.fs),
   pikselin gercek derinligine gore. Neden bu tek dogru sekil:

     * GOKYUZU BASKA BIR PROGRAMDIR (skybox shader). fogColor oraya hic
       ulasmaz, bu yuzden gokyuzu ne karartilir ne ortulur.
     * Karisim orani her pikselin GERCEK uzakliginin fonksiyonudur; gecis
       duzgun bir GRADYANDIR. Halka yok, silindir yok, arkasindan bakilan bir
       siluet yok.

   Onceki surum bunu geometriyle yapiyordu: gokyuzune opak bir "duvar" ve bir
   "etek" cizip derinlik yazarak mesafeyi kesiyordu. O yaklasim yanlisti ve
   ekranda SIYAH BIR SILINDIR olarak gorunuyordu — cizim sirasi geregi
   gokyuzunu kapatiyor, opak arazi de sisi eziyordu. Tasarim bu yuzden
   tamamen degistirildi.

   SLOT TABLOSU — SharedPipeline/gcl_native_types.c (`f_fog`) ve
   GCL/SimpleRunner/gcl_runner.c (`last_fog` + g_native_slot_maps) ile BIREBIR
   ayni kalmak zorundadir:

       0 Handle          1 Color          2 Density        3 Start
       4 End             5 Mode           6 Enabled        7 Height
       8 Falloff         9 Alpha         10 Noise         11 HeightFog

   Runtime bir native struct argümanini BILDIRIM SIRASINA gore sayilara acar
   ve degerleri LastSlot kanaliyla geri yayinlar; bu yuzden script'in
   `fogy.End = 100.0f;` atamasi buraya argv olarak gelir.

   Sabitler (`RaylibFOG.Linear` ve arkadaslari) ayni yoldan cozulen siradan
   uyelerdir — ayri bir mekanizma yok. */

#include "gcl_module.h"

/* windows.h, wingdi.h/winuser.h'yi cekmemeli: onlarin Rectangle/CloseWindow
   adlari raylib'in adlariyla catisir. Burada yalnizca GetProcAddress lazim. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- olculer ---------- */

#define FOG_SLOT_COUNT      12
#define FOG_MAX             4

/* Fades driven by `Falloff`: clamped so the height band can neither vanish
   (0) nor swallow the whole column (1). */
#define FOG_SOFT_MIN        0.05f
#define FOG_SOFT_MAX        0.90f

/* Smallest ramp the shader is given. `end <= start` sifira bolme olurdu; son
   deger burada, tek bir yerde duzeltilir. */
#define FOG_MIN_RADIUS      1.0f
#define FOG_MIN_HEIGHT      1.0f

/* CreateFog() ile kurulan belgelenmis varsayilanlar. */
#define FOG_DEF_COLOR       0xFFFFFFFFu            /* Raylib.WHITE */
#define FOG_DEF_DENSITY     0.5
#define FOG_DEF_START       10.0
#define FOG_DEF_END         100.0
#define FOG_DEF_ENABLED     1.0
#define FOG_DEF_HEIGHT      50.0
#define FOG_DEF_FALLOFF     0.25
#define FOG_DEF_ALPHA       1.0
#define FOG_DEF_NOISE       0.0
#define FOG_DEF_MODE        FOG_EXPONENTIAL_SQUARED
#define FOG_DEF_HEIGHT_FOG  0.0

/* ---------- IKI AYRI INDEKS UZAYI — karistirilmasi SISI OLDURUYORDU ----------

   Bu modul iki farkli "argüman listesi" gorur ve bunlar AYNI SEY DEGILDIR:

   1. YUVA INDEKSLERI (S_*) — `SharedPipeline/gcl_native_types.c`teki `f_fog`
      ve `GCL/SimpleRunner/gcl_runner.c`teki `last_fog` ile birebir aynidir.
      `fogy.Update()` / `fogy.Draw()` cagrildiginda runner struct'in butun
      alanlarini BU siraya gore paketler; Read/LastSlot kanali da bu sirayi
      kullanir. OLCULDU: bir tani calistirmasinda

          CreateFog(RED, 11, 22, 33, true, false) + uye atamalari
          -> printf(fogy.Start) = 111  (atama dogru okunuyor)

      yani bu sira DOGRUDUR ve degistirilmemelidir.

   2. ARGÜMAN KONUMLARI (A_*) — yalnizca `CreateFog(color, density, start,
      end, enabled, heightFog)` cagrisinin KENDI parametre sirasidir.

   ILK SURUMDEKI HATA: `CreateFog` bu ikisini karistirdi ve yuva indeksini
   `num_arg()`e verdi. O zaman `start` argv[2]de dururken kod argv[S_START=3]u
   okuyordu; `end` de argv[4] yerine argv[S_END=4]... yani start ile end'in
   yeri takas edildi ve komsu alanlar birer kaydi. Sonuc: `start=45, end=1`.
   `end <= start` oldugu icin rampa hicbir zaman baslamadi ve ekranda sis
   gorunmedi — shader dogru, uniform'lar dogru, yalnizca iki sayi yanlis
   yerdeydi. Bu yuzden iki uzay asagida AYRI adlarla durur; ayni isimle
   cagrilmalari derleyiciye yakalanmaz, o yuzden isimler ayrisir. */
enum {
    S_HANDLE = 0, S_COLOR, S_DENSITY, S_START, S_END, S_MODE,
    S_ENABLED, S_HEIGHT, S_FALLOFF, S_ALPHA, S_NOISE, S_HEIGHT_FOG
};

/* `RaylibFOG.CreateFog(color, density, start, end, enabled, heightFog)` */
enum {
    A_COLOR = 0, A_DENSITY, A_START, A_END, A_ENABLED, A_HEIGHT_FOG
};

/* Script'in yazdigi sira (`fogy.Mode = RaylibFOG.ExponentialSquared;`). */
enum { FOG_LINEAR = 0, FOG_EXPONENTIAL, FOG_EXPONENTIAL_SQUARED };

/* ---------- modul durumu ---------- */

typedef struct {
    int used;
} FogStore;

static FogStore g_fog[FOG_MAX];
static double   g_slot[FOG_SLOT_COUNT] = { -1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                          0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

/* ---------- Raylib.dll: shader uniform'lari ----------

   GL programi ve kamera yuvasi Raylib.dll'de yasar; modul oraya dokunmaz,
   yalnizca "su shader'a su degerleri yaz" der. Iki modul tek bir raylib
   durumunu paylasmak ZORUNDADIR — bu yuzden ayni yerlesim RaylibSimpleLight
   icin de kullaniliyor (bkz. Modules/gcl_SimpleLight.c). */

typedef void (*GclShaderSetFogFn)(int handle, unsigned int color,
                                  float start, float end, float density,
                                  int mode, float strength,
                                  float height_fog, float height,
                                  float falloff);

#ifdef _WIN32
static void *module_symbol(const char *dll, const char *name) {
    HMODULE h = GetModuleHandleA(dll);
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *module_symbol(const char *dll, const char *name) {
    (void)dll; (void)name; return NULL;
}
#endif

static GclShaderSetFogFn shader_set_fog(void) {
    static GclShaderSetFogFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        fn = (GclShaderSetFogFn)module_symbol("Raylib.dll", "gcl_raylib_shader_set_fog");
    }
    return fn;
}

/* ---------- hangi shader? ----------

   Sira ONEMLIDIR: cizilmekte olan program kazanir (Raylib.dll'in cevabi).
   "En son olusturulan" yalnizca yedektir; yanlis programi hedeflemek
   uniform'lari ekranda olmayan bir shader'a yazar ve sahne degismez. */
typedef int (*GclShaderCurrentFn)(void);

static GclShaderCurrentFn light_shader(void) {
    static GclShaderCurrentFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        fn = (GclShaderCurrentFn)module_symbol("Raylib.dll", "gcl_raylib_light_shader");
    }
    return fn;
}

static GclShaderCurrentFn created_shader(void) {
    static GclShaderCurrentFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        fn = (GclShaderCurrentFn)module_symbol("RaylibShader.dll", "gcl_shader_current");
    }
    return fn;
}

static int target_shader(void) {
    GclShaderCurrentFn selected = light_shader();
    int handle = selected ? selected() : -1;
    GclShaderCurrentFn created;
    if (handle >= 0) return handle;
    created = created_shader();
    return created ? created() : -1;
}

/* ---------- slot <-> durum ---------- */

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Gelen YUVA listesini (S_* sirasi) modulun gorunum degerlerine cevirir.
   Eksik yuva belgelenmis varsayilana gelir; boylece hem tam hem kisa cagri
   ayni durumu uretir. Her yuva HER okumada yazilir, yoksa kisa bir cagri
   onceki cagrinin degerlerini tek bir g_slot[] dizisinden miras alirdi.

   TEK ISTISNA `S_COLOR`: rengi burada OKUMAK/ezmek yoktur. Rengin iki kaynagi
   vardir — `CreateFog`un ilk parametresi ve `fogy.Color = ...;` atamasi — ve
   ikisi de g_slot[S_COLOR]a yazar (bkz. fn_create_fog / fn_color). Renk zaten
   dogru yuvada durdugu icin burada varsayilana cevirmek yapilan atamayi
   silmekten baska bir sey yapmaz. */
static void read_slots(int argc, const char **argv, int index) {
    int i;
    if (index < 0 || index >= FOG_MAX) index = 0;
    g_slot[S_HANDLE]     = (double)index;
    g_slot[S_COLOR]      = (argc > S_COLOR)      ? num_arg(argc, argv, S_COLOR)      : (double)FOG_DEF_COLOR;
    g_slot[S_DENSITY]    = (argc > S_DENSITY)    ? num_arg(argc, argv, S_DENSITY)    : FOG_DEF_DENSITY;
    g_slot[S_START]      = (argc > S_START)      ? num_arg(argc, argv, S_START)      : FOG_DEF_START;
    g_slot[S_END]        = (argc > S_END)        ? num_arg(argc, argv, S_END)        : FOG_DEF_END;
    g_slot[S_MODE]       = (argc > S_MODE)       ? num_arg(argc, argv, S_MODE)       : (double)FOG_DEF_MODE;
    /* Enabled / HeightFog ANAHTARDIR, olcek degil: `true` de `1` de ayni
       katmani acar; 0 ya da hic verilmemis kapali demektir. */
    g_slot[S_ENABLED]    = (argc > S_ENABLED)    ? (num_arg(argc, argv, S_ENABLED)    != 0.0 ? 1.0 : 0.0) : FOG_DEF_ENABLED;
    g_slot[S_HEIGHT]     = (argc > S_HEIGHT)     ? num_arg(argc, argv, S_HEIGHT)     : FOG_DEF_HEIGHT;
    g_slot[S_FALLOFF]    = (argc > S_FALLOFF)    ? num_arg(argc, argv, S_FALLOFF)    : FOG_DEF_FALLOFF;
    g_slot[S_ALPHA]      = (argc > S_ALPHA)      ? num_arg(argc, argv, S_ALPHA)      : FOG_DEF_ALPHA;
    g_slot[S_NOISE]      = (argc > S_NOISE)      ? num_arg(argc, argv, S_NOISE)      : FOG_DEF_NOISE;
    g_slot[S_HEIGHT_FOG] = (argc > S_HEIGHT_FOG) ? (num_arg(argc, argv, S_HEIGHT_FOG) != 0.0 ? 1.0 : 0.0) : FOG_DEF_HEIGHT_FOG;

    i = (int)g_slot[S_MODE];
    if (i < FOG_LINEAR || i > FOG_EXPONENTIAL_SQUARED) g_slot[S_MODE] = (double)FOG_DEF_MODE;
    g_slot[S_DENSITY] = clampd(g_slot[S_DENSITY], 0.0, 1.0);
    g_slot[S_ALPHA]   = clampd(g_slot[S_ALPHA],   0.0, 1.0);
    g_slot[S_NOISE]   = clampd(g_slot[S_NOISE],   0.0, 1.0);
    g_slot[S_FALLOFF] = clampd(g_slot[S_FALLOFF], FOG_SOFT_MIN, FOG_SOFT_MAX);
    g_slot[S_START]   = g_slot[S_START] > FOG_MIN_RADIUS ? g_slot[S_START] : FOG_MIN_RADIUS;
    g_slot[S_END]     = g_slot[S_END]   > g_slot[S_START] ? g_slot[S_END] : g_slot[S_START] * 2.0;
    g_slot[S_HEIGHT]  = g_slot[S_HEIGHT] > FOG_MIN_HEIGHT ? g_slot[S_HEIGHT] : FOG_MIN_HEIGHT;
}

/* ---------- modul uyeleri ---------- */

/* Draw() — shader'i besler; EKRANA HICBIR SEY CIZMEZ.
   `fogy.Update(); fogy.Draw();` ikilisi belgelenmis kullanimdir; ikisi de ayni
   isi yapar, tekrar cagrilmasi zararsizdir (idempotent). */
static double fn_draw(int argc, const char **argv) {
    int    index = (int)num_arg(argc, argv, S_HANDLE);
    int    shader, mode;
    double strength;
    GclShaderSetFogFn set_fog = shader_set_fog();

    read_slots(argc, argv, index);
    if (index < 0 || index >= FOG_MAX) index = 0;
    g_fog[index].used = 1;

    /* Kapatmak icin sahneyi yeniden kurmak GEREKMEZ: guc 0 gonderilir ve
       shader karisim oranini 0'la carpar. */
    if (g_slot[S_ENABLED] == 0.0) {
        strength = 0.0;
    } else {
        /* Alpha son bir kisistir; Density artik egriyi surer, bu yuzden onu
           burada da carpmak etkiyi iki kez kisardi. */
        strength = g_slot[S_ALPHA];
    }

    shader = target_shader();
    if (shader < 0 || !set_fog) return 0.0;   /* shader yok: yazacak yer yok */

    mode = (int)g_slot[S_MODE];
    set_fog(shader, (unsigned int)(long long)g_slot[S_COLOR],
            (float)g_slot[S_START], (float)g_slot[S_END], (float)g_slot[S_DENSITY],
            mode, (float)strength,
            (float)g_slot[S_HEIGHT_FOG], (float)g_slot[S_HEIGHT],
            (float)g_slot[S_FALLOFF]);
    return 0.0;
}

/* Update() — Draw() ile ayni: uniform'lari yazar, cizmez. Ayri durmasinin
   sebebi belgelenmis cagri ciftini desteklemek. */
static double fn_update(int argc, const char **argv) {
    return fn_draw(argc, argv);
}

/* CreateFog(color, density, start, end, enabled, heightFog) -> kayit indeksi
   (script bunu FOG.Handle'da tutar). Her cagri YENI bir yuva alir; boylece
   ayni anda birden fazla sis (ornegin bir gecis efekti) kurulabilir.

   Argümanlar sirayla 1, 2, 3, 4, 6 ve 11. yuvalara oturur; kalan yuvalar
   belgelenmis varsayilanlarini alir (eksik argümani read_slots() tamamlar). */
static double fn_create_fog(int argc, const char **argv) {
    for (int i = 0; i < FOG_MAX; i++) {
        if (g_fog[i].used) continue;
        memset(&g_fog[i], 0, sizeof(g_fog[i]));
        g_fog[i].used = 1;

        /* DIKKAT: burada A_* konumlari okunur, S_* DEGIL. Argümanlar cagrinin
           KENDI parametre sirasina gore gelir (renk ilk sirada); yuva
           indeksleri baska bir seydir ve yukaridaki uzay notunda aciklanir.
           Ilk surumde S_* kullanildigi icin her alan bir yuva kaymisti. */
        g_slot[S_HANDLE]     = (double)i;
        g_slot[S_COLOR]      = (argc > A_COLOR)      ? num_arg(argc, argv, A_COLOR)      : (double)FOG_DEF_COLOR;
        g_slot[S_DENSITY]    = (argc > A_DENSITY)    ? num_arg(argc, argv, A_DENSITY)    : FOG_DEF_DENSITY;
        g_slot[S_START]      = (argc > A_START)      ? num_arg(argc, argv, A_START)      : FOG_DEF_START;
        g_slot[S_END]        = (argc > A_END)        ? num_arg(argc, argv, A_END)        : FOG_DEF_END;
        g_slot[S_ENABLED]    = (argc > A_ENABLED)    ? (num_arg(argc, argv, A_ENABLED)    != 0.0 ? 1.0 : 0.0) : FOG_DEF_ENABLED;
        g_slot[S_HEIGHT_FOG] = (argc > A_HEIGHT_FOG) ? (num_arg(argc, argv, A_HEIGHT_FOG) != 0.0 ? 1.0 : 0.0) : FOG_DEF_HEIGHT_FOG;
        g_slot[S_MODE]       = (double)FOG_DEF_MODE;
        g_slot[S_HEIGHT]     = FOG_DEF_HEIGHT;
        g_slot[S_FALLOFF]    = FOG_DEF_FALLOFF;
        g_slot[S_ALPHA]      = FOG_DEF_ALPHA;
        g_slot[S_NOISE]      = FOG_DEF_NOISE;
        return (double)i;
    }
    g_slot[S_HANDLE] = -1.0;
    return -1.0;
}

/* Alan erisimcileri — HEM OKUR HEM YAZAR.

   Runner bir struct degiskeninin alanlarini bu uyelerden doldurur
   (g_native_last_maps[] / store_slots_into_var) ve

       fogy.Start = 111.0f;

   yazildiginda degeri AYNI uyeye argüman olarak geri gonderir. Bu yuzden her
   erisimci iki yonludur:

       argumansiz  -> degeri dondur (okuma)
       argümanli   -> degeri sakla ve dondur (yazma)

   Ilk surumde hepsi YALNIZCA okuyordu ve sonuc su sessiz hataydi: script'in
   `fogy.End = 45.0f;` atamasi modulu HIC gormuyor, modul CreateFog'un ilk
   degerleriyle ciziyordu. Ne hata cikar ne sahne degisir — en zor teshis
   edilen tur. Yazarken de ayni olcek uygulanir; boylece script'in OKUDUGU
   deger ile modulun KULLANDIGI deger ayrisamaz. */
static double slot_get(int slot) { return g_slot[slot]; }

static double slot_set(int slot, int argc, const char **argv) {
    if (argc > 0 && argv && argv[0]) g_slot[slot] = atof(argv[0]);
    return g_slot[slot];
}
static double slot_set_switch(int slot, int argc, const char **argv) {
    if (argc > 0 && argv && argv[0]) g_slot[slot] = (atof(argv[0]) != 0.0) ? 1.0 : 0.0;
    return g_slot[slot];
}
static double slot_set_unit(int slot, int argc, const char **argv) {
    if (argc > 0 && argv && argv[0]) g_slot[slot] = clampd(atof(argv[0]), 0.0, 1.0);
    return g_slot[slot];
}
static double slot_set_mode(int slot, int argc, const char **argv) {
    if (argc > 0 && argv && argv[0]) {
        double v = atof(argv[0]);
        if (v >= FOG_LINEAR && v <= FOG_EXPONENTIAL_SQUARED) g_slot[slot] = v;
    }
    return g_slot[slot];
}

#define FOG_FIELD(FN, SLOT, SETTER)                          \
    static double FN(int argc, const char **argv) {          \
        return SETTER(SLOT, argc, argv);                      \
    }

/* `Handle` bir KIMLIKTIR: CreateFog'un dondurdugu kayit indeksi. Bir script'in
   onu elle degistirmesi baska bir sisin yuvasini hedeflemesi olurdu, bu yuzden
   tek YAZILAMAZ alandir; degeri yalnizca CreateFog kurar. */
static double fn_handle(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_HANDLE];
}

FOG_FIELD(fn_color,      S_COLOR,      slot_set)
FOG_FIELD(fn_density,    S_DENSITY,    slot_set_unit)
FOG_FIELD(fn_start,      S_START,      slot_set)
FOG_FIELD(fn_end,        S_END,        slot_set)
FOG_FIELD(fn_mode,       S_MODE,       slot_set_mode)
FOG_FIELD(fn_enabled,    S_ENABLED,    slot_set_switch)
FOG_FIELD(fn_height,     S_HEIGHT,     slot_set)
FOG_FIELD(fn_falloff,    S_FALLOFF,    slot_set)
FOG_FIELD(fn_alpha,      S_ALPHA,      slot_set_unit)
FOG_FIELD(fn_noise,      S_NOISE,      slot_set_unit)
FOG_FIELD(fn_height_fog, S_HEIGHT_FOG, slot_set_switch)
#undef FOG_FIELD

/* LastSlot(): argumansiz -> yuva sayisi; LastSlot(i) -> i. yuva. Struct metot
   cagrilarindan sonra (fogy.Update(), fogy.Draw()) runner degiskenin
   alanlarini bu kanaldan tazeler. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)FOG_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= FOG_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* Mod sabitleri: `fogy.Mode = RaylibFOG.ExponentialSquared;` bir acilir menu
   gibi okunsun. Degeri dogrudan donduren siradan uyelerdir, yani raylib'in
   renk sabitleriyle AYNI yoldan cozulur — ayri bir mekanizma yok. */
static double fn_const_linear(int argc, const char **argv) { (void)argc; (void)argv; return (double)FOG_LINEAR; }
static double fn_const_exp(int argc, const char **argv)    { (void)argc; (void)argv; return (double)FOG_EXPONENTIAL; }
static double fn_const_exp2(int argc, const char **argv)   { (void)argc; (void)argv; return (double)FOG_EXPONENTIAL_SQUARED; }

/* GCL scriptleri uyeleri modulun belgeledigi adlarla cagirir
   (RaylibFOG.CreateFog, fogy.Draw, ...); bu tablo script'e bakan yazimi C
   fonksiyon adindan bagimsiz tutar. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_fog,   "CreateFog"),
    E(update,       "Update"),
    E(draw,         "Draw"),
    E(handle,       "Handle"),
    E(color,        "Color"),
    E(density,      "Density"),
    E(start,        "Start"),
    E(end,          "End"),
    E(mode,         "Mode"),
    E(enabled,      "Enabled"),
    E(height,       "Height"),
    E(falloff,      "Falloff"),
    E(alpha,        "Alpha"),
    E(noise,        "Noise"),
    E(height_fog,   "HeightFog"),
    E(last_slot,    "LastSlot"),
    E(const_linear, "Linear"),
    E(const_exp,    "Exponential"),
    E(const_exp2,   "ExponentialSquared"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibfog_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
