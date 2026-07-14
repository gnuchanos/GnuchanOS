/* ============================================================
 * py_embed.c — Implementation of the high-level Python embed API
 *
 * Wraps py_loader (load/resolve) and py_runner (init, script
 * execution, finalize) into a single PythonEmbed struct for
 * easy use by flag_pyrun.c.
 * ============================================================ */

#include "py_embed.h"
#include "py_loader.h"
#include "py_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* py_load — load DLL, resolve API, populate embed struct              */
/* ------------------------------------------------------------------ */
PythonEmbed* py_load(const char *dll_path) {
    if (!dll_path) {
        fprintf(stderr, "Error: py_load: NULL dll_path\n");
        return NULL;
    }

    PythonEmbed *embed = (PythonEmbed*)calloc(1, sizeof(PythonEmbed));
    if (!embed) {
        fprintf(stderr, "Error: py_load: out of memory\n");
        return NULL;
    }

    /* Load the library */
    if (load_py_library(dll_path, &embed->lib) != 0) {
        fprintf(stderr, "Error: py_load: failed to load '%s'\n", dll_path);
        free(embed);
        return NULL;
    }

    /* Resolve all Python API symbols */
    if (resolve_py_api(embed->lib, &embed->api) != 0) {
        fprintf(stderr, "Error: py_load: symbol resolution failed\n");
        unload_py_library(embed->lib);
        free(embed);
        return NULL;
    }

    /* Populate convenience pointers */
    embed->Py_Initialize      = embed->api.Py_Initialize;
    embed->Py_IsInitialized   = embed->api.Py_IsInitialized;
    embed->Py_Finalize        = embed->api.Py_Finalize;
    embed->Py_FinalizeEx      = embed->api.Py_FinalizeEx;
    embed->PyRun_SimpleString = embed->api.PyRun_SimpleString;
    embed->PyRun_SimpleFile   = embed->api.PyRun_SimpleFile;
    embed->Py_GetVersion      = embed->api.Py_GetVersion;
    embed->Py_GetCopyright    = embed->api.Py_GetCopyright;

    return embed;
}

/* ------------------------------------------------------------------ */
/* py_sethome — set Python home path before initialization            */
/* ------------------------------------------------------------------ */
void py_sethome(PythonEmbed *embed, const char *home) {
    if (!embed) return;
    if (embed->home_path) {
        free(embed->home_path);
    }
    embed->home_path = home ? strdup(home) : NULL;
}

/* ------------------------------------------------------------------ */
/* py_init — initialize Python interpreter                            */
/* ------------------------------------------------------------------ */
int py_init(PythonEmbed *embed) {
    if (!embed) return -1;
    return init_python(&embed->api, embed->home_path);
}

/* ------------------------------------------------------------------ */
/* py_runfile — load and run a .py file                               */
/* ------------------------------------------------------------------ */
int py_runfile(PythonEmbed *embed, const char *path) {
    if (!embed) return 1;
    return run_py_file(&embed->api, path);
}

/* ------------------------------------------------------------------ */
/* py_runstr — execute Python code from a string                      */
/* ------------------------------------------------------------------ */
int py_runstr(PythonEmbed *embed, const char *code) {
    if (!embed) return 1;
    return run_py_string(&embed->api, code);
}

/* ------------------------------------------------------------------ */
/* py_version — print Python version info                             */
/* ------------------------------------------------------------------ */
void py_version(PythonEmbed *embed) {
    if (!embed) return;
    print_py_version(&embed->api);
}

/* ------------------------------------------------------------------ */
/* py_finalize — shutdown Python interpreter                          */
/* ------------------------------------------------------------------ */
int py_finalize(PythonEmbed *embed) {
    if (!embed) return -1;
    return finalize_python(&embed->api);
}

/* ------------------------------------------------------------------ */
/* py_unload — finalize Python (if running) and unload DLL             */
/* ------------------------------------------------------------------ */
void py_unload(PythonEmbed *embed) {
    if (!embed) return;

    /* Finalize Python if initialized */
    finalize_python(&embed->api);

    /* Unload the library */
    if (embed->lib) {
        unload_py_library(embed->lib);
        embed->lib = NULL;
    }

    /* Free home path */
    if (embed->home_path) {
        free(embed->home_path);
        embed->home_path = NULL;
    }

    free(embed);
}
