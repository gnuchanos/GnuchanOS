/*
 * gcl_simple_runner.h — Runs a GCL file through the lexer→parser→runner pipeline.
 */

#ifndef GCL_SIMPLE_RUNNER_H
#define GCL_SIMPLE_RUNNER_H

#include <stddef.h>

/* Runs the source. Success: 0, error: -1. */
int gcl_simple_run_source(const char *src, size_t len, const char *base_dir,
                          int argc, char **argv);

#endif /* GCL_SIMPLE_RUNNER_H */
