#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================
// GCL CLI Implementation
// ============================================================

void cli_print_version(void) {
    printf("GCL — Gnuchan C-Like Language Compiler\n");
    printf("Version 0.1.0 (GNU99 compatible)\n");
    printf("Part of GnuchanOS\n");
}

void cli_print_help(void) {
    printf("Usage: gcl [flags] <file.gcsf> [-o output]\n\n");
    printf("Flags:\n");
    printf("  -version, -v          Show version info\n");
    printf("  -help, -h             Show this help\n");
    printf("  -o <path>             Output path\n");
    printf("  -run <file.gcsf>      Compile and run immediately\n");
    printf("  -lexer <file.gcsf>    Dump token stream\n");
    printf("  -parser <file.gcsf>   Dump parse tree\n");
    printf("  -ast <file.gcsf>      Dump AST\n");
    printf("  -ir <file.gcsf>       Dump IR (not yet implemented)\n");
    printf("  -codegen <file.gcsf>  Dump generated C code\n");
    printf("  -debug                Enable debug output\n");
    printf("  -linclude <path>      Add include path\n");
    printf("  -llib <path>          Add library path\n");
    printf("  -lextend <path>       Add extension module path\n");
    printf("\nExamples:\n");
    printf("  gcl file.gcsf -o build/output\n");
    printf("  gcl -run file.gcsf\n");
    printf("  gcl -lexer file.gcsf\n");
}

void cli_parse(int argc, char *argv[], GclCliOptions *opts) {
    memset(opts, 0, sizeof(GclCliOptions));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "-v") == 0) {
            opts->version = 1;
        } else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0) {
            opts->help = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            opts->output_file = argv[++i];
        } else if (strcmp(argv[i], "-run") == 0 && i + 1 < argc) {
            opts->input_file = argv[++i];
            opts->run_mode = 1;
        } else if (strcmp(argv[i], "-lexer") == 0 && i + 1 < argc) {
            opts->input_file = argv[++i];
            opts->lexer_dump = 1;
        } else if (strcmp(argv[i], "-parser") == 0 && i + 1 < argc) {
            opts->input_file = argv[++i];
            opts->parser_dump = 1;
        } else if (strcmp(argv[i], "-ast") == 0 && i + 1 < argc) {
            opts->input_file = argv[++i];
            opts->ast_dump = 1;
        } else if (strcmp(argv[i], "-ir") == 0 && i + 1 < argc) {
            opts->input_file = argv[++i];
            opts->ir_dump = 1;
        } else if (strcmp(argv[i], "-codegen") == 0) {
            opts->codegen_dump = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts->input_file = argv[++i];
            }
        } else if (strcmp(argv[i], "-debug") == 0) {
            opts->debug = 1;
        } else if (strcmp(argv[i], "-linclude") == 0 && i + 1 < argc) {
            if (opts->include_count < 16)
                opts->include_paths[opts->include_count++] = argv[++i];
        } else if (strcmp(argv[i], "-llib") == 0 && i + 1 < argc) {
            if (opts->lib_count < 16)
                opts->lib_paths[opts->lib_count++] = argv[++i];
        } else if (strcmp(argv[i], "-lextend") == 0 && i + 1 < argc) {
            // extension path — stored in lib_paths for now
            if (opts->lib_count < 16)
                opts->lib_paths[opts->lib_count++] = argv[++i];
        } else if (argv[i][0] != '-') {
            // positional: input file
            opts->input_file = argv[i];
        }
    }
}
