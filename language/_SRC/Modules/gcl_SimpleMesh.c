/*
#native <RaylibSimpleMesh>

# Single Terrain
RaylibSimpleMesh.Terrain Terrain = RaylibSimpleMesh.TerrainLoad("terrain.obj");
Terrain.Material.Texture = Raylib.LoadTexture("terrain.png");
Terrain.Material.Color   = Raylib.WHITE;

Terrain.Draw();
RaylibSimpleMesh.TerrainUnload("terrain.obj");

# single mesh
RaylibSimpleMesh.Mesh CUBE = RaylibSimpleMesh.LoadObj("cube.obj");
CUBE.Material.Texture = Raylib.LoadTexture("box.png");
CUBE.Material.Color   = Raylib.WHITE;

CUBE.Position.x = 0;    # dunya konumu (varsayilan 0,0,0)
CUBE.Position.y = 0;
CUBE.Position.z = 6;
CUBE.Rotate.y   = 45;   # derece, XYZ sirayla (varsayilan 0,0,0)
CUBE.Scale.x    = 1;    # varsayilan 1,1,1
CUBE.Scale.y    = 1;
CUBE.Scale.z    = 1;

CUBE.Draw();

RaylibSimpleMesh.UnloadObj(CUBE);

*/

/* Backend.

   TerrainLoad(fileName) and LoadObj(fileName) are the SAME operation: load a
   file as a raylib Model and keep it in this module's registry. The returned
   number is the registry index, and that is what the script stores in the
   struct's Handle leaf. There is deliberately one registry and one Draw()
   path, so a Terrain and a Mesh behave identically and neither can drift from
   the other.

   The runtime flattens a native struct argument into numbers in DECLARATION
   order and publishes updates back through the LastSlot channel (slot i ->
   scalar leaf i of the type, see SharedPipeline/gcl_native_types.c). The
   first three slots are SHARED by both types, and that shared prefix is the
   whole contract between them:

       0 Material.Texture   1 Material.Color   2 Handle

   Draw() recomputes the material from slots 0..1 and draws the model of slot
   2, so the script's assignments take effect.

   A Mesh then carries NINE MORE slots, because an object placed in the world
   has to say WHERE it is:

       3..5  Position.x/y/z   (default 0,0,0)
       6..8  Rotate.x/y/z     (default 0,0,0, in DEGREES)
       9..11 Scale.x/y/z      (default 1,1,1)

   Terrain does NOT: it is the one large ground mesh that every other module
   samples, and it belongs where the file puts it. So Terrain.Draw() sends 3
   arguments while Mesh.Draw() sends 12, and read_mesh() below RESETS the
   transform slots whenever they are absent. That reset is what keeps the two
   types from leaking into each other through the single g_slot[] array:
   without it a Terrain drawn after a moved Mesh would silently inherit the
   Mesh's translation, because a 3-argument call leaves slots 3..11 untouched.

   TerrainUnload(fileName) frees every registry entry loaded from that file;
   UnloadObj(struct) frees the one the flattened struct points at.

   The exported gcl_simplemesh_terrain_height() is the terrain sampler that
   RaylibSimpleCollision.dll calls (both modules run in the same process). */

#include "gcl_module.h"

/* Only the loader API is needed from windows.h; NOGDI/NOUSER keep wingdi.h
   and winuser.h out, whose Rectangle/CloseWindow clash with raylib's. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include "raylib.h"

/* raymath is header-only, and WITHOUT one of its two mode macros RMAPI expands
   to a plain C99 `inline` (raymath.h:83) that has no out-of-line definition -
   using MatrixMultiply/MatrixRotateXYZ then fails at LINK time. Pinning it to
   the static-inline mode is the documented way for a consumer to include it. */
#define RAYMATH_STATIC_INLINE
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MESH_SLOT_COUNT 12
#define MODEL_MAX       64
#define MODEL_NAME_MAX  512

#define TERRAIN_HIT_NONE (-1.0e30f)

/* Slot order IS the declaration order of the GCL types (see
   SharedPipeline/gcl_native_types.c): Terrain stops after M_HANDLE, Mesh
   continues through M_SCALE_Z. Keep the two in step. */
