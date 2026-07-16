/* ============================================================
 * py_runner.h — Python initialization, script execution, shutdown
 *
 * Wraps the resolved PythonAPI to provide a clean interface for:
 *   - init_python()            → initialize Python interpreter
 *   - run_py_string()          → execute Python code from string
 *   - run_py_file()            → execute a .py file
 *   - print_py_version()       → print Python version info
 *   - finalize_python()        → shutdown Python interpreter
 * ============================================================ */

#ifndef PY_RUNNER_H
#define PY_RUNNER_H

#include "py_api.h"

/**
 * init_python - Initialize the Python interpreter.
 * @home: if non-NULL, call Py_SetPythonHome before initialization.
 * Returns 0 on success, -1 on failure.
 */
int init_python(const PythonAPI *api, const char *home);

/**
 * run_py_string - Execute Python code from a string.
 * @code: Python source code to execute.
 * Returns 0 on success, 1 on error.
 */
int run_py_string(const PythonAPI *api, const char *code);

/**
 * run_py_file - Load and execute a .py file.
 * @path: path to the Python script.
 * Returns 0 on success, 1 on error.
 */
int run_py_file(const PythonAPI *api, const char *path);

/**
 * print_py_version - Print Python version and copyright info to stdout.
 */
void print_py_version(const PythonAPI *api);

/**
 * finalize_python - Shutdown the Python interpreter.
 * Returns 0 on success, -1 on failure.
 */
int finalize_python(const PythonAPI *api);

/**
 * detect_pyhome - Auto-discover PYTHONHOME from DLL path.
 * Called when neither -pyhome nor PYTHONHOME env var are set.
 * @dll_path:   path to the Python DLL
 * @user_home:  the user-provided -pyhome value (may be NULL)
 * Returns a heap-allocated path string, or NULL if detection fails.
 */
char* detect_pyhome(const char *dll_path, const char *user_home);

#endif /* PY_RUNNER_H */
