#ifndef GCL_ERROR_H
#define GCL_ERROR_H

#include "gcl.h"

/* hata kodlari */
typedef enum {
    E_OK = 0,
    E_INTERNAL,
    E_SYNTAX, E_EXPECTED_IDENT, E_EXPECTED_SEMI, E_EXPECTED_EXPR, E_INVALID_TOKEN,
    E_TYPE, E_UNDECLARED, E_REDECLARED, E_MISMATCH,
    E_MEM_OUT, E_DOUBLE_FREE, E_NULL_DEREF, E_BOUNDS,
    E_IO_OPEN, E_IO_READ, E_IO_WRITE
} ErrCode;

/* mor tema — sadece mor tonlari */
#define C_PURPLE  "\033[1;35m"   /* koyu mor */
#define C_LPURPLE "\033[95m"     /* acik mor */
#define C_RED     "\033[1;31m"   /* error */
#define C_YELLOW  "\033[1;33m"   /* warning */
#define C_RESET   "\033[0m"

void error_report(ErrCode code, SourceLoc loc, const char *src,
                  const char *fmt, ...);
void warning_report(SourceLoc loc, const char *fmt, ...);
void error_set_filename(const char *fname);

#endif
