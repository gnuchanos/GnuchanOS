/*
#native <RaylibSimpleMesh>

RaylibSimpleMesh.Terrain Terrain = RaylibSimpleMesh.TerrainLoad("terrain.obj");
Terrain.Material.Texture = Raylib.LoadTexture("terrain.png");
Terrain.Material.Color   = Raylib.WHITE;

Terrain.Draw()

RaylibSimpleMesh.TerrainUnload("terrain.obj")

*/

/* Backend.

   TerrainLoad(fileName) loads the file as a raylib Model and keeps it in this
   module's registry; the returned number is the registry index, which is also
   the value the script stores in Terrain.Handle.

   The runtime flattens a native struct argument into numbers in DECLARATION
   order and publishes updates back through the LastSlot channel (slot i ->
   scalar leaf i of the Terrain type, see SharedPipeline/gcl_native_types.c).
   Slot table:

       0 Material.Texture   1 Material.Color   2 Handle

   Terrain.Draw() recomputes the material from slots 0..1 and draws the model
   of slot 2, so the script's assignments take effect.

   TerrainUnload(fileName) frees every registry entry loaded from that file.

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERRAIN_SLOT_COUNT 3
#define TERRAIN_MAX        64
#define TERRAIN_NAME_MAX   512

#define TERRAIN_HIT_NONE (-1.0e30f)

enum { T_TEXTURE = 0, T_COLOR, T_HANDLE };

typedef struct {
    char   file[TERRAIN_NAME_MAX];
    Model  model;
    int    used;
} TerrainEntry;

static TerrainEntry g_terrain[TERRAIN_MAX];
static double       g_slot[TERRAIN_SLOT_COUNT];

static const char *str_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return "";
    return argv[i];
}

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

static unsigned int color_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0xFFFFFFFFu;   /* Raylib.WHITE */
    return (unsigned int)strtoull(argv[i], NULL, 10);
}

static Color unpack_color(unsigned int v) {
    Color c;
    c.r = (unsigned char)(v & 0xFF);
    c.g = (unsigned char)((v >> 8) & 0xFF);
    c.b = (unsigned char)((v >> 16) & 0xFF);
    c.a = (unsigned char)((v >> 24) & 0xFF);
    return c;
}

/* Load the flattened Terrain struct: slots come in declaration order, and a
   missing Material.Texture keeps the model's own default material. */
static void read_terrain(int argc, const char **argv) {
    g_slot[T_TEXTURE] = (argc > T_TEXTURE) ? num_arg(argc, argv, T_TEXTURE) : -1.0;
    g_slot[T_COLOR]   = (argc > T_COLOR)   ? num_arg(argc, argv, T_COLOR)
                                          : (double)0xFFFFFFFFu;
    g_slot[T_HANDLE]  = (argc > T_HANDLE)  ? num_arg(argc, argv, T_HANDLE) : -1.0;
}

static TerrainEntry *entry_of(int handle) {
    if (handle < 0 || handle >= TERRAIN_MAX) return NULL;
    if (!g_terrain[handle].used) return NULL;
    return &g_terrain[handle];
}

static Model model_of(int handle) {
    TerrainEntry *e = entry_of(handle);
    if (!e) return (Model){0};
    return e->model;
}

