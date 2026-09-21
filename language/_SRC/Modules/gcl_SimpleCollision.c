/*
RaylibSimpleCollision.TerrainCollision(PLAYER, Terrain);
*/

/* Backend.

   TerrainCollision(player, terrain) takes the two native structs the runtime
   flattens in DECLARATION order (see SharedPipeline/gcl_native_types.c):

       args 0..26   the FPS player   (same slot order as RaylibFPS.dll)
       args 27..29  the terrain      (Material.Texture, Material.Color, Handle)

   The player's feet are placed on the terrain surface under the player, and
   the whole player struct is published back through LastSlot so the runtime
   can write it into the PLAYER variable again. The camera slots (16..26) are
   recomputed here as well, because the script draws immediately after this
   call and would otherwise use the previous frame's camera.

   The surface height comes from RaylibSimpleMesh.dll, which owns the mesh
   (gcl_simplemesh_terrain_height). When that module is not loaded the player
   simply rests on the ground plane y = 0, which is what an empty scene means. */

#include "gcl_module.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COL_PLAYER_SLOTS  27
#define COL_TERRAIN_SLOTS 3
#define COL_SLOT_COUNT    27

#define COL_DEG2RAD       0.017453292519943295
#define COL_NO_HIT        (-1.0e30)

enum {
    S_POS_X = 0, S_POS_Y, S_POS_Z,
    S_ROT_X, S_ROT_Y, S_ROT_Z,
    S_LOOK_X, S_LOOK_Y, S_LOOK_Z,
    S_HEIGHT, S_GRAVITY, S_GRAVITY_ON,
    S_FORWARD, S_BACKWARD, S_LEFT, S_RIGHT,
    S_CAM_POS_X, S_CAM_POS_Y, S_CAM_POS_Z,
    S_CAM_TGT_X, S_CAM_TGT_Y, S_CAM_TGT_Z,
    S_CAM_UP_X, S_CAM_UP_Y, S_CAM_UP_Z,
    S_CAM_FOVY, S_CAM_PROJECTION
};

enum { T_TEXTURE = 0, T_COLOR, T_HANDLE };

static double g_slot[COL_SLOT_COUNT];
static int    g_terrain_handle = -1;

static double num_arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

/* ---------- the mesh module's sampler ---------- */

#ifdef _WIN32
#include <windows.h>

typedef double (*GclTerrainHeightFn)(double handle, double x, double z);

static GclTerrainHeightFn terrain_height_fn(void) {
    static GclTerrainHeightFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("RaylibSimpleMesh.dll");
        if (h) fn = (GclTerrainHeightFn)(void *)GetProcAddress(h, "gcl_simplemesh_terrain_height");
    }
    return fn;
}
#else
typedef double (*GclTerrainHeightFn)(double handle, double x, double z);
static GclTerrainHeightFn terrain_height_fn(void) { return NULL; }
#endif

/* Height of the terrain under (x, z); COL_NO_HIT when nothing is there. */
static double surface_height(double x, double z) {
    GclTerrainHeightFn sample = terrain_height_fn();
    if (!sample || g_terrain_handle < 0) return COL_NO_HIT;
    return sample((double)g_terrain_handle, x, z);
}

/* ---------- read / publish ---------- */

static void read_player(int argc, const char **argv) {
    for (int i = 0; i < COL_PLAYER_SLOTS; i++) g_slot[i] = num_arg(argc, argv, i);
    if (!(g_slot[S_HEIGHT] > 0.0)) g_slot[S_HEIGHT] = 1.8;      /* human head */
    if (!(g_slot[S_GRAVITY] > 0.0)) g_slot[S_GRAVITY] = 9.8;
    if (!(g_slot[S_CAM_FOVY] > 0.0)) g_slot[S_CAM_FOVY] = 60.0;

    /* the terrain struct follows the player in the argument list */
    if (argc > COL_PLAYER_SLOTS + T_HANDLE)
        g_terrain_handle = (int)num_arg(argc, argv, COL_PLAYER_SLOTS + T_HANDLE);
}

/* Camera slots from the (possibly corrected) player state. */
static void build_camera(void) {
    float yaw   = (float)(g_slot[S_ROT_X] * COL_DEG2RAD);
    float pitch = (float)(g_slot[S_ROT_Y] * COL_DEG2RAD);
    float cp    = cosf(pitch);
    float ex = (float)g_slot[S_POS_X];
    float ey = (float)(g_slot[S_POS_Y] + g_slot[S_HEIGHT]);
    float ez = (float)g_slot[S_POS_Z];
    float tx = (float)g_slot[S_LOOK_X];
    float ty = (float)g_slot[S_LOOK_Y];
    float tz = (float)g_slot[S_LOOK_Z];

    if (tx == 0.0f && ty == 0.0f && tz == 0.0f) {
        tx = ex + sinf(yaw) * cp * 10.0f;
        ty = ey + sinf(pitch) * 10.0f;
        tz = ez + cosf(yaw) * cp * 10.0f;
    }
    g_slot[S_CAM_POS_X] = ex; g_slot[S_CAM_POS_Y] = ey; g_slot[S_CAM_POS_Z] = ez;
    g_slot[S_CAM_TGT_X] = tx; g_slot[S_CAM_TGT_Y] = ty; g_slot[S_CAM_TGT_Z] = tz;
    g_slot[S_CAM_UP_X] = 0.0; g_slot[S_CAM_UP_Y] = 1.0; g_slot[S_CAM_UP_Z] = 0.0;
    g_slot[S_CAM_PROJECTION] = 0;   /* CAMERA_PERSPECTIVE */
}

/* ---------- collision ---------- */

/* TerrainCollision(player, terrain) — put the player on the surface. */
static double fn_terrain_collision(int argc, const char **argv) {
    double ground;
    read_player(argc, argv);

    ground = surface_height(g_slot[S_POS_X], g_slot[S_POS_Z]);
    if (ground == COL_NO_HIT) ground = 0.0;          /* no mesh: flat ground */

    /* Oyuncu yüzeyin ALTINDA ya da tam üzerindeyse yüzeye oturur; yüzeyin
       ÜSTÜNDEYKEN (yani zıplarken) dokunulmaz.

       Eski koşul `POS_Y <= ground || (ground - POS_Y) <= STEP_HEIGHT` idi.
       İkinci terim havadaki oyuncu için de doğrudur — (ground - POS_Y)
       negatiftir ve her zaman 0.6'dan küçüktür — bu yüzden zıplamanın
       kazandırdığı her santim aynı karede geri alınıyor ve zıplama hiç
       görünmüyordu. Aşağı çekmek yerine yalnızca düşmeyi engellemek, araziden
       aşağı inerken düşüşü de doğal bırakır. */
    if (g_slot[S_POS_Y] <= ground) g_slot[S_POS_Y] = ground;

    build_camera();
    return 0.0;
}

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)COL_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= COL_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* GCL scripts address these members by the names the module documents
   (RaylibSimpleCollision.TerrainCollision(PLAYER, Terrain)); LastSlot is the
   runner's read-back channel (store_slots_into_var asks for "LastSlot"). The
   explicit strings keep the script-facing spelling independent of the C
   function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(terrain_collision, "TerrainCollision"),
    E(last_slot,         "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibsimplecollision_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
