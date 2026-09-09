/*
 * gcl_module.h — GCL native modül API'si.
 *
 * simple_doc.md: modüller .dll/.so olarak Library/ altında bulunur.
 * #native <Math> direktifi modülü yükler; Math.member çağrıları modül
 * fonksiyon tablosundan çözülür.
 */

#ifndef GCL_MODULE_H
#define GCL_MODULE_H

#include <stddef.h>

#ifdef _WIN32
#define GCL_EXPORT __declspec(dllexport)
#else
#define GCL_EXPORT __attribute__((visibility("default")))
#endif

/* GCL tarafından geçilen argümanlar string dizisi olarak gelir.
   Sayısal fonksiyonlar atof/strtod ile çevirir; string fonksiyonlar direkt kullanır. */
typedef double (*GclNativeFn)(int argc, const char **argv);

typedef struct {
    const char *name;
    GclNativeFn fn;
} GclNativeEntry;

/* Modül bu fonksiyonu export eder: fonksiyon tablosunu döndürür. */
typedef const GclNativeEntry *(*GclModuleGetFunctions)(int *count);

#endif /* GCL_MODULE_H */
