#ifndef GCL_VERSION_H
#define GCL_VERSION_H

#define GCL_MAJOR 0
#define GCL_MINOR 1
#define GCL_PATCH 0

#define GCL_STAGE "dev"        /* dev, alpha, beta, rc, stable */

#define GCL_VERSION_STR "0.1.0-dev"
#define GCL_LANG_NAME "Gnuchan C-Like"
#define GCL_LANG_STANDARD "GCL'25"

void version_print_full(void);
void version_print_short(void);
int  version_check_lang(const char *req);

#endif
