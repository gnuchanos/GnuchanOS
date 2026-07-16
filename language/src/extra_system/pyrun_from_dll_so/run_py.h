/* ============================================================
 * run_py.h — Python script runner entry point
 *
 * Called by flag_pyrun.c after argument parsing.  Delegates
 * to py_embed (which wraps py_loader + py_runner).
 *
 * Usage from gcl:
 *   gcl -pyrun <script.py> -dll <path/python314.dll>
 *   gcl -pyrun <script.py> -so  <path/libpython3.14.so>
 * ============================================================ */

#ifndef RUN_PY_H
#define RUN_PY_H

/**
 * run_py - Load python314.dll/so, initialize Python, run script.
 * @script:    path to the .py file to execute.
 * @dll_path:  path to python314.dll or libpython3.14.so.
 * @home:      Python home directory (where Lib/, DLLs/ are), or NULL.
 *
 * Returns 0 on success, 1 on failure (error printed to stderr).
 */
int run_py(const char *script, const char *dll_path, const char *home);

#endif /* RUN_PY_H */
