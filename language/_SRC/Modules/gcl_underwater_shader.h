/* gcl_underwater_shader.h — SUYUN ICINDEN BAKIS.

   NE YAPAR: BeginMode3D kapandiktan sonra, ekran uzayinda calisan tek bir
   fragment asamasi. Test ettigi soru TEKTIR:

       "Bu pikselin isini GERCEK su hacmine carpiyor mu?"

   Cevap EVET ise piksel su ile boyanir; HAYIR ise ATILIR (discard) ve orada
   zaten cizilmis olan sahne - gokyuzu, arazi, su yuzeyi - aynen kalir.

   SU CIZGISI UYDURULMAZ, IZDUSURULUR.

   Onceki surum, ekranin ust sinirini ekran koordinatinin sin()'iyle
   salliyordu. Bu YANLISTI: uydurma bir dalga, 3B denizde gorunen dalgayla
   hicbir iliski tasimaz. Kamerayi cevirdiginde cizgi kipirdamaz, suyun
   gercek sirtlariyla hizalanmaz; kullanici "bu benim baktigim dalga degil"
   der, cunku degildir.

   Burada cizgi GERCEK YUZEYIN KENDISIDIR:
     1. Pikselden bir isin atilir (kamera tabani + gl_FragCoord).
     2. Isin, 3B yuzey shader'inin KULLANDIGI AYNI dalga alaniyla kesistirilir
        (gclUwWave + uwAmp + uwSurfaceY). Ayni formul, ayni saat, ayni faz.
     3. Vurus noktasi su hacminin [uwBoxMin, uwBoxMax] icinde degilse isin
        suyu iskalayip gokyuzune kacmistir -> piksel ATILIR.

   Boylece ekrandaki su siniri, 3B mesh'in kameradan gecirilmis izdusumudur;
   yukaridan baktigin sirt ile asagidan baktigin sinir AYNI sudur.

   ASAGIDAN GORUNUS YUKARIDAN GORUNUSE BENZEMEZ. Isin yuzeyin ALT yuzune
   carptiginda iki sey olur:
     * Kritik aci (48.6 derece) altinda ise isin suyu terk eder -> SNELL
       PENCERESI: gokyuzunun kendisi (gclSkyColor) pencereden gorunur.
     * Kritik acinin uzerinde ise TOPLAM IC YANSIMA olur; yuzey ayna gibi
       davranir ve isin suyun icinde kalir.
   Normaller dalga gradyanindan geldigi icin pencerenin KENARI da gercek
   dalgayla titrer.

   GOKYUZU NEREDEN: pencereden gorunen gok, gökcübbenin TA KENDISIDIR.
   GCL_SKY_FULL buraya dahil edilir ve modul ayni degerleri yazar (bkz.
   gcl_SimpleWater.c: uw_push_sky). Ayri bir gradyan kullanilsaydi pencereden
   "baska bir gokyuzu" gorunurdu.

   *** MACRO GOVDE KURALI - DUZENLEMEDEN ONCE OKU ***
   Asagidaki makrolar ardisik C string literallerine acilir. Boyle bir govde
   YALNIZCA string literali ve satir devami icerebilir; cok satirli bir C
   yorumu ICEREMEZ (satir birlestirme yorumlar silinmeden ONCE olur). Metnin
   tamami makrolarin DISINDA tutulur.

   PORTABILITY: tek kaynak GL 3.3 / GL 2.1 / GLES2-3 besler. `__VERSION__`
   surucuden gelir; `precision` yalnizca GL_ES altinda yazilir. `#version`
   satirini CAGIRAN ekler (bkz. gcl_SimpleWater.c: build_shader). */

#ifndef GCL_UNDERWATER_SHADER_H
#define GCL_UNDERWATER_SHADER_H

#include "gcl_sky_shared.h"

/* Masaustu varsayilani; script Raylib.GLSL_VERSION ile degistirebilir. */
#define UNDERWATER_DEFAULT_GLSL 330

/* ---------- UNIFORM ADLARI ----------
   Modulun GetShaderLocation'a verdigi dizelerle BIREBIR ayni olmalidir. */
#define UW_U_RES      "uwRes"
#define UW_U_TIME     "uwTime"
#define UW_U_CAMPOS   "uwCamPos"
#define UW_U_CAMFWD   "uwCamFwd"
#define UW_U_CAMRIGHT "uwCamRight"
#define UW_U_CAMUP    "uwCamUp"
#define UW_U_TANHALF  "uwTanHalf"
#define UW_U_ASPECT   "uwAspect"
#define UW_U_SURFACEY "uwSurfaceY"
#define UW_U_BOXMIN   "uwBoxMin"
#define UW_U_BOXMAX   "uwBoxMax"
#define UW_U_AMP      "uwAmp"
#define UW_U_FREQ     "uwFreq"
#define UW_U_SPEED    "uwSpeed"
#define UW_U_SHALLOW  "uwShallow"
#define UW_U_DEEP     "uwDeep"
#define UW_U_SUBMERGE "uwSubmerge"

