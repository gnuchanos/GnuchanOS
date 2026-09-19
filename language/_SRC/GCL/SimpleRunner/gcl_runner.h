/*
 * gcl_runner.h — GCL AST yürütücü (SimpleRunner'a ait).
 */

#ifndef GCL_RUNNER_H
#define GCL_RUNNER_H

#include "gcl_parser.h"
/* TURN 50 - the runner is a PRODUCER of diagnostics, not a printer of strings:
   a runtime failure now carries a code, a span and a source frame, exactly like
   a lexer/parser one (same vocabulary, same renderer). gcl_diag.h is the only
   dependency, and it deliberately drags in nothing but the lexer's token names
   and the source map, so the interpreter stays raylib-free. */
#include "gcl_diag.h"

typedef struct GclEnv GclEnv;

/* ---------- runtime diagnostics ---------- */

/* Tell the runner WHERE its own line/column numbers live. The lexer and the
   parser only ever see the PREPROCESSED buffer, so an AST node's line is a line
   of THAT buffer; the map is what turns it back into the file the user wrote.
   `src` is the fallback when the map cannot resolve a line (and both may be
   NULL, in which case a diagnostic still renders as a header).

   `file` is the DEFAULT file name for a diagnostic the map cannot resolve -
   the same role the first argument of gcl_diag_list_init plays for the lexer
   and the parser. It is what keeps `file: error: ...` from degrading to
   `<input>: error: ...` when a failure has no position at all (the module
   loading loop runs before any statement exists to point at).

   The runner borrows all four and must be called again for every run. */
void gcl_runtime_set_source(const char *file, const char *src, size_t len,
                            const GclSourceMap *map);

/* Every runtime diagnostic of the CURRENT run, in the order it was reported
   (the list is cleared at the start of each gcl_run_program). Never NULL.
   This is the runtime's half of the shared vocabulary: the caller renders it
   with gcl_diag_list_print_mapped(), exactly like the lexer's and the parser's
   list, so a runtime failure carries the same code, span and source frame. */
const GclDiagList *gcl_runtime_diags(void);

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
