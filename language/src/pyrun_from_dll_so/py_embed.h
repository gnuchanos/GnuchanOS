/* ============================================================
 * py_embed.h — High-level embed API for python314.dll/so
 *
 * Provides a single-header convenience interface wrapping
 * py_loader and py_runner. Used by flag_pyrun.c.
 *
 * Functions:
 *   py_load(path)         → PythonEmbed*  (load DLL, resolve API)
 *   py_sethome(embed, h)  → void          (set Python home path)
 *   py_init(embed)        → int           (init Python interpreter)
 *   py_runfile(embed, p)  → int           (run .py file)
 *   py_runstr(embed, s)   → int           (run Python string)
 *   py_version(embed)     → void          (print version info)
 *   py_finalize(embed)    → void          (shutdown Python)
 *   py_unload(embed)      → void          (finalize + unload DLL)
 * ============================================================ */

#ifndef PY_EMBED_H
#define PY_EMBED_H

#include "py_api.h"
#include "py_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Opaque struct holding loaded DLL handle + resolved API ---- */
typedef struct PythonEmbed {
    LIB_HANDLE lib;               /* HMODULE or void*              */
    PythonAPI  api;               /* resolved function pointers     */

    /* Convenience pointers */
    Py_Initialize_t        Py_Initialize;
    Py_IsInitialized_t     Py_IsInitialized;
    Py_Finalize_t          Py_Finalize;
    Py_FinalizeEx_t        Py_FinalizeEx;
    PyRun_SimpleString_t   PyRun_SimpleString;
    PyRun_SimpleFile_t     PyRun_SimpleFile;
    Py_GetVersion_t        Py_GetVersion;
    Py_GetCopyright_t      Py_GetCopyright;

    /* Python home path (set before Py_Initialize) */
    char                   *home_path;
} PythonEmbed;

/* ---- Lifecycle ---- */

/**
 * py_load - Load python314.dll/so and resolve all Python C API symbols.
 * @dll_path: filesystem path to the shared library.
 * Returns a heap-allocated PythonEmbed* on success, NULL on failure.
 */
PythonEmbed* py_load(const char *dll_path);

/**
 * py_sethome - Set Python home path (where Lib/, DLLs/ are located).
 * Must be called before py_init.
 * @embed: initialized PythonEmbed from py_load.
 * @home:  Python home directory path.
 */
void py_sethome(PythonEmbed *embed, const char *home);

/**
 * py_init - Initialize the Python interpreter.
 * @embed: initialized PythonEmbed from py_load.
 * Returns 0 on success, -1 on failure.
 */
int py_init(PythonEmbed *embed);

/**
 * py_runfile - Load and execute a .py file.
 * @embed: initialized PythonEmbed.
 * @path:  path to the Python script.
 * Returns 0 on success, 1 on failure.
 */
int py_runfile(PythonEmbed *embed, const char *path);

/**
 * py_runstr - Execute Python code from a string.
 * @embed: initialized PythonEmbed.
 * @code:  Python source code string.
 * Returns 0 on success, 1 on failure.
 */
int py_runstr(PythonEmbed *embed, const char *code);

/**
 * py_version - Print Python version and copyright info.
 */
void py_version(PythonEmbed *embed);

/**
 * py_finalize - Shutdown the Python interpreter (if initialized).
 * Returns 0 on success, -1 on failure.
 */
int py_finalize(PythonEmbed *embed);

/**
 * py_unload - Finalize Python (if running) and unload the DLL.
 * @embed: PythonEmbed to tear down (is freed, do not reuse).
 */
void py_unload(PythonEmbed *embed);

#ifdef __cplusplus
}
#endif

#endif /* PY_EMBED_H */
