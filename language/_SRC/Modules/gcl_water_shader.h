;/* gcl_water_shader.h — GLSL for the RaylibSimpleWater module.

   WHY SHADER: a water surface is not a texture with an alpha value. Three
   things happen PER PIXEL and none of them can be baked:

     1. The surface MOVES. The waves are a function of time and world position,
        so a still mesh with a scrolling normal map shows the map sliding
        across a flat sheet instead of a surface that rises and falls.
     2. The normal is recomputed from those same waves, or the lighting and the
        reflection would describe a flat plane while the silhouette is wavy.
     3. Fresnel decides how much SKY versus how much WATER you see, and that
        ratio changes on every pixel of a large surface.

   THE SKY COMES FROM THE SKYBOX, NOT FROM THIS FILE. The sky is procedural:
   gradient, horizon glow, milky way, three star layers, a sun disc with its
   glow, a soft cloud layer and a cartoon cloud layer. A reflection that
   reproduces only the gradient is not reflecting the sky — it is reflecting a
   DIFFERENT sky, and the missing parts are exactly the ones a viewer notices:
   the clouds and the sun. `gcl_sky_shared.h` holds the one and only
   implementation, used by both the dome and this reflection.

   *** MACRO BODY RULE — READ BEFORE EDITING ***
   The macros below expand to runs of adjacent C string literals. Such a body
   may contain ONLY string literals and line continuations. It must NOT contain
   a multi-line C comment: line splicing happens BEFORE comments are removed, so
   a comment line without a trailing backslash TERMINATES the macro, and the
   backslash on the comment's LAST line glues the following string literal onto
   the comment's closing delimiter — leaving a bare string literal that the
   compiler rejects ("expected identifier or '(' before string constant").
   Keep all prose OUTSIDE the macros.

   GEOMETRY CONTRACT: the module sets `matModel` itself (raylib fills it from
   the Model's transform before every DrawMesh), so `matModel * vertexPosition`
   is the world position.

   RAYLIB'S OWN UNIFORMS ARE USED, NOT REPLACED: `mvp` and `matModel` are the
   names raylib looks up for its SHADER_LOC_* slots (rmodels.c: DrawMesh sets
   them before drawing). Declaring them here is what lets the module use an
   ordinary DrawModel() path — the same contract gcl_skybox_shader.h uses.

   UNITS, stated once because both stages depend on them:
       waterWave.x  amplitude, WORLD units (module scales it by model size)
       waterWave.y  angular frequency, RADIANS PER WORLD UNIT (so 2*PI*k/model
                    gives exactly k crests across the surface)
       waterWave.z  time multiplier
       waterWave.w  the per-pixel ripple DETAIL AMOUNT (0..1)

   PORTABILITY: one source feeds GL 3.3 / GL 2.1 / GLES2-3. `__VERSION__` is
   defined by the driver; `precision` is declared only under GL_ES (desktop
   GL 2.1 rejects the line). The `#version` line is prepended by the CALLER. */

#ifndef GCL_WATER_SHADER_H
#define GCL_WATER_SHADER_H

#include "gcl_sky_shared.h"

/* Desktop default; a script may override through Raylib.GLSL_VERSION. */
#define WATER_DEFAULT_GLSL 330

/* ---------- UNIFORM NAMES ----------
   These MUST match the strings the module passes to GetShaderLocation.

   The sky uniforms are NOT listed here: they are the skybox's own names
   (SKY_U_* in gcl_sky_shared.h) and the module fills them from the skybox's
   live state, so both shaders read literally the same values. */
#define WATER_U_TIME      "waterTime"
#define WATER_U_VIEWPOS   "waterViewPos"
#define WATER_U_SHALLOW   "waterShallow"
#define WATER_U_DEEP      "waterDeep"
#define WATER_U_WAVE      "waterWave"
#define WATER_U_SURF      "waterSurface"
#define WATER_U_FOAM      "waterFoam"

