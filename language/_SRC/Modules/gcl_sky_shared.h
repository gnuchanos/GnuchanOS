/* gcl_sky_shared.h — THE sky, in one place.

   WHY THIS FILE EXISTS: the skybox is procedural. Its colour comes from a
   gradient, a horizon glow, a milky-way band, three star layers, a sun disc
   with its glow, a soft cloud layer and a cartoon cloud layer. A surface that
   reflects only two of those terms is not reflecting the sky — it is
   reflecting a DIFFERENT sky, and the difference is exactly the parts a viewer
   notices: the clouds and the sun.

   So the whole sky is expressed here as ONE function over a view direction:

       vec3 gclSkyColor(vec3 dir)

   Both the dome shader and the water reflection shader call it. They cannot
   drift apart, because there is only one implementation.

   *** MACRO BODY RULE — READ BEFORE EDITING ***
   Every macro below expands to a run of adjacent C string literals. Such a
   macro body may contain ONLY string literals and line continuations. It must
   NOT contain a multi-line C comment: line splicing happens BEFORE comments are
   removed, so a comment line without a trailing backslash TERMINATES the macro,
   and the backslash on the comment's LAST line then glues the following string
   literal onto the comment's closing delimiter — leaving a bare string literal
   that the compiler rejects ("expected identifier or '(' before string
   constant"). The symptom is a cascade of bogus errors far from the real cause.
   Keep all prose OUTSIDE the macros, exactly as done here.

   PORTABILITY: `__VERSION__` is defined by the driver, `precision` only under
   GL_ES (desktop GL 2.1 rejects the line). The `#version` line is prepended by
   the caller, as gcl_skybox_shader.h documents. */

#ifndef GCL_SKY_SHARED_H
#define GCL_SKY_SHARED_H

/* UNIFORM NAMES — these MUST match the strings the modules pass to
   GetShaderLocation. They are unchanged from the skybox's original set, so the
   dome side needed no renaming. */
#define SKY_U_TOP        "skyTop"
#define SKY_U_HORIZON    "skyHorizon"
#define SKY_U_BOTTOM     "skyBottom"
#define SKY_U_SUN_DIR    "sunDir"
#define SKY_U_SUN_COLOR  "sunColor"
#define SKY_U_SUN_PARAMS "sunParams"
#define SKY_U_CLOUD_LIT  "cloudColor"
#define SKY_U_CLOUD_DARK "cloudShade"
#define SKY_U_CLOUD_P    "cloudParams"
#define SKY_U_STAR_COLOR "starColor"
#define SKY_U_STAR_P     "starParams"
#define SKY_U_SKY_P      "skyParams"
#define SKY_U_BAND_P     "bandParams"

/* THE UNIFORM BLOCK.

   Contract of each value (identical to the skybox implementation):
     skyTop / skyHorizon / skyBottom — gradient: zenith / horizon / below
     sunDir / sunColor / sunParams.xyz — sun disc plus its glow
     sunParams.w  — NIGHT factor (0 day .. 1 night); gates stars and milky way
     starColor / starParams.y — star tint and cell density
     starParams.z — star brightness, starParams.x — twinkle clock
     bandParams.x — milky-way intensity, .y band width,
                    .z cartoon-cloud switch (0/1), .w horizon glow
     cloudParams  — x threshold, y softness, z height scale, w amount
     skyParams.x  — twinkle clock, .y gradient exponent,
                    .z CLOUD clock (seconds), .w horizon haze

   `mvp` is NOT here: it belongs to the caller's vertex stage and raylib fills
   it itself for SHADER_LOC_MATRIX_MVP. */
#define GCL_SKY_UNIFORMS \
"uniform vec3 skyTop;\n" \
"uniform vec3 skyHorizon;\n" \
"uniform vec3 skyBottom;\n" \
"uniform vec3 sunDir;\n" \
"uniform vec3 sunColor;\n" \
"uniform vec4 sunParams;\n" \
"uniform vec3 cloudColor;\n" \
"uniform vec3 cloudShade;\n" \
"uniform vec4 cloudParams;\n" \
"uniform vec3 starColor;\n" \
"uniform vec4 starParams;\n" \
"uniform vec4 skyParams;\n" \
"uniform vec4 bandParams;\n"

/* HASHES AND NOISE.

   gclFbm returns 0..1 with a MEAN OF 0.484 (amplitudes 0.5+0.25+... = 0.969,
   noise mean 0.5). Cloud thresholds must be chosen BELOW that value; above it
   the sky is empty in practice. */
