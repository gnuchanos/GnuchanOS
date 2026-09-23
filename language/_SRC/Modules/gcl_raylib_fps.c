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
Player.Move() # wasd + space, SHIFT = sprint, CTRL = crouch
Player.Look() # mouse

#// SHIFT holds to sprint: faster, and the field of view widens slightly so
#// the speed is visible. CTRL holds to crouch: the camera sinks and the
#// player moves slowly. Crouching disables sprinting — holding both runs at
#// crouch speed.

#// IN WATER, Move() SWIMS. Gravity and buoyancy are both off, so letting go
#// of the keys holds your depth. W/S swim along the view direction INCLUDING
#// pitch — look down and hold W to dive. SPACE and CTRL push straight up and
#// down. A/D strafe and never change depth. The head cannot break the
#// surface: you rise to the waterline and float there.

Player.Foward += speed;
Player.Backward += speed;
Player.Left += speed;
Player.Right += speed;

# camera
Player.LookAt = (Raylib.Vector3){0, 0, 0};
Player.LookAt.x = 0;
Player.LookAt.y = 0;
Player.LookAt.z = 0;

# getposition
RaylibFPS.GetPositionX(p),
RaylibFPS.GetPositionY(p),
RaylibFPS.GetPositionZ(p));

# default camera
#// purpose: to be able to do something like a cinematic camera
RaylibFPS.Camera3D CurrentCamera = Player.Camera;
RaylibFPS.Height # camera height

# note: it's camera name not eye or eye height
RaylibFPS.Camera # there is no height only player height camera is like human head default is 1.8

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

/* ---------- Sprint (SHIFT) and crouch (CTRL) ----------

   SHIFT sprints, CTRL crouches, and the two are MUTUALLY EXCLUSIVE: a crouched
   player has no stride to sprint with, so holding SHIFT while crouched must
   not speed anything up.

   Sprinting also widens the field of view a little. That widening is what
   sells the sensation of speed — the edges of the screen slide past faster
   while the centre stays readable. It is applied to the CAMERA only: feet,
   collision and hit detection all keep working on the unchanged player
   position, so the effect can never push the player through geometry.

   Crouching lowers the EYE, not the feet: the position slots stay on the
   ground (collision keeps working) and only the camera sinks. */
#define FPS_RUN_MULTIPLIER     1.75f  /* sprint speed, relative to walking   */
#define FPS_CROUCH_SPEED       0.45f  /* crouch speed, relative to walking   */
/* How far the EYE sinks when fully crouched, in METRES — not a fraction of
   the body. The player keeps standing on the same spot (collision is
   untouched); only the camera is lowered, so the crouch is purely visual. */
#define FPS_CROUCH_EYE_DROP    0.45f
/* The eye is never allowed closer than this to the feet, so crouching cannot
   push the camera into the ground on a short body. */
#define FPS_CROUCH_EYE_MIN     0.30f
#define FPS_RUN_FOVY_BOOST     7.0f   /* extra fovy degrees while sprinting  */
#define FPS_STANCE_BLEND       12.0f  /* crouch/sprint blend rate, per second */
/* Smallest crouch/sprint blend step allowed in one frame. This is the floor
   that keeps the stance moving when the frame time reads as zero or near zero;
   without it the blend factor is 0 and holding CTRL does nothing at all. */
#define FPS_STANCE_MIN_STEP    0.08f