/* SIS. Su kendi GLSL programini kullandigi icin RaylibFOG'un yazdigi
   `fogColor` / `fogParams` / `fogShape` ona HIC ULASMAZ: fog modulu
   uniform'lari CIZILMEKTE OLAN sahnenin shader'ina yazar, su ise ayri bir
   programdir. Su yuzeyi bu yuzden sisli bir arazinin uzerinde cam gibi
   duruyordu.

   Cozum, gokyuzu icin kullanilanin aynisi: modul ayni degerleri
   Raylib.dll'den OKUR (gcl_raylib_shader_get_fog) ve asagidaki ADLARLA kendi
   shader'ina yazar. Boylece su, araziyle AYNI sis egrisini ve AYNI rengi
   kullanir; iki ayri sis tanimi bakim gerektirmez. */
#define WATER_U_FOG_COLOR  "waterFogColor"
#define WATER_U_FOG_PARAMS "waterFogParams"
#define WATER_U_FOG_SHAPE  "waterFogShape"

/* THE WAVE FIELD, shared verbatim by both stages.

   Three travelling sines whose directions are deliberately NOT axis-aligned
   (the third runs on the diagonal); three parallel crests would read as
   corrugation rather than as water. The directions are NORMALISED so the
   frequency means the same thing in every direction — an unnormalised
   direction would silently make one crest shorter than the others.

   The gradient is written out ANALYTICALLY rather than sampled with two extra
   height evaluations: a finite difference over the tessellation step returns
   the mesh's own faceting as if it were a slope, which is exactly the artefact
   the normal is supposed to describe.

   NOTE ON `const`: inside the GLSL below, `const` may NOT be used for the
   direction vectors. In GLSL 3.30 a const variable can only be initialised by a
   CONSTANT EXPRESSION, and a built-in call (normalize) is not one. The compiler
   rejects it, the shader fails to compile, and the water silently never draws. */
#define WATER_WAVE_FN \
"void gclWaterWave(vec2 p, float t, float freq, float speed, out float h, out vec2 grad)\n" \
"{\n" \
"    vec2 d1 = normalize(vec2( 1.00,  0.28));\n" \
"    vec2 d2 = normalize(vec2(-0.36,  1.00));\n" \
"    vec2 d3 = normalize(vec2( 0.72,  0.72));\n" \
"    float f1 = 1.00*freq, f2 = 1.73*freq, f3 = 2.41*freq;\n" \
"    float s1 = 0.90*speed, s2 = 1.27*speed, s3 = 1.61*speed;\n" \
"    float a1 = 1.00, a2 = 0.62, a3 = 0.34;\n" \
"    float p1 = dot(p, d1)*f1 + t*s1;\n" \
"    float p2 = dot(p, d2)*f2 + t*s2;\n" \
"    float p3 = dot(p, d3)*f3 + t*s3;\n" \
"    h = a1*sin(p1) + a2*sin(p2) + a3*sin(p3);\n" \
"    grad = a1*cos(p1)*f1*d1 + a2*cos(p2)*f2*d2 + a3*cos(p3)*f3*d3;\n" \
"}\n"

/* VERTEX STAGE.

   Displace the surface, then rebuild the normal from the SAME field so lighting
   and displacement can never describe different surfaces. `waterWave.x` is the
   amplitude in world units; the slope is `amplitude * gradient`, which is why
   the gradient is scaled here and nowhere else. The surface normal of
   y = f(x,z) is (-df/dx, 1, -df/dz).

   WAVES ONLY ON THE TOP. The wave field is a height over the XZ plane, so it
   describes an upward-facing surface and nothing else. Applying it to every
   vertex paints that field down the VERTICAL WALLS of a block-shaped mesh,
   where `world.xz` barely changes while the surface runs away — the gradient
   swings between extremes from one vertex to the next and the walls come out as
   hard vertical stripes. So the two normals are blended by how upward-facing
   the MESH normal is: the flat top takes the wave normals, upright sides keep
   their own. `smoothstep` rather than a step keeps the rim from being a hard
   crease. This still ignores the mesh normal on the TOP, which is the point: a
   water plane is flat, so its surface direction has to come from the waves. */
