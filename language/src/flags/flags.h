#ifndef FLAGS_H
#define FLAGS_H

#include <stdbool.h>

/* ── Flag result codes ─────────────────────────────────────────── */
typedef enum {
    FLAG_OK = 0,       /* flag handled, continue processing          */
    FLAG_EXIT,         /* flag handled, exit program                 */
    FLAG_ERROR         /* flag error, exit with message              */
} FlagResult;

/* ── Flag handler signature ────────────────────────────────────── */
/* Receives argc, argv, and a pointer to the current index i.      */
/* The handler may increment i if it consumes additional arguments. */
typedef FlagResult (*FlagHandler)(int argc, char* argv[], int* i);

/* ── Flag descriptor ───────────────────────────────────────────── */
typedef struct {
    const char* name;         /* long name  e.g. "version"           */
    const char* alias;        /* single-char alias e.g. "v" (or NULL)*/
    const char* category;     /* group name in help output, or NULL */
    const char* description;  /* help text line                      */
    FlagHandler handler;
    bool needs_value;         /* true if a following arg is expected */
} Flag;

/* ── Registry API ──────────────────────────────────────────────── */
void          flag_register(const Flag f);
FlagResult    flag_process(int argc, char* argv[]);
void          flag_print_help(void);
const Flag*   flag_find(const char* name);

/* ── Module init functions ─────────────────────────────────────── */
void flag_version_init(void);
void flag_help_init(void);
void flag_run_init(void);
void flag_build_init(void);
void flag_lexer_init(void);
void flag_parser_init(void);
void flag_ast_init(void);
void flag_ir_init(void);
void flag_codegen_init(void);
void flag_all_flags_init(void);
void flag_linclude_init(void);
void flag_llib_init(void);
void flag_lextend_init(void);
void flag_wasm_init(void);
void flag_debug_init(void);
void flag_dll_init(void);
const char* flag_dll_get(void);
void flag_luarun_init(void);
FlagResult flag_luarun_execute(void);
void flag_pyrun_init(void);
FlagResult flag_pyrun_execute(void);

/* ── Convenience: register all built-in flags ─────────────────── */
static inline void flags_init(void) {
    flag_version_init();
    flag_help_init();
    flag_run_init();
    flag_build_init();
    flag_lexer_init();
    flag_parser_init();
    flag_ast_init();
    flag_ir_init();
    flag_codegen_init();
    flag_all_flags_init();
    flag_linclude_init();
    flag_llib_init();
    flag_lextend_init();
    flag_wasm_init();
    flag_debug_init();
    flag_dll_init();
    flag_luarun_init();
    flag_pyrun_init();
}

#endif /* FLAGS_H */
