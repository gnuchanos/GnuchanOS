/*
 * gcl_simple_runner.h — Runs a GCL file through the lexer→parser→runner pipeline.
 */

#ifndef GCL_SIMPLE_RUNNER_H
#define GCL_SIMPLE_RUNNER_H

#include <stddef.h>

/* Runs the source. Success: 0, error: -1.

   `file_name` is the path shown in a diagnostic header
   (`file:line:col: severity: message [CODE]`); NULL means "no name", which the
   renderer prints as `<input>`. It is borrowed for the duration of the call. */
int gcl_simple_run_source(const char *src, size_t len, const char *file_name,
                          const char *base_dir, int argc, char **argv);

#endif /* GCL_SIMPLE_RUNNER_H */
