/*
this is examplefor fps player

#native <RaylibFPS> --> RaylibFPS.dll

# first version
Raylib.FPS Player;

#// properties

Player.Position.x = 300;
Player.Position.y = 300;
Player.Position.z = 300;

Player.Rotate.x = 300; # left+right
Player.Rotate.y = 300; # up+down
Player.Rotate.z = 300; # forward+backward

#// Gravity
Player.IsGravityOn # bool
Player.Gravity     # float

# move
Player.Move() # wasd + space
Player.Look() # mouse


Player.Foward += speed;
Player.Backward += speed;
Player.Left += speed;
Player.Right += speed;

# camera
Player.LookAt = (Raylib.Vector3){0, 0, 0};
Player.LookAt.x = 0;
Player.LookAt.y = 0;
Player.LookAt.z = 0;


# default camera
#// purpose: to be able to do something like a cinematic camera
Raylib.Camera3D CurrentCamera = Player.Camera;

# note: it's camera name not eye or eye height
Player.Camera # there is no height only player height camera is like human head default is 1.8

Raylib.BeginMode3D(CurrentCamera);
Raylib.EndMode3D(void);
*/

/* Backend.

   The player state is not kept here: the runtime flattens a native struct
   argument into numbers in DECLARATION order, and this module publishes the
   updated numbers back through the LastSlot channel (slot i -> scalar leaf i
   of the FPS type, see SharedPipeline/gcl_native_types.c).

   Slot table (must stay in step with the FPS field list):

       0 PosX   1 PosY   2 PosZ
       3 RotX   4 RotY   5 RotZ      RotX yaw (left+right), RotY pitch (up+down)
       6 LookX  7 LookY  8 LookZ     0,0,0 = aim straight down the view direction
       9 Height 10 Gravity 11 IsGravityOn
      12 Forward 13 Backward 14 Left 15 Right   extra speed multipliers
      16 CamPosX 17 CamPosY 18 CamPosZ
      19 CamTgtX 20 CamTgtY 21 CamTgtZ
      22 CamUpX  23 CamUpY  24 CamUpZ
      25 CamFovy 26 CamProjection

   Slots 16..26 are Player.Camera: the Eye/Target/Up/fovy/projection values
   the script hands to Raylib.BeginMode3D(). */

#include "gcl_module.h"

/* windows.h must not drag in wingdi.h/winuser.h: those headers declare
   Rectangle, CloseWindow and ShowCursor, which collide with raylib's own
   names. Only the loader API (GetModuleHandleA/GetProcAddress) is needed. */
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
#include <stdlib.h>
#include <string.h>

#define FPS_SLOT_COUNT       27
#define FPS_STATE_SLOTS      16   /* player state: Position..Right */
#define FPS_DEFAULT_HEIGHT   1.8f /* "camera is like a human head" */
#define FPS_DEFAULT_GRAVITY  9.8f
#define FPS_DEFAULT_FOVY     60.0f
#define FPS_MOVE_SPEED       5.0f /* units per second */
#define FPS_JUMP_SPEED       6.0f /* impulse of SPACE, in units per second */
#define FPS_LOOK_SPEED       0.15f
#define FPS_PITCH_LIMIT      89.0f
#define FPS_DEG2RAD          0.017453292519943295

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

static double g_slot[FPS_SLOT_COUNT];
static float  g_vel_y = 0.0f;
static int    g_cursor_locked = 0;
/* Vertical state across frames: the y we published last frame, and whether the
   player is standing on something. Both are needed because the ground the
   player stands on belongs to another module (RaylibSimpleCollision.dll). */
static double g_published_y = -1.0e9;
static int    g_grounded    = 1;

static double arg(int argc, const char **argv, int i) {
    if (i >= argc || !argv || !argv[i]) return 0.0;
    return atof(argv[i]);
}

static float rad(double degrees) { return (float)(degrees * FPS_DEG2RAD); }

/* Load the flattened player struct; missing trailing fields keep their
   documented default (height 1.8, gravity 9.8, fovy 60). */
