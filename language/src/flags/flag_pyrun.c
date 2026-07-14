/*
 * flag_pyrun.c — -pyrun flag for running Python scripts
 *
 * -pyrun  <file.py>            run a Python script via python314.dll/so
 * -pydll  <path.dll>           specify Python DLL path (overrides shared -dll)
 * -pyso   <path.so>            specify Python SO path (alias for -pydll)
 * -pyhome <path>               set PYTHONHOME (where Lib/, DLLs/ are located)
 *
 * These are separate flags so they can appear in any order.
 * The actual Python execution is deferred to flag_pyrun_execute(),
 * called from main.c after all flags are processed.
 *
 * NOTE: The generic -dll / -so flags are registered in flag_dll.c
 * and shared between -luarun and -pyrun. Use -pydll / -pyso for
 * Python-specific overrides.
 */

#include "flags.h"
#include "pyrun_from_dll_so/run_py.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- state shared between the flag handlers ---- */
static const char *g_py_script = NULL;        /* set by -pyrun          */
static const char *g_py_dll    = NULL;         /* -pydll / -pyso override */
static int         g_py_dll_explicit = 0;     /* 1 if user passed -pydll */
static const char *g_py_home   = NULL;        /* -pyhome override       */

/* ------------------------------------------------------- */
/* -pydll handler                                          */
/* ------------------------------------------------------- */
static FlagResult handler_pydll(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -pydll requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_py_dll = argv[*i];
    g_py_dll_explicit = 1;
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -pyso handler (alias for -pydll)                       */
/* ------------------------------------------------------- */
static FlagResult handler_pyso(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -pyso requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_py_dll = argv[*i];
    g_py_dll_explicit = 1;
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -pyhome handler — set PYTHONHOME                       */
/* ------------------------------------------------------- */
static FlagResult handler_pyhome(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "error: -pyhome requires a path argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_py_home = argv[*i];
    return FLAG_OK;
}

/* ------------------------------------------------------- */
/* -pyrun handler — store script path, defer execution    */
/* ------------------------------------------------------- */
static FlagResult handler_pyrun(int argc, char *argv[], int *i) {
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        fprintf(stderr, "error: -pyrun requires a .py file argument\n");
        return FLAG_ERROR;
    }
    (*i)++;
    g_py_script = argv[*i];
    return FLAG_OK;     /* don't exit yet — let other flags be processed */
}

/* ------------------------------------------------------- */
/* flag_pyrun_execute — run the pending Python script      */
/* Called from main.c after flag_process returns.          */
/* ------------------------------------------------------- */
FlagResult flag_pyrun_execute(void) {
    if (!g_py_script)
        return FLAG_OK;     /* nothing to do */

    /* Determine DLL path: explicit -pydll > shared -dll > default */
    const char *dll_path = g_py_dll;
    if (!dll_path || !g_py_dll_explicit)
        dll_path = flag_dll_get();
    if (!dll_path) {
        fprintf(stderr, "error: -pyrun requires -pydll or -dll flag\n");
        return FLAG_ERROR;
    }

    int status = run_py(g_py_script, dll_path, g_py_home);
    g_py_script = NULL;     /* prevent double-execution */
    return (status == 0) ? FLAG_EXIT : FLAG_ERROR;
}

/* ------------------------------------------------------- */
/* flag_pyrun_init — register the pyrun flags             */
/* ------------------------------------------------------- */
void flag_pyrun_init(void) {
    Flag f;

    f = (Flag){
        .name        = "pyrun",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Run Python script via python314.dll/so",
        .handler     = handler_pyrun,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "pydll",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to Python DLL (overrides shared -dll)",
        .handler     = handler_pydll,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "pyso",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Path to Python SO (alias for -pydll)",
        .handler     = handler_pyso,
        .needs_value = true
    };
    flag_register(f);

    f = (Flag){
        .name        = "pyhome",
        .alias       = NULL,
        .category    = "Run (Embedded)",
        .description = "Set PYTHONHOME for embedded Python",
        .handler     = handler_pyhome,
        .needs_value = true
    };
    flag_register(f);
}
