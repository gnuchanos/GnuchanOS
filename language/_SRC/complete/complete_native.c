/*
 * complete_native.c — Native module API database (§4).
 *
 * Raylib/Raygui members are generated (complete_native_db.c). The Math/Stdio/Embed
 * and native struct field lists (§4.5) are kept here by hand.
 *
 * The `ret` field is for chained completion:
 *   Raylib.GetMousePosition() -> Vector2 => ".x / .y" is suggested (E4).
 */
#include "complete_native.h"

/* ------------------------------------------------------------------ */
/* Convenience macros                                                   */
/* ------------------------------------------------------------------ */
#define M_FUNC(nm, rt, ps, cat)  { nm, rt, ps, cat, "", NF_FUNC }
#define M_RET(nm, rt, ps, cat)   { nm, rt, ps, cat, "", NF_FUNC }
#define M_TYPE(nm, cat)          { nm, nm, "", cat, "", NF_TYPE }
#define M_CONST(nm, cat)         { nm, "", "", cat, "", NF_CONST }

/* ------------------------------------------------------------------ */
/* Math                                                                */
/* ------------------------------------------------------------------ */
static const GclNativeMember gcl_math_members[] = {
    M_FUNC("randInt",    "int",     "int min, int max",          "core"),
    M_FUNC("randint",    "int",     "int min, int max",          "core"),
    M_FUNC("randFloat",  "float64", "float64 min, float64 max",  "core"),
    M_FUNC("randfloat",  "float64", "float64 min, float64 max",  "core"),
    M_FUNC("min",        "float64", "float64 a, float64 b",      "core"),
    M_FUNC("max",        "float64", "float64 a, float64 b",      "core"),
    M_FUNC("abs",        "float64", "float64 x",                 "core"),
    M_FUNC("floor",      "float64", "float64 x",                 "core"),
    M_FUNC("ceil",       "float64", "float64 x",                 "core"),
    M_FUNC("round",      "float64", "float64 x",                 "core"),
    M_FUNC("sqrt",       "float64", "float64 x",                 "core"),
    M_FUNC("pow",        "float64", "float64 base, float64 exp", "core"),
    M_FUNC("sin",        "float64", "float64 degrees",           "trig"),
    M_FUNC("cos",        "float64", "float64 degrees",           "trig"),
    M_FUNC("tan",        "float64", "float64 degrees",           "trig"),
    M_FUNC("asin",       "float64", "float64 x",                 "trig"),
    M_FUNC("acos",       "float64", "float64 x",                 "trig"),
    M_FUNC("atan",       "float64", "float64 x",                 "trig"),
    M_FUNC("log",        "float64", "float64 x",                 "core"),
    M_FUNC("log10",      "float64", "float64 x",                 "core"),
    M_FUNC("exp",        "float64", "float64 x",                 "core"),
    M_FUNC("clamp",      "float64", "float64 v, float64 lo, float64 hi", "core"),
    M_FUNC("sign",       "float64", "float64 x",                 "core"),
    M_FUNC("randBool",   "int",     "void",                      "core"),
    M_FUNC("randbool",   "int",     "void",                      "core"),
    M_FUNC("randChoice", "float64", "float64 a, float64 b",      "core"),
    M_FUNC("randchoice", "float64", "float64 a, float64 b",      "core"),
    M_FUNC("randSign",   "int",     "void",                      "core"),
    M_FUNC("randsign",   "int",     "void",                      "core"),
};

