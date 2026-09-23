/*
 * gcl_native_types.c — native modul struct alan tablosu (tek kaynak).
 *
 * Alan listeleri raylib basliklarindan elle tutulur; tamamlama motoru ile
 * yorumlayici ayni tabloyu paylasir (bkz. gcl_native_types.h).
 *
 * UYARI: Bir tipin alan listesi eksikse `x.` zinciri SESSIZCE bos doner —
 * ne tamamlamada oneri cikar, ne de yorumlayicida deger okunur. Yeni bir
 * raylib struct'i kullanima girerse buraya eklenmelidir.
 */
#include "gcl_native_types.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Skaler alanli tipler                                                 */
/* ------------------------------------------------------------------ */

static const GclNativeField f_vec2[]  = { {"x","float"}, {"y","float"} };
static const GclNativeField f_vec3[]  = { {"x","float"}, {"y","float"}, {"z","float"} };
static const GclNativeField f_vec4[]  = { {"x","float"}, {"y","float"}, {"z","float"}, {"w","float"} };
static const GclNativeField f_rect[]  = { {"x","float"}, {"y","float"}, {"width","float"}, {"height","float"} };
static const GclNativeField f_color[] = { {"r","int"}, {"g","int"}, {"b","int"}, {"a","int"} };

/* Camera3D and Camera resolve to the same 5 fields (raylib typedef Camera3D Camera). */
static const GclNativeField f_camera[] = {
    {"position","Vector3"}, {"target","Vector3"}, {"up","Vector3"},
    {"fovy","float"}, {"projection","int"}
};
static const GclNativeField f_camera2d[] = {
    {"offset","Vector2"}, {"target","Vector2"}, {"rotation","float"}, {"zoom","float"}
};
static const GclNativeField f_ray[] = { {"position","Vector3"}, {"direction","Vector3"} };
static const GclNativeField f_raycol[] = {
    {"hit","int"}, {"distance","float"}, {"point","Vector3"}, {"normal","Vector3"}
};
static const GclNativeField f_bbox[] = { {"min","Vector3"}, {"max","Vector3"} };
static const GclNativeField f_matrix[] = {
    {"m0","float"},{"m1","float"},{"m2","float"},{"m3","float"},
    {"m4","float"},{"m5","float"},{"m6","float"},{"m7","float"},
    {"m8","float"},{"m9","float"},{"m10","float"},{"m11","float"},
    {"m12","float"},{"m13","float"},{"m14","float"},{"m15","float"}
};
static const GclNativeField f_npatch[] = {
    {"source","Rectangle"}, {"left","int"}, {"top","int"},
    {"right","int"}, {"bottom","int"}, {"layout","int"}
};
static const GclNativeField f_glyph[] = {
    {"value","int"}, {"offsetX","int"}, {"offsetY","int"},
    {"advanceX","int"}, {"image","Image"}
};
static const GclNativeField f_texture2d[] = {
    {"id","int"}, {"width","int"}, {"height","int"}, {"mipmaps","int"}, {"format","int"}
};
static const GclNativeField f_rtex2d[] = {
    {"id","int"}, {"texture","Texture2D"}, {"depth","Texture2D"}
};
static const GclNativeField f_font[] = {
    {"baseSize","int"}, {"glyphCount","int"}, {"glyphPadding","int"},
    {"texture","Texture2D"}, {"recs","Rectangle"}, {"glyphs","GlyphInfo"}
};
static const GclNativeField f_shader[] = { {"id","int"}, {"locs","int"} };
static const GclNativeField f_image[] = {
    {"data","void"}, {"width","int"}, {"height","int"},
    {"mipmaps","int"}, {"format","int"}
};
static const GclNativeField f_aevent[] = {
    {"frame","int"}, {"type","int"}, {"params","int"}
};
/* VrDeviceInfo — raylib 6.1 (HMD cihaz parametreleri) */
static const GclNativeField f_vrdevice[] = {
    {"hResolution","int"}, {"vResolution","int"},
    {"hScreenSize","float"}, {"vScreenSize","float"},
    {"eyeToScreenDistance","float"}, {"lensSeparationDistance","float"},
    {"interpupillaryDistance","float"},
    {"lensDistortionValues","void"}, {"chromaAbCorrection","void"}
};
/* VrStereoConfig — raylib 6.1 (matrisler + lens/ekran merkezleri + olcek) */
static const GclNativeField f_vrstereo[] = {
    {"projection","Matrix"}, {"viewOffset","Matrix"},
    {"leftLensCenter","Vector2"}, {"rightLensCenter","Vector2"},
    {"leftScreenCenter","Vector2"}, {"rightScreenCenter","Vector2"},
    {"scale","Vector2"}, {"scaleIn","Vector2"}
};
/* Texture / TextureCubemap ayni duzene sahiptir (Texture2D ile ayni). */
static const GclNativeField f_texture[] = {
    {"id","int"}, {"width","int"}, {"height","int"}, {"mipmaps","int"}, {"format","int"}
};
static const GclNativeField f_rtexture[] = {
    {"id","int"}, {"texture","Texture"}, {"depth","Texture"}
};

