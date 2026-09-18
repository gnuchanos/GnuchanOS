/*
 * complete_native.c â€” Native module API database (Â§4).
 *
 * Raylib/Raygui members are generated (complete_native_db.c). The Math/Stdio/Embed
 * and native struct field lists (Â§4.5) are kept here by hand.
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
/* Struct field table (4.5) - moved                                     */
/* ------------------------------------------------------------------ */

/* The field lists (Vector2 / Rectangle / Color / Camera3D / ...) and the
   lookup helpers gcl_native_struct() / gcl_native_is_struct() /
   gcl_native_struct_names() now live in:

       SharedPipeline/gcl_native_types.c

   Reason: the interpreter (GCL/SimpleRunner/gcl_runner.c) needs the SAME
   table to build the members of `Raylib.Rectangle r;` - the completion
   engine must not own knowledge the runtime depends on, and two copies of
   forty field lists would silently drift apart. complete_native.h
   re-exports that header, so existing callers are unchanged. */
