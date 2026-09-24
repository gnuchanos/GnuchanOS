#version 330

// ---------------------------------------------------------------------------
// lighting.fs — arazi/kutu fragment asamasi
//
// Iki bagimsiz is yapar ve ikisi de AYNI fragment icinde, pikselin kendi
// verisiyle hesaplanir:
//
//   1. ISIK  — yonlu/nokta isiklarinin yayilmasi + parlama + ortam dolgusu
//   2. SIS   — kameradan olculen GERCEK mesafeye gore renk karisimi
//
// SIS NEDEN BURADA: RaylibFOG modulu hicbir sey cizmez, yalnizca asagidaki
// `fogColor` / `fogParams` / `fogShape` uniform'larini doldurur. Karisim
// burada olur. Alternatif (gokyuzune opak bir duvar cizip derinlik yazmak)
// denenmis ve YANLISTIR: cizim sirasi geregi gokyuzunu kapatir ve opak arazi
// sisi ezer. Burada ise:
//
//   * GOKYUZU BASKA BIR PROGRAMDIR, `fogColor` oraya hic ulasmaz;
//   * oran her pikselin gercek uzakliginin fonksiyonudur, yani gecis duzgun
//     bir GRADYANDIR — halka yok, silindir yok, arkasindan bakilan siluet yok.
//
// RENK UZAYI: doku sRGB olarak saklanir (PNG'deki sayilar ZATEN gama
// kodludur). Isik hesabi lineer uzayda yapilmali, yoksa gama IKI KEZ uygulanir
// ve her renk yikanir. Bu yuzden: co -> lineer, hesapla -> sRGB'ye kodla.
// ---------------------------------------------------------------------------

// Giris nitelikleri (vertex shader'dan)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Giris uniform'lari
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Cikis rengi
out vec4 finalColor;

// ---------------------------------------------------------------------------
// Isik
// ---------------------------------------------------------------------------
#define MAX_LIGHTS              5
#define LIGHT_DIRECTIONAL       0
#define LIGHT_POINT             1

// Parlama keskinligi. Buyudukce vurgu kuculur ve sertlesir.
#define SPECULAR_SHININESS      48.0

// Noktasal isik sonum katsayilari: `atten = 1/(1 + k1*d + k2*d*d)`.
//
// NEDEN SART: noktasal isik, sonum olmadan butun dunyayi ayni parlaklikta
// aydinlatir. Uzaklik duygusu olmadigi icin aydinlattigi nesne "isikla
// aydinlatilmis" degil, KENDI KENDINE PARLAYAN bir malzeme gibi gorunur -
// arizilan sikayet tam buydu: "hafif parlayan materialden yapilmis bir kup".
#define POINT_ATTEN_K1          0.09
#define POINT_ATTEN_K2          0.032

// Isigin etki yaricapi (metre). Otesinde katki kirpilir; kirpma, isik
// havuzunun kenarindaki sayisal gurultuyu onler.
#define POINT_RANGE             18.0

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

// ---------------------------------------------------------------------------
// Sis — RaylibFOG tarafindan doldurulur
//
//   fogParams.x  baslangic mesafesi (sis burada baslar)
//   fogParams.y  bitis mesafesi     (sis burada TAMdir)
//   fogParams.z  yogunluk           (egrinin siddeti)
//   fogParams.w  mod: 0 lineer, 1 ustel, 2 ustel-kare
//   fogShape.x   ana siddet, 0 = sis kapali (yeniden kurmaya gerek yok)
//   fogShape.y   1 = yukseklik sisi acik
//   fogShape.z   yukseklik tavani (metre)
//   fogShape.w   yukseklik gecisinin yumusakligi
// ---------------------------------------------------------------------------
uniform vec3 fogColor;
uniform vec4 fogParams;
uniform vec4 fogShape;

#define FOG_LINEAR              0.0
#define FOG_EXPONENTIAL         1.0
#define FOG_EXPONENTIAL_SQUARED 2.0

// Ustel egrinin ne kadar hizli kapandigi. Bu sayilar "ne kadar sisli
// gorunuyor" sorusunun cevabidir; Start/End bandin YERINI belirler.
#define FOG_EXP_GAIN            2.0
#define FOG_EXPSQ_GAIN          3.0