#define GCL_SKY_NOISE_A \
"float gclHash(vec2 p)\n" \
"{\n" \
"    return fract(sin(dot(p, vec2(127.1, 311.7)))*43758.5453123);\n" \
"}\n" \
"float gclHash3(vec3 p)\n" \
"{\n" \
"    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7)))*43758.5453123);\n" \
"}\n" \
"float gclNoise(vec2 p)\n" \
"{\n" \
"    vec2 i = floor(p);\n" \
"    vec2 f = fract(p);\n" \
"    vec2 u = f*f*(3.0 - 2.0*f);\n" \
"    float a = gclHash(i);\n" \
"    float b = gclHash(i + vec2(1.0, 0.0));\n" \
"    float c = gclHash(i + vec2(0.0, 1.0));\n" \
"    float d = gclHash(i + vec2(1.0, 1.0));\n" \
"    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);\n" \
"}\n" \
"float gclFbm(vec2 p)\n" \
"{\n" \
"    float v = 0.0;\n" \
"    float a = 0.5;\n" \
"    for (int i = 0; i < 5; i++)\n" \
"    {\n" \
"        v += a*gclNoise(p);\n" \
"        p = p*2.03 + 17.3;\n" \
"        a *= 0.5;\n" \
"    }\n" \
"    return v;\n" \
"}\n" \
"float gclFbmLod(vec2 p, float detail)\n" \
"{\n" \
"    float v = 0.0;\n" \
"    float w = 0.0;\n" \
"    float a = 0.5;\n" \
"    for (int i = 0; i < 5; i++)\n" \
"    {\n" \
"        float k = (i == 0) ? 1.0 : detail;\n" \
"        v += a*k*gclNoise(p);\n" \
"        w += a*k;\n" \
"        p = p*2.03 + 17.3;\n" \
"        a *= 0.5;\n" \
"    }\n" \
"    return v*(0.968/max(w, 0.0001));\n" \
"}\n"

/* gclFbmLod exists because gclFbm always samples five octaves. The inverse
   projection (d.xz/d.y) grows without bound as a ray approaches the horizon,
   so ONE PIXEL there covers an enormous area: point-sampling the high octaves
   in that region is textbook spatial aliasing, and on screen it reads as
   stripes running toward the horizon. There are no clouds past the horizon in
   reality either — the world is round and the sky simply compresses.

   This is a mipmap in another form: `detail` scales the octave weights down
   with distance, so 1 keeps every octave and 0 keeps only the base. The result
   is divided by the weight sum and scaled by 0.484 so the MEAN IS PRESERVED —
   without that, dropping detail would drag the mean down and the cloud
   thresholds (0.42/0.46) would stop meaning anything. */
#define GCL_SKY_NOISE GCL_SKY_NOISE_A

/* ONE STAR LAYER.

   CELL OVERFLOW — the cause of a "clipped circle": a star is drawn at a point
   displaced from its cell centre by `jit`, with radius `size`. A sample point
   only measures its distance to the star IN ITS OWN CELL, so if the displaced
   star comes closer than `size` to the cell edge, a slice of its disc spills
   into the neighbour; there the neighbour's own star is measured instead and
   that slice is not drawn. Some stars then look cut off by a straight edge.

   THE FIX IS AN INEQUALITY, not an approximation: |jit| + size <= 0.5 (cell
   radius). A cell spans [-0.5, 0.5) after `fract`, so the distance to the edge
   is exactly 0.5. With jitter amplitude 0.30 (±0.15) and radius STAR_MAX_R =
   0.32, 0.15 + 0.32 = 0.47 < 0.5: the disc is ALWAYS inside its own cell. The
   radius is min-clamped because `baseSize` comes from the caller, so a large
   value cannot break the inequality either. */
#define GCL_SKY_STARS \
"void gclStarLayer(vec3 d, float freq, float density, float baseSize,\n" \
"                  float seed, float time, out float energy, out vec3 tint)\n" \
"{\n" \
"    const float STAR_MAX_R = 0.32;\n" \
"    vec3  g    = d*freq;\n" \
"    vec3  cell = floor(g);\n" \
"    vec3  off  = fract(g) - 0.5;\n" \
"    float h1 = gclHash3(cell + seed);\n" \
"    float h2 = gclHash3(cell.yzx + seed*1.37);\n" \
"    float h3 = gclHash3(cell.zxy + seed*2.11);\n" \
"    energy = 0.0;\n" \
"    tint   = vec3(1.0);\n" \
"    if (h1 > density)\n" \
"    {\n" \
"        vec3  jit  = (vec3(h1, h2, h3) - 0.5)*0.30;\n" \
"        float r    = length(off - jit);\n" \
"        float mag  = pow(h3, 2.2);\n" \
"        float size = min(baseSize*(0.5 + 1.2*mag), STAR_MAX_R);\n" \
"        float core = smoothstep(size, size*0.12, r);\n" \
"        float halo = exp(-r*20.0)*0.30*(0.35 + mag);\n" \
"        float tw   = 0.78 + 0.22*sin(time*2.6 + h1*6.2831853);\n" \
"        energy = (core + halo)*tw*(0.28 + 0.72*mag);\n" \
"        tint = mix(vec3(0.72, 0.83, 1.00), vec3(1.00, 0.94, 0.80),\n" \
"                   smoothstep(0.25, 0.75, h2));\n" \
"        tint = mix(tint, vec3(1.00, 0.70, 0.48), smoothstep(0.88, 1.00, h2));\n" \
"    }\n" \
"}\n"

