/* ============================================================
 * py_runner.c — Implementation: Python initialization,
 *                script execution, and shutdown
 *
 * PYTHONHOME auto-detection:
 *   Python embeddable distribution ships self-contained:
 *     path/to/python3.14.zip   (stdlib)
 *     path/to/python314.dll    (the interpreter DLL)
 *   detect_pyhome() finds PYTHONHOME by looking for these
 *   files relative to the DLL path, with NO dependency on
 *   system registry or installed Python.
 * ============================================================ */

#include "py_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

/* strdup portability */
#if defined(_MSC_VER)
#define strdup_ _strdup
#else
static char* strdup_(const char *s) {
    if (!s) return NULL;
    size_t l = strlen(s) + 1;
    char *d = (char*)malloc(l);
    if (d) memcpy(d, s, l);
    return d;
}
#endif

/* -------------------------------------------------------- */
/* check_dir_exists — 1 if path is an existing directory    */
/* -------------------------------------------------------- */
static int check_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) != 0;
}

/* -------------------------------------------------------- */
/* check_file_exists — 1 if path is an existing file        */
/* -------------------------------------------------------- */
static int check_file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/* -------------------------------------------------------- */
/* check_embeddable_python — 1 if base_dir (length base_len)*/
/* contains a Python stdlib indicator: Lib/encodings/ dir   */
/* OR python3.14.zip file  OR python3.dll (embeddable)      */
/* -------------------------------------------------------- */
static int check_embeddable_python(const char *base, int base_len) {
    char buf[2048];
    if (base_len <= 0 || base_len + 30 > (int)sizeof(buf)) return 0;

    /* Lib/encodings/ directory */
    memcpy(buf, base, base_len);
    memcpy(buf + base_len, "/Lib/encodings", 15);
    if (check_dir_exists(buf)) return 1;

    /* python3.14.zip (embedded stdlib) */
    memcpy(buf, base, base_len);
    memcpy(buf + base_len, "/python3.14.zip", 16);
    if (check_file_exists(buf)) return 1;

#ifdef _WIN32
    /* python3.dll (present in Windows embeddable distro) */
    memcpy(buf, base, base_len);
    memcpy(buf + base_len, "/python3.dll", 12);
    if (check_file_exists(buf)) return 1;
#endif

    return 0;
}

/* -------------------------------------------------------- */
/* detect_pyhome — auto-discover PYTHONHOME                 */
/* Only looks relative to the DLL path. No system deps.     */
/* -------------------------------------------------------- */
char* detect_pyhome(const char *dll_path, const char *user_home) {
    if (user_home && user_home[0]) return NULL;
    const char *env = getenv("PYTHONHOME");
    if (env && env[0]) return NULL;
    if (!dll_path || !dll_path[0]) return NULL;

    char abs_path[2048];
    char dll_dir[2048];
    int dll_dir_len = 0;

#ifdef _WIN32
    DWORD len = GetFullPathNameA(dll_path, (DWORD)sizeof(abs_path), abs_path, NULL);
    if (len == 0 || len >= sizeof(abs_path)) return NULL;
    {
        char *last_slash = strrchr(abs_path, '\\');
        if (!last_slash) last_slash = strrchr(abs_path, '/');
        if (!last_slash) return NULL;
        dll_dir_len = (int)(last_slash - abs_path);
        if (dll_dir_len >= (int)sizeof(dll_dir)) return NULL;
        memcpy(dll_dir, abs_path, dll_dir_len);
        dll_dir[dll_dir_len] = '\0';
    }
#else
    {
        if (!realpath(dll_path, abs_path)) return NULL;
        char *last_slash = strrchr(abs_path, '/');
        if (!last_slash) return NULL;
        dll_dir_len = (int)(last_slash - abs_path);
        if (dll_dir_len >= (int)sizeof(dll_dir)) return NULL;
        memcpy(dll_dir, abs_path, dll_dir_len);
        dll_dir[dll_dir_len] = '\0';
    }
#endif

    /* Strategy 1: DLL's own directory */
    if (check_embeddable_python(dll_dir, dll_dir_len))
        return strdup_(dll_dir);

    /* Strategy 2: Parent directory */
    {
        char *last_sep = strrchr(dll_dir, '/');
#ifdef _WIN32
        if (!last_sep) last_sep = strrchr(dll_dir, '\\');
#endif
        if (last_sep) {
            int parent_len = (int)(last_sep - dll_dir);
            if (check_embeddable_python(dll_dir, parent_len)) {
                char *p = (char*)malloc(parent_len + 1);
                if (p) { memcpy(p, dll_dir, parent_len); p[parent_len] = '\0'; }
                return p;
            }
        }
    }

    return NULL;
}


