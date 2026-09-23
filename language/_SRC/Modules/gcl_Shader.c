/*
#native <RaylibShader>

RaylibShader.SimpleShader shader = RaylibShader.CreateSimpleShader("lighting.vs", "lighting.fs", GLSL_VERSION);

Raylib.BeginMode3D(CurrentCamera);
    Raylib.BeginShaderMode(shader);
        Terrain.Draw();
    Raylib.EndShaderMode();
Raylib.EndMode3D(void);

*/

/* Backend.

   CreateSimpleShader(vsFileName, fsFileName, glslVersion) compiles the pair as
   ONE raylib shader and returns the handle the script keeps in
   `SimpleShader.Handle`; the same handle is what `Raylib.BeginShaderMode(shader)`
   takes, because the program itself is registered inside Raylib.dll — see
   `gcl_raylib_simple_shader()` there.

   The GL program MUST live in Raylib.dll: a second module embedding its own
   raylib copy would create a second GL state, and BeginShaderMode would then
   bind a program the drawing module knows nothing about. This module therefore
   only forwards to the exported helper and never calls a GL entry point itself.

   Slot table (must stay in step with the `SimpleShader` field list in
   SharedPipeline/gcl_native_types.c):

       0 Handle

   `Handle` is also a named member: the runtime's declaration path
   (`fill_native_from_last_slot`) reads the freshly created value back through
   NAMED accessors, so `RaylibShader.SimpleShader s = CreateSimpleShader(...)`
   fills s.Handle even though the call returns a plain number.

   `gcl_shader_current()` is exported for RaylibSimpleLight.dll: a light needs
   the shader whose uniforms it feeds, and the scripts never spell that link
   out, so the shader that was created last is the one in use. */

#include "gcl_module.h"

/* windows.h must not drag in wingdi.h/winuser.h: their Rectangle/CloseWindow
   clash with raylib's names. Only the loader API is needed here. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

#include <stdlib.h>
#include <string.h>

#define SHADER_SLOT_COUNT 1
#define SHADER_DEFAULT_GLSL 330

enum { S_HANDLE = 0 };

static double g_slot[SHADER_SLOT_COUNT] = { -1.0 };

/* ---------- Raylib.dll: the helper that owns the GL program ---------- */

typedef int (*GclSimpleShaderFn)(const char *vs_file, const char *fs_file, int glsl_version);

#ifdef _WIN32
static GclSimpleShaderFn raylib_simple_shader(void) {
    static GclSimpleShaderFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        HMODULE h = GetModuleHandleA("Raylib.dll");
        if (h) fn = (GclSimpleShaderFn)(void *)GetProcAddress(h, "gcl_raylib_simple_shader");
    }
    return fn;
}
#else
static GclSimpleShaderFn raylib_simple_shader(void) { return NULL; }
#endif

/* ---------- members ---------- */

/* CreateSimpleShader(vsFileName, fsFileName, glslVersion) -> handle. */
static double fn_create_simple_shader(int argc, const char **argv) {
    const char *vs_file = (argc > 0 && argv[0]) ? argv[0] : "";
    const char *fs_file = (argc > 1 && argv[1]) ? argv[1] : "";
    int glsl_version    = (argc > 2 && argv[2] && argv[2][0]) ? atoi(argv[2]) : SHADER_DEFAULT_GLSL;
    GclSimpleShaderFn load = raylib_simple_shader();
    double handle = -1.0;

    if (load && vs_file[0] && fs_file[0]) handle = (double)load(vs_file, fs_file, glsl_version);
    g_slot[S_HANDLE] = handle;
    return handle;
}

/* Handle() -> the shader created last (the runtime's read-back accessor). */
static double fn_handle(int argc, const char **argv) {
    (void)argc; (void)argv;
    return g_slot[S_HANDLE];
}

/* LastSlot(): no argument -> slot count; LastSlot(i) -> slot i. */
static double fn_last_slot(int argc, const char **argv) {
    if (argc < 1 || !argv || !argv[0]) return (double)SHADER_SLOT_COUNT;
    int i = (int)atof(argv[0]);
    if (i < 0 || i >= SHADER_SLOT_COUNT) return 0.0;
    return g_slot[i];
}

/* The shader the light modules feed (see the header comment). */
GCL_EXPORT int gcl_shader_current(void) {
    return (int)g_slot[S_HANDLE];
}

/* GCL scripts address these members by the names the module documents
   (RaylibShader.CreateSimpleShader); Handle is the declaration read-back
   accessor and LastSlot the generic slot channel. The explicit strings keep
   the script-facing spelling independent of the C function name. */
#define E(NAME, STR) {STR, fn_##NAME}

static const GclNativeEntry g_entries[] = {
    E(create_simple_shader, "CreateSimpleShader"),
    E(handle,               "Handle"),
    E(last_slot,            "LastSlot"),
};

GCL_EXPORT const GclNativeEntry *gcl_raylibshader_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
