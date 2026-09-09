/*
 * gcl_ide_clipboard.h — sistem panosu API.
 */

#ifndef GCL_IDE_CLIPBOARD_H
#define GCL_IDE_CLIPBOARD_H

#include <stddef.h>

void gcl_ide_clipboard_set_text(const char *text, size_t len);
char *gcl_ide_clipboard_get_text(void);

#endif /* GCL_IDE_CLIPBOARD_H */