/* --- Kayitli (handle tablosunda tutulan) tipler ---
   Bu tipler `reg_*(` cagrilariyla uretilir; alan listeleri BURADA olmak
   ZORUNDADIR, aksi halde `Raylib.LoadModel("x").transform.` gibi ifadeler
   sessizce bos doner. */
static const GclNativeField f_quaternion[] = {
    {"x","float"}, {"y","float"}, {"z","float"}, {"w","float"}
};
static const GclNativeField f_transform[] = {
    {"translation","Vector3"}, {"rotation","Quaternion"}, {"scale","Vector3"}
};
/* Mesh — raylib 6.1 duzeni (boneIds -> boneIndices, boneCount eklendi) */
static const GclNativeField f_mesh[] = {
    {"vertexCount","int"}, {"triangleCount","int"},
    {"vertices","void"}, {"texcoords","void"}, {"texcoords2","void"},
    {"normals","void"}, {"tangents","void"}, {"colors","void"},
    {"indices","void"},
    {"boneCount","int"}, {"boneIndices","void"}, {"boneWeights","void"},
    {"animVertices","void"}, {"animNormals","void"},
    {"vaoId","int"}, {"vboId","void"}
};
static const GclNativeField f_materialmap[] = {
    {"texture","Texture2D"}, {"color","Color"}, {"value","float"}
};
static const GclNativeField f_material[] = {
    {"shader","Shader"}, {"maps","MaterialMap"}, {"params","void"}
};
/* BoneInfo — raylib 6.1: boneId -> parent */
static const GclNativeField f_boneinfo[] = {
    {"name","void"}, {"parent","int"}
};
/* ModelSkeleton — raylib 6.1'de Model'in icinde yasar */
static const GclNativeField f_modelskeleton[] = {
    {"boneCount","int"}, {"bones","BoneInfo"}, {"bindPose","Transform"}
};
/* ModelAnimation — raylib 6.1 (transform/animFrameCount -> keyframeCount/keyframePoses) */
static const GclNativeField f_modelanim[] = {
    {"name","void"}, {"boneCount","int"}, {"keyframeCount","int"}, {"keyframePoses","Transform"}
};
/* Model — raylib 6.1 duzeni (bones/bindPose -> skeleton + currentPose/boneMatrices) */
static const GclNativeField f_model[] = {
    {"transform","Matrix"}, {"meshCount","int"}, {"materialCount","int"},
    {"meshes","Mesh"}, {"materials","Material"}, {"meshMaterial","void"},
    {"skeleton","ModelSkeleton"}, {"currentPose","Transform"}, {"boneMatrices","Matrix"}
};
static const GclNativeField f_wave[] = {
    {"frameCount","int"}, {"sampleRate","int"}, {"sampleSize","int"},
    {"channels","int"}, {"data","void"}
};
static const GclNativeField f_audiostream[] = {
    {"sampleRate","int"}, {"sampleSize","int"}, {"channels","int"},
    {"buffer","void"}, {"processor","void"}
};
static const GclNativeField f_sound[] = {
    {"stream","AudioStream"}, {"frameCount","int"}
};
static const GclNativeField f_music[] = {
    {"frameCount","int"}, {"looping","int"},
    {"ctxType","int"}, {"ctxData","void"}
};
static const GclNativeField f_filepathlist[] = {
    {"count","int"}, {"paths","void"}
};
static const GclNativeField f_aeventlist[] = {
    {"capacity","int"}, {"count","int"}, {"events","AutomationEvent"}
};
/* --- Oyun modulu tipleri (RaylibFPS / RaylibSimpleMesh / RaylibSimpleCollision) ---
   Bu iki tip modullere AITTIR: alan listeleri modulun LastSlot kanalindaki
   sirayla birebir ayni olmak zorundadir (bkz. Modules/gcl_raylib_fps.c ve
   Modules/gcl_SimpleMesh.c). Bir alan eklenirse modul de guncellenmelidir. */
