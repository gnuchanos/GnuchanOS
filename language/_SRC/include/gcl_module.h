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

/* ---------- Host API — modülden GCL'e GERİ ÇAĞRI (callback köprüsü) ----------

   NEDEN: bazı raylib üyeleri bir C FONKSİYON İŞARETÇİSİ alır
   (SetTraceLogCallback, SetLoadFileDataCallback, SetAudioStreamCallback, ...).
   Modüle yalnızca `const char **argv` geldiği için bir GCL fonksiyonu C'ye
   geçirilemiyordu; bu üyeler no-op stub'tı. Artık modül, çağrılacak GCL
   fonksiyonunun ADINI alır ve host API ile onu gerçekten çağırır.

   İŞ PARÇACICI GÜVENLİĞİ: host->call_gcl YALNIZCA ANA iş parçacığından
   çağrılabilir (yorumlayıcının ortamı paylaşılan durumdur). Ses callback'i
   ayrı bir iş parçacığında koştuğu için oradan ASLA çağrılmaz: modül ses
   örneklerini bir halka tamponda üretir, gerçek zamanlı trambolin yalnızca
   tamponu boşaltır, üretim ise `GclModuleTickFn` içinde (ana iş parçacığı)
   host->call_gcl ile yapılır. */

#define GCL_HOST_ABI_VERSION 1

/* GCL fonksiyonuna geçirilecek tek bir argüman. */
typedef struct {
    int          is_string;   /* 0: sayı (num), 1: metin (str) */
    double       num;
    const char  *str;
} GclHostArg;

typedef struct GclHostApi {
    int abi_version;

    /* GCL'de TANIMLI bir fonksiyonu ada göre çağırır (ANA iş parçacığı).
       argv[i].is_string ise parametre metin, değilse sayı olarak bağlanır —
       yorumlayıcı normal çağrılardaki bağlama kuralının aynısını uygular.
       ret: fonksiyonun dönüş değeri (NULL olabilir).
       Dönüş: 0 = çağrıldı, -1 = fonksiyon yok / runner hazır değil. */
    int (*call_gcl)(void *host, const char *fn_name, int argc,
                    const GclHostArg *argv, double *ret);

    void *host;   /* opak — runtime'ın ortam işaretçisi (GclEnv*) */
} GclHostApi;

/* Modül İSTESE bu sembolü ihraç eder; runtime modülü yükledikten hemen sonra
   bir kez çağırır. İhraç etmeyen modüller eskisi gibi çalışır (uyumlu). */
typedef void (*GclModuleSetHostFn)(const GclHostApi *api);

/* Modül İSTESE bu sembolü ihraç eder; runtime HER native modül çağrısından
   önce (ana iş parçacığı) çağırır. Ertelenmiş işler burada üretilir. */
typedef void (*GclModuleTickFn)(const GclHostApi *api);

#endif /* GCL_MODULE_H */