#define WATER_VERTEX_SRC \
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
"GCL_ATTR vec3 vertexNormal;\n" \
"uniform mat4 mvp;\n" \
"uniform mat4 matModel;\n" \
"uniform float waterTime;\n" \
"uniform vec4 waterWave;\n" \
"GCL_VOUT vec3 vWorldPos;\n" \
"GCL_VOUT vec3 vNormal;\n" \
WATER_WAVE_FN \
"void main(void)\n" \
"{\n" \
"    vec4 world = matModel*vec4(vertexPosition, 1.0);\n" \
"    float h;\n" \
"    vec2  grad;\n" \
"    gclWaterWave(world.xz, waterTime, waterWave.y, waterWave.z, h, grad);\n" \
"    world.y += h*waterWave.x;\n" \
"    vec3 meshN = normalize(mat3(matModel)*vertexNormal);\n" \
"    vec3 waveN = normalize(mat3(matModel)*vec3(-grad.x*waterWave.x, 1.0, -grad.y*waterWave.x));\n" \
"    float up   = smoothstep(0.5, 0.85, meshN.y);\n" \
"    vNormal    = normalize(mix(meshN, waveN, up));\n" \
"    vWorldPos = world.xyz;\n" \
"    gl_Position = mvp*vec4(world.xyz, 1.0);\n" \
"}\n"

/* FRAGMENT STAGE.

   WHAT MAKES IT READ AS WATER, in order of how much each term matters:

   1. THE REFLECTED SKY IS THE SURFACE. Water is mostly a mirror; its own
      colour only shows where you look steeply into it. `fresnel` therefore
      drives the mix, and at grazing angles the reflection is nearly total.
   2. THE NORMAL IS NOT FLAT. If the normal barely varies, Fresnel evaluates to
      the same number on every pixel and the whole sheet collapses to one flat
      colour — which is exactly what "a transparent blue overlay" looks like.
      The wave field is therefore evaluated AGAIN here at a higher frequency, so
      neighbouring pixels catch different parts of the sky.
   3. THE SUN GLINT breaks that reflection into a glitter path.

   The per-pixel ripples use the same `gclWaterWave` as the vertex stage, only
   at a frequency multiplier, so the detail belongs to the same motion instead
   of being a second, unrelated pattern.

   THE SKY IS `gclSkyColor`. Clouds, sun disc, milky way and stars are all
   inside that call, which is why they appear in the surface; a private gradient
   in this file could never match the dome. `rdir.y` is CLAMPED before sampling:
   a ray reflected slightly downward would otherwise look "under" the sky dome,
   where the horizon colour folds into an arbitrary deep tone. Water reflects
   the sky above the horizon, so that is what it is given.

   The sun glint is ADDED on top of the sun the sky already contains: the disc
   gives the steady reflection, this gives the broken, glittery path that only
   appears because the waves keep tilting.

   THE SURFACE IS A BODY, NOT A SHELL. `water.obj` is a closed volume with an
   open bottom, so anything that shows through the surface also shows the inside
   of that volume. If alpha starts at `waterSurface.x` the surface is half
   transparent by default and the water reads as HOLLOW — the terrain underneath
   is visible at full strength and the waves look like a thin sheet lying on it.

   So alpha now starts near OPAQUE and only `refraction` opens it up:

       seeThrough = refraction * (1 - fresnel)
       alpha      = 1 - seeThrough * (1 - waterSurface.x)

   `Refraction` therefore becomes the one meaningful "how much of the bottom do
   I see" dial, and `Alpha` becomes a floor rather than the whole answer. A
   grazing look is opaque on its own because `fresnel` is near 1 there, which is
   also physically right: you never see the bottom of water you look along. */
/* SIS — lighting.fs'deki formulle BIREBIR ayni.

   Kopyalamak bilinclidir: su ile arazi ayni sis egrisini kullanmazsa, sisli bir
   sahnede su yuzeyi cevresindeki araziden farkli bir mesafede "kaymaya" baslar
   ve eklem yeri belli olur. Iki taraf da ayni sayilari okudugu surece (modul
   ikisini de Raylib.dll'den alir) gorunum tek bir sistemden cikar.

   Mesafe BURADA da kameradan olculur: sis nesneye yapisik degildir, gercek
   derinlige gore kayar. */