/* VERTEX ASAMASI.

   Dolgu, raylib'in DrawRectangle cizimidir: mvp + vertexPosition yeterlidir.
   Ekran konumu (her piksel icin) fragment asamasinda gl_FragCoord'tan okunur;
   boylece vertexTexCoord'a hic ihtiyac kalmaz ve cizim cagrisi degismez. */
#define UNDERWATER_VERTEX_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_ATTR in\n" \
"    #define GCL_VOUT out\n" \
"#else\n" \
"    #define GCL_ATTR attribute\n" \
"    #define GCL_VOUT varying\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision highp float;\n" \
"#endif\n" \
"GCL_ATTR vec3 vertexPosition;\n" \
"GCL_ATTR vec2 vertexTexCoord;\n" \
"GCL_ATTR vec4 vertexColor;\n" \
"uniform mat4 mvp;\n" \
"GCL_VOUT vec2 fragTexCoord;\n" \
"GCL_VOUT vec4 fragColor;\n" \
"void main(void)\n" \
"{\n" \
"    fragTexCoord = vertexTexCoord;\n" \
"    fragColor    = vertexColor;\n" \
"    gl_Position  = mvp*vec4(vertexPosition, 1.0);\n" \
"}\n"

/* FRAGMENT ASAMASI.

   ISIN-YUZEY KESISIMI. Yuzey y = uwSurfaceY + h(x,z)*uwAmp oldugu icin
   kesisme zamani t = (uwSurfaceY + h - camY)/dir.y'dir; h ise t'ye baglidir.
   Bu yuzden 4 kez ust uste cozulur (sabit nokta yinelemesi): ilk tahmin duz
   duzlemden gelir, sonraki turlar dalgayi hesaba katar. Egrilik kucuk
   oldugu icin 4 tur fazlasiyla yeterlidir.

   ISARET KURALLARI (ikisi de ters yazilirsa goruntu sessizce bozulur):
     * cosI = dot(dir, n). Isin de normal de YUKARI bakar; dikeye yakin
       bakista cosI buyur ve pencere ORADA acilir. Basina eksi koymak onu her
       karede 0'a kilitler ve pencere hicbir zaman acilmaz.
     * refract(dir, -n, 1.3333). GLSL `refract` normali isiga KARSI bekler
       (N.I < 0), bu yuzden normal TERS cevrilir. Eta, GELEN ortamin indisi
       bolu GECEN ortamin indisidir: su(1.3333)/hava(1.0). Ters yazilirsa
       (0.75) toplam ic yansima hic tetiklenmez ve isin asagi kirilir.

   SEFFAFLIK: dolgu sahneyi GIZLEMEZ, BOYAR. Alfa suya girme derinligiyle
   0.34'ten 0.52'ye cikar; oyuncu onunu gormeye devam eder. */