// ---------------------------------------------------------------------------
// Yardimcilar
// ---------------------------------------------------------------------------

// Doku sRGB kodludur; isik hesabi lineer uzayda yapilmali.
vec3 srgb_to_linear(vec3 c) {
    return pow(c, vec3(2.2));
}

// Ekrana yazmadan once tekrar sRGB'ye kodla. Beyaz isik + beyaz ton ile bu
// islem okunan texel'in AYNISINI geri verir, yani doku rengi korunur.
vec3 linear_to_srgb(vec3 c) {
    return pow(c, vec3(1.0/2.2));
}

// Tek bir isigin yayilma ve parlama katkisi. `lightDot` ve `specular`
// biriktiricilerine EKLER (ilk degerleri cagiran verir).
void accumulate_light(int index, vec3 normal, vec3 viewDir,
                      inout vec3 lightDot, inout vec3 specular) {
    if (lights[index].enabled != 1) return;

    vec3  lightDir = vec3(0.0);
    /* Noktasal isikta mesafe sonumu; yonlu isikte 1 (gunes uzaklasmaz). */
    float atten    = 1.0;

    if (lights[index].type == LIGHT_DIRECTIONAL) {
        lightDir = -normalize(lights[index].target - lights[index].position);
    } else if (lights[index].type == LIGHT_POINT) {
        vec3  toLight = lights[index].position - fragPosition;
        float dist    = length(toLight);
        lightDir      = normalize(toLight);

        atten = 1.0/(1.0 + POINT_ATTEN_K1*dist + POINT_ATTEN_K2*dist*dist);

        /* Etki yaricapinin disinda katki yok. Yumusak inis: sert bir esik
           isik havuzunun kenarinda gorunur bir HALKA cizerdi, oysa gercek
           isikta sonum zaten asimptotiktir. */
        atten *= 1.0 - smoothstep(POINT_RANGE*0.75, POINT_RANGE, dist);
    }

    float NdotL = max(dot(normal, lightDir), 0.0);
    lightDot += lights[index].color.rgb*(NdotL*atten);

    // Parlama isigin KENDI rengini tasir. Rengi `tint`e duz sayi olarak
    // eklemek (raylib'in stok ornegi boyle yapar) uc kanali esit yukseltir ve
    // dokuyu agartir; bu yuzden ayri bir vektorde biriktirilir.
    if (NdotL > 0.0) {
        float specCo = pow(max(0.0, dot(viewDir, reflect(-lightDir, normal))),
                           SPECULAR_SHININESS);
        specular += lights[index].color.rgb*(specCo*atten);
    }
}

// Kamera ile bu fragment arasindaki GERCEK mesafeyi olcer. Sis, dunya uzayinda
// yapilan bu olcume dayanir; nesne otelense de sis onunla hareket etmez.
float eye_distance() {
    return length(viewPos - fragPosition);
}

// Mesafeyi sis bandindaki konuma cevirir: `Start`ta 0, `End`te 1.
//
// Start/End HER modda ASIL kontroldur; mod yalnizca bandin ICINDEKI egrinin
// seklini secer. Boylece Start/End'i oynatmak, secili mod ne olursa olsun
// sisi ONGORULEBILIR yonde hareket ettirir — ham bir ustel katsayida bu dogru
// degildir, cunku mesafe once olceklenmeden egri anlamsizdir.
float fog_band_position() {
    float span = max(fogParams.y - fogParams.x, 0.001);
    return clamp((eye_distance() - fogParams.x)/span, 0.0, 1.0);
}

// Bandin icindeki egri. `t` = 0 yakin (sis yok), 1 uzak (sis tam).
float fog_curve(float t) {
    float mode = fogParams.w;

    if (mode > FOG_EXPONENTIAL) {
        // ustel-kare: basta yavas, sonra sert kapanir. Silent Hill'in duz gri
        // bir tabaka degil de "derinlik" gibi okunmasini saglayan egri budur.
        return 1.0 - exp(-FOG_EXPSQ_GAIN*t*t);
    }
    if (mode > FOG_LINEAR) {
        // ustel: daha yumusak, daha esit bir pus
        return 1.0 - exp(-FOG_EXP_GAIN*t);
    }
    // lineer: duz rampa; uzak kenar yumusak kalsin diye bir kez karelenir
    return t*t;
}