enum {
    M_TEXTURE = 0, M_COLOR, M_HANDLE,
    M_POS_X, M_POS_Y, M_POS_Z,
    M_ROT_X, M_ROT_Y, M_ROT_Z,
    M_SCALE_X, M_SCALE_Y, M_SCALE_Z
};

#define MESH_DEG2RAD 0.017453292519943295f

typedef struct {
    char   file[MODEL_NAME_MAX];
    Model  model;
    /* The transform the FILE itself carries (identity for every .obj here, but
       a .gltf may not be). Every draw composes the script's Position/Rotate/
       Scale ON TOP of this and writes the product into model.transform, so
       the placement is rebuilt from the base each frame instead of being
       multiplied into the previous frame's result - which would make a
       stationary object drift away at the speed of its own translation. */
    Matrix base_transform;
    /* Modelin KENDI vertex sinirlari (yerel AABB).

       NEDEN ONBELLEK: bu kutu HER KAREDE, her SimpleBoxCollision() cagrisinda
       sorulur (gcl_simplemesh_world_bounds -> local_bounds). Ham hali butun
       vertex'leri gezer: mesh sayisi x vertex sayisi. Bir kez hesaplamak bunu
       O(1)'e indirir. Vertex verisi LoadModel'den sonra DEGISMEZ, bu yuzden
       kutu kaydin omru boyunca gecerlidir; yalnizca release_entry sifirlar. */
    float  local_min[3];
    float  local_max[3];
    int    local_bounds_ready;
    int    used;
} ModelEntry;

static ModelEntry g_models[MODEL_MAX];
static double     g_slot[MESH_SLOT_COUNT];

static const char *str_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return "";
    return argv[i];
}

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* Paketlenmiş renk -> 32-bit desen.

   `Material.Color` GCL tarafında `int` olarak bildirilmiştir, bu yüzden runner
   İŞARETLİ 32-bit değeri gönderir: Raylib.WHITE (0xFFFFFFFF) buraya -1 olarak
   ulaşır. Double'ı doğrudan `unsigned int`'e çevirmek negatif değerlerde
   TANIMSIZ davranıştır; işaretli tamsayı üzerinden gidip 32 biti
   maskelediğimizde desen aynen korunur. */
static unsigned int color_bits(double v) {
    long long i = (long long)v;
    return (unsigned int)(i & 0xFFFFFFFFLL);
}

static Color unpack_color(unsigned int v) {
    Color c;
    c.r = (unsigned char)(v & 0xFF);
    c.g = (unsigned char)((v >> 8) & 0xFF);
    c.b = (unsigned char)((v >> 16) & 0xFF);
    c.a = (unsigned char)((v >> 24) & 0xFF);
    return c;
}

/* Slot i if the caller sent it, otherwise `dflt`. */
static double slot_or(int argc, const char **argv, int i, double dflt) {
    return (argc > i) ? num_arg(argc, argv, i) : dflt;
}

/* Load the flattened struct: slots come in declaration order, and a missing
   Material.Texture keeps the model's own default material.

   EVERY slot is written here, the transform ones back to their defaults
   first. That reset is the reason a Terrain (3 arguments) drawn after a moved
   Mesh cannot inherit the Mesh's placement: the two types share this one
   g_slot[] array, and a short call leaves the higher slots exactly as the
   previous call left them. */
static void read_mesh(int argc, const char **argv) {
    g_slot[M_TEXTURE] = slot_or(argc, argv, M_TEXTURE, -1.0);
    g_slot[M_COLOR]   = slot_or(argc, argv, M_COLOR,   (double)0xFFFFFFFFu);
    g_slot[M_HANDLE]  = slot_or(argc, argv, M_HANDLE,  -1.0);

    g_slot[M_POS_X]   = slot_or(argc, argv, M_POS_X,   0.0);
    g_slot[M_POS_Y]   = slot_or(argc, argv, M_POS_Y,   0.0);
    g_slot[M_POS_Z]   = slot_or(argc, argv, M_POS_Z,   0.0);
    g_slot[M_ROT_X]   = slot_or(argc, argv, M_ROT_X,   0.0);
    g_slot[M_ROT_Y]   = slot_or(argc, argv, M_ROT_Y,   0.0);
    g_slot[M_ROT_Z]   = slot_or(argc, argv, M_ROT_Z,   0.0);
    g_slot[M_SCALE_X] = slot_or(argc, argv, M_SCALE_X, 1.0);
    g_slot[M_SCALE_Y] = slot_or(argc, argv, M_SCALE_Y, 1.0);
    g_slot[M_SCALE_Z] = slot_or(argc, argv, M_SCALE_Z, 1.0);
}