/* CARTOON CLOUD LAYER — hard-edged, flat coloured, deliberately un-soft.

   Two things make it cartoon: the threshold is HARD (the smoothstep band is
   very narrow, so there are no mid-tones), and the shade is laid down as its
   own dark tone on the lower edge instead of being blended into the sky, so it
   reads like a drawn line.

   ITS COLOUR FOLLOWS DAY/NIGHT: it is not lit from the sun direction but from
   the sun's ELEVATION (`day`), so the sun side and the cloud body share one
   tone. Near white by day, dark blue-grey at night.

   FREQUENCY MATTERS: sampling the noise at p*0.16 reduced the ENTIRE sky to
   one giant blob and no cloud was ever visible. 2.40 gives dozens of separate
   clouds. The threshold (0.46) sits below the fbm mean (0.484) so the sky
   stays cloudy.

   FLOW: `skyParams.z` is the CLOUD CLOCK in seconds; the module advances it
   every frame by CloudSpeed. Clouds drift even in the fixed presets (night,
   day, ...) because this clock is kept SEPARATE from the sky clock. A near-zero
   coefficient makes clouds drift ~0.02 cells per second — invisible. 0.35 at
   CloudSpeed=1 drifts ~0.35 cells per second: a cloud crosses its own size in
   about 3 s, which is the right speed.

   THREE QUANTITIES vary with distance, and they are what makes the horizon
   read as "real": `dist` is the inverse projection itself (because the world
   is round, clouds COMPRESS toward the horizon as this grows); `detail` drops
   the octave count with distance so nothing aliases; `reach` fades coverage
   with distance so cloud ENDS just above the horizon. `detail` alone fixes the
   stripes but still leaves a squeezed texture if cloud runs all the way to the
   horizon; `reach` is what terminates it. Together: crisp and billowing
   nearby, flattened and hazed far away. */
#define GCL_SKY_CARTOON_CLOUD \
"vec4 gclCartoonCloud(vec3 d, float day)\n" \
"{\n" \
"    float h = d.y;\n" \
"    if (h <= 0.02) return vec4(0.0);\n" \
"    vec2  q      = d.xz/max(h, 0.10)*2.40 + vec2(skyParams.z*0.35, skyParams.z*0.18);\n" \
"    float dist   = length(d.xz)/max(h, 0.001);\n" \
"    float detail = 1.0 - smoothstep(3.0, 11.0, dist);\n" \
"    float reach  = 1.0 - smoothstep(10.0, 22.0, dist);\n" \
"    if (reach <= 0.001) return vec4(0.0);\n" \
"    float n  = gclFbmLod(q, detail)*0.62 + gclFbmLod(q*2.6, detail)*0.38;\n" \
"    vec3  white = vec3(0.98, 0.99, 1.00);\n" \
"    vec3  night = vec3(0.16, 0.19, 0.28);\n" \
"    vec3  body  = mix(night, white, day);\n" \
"    float under = smoothstep(0.46, 0.54, n);\n" \
"    vec3  shade = mix(body*0.60, body*0.78, day);\n" \
"    return vec4(mix(shade, body, under), under*reach);\n" \
"}\n"

/* THE SKY.

   Colour for a view direction. This is the single definition the dome and the
   water reflection both use.

   The order of terms is the skybox's own — it was moved here verbatim — so the
   dome looks exactly as before while the water now reflects all of it:
   gradient, horizon glow, milky way, stars, sun disc + glow, soft cloud layer,
   cartoon cloud layer.

   `dir` need not be normalised; it is normalised here so callers can pass a
   reflection vector directly.

   The SOFT cloud layer repeats the inverse-projection limit used above. `layer`
   does fade with d.y, but aliasing kicked in before that fade completed, so the
   same dist/detail/reach treatment is applied. */