// Yukseklik sisi: pus `height`in ALTINDA oturur ve son `falloff`lik dilimde
// incelir. CARPILIR, eklenmez — eklenirse yogun bir vadi butun haritanin
// uzerinde opak bir tavana donusur.
float height_fog_attenuation() {
    if (fogShape.y <= 0.5) return 1.0;

    float softness = clamp(fogShape.w, 0.0, 0.95);
    float band = max(fogShape.z*(1.0 - softness), 0.001);
    return 1.0 - smoothstep(fogShape.z - band, fogShape.z, fragPosition.y);
}

// Nihai sis orani (0 = arazi kendi renginde, 1 = tamamen sis rengi).
float fog_amount() {
    float amount = fog_curve(fog_band_position());

    // Yogunluk butun egriyi olcekler.
    amount *= clamp(fogParams.z*2.0, 0.0, 1.0);

    amount *= height_fog_attenuation();

    // Ana siddet HER SEYI carpar: 0 verildiginde arazi kendi rengini geri alir
    // ve hicbir sey yeniden kurulmaz.
    return clamp(amount*fogShape.x, 0.0, 1.0);
}

// ---------------------------------------------------------------------------

void main()
{
    // Doku sRGB'den lineer uzaya. Isik hesabi bu uzayda yapilir.
    vec4 texelColor  = texture(texture0, fragTexCoord);
    vec3 texelLinear = srgb_to_linear(texelColor.rgb);

    vec3 normal  = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec4 tint    = colDiffuse*fragColor;

    // --- Isik -------------------------------------------------------------
    vec3 lightDot = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        accumulate_light(i, normal, viewDir, lightDot, specular);
    }

    // Yayilma + parlama, ikisi de lineer uzayda.
    vec3 lit = texelLinear*(lightDot + specular);

    // Ortam dolgusu. `ambient` son siddetini ZATEN tasir: isik modulu gunes
    // rengini SUN_AMBIENT_NIGHT..SUN_AMBIENT_DAY araliginda olcekler
    // (bkz. Modules/gcl_SimpleLight.c) ve dogrudan terimi SUN_DIRECT_MAX ile
    // sinirlar, boylece dogrudan + ortam <= 1.0 kalir. Burada TEKRAR bir
    // carpan uygulamak ayni kisitlamayi IKI KEZ uygular: dolgu gokyuzu
    // renginin %1-5'ine duser ve gunesin vurmadigi her yuz — sabah/aksam
    // sahnesinin tamami — parlak gokyuzunun altinda SIYAH cikar.
    // YARIM-KURE (hemisphere) ORTAM ISIGI.
    //
    // Tek bir ambient rengi her yuzu AYNI yukseltir ve goruntu duzlesir: magara
    // duvari ile acik arazinin yuzu ayirt edilemez ("magaralarin ici disi ile
    // ayni dumduz"). Gercek sacilma GOKTEN gelir: yukari bakan yuz gokyuzunu
    // gorur, asagi bakan yuz zeminden sekme alir.
    //
    // Gokyuzu rengi = modulden gelen ambient; zemin sekmesi onun koyu bir
    // kesri. Yeni bir uniform GEREKMEZ ve mevcut denge korunur; yalnizca YONE
    // bagli degisim eklenir: tavan/taban koyulasir, ust yuzler acilir.
    //
    // `normal.y` = 1 (yukari) -> skyAmb; = -1 (asagi) -> groundAmb.
    vec3  skyAmb    = ambient.rgb;
    vec3  groundAmb = ambient.rgb*0.35;
    float hemi      = clamp(0.5 + 0.5*normal.y, 0.0, 1.0);

    lit += texelLinear*mix(groundAmb, skyAmb, hemi);

    // Ekran icin sRGB'ye geri kodla.
    vec3 shaded = linear_to_srgb(lit*vec3(tint.r, tint.g, tint.b));

    // --- Sis --------------------------------------------------------------
    float fogAmt = fog_amount();

    finalColor = vec4(mix(shaded, fogColor, fogAmt), texelColor.a*tint.a);
}
