/*
 * gcl_simple_runner.h — GCL dosyasını lexer→parser→runner pipeline'ından geçirir.
 */

#ifndef GCL_SIMPLE_RUNNER_H
#define GCL_SIMPLE_RUNNER_H

#include <stddef.h>

/* Kaynağı çalıştırır. Başarı: 0, hata: -1. */
int gcl_simple_run_source(const char *src, size_t len, const char *base_dir,
                          int argc, char **argv);

#endif /* GCL_SIMPLE_RUNNER_H */
