/* CLI: komut satiri argumanlarini cozumler */
#include "gcl.h"
#include "version.h"
#include "error.h"

int parse_cli(int argc, char **argv, GCLConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            version_print_full(); exit(0);
        } else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%sGCL -- Gnuchan C-Like Language Compiler%s\n", C_PURPLE, C_RESET);
            printf("%s--------------------------------%s\n", C_PURPLE, C_RESET);
            printf("Usage: %sgcl%s [options] %sfile.gcsf%s\n", C_LPURPLE, C_RESET, C_PURPLE, C_RESET);
            printf("\n%sOptions:%s\n", C_PURPLE, C_RESET);
            printf("  %s-v, -version%s          Show version\n", C_LPURPLE, C_RESET);
            printf("  %s-h, -help%s             Show this help\n", C_LPURPLE, C_RESET);
            printf("\n%sPath / Library:%s\n", C_PURPLE, C_RESET);
            printf("  %s-linclude%s <path>       Extra include path\n", C_LPURPLE, C_RESET);
            printf("  %s-llib%s <path>           Extra library path\n", C_LPURPLE, C_RESET);
            printf("  %s-lextend%s <path>        Extension module path\n", C_LPURPLE, C_RESET);
            printf("\n%sDebug / Pipeline:%s\n", C_PURPLE, C_RESET);
            printf("  %s-lexer%s <file>          Token stream dump\n", C_LPURPLE, C_RESET);
            printf("  %s-parser%s <file>         Parse tree dump\n", C_LPURPLE, C_RESET);
            printf("  %s-ast%s <file>            AST dump\n", C_LPURPLE, C_RESET);
            printf("  %s-ir%s <file>             IR dump\n", C_LPURPLE, C_RESET);
            printf("  %s-codegen%s <file>        Generated C code\n", C_LPURPLE, C_RESET);
            printf("  %s-debug%s                 Enable debug output\n", C_LPURPLE, C_RESET);
            printf("  %s-o%s <output>            Output file/executable\n", C_LPURPLE, C_RESET);
            printf("  %s-run%s <file>            Compile and run\n", C_LPURPLE, C_RESET);
            printf("\n%sFlags can be combined: %sgcl -lexer -ast file.gcsf%s\n", C_PURPLE, C_LPURPLE, C_RESET);
            exit(0);
        } else if (strcmp(argv[i], "-linclude") == 0 && i+1 < argc) {
            if (cfg->num_include_dirs < MAX_PATHS)
                cfg->include_dirs[cfg->num_include_dirs++] = argv[++i];
            else i++;
        } else if (strcmp(argv[i], "-llib") == 0 && i+1 < argc) {
            if (cfg->num_lib_dirs < MAX_PATHS)
                cfg->lib_dirs[cfg->num_lib_dirs++] = argv[++i];
            else i++;
        } else if (strcmp(argv[i], "-lextend") == 0 && i+1 < argc) {
            if (cfg->num_extend_dirs < MAX_PATHS)
                cfg->extend_dirs[cfg->num_extend_dirs++] = argv[++i];
            else i++;
        } else if (strcmp(argv[i], "-lexer") == 0) {
            cfg->stop_lexer = 1;
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-parser") == 0) {
            cfg->stop_parser = 1;
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-ast") == 0) {
            cfg->stop_ast = 1;
            cfg->stop_parser = 1;
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-ir") == 0) {
            cfg->stop_ir = 1;
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-codegen") == 0) {
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            cfg->output_file = argv[++i];
        } else if (strcmp(argv[i], "-run") == 0) {
            cfg->do_run = 1;
            if (i+1 < argc && argv[i+1][0] != '-') cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-debug") == 0) {
            cfg->debug = DBG_ALL;
        } else if (argv[i][0] != '-') {
            cfg->input_file = argv[i];
        }
    }
    if (!cfg->input_file && argc > 1) {
        fprintf(stderr, "error: no input file\n");
        return 1;
    }
    return 0;
}