/* The script's placement as a matrix, composed in the SAME order raylib's own
   DrawModelEx uses (rmodels.c: matScale x matRotation x matTranslation), so a
   Mesh placed through these slots lands exactly where DrawModelEx would put
   it. Rotate is in DEGREES, as the GCL type documents. */
static Matrix compose_transform(double px, double py, double pz,
                                double rx, double ry, double rz,
                                double sx, double sy, double sz) {
    Matrix mat_scale = MatrixScale((float)sx, (float)sy, (float)sz);
    Matrix mat_rot   = MatrixRotateXYZ((Vector3){
        (float)(rx * MESH_DEG2RAD),
        (float)(ry * MESH_DEG2RAD),
        (float)(rz * MESH_DEG2RAD)
    });
    Matrix mat_move  = MatrixTranslate((float)px, (float)py, (float)pz);
    return MatrixMultiply(MatrixMultiply(mat_scale, mat_rot), mat_move);
}

/* The caller's flattened transform slots. Position/Rotate/Scale come from the
   caller (gcl_simplemesh_world_bounds passes the Mesh struct's own slots) or
   from g_slot[] (Draw). ONE definition, because the box the collision module
   resolves against must be the box that was drawn. */
static Matrix mesh_transform(void) {
    return compose_transform(g_slot[M_POS_X], g_slot[M_POS_Y], g_slot[M_POS_Z],
                             g_slot[M_ROT_X], g_slot[M_ROT_Y], g_slot[M_ROT_Z],
                             g_slot[M_SCALE_X], g_slot[M_SCALE_Y], g_slot[M_SCALE_Z]);
}

static ModelEntry *entry_of(int handle) {
    if (handle < 0 || handle >= MODEL_MAX) return NULL;
    if (!g_models[handle].used) return NULL;
    return &g_models[handle];
}

static Model model_of(int handle) {
    ModelEntry *e = entry_of(handle);
    if (!e) return (Model){0};
    return e->model;
}

/* Find a free slot, or reuse the one already loaded from the same file. */
static int take_entry(const char *file) {
    for (int i = 0; i < MODEL_MAX; i++) {
        if (g_models[i].used &&
            strncmp(g_models[i].file, file, MODEL_NAME_MAX) == 0)
            return i;
    }
    for (int i = 0; i < MODEL_MAX; i++) {
        if (!g_models[i].used) return i;
    }
    return -1;
}

/* Raylib.dll keeps its textures in its own registry; the module asks it for
   the entry instead of keeping a second copy. The symbol is resolved lazily so
   the module also runs when Raylib.dll is not loaded. */
typedef Texture2D (*GclRaylibTextureGet)(int);

#ifdef _WIN32
static GclRaylibTextureGet raylib_texture_get(void) {
    static GclRaylibTextureGet fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("Raylib.dll");
        if (h) fn = (GclRaylibTextureGet)(void *)GetProcAddress(h, "gcl_raylib_texture_get");
    }
    return fn;
}
#else
static GclRaylibTextureGet raylib_texture_get(void) { return NULL; }
#endif

/* Asset yolu çözümü Raylib.dll'de TEK kaynaktan gelir (gcl_raylib_asset_path):
   script "terrain.obj" yazar, dosya ise projenin varlık dizinindedir. Kuralı
   burada TEKRARLAMAK yerine yokluyoruz; sembol yoksa ad aynen kullanılır. */
typedef const char *(*GclRaylibAssetPath)(const char *);

#ifdef _WIN32
static GclRaylibAssetPath raylib_asset_path(void) {
    static GclRaylibAssetPath fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("Raylib.dll");
        if (h) fn = (GclRaylibAssetPath)(void *)GetProcAddress(h, "gcl_raylib_asset_path");
    }
    return fn;
}
#else
static GclRaylibAssetPath raylib_asset_path(void) { return NULL; }
#endif

