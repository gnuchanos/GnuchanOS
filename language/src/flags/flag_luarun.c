/*
 * flag_luarun.c — -luarun, -dll, -so flags
 *
 * -luarun <file.lua>   run a Lua script
 * -dll   <path.dll>    specify Lua DLL path
 * -so    <path.so>     specify Lua SO path (alias for -dll)
 *
 * These are separate flags so they can appear in any order.
 * The actual Lua execution is deferred to flag_luarun_execute(),
 * called from main.c after all flags are processed.
 */

#include "flags.h"
#include "luarun_from_dll_so/run_lua.h"
#include <stdio.h>
#include <string.h>

/* ---- state shared between the three flag handlers ---- */
static const char *g_lua_script = NULL;   /* set by -luarun */
static const char *g_lua_dll    = "dll/lua55.dll";  /* -dll / -so override; default */

/* ------------------------------------------------------- */
/* -dll handler                                           */
/* ------------------------------------------------------- */
static FlagResult handler_dll(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -dll requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_lua_dll = argv[*i];
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -so handler (alias for -dll)                            */
/* ------------------------------------------------------- */
static FlagResult handler_so(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -so requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_lua_dll = argv[*i];
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
    return FLAG_OK;     /* don't exit yet — let other flags (-dll/-so) be processed */
}

/* ------------------------------------------------------- */
/* flag_luarun_execute — run the pending Lua script        */
/* Called from main.c after flag_process returns.          */
/* ------------------------------------------------------- */
FlagResult flag_luarun_execute(void) {
    if (!g_lua_script)
        return FLAG_OK;     /* nothing to do */

    int status = run_lua(g_lua_script, g_lua_dll);
    g_lua_script = NULL;    /* prevent double-execution */
    return (status == 0) ? FLAG_EXIT : FLAG_ERROR;
}

/* ------------------------------------------------------- */
/* flag_luarun_init — register all three flags             */
/* ------------------------------------------------------- */
void flag_luarun_init(void) {
    Flag f;

    f = (Flag){
        .name        = "luarun",
        .alias       = NULL,
        .description = "Run Lua script via lua55.dll/so",
        .handler     = handler_luarun,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "dll",
        .alias       = NULL,
        .description = "Path to Lua DLL (for -luarun)",
        .handler     = handler_dll,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "so",
        .alias       = NULL,
        .description = "Path to Lua SO (alias for -dll)",
        .handler     = handler_so,
        .needs_value = true
    };
    flag_register(f);
}
