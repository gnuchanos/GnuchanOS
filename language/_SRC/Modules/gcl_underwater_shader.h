/* gcl_underwater_shader.h — SU ALTI EKRAN EFEKTI.

   TASARIM (tek cumle): SUYUN ALTINDA blur + mavi + saydam; SUYUN USTUNDE
   hicbir sey. Suyun ustundeki goruntu zaten 3B su yuzeyinin kendi
   shader'indan gelir (bkz. gcl_water_shader.h: dalga + gokyuzu yansimasi);
   bu dosya o goruntuye YALNIZCA kamera suya girdiginde dokunur.

   NE YAPAR:

     1. KAPLAMA (PIKSEL BASINA) — kameradan atilan isin, dalga yuzeyinin
        ALTINA iniyor mu? Iniyorsa piksel su altidir; inmiyorsa kuru kalir.
        Yarim batmisken ekranin bir kismi su altinda, gerisi normal olur.

     2. DALGALANMA (UW_WARP) — goruntu, dalga alaninin EGIMIYLE bukulur.
        Gormenin en tanidik isareti: suyun icinde her sey hafifce akar.

     3. BLUR + KROMATIK SAPMA — bes ornekli yumusatma; kirmizi ve mavi
        kanallar ZIT yonlere kaydirilir (su bir prizma gibi davranir).

     4. SOGURMA (UW_ABSORB) — kirmizi, yesil ve maviden ONCE sogurulur;
        derinlik arttikca goruntu koyulasmakla kalmaz, MAVIYE KAYAR.

     5. MAVI TON — sahne su paletine cekilir. KARARTMAZ, tonlar.

     6. KURSAGI (UW_CAUSTIC) — dalga merceklerinin yuzey altinda biraktigi
        parlak aglar; ag dalga alaninin KENDISINDEN turetilir.

     7. SAYDAMLIK — alfa = suya giris siddeti. Kuru pikselde alfa 0'dir,
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
/* UW_TINT / UW_LIGHT: su tonunun siddeti ve tona eklenen beyaz.
   Ikisi de 0.70 / 0.32'ye cikarildi ve YANLISTI: ekran "hayvan gibi beyaz
   parladi" ve suyun dibinde on gorunmez oldu. Ton suyun rengini ANLATIR,
   ekrani yakmaz; 0.55 / 0.10 belgelenmis ve dogru degerlerdir. */
#define UW_TINT  0.55
#define UW_LIGHT 0.10

/* UW_WARP: dalgalanmanin genligi, EKRAN ORANI. Su altinda goruntu, yuzeyin
   egimiyle bukulur; gormenin en tanidik isareti budur. 0.012 = ekranin
   ~%1.2'si; okunurlugu bozmadan "suyun icindeyim" dedirten deger.

   UW_CHROMA: kromatik sapma, PIKSEL. Kirmizi ve mavi kanallar farkli
   bukulur, cunku su bir prizma gibi davranir. 2 piksel: fark edilir ama
   goruntuyu dagitmaz.

   UW_ABSORB: sogurma. Kirmizi, suda ILK sogurulan renktir; derinlik
   arttikca goruntu maviye kayar. Bu yalnizca bir ton DEGIL, kanal bazli bir
   kayiptir - bu yuzden ayri bir katsayidir.

   UW_CAUSTIC: kursagi parlakligi. Su yuzeyinden gecen isik, dalga
   mercekleriyle yogunlasip parlak aglar birakir.

   BU DORT SAYI, SHADER GOVDESINDEKI DORT #define ILE AYNI OLMAK ZORUNDADIR.
   Govde C makrolarini GOREMEZ (ardisik string literaline acilir), bu yuzden
   degerler orada AYRICA yazilir; buradaki blok onlarin AYNASIDIR, ikinci bir
   ayar yeri DEGIL. Ikisi ayrisirsa yorum YALAN soyler: burasi 0.012 derken
   govdede 0.0100 kalmasi tam olarak bu hataydi. */
