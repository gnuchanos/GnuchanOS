/* Linker: GCL kaynak + kutuphaneleri GCC ile birlestirir */
#include "gcl.h"
#include "linker.h"

int linker_link(GCLConfig *cfg, const char *c_file, const char *output) {
    /* GCC cagrisi olustur:
       gcc -std=gnu99 input.c -o output -lm
          + -I include_dirs
          + -L lib_dirs
          + extern_libs (.dll/.so/.a) */
    char cmd[4096];
    int pos = 0;
    pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                    "gcc -std=gnu99 -Wall -Wextra \"%s\" -o \"%s\" -lm",
                    c_file, output);

    for (int i = 0; i < cfg->num_include_dirs; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -I\"%s\"", cfg->include_dirs[i]);

    for (int i = 0; i < cfg->num_lib_dirs; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -L\"%s\"", cfg->lib_dirs[i]);

    for (int i = 0; i < cfg->num_extend_dirs; i++) {
        const char *lib = cfg->extend_dirs[i];
        size_t len = strlen(lib);
        /* .gclib: GCL kaynak dosyasi -> #include ile eklenir */
        if (len > 6 && strcmp(lib + len - 6, ".gclib") == 0) {
            pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -include \"%s\"", lib);
        } else {
            /* .dll, .so, .a: direkt link */
            pos += snprintf(cmd + pos, sizeof(cmd) - pos, " \"%s\"", lib);
        }
    }

    if (cfg->debug)
        fprintf(stderr, "link: %s\n", cmd);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "link failed (exit %d)\n", ret);
        return 1;
    }
    return 0;
}