static const GclNativeField f_fps[] = {
    {"Position","Vector3"}, {"Rotate","Vector3"}, {"LookAt","Vector3"},
    {"Height","float"}, {"Gravity","float"}, {"IsGravityOn","int"},
    {"Forward","float"}, {"Backward","float"}, {"Left","float"}, {"Right","float"},
    {"Camera","Camera3D"}
};
static const GclNativeField f_terrainmaterial[] = {
    {"Texture","int"}, {"Color","int"}
};
static const GclNativeField f_terrain[] = {
    {"Material","TerrainMaterial"}, {"Handle","int"}
};
/* RaylibSimpleMesh.Mesh — modulun KENDI mesh tutamacI, raylib'in ham Mesh
   struct'i DEGIL. Tabloya NITELIKLI adiyla kaydedilir: lookup_native_struct()
   once tam adi dener, ancak bulamazsa noktadan sonrasina duser
   (bkz. GCL/SimpleRunner/gcl_runner.c). Bu sayede

       RaylibSimpleMesh.Mesh CUBE;   // bu tip
       Mesh m;                       // raylib'in ham mesh struct'i

   ikisi de dogru cozulur. Duz "Mesh" adiyla eklenirse raylib'in kendi girdisi
   once geldigi icin golgelenir ve `CUBE.Material.Texture` sessizce 0 donerdi.
   Alan sirasi modulun LastSlot kanaliyla birebir ayni olmali
   (bkz. Modules/gcl_SimpleMesh.c): Texture, Color, Handle — ve ARDINDAN
   Position, Rotate, Scale. Ilk uc alan Terrain ile PAYLASILIR; dokuz ek alan
   yalnizca Mesh'tedir, cunku dunyada duran tek tek nesnelerin nerede
   durdugunu soylemesi gerekir (Terrain dosyanin koydugu yerde durur).
   Toplam 3 + 9 = 12 skaler yaprak; sayaclar gcl_runner.c'deki
   g_native_slot_maps[] ile AYNI olmak zorundadir. */
static const GclNativeField f_simplemesh[] = {
    {"Material","TerrainMaterial"}, {"Handle","int"},
    {"Position","Vector3"}, {"Rotate","Vector3"}, {"Scale","Vector3"}
};
/* RaylibSimpleLight.SunLight: Handle + Rotate. The rotation IS the sun angle
   (Rotate.z = elevation, Rotate.y = azimuth) — see Modules/gcl_SimpleLight.c,
   whose slot table (Handle, RotX, RotY, RotZ) matches this list exactly. */
static const GclNativeField f_sunlight[] = {
    {"Handle","int"}, {"Rotate","Vector3"}
};
/* RaylibFOG.FOG — distance fog. The field order must match the module's
   LastSlot channel and gcl_runner.c's `last_fog` + `g_native_slot_maps`
   entry, character for character (12 scalar leaves).

   `Color` is a packed 0..255 value like every other colour in GCL, while
   `Enabled` and `HeightFog` are SWITCHES: the module reduces them with
   `!= 0`, so writing `true` and writing `1` open the same layer. */
