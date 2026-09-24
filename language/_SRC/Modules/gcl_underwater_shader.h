/* gcl_underwater_shader.h — SU ALTI EKRAN EFEKTI.

   TASARIM (tek cumle): SUYUN ALTINDA blur + mavi + saydam; SUYUN USTUNDE
   hicbir sey. Suyun ustundeki goruntu zaten 3B su yuzeyinin kendi
   shader'indan gelir (bkz. gcl_water_shader.h: dalga + gokyuzu yansimasi);
   bu dosya o goruntuye YALNIZCA kamera suya girdiginde dokunur.

   NE YAPAR:

     1. KAPLAMA (PIKSEL BASINA) — kameradan atilan isin, dalga yuzeyinin
        ALTINA iniyor mu? Iniyorsa piksel su altidir; inmiyorsa kuru kalir.
        Yarim batmisken ekranin bir kismi su altinda, gerisi normal olur.

     2. HAFIF BLUR — 9 ornekli, kucuk yaricapli yumusatma.

     3. MAVI TON — sahne acik maviye cekilir. KARARTMAZ, tonlar.

     4. SAYDAMLIK — alfa = suya giris siddeti. Kuru pikselde alfa 0'dir,
        yani orada sahne AYNEN kalir.

   DALGA ALANI: 3B yuzeyle BIREBIR ayni formul (gcl_water_shader.h:
   gclWaterWave). Ayni yonler, ayni frekans katlari, ayni faz. Ayri bir alan
   kullanan bir sinir, ekrandaki dalgayla hizasiz kalirdi.

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

/* Masaustu varsayilani; script Raylib.GLSL_VERSION ile degistirebilir. */
#define UNDERWATER_DEFAULT_GLSL 330

/* ---------- UNIFORM ADLARI ----------
   Modulun GetShaderLocation'a verdigi dizelerle BIREBIR ayni olmalidir.

   `texture0` RAYLIB'IN KENDI ADIDIR: doku cizen bir dortgen (DrawTexturePro)
   cizildiginde partinin doku birimi otomatik olarak doku 0'a baglanir ve
   shader'a `texture0` adiyla verilir. */
#define UW_U_SCENE    "texture0"
#define UW_U_RES      "uwRes"
#define UW_U_TIME     "uwTime"
#define UW_U_SHALLOW  "uwShallow"
#define UW_U_DEEP     "uwDeep"
#define UW_U_SUBMERGE "uwSubmerge"
#define UW_U_CAMPOS   "uwCamPos"
#define UW_U_CAMFWD   "uwCamFwd"
#define UW_U_CAMRIGHT "uwCamRight"
#define UW_U_CAMUP    "uwCamUp"
#define UW_U_TANHALF  "uwTanHalf"
#define UW_U_ASPECT   "uwAspect"
#define UW_U_SURFACEY "uwSurfaceY"
#define UW_U_AMP      "uwAmp"
#define UW_U_FREQ     "uwFreq"
#define UW_U_SPEED    "uwSpeed"

/* ---------- AYARLAR ----------

   UW_BLUR: bulaniklik yaricapi (piksel). Kucuk: amac goruntuyu dagitmak
            degil, suda gorusun yumusadigini hissettirmek.
   UW_TINT: mavi tonun siddeti.

   UW_LIGHT: tona EKLENEN BEYAZ orani. 0.55 IDI VE YANLISTI: saf beyazin
   %55'i tona karistiginda ton (0.09,0.25,0.35) yerine ~(0.59,0.66,0.71)
   oluyor, yani neredeyse BEYAZ. Su altinda ekranin boya gibi bembeyaz
   gorunmesinin sebebi buydu. Ton, suyun KENDI rengi olmali; beyaz serpme
   yalnizca goruntunun gomulmesini engelleyecek kadar tutulur. */
#define UW_BLUR  2.4
#define UW_TINT  0.55
#define UW_LIGHT 0.10

/* VERTEX ASAMASI. raylib'in cizim partisi uzerinden gecer. */
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