/* ---------- YUZME (Half-Life modeli) ----------

   ESKI HATA ("tekne gibi yuzme"): kaldirma kuvveti yercekiminden buyuktu
   (buoyancy 1.7 > 1.0, yani net YUKARI kuvvet), bu yuzden suya giren oyuncu
   yuzeye firliyor ve orada saliniyordu. Kaldirma TAMAMEN kaldirildi.

   Yeni kural:
     - YERCEKIMI DE KALDIRMA DA YOK. Tus birakilinca oyuncu bulundugu
       DERINLIKTE kalir; kendiliginden ne cikar ne batar.
     - W/S BAKIS YONUNDE yuzer (PITCH DAHIL): asagi bakip W = dal, yukari
       bakip W = cik. Dik dalmanin tek yolu budur.
     - SPACE yukari, CTRL asagi iter; bakis yonunden bagimsizdir.
     - A/D yan kaydirir, ASLA dikey bilesen uretmez.
     - KAFA YUZEYI GECEMEZ: yuzeye kadar cikip orada yuzersin. Sudan tamamen
       cikis, zeminin yukselmesiyle olur (collision ayaklari kaldirir).

   HEDEF HIZ + DIRENC: ivme biriktirilmez. Her karede hedef hiz hesaplanir ve
   mevcut hiz ona DRAG oraniyla yaklastirilir; boylece hiz sinirsiz buyumez ve
   tus birakilinca kisa surede durur. */
#define FPS_SWIM_MOVE_SPEED   3.2f   /* yuzme hizi (karada 5.0)             */
#define FPS_SWIM_UP_SPEED     2.6f   /* SPACE: duz yukari, birim/sn         */
#define FPS_SWIM_DOWN_SPEED   2.6f   /* CTRL : duz asagi, birim/sn          */
#define FPS_SWIM_DRAG         7.0f   /* hedef hiza yaklasma orani (/sn)     */
/* GIRIS/CIKIS ESIKLERI BURADA YASAMAZ. Yuzme karari artik SU modulunun
   (bkz. gcl_SimpleWater.c: BodyHeight / SwimDepth + histerezis) ve FPS yalnizca
   sorar. Iki modul kendi esigini tutsaydi, script'in `water.FPS_IsSwimming()`
   cevabi ile fizigin kullandigi cevap ayrisirdi: ekran "yuzuyorum" derken
   karakter yuruyebilir, ya da tersi. */

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
/* Vertical state across frames: the y published last frame, and whether the
   player is standing on something. Both are needed because the ground the
   player stands on belongs to another module (RaylibSimpleCollision.dll). */
static double g_published_y = -1.0e9;
static int    g_grounded    = 1;
/* Bu karede YUZME modunda miyiz. Move() doldurur; dikey kod bunu gorup kara
   fizigi yerine yuzme modelini uygular. Giris/cikis HISTEREZISLIDIR
   (bkz. swim_decision) — tek esikle oyuncu yuzeyde titrer. */
static int    g_swimming_now = 0;
/* Gecen kare yuzuyor muyduk? Sudan CIKIS anini yakalamak icin: yuzmeden kalan
   dikey hiz kara fizigine sizarsa oyuncu sudan firlayip zipliyor gibi
   gorunur; cikista bir kez sifirlanir. */
static int    g_was_swimming = 0;

/* Stance, BLENDED rather than switched: 0 = standing, 1 = fully crouched.
   Standing up and crouching each take a fraction of a second, so the camera
   does not teleport between the two eye heights. */
static float  g_crouch    = 0.0f;
/* Sprint blend: 0 = walking fovy, 1 = the full sprint fovy. Blended for the
   same reason — a fovy that snaps reads as a glitch, not as acceleration. */
static float  g_sprint    = 0.0f;
/* The fovy the SCRIPT asked for, kept apart from the value this module
   publishes. The read-back channel copies the slots into the player struct, so
   the widened fovy comes straight back in as the next frame's base; without
   this split it would grow by FPS_RUN_FOVY_BOOST on every single frame. */
static float  g_base_fovy = FPS_DEFAULT_FOVY;
static float  g_sent_fovy = 0.0f;
/* Same idea for the player HEIGHT. Height is the camera height AND the body
   length: ducking means publishing a SMALLER Height, not moving a private
   offset the script can never see. The value the script asked for is kept here
   because the read-back channel copies the published (ducked) height into the
   player struct — without remembering the standing height, each frame would
   shrink from the previous frame's ducked value and the player would sink
   through the floor. */
static float  g_base_height = FPS_DEFAULT_HEIGHT;
static float  g_sent_height = 0.0f;

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