/* ------------------------------------------------------------------ */
/* Stdio                                                               */
/* ------------------------------------------------------------------ */
static const GclNativeMember gcl_stdio_members[] = {
    M_FUNC("printf",      "void",   "gcChar format, ...",              "io"),
    M_FUNC("scanf",       "void",   "gcChar name",                     "io"),
    M_FUNC("openfile",    "void",   "gcChar path, gcChar mode",        "file"),
    M_FUNC("writefile",   "void",   "gcChar path, gcChar content",     "file"),
    M_FUNC("readfile",    "gcChar", "gcChar path",                     "file"),
    M_FUNC("closefile",   "void",   "gcChar path",                     "file"),
    M_FUNC("appendfile",  "void",   "gcChar path, gcChar content",     "file"),
    M_FUNC("fileexists",  "int",    "gcChar path",                     "file"),
    M_FUNC("deletefile",  "void",   "gcChar path",                     "file"),
    M_FUNC("renamefile",  "void",   "gcChar path, gcChar newName",     "file"),
    M_FUNC("filesize",    "int",    "gcChar path",                     "file"),
    M_FUNC("flushfile",   "void",   "gcChar path",                     "file"),
};

/* ------------------------------------------------------------------ */
/* Embed                                                               */
/* ------------------------------------------------------------------ */
static const GclNativeMember gcl_embed_members[] = {
    M_FUNC("Run",       "void",    "gcChar file",   "embed"),
    M_FUNC("Stop",      "void",    "void",          "embed"),
    M_FUNC("IsActive",  "int",     "void",          "embed"),
    M_FUNC("GetValue",  "float64", "gcChar name",   "embed"),
    M_FUNC("SendValue", "void",    "gcChar name, float64 value", "embed"),
};

/* ------------------------------------------------------------------ */
/* Module table (generated from Raylib/Raygui tables)                   */
/* ------------------------------------------------------------------ */
/* The module table is initialized at runtime: because gcl_native_db_*_count is a
   `const int` variable (not a constant expression in C), it cannot be used in a static
   initializer. Therefore the table is filled via lazy init. */
#define GCL_NATIVE_MODULE_COUNT 5
static GclNativeModule g_native_modules_table[GCL_NATIVE_MODULE_COUNT];
static int g_native_modules_ready = 0;

static void gcl_native_ensure_table(void) {
    if (g_native_modules_ready) return;
    g_native_modules_table[0].module = "Math";
    g_native_modules_table[0].members = gcl_math_members;
    g_native_modules_table[0].member_count =
        (int)(sizeof(gcl_math_members) / sizeof(gcl_math_members[0]));
    g_native_modules_table[1].module = "Stdio";
    g_native_modules_table[1].members = gcl_stdio_members;
    g_native_modules_table[1].member_count =
        (int)(sizeof(gcl_stdio_members) / sizeof(gcl_stdio_members[0]));
    g_native_modules_table[2].module = "Embed";
    g_native_modules_table[2].members = gcl_embed_members;
    g_native_modules_table[2].member_count =
        (int)(sizeof(gcl_embed_members) / sizeof(gcl_embed_members[0]));
    g_native_modules_table[3].module = "Raylib";
    g_native_modules_table[3].members = gcl_native_db_Raylib;
    g_native_modules_table[3].member_count = gcl_native_db_Raylib_count;
    g_native_modules_table[4].module = "Raygui";
    g_native_modules_table[4].members = gcl_native_db_Raygui;
    g_native_modules_table[4].member_count = gcl_native_db_Raygui_count;
    g_native_modules_ready = 1;
}

static const char *const gcl_module_names[] = {
    "Math", "Stdio", "Embed", "Raylib", "Raygui", NULL
};

const char *const *gcl_native_module_names(void) {
    return gcl_module_names;
}

int gcl_native_is_module(const char *name) {
    if (!name || !name[0]) return 0;
    gcl_native_ensure_table();
    for (int i = 0; i < GCL_NATIVE_MODULE_COUNT; i++)
        if (strcmp(g_native_modules_table[i].module, name) == 0) return 1;
    return 0;
}

const GclNativeModule *gcl_native_module(const char *name) {
    if (!name || !name[0]) return NULL;
    gcl_native_ensure_table();
    for (int i = 0; i < GCL_NATIVE_MODULE_COUNT; i++)
        if (strcmp(g_native_modules_table[i].module, name) == 0) return &g_native_modules_table[i];
    return NULL;
}

