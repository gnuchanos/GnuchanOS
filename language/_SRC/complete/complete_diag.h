/*
 * complete_diag.h — Engine diagnostics (diagnostic, §5.4).
 *
 * In GCL, the printf placeholder is `{}` (`%s` does not exist): `printf("a={} b={}", x, y)`.
 * This module compares the number of `{}` placeholders with the number of following
 * arguments when the cursor is inside a printf call and reports any mismatch.
 *
 * Diagnostics are INDEPENDENT of the popup/list state (they can still be produced even if CTX_NONE).
 */
#ifndef GCL_COMPLETE_DIAG_H
#define GCL_COMPLETE_DIAG_H

#include "gcl_complete.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * printf {} placeholder check (§5.4). If the cursor is inside a closed printf call,
 * this compares the number of `{}` placeholders with the number of arguments.
 * On mismatch, out->have_diagnostic = 1 and out->diag_message is filled in.
 * Otherwise it leaves the diagnostic fields in out unchanged.
 */
void gcl_diag_check_printf(const char *text, size_t text_len, size_t cursor,
                           GclCompletionResult *out);

#ifdef __cplusplus
}
#endif

#endif /* GCL_COMPLETE_DIAG_H */