/* Feet position + player height = eye position.

   Height is BOTH the body length and the camera height, so there is no second
   offset here: ducking simply publishes a smaller Height (see update_stance)
   and the camera follows from it. Keeping one number means the script can read
   the ducked value back off Player.Height with no extra member. */
static void eye_position(float *x, float *y, float *z) {
    *x = (float)g_slot[S_POS_X];
    *y = (float)g_slot[S_POS_Y] + (float)g_slot[S_HEIGHT];
    *z = (float)g_slot[S_POS_Z];
}

/* Publish the camera fovy: the script's value plus the sprint widening.

   The base is remembered module-side because the read-back channel copies the
   slots into the player struct, so the widened value would come straight back
   in as the next frame's base and the fovy would climb on every frame. Only a
   value this module did NOT publish can have come from the script, and that is
   the one that updates the base. */
static void apply_fovy(void) {
    float incoming = (float)g_slot[S_CAM_FOVY];
    if (!(incoming > 0.0f)) incoming = FPS_DEFAULT_FOVY;
    if (fabsf(incoming - g_sent_fovy) > 0.001f) g_base_fovy = incoming;
    g_slot[S_CAM_FOVY] = (double)(g_base_fovy + FPS_RUN_FOVY_BOOST * g_sprint);
    g_sent_fovy = (float)g_slot[S_CAM_FOVY];
}

/* Blend the stance towards whatever is held this frame.

   The step is frame-rate compensated AND clamped from below.

   `1 - exp(-k*dt)` is the textbook frame-rate independent factor, but it
   collapses to zero when the frame time reads as zero or near zero — and a
   zero step is precisely what makes crouching look like it does nothing at
   all when CTRL is held. The step is therefore floored at
   FPS_STANCE_MIN_STEP, so the stance ALWAYS moves towards what is held no
   matter what the frame timer reports. */
static void update_stance(float dt, int want_crouch, int want_sprint) {
    float blend = FPS_STANCE_BLEND * dt;
    float incoming;
    float ducked;

    if (blend > 1.0f) blend = 1.0f;
    if (!(blend > FPS_STANCE_MIN_STEP)) blend = FPS_STANCE_MIN_STEP;
    g_crouch += ((want_crouch ? 1.0f : 0.0f) - g_crouch) * blend;
    g_sprint += ((want_sprint ? 1.0f : 0.0f) - g_sprint) * blend;

    /* ---------- DUCK: publish the shrunken Height ----------

       Height IS the camera height (and the body length), so this one
       assignment is the whole duck: eye_position() adds Height to the feet and
       the view drops with it. Nothing else moves — the feet stay on the
       surface, so collision keeps working.

       The script's own Height is the base. The value this module published
       last frame comes back through the same slot, so it is recognised by
       comparing with g_sent_height and NOT taken as a new base; without that
       the height would shrink from its own output every frame and the player
       would sink through the floor. */
    incoming = (float)g_slot[S_HEIGHT];
    if (!(incoming > 0.0f)) incoming = FPS_DEFAULT_HEIGHT;
    if (fabsf(incoming - g_sent_height) > 0.0005f) g_base_height = incoming;

    ducked = g_base_height - FPS_CROUCH_EYE_DROP * g_crouch;
    if (ducked < FPS_CROUCH_EYE_MIN) ducked = FPS_CROUCH_EYE_MIN;

    g_slot[S_HEIGHT] = (double)ducked;
    g_sent_height    = ducked;
}

/* Walking speed of the current stance. Crouching wins over sprinting: the two
   are mutually exclusive, and a crouched player has no stride to sprint with. */
static double stance_speed(int crouching, int sprinting) {
    if (crouching) return (double)FPS_CROUCH_SPEED;
    if (sprinting) return (double)FPS_RUN_MULTIPLIER;
    return 1.0;
}

/* ------------------------------------------------------------------
   Su sorgulari. Suyun yeri ve derinligi bu modulde BILINMEZ; hepsi
   RaylibSimpleWater.dll'den sorulur. O modul yuklu degilse (su kullanmayan
   bir sahne) her sorgu 0 doner ve kara fizigi AYNEN surer.
   ------------------------------------------------------------------ */