static const GclNativeField f_fog[] = {
    {"Handle","int"}, {"Color","int"}, {"Density","float"}, {"Start","float"},
    {"End","float"}, {"Mode","int"}, {"Enabled","int"}, {"Height","float"},
    {"Falloff","float"}, {"Alpha","float"}, {"Noise","float"}, {"HeightFog","int"}
};

/* RaylibShader.SimpleShader: the shader handle returned by CreateSimpleShader.
   ONE scalar leaf, so passing the variable to Raylib.BeginShaderMode() forwards
   the handle itself. */
static const GclNativeField f_simpleshader[] = {
    {"Handle","int"}
};
/* RaylibSimpleWater.Water — su yuzeyi. Alan sirasi modulun LastSlot kanaliyla
   BIREBIR ayni olmak zorundadir (bkz. Modules/gcl_SimpleWater.c ve
   gcl_runner.c'deki g_native_slot_maps[]): 19 skaler yaprak. Iki nokta:
   Bu tip YALNIZCA SUYU TANIMLAR; icinde yuzme esigi, kapsul olcusu ya da
   "yuzuyor sayilma" karari YOKTUR. Boyle bir esik tutmak, ekranin
   "yuzuyorum" dedigi an ile fizigin yuzdugu ani birbirinden ayirirdi; karar
   tek bir yerde (bkz. Modules/gcl_raylib_fps.c) verilir.

   * `Color` SUYUN TONUDUR, aydinlatma rengi degil; shader'in taban renkleri
     bununla CARPILIR. Varsayilan Raylib.WHITE oldugu icin hicbir sey
     yazilmazsa varsayilan su rengi aynen gorunur.
   * Yerlesim `Position/Rotate/Scale` ucundan kurulur ve RaylibSimpleMesh.Mesh
     ile AYNI kurali izler (Rotate DERECE, Scale varsayilan 1). Ic ice Vector3
     alanlari skaler yapraklara ACIYARAK katilir: Position x/y/z, sonra
     Rotate x/y/z, sonra Scale x/y/z — slot sayaci bu sirayi bekler. */
static const GclNativeField f_water[] = {
    {"Color","int"}, {"Handle","int"},
    {"Position","Vector3"}, {"Rotate","Vector3"}, {"Scale","Vector3"},
    {"Alpha","float"}, {"Reflection","float"}, {"Refraction","float"},
    {"Fresnel","float"}, {"WaveStrength","float"}, {"WaveSpeed","float"},
    {"WaveScale","float"}, {"Foam","float"}
};

/* RaylibSKYBOX.Skybox — yazi tabanli shader gokyuzu. Alan sirasi modulun
   LastSlot kanaliyla BIREBIR ayni olmak zorundadir (bkz. Modules/gcl_skybox.c
   ve gcl_runner.c'deki g_native_slot_maps[]): 10 skaler yaprak. DailyCycle bir
   SABIT GORUNUMdur (RaylibSKYBOX.night = 0 ... cycle = 10).

   CloudON ek KATMANI acar: keskin kenarli, duz renkli (cartoon) ikinci bulut
   katmani. Rengi gunesin yuksekligine gore degisir — gunduz beyaz, gece koyu
   mavi-gri. 0 = kapali (varsayilan). */
static const GclNativeField f_skybox[] = {
    {"Handle","int"}, {"DailyCycle","int"},
    {"Time","float"}, {"DayLength","float"},
    {"CloudAmount","float"}, {"CloudSpeed","float"}, {"StarAmount","float"},
    {"SunSize","float"}, {"SunBrightness","float"}, {"CloudON","int"}
};

/* ------------------------------------------------------------------ */
/* Tip tablosu                                                          */
/* ------------------------------------------------------------------ */