#define UW_WARP   0.0100
#define UW_CHROMA 2.0
#define UW_ABSORB 0.45
#define UW_CAUSTIC 0.55

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
"    #define GCL_TEX texture\n" \
"out vec4 gclFragColor;\n" \
"#else\n" \
"    #define GCL_FRAG gl_FragColor\n" \
"    #define GCL_VIN varying\n" \
"    #define GCL_TEX texture2D\n" \
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
"#define UW_WARP    0.0100\n" \
"#define UW_CHROMA  2.0\n" \
"#define UW_ABSORB  0.45\n" \
"#define UW_CAUSTIC 0.55\n" \
"/* ---------- DALGA ALANI ----------\n" \
"\n" \
"   3B su yuzeyiyle AYNI formul (bkz. gcl_water_shader.h: gclWaterWave):\n" \
"   ayni yonler, ayni frekans katlari, ayni faz. */\n" \
"float gclUwWave(vec2 p, float t)\n" \
"{\n" \
"    vec2 d1 = normalize(vec2( 1.00,  0.28));\n" \
"    vec2 d2 = normalize(vec2(-0.36,  1.00));\n" \
"    vec2 d3 = normalize(vec2( 0.72,  0.72));\n" \
"    float p1 = dot(p, d1)*(1.00*uwFreq) + t*(0.90*uwSpeed);\n" \
"    float p2 = dot(p, d2)*(1.73*uwFreq) + t*(1.27*uwSpeed);\n" \
"    float p3 = dot(p, d3)*(2.41*uwFreq) + t*(1.61*uwSpeed);\n" \
"    return 1.00*sin(p1) + 0.62*sin(p2) + 0.34*sin(p3);\n" \
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
"    /* ---------- YUZME KILIDI: KAMERA SANAL OLARAK YUZEYIN ALTINDA ----------\n" \
"\n" \
"       BURASI \"SU ALTINA GIRIYORUM AMA HICBIR SEY OLMUYOR\" HATASININ KOKU.\n" \
"\n" \
"       Fizik, YUZME modunda kafayi SU YUZEYINE KILITLER (bkz.\n" \
"       gcl_raylib_fps.c: swim_move -> `max_feet = surface - Height`).\n" \
"       Yani kamera yuzeyin TAM USTUNDE kalir; `camF` hicbir zaman\n" \
"       negatife inmez ve asagidaki `camF < 0.05` kapisi HIC ACILMAZ.\n" \
"       Oyuncu suyun icinde, gogsune kadar batmis olsa bile ekranda\n" \
"       TEK BIR su alti pikseli olusmaz - bildirilen hal TAM OLARAK budur.\n" \
"\n" \
"       Cozum: oyuncu suyun ICINDEYKEN (uwSubmerge > 0) kamerayi yuzeyin\n" \
"       ALTINDA SAY. Pay KUCUK tutulur (0.35 m): gorunen yuzey yalnizca\n" \
"       +-1.96*uwAmp = ~0.06 m gezingi icin bu pay fazlasiyla yeterli,\n" \
"       buyuk bir pay kamerayi suyun COK altina koyar ve kenarda bile her\n" \
"       isin suya girip ekrani tumden kaplar. */\n" \
"    float wh = gclUwWave(uwCamPos.xz, uwTime);\n" \
"    float camF = uwCamPos.y - (uwSurfaceY + wh*uwAmp) - uwSubmerge*0.35;\n" \
"    float wet  = 0.0;\n" \
"    if (camF < 0.05) {\n" \
"        float minF = 1.0e9;\n" \
"        float t    = 1.5;\n" \
"        for (int i = 0; i < 16; ++i) {\n" \
"            vec3  p = uwCamPos + dir*t;\n" \
"            float h2 = gclUwWave(p.xz, uwTime);\n" \
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
"    if (amt <= 0.004) { GCL_FRAG = vec4(GCL_TEX(texture0, uv).rgb, 0.0); return; }\n" \
"    /* ---------- 1. DALGALANMA ----------\n" \
"\n" \
"       Goruntuyu dalga alaninin EGIMI buker. Egim, alanin iki komsu\n" \
"       noktadaki farkiyla olculur (merkezi fark): boylece bukme, 3B\n" \
"       yuzeyin o noktadaki egimiyle AYNI yonu ve AYNI fazi tasir.\n" \
"\n" \
"       UW_WARP kucuk tutulur cunku egim 8'e kadar cikabilir (uc sinusun\n" \
"       frekans katsayilarinin toplami). Ham deger dogrudan uygulansaydi\n" \
"       goruntu ekranin ~%10'u kadar kayar ve okunmaz hale gelirdi. */\n" \
"    float e = 0.35;\n" \
"    float hx1 = gclUwWave(uwCamPos.xz + vec2(e, 0.0), uwTime);\n" \
"    float hx0 = gclUwWave(uwCamPos.xz - vec2(e, 0.0), uwTime);\n" \
"    float hy1 = gclUwWave(uwCamPos.xz + vec2(0.0, e), uwTime);\n" \
"    float hy0 = gclUwWave(uwCamPos.xz - vec2(0.0, e), uwTime);\n" \
"    vec2  warp = vec2(hx1 - hx0, hy1 - hy0)*UW_WARP*amt;\n" \
"    vec2  wuv  = uv + warp;\n" \
"    /* ---------- 2. BLUR + KROMATIK SAPMA ----------\n" \
"\n" \
"       Bes ornekli yumusatma ve kanal bazli kaydirma TEK geciste: yesil\n" \
"       bulanik tabandan, kirmizi ile mavi ise ZIT yonlerdeki\n" \
"       orneklerden gelir. Su bir prizma gibi davranir; kanallari ayri\n" \
"       ayri bukmek, tek bir ortak kaydirmadan daha inandiricidir ve\n" \
"       maliyeti yalnizca iki ek ornektir. */\n" \
"    float r    = UW_BLUR*px.x;\n" \
"    vec3  blur = GCL_TEX(texture0, wuv).rgb*0.40\n" \
"               + GCL_TEX(texture0, wuv + vec2( r, 0.0)).rgb*0.15\n" \
"               + GCL_TEX(texture0, wuv + vec2(-r, 0.0)).rgb*0.15\n" \
"               + GCL_TEX(texture0, wuv + vec2(0.0,  r)).rgb*0.15\n" \
"               + GCL_TEX(texture0, wuv + vec2(0.0, -r)).rgb*0.15;\n" \
"    vec2  ca   = vec2(UW_CHROMA*px.x, 0.0)*amt;\n" \
"    vec3  col  = vec3(GCL_TEX(texture0, wuv + ca).r,\n" \
"                      blur.g,\n" \
"                      GCL_TEX(texture0, wuv - ca).b);\n" \
"    /* ---------- 3. SOGURMA ----------\n" \
"\n" \
"       Su, kirmiziyi yesilden ve maviden ONCE sogurur. Bu yuzden\n" \
"       derinlik arttikca goruntu yalnizca koyulasmaz, MAVIYE KAYAR.\n" \
"       Kanal bazli bu kayip duz bir ton karisimi DEGILDIR; ikisi\n" \
"       birlikte kullanilir. */\n" \
"    col.r *= 1.0 - UW_ABSORB*amt;\n" \
"    col.g *= 1.0 - UW_ABSORB*0.35*amt;\n" \
"    /* ---------- 4. MAVI TON ----------\n" \
"\n" \
"       Hedef renk su paletinden secilip AYDINLATILIR; boylece ton\n" \
"       eklerken goruntu KARARMAZ. */\n" \
"    vec3 tint = mix(uwShallow, uwDeep, 0.45 + 0.35*uv.y);\n" \
"    tint = mix(tint, vec3(1.0), UW_LIGHT);\n" \
"    col = mix(col, tint, UW_TINT*amt);\n" \
"    /* ---------- 5. KURSAGI (CAUSTIC) ----------\n" \
"\n" \
"       Su yuzeyinden gecen isik, dalga mercekleriyle yogunlasip yuzeyin\n" \
"       altinda parlak aglar birakir. Ag dalga alaninin KENDISINDEN\n" \
"       turetilir: ayni alan hem yuzeyi buker hem kursagi cizer, boylece\n" \
"       ikisi hicbir zaman hizasiz kalmaz. */\n" \
"    float cuv = gclUwWave(wuv*6.0 + vec2(uwTime*0.08, uwTime*0.05), uwTime*0.60);\n" \
"    float caustic = pow(clamp(cuv*0.35 + 0.5, 0.0, 1.0), 3.0);\n" \
"    col += tint*caustic*(UW_CAUSTIC*amt);\n" \
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