#define UNDERWATER_FRAGMENT_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_FRAG gclFragColor\n" \
"    #define GCL_VIN in\n" \
"    out vec4 gclFragColor;\n" \
"#else\n" \
"    #define GCL_FRAG gl_FragColor\n" \
"    #define GCL_VIN varying\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision highp float;\n" \
"#endif\n" \
"GCL_VIN vec2 fragTexCoord;\n" \
"GCL_VIN vec4 fragColor;\n" \
"uniform vec2  uwRes;\n" \
"uniform float uwTime;\n" \
"uniform vec3  uwCamPos;\n" \
"uniform vec3  uwCamFwd;\n" \
"uniform vec3  uwCamRight;\n" \
"uniform vec3  uwCamUp;\n" \
"uniform float uwTanHalf;\n" \
"uniform float uwAspect;\n" \
"uniform float uwSurfaceY;\n" \
"uniform vec3  uwBoxMin;\n" \
"uniform vec3  uwBoxMax;\n" \
"uniform float uwAmp;\n" \
"uniform float uwFreq;\n" \
"uniform float uwSpeed;\n" \
"uniform vec3  uwShallow;\n" \
"uniform vec3  uwDeep;\n" \
"uniform float uwSubmerge;\n" \
GCL_SKY_FULL \
"void gclUwWave(vec2 p, float t, out float h, out vec2 g)\n" \
"{\n" \
"    vec2 d1 = normalize(vec2( 1.00,  0.28));\n" \
"    vec2 d2 = normalize(vec2(-0.36,  1.00));\n" \
"    vec2 d3 = normalize(vec2( 0.72,  0.72));\n" \
"    float f1 = 1.00*uwFreq, f2 = 1.73*uwFreq, f3 = 2.41*uwFreq;\n" \
"    float s1 = 0.90*uwSpeed, s2 = 1.27*uwSpeed, s3 = 1.61*uwSpeed;\n" \
"    float p1 = dot(p, d1)*f1 + t*s1;\n" \
"    float p2 = dot(p, d2)*f2 + t*s2;\n" \
"    float p3 = dot(p, d3)*f3 + t*s3;\n" \
"    h = 1.00*sin(p1) + 0.62*sin(p2) + 0.34*sin(p3);\n" \
"    g = 1.00*cos(p1)*f1*d1 + 0.62*cos(p2)*f2*d2 + 0.34*cos(p3)*f3*d3;\n" \
"}\n" \
"void main(void)\n" \
"{\n" \
"    vec2  uv  = gl_FragCoord.xy/max(uwRes, vec2(1.0));\n" \
"    vec2  ndc = uv*2.0 - 1.0;\n" \
"    vec3  dir = normalize(uwCamFwd\n" \
"                        + uwCamRight*(ndc.x*uwAspect*uwTanHalf)\n" \
"                        + uwCamUp   *(ndc.y*uwTanHalf));\n" \
"    float sub = clamp(uwSubmerge, 0.0, 1.0);\n" \
"    float h = 0.0;\n" \
"    vec2  g = vec2(0.0);\n" \
"    bool  camIn = (uwCamPos.x > uwBoxMin.x) && (uwCamPos.x < uwBoxMax.x)\n" \
"               && (uwCamPos.z > uwBoxMin.z) && (uwCamPos.z < uwBoxMax.z)\n" \
"               && (uwCamPos.y < uwSurfaceY);\n" \
"    bool  isWater = false;\n" \
"    bool  hitTop  = false;\n" \
"    if (dir.y > 0.0005)\n" \
"    {\n" \
"        float t = (uwSurfaceY - uwCamPos.y)/dir.y;\n" \
"        for (int i = 0; i < 4; i++)\n" \
"        {\n" \
"            vec3 q = uwCamPos + dir*max(t, 0.0);\n" \
"            gclUwWave(q.xz, uwTime, h, g);\n" \
"            t = (uwSurfaceY + h*uwAmp - uwCamPos.y)/dir.y;\n" \
"        }\n" \
"        vec3 q = uwCamPos + dir*max(t, 0.0);\n" \
"        bool hitXZ = (q.x > uwBoxMin.x) && (q.x < uwBoxMax.x)\n" \
"                  && (q.z > uwBoxMin.z) && (q.z < uwBoxMax.z);\n" \
"        if (hitXZ && t > 0.0) { isWater = true; hitTop = true; }\n" \
"        else if (camIn)       { isWater = true; }\n" \
"    }\n" \
"    else\n" \
"    {\n" \
"        isWater = camIn;\n" \
"    }\n" \
"    if (!isWater) discard;\n" \
"    float vert = clamp((ndc.y + 1.0)*0.5, 0.0, 1.0);\n" \
"    vec3 body = mix(uwDeep, uwShallow, vert*0.80);\n" \
"    float shaft = 0.5 + 0.5*sin(ndc.x*3.30 + uwTime*0.22\n" \
"                              + sin(ndc.x*0.90 + uwTime*0.11)*1.6);\n" \
"    shaft = shaft*shaft*shaft;\n" \
"    body += sunColor*shaft*exp(-(1.0 - vert)*2.6)*0.28;\n" \
"    vec3 col = body;\n" \
"    if (hitTop)\n" \
"    {\n" \
"        vec3  n    = normalize(vec3(-g.x*uwAmp, 1.0, -g.y*uwAmp));\n" \
"        float cosI = clamp(dot(dir, n), 0.0, 1.0);\n" \
"        const float COS_CRIT = 0.6614;\n" \
"        vec3  refr = refract(dir, -n, 1.3333);\n" \
"        float win  = smoothstep(COS_CRIT - 0.10, COS_CRIT + 0.02, cosI);\n" \
"        if (dot(refr, refr) < 0.001) refr = reflect(dir, n);\n" \
"        vec3  skyIn  = gclSkyColor(vec3(refr.x, max(refr.y, 0.02), refr.z));\n" \
"        vec3  refl   = reflect(dir, n);\n" \
"        vec3  mirror = gclSkyColor(vec3(refl.x, max(refl.y, 0.06), refl.z));\n" \
"        vec3  tirCol = mix(uwDeep*0.55, mirror*0.42, 0.55);\n" \
"        col = mix(tirCol, skyIn*1.08, win);\n" \
"        float rim = exp(-abs(cosI - COS_CRIT)*16.0);\n" \
"        col += sunColor*rim*(1.0 - win)*0.30;\n" \
"    }\n" \
"    float a = clamp(0.34 + 0.18*sub, 0.0, 1.0);\n" \
"    GCL_FRAG = vec4(col, a);\n" \
"}\n"

#endif /* GCL_UNDERWATER_SHADER_H */
