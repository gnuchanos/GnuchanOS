/*
 * gcl.h — SharedPipeline public API
 *
 * This is the ONLY header main.c includes. It exposes:
 *   gcl_dump_tokens()    — lexer dump
 *   gcl_dump_ast()       — parser/AST dump
 *   gcl_dump_ir()        — IR dump
 *   gcl_full_pipeline()  — semantic + typecheck (no exec)
 *   gcl_run_file()       — full pipeline + interpreter
 *   GclDiagBag           — diagnostics bag type
 *   gcl_diag_bag_init()  — init diagnostics
 */

#ifndef GCL_H
#define GCL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Diagnostics ──────────────────────────────────── */

#define GCL_DIAG_MAX 256

typedef enum {
    GCL_DIAG_ERROR,
    GCL_DIAG_WARNING,
    GCL_DIAG_DEBUG
} GclDiagLevel;

typedef struct {
    GclDiagLevel level;
    int          line;
    int          col;
    char         message[512];
    char         file[256];
} GclDiagEntry;

typedef struct {
    GclDiagEntry entries[GCL_DIAG_MAX];
    int          count;
    int          error_count;
    int          warning_count;
} GclDiagBag;

void gcl_diag_bag_init(GclDiagBag *bag);
void gcl_diag_add(GclDiagBag *bag, GclDiagLevel level, int line, int col, const char *file, const char *fmt, ...);
void gcl_diag_print_all(const GclDiagBag *bag);

/* ── Public Pipeline API ──────────────────────────── */

int gcl_dump_tokens(const char *source);
int gcl_dump_ast(const char *source);
int gcl_dump_ir(const char *source);
int gcl_full_pipeline(const char *source, GclDiagBag *diag);
int gcl_run_file(const char *source, const char *filepath);

#endif /* GCL_H */