#define WATER_FOG_FN \
"float gclWaterFogBand()\n" \
"{\n" \
"    float span = max(waterFogParams.y - waterFogParams.x, 0.001);\n" \
"    float d = length(waterViewPos - vWorldPos);\n" \
"    return clamp((d - waterFogParams.x)/span, 0.0, 1.0);\n" \
"}\n" \
"float gclWaterFogCurve(float t)\n" \
"{\n" \
"    float mode = waterFogParams.w;\n" \
"    if (mode > 1.0) return 1.0 - exp(-3.0*t*t);\n" \
"    if (mode > 0.0) return 1.0 - exp(-2.0*t);\n" \
"    return t*t;\n" \
"}\n" \
"float gclWaterFogAttenuation()\n" \
"{\n" \
"    if (waterFogShape.y <= 0.5) return 1.0;\n" \
"    float softness = clamp(waterFogShape.w, 0.0, 0.95);\n" \
"    float band = max(waterFogShape.z*(1.0 - softness), 0.001);\n" \
"    return 1.0 - smoothstep(waterFogShape.z - band, waterFogShape.z, vWorldPos.y);\n" \
"}\n" \
"float gclWaterFogAmount()\n" \
"{\n" \
"    float amount = gclWaterFogCurve(gclWaterFogBand());\n" \
"    amount *= clamp(waterFogParams.z*2.0, 0.0, 1.0);\n" \
"    amount *= gclWaterFogAttenuation();\n" \
"    return clamp(amount*waterFogShape.x, 0.0, 1.0);\n" \
"}\n"

#define WATER_FRAGMENT_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_VIN in\n" \
"    out vec4 gclFragColor;\n" \
"    #define GCL_FRAG gclFragColor\n" \
"#else\n" \
"    #define GCL_VIN varying\n" \
"    #define GCL_FRAG gl_FragColor\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision highp float;\n" \
"#endif\n" \
"GCL_VIN vec3 vWorldPos;\n" \
"GCL_VIN vec3 vNormal;\n" \
"uniform vec3 waterViewPos;\n" \
"uniform vec3 waterShallow;\n" \
"uniform vec3 waterDeep;\n" \
"uniform float waterTime;\n" \
"uniform vec4 waterWave;\n" \
"uniform vec4 waterSurface;\n" \
"uniform vec4 waterFoam;\n" \
"uniform vec3 waterFogColor;\n" \
"uniform vec4 waterFogParams;\n" \
"uniform vec4 waterFogShape;\n" \
GCL_SKY_FULL \
WATER_WAVE_FN \
WATER_FOG_FN \
"float gclWaterHash(vec2 p)\n" \
"{\n" \
"    return fract(sin(dot(p, vec2(127.1, 311.7)))*43758.5453123);\n" \
"}\n" \
"float gclWaterNoise(vec2 p)\n" \
"{\n" \
"    vec2 i = floor(p);\n" \
"    vec2 f = fract(p);\n" \
"    vec2 u = f*f*(3.0 - 2.0*f);\n" \
"    float a = gclWaterHash(i);\n" \
"    float b = gclWaterHash(i + vec2(1.0, 0.0));\n" \
"    float c = gclWaterHash(i + vec2(0.0, 1.0));\n" \
"    float d = gclWaterHash(i + vec2(1.0, 1.0));\n" \
"    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);\n" \
"}\n" \
"void main(void)\n" \
"{\n" \
"    float alpha      = waterSurface.x;\n" \
"    float reflection = waterSurface.y;\n" \
"    float refraction = waterSurface.z;\n" \
"    float fres_pow   = max(waterSurface.w, 0.5);\n" \
"    vec3  v = normalize(waterViewPos - vWorldPos);\n" \
"    vec3  n = normalize(vNormal);\n" \
"    const float DETAIL_FREQ = 3.0;\n" \
"    float dfq  = waterWave.y*DETAIL_FREQ;\n" \
"    float step = 0.25/max(dfq, 0.0001);\n" \
"    float h0, hx, hz;\n" \
"    vec2  g0, gx, gz;\n" \
"    gclWaterWave(vWorldPos.xz,                   waterTime, dfq, waterWave.z, h0, g0);\n" \
"    gclWaterWave(vWorldPos.xz + vec2(step, 0.0), waterTime, dfq, waterWave.z, hx, gx);\n" \
"    gclWaterWave(vWorldPos.xz + vec2(0.0, step), waterTime, dfq, waterWave.z, hz, gz);\n" \
"    float detail = clamp(waterWave.w, 0.0, 1.0)*waterWave.x;\n" \
"    float ddx = (hx - h0)/step;\n" \
"    float ddz = (hz - h0)/step;\n" \
"    n = normalize(n + vec3(-ddx*detail, 0.0, -ddz*detail));\n" \
"    float backfacing = gl_FrontFacing ? 0.0 : 1.0;\n" \
"    n = normalize(mix(n, -n, backfacing));\n" \
"    float facing = clamp(dot(n, v), 0.0, 1.0);\n" \
/* FRESNEL FLOOR. The physical curve (0.02 at normal incidence) is correct for
   real water, and wrong for this one: the camera sits 1.8 units above a plane
   and looks along it, so `facing` swings between ~0.02 and ~0.2 and a curve
   anchored at 0.02 evaluates to NEARLY ZERO across the whole surface. The sky
   term then contributes almost nothing and the water reads as flat blue — no
   amount of correct sky data can show through a coefficient of 0.01.

   So the floor is raised to 0.45: even looked at straight down, nearly half of
   what you see is the sky. That is not physics, it is the stylisation every
   game water uses, and it is what makes the reflection readable at all. The
   grazing end still reaches 1.0, so the horizon keeps the almost-total mirror
   that makes water read as water. */ \
