/*
 * gcl — gcLang Compiler / Runner
 *
 * Entry point. Parses CLI flags via the modular flags/ system.
 * Each flag is a separate module in flags/ registered during startup.
 */

#include "flags/flags.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    /* register all flag modules */
    flags_init();

    /* process argv — each handler is called in order */
    FlagResult r = flag_process(argc, argv);

    /* deferred execution: -luarun, -dll, -so flags may have been set */
    if (r == FLAG_OK) {
        r = flag_luarun_execute();
    }

    switch (r) {
        case FLAG_OK:
            return 0;
        case FLAG_EXIT:
            return 0;
        case FLAG_ERROR:
            return 1;
    }

    return 0;
}