static const GclNativeStruct gcl_native_structs[] = {
    { "Vector2",         f_vec2,     (int)(sizeof(f_vec2)     / sizeof(f_vec2[0]))     },
    { "Vector3",         f_vec3,     (int)(sizeof(f_vec3)     / sizeof(f_vec3[0]))     },
    { "Vector4",         f_vec4,     (int)(sizeof(f_vec4)     / sizeof(f_vec4[0]))     },
    { "Rectangle",       f_rect,     (int)(sizeof(f_rect)     / sizeof(f_rect[0]))     },
    { "Color",           f_color,    (int)(sizeof(f_color)    / sizeof(f_color[0]))    },
    { "Camera",          f_camera,   (int)(sizeof(f_camera)   / sizeof(f_camera[0]))   },
    { "Camera3D",        f_camera,   (int)(sizeof(f_camera)   / sizeof(f_camera[0]))   },
    { "Camera2D",        f_camera2d, (int)(sizeof(f_camera2d) / sizeof(f_camera2d[0])) },
    { "Ray",             f_ray,      (int)(sizeof(f_ray)      / sizeof(f_ray[0]))      },
    { "RayCollision",    f_raycol,   (int)(sizeof(f_raycol)   / sizeof(f_raycol[0]))   },
    { "BoundingBox",     f_bbox,     (int)(sizeof(f_bbox)     / sizeof(f_bbox[0]))     },
    { "Matrix",          f_matrix,   (int)(sizeof(f_matrix)   / sizeof(f_matrix[0]))   },
    { "NPatchInfo",      f_npatch,   (int)(sizeof(f_npatch)   / sizeof(f_npatch[0]))   },
    { "GlyphInfo",       f_glyph,    (int)(sizeof(f_glyph)    / sizeof(f_glyph[0]))    },
    { "Texture2D",       f_texture2d,(int)(sizeof(f_texture2d)/ sizeof(f_texture2d[0]))},
    { "RenderTexture2D", f_rtex2d,   (int)(sizeof(f_rtex2d)   / sizeof(f_rtex2d[0]))   },
    { "Font",            f_font,     (int)(sizeof(f_font)     / sizeof(f_font[0]))     },
    { "Shader",          f_shader,   (int)(sizeof(f_shader)   / sizeof(f_shader[0]))   },
    { "Image",           f_image,    (int)(sizeof(f_image)    / sizeof(f_image[0]))    },
    { "AutomationEvent", f_aevent,   (int)(sizeof(f_aevent)   / sizeof(f_aevent[0]))   },
    { "VrStereoConfig",  f_vrstereo, (int)(sizeof(f_vrstereo) / sizeof(f_vrstereo[0])) },
    /* Kayitli (reg_*) tipler — bkz. yukaridaki alan listesi notu. */
    { "Quaternion",         f_quaternion,  (int)(sizeof(f_quaternion)  / sizeof(f_quaternion[0]))  },
    { "Transform",          f_transform,   (int)(sizeof(f_transform)   / sizeof(f_transform[0]))   },
    { "Mesh",               f_mesh,        (int)(sizeof(f_mesh)        / sizeof(f_mesh[0]))        },
    { "MaterialMap",        f_materialmap, (int)(sizeof(f_materialmap) / sizeof(f_materialmap[0])) },
    { "Material",           f_material,    (int)(sizeof(f_material)    / sizeof(f_material[0]))    },
    { "BoneInfo",           f_boneinfo,    (int)(sizeof(f_boneinfo)    / sizeof(f_boneinfo[0]))    },
    { "Model",              f_model,       (int)(sizeof(f_model)       / sizeof(f_model[0]))       },
    { "Wave",               f_wave,        (int)(sizeof(f_wave)        / sizeof(f_wave[0]))        },
    { "AudioStream",        f_audiostream, (int)(sizeof(f_audiostream) / sizeof(f_audiostream[0])) },
    { "Sound",              f_sound,       (int)(sizeof(f_sound)       / sizeof(f_sound[0]))       },
    { "Music",              f_music,       (int)(sizeof(f_music)       / sizeof(f_music[0]))       },
    { "FilePathList",       f_filepathlist,(int)(sizeof(f_filepathlist)/ sizeof(f_filepathlist[0]))},
    { "AutomationEventList",f_aeventlist,  (int)(sizeof(f_aeventlist)  / sizeof(f_aeventlist[0]))  },
    /* raylib 6.1 ek tipler */
    { "Texture",            f_texture,      (int)(sizeof(f_texture)      / sizeof(f_texture[0]))      },
    { "TextureCubemap",     f_texture,      (int)(sizeof(f_texture)      / sizeof(f_texture[0]))      },
    { "RenderTexture",      f_rtexture,     (int)(sizeof(f_rtexture)     / sizeof(f_rtexture[0]))     },
    { "ModelSkeleton",      f_modelskeleton,(int)(sizeof(f_modelskeleton)/ sizeof(f_modelskeleton[0]))},
    { "ModelAnimation",     f_modelanim,    (int)(sizeof(f_modelanim)    / sizeof(f_modelanim[0]))    },
    { "VrDeviceInfo",       f_vrdevice,     (int)(sizeof(f_vrdevice)     / sizeof(f_vrdevice[0]))     },
    /* Oyun modulu tipleri (bkz. yukaridaki alan listesi notu). */
    { "FPS",                f_fps,          (int)(sizeof(f_fps)          / sizeof(f_fps[0]))          },
    { "TerrainMaterial",    f_terrainmaterial,(int)(sizeof(f_terrainmaterial)/sizeof(f_terrainmaterial[0]))},
    { "Terrain",            f_terrain,      (int)(sizeof(f_terrain)      / sizeof(f_terrain[0]))      },
    /* NITELIKLI ad sart: duz "Mesh" yukarida raylib'in ham mesh struct'i olarak
       zaten kayitli (bkz. f_simplemesh notu). */
    { "RaylibSimpleMesh.Mesh", f_simplemesh,(int)(sizeof(f_simplemesh)   / sizeof(f_simplemesh[0]))   },
    { "SunLight",           f_sunlight,     (int)(sizeof(f_sunlight)     / sizeof(f_sunlight[0]))     },
    { "SimpleShader",       f_simpleshader, (int)(sizeof(f_simpleshader) / sizeof(f_simpleshader[0])) },
    { "Water",              f_water,        (int)(sizeof(f_water)        / sizeof(f_water[0]))        },
    { "Skybox",             f_skybox,       (int)(sizeof(f_skybox)       / sizeof(f_skybox[0]))       },
    { "FOG",                f_fog,          (int)(sizeof(f_fog)          / sizeof(f_fog[0]))          },
};

