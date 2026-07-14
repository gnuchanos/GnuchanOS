/*
 * flag_luarun.c — -luarun flag for running Lua scripts
 *
 * -luarun <file.lua>          run a Lua script
 * -ldll   <path.dll>          specify Lua DLL path (overrides shared -dll)
 * -lso    <path.so>           specify Lua SO path (alias for -ldll)
 *
 * These are separate flags so they can appear in any order.
 * The actual Lua execution is deferred to flag_luarun_execute(),
 * called from main.c after all flags are processed.
 *
 * NOTE: The generic -dll / -so flags are registered in flag_dll.c
 * and shared between -luarun and -pyrun. Use -ldll / -lso for
 * Lua-specific overrides.
 */

#include "flags.h"
#include "luarun_from_dll_so/run_lua.h"
#include <stdio.h>
#include <string.h>

/* ---- state shared between the flag handlers ---- */
static const char *g_lua_script = NULL;       /* set by -luarun          */
static const char *g_lua_dll    = NULL;        /* -ldll / -lso override  */
static int         g_lua_dll_explicit = 0;    /* 1 if user passed -ldll */

/* ------------------------------------------------------- */
/* -ldll handler                                           */
/* ------------------------------------------------------- */
static FlagResult handler_ldll(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -ldll requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_lua_dll = argv[*i];
    g_lua_dll_explicit = 1;
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -lso handler (alias for -ldll)                          */
/* ------------------------------------------------------- */
static FlagResult handler_lso(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -lso requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_lua_dll = argv[*i];
    g_lua_dll_explicit = 1;
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -luarun handler — store script path, defer execution    */
/* ------------------------------------------------------- */
static FlagResult handler_luarun(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -luarun requires a .lua file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_lua_script = argv[*i];
    return FLAG_OK;     /* don't exit yet — let other flags be processed */
}

/* ------------------------------------------------------- */
/* flag_luarun_execute — run the pending Lua script        */
/* Called from main.c after flag_process returns.          */
/* ------------------------------------------------------- */
FlagResult flag_luarun_execute(void) {
    if (!g_lua_script)
        return FLAG_OK;     /* nothing to do */

    /* Determine DLL path: explicit -ldll > shared -dll > default */
    const char *dll_path = g_lua_dll;
    if (!dll_path || !g_lua_dll_explicit)
        dll_path = flag_dll_get();
    if (!dll_path) {
        fprintf(stderr, "error: -luarun requires -ldll or -dll flag\n");
        return FLAG_ERROR;
    }

    int status = run_lua(g_lua_script, dll_path);
    g_lua_script = NULL;    /* prevent double-execution */
    return (status == 0) ? FLAG_EXIT : FLAG_ERROR;
}

/* ------------------------------------------------------- */
/* flag_luarun_init — register flags                       */
/* ------------------------------------------------------- */
void flag_luarun_init(void) {
    Flag f;

    f = (Flag){
        .name        = "luarun",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Run Lua script via lua55.dll/so",
        .handler     = handler_luarun,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "ldll",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to Lua DLL (overrides shared -dll)",
        .handler     = handler_ldll,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "lso",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to Lua SO (alias for -ldll)",
        .handler     = handler_lso,
        .needs_value = true
    };
    flag_register(f);
}
