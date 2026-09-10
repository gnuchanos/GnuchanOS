/*
 * gcl_module.h — GCL native module API.
 *
 * simple_doc.md: modules are found under Library/ as .dll/.so.
 * The #native <Math> directive loads the module; Math.member calls are
 * resolved from the module function table.
 */

#ifndef GCL_MODULE_H
#define GCL_MODULE_H

#include <stddef.h>

#ifdef _WIN32
#define GCL_EXPORT __declspec(dllexport)
#else
#define GCL_EXPORT __attribute__((visibility("default")))
#endif

/* Arguments passed by GCL arrive as an array of strings.
   Numeric functions parse them with atof/strtod; string functions use them directly. */
typedef double (*GclNativeFn)(int argc, const char **argv);

typedef struct {
    const char *name;
    GclNativeFn fn;
} GclNativeEntry;

/* The module exports this function: it returns the function table. */
typedef const GclNativeEntry *(*GclModuleGetFunctions)(int *count);

#endif /* GCL_MODULE_H */
