#version 120

// ---------------------------------------------------------------------------
// lighting_gl2.fs — lighting.fs'in OpenGL 2.1 / GLSL 1.20 varyanti.
//
// 330'LUK DOSYAYLA FARKI YALNIZCA SOZDIZIMIDIR; isik ve sis formulleri
// birebir aynidir. GLSL 1.20'de:
//   in            -> varying
//   texture()     -> texture2D()
//   out vec4 X    -> gl_FragColor (yerlesik cikis)
// `pow`, `mix`, `smoothstep`, `clamp`, `reflect`, `length`, `normalize` ve
// `inout` fonksiyon parametreleri 1.20'de ZATEN vardir; bu yuzden govdeler
// degismeden kalir.
//
// NEDEN GEREKLI: Intel GM965/GL960 (GMA X3100) OpenGL 2.1 tavanlidir; 330'luk
// bir shader orada DERLENMEZ ve sahne cizimsiz kalir. Script
// `Raylib.GetGLSLVersion()` ile hangi dosyanin yuklenecegine karar verir.
// ---------------------------------------------------------------------------

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// ---------------------------------------------------------------------------
// Isik
// ---------------------------------------------------------------------------
#define MAX_LIGHTS              5
#define LIGHT_DIRECTIONAL       0
#define LIGHT_POINT             1

#define SPECULAR_SHININESS      48.0

#define POINT_ATTEN_K1          0.09
#define POINT_ATTEN_K2          0.032
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
// Sis
// ---------------------------------------------------------------------------
uniform vec3 fogColor;
uniform vec4 fogParams;
uniform vec4 fogShape;

#define FOG_LINEAR              0.0
#define FOG_EXPONENTIAL         1.0
#define FOG_EXPONENTIAL_SQUARED 2.0

#define FOG_EXP_GAIN            2.0
#define FOG_EXPSQ_GAIN          3.0

// ---------------------------------------------------------------------------
// Yardimcilar
// ---------------------------------------------------------------------------

vec3 srgb_to_linear(vec3 c) {
    return pow(c, vec3(2.2));
}

vec3 linear_to_srgb(vec3 c) {
    return pow(c, vec3(1.0/2.2));
}

void accumulate_light(int index, vec3 normal, vec3 viewDir,
                      inout vec3 lightDot, inout vec3 specular) {
    if (lights[index].enabled != 1) return;

    vec3  lightDir = vec3(0.0);
    float atten    = 1.0;

    if (lights[index].type == LIGHT_DIRECTIONAL) {
        lightDir = -normalize(lights[index].target - lights[index].position);
    } else if (lights[index].type == LIGHT_POINT) {
        vec3  toLight = lights[index].position - fragPosition;
        float dist    = length(toLight);
        lightDir      = normalize(toLight);

        atten = 1.0/(1.0 + POINT_ATTEN_K1*dist + POINT_ATTEN_K2*dist*dist);
        atten *= 1.0 - smoothstep(POINT_RANGE*0.75, POINT_RANGE, dist);
    }

    float NdotL = max(dot(normal, lightDir), 0.0);
    lightDot += lights[index].color.rgb*(NdotL*atten);

    if (NdotL > 0.0) {
        float specCo = pow(max(0.0, dot(viewDir, reflect(-lightDir, normal))),
                           SPECULAR_SHININESS);
        specular += lights[index].color.rgb*(specCo*atten);
    }
}

float eye_distance() {
    return length(viewPos - fragPosition);
}

float fog_band_position() {
    float span = max(fogParams.y - fogParams.x, 0.001);
    return clamp((eye_distance() - fogParams.x)/span, 0.0, 1.0);
}

float fog_curve(float t) {
    float mode = fogParams.w;

    if (mode > FOG_EXPONENTIAL) {
        return 1.0 - exp(-FOG_EXPSQ_GAIN*t*t);
    }
    if (mode > FOG_LINEAR) {
        return 1.0 - exp(-FOG_EXP_GAIN*t);
    }
    return t*t;
}

float height_fog_attenuation() {
    if (fogShape.y <= 0.5) return 1.0;

    float softness = clamp(fogShape.w, 0.0, 0.95);
    float band = max(fogShape.z*(1.0 - softness), 0.001);
    return 1.0 - smoothstep(fogShape.z - band, fogShape.z, fragPosition.y);
}

float fog_amount() {
    float amount = fog_curve(fog_band_position());
    amount *= clamp(fogParams.z*2.0, 0.0, 1.0);
    amount *= height_fog_attenuation();
    return clamp(amount*fogShape.x, 0.0, 1.0);
}

// ---------------------------------------------------------------------------

void main()
{
    // 1.20'de doku ornekleme texture2D() ile yapilir.
    vec4 texelColor  = texture2D(texture0, fragTexCoord);
    vec3 texelLinear = srgb_to_linear(texelColor.rgb);

    vec3 normal  = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec4 tint    = colDiffuse*fragColor;

    vec3 lightDot = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        accumulate_light(i, normal, viewDir, lightDot, specular);
    }

    vec3 lit = texelLinear*(lightDot + specular);

    vec3  skyAmb    = ambient.rgb;
    vec3  groundAmb = ambient.rgb*0.35;
    float hemi      = clamp(0.5 + 0.5*normal.y, 0.0, 1.0);

    lit += texelLinear*mix(groundAmb, skyAmb, hemi);

    vec3 shaded = linear_to_srgb(lit*vec3(tint.r, tint.g, tint.b));

    float fogAmt = fog_amount();

    // 1.20'de cikis degiskeni yerine yerlesik gl_FragColor yazilir.
    gl_FragColor = vec4(mix(shaded, fogColor, fogAmt), texelColor.a*tint.a);
}
