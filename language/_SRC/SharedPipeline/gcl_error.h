/*
 * gcl_error.h — GCL hata tipleri.
 */

#ifndef GCL_ERROR_H
#define GCL_ERROR_H

#include <stddef.h>

typedef struct GclError GclError;

GclError *gcl_error_io(const char *msg);
GclError *gcl_error_alloc(const char *msg);
GclError *gcl_error_runtime(const char *msg);
GclError *gcl_error_parse(const char *msg, int line, int col);
void gcl_error_free(GclError *err);
char *gcl_error_format(GclError *err);
const char *gcl_error_msg(const GclError *err);

#endif /* GCL_ERROR_H */