const GclNativeMember *gcl_native_find(const char *module, const char *member) {
    const GclNativeModule *m = gcl_native_module(module);
    if (!m || !member || !member[0]) return NULL;
    for (int i = 0; i < m->member_count; i++)
        if (m->members[i].name && strcmp(m->members[i].name, member) == 0) return &m->members[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Struct fields (§4.5)                                                 */
/* ------------------------------------------------------------------ */

/* Scalar types — no fields; chaining stops here. */
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
/* VrStereoConfig — raylib 6.1 (matrisler + lens/ekran merkezleri + ölçek) */
static const GclNativeField f_vrstereo[] = {
    {"projection","Matrix"}, {"viewOffset","Matrix"},
    {"leftLensCenter","Vector2"}, {"rightLensCenter","Vector2"},
    {"leftScreenCenter","Vector2"}, {"rightScreenCenter","Vector2"},
    {"scale","Vector2"}, {"scaleIn","Vector2"}
};
/* Texture / TextureCubemap aynı düzene sahiptir (Texture2D ile aynı). */
static const GclNativeField f_texture[] = {
    {"id","int"}, {"width","int"}, {"height","int"}, {"mipmaps","int"}, {"format","int"}
};
static const GclNativeField f_rtexture[] = {
    {"id","int"}, {"texture","Texture"}, {"depth","Texture"}
};

/* --- Kayitli (handle tablosunda tutulan) tipler ---
   Bu tipler `reg_*(` cagrilariyla uretilir; tamamlama zincirinin calismasi
   icin alan listeleri burada olmak ZORUNDADIR (aksi halde
   `Raylib.LoadModel("x").transform.` gibi ifadeler sessizce bos doner). */
static const GclNativeField f_quaternion[] = {
    {"x","float"}, {"y","float"}, {"z","float"}, {"w","float"}
};
static const GclNativeField f_transform[] = {
    {"translation","Vector3"}, {"rotation","Quaternion"}, {"scale","Vector3"}
};
/* Mesh — raylib 6.1 düzeni (boneIds -> boneIndices, boneCount eklendi) */
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
/* ModelSkeleton — raylib 6.1'de Model'in içinde yaşar */
static const GclNativeField f_modelskeleton[] = {
    {"boneCount","int"}, {"bones","BoneInfo"}, {"bindPose","Transform"}
};
/* ModelAnimation — raylib 6.1 (transform/animFrameCount -> keyframeCount/keyframePoses) */
static const GclNativeField f_modelanim[] = {
    {"name","void"}, {"boneCount","int"}, {"keyframeCount","int"}, {"keyframePoses","Transform"}
};
/* Model — raylib 6.1 düzeni (bones/bindPose -> skeleton + currentPose/boneMatrices) */
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
};

const GclNativeStruct *gcl_native_struct(const char *type) {
    if (!type || !type[0]) return NULL;
    for (int i = 0; i < (int)(sizeof(gcl_native_structs) / sizeof(gcl_native_structs[0])); i++)
        if (strcmp(gcl_native_structs[i].type, type) == 0) return &gcl_native_structs[i];
    return NULL;
}

int gcl_native_is_struct(const char *type) {
    return gcl_native_struct(type) != NULL;
}

/* Struct adlarını tek seferde indeksle (lazy init; tamamlama ve DB denetimi
   aynı listeyi kullanır). */
#define GCL_NATIVE_STRUCT_MAX 64

const char *const *gcl_native_struct_names(void) {
    static const char *names[GCL_NATIVE_STRUCT_MAX + 1];
    static int ready = 0;
    if (!ready) {
        int n = (int)(sizeof(gcl_native_structs) / sizeof(gcl_native_structs[0]));
        if (n > GCL_NATIVE_STRUCT_MAX) n = GCL_NATIVE_STRUCT_MAX;
        for (int i = 0; i < n; i++) names[i] = gcl_native_structs[i].type;
        names[n] = NULL;
        ready = 1;
    }
    return names;
}