/* ================================================================ */
/*                    Python lifecycle functions                     */
/* ================================================================ */

/* ------------------------------------------------------------------ */
/* init_python — Initialize the Python interpreter                    */
/* ------------------------------------------------------------------ */
int init_python(const PythonAPI *api, const char *home) {
    if (!api || !api->Py_Initialize) {
        fprintf(stderr, "Error: PythonAPI not initialized\n");
        return -1;
    }

    if (api->Py_IsInitialized && api->Py_IsInitialized()) {
        return 0;
    }

    /* Set PYTHONHOME so Python finds its stdlib
     * (buf stays allocated — putenv keeps the pointer) */
    if (home && home[0]) {
        size_t len = 12 + strlen(home) + 1;
        char *buf = (char*)malloc(len);
        if (buf) {
            snprintf(buf, len, "PYTHONHOME=%s", home);
            putenv(buf);
        }
    }

    api->Py_Initialize();

    if (api->Py_IsInitialized && !api->Py_IsInitialized()) {
        fprintf(stderr, "Error: Py_Initialize() failed\n");
        fprintf(stderr, "Hint: Ship Python embeddable package with your app.\n");
        fprintf(stderr, "      Place python3.14.zip alongside the DLL.\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* run_py_string — Execute Python code from a string                  */
/* ------------------------------------------------------------------ */
int run_py_string(const PythonAPI *api, const char *code) {
    if (!api || !api->PyRun_SimpleString || !code) {
        fprintf(stderr, "Error: invalid arguments to run_py_string\n");
        return 1;
    }

    int result = api->PyRun_SimpleString(code);
    if (result != 0) {
        fprintf(stderr, "Error executing Python code (exit=%d)\n", result);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* run_py_file — Load and execute a .py file                          */
/* ------------------------------------------------------------------ */
int run_py_file(const PythonAPI *api, const char *path) {
    if (!api || !api->PyRun_SimpleFile || !path) {
        fprintf(stderr, "Error: invalid arguments to run_py_file\n");
        return 1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: could not open '%s'\n", path);
        return 1;
    }

    int result = api->PyRun_SimpleFile(fp, path);
    fclose(fp);

    if (result != 0) {
        fprintf(stderr, "Error executing Python file (exit=%d)\n", result);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* print_py_version — Print Python version info                       */
/* ------------------------------------------------------------------ */
void print_py_version(const PythonAPI *api) {
    if (!api) return;
    if (api->Py_GetVersion) {
        printf("%s\n", api->Py_GetVersion());
    }
}

/* ------------------------------------------------------------------ */
/* finalize_python — Shutdown the Python interpreter                  */
/* ------------------------------------------------------------------ */
int finalize_python(const PythonAPI *api) {
    if (!api) return -1;
    if (!api->Py_IsInitialized || !api->Py_IsInitialized())
        return 0;

    if (api->Py_FinalizeEx) {
        int result = api->Py_FinalizeEx();
        if (result < 0) return -1;
    } else if (api->Py_Finalize) {
        api->Py_Finalize();
    }
    return 0;
}
