#ifndef GCL_PREPROCESSOR_H
#define GCL_PREPROCESSOR_H

#include "types.h"

/* Reset preprocessor state (called once per compilation). */
void preprocessor_init(void);

/* Run inline preprocessor: evaluate #if/#ifdef/#ifndef, execute #define/#undef, filter dead code.
   keep_all=1 preserves conditional markers for codegen; keep_all=0 strips dead branches. */
AstNode *preprocess_inline_ex(AstNode *prog, int keep_all);

static inline AstNode *preprocess_inline(AstNode *prog)   { return preprocess_inline_ex(prog, 0); }
static inline AstNode *preprocess_codegen(AstNode *prog)  { return preprocess_inline_ex(prog, 1); }

/* Load all #include / #lib files recursively into the include table.
   src_dir points to the input file's directory.
   linclude_dir and llib_dir are optional -I / -L style search paths (can be NULL). */
void preprocess_load(AstNode *prog, const char *src_dir, const char *linclude_dir, const char *llib_dir);

/* Access the include table (for exporter). */
int           preprocess_included_count(void);
const char   *preprocess_included_name(int i);
AstNode      *preprocess_included_ast(int i);
int           preprocess_included_is_lib(int i);

/* Free all included ASTs and reset the table. */
void preprocess_free_included(void);

/* Resolve a #include/#lib/#extern filename to a source directory path.
   Tries plain name, .gcsf, .gclib, .h extensions. Returns heap-allocated content or NULL. */
char *preprocess_resolve_path(const char *src_dir, const char *filename);

#endif