#define GCL_SKY_COLOR_FN \
"vec3 gclSkyColor(vec3 dir)\n" \
"{\n" \
"    vec3  d     = normalize(dir);\n" \
"    float h     = clamp(d.y, -1.0, 1.0);\n" \
"    float time  = skyParams.x;\n" \
"    float night = clamp(sunParams.w, 0.0, 1.0);\n" \
"    float day   = 1.0 - night;\n" \
"    float up    = pow(clamp(h, 0.0, 1.0), skyParams.y);\n" \
"    float dn    = pow(clamp(-h, 0.0, 1.0), skyParams.y);\n" \
"    vec3  c     = (h >= 0.0) ? mix(skyHorizon, skyTop, up)\n" \
"                            : mix(skyHorizon, skyBottom, dn);\n" \
"    float hz    = pow(1.0 - clamp(abs(h), 0.0, 1.0), 10.0);\n" \
"    c += skyHorizon*hz*bandParams.w;\n" \
"    vec3  bandN = normalize(vec3(0.42, 0.55, -0.72));\n" \
"    vec3  bandE = normalize(cross(bandN, vec3(0.0, 1.0, 0.0)));\n" \
"    vec3  bandF = cross(bandN, bandE);\n" \
"    float bd    = dot(d, bandN);\n" \
"    float band  = exp(-bd*bd*bandParams.y);\n" \
"    float dust  = gclFbm(vec2(dot(d, bandE), dot(d, bandF))*3.2);\n" \
"    float milky = band*(0.30 + 0.85*dust);\n" \
"    c += starColor*milky*bandParams.x*night;\n" \
"    float e0, e1, e2;\n" \
"    vec3  t0, t1, t2;\n" \
"    gclStarLayer(d, starParams.y*0.62, 0.895, 0.30,  3.0, time, e0, t0);\n" \
"    gclStarLayer(d, starParams.y*1.45, 0.800, 0.20, 17.0, time, e1, t1);\n" \
"    gclStarLayer(d, starParams.y*3.05, 0.700, 0.13, 41.0, time, e2, t2);\n" \
"    float boost = 1.0 + 1.8*band;\n" \
"    vec3  stars = t0*e0*1.00 + t1*e1*0.62 + t2*e2*0.34;\n" \
"    c += stars*starColor*starParams.z*boost*night;\n" \
"    float sd   = dot(d, sunDir);\n" \
"    float disc = smoothstep(sunParams.y, sunParams.x, sd);\n" \
"    float glow = pow(max(sd, 0.0), 12.0)*sunParams.z*(1.0 - 0.72*night);\n" \
"    c += sunColor*(disc + glow);\n" \
"    float layer = clamp(d.y*cloudParams.z, 0.0, 1.0);\n" \
"    if (layer > 0.001 && cloudParams.w > 0.001)\n" \
"    {\n" \
"        vec2  p     = d.xz/max(d.y, 0.10);\n" \
"        float dist  = length(d.xz)/max(d.y, 0.001);\n" \
"        float det   = 1.0 - smoothstep(3.0, 11.0, dist);\n" \
"        float reach = 1.0 - smoothstep(10.0, 22.0, dist);\n" \
"        vec2  q     = p*2.00 + vec2(skyParams.z*0.50, skyParams.z*0.26);\n" \
"        float n     = gclFbmLod(q*1.5, det);\n" \
"        float cl    = smoothstep(cloudParams.x,\n" \
"                                 cloudParams.x + max(cloudParams.y, 0.01), n);\n" \
"        float lit   = clamp(sd*0.5 + 0.5, 0.0, 1.0)*(1.0 - 0.75*night);\n" \
"        c = mix(c, mix(cloudShade, cloudColor, lit), cl*layer*cloudParams.w*reach);\n" \
"    }\n" \
"    if (bandParams.z > 0.5)\n" \
"    {\n" \
"        vec4 cc = gclCartoonCloud(d, day);\n" \
"        c = mix(c, cc.rgb, cc.a);\n" \
"    }\n" \
"    return c;\n" \
"}\n"

/* Everything a shader needs to reproduce the sky: uniforms, noise, stars,
   clouds and the colour function, in one piece — so the include order cannot
   be got wrong by a caller. */
#define GCL_SKY_FULL \
GCL_SKY_UNIFORMS \
GCL_SKY_NOISE \
GCL_SKY_STARS \
GCL_SKY_CARTOON_CLOUD \
GCL_SKY_COLOR_FN

#endif /* GCL_SKY_SHARED_H */
