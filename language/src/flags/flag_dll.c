/*
 * flag_dll.c — Shared -dll / -so flags for embedded runtimes
 *
 * Both -luarun and -pyrun use these flags to specify the DLL/SO path.
 * Each runtime has its own default but respects the shared override.
 *
 * Order of precedence:
 *   1. Runtime-specific flag (e.g. -pydll for Python)
 *   2. Shared -dll / -so flag
 *   3. Built-in default
 */

#include "flags.h"
#include <stdio.h>

/* ---- state ---- */
static const char *g_shared_dll = NULL;

/* ------------------------------------------------------- */
/* -dll handler                                           */
/* ------------------------------------------------------- */
static FlagResult handler_dll(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -dll requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_shared_dll = argv[*i];
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
    g_shared_dll = argv[*i];
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* flag_dll_get — get the shared DLL path (may be NULL)    */
/* ------------------------------------------------------- */
const char* flag_dll_get(void) {
    return g_shared_dll;
}

/* ------------------------------------------------------- */
/* flag_dll_init — register -dll and -so flags             */
/* ------------------------------------------------------- */
void flag_dll_init(void) {
    Flag f;

    f = (Flag){
        .name        = "dll",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to shared library DLL (for -luarun or -pyrun)",
        .handler     = handler_dll,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "so",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to shared library SO (alias for -dll)",
        .handler     = handler_so,
        .needs_value = true
    };
    flag_register(f);
}