/* Dosya adını çöz (<proje>/assets/<ad> varsa o). */
static const char *resolve_file(const char *file) {
    GclRaylibAssetPath resolve = raylib_asset_path();
    if (!resolve || !file) return file ? file : "";
    return resolve(file);
}

/* ---------- Raylib.BeginShaderMode() secilen shader ----------

   Bir script sader'ı söyle bağlar:

       Raylib.BeginShaderMode(shader);
           Terrain.Draw();
           CUBE.Draw();
       Raylib.EndShaderMode();

   Ama raylib'in DrawModel()'u BeginShaderMode()'u YOK SAYAR: DrawMesh()
   kosulsuz `rlEnableShader(material.shader.id)` çağırır, yani model her zaman
   MATERYALİNDE duran shader ile çizilir ve arada bağlanan shader sessizce
   atılır. Bu yüzden ışıklandırma shader'ı hiç görünmüyordu: model varsayılan
   shader ile, yani düz/aydınlatılmamış çiziliyordu.

   Modelin sahibi bu modüldür, GL programının sahibi Raylib.dll'dir; bu yüzden
   seçimi buradan soruyoruz (gcl_raylib_shader_current) ve gerçek Shader'ı
   alıp (gcl_raylib_shader_get) çizimden ÖNCE materyale koyuyoruz. Semboller
   tembel çözülür: modül Raylib.dll yokken de yüklenebilsin. */
typedef int (*GclRaylibShaderCurrent)(void);
typedef Shader (*GclRaylibShaderGet)(int);

#ifdef _WIN32
static void *raylib_shader_symbol(const char *name) {
    HMODULE h = GetModuleHandleA("Raylib.dll");
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
static void *raylib_shader_symbol(const char *name) { (void)name; return NULL; }
#endif

static GclRaylibShaderCurrent raylib_shader_current(void) {
    static GclRaylibShaderCurrent fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclRaylibShaderCurrent)raylib_shader_symbol("gcl_raylib_shader_current"); }
    return fn;
}

static GclRaylibShaderGet raylib_shader_get(void) {
    static GclRaylibShaderGet fn = NULL;
    static int tried = 0;
    if (!tried) { tried = 1; fn = (GclRaylibShaderGet)raylib_shader_symbol("gcl_raylib_shader_get"); }
    return fn;
}

/* Secili shader'i modelin 0 numarali materyaline koy; secim yoksa (-1)
   materyal kendi varsayilan shader'inda kalir. */
static void apply_bound_shader(int handle) {
    ModelEntry *e = entry_of(handle);
    GclRaylibShaderCurrent current = raylib_shader_current();
    GclRaylibShaderGet     get     = raylib_shader_get();
    int shader_handle;

    if (!e || e->model.materialCount <= 0 || !current || !get) return;
    shader_handle = current();
    if (shader_handle < 0) return;
    e->model.materials[0].shader = get(shader_handle);
}

/* Apply the script's material to the model: Texture is a Raylib Texture2D
   handle (an int), Color is a packed R|G<<8|B<<16|A<<24 value. Both are stored
   in the model's material 0, which is what DrawModel uses. */
static void apply_material(int handle, double texture, double color) {
    ModelEntry *e = entry_of(handle);
    int         texture_handle;
    if (!e || e->model.materialCount <= 0) return;

    texture_handle = (int)texture;
    if (texture_handle >= 0) {
        GclRaylibTextureGet get_tex = raylib_texture_get();
        if (get_tex)
            e->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
                get_tex(texture_handle);
    }
    e->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = unpack_color(color_bits(color));
}

/* Ortak yukleyici: TerrainLoad ve LoadObj ayni isi yapar.

   Ayni dosya ikinci kez istenirse YENI bir kopya yuklenmez, var olan kayit
   dondurulur; yoksa her cagri bir model daha yukleyip GPU'da yer birakirdi. */
