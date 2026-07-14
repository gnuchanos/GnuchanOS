/* ============================================================
 * py_api.h — Python C API function pointer types
 *
 * All Python C API functions used by the pyrun runner are
 * declared as function-pointer typedefs here. They are resolved
 * at runtime from python314.dll / libpython3.14.so via py_loader.
 * ============================================================ */

#ifndef PY_API_H
#define PY_API_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Core Python API function pointers ---- */

/* Initialize/finalize */
typedef void    (*Py_Initialize_t)(void);
typedef int     (*Py_IsInitialized_t)(void);
typedef void    (*Py_Finalize_t)(void);
typedef int     (*Py_FinalizeEx_t)(void);

/* Running code */
typedef int     (*PyRun_SimpleString_t)(const char *command);
typedef int     (*PyRun_SimpleFile_t)(FILE *fp, const char *filename);

/* Version / info */
typedef const char* (*Py_GetVersion_t)(void);
typedef const char* (*Py_GetCopyright_t)(void);

/* ---- Struct holding all resolved function pointers ---- */
typedef struct PythonAPI {
/* Core lifecycle */
    Py_Initialize_t        Py_Initialize;
    Py_IsInitialized_t     Py_IsInitialized;
    Py_Finalize_t          Py_Finalize;
    Py_FinalizeEx_t        Py_FinalizeEx;

    /* Script execution */
    PyRun_SimpleString_t   PyRun_SimpleString;
    PyRun_SimpleFile_t     PyRun_SimpleFile;

    /* Info */
    Py_GetVersion_t        Py_GetVersion;
    Py_GetCopyright_t      Py_GetCopyright;
} PythonAPI;

#ifdef __cplusplus
}
#endif

#endif /* PY_API_H */
