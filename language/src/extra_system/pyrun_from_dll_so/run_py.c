/* ============================================================
 * run_py.c — Python script runner entry point
 *
 * Called by flag_pyrun.c after argument parsing.  Delegates
 * to py_embed (which wraps py_loader + py_runner).
 *
 * Usage from gcl:
 *   gcl -pyrun <script.py> -pydll <path/python314.dll>
 *   gcl -pyrun <script.py> -pyso <path/libpython3.14.so>
 *   gcl -pyrun <script.py> -pyhome <PythonHome>
 * ============================================================ */

#include "run_py.h"
#include "py_embed.h"
#include "py_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run_py(const char *script, const char *dll_path, const char *home) {
    /* Load python314.dll/so */
    PythonEmbed *py = py_load(dll_path);
    if (!py) {
        fprintf(stderr, "error: failed to load '%s'\n", dll_path);
        return 1;
    }

    /* Set Python home path:
     * 1. Use -pyhome flag if provided
     * 2. Otherwise check PYTHONHOME env var
     * 3. Auto-detect from DLL path
     */
    if (home) {
        py_sethome(py, home);
    } else {
        const char *env_home = getenv("PYTHONHOME");
        if (env_home && env_home[0]) {
            py_sethome(py, env_home);
        } else {
            char *auto_home = detect_pyhome(dll_path, NULL);
            if (auto_home) {
                py_sethome(py, auto_home);
                free(auto_home);
            }
        }
    }

    /* Initialize Python interpreter */
    if (py_init(py) != 0) {
        fprintf(stderr, "error: failed to initialize Python\n");
        py_unload(py);
        return 1;
    }

    /* Run the script */
    int status = py_runfile(py, script);
    if (status != 0) {
        fprintf(stderr, "error running '%s'\n", script);
        py_unload(py);
        return 1;
    }

    /* Cleanup */
    py_unload(py);
    return 0;
}