static double load_model_entry(const char *file) {
    int slot = take_entry(file);
    Model model;
    if (!file[0] || slot < 0) return -1.0;

    /* Yayinlanan durumu YUKLEME ANINDA gecerli yap.

       `RaylibSimpleMesh.Mesh CUBE = RaylibSimpleMesh.LoadObj("cube.obj");`
       bildirimi, cagri dondukten sonra modulun LastSlot kanalini okuyup
       TUM 12 yuvayi degiskene yazar (slot i -> CUBE'nin i. skaler yapragi).
       Yalnizca Handle'i yazip kalanlari static sifirda birakmak, Scale'i
       (0,0,0) yapardi: mesh_transform() her ekseni 0 ile carpar, model
       noktaya coker ve hicbir sey gorunmez. read_mesh(0, NULL)
       dokunulmayan her yuvayi belgelenmis varsayilanina cevirir
       (Texture -1, Color WHITE, Position/Rotate 0, Scale 1); Handle
       asagida gercek kayit indeksiyle ezilir. */
    read_mesh(0, NULL);

    if (g_models[slot].used) {
        g_slot[M_HANDLE] = (double)slot;
        return (double)slot;
    }
    model = LoadModel(resolve_file(file));
    if (!model.meshCount) return -1.0;
    g_models[slot].model = model;
    g_models[slot].base_transform = model.transform;
    g_models[slot].used  = 1;
    snprintf(g_models[slot].file, MODEL_NAME_MAX, "%s", file);
    g_slot[M_HANDLE] = (double)slot;
    return (double)slot;
}

/* Kaydi bosalt: modeli serbest birak ve slotu geri ver. */
static void release_entry(int handle) {
    if (!entry_of(handle)) return;
    UnloadModel(g_models[handle].model);
    g_models[handle].model = (Model){0};
    g_models[handle].used  = 0;
    g_models[handle].file[0] = 0;
    /* Onbellek bu kayda aitti; yuvayi devralan yeni model kendi kutusunu
       bastan hesaplasin. */
    g_models[handle].local_bounds_ready = 0;
}

/* TerrainLoad(fileName) -> registry index (the script keeps it in Terrain.Handle). */
static double fn_terrain_load(int argc, const char **argv) {
    return load_model_entry(str_arg(argc, argv, 0));
}

/* LoadObj(fileName) -> registry index (the script keeps it in Mesh.Handle). */
static double fn_load_obj(int argc, const char **argv) {
    return load_model_entry(str_arg(argc, argv, 0));
}

/* TerrainUnload(fileName) — frees the models loaded from that file. */
static double fn_terrain_unload(int argc, const char **argv) {
    const char *file = str_arg(argc, argv, 0);
    for (int i = 0; i < MODEL_MAX; i++) {
        if (!g_models[i].used) continue;
        if (file[0] && strncmp(g_models[i].file, file, MODEL_NAME_MAX) != 0) continue;
        release_entry(i);
    }
    return 0.0;
}

/* UnloadObj(struct) — serbest birakilacak modeli duzlesmis struct'in Handle
   yapragindan (slot 2) alir. Ciplak bir tutamac da kabul edilir:
   UnloadObj(CUBE) ve UnloadObj(handle) ayni sekilde calisir. */
static double fn_unload_obj(int argc, const char **argv) {
    int handle;
    read_mesh(argc, argv);
    handle = (int)g_slot[M_HANDLE];
    if (!entry_of(handle)) handle = (int)num_arg(argc, argv, 0);
    release_entry(handle);
    return 0.0;
}

/* Draw() — draws whatever the flattened struct points at. Both Terrain and
   Mesh take this path; a Terrain simply arrives with the transform slots in
   their defaults, which compose to no placement at all.

   DrawModel() is called with an identity position and scale ON PURPOSE: the
   placement is already in model.transform, and DrawModel would otherwise
   multiply its own translation in on top of it (rmodels.c: DrawModel ->
   DrawModelEx -> `model.transform = MatrixMultiply(model.transform,
   matTransform)`), moving the object twice. */
static double fn_draw(int argc, const char **argv) {
    int handle;
    ModelEntry *e;
    read_mesh(argc, argv);
    handle = (int)g_slot[M_HANDLE];
    e = entry_of(handle);
    if (!e) return 0.0;
    apply_material(handle, g_slot[M_TEXTURE], g_slot[M_COLOR]);
    apply_bound_shader(handle);
    e->model.transform = MatrixMultiply(e->base_transform, mesh_transform());
    DrawModel(e->model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    return 0.0;
}

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)MESH_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= MESH_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* ---------- Terrain sampler used by RaylibSimpleCollision.dll ----------

   The collision module receives the terrain handle (slot 2 of the Terrain
   struct) and needs the surface height at a point. The mesh belongs to this
   module, so the query lives here: cast a ray down through the model and
   return the nearest hit, or TERRAIN_HIT_NONE when there is none. */