/* TEK KAPI: `gcl_water_fps_probe(x, feet, z, eye, ...)`.

   NEDEN (x, z) SART: "suda miyim" bir ALAN sorusudur. Su modulu alani
   (x, z) noktasindan asagi atilan isinlarla tarar ve hacmin ICINDE olmayi
   arar (bkz. gcl_SimpleWater.c: water_area_at). Eski surum yalnizca ayak/goz
   YUKSEKLIGINI geciriyordu; bu yuzden suyun DISINDA, arazinin cukurunda duran
   oyuncu da "suda" sayiliyor, FPS yercekimini kapatiyor ve karakter cukurda
   havada asili kaliyordu — "terrain collision sorun cikariyor" hatasinin
   kaynagi buydu.

   Girdiler: konumun uc bileseninin TAMAMI (x, ayak y, z) ve goz yuksekligi.
   Ciktilar: vucut/goz suda mi, derinlik, su YUZEYI ve TABANI. Yuzey ve taban
   da buradan gelir; FPS ikinci bir sembole (surface/bottom) ihtiyac duymaz,
   boylece iki sorgu ayrisamaz. */
typedef int (*GclWaterProbeFn)(float, float, float *, float *);

static GclWaterProbeFn water_probe_fn(void) {
#ifdef _WIN32
    static GclWaterProbeFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("RaylibSimpleWater.dll");
        if (h) fn = (GclWaterProbeFn)(void *)GetProcAddress(h, "gcl_water_volume_probe");
    }
    return fn;
#else
    return NULL;
#endif
}

/* Su sorgusunun tam cevabi. */
typedef struct {
    float surface, bottom, feet, eye, depth;
    int   body, swim, under;
} FpsWater;

/* ---------- YUZME ESIGI (BURADA) ----------

   Yuzme karari BURADA verilir; su modulu (RaylibSimpleWater) YALNIZCA
   geometri doner (yuzey + taban). Esigi su modulunde tutmak, ekranin
   "yuzuyorum" dedigi an ile fizigin yuzdugu ani birbirinden ayirirdi:
   karar TEK bir yerde verilmelidir ve o yer oyuncunun kendi moduludur.

   Yercekimi suda kapalidir; oyuncuyu askida tutan sey bu esiktir. Kapsulun
   bu kadari battiginda (varsayilan 1.8'in %60'i = ~1.08 birim) yuzme modu
   acilir. Tek esik olsaydi oyuncu tam sinirda her karede yuzme<->yurume
   arasinda gidip gelip titrerdi; bu yuzden CIKIS esigi girisinkinden bir
   miktar SIGDIR (histerezis). Durum MODULDE tutulur cunku karari veren de
   bu moduldur. */
#define FPS_SWIM_SUBMERGE     0.60f
#define FPS_SWIM_EXIT_MARGIN  0.15f

/* Histerezis durumu. Suyun DISINDA her sorgu bunu SIFIRLAR, boylece suya
   yeniden giris tam esikten olur. */
static int g_swim_state = 0;

/* Su seviyesine gore vucut/goz/yuzme durumu; girdi yalnizca YUZEYDIR. */
static void water_classify(FpsWater *w) {
    double depth  = (double)w->surface - (double)w->feet;
    double swim_d = (double)g_slot[S_HEIGHT] * (double)FPS_SWIM_SUBMERGE;

    if (depth < 0.0) depth = 0.0;

    w->body  = (w->feet < w->surface) ? 1 : 0;
    w->under = (w->eye  < w->surface) ? 1 : 0;
    w->depth = (float)depth;

    if (g_swim_state) g_swim_state = (depth > swim_d - FPS_SWIM_EXIT_MARGIN) ? 1 : 0;
    else              g_swim_state = (depth >= swim_d) ? 1 : 0;
    w->swim  = g_swim_state;
}