static void read_player(int argc, const char **argv) {
    for (int i = 0; i < FPS_SLOT_COUNT; i++) g_slot[i] = arg(argc, argv, i);
    if (!(g_slot[S_HEIGHT] > 0.0))   g_slot[S_HEIGHT]  = FPS_DEFAULT_HEIGHT;
    if (!(g_slot[S_GRAVITY] > 0.0))  g_slot[S_GRAVITY] = FPS_DEFAULT_GRAVITY;
    if (!(g_slot[S_CAM_FOVY] > 0.0)) g_slot[S_CAM_FOVY] = FPS_DEFAULT_FOVY;
}

/* Feet position + player height = eye position. */
static void eye_position(float *x, float *y, float *z) {
    *x = (float)g_slot[S_POS_X];
    *y = (float)g_slot[S_POS_Y] + (float)g_slot[S_HEIGHT];
    *z = (float)g_slot[S_POS_Z];
}

/* Ground plane basis from yaw (Rotate.x).

   forward = (sin yaw, cos yaw): yaw 0 looks down +Z, which is the direction
   build_camera() aims the camera at.

   right is NOT the algebraic transpose of forward here: in a right-handed
   system right = forward x up, so with forward = +Z the screen-right is -X.
   Spelling it out: right = (-cos yaw, sin yaw). Writing it as
   (cos yaw, -sin yaw) mirrors the strafe and is why A/D came out swapped. */
static void basis(float *fx, float *fz, float *rx, float *rz) {
    float yaw = rad(g_slot[S_ROT_X]);
    *fx =  sinf(yaw);  *fz = cosf(yaw);
    *rx = -cosf(yaw);  *rz = sinf(yaw);
}

/* Fill slots 16..26 (Player.Camera) from the player state. */
static void build_camera(void) {
    float ex, ey, ez, fx, fz, sx, sz;
    eye_position(&ex, &ey, &ez);
    basis(&fx, &fz, &sx, &sz);
    (void)sx; (void)sz;

    float pitch = rad(g_slot[S_ROT_Y]);
    float cp    = cosf(pitch);
    float tx = (float)g_slot[S_LOOK_X];
    float ty = (float)g_slot[S_LOOK_Y];
    float tz = (float)g_slot[S_LOOK_Z];
    if (tx == 0.0f && ty == 0.0f && tz == 0.0f) {
        /* no LookAt: look straight down the yaw/pitch direction */
        tx = ex + fx * cp * 10.0f;
        ty = ey + sinf(pitch) * 10.0f;
        tz = ez + fz * cp * 10.0f;
    }
    g_slot[S_CAM_POS_X] = ex; g_slot[S_CAM_POS_Y] = ey; g_slot[S_CAM_POS_Z] = ez;
    g_slot[S_CAM_TGT_X] = tx; g_slot[S_CAM_TGT_Y] = ty; g_slot[S_CAM_TGT_Z] = tz;
    g_slot[S_CAM_UP_X] = 0.0; g_slot[S_CAM_UP_Y] = 1.0; g_slot[S_CAM_UP_Z] = 0.0;
    if (!(g_slot[S_CAM_FOVY] > 0.0)) g_slot[S_CAM_FOVY] = FPS_DEFAULT_FOVY;
    g_slot[S_CAM_PROJECTION] = CAMERA_PERSPECTIVE;
}

/* Raylib.BeginMode3D() with no argument draws with Raylib.dll's own camera
   slot. Publishing the player camera there is optional: it is resolved lazily
   so the module also runs when Raylib.dll is not loaded. */
typedef void (*GclRaylibCameraSet)(double, double, double, double, double, double,
                                   double, double, double, double, double);

#ifdef _WIN32
#include <windows.h>
static GclRaylibCameraSet raylib_camera_set(void) {
    static GclRaylibCameraSet fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("Raylib.dll");
        if (h) fn = (GclRaylibCameraSet)(void *)GetProcAddress(h, "gcl_raylib_camera_set");
    }
    return fn;
}
#else
static GclRaylibCameraSet raylib_camera_set(void) { return NULL; }
#endif

