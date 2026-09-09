/*
 * gcl_runner.h — GCL AST yürütücü (SimpleRunner'a ait).
 */

#ifndef GCL_RUNNER_H
#define GCL_RUNNER_H

#include "gcl_parser.h"

typedef struct GclEnv GclEnv;

/* #extern <dll> — harici DLL fonksiyon parametre tipleri */
typedef enum {
    GCL_EXT_VOID = 0,
    GCL_EXT_INT,
    GCL_EXT_DOUBLE,
    GCL_EXT_STRING,   /* const char * */
} GclExternType;

/* #extern <raylib.dll> — harici DLL tanımı */
typedef struct {
    char *dll_name;
    void *handle;   /* yüklü handle (runtime) */
} GclExternDll;

/* #register void InitWindow(int, int, const char*) — harici fonksiyon kaydı */
typedef struct {
    char *dll_name;       /* kayıt hangi DLL'e ait */
    char *func_name;      /* InitWindow */
    GclExternType ret;    /* dönüş tipi */
    GclExternType params[8];
    int param_count;
    void *fn_ptr;         /* GetProcAddress/dlsym sonucu (runtime) */
} GclExternReg;

/* programı çalıştırır; çıktı stdout'a yazılır.
   base_dir: #include ve modül (.dll/.so) arama dizini.
    native_modules: #native <Ad> ile bildirilen modül adları (Library'in altındaki .dll veya .so).
    NOTE: #lib/.gclib support removed; no lib_modules parameter.
   extern_dlls: #extern <dll> ile bildirilen harici DLL'ler (izole pointer'lar).
   extern_regs: #register fonksiyon kayıtları (izole pointer'lar).
   Başarı: 0, hata: -1 */
int gcl_run_program(
    GclProgram *prog,
    const char *base_dir,
    const char **native_modules,
    int native_count,
    const char **extern_dlls,
    int extern_dll_count,
    const char **extern_regs,
    int extern_reg_count,
    int argc,
    char **argv
);

#endif /* GCL_RUNNER_H */