#define GCL_NATIVE_STRUCT_COUNT \
    ((int)(sizeof(gcl_native_structs) / sizeof(gcl_native_structs[0])))

const GclNativeStruct *gcl_native_struct(const char *type) {
    if (!type || !type[0]) return NULL;
    for (int i = 0; i < GCL_NATIVE_STRUCT_COUNT; i++)
        if (strcmp(gcl_native_structs[i].type, type) == 0) return &gcl_native_structs[i];
    return NULL;
}

int gcl_native_is_struct(const char *type) {
    return gcl_native_struct(type) != NULL;
}

/* Struct adlarini tek seferde indeksle (lazy init; tamamlama ve DB denetimi
   ayni listeyi kullanir). */
#define GCL_NATIVE_STRUCT_MAX 64

const char *const *gcl_native_struct_names(void) {
    static const char *names[GCL_NATIVE_STRUCT_MAX + 1];
    static int ready = 0;
    if (!ready) {
        int n = GCL_NATIVE_STRUCT_COUNT;
        if (n > GCL_NATIVE_STRUCT_MAX) n = GCL_NATIVE_STRUCT_MAX;
        for (int i = 0; i < n; i++) names[i] = gcl_native_structs[i].type;
        names[n] = NULL;
        ready = 1;
    }
    return names;
}

const char *gcl_native_strip_module_prefix(const char *name,
                                           int (*is_module)(const char *)) {
    if (!name) return "";
    const char *dot = strchr(name, '.');
    if (!dot || !dot[1]) return name;
    char mod[64];
    size_t n = (size_t)(dot - name);
    if (n == 0 || n >= sizeof(mod)) return name;
    memcpy(mod, name, n);
    mod[n] = '\0';
    if (is_module && !is_module(mod)) return name;
    return dot + 1;
}