/* Oyuncunun SUYU, YATAY KONUMU dahil. 1 = su bolgesinin icinde (alan),
   0 = su yok. `out` her durumda sifirlanir, boylece sudan cikan kare bayatl
   deger kullanmaz. */
static int water_query(FpsWater *out) {
    GclWaterProbeFn fn;
    if (!out) return 0;
    out->surface = out->bottom = 0.0f;
    out->body = out->swim = out->under = 0;
    out->feet = (float)g_slot[S_POS_Y];
    out->eye  = out->feet + (float)g_slot[S_HEIGHT];
    out->depth = 0.0f;

    fn = water_probe_fn();
    if (!fn) return 0;
    if (!fn((float)g_slot[S_POS_X], (float)g_slot[S_POS_Z],
            &out->surface, &out->bottom)) {
        g_swim_state = 0;   /* suyun disinda: sonraki giris tam esikten */
        return 0;
    }
    water_classify(out);
    return 1;
}

/* ARAZI TABANI SORGUSU BURADA YOKTUR.

   Yuzme sirasinda yercekimi kapalidir; karakteri deniz dibinden YUKARIDA
   tutan sey artik bir "en alt yukseklik" kelepcesi DEGIL, arazi collision
   modulunun kendi kapsul-ucgen cozumudur (bkz. gcl_SimpleCollision.c:
   TerrainCollision). Arazi bir UCGEN KUMESIDIR — magaralar, tavanlar ve
   cikintilar olabilir; "ayagin altindaki en yuksek yuzey" diye bir kelepce
   magaranin tavanini zemin sanip oyuncuyu yukari isinlardi. Bu yuzden burada
   boyle bir sorgu YOKTUR: dusey cozum tek bir yerde, collision modulunde
   yapilir. */

/* SUYA GIRIS/CIKIS karari SU MODULUNDE verilir (BodyHeight / SwimDepth +
   histerezis, bkz. gcl_SimpleWater.c: water_classify). Burada YALNIZCA
   sonuc okunur: `water_query()->swim`. Iki modul kendi esigini tutsaydi,
   script'in `water.FPS_IsSwimming()` cevabi ile fizigin kullandigi cevap
   ayrisirdi. */
/* Ground plane basis from yaw (Rotate.x).

   forward = (sin yaw, cos yaw): yaw 0 looks down +Z, which is the direction
   build_camera() aims the camera at.

   right is NOT the algebraic transpose of forward here: in a right-handed
   system right = forward x up, so with forward = +Z the screen-right is -X.
   Spelling it out: right = (-cos yaw, sin yaw). Writing it as
   (cos yaw, -sin yaw) would mirror the strafe, which is why A/D came out
   swapped before. */
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
    apply_fovy();
    g_slot[S_CAM_PROJECTION] = CAMERA_PERSPECTIVE;
}

/* Raylib.BeginMode3D() with no argument draws with Raylib.dll's own camera
   slot. Publishing the player camera there is optional: it is resolved lazily
   so the module also runs when Raylib.dll is not loaded. */
typedef void (*GclRaylibCameraSet)(double, double, double, double, double, double,
                                   double, double, double, double, double);

static GclRaylibCameraSet raylib_camera_set(void) {
#ifdef _WIN32
    static GclRaylibCameraSet fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("Raylib.dll");
        if (h) fn = (GclRaylibCameraSet)(void *)GetProcAddress(h, "gcl_raylib_camera_set");
    }
    return fn;
#else
    return NULL;
#endif
}

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

/* Sudan CIKIS temizligi. Yuzmeden kalan DIKEY hiz kara fizigine sizarsa
   oyuncu sudan firlayip zipliyor gibi gorunur; karaya cikista bir kez
   sifirlanir. */
static void leave_water(void) {
    if (g_was_swimming) {
        g_vel_y        = 0.0f;
        g_grounded     = 0;
        g_was_swimming = 0;
    }
}