GCL_EXPORT double gcl_simplemesh_terrain_height(double handle_d, double x, double z) {
    int   handle = (int)handle_d;
    Model model  = model_of(handle);
    RayCollision best = {0};

    if (!model.meshCount) return (double)TERRAIN_HIT_NONE;

    {
        Ray ray = { {(float)x, 1000.0f, (float)z}, {0.0f, -1.0f, 0.0f} };
        best.distance = 1.0e30f;
        for (int i = 0; i < model.meshCount; i++) {
            RayCollision hit = GetRayCollisionMesh(ray, model.meshes[i], model.transform);
            if (hit.hit && hit.distance < best.distance) best = hit;
        }
    }
    if (!best.hit) return (double)TERRAIN_HIT_NONE;

    return (double)best.point.y;
}

/* Ayni isin, ama NORMALI de dondurur. Kapsul collision egimi bununla olcer:
   normal.y yuzeyin ne kadar yukari baktigidir, yani dikligin tek olcusu.
   normal.y = 1 duz zemin; 0'a yaklastikca duvar.

   YUKSEKLIK VE NORMAL AYNI VURUSTAN gelir. Iki ayri isin kullanilsaydi
   farkli ucgenleri vurabilirler ve oyuncu tam bir ucgen kenarinda yanlis
   karar alirdi. Normal DUNYA uzayindadir: raylib ucgen koselerini once
   donusumle dunyaya tasir, normali oradan hesaplar. */
GCL_EXPORT double gcl_simplemesh_terrain_hit(double handle_d, double x, double z,
                                             float *out_normal) {
    int   handle = (int)handle_d;
    Model model  = model_of(handle);
    RayCollision best = {0};

    if (!model.meshCount) return (double)TERRAIN_HIT_NONE;

    {
        Ray ray = { {(float)x, 1000.0f, (float)z}, {0.0f, -1.0f, 0.0f} };
        best.distance = 1.0e30f;
        for (int i = 0; i < model.meshCount; i++) {
            RayCollision hit = GetRayCollisionMesh(ray, model.meshes[i], model.transform);
            if (hit.hit && hit.distance < best.distance) best = hit;
        }
    }
    if (!best.hit) return (double)TERRAIN_HIT_NONE;

    if (out_normal) {
        out_normal[0] = best.normal.x;
        out_normal[1] = best.normal.y;
        out_normal[2] = best.normal.z;
    }
    return (double)best.point.y;
}

/* Bounding box of the terrain, for callers that only need its extent. */
GCL_EXPORT int gcl_simplemesh_terrain_bounds(double handle_d, float *out6) {
    int handle = (int)handle_d;
    BoundingBox box;
    if (!out6 || !entry_of(handle)) return 0;
    box = GetModelBoundingBox(g_models[handle].model);
    out6[0] = box.min.x; out6[1] = box.min.y; out6[2] = box.min.z;
    out6[3] = box.max.x; out6[4] = box.max.y; out6[5] = box.max.z;
    return 1;
}

/* ---------- World-space bounding box, used by RaylibSimpleCollision.dll ----------

   `RaylibSimpleCollision.SimpleBoxCollision(PLAYER, CUBE)` has to know where
   CUBE actually IS in the world, and only this module can answer that: the
   model, the transform the FILE carries (base_transform) and the script's
   Position/Rotate/Scale all live here. The nine doubles the caller passes are
   the flattened transform slots of the Mesh struct it was handed; asking it
   to rebuild the matrix on its own would put the scale x rotate x translate
   rule in TWO places, and the collision box would then drift away from the
   box that is drawn - the one failure a collision module must never have.

   The LOCAL box is read straight from the mesh VERTICES rather than through
   raylib's GetModelBoundingBox(): that helper's treatment of model.transform
   has changed between raylib releases, and a collision box whose size depends
   on the raylib version is a bug waiting to happen. Raw vertex floats are
   stable.

   local_bounds() is that LOCAL box, and it is deliberately an axis-aligned
   min/max pair: it is what the model's own vertices span, before anything is
   placed. Orientation comes from the transform, in gcl_simplemesh_world_obb()
   below, and never from re-deriving it per caller.

   Returns 1 and writes min.xyz then max.xyz into min3/max3; 0 for a mesh with
   no vertices (the outputs are left untouched). */