/* FRAGMENT ASAMASI. */
#define UNDERWATER_FRAGMENT_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_FRAG gclFragColor\n" \
"    #define GCL_VIN in\n" \
"out vec4 gclFragColor;\n" \
"#else\n" \
"    #define GCL_FRAG gl_FragColor\n" \
"    #define GCL_VIN varying\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision highp float;\n" \
"#endif\n" \
"GCL_VIN vec2 fragTexCoord;\n" \
"GCL_VIN vec4 fragColor;\n" \
"uniform sampler2D texture0;\n" \
"uniform vec2  uwRes;\n" \
"uniform float uwTime;\n" \
"uniform vec3  uwShallow;\n" \
"uniform vec3  uwDeep;\n" \
"uniform float uwSubmerge;\n" \
"uniform vec3  uwCamPos;\n" \
"uniform vec3  uwCamFwd;\n" \
"uniform vec3  uwCamRight;\n" \
"uniform vec3  uwCamUp;\n" \
"uniform float uwTanHalf;\n" \
"uniform float uwAspect;\n" \
"uniform float uwSurfaceY;\n" \
"uniform float uwAmp;\n" \
"uniform float uwFreq;\n" \
"uniform float uwSpeed;\n" \
"#define UW_BLUR  2.4\n" \
"#define UW_TINT  0.55\n" \
"#define UW_LIGHT 0.10\n" \
"/* ---------- DALGA ALANI ----------\n" \
"\n" \
"   3B su yuzeyiyle AYNI formul (bkz. gcl_water_shader.h: gclWaterWave):\n" \
"   ayni yonler, ayni frekans katlari, ayni faz. */\n" \
"void gclUwWave(vec2 p, float t, out float h)\n" \
"{\n" \
"    vec2 d1 = normalize(vec2( 1.00,  0.28));\n" \
"    vec2 d2 = normalize(vec2(-0.36,  1.00));\n" \
"    vec2 d3 = normalize(vec2( 0.72,  0.72));\n" \
"    float p1 = dot(p, d1)*(1.00*uwFreq) + t*(0.90*uwSpeed);\n" \
"    float p2 = dot(p, d2)*(1.73*uwFreq) + t*(1.27*uwSpeed);\n" \
"    float p3 = dot(p, d3)*(2.41*uwFreq) + t*(1.61*uwSpeed);\n" \
"    h = 1.00*sin(p1) + 0.62*sin(p2) + 0.34*sin(p3);\n" \
"}\n" \
"void main(void)\n" \
"{\n" \
"    vec2  uv  = fragTexCoord;\n" \
"    float sub = clamp(uwSubmerge, 0.0, 1.0);\n" \
"    vec2  px  = 1.0/max(uwRes, vec2(1.0));\n" \
"    /* uv.y=0 ekranin USTUDUR (bkz. gcl_SimpleWater.c: dondurme notu),\n" \
"       bu yuzden NDC y'si ters cevrilir. */\n" \
"    vec2  ndc = vec2(uv.x*2.0 - 1.0, 1.0 - uv.y*2.0);\n" \
"    vec3  vue  = uwCamFwd\n" \
"               + uwCamRight*(ndc.x*uwAspect*uwTanHalf)\n" \
"               + uwCamUp   *(ndc.y*uwTanHalf);\n" \
"    float vlen = length(vue);\n" \
"    vec3  dir  = (vlen > 1.0e-5) ? vue/vlen : vec3(0.0, 0.0, -1.0);\n" \
"    /* ---------- KAPLAMA: ISIN DALGANIN ALTINA INIYOR MU ----------\n" \
"\n" \
"       Mercek yuzeyin ALTINDA ise bu piksel su altidir. Ancak mercek tam\n" \
"       su cizgisindeyken (yarim batmis) bazi isinlar dalga CUKURUNDAN\n" \
"       gecerek suyu erken terk eder ve gokyuzunu gorur; ekrandaki kuru\n" \
"       bolge TAM OLARAK o isinlardir. Bu yuzden isin ilerletilir ve\n" \
"       DALGA alanina gore en alcak noktasi olculur: en alcak nokta\n" \
"       yuzeyin ALTINDA ise isin suyun icindedir.\n" \
"\n" \
"       Mesafeler carpanla buyur (1.5, 2.5, 4.3 ... ~1900): yakin plandan\n" \
"       ufka kadar tek geciste taranir. Sabit kisa bir mesafe yetmezdi,\n" \
"       cunku ufka yakin isinlar yuzeye cok uzakta yaklasir. */\n" \
"    float wh;\n" \
"    gclUwWave(uwCamPos.xz, uwTime, wh);\n" \
"    float camF = uwCamPos.y - (uwSurfaceY + wh*uwAmp);\n" \
"    float wet  = 0.0;\n" \
"    if (camF < 0.05) {\n" \
"        float minF = 1.0e9;\n" \
"        float t    = 1.5;\n" \
"        for (int i = 0; i < 16; ++i) {\n" \
"            vec3  p = uwCamPos + dir*t;\n" \
"            float h2;\n" \
"            gclUwWave(p.xz, uwTime, h2);\n" \
"            float f = p.y - (uwSurfaceY + h2*uwAmp);\n" \
"            if (f < minF) minF = f;\n" \
"            t *= 1.7;\n" \
"        }\n" \
"        float edge = 0.03 + 0.30*uwAmp;\n" \
"        float occ  = 1.0 - smoothstep(-edge, edge, minF);\n" \
"        float camU = smoothstep(0.06, -0.06, camF);\n" \
"        wet = occ*camU;\n" \
"    }\n" \
"    float amt = sub*wet;\n" \
"    if (amt <= 0.004) { GCL_FRAG = vec4(texture(texture0, uv).rgb, 0.0); return; }\n" \
"    /* ---------- 1. HAFIF BLUR ----------\n" \
"\n" \
"       9 ornek. Yaricap KUCUK: goruntu okunur kalmali. */\n" \
"    float r   = UW_BLUR*px.x;\n" \
"    vec2  off = vec2(r, r);\n" \
"    vec3 acc = vec3(0.0);\n" \
"    acc += texture(texture0, uv).rgb*4.0;\n" \
"    acc += texture(texture0, uv + vec2( off.x, 0.0)).rgb*2.0;\n" \
"    acc += texture(texture0, uv + vec2(-off.x, 0.0)).rgb*2.0;\n" \
"    acc += texture(texture0, uv + vec2(0.0,  off.y)).rgb*2.0;\n" \
"    acc += texture(texture0, uv + vec2(0.0, -off.y)).rgb*2.0;\n" \
"    acc += texture(texture0, uv + vec2( off.x,  off.y)).rgb;\n" \
"    acc += texture(texture0, uv + vec2(-off.x,  off.y)).rgb;\n" \
"    acc += texture(texture0, uv + vec2( off.x, -off.y)).rgb;\n" \
"    acc += texture(texture0, uv + vec2(-off.x, -off.y)).rgb;\n" \
"    vec3 col = acc/16.0;\n" \
"    /* ---------- 2. MAVI TON ----------\n" \
"\n" \
"       Hedef renk su paletinden secilip AYDINLATILIR; boylece ton\n" \
"       eklerken goruntu KARARMAZ. */\n" \
"    vec3 tint = mix(uwShallow, uwDeep, 0.45 + 0.35*uv.y);\n" \
"    tint = mix(tint, vec3(1.0), UW_LIGHT);\n" \
"    col = mix(col, tint, UW_TINT*amt);\n" \
"    /* EKRAN BEYAZA VARAMAZ. Ton karisimi tek basina yeterli degil:\n" \
"       sahnenin kendisi (gokyuzu, gunesli arazi) zaten 1.0 olabilir ve\n" \
"       blur onu yayar. Tavan, su altinda DUZ BEYAZ ihtimalini kapatir. */\n" \
"    col = min(col, vec3(0.82));\n" \
"    /* ---------- 3. SAYDAMLIK ----------\n" \
"\n" \
"       alfa = giris siddeti. Kuru pikselde 0'dir; orada sahne AYNEN kalir. */\n" \
"    GCL_FRAG = vec4(col, amt);\n" \
"}\n"

#endif /* GCL_UNDERWATER_SHADER_H */