"    float fresnel = mix(0.45, 1.0, pow(1.0 - facing, fres_pow));\n" \
"    fresnel *= clamp(reflection, 0.0, 1.0)*(1.0 - backfacing);\n" \
"    vec3  rdir = reflect(-v, n);\n" \
"    vec3  sky  = gclSkyColor(vec3(rdir.x, max(rdir.y, 0.001), rdir.z));\n" \
"    vec3  sun  = normalize(sunDir);\n" \
"    float spec = pow(max(dot(rdir, sun), 0.0), 180.0);\n" \
"    sky += sunColor*spec*2.2;\n" \
"    vec3 body = mix(waterShallow, waterDeep,\n" \
"                    clamp((1.0 - facing)*(1.0 - refraction*0.6), 0.0, 1.0));\n" \
"    vec3 col  = mix(body, sky, fresnel);\n" \
"    col = mix(col, waterDeep*0.75 + waterShallow*0.10, backfacing);\n" \
"    float crest = clamp((n.y - 0.90)*10.0, 0.0, 1.0);\n" \
"    float fleck = gclWaterNoise(vWorldPos.xz*2.3 + waterTime*0.35);\n" \
"    float foam  = smoothstep(0.55, 0.55 + max(waterFoam.y, 0.02), crest*fleck);\n" \
"    foam *= clamp(waterFoam.x, 0.0, 1.0);\n" \
"    col = mix(col, vec3(0.94, 0.97, 1.00), foam);\n" \
/* SIS EN SONDA uygulanir: yuzeyin kendi rengi (gokyuzu yansimasi, govde,
   kopuk) tamamlandiktan SONRA sis rengine dogru karistirilir. Sisi daha erken
   uygulamak, yansimanin da sisin arkasinda kalmasina ve suyun "sise ragmen
   parlak" gorunmesine yol acardi.

   ALFA DEGISMEZ: sis rengi getirir, saydamlik degil. Alfayi da sisle
   karistirmak uzaktaki suyu saydamlastirip altindaki araziyi gosterirdi; oysa
   gercek sista yuzey opaklasir. */ \
"    col = mix(col, waterFogColor, gclWaterFogAmount());\n" \
"    float seeThrough = clamp(refraction, 0.0, 1.0)*(1.0 - fresnel);\n" \
"    float a = clamp(1.0 - seeThrough*(1.0 - alpha) + foam, 0.0, 1.0);\n" \
"    a = mix(a, 0.94, backfacing);\n" \
"    GCL_FRAG = vec4(col, a);\n" \
"}\n"

#endif /* GCL_WATER_SHADER_H */