static void push_camera(void) {
    build_camera();
    GclRaylibCameraSet set = raylib_camera_set();
    if (set) {
        set(g_slot[S_CAM_POS_X], g_slot[S_CAM_POS_Y], g_slot[S_CAM_POS_Z],
            g_slot[S_CAM_TGT_X], g_slot[S_CAM_TGT_Y], g_slot[S_CAM_TGT_Z],
            g_slot[S_CAM_UP_X],  g_slot[S_CAM_UP_Y],  g_slot[S_CAM_UP_Z],
            g_slot[S_CAM_FOVY],  g_slot[S_CAM_PROJECTION]);
    }
}

/* Is the terrain collision module part of this program?

   When RaylibSimpleCollision.dll is loaded it OWNS the vertical position: it
   puts the feet on the real surface, and it falls back to the y = 0 plane by
   itself when no mesh is loaded. The flat plane must then NOT also be applied
   here: terrain can sit below y = 0 (pits, valleys), and clamping to the plane
   would hold the player in the air above them, so holes looked like they did
   not exist and walking felt like flying.

   Read every call rather than caching: one GetModuleHandleA per frame is far
   cheaper than the raylib work in the same frame, and it cannot go stale. */
static int collision_module_present(void) {
#ifdef _WIN32
    return GetModuleHandleA("RaylibSimpleCollision.dll") != NULL;
#else
    return 0;
#endif
}

/* Move() — wasd + space. Forward/Backward/Left/Right are extra speed
   multipliers, exactly as the doc comment uses them. */
static double fn_move(int argc, const char **argv) {
    read_player(argc, argv);

    float dt = GetFrameTime();
    if (!(dt > 0.0f)) dt = 1.0f / 60.0f;
    float step = FPS_MOVE_SPEED * dt;

    float fx, fz, rx, rz;
    basis(&fx, &fz, &rx, &rz);

    /* ---------- horizontal: WASD, DIAGONAL-NORMALIZED ----------

       Two keys at once must not be faster than one. Previously each key moved
       the position on its own, so W+D produced a vector of length sqrt(2) and
       the player ran ~41% faster on the diagonal (and W+A+... worse).

       The wish DIRECTION is built from the pressed keys, then normalized to
       unit length; the speed is applied once, afterwards. Normalizing the
       direction and not the final displacement is what keeps the documented
       speed multipliers meaningful: Player.Forward/Backward/Left/Right still
       scale the speed exactly as before (they arrive as 0 by default). */
    int key_fwd = IsKeyDown(KEY_W) ? 1 : 0;
    int key_bwd = IsKeyDown(KEY_S) ? 1 : 0;
    int key_rgt = IsKeyDown(KEY_D) ? 1 : 0;
    int key_lft = IsKeyDown(KEY_A) ? 1 : 0;

    double dir_x = fx * (double)(key_fwd - key_bwd) + rx * (double)(key_rgt - key_lft);
    double dir_z = fz * (double)(key_fwd - key_bwd) + rz * (double)(key_rgt - key_lft);

    double dir_len = sqrt(dir_x * dir_x + dir_z * dir_z);
    if (dir_len > 1.0) { dir_x /= dir_len; dir_z /= dir_len; }

    /* The speed multiplier of whichever directions are held; the largest one
       wins, so a diagonal never stacks two multipliers into double speed. */
    double boost = 1.0;
    if (key_fwd) { double b = 1.0 + g_slot[S_FORWARD];  if (b > boost) boost = b; }
    if (key_bwd) { double b = 1.0 + g_slot[S_BACKWARD]; if (b > boost) boost = b; }
    if (key_rgt) { double b = 1.0 + g_slot[S_RIGHT];    if (b > boost) boost = b; }
    if (key_lft) { double b = 1.0 + g_slot[S_LEFT];     if (b > boost) boost = b; }

    if (dir_len > 0.0) {
        g_slot[S_POS_X] += dir_x * (double)step * boost;
        g_slot[S_POS_Z] += dir_z * (double)step * boost;
    }

    /* ---------- vertical: gravity + SPACE jump ----------

       SPACE is an IMPULSE, not a lift: it sets the upward speed once and the
       arc follows from gravity. Gravity therefore has to run whenever the
       player is in the air, not only under `IsGravityOn` — a jump on an
       otherwise gravity-free scene would never come back down.

       "Am I standing on something?" cannot be answered here, because the
       ground belongs to RaylibSimpleCollision.dll. That module snaps the feet
       onto the surface, so the y it hands back next frame is HIGHER than the
       value this module published while falling: that single sign is the
       landing (and the uphill step) signal. It also covers the very first
       frame, because g_published_y starts far below. */
    double y_in = g_slot[S_POS_Y];
    if (y_in > g_published_y + 1.0e-5 && g_vel_y < 0.0f) {
        g_vel_y = 0.0f;          /* landed / resting on the surface */
        g_grounded = 1;
    } else if (g_vel_y > 0.0f) {
        g_grounded = 0;          /* rising: not on the ground any more */
    }

    if (IsKeyDown(KEY_SPACE) && g_grounded) {
        g_vel_y = FPS_JUMP_SPEED;
        g_grounded = 0;
    }

    g_vel_y -= (float)g_slot[S_GRAVITY] * dt;
    g_slot[S_POS_Y] = y_in + (double)g_vel_y * (double)dt;

    /* No collision module in the scene: the ground plane stays y = 0, which is
       the documented "empty scene" floor. With the collision module present the
       surface (possibly below y = 0) is the floor, so this clamp is skipped —
       see collision_module_present(). */
    if (!collision_module_present() && g_slot[S_POS_Y] < 0.0) {
        g_slot[S_POS_Y] = 0.0;
        g_vel_y = 0.0f;
        g_grounded = 1;
    }

    g_published_y = g_slot[S_POS_Y];

    push_camera();
    return 0.0;
}