static int local_bounds(ModelEntry *e, float *min3, float *max3) {
    int seen = 0;
    if (!e) return 0;

    /* ONBELLEK. Bu fonksiyon HER KAREDE, her SimpleBoxCollision() cagrisinda
       calisir; ham hali butun vertex'leri gezer (mesh sayisi x vertex sayisi).
       Kutu bir kez hesaplanip kayda yazilir, sonraki kareler O(1) okur.
       Vertex verisi LoadModel'den sonra degismedigi icin bu her zaman
       gecerlidir; release_entry() bayragi sifirlar. */
    if (e->local_bounds_ready) {
        for (int c = 0; c < 3; c++) {
            min3[c] = e->local_min[c];
            max3[c] = e->local_max[c];
        }
        return 1;
    }

    for (int i = 0; i < e->model.meshCount; i++) {
        Mesh m = e->model.meshes[i];
        const float *v = (const float *)m.vertices;
        if (!v || m.vertexCount <= 0) continue;
        for (int k = 0; k < m.vertexCount; k++) {
            float x = v[k * 3 + 0];
            float y = v[k * 3 + 1];
            float z = v[k * 3 + 2];
            if (!seen) {
                min3[0] = max3[0] = x;
                min3[1] = max3[1] = y;
                min3[2] = max3[2] = z;
                seen = 1;
            } else {
                if (x < min3[0]) min3[0] = x; else if (x > max3[0]) max3[0] = x;
                if (y < min3[1]) min3[1] = y; else if (y > max3[1]) max3[1] = y;
                if (z < min3[2]) min3[2] = z; else if (z > max3[2]) max3[2] = z;
            }
        }
    }

    /* Hesaplanan kutuyu kayda yaz; bir sonraki cagri dogrudan buradan okur. */
    if (seen) {
        for (int c = 0; c < 3; c++) {
            e->local_min[c] = min3[c];
            e->local_max[c] = max3[c];
        }
        e->local_bounds_ready = 1;
    }
    return seen;
}

GCL_EXPORT int gcl_simplemesh_world_bounds(double handle_d,
                                           double px, double py, double pz,
                                           double rx, double ry, double rz,
                                           double sx, double sy, double sz,
                                           float *out6) {
    int         handle = (int)handle_d;
    ModelEntry *e      = entry_of(handle);
    float       lo[3], hi[3];
    Matrix      full;
    int         seen = 0;

    if (!out6 || !local_bounds(e, lo, hi)) return 0;

    full = MatrixMultiply(e->base_transform,
                          compose_transform(px, py, pz, rx, ry, rz, sx, sy, sz));

    for (int c = 0; c < 8; c++) {
        Vector3 p = { (c & 1) ? hi[0] : lo[0],
                      (c & 2) ? hi[1] : lo[1],
                      (c & 4) ? hi[2] : lo[2] };
        Vector3 w = Vector3Transform(p, full);
        if (!seen) {
            out6[0] = out6[3] = w.x;
            out6[1] = out6[4] = w.y;
            out6[2] = out6[5] = w.z;
            seen = 1;
        } else {
            if (w.x < out6[0]) out6[0] = w.x; else if (w.x > out6[3]) out6[3] = w.x;
            if (w.y < out6[1]) out6[1] = w.y; else if (w.y > out6[4]) out6[4] = w.y;
            if (w.z < out6[2]) out6[2] = w.z; else if (w.z > out6[5]) out6[5] = w.z;
        }
    }
    return 1;
}

/* ---------- Ucgen erisimi, RaylibSimpleCollision.dll icin ----------

   Kapsul carpismasi (bkz. gcl_SimpleCollision.c) tek tek UCGENLERLE calisir.
   Modelin ve donusum kuralinin sahibi bu modul oldugu icin ucgenler buradan,
   DUNYA uzayinda verilir: collision'in kendi matris carpimini kurmasi
   gerekmez ve CIZILEN geometri ile CARPISILAN geometri ayrisamaz. */