/* Yuzme modundaki hareket.

   YATAY: WASD duzlemde. A/D ASLA dikey bilesen uretmez; uretseydi yan
   kayarken istenmeden dalardin.

   DIKEY: hedef hiz = bakis yonundeki W/S bileseni + SPACE/CTRL itkisi.
          Bakis bileseni sinf(pitch) ile olceklenir: asagi bakip W basmak
          daldirir, yukari bakip W basmak cikarir. Mevcut hiz bu hedefe
          FPS_SWIM_DRAG oraniyla cekilir; ivme biriktirilmez.

   YUZEY KILIDI: kafa yuzeyi GECEMEZ. Yuzeye kadar cikilir ve orada yuzulur;
   sudan firlamak yoktur. Sudan tamamen cikis, zeminin yukselmesiyle olur:
   collision ayaklari kaldirir, goz yuzeyin ustune cikar ve histerezis
   yuzmeyi birakir. */
static void swim_move(float dt, int key_fwd, int key_bwd,
                      float dir_x, float dir_z, float dir_len,
                      float boost, float surface_y) {
    (void)surface_y;
    float k = FPS_SWIM_DRAG * dt;
    if (k > 1.0f) k = 1.0f;

    /* --- yatay: ayni yon vektoru, yuzme hiziyla --- */
    if (dir_len > 0.0f) {
        float step = FPS_SWIM_MOVE_SPEED * dt;
        g_slot[S_POS_X] += (double)(dir_x * step * boost);
        g_slot[S_POS_Z] += (double)(dir_z * step * boost);
    }

    /* --- dikey: hedef hiz + direnc --- */
    {
        float look_up  = sinf(rad(g_slot[S_ROT_Y]));  /* +1 yukari, -1 asagi */
        float wish     = (float)(key_fwd - key_bwd);  /* W:+1, S:-1          */
        float target_y = look_up * wish * FPS_SWIM_MOVE_SPEED;

        if (IsKeyDown(KEY_SPACE)) target_y += FPS_SWIM_UP_SPEED;
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
            target_y -= FPS_SWIM_DOWN_SPEED;

        g_vel_y += (target_y - g_vel_y) * k;
    }

    g_grounded     = 0;
    g_was_swimming = 1;

    /* DIKEY: once yuzme hizini uygula, sonra tavan kontrolu.

       `start_y` KRITIK. Kelepce YALNIZCA yuzme hiziyla yukari CIKARKEN
       uygulanir: oyuncu yuzeye kadar cikar ve orada yuzer, sudan firlamaz.

       Zemin collision'i oyuncuyu zaten tavanin USTUNE kaldirmissa kelepce
       DEVREYE GIRMEZ — kiradan cikarken tam olarak bu olur. Aksi halde iki
       modul her karede birbirini yer: zemin yukari iter, tavan asagi basar ve
       oyuncu su kenarinda TAKILI kalir. Yatayda zeminin sahibi collision,
       dikeyde de son soz ona aittir. */
    {
        double start_y = g_slot[S_POS_Y];
        double max_feet;

        g_slot[S_POS_Y] = start_y + (double)g_vel_y * (double)dt;

        max_feet = (double)surface_y - g_slot[S_HEIGHT];
        if (start_y <= max_feet && g_slot[S_POS_Y] > max_feet) {
            g_slot[S_POS_Y] = max_feet;
            if (g_vel_y > 0.0f) g_vel_y = 0.0f;
        }
    }

    g_published_y = g_slot[S_POS_Y];

    push_camera();
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

       Two keys at once must not be faster than one. W+D would otherwise give
       a vector of length sqrt(2) and the player would run ~41% faster on the
       diagonal. The wish DIRECTION is normalized and the speed is applied
       once, so Player.Forward/Backward/Left/Right still scale speed exactly
       as documented. */
    int key_fwd = IsKeyDown(KEY_W) ? 1 : 0;
    int key_bwd = IsKeyDown(KEY_S) ? 1 : 0;
    int key_rgt = IsKeyDown(KEY_D) ? 1 : 0;
    int key_lft = IsKeyDown(KEY_A) ? 1 : 0;

    float dir_x = fx * (float)(key_fwd - key_bwd) + rx * (float)(key_rgt - key_lft);
    float dir_z = fz * (float)(key_fwd - key_bwd) + rz * (float)(key_rgt - key_lft);

    float dir_len = sqrtf(dir_x * dir_x + dir_z * dir_z);
    if (dir_len > 1.0f) { dir_x /= dir_len; dir_z /= dir_len; }

    /* The speed multiplier of whichever directions are held; the largest one
       wins, so a diagonal never stacks two multipliers into double speed. */
    float boost = 1.0f;
    if (key_fwd) { float b = (float)(1.0 + g_slot[S_FORWARD]);  if (b > boost) boost = b; }
    if (key_bwd) { float b = (float)(1.0 + g_slot[S_BACKWARD]); if (b > boost) boost = b; }
    if (key_rgt) { float b = (float)(1.0 + g_slot[S_RIGHT]);    if (b > boost) boost = b; }
    if (key_lft) { float b = (float)(1.0 + g_slot[S_LEFT]);     if (b > boost) boost = b; }

    /* ---------- stance: CTRL crouches, SHIFT sprints ----------

       Sprinting requires actually MOVING. Holding SHIFT while standing still
       must not widen the fovy, or the view lurches every time the key is
       tapped. Crouching cancels sprinting outright — the two are mutually
       exclusive, so `sprinting` can never be true while CTRL is down. */
    int crouching = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) ? 1 : 0;
    int sprinting = ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && !crouching && dir_len > 0.0f) ? 1 : 0;
    update_stance(dt, crouching, sprinting);
    double speed = stance_speed(crouching, sprinting);

    /* ---------- SU KARARI ----------

       Suyun konumu, yuzeyi ve "yuzuyor muyum" karari SU MODULUNDEN gelir
       (bkz. gcl_SimpleWater.c). Karar (x, z) ALANINI da hesaba katar; suyun
       DISINDA bir karar 0'dir ve kara fizigi AYNEN surer. Su modulu yuklu
       degilse sorgu 0 doner ve yine kara fizigi surer. */
    {
        FpsWater water;
        int has_water = water_query(&water);

        g_swimming_now = has_water ? water.swim : 0;

        if (g_swimming_now) {
            swim_move(dt, key_fwd, key_bwd, dir_x, dir_z, dir_len, boost,
                      water.surface);
            return 0.0;
        }
    }
    leave_water();

    if (dir_len > 0.0f) {
        g_slot[S_POS_X] += (double)dir_x * (double)step * (double)boost * speed;
        g_slot[S_POS_Z] += (double)dir_z * (double)step * (double)boost * speed;
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
       Kilit olmadan fare pencereyi terk eder, raylib delta uretmez ve bakis
       donmez. DisableCursor hem imleci gizler hem de onu her karede pencere
       merkezine geri cekerek sonsuz donusu mumkun kilar. */
    if (!g_cursor_locked) { DisableCursor(); g_cursor_locked = 1; }

    /* Fare sag (delta.x > 0) SAGA bakmali, yani yaw AZALMALI: artan yaw
       forward'i +Z'den +X'e dondurur ve bu, ekranda SOLA donustur (sag vektor
       -X'tir; bkz. basis()). Ayni sekilde raylib'in y ekseni ASAGI buyur, bu
       yuzden asagi fare (delta.y > 0) asagi bakmali, yani pitch AZALMALI:
       pitch > 0 yukari bakar (build_camera: ty = ey + sin(pitch) * 10). */
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

/* ---------- handle-based player positions ----------

   The struct API above carries ONE player through the slot channel. A script
   that wants to keep several positions around, or that just wants to poke at
   numbers without declaring a struct, uses these members instead:

       int p = RaylibFPS.Create();
       RaylibFPS.SetPosition(p, 1, 2, 3);
       RaylibFPS.GetPositionX(p);      // 1

   This pool is deliberately SEPARATE from the slot channel: the two APIs would
   otherwise overwrite each other's numbers every frame. Create() hands out a
   handle, and every handle keeps its own position until the program ends. */
#define FPS_MAX_PLAYERS 16

static double g_player_pos[FPS_MAX_PLAYERS][3];
static int    g_player_used[FPS_MAX_PLAYERS];

static double fn_create(int argc, const char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < FPS_MAX_PLAYERS; i++) {
        if (g_player_used[i]) continue;
        g_player_used[i] = 1;
        g_player_pos[i][0] = 0.0;
        g_player_pos[i][1] = 0.0;
        g_player_pos[i][2] = 0.0;
        return (double)i;
    }
    return -1.0;   /* pool exhausted */
}

