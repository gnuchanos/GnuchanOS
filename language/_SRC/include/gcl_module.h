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

/* ---------- finding a symbol ANOTHER module exported ----------

   The modules are not independent: `Raylib` owns the single raylib state (its
   GL program, its camera slot, its texture registry, its asset-path rule) and
   the others — RaylibSimpleMesh, RaylibSkybox, RaylibSimpleLight, RaylibShader,
   RaylibSimpleWater, RaylibSimpleCollision — ask it for what they need instead
   of keeping a second copy. A second copy is exactly what breaks: two raylib
   states, two registries, two cameras that drift apart.

   Windows and ELF reach the same answer through different doors:

     * Windows: load the exporting library by name and GetProcAddress it.
     * ELF: there is ONE global symbol namespace. Modules are dlopen'd with
       RTLD_GLOBAL (see native_load in gcl_runner.c), so dlsym(RTLD_DEFAULT, ...)
       already sees every module's exports. The library name is not consulted —
       a file name has no meaning to the dynamic linker, and a mapping from
       "Raylib.dll" to "Raylib.so" here would be one more table to keep in sync.

   Every caller used to spell the Windows half out for itself and stub the
   other half out with `return NULL` — so on Linux every cross-module feature
   silently degraded: the asset-path rule was not applied (models loaded from
   the wrong directory, as the FPS demo did), the shader the script bound was
   never found, and the light module could not reach the shader it lights. The
   behaviour differed by platform for no reason a script could see. That
   difference now lives here, once. */
#ifdef _WIN32
/* GDI and USER are switched off BEFORE windows.h is read. Their `Rectangle`,
   `CloseWindow` and `ShowCursor` collide by name with raylib's, and a module
   that pulled both in would silently get the wrong one. Nothing that includes
   this header wants them: the two callers that do need GDI (Raygui's file
   dialogs, the IDE's native dialog) do not include this header at all.
   WIN32_LEAN_AND_MEAN and the guards are only set when absent, so a caller
   that already asked for the full headers keeps them. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
static void *gcl_module_symbol(const char *library, const char *name) {
    HMODULE h;
    if (!name || !name[0]) return NULL;
    h = library && library[0] ? GetModuleHandleA(library) : NULL;
    return h ? (void *)GetProcAddress(h, name) : NULL;
}
#else
#include <dlfcn.h>
static void *gcl_module_symbol(const char *library, const char *name) {
    (void)library;   /* one namespace on ELF — the name is not consulted */
    if (!name || !name[0]) return NULL;
    return dlsym(RTLD_DEFAULT, name);
}
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
