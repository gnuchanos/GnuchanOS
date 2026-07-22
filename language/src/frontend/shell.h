#ifndef GCL_SHELL_H
#define GCL_SHELL_H

#include "codegen.h"

/* Run the interactive GCL shell (REPL).
   Returns 0 on normal exit. */
int shell_run(CodegenOpts *opts);

#endif