static int mesh_triangle_count(const Mesh *m) {
    if (!m || m->vertexCount <= 0) return 0;
    if (m->triangleCount > 0) return m->triangleCount;
    return m->vertexCount / 3;   /* indekssiz (duz vertex) mesh */
}

static int mesh_triangle_at(const Mesh *m, int t, float out9[9]) {
    const float *v = (const float *)m->vertices;
    int i0, i1, i2, k;

    if (!m || !v || t < 0 || t >= mesh_triangle_count(m)) return 0;

    if (m->indices) {
        i0 = m->indices[t * 3 + 0];
        i1 = m->indices[t * 3 + 1];
        i2 = m->indices[t * 3 + 2];
    } else {
        i0 = t * 3 + 0; i1 = t * 3 + 1; i2 = t * 3 + 2;
    }
    if (i0 >= m->vertexCount || i1 >= m->vertexCount || i2 >= m->vertexCount) return 0;

    for (k = 0; k < 3; k++) {
        out9[k]     = v[i0 * 3 + k];
        out9[3 + k] = v[i1 * 3 + k];
        out9[6 + k] = v[i2 * 3 + k];
    }
    return 1;
}

/* Bir mesh'in ucgen sayisi. Donusum onemsizdir (sayi yerel veriden gelir),
   parametreler arayuzu tek bicimde tutmak icin alinir. */
GCL_EXPORT int gcl_simplemesh_triangle_count(double handle_d,
                                             double px, double py, double pz,
                                             double rx, double ry, double rz,
                                             double sx, double sy, double sz) {
    ModelEntry *e = entry_of((int)handle_d);
    int total = 0;
    (void)px; (void)py; (void)pz; (void)rx; (void)ry; (void)rz;
    (void)sx; (void)sy; (void)sz;
    if (!e) return 0;
    for (int i = 0; i < e->model.meshCount; i++)
        total += mesh_triangle_count(&e->model.meshes[i]);
    return total;
}

/* `index` numarali ucgenin DUNYA uzayindaki uc kosesi (out9: v0.xyz v1.xyz
   v2.xyz). Modelin kendi donusumu ile script'in Position/Rotate/Scale'i
   CIZIMLE AYNI sirada uygulanir. */
GCL_EXPORT int gcl_simplemesh_triangle(double handle_d,
                                       double px, double py, double pz,
                                       double rx, double ry, double rz,
                                       double sx, double sy, double sz,
                                       int index, float *out9) {
    ModelEntry *e = entry_of((int)handle_d);
    Matrix full;
    float  local[9];
    int    t;

    if (!e || !out9 || index < 0) return 0;

    t = index;
    for (int i = 0; i < e->model.meshCount; i++) {
        int count = mesh_triangle_count(&e->model.meshes[i]);
        if (t >= count) { t -= count; continue; }
        if (!mesh_triangle_at(&e->model.meshes[i], t, local)) return 0;

        full = MatrixMultiply(e->base_transform,
                              compose_transform(px, py, pz, rx, ry, rz, sx, sy, sz));
        for (int v = 0; v < 3; v++) {
            Vector3 p = { local[v * 3 + 0], local[v * 3 + 1], local[v * 3 + 2] };
            Vector3 w = Vector3Transform(p, full);
            out9[v * 3 + 0] = w.x; out9[v * 3 + 1] = w.y; out9[v * 3 + 2] = w.z;
        }
        return 1;
    }
    return 0;
}

/* GCL scripts address these members by the names the module documents
   (RaylibSimpleMesh.TerrainLoad, RaylibSimpleMesh.LoadObj, Draw, UnloadObj);
   LastSlot is the runner's read-back channel (store_slots_into_var asks for
   "LastSlot"). The explicit strings keep the script-facing spelling
   independent of the C function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(terrain_load,   "TerrainLoad"),
    E(terrain_unload, "TerrainUnload"),
    E(load_obj,       "LoadObj"),
    E(unload_obj,     "UnloadObj"),
    E(draw,           "Draw"),
    E(last_slot,      "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplemesh_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
