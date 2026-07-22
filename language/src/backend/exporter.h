#ifndef GCL_EXPORTER_H
#define GCL_EXPORTER_H

#include "types.h"

/* Export a multi-file C project from the compiled AST.
   base_name = output path prefix (e.g. "out/foo").
   lextend_dir = directory to search for #extern files (can be NULL).
   Returns 0 on success. */
int export_project(AstNode *prog, const char *base_name, const char *lextend_dir);

#endif