static double fn_set_position(int argc, const char **argv) {
    int i = (int)arg(argc, argv, 0);
    if (i < 0 || i >= FPS_MAX_PLAYERS || !g_player_used[i]) return 0.0;
    g_player_pos[i][0] = arg(argc, argv, 1);
    g_player_pos[i][1] = arg(argc, argv, 2);
    g_player_pos[i][2] = arg(argc, argv, 3);
    return 0.0;
}

/* Axis read-back: GetPositionX/Y/Z(handle) -> that coordinate, 0 for an
   unknown handle. One helper keeps the three members from drifting apart. */
static double player_axis(int argc, const char **argv, int axis) {
    int i = (int)arg(argc, argv, 0);
    if (i < 0 || i >= FPS_MAX_PLAYERS || !g_player_used[i]) return 0.0;
    return g_player_pos[i][axis];
}

static double fn_get_position_x(int argc, const char **argv) { return player_axis(argc, argv, 0); }
static double fn_get_position_y(int argc, const char **argv) { return player_axis(argc, argv, 1); }
static double fn_get_position_z(int argc, const char **argv) { return player_axis(argc, argv, 2); }

/* Cross-module helper (EXPORTED, called from RaylibSimpleWater.dll).

   `gcl_water_fps_state` — oyuncunun SUYA NE KADAR GIRDIGI.

   NEDEN GEREKLI: su alti ekran perdesi "goz yuzeyin altinda mi" diye
   soruyordu. Ama yuzme modunda KAFA YUZEYE KILITLIDIR (bkz. swim_move:
   `max_feet = surface - Height`), yani goz yuzeyin ALTINA hic inmez. O soru
   bu yuzden HER ZAMAN "hayir" cevabini verir ve perde hic inmez —
   "suya giriyorum ama hicbir sey olmuyor" diye bildirilen hal.

   Oyuncunun suya girdigini gosteren sey GOZUN degil GOVDENIN batmasidir. Bu
   olcum burada zaten her karede yapiliyor (bkz. water_classify); su modulu
   ayni sayilari ikinci kez hesaplamasin diye PAYLASILIR. Iki modul ayni
   esikleri kullanir, boylece ekranin "suya girdim" dedigi an ile fizigin
   suya girdigi an ayrisamaz.

   Donus: 1 = out8 yazildi, 0 = o noktada su yok (ya da su modulu yuklu
   degil). Sira: surface, bottom, feet, eye, height, depth, body, swim. */
GCL_EXPORT int gcl_water_fps_state(float *out8) {
    FpsWater w;
    if (!out8) return 0;
    if (!water_query(&w)) return 0;
    out8[0] = w.surface;
    out8[1] = w.bottom;
    out8[2] = w.feet;
    out8[3] = w.eye;
    out8[4] = (float)g_slot[S_HEIGHT];
    out8[5] = w.depth;
    out8[6] = (float)w.body;
    out8[7] = (float)w.swim;
    return 1;
}

/* GCL scripts address these members by the names the module documents
   (Player.Move(), Player.Look()); LastSlot is the runner's read-back
   channel (call_native_member and store_slots_into_var both ask for
   "LastSlot"). The explicit string keeps the script-facing spelling
   independent of the C function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(move,           "Move"),
    E(look,           "Look"),
    E(create,         "Create"),
    E(set_position,   "SetPosition"),
    E(get_position_x, "GetPositionX"),
    E(get_position_y, "GetPositionY"),
    E(get_position_z, "GetPositionZ"),
    E(last_slot,      "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibfps_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