/* Find a free slot, or reuse the one already loaded from the same file. */
static int take_entry(const char *file) {
    for (int i = 0; i < TERRAIN_MAX; i++) {
        if (g_terrain[i].used &&
            strncmp(g_terrain[i].file, file, TERRAIN_NAME_MAX) == 0)
            return i;
    }
    for (int i = 0; i < TERRAIN_MAX; i++) {
        if (!g_terrain[i].used) return i;
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

/* Apply the script's material to the model: Texture is a Raylib Texture2D
   handle (an int), Color is a packed R|G<<8|B<<16|A<<24 value. Both are stored
   in the model's material 0, which is what DrawModel uses. */
static void apply_material(int handle, double texture, double color) {
    TerrainEntry *e = entry_of(handle);
    int          texture_handle;
    if (!e || e->model.materialCount <= 0) return;

    texture_handle = (int)texture;
    if (texture_handle >= 0) {
        GclRaylibTextureGet get_tex = raylib_texture_get();
        if (get_tex)
            e->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture =
                get_tex(texture_handle);
    }
    e->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = unpack_color((unsigned int)color);
}

/* TerrainLoad(fileName) -> registry index (the script keeps it in Terrain.Handle). */
static double fn_terrain_load(int argc, const char **argv) {
    const char *file = str_arg(argc, argv, 0);
    int slot = take_entry(file);
    Model model;
    if (!file[0] || slot < 0) return -1.0;
    if (g_terrain[slot].used) {
        g_slot[T_HANDLE] = (double)slot;
        return (double)slot;
    }
    model = LoadModel(resolve_file(file));
    if (!model.meshCount) return -1.0;
    g_terrain[slot].model = model;
    g_terrain[slot].used  = 1;
    snprintf(g_terrain[slot].file, TERRAIN_NAME_MAX, "%s", file);
    g_slot[T_HANDLE] = (double)slot;
    return (double)slot;
}

/* TerrainUnload(fileName) — frees the models loaded from that file. */
static double fn_terrain_unload(int argc, const char **argv) {
    const char *file = str_arg(argc, argv, 0);
    for (int i = 0; i < TERRAIN_MAX; i++) {
        if (!g_terrain[i].used) continue;
        if (file[0] && strncmp(g_terrain[i].file, file, TERRAIN_NAME_MAX) != 0) continue;
        UnloadModel(g_terrain[i].model);
        g_terrain[i].used = 0;
        g_terrain[i].file[0] = 0;
    }
    return 0.0;
}

/* Draw() — draws the terrain the flattened struct points at. */
static double fn_draw(int argc, const char **argv) {
    int handle;
    read_terrain(argc, argv);
    handle = (int)g_slot[T_HANDLE];
    if (!entry_of(handle)) return 0.0;
    apply_material(handle, g_slot[T_TEXTURE], g_slot[T_COLOR]);
    DrawModel(g_terrain[handle].model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    return 0.0;
}

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)TERRAIN_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= TERRAIN_SLOT_COUNT) return 0.0;
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
    if (!model.meshCount) return (double)TERRAIN_HIT_NONE;
    Ray ray = { {(float)x, 1000.0f, (float)z}, {0.0f, -1.0f, 0.0f} };
    RayCollision best = {0};
    best.distance = 1.0e30f;
    for (int i = 0; i < model.meshCount; i++) {
        RayCollision hit = GetRayCollisionMesh(ray, model.meshes[i], model.transform);
        if (hit.hit && hit.distance < best.distance) best = hit;
    }
    if (!best.hit) return (double)TERRAIN_HIT_NONE;
    return (double)best.point.y;
}

/* Bounding box of the terrain, for callers that only need its extent. */
GCL_EXPORT int gcl_simplemesh_terrain_bounds(double handle_d, float *out6) {
    int handle = (int)handle_d;
    BoundingBox box;
    if (!out6 || !entry_of(handle)) return 0;
    box = GetModelBoundingBox(g_terrain[handle].model);
    out6[0] = box.min.x; out6[1] = box.min.y; out6[2] = box.min.z;
    out6[3] = box.max.x; out6[4] = box.max.y; out6[5] = box.max.z;
    return 1;
}

/* GCL scripts address these members by the names the module documents
   (RaylibSimpleMesh.TerrainLoad, Terrain.Draw, TerrainUnload); LastSlot is the
   runner's read-back channel (store_slots_into_var asks for "LastSlot"). The
   explicit strings keep the script-facing spelling independent of the C
   function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(terrain_load,   "TerrainLoad"),
    E(terrain_unload, "TerrainUnload"),
    E(draw,           "Draw"),
    E(last_slot,      "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplemesh_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
