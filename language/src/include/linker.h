#ifndef GCL_LINKER_H
#define GCL_LINKER_H

#include "gcl.h"

/* Linker: .gclib, .dll/.so/.a ve GCL kaynak dosyalarini birlestirir */
int linker_link(GCLConfig *cfg, const char *c_file, const char *output);
/* - c_file: codegen'in urettigi C dosyasi
   - output: final executable
   - cfg->include_dirs: -I icin
   - cfg->lib_dirs: -L icin
   - cfg->extern_libs: .dll/.so/.a yollari
*/

#endif