/* Look() — mouse. Rotate.x is yaw (left+right), Rotate.y is pitch (up+down). */
static double fn_look(int argc, const char **argv) {
    read_player(argc, argv);

    /* FPS gorunumu: fare pencereye KILITLENIR (ilk Look() cagrisinda bir kez).
       Dokumante edilen sozlesme "Player.Look() # mouse"tur; kilit olmadan fare
       pencereyi terk eder, raylib delta uretmez ve bakis donmez. DisableCursor
       hem imleci gizler hem de onu her karede pencere merkezine geri cekerek
       sonsuz donusu mumkun kilar. */
    if (!g_cursor_locked) { DisableCursor(); g_cursor_locked = 1; }

    /* Fare sag (delta.x > 0) SAGA bakmali, yani yaw AZALMALI: artan yaw
       forward'i +Z'den +X'e dondurur ve bu, ekranda SOLA donustur (sag vektor
       -X'tir; bkz. basis()). Ayni sekilde raylib'in y ekseni ASAGI buyur, bu
       yuzden asagi fare (delta.y > 0) asagi bakmali, yani pitch AZALMALI:
       pitch > 0 yukari bakar (build_camera: ty = ey + sin(pitch) * 10).
       Ikisi de isareti cevrilmeden TERS calisiyordu. */
    Vector2 delta = GetMouseDelta();
    g_slot[S_ROT_X] -= (double)delta.x * FPS_LOOK_SPEED;
    g_slot[S_ROT_Y] -= (double)delta.y * FPS_LOOK_SPEED;
    if (g_slot[S_ROT_Y] >  FPS_PITCH_LIMIT) g_slot[S_ROT_Y] =  FPS_PITCH_LIMIT;
    if (g_slot[S_ROT_Y] < -FPS_PITCH_LIMIT) g_slot[S_ROT_Y] = -FPS_PITCH_LIMIT;

    push_camera();
    return 0.0;
}

/* LastSlot(): no argument -> how many slots this module publishes;
   LastSlot(i) -> slot i, or 0 when out of range. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)FPS_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= FPS_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* GCL scripts address these members by the names the module documents
   (Player.Move(), Player.Look()); LastSlot is the runner's read-back
   channel (call_native_member and store_slots_into_var both ask for
   "LastSlot"). The explicit string keeps the script-facing spelling
   independent of the C function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(move,      "Move"),
    E(look,      "Look"),
    E(last_slot, "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibfps_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
