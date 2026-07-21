// main.c — GCL Compiler entry point
#include <stdio.h>
#include "flags/flags.h"
#include "shell/shell.h"

int main(int argc, char** argv) {
    GclFlags* f = gcl_flags_parse(argc, argv);

    if (f->show_version) {
        printf("GCL Compiler v0.1.0-dev\n");
        gcl_flags_free(f);
        return 0;
    }

    if (f->show_help) {
        printf("GCL Compiler\n");
        printf("  gcl [flags] [file.gcsf]\n\n");
        printf("Flags:\n");
        printf("  -version, -v       version info\n");
        printf("  -help, -h          this help\n");
        printf("  -debug             debug mode\n");
        printf("  -lexer <file>      token stream\n");
        printf("  -parser <file>     parse tree\n");
        printf("  -ast <file>        AST dump\n");
        printf("  -ir <file>         IR dump\n");
        printf("  -codegen <file>    generated code\n");
        printf("  -o <file>          output file\n");
        printf("  -linclude <path>   include path\n");
        printf("  -llib <path>       library path\n");
        printf("  -lextend <path>    extension path\n");
        gcl_flags_free(f);
        return 0;
    }

    if (f->interactive) {
        gcl_flags_free(f);
        gcl_shell_run();
        return 0;
    }

    // Non-interactive pipeline mode
    printf("[GCL] input=%s, pipeline=%d, debug=%d\n",
           f->input_file, f->pipeline, f->debug);
    if (f->output_file) printf("[GCL] output=%s\n", f->output_file);
    for (int i = 0; i < f->linclude_count; i++) printf("[GCL] linclude=%s\n", f->linclude[i]);
    for (int i = 0; i < f->llib_count; i++) printf("[GCL] llib=%s\n", f->llib[i]);
    for (int i = 0; i < f->lextend_count; i++) printf("[GCL] lextend=%s\n", f->lextend[i]);

    // TODO: route to pipeline stages based on f->pipeline
    printf("[GCL] Pipeline not yet implemented (placeholder)\n");

    gcl_flags_free(f);
    return 0;
}
