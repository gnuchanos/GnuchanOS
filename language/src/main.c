#include "lexer.h"
#include "parser.h"
#include "parse_directive.h"
#include "shell.h"
#include "defines.h"
#include "codegen.h"
#include "colors.h"
#include "io.h"
#include "preprocessor.h"
#include "exporter.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== main ========== */

int main(int argc, char **argv) {
    defines_init();
    preprocessor_init();

    CodegenOpts opts = { .mode = MODE_EXEC, .output = NULL, .base_name = NULL, .debug_flag = 0 };
    const char *input_file = NULL;
    const char *input_file_heap = NULL;  /* set when input_file points to heap-allocated auto_path */
    const char *lextend_dir = NULL;
    const char *linclude_dir = NULL;
    const char *llib_dir = NULL;

    #define NEXT_ARG_OR_ERR(flag) \
        do { if (i+1>=argc) { fprintf(stderr,CLR_RED "error:" CLR_RESET " %s requires a value\n",flag); return 1; } } while(0)

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i],"-version")==0||strcmp(argv[i],"-v")==0) { printf(CLR_PURPLE "gcl v0.2" CLR_RESET "\n"); return 0; }
        if (strcmp(argv[i],"-help")==0||strcmp(argv[i],"-h")==0) {
            printf(CLR_PURPLE "gcl v0.2" CLR_RESET " — GnuchanOS C-like Language\n\n");
            printf("  gcl <file.gcsf>                 compile to exe\n");
            printf("  gcl -o <name> <file.gcsf>       multi-file C project\n");
            printf("  gcl -ir <file.gcsf>             IR dump\n");
            printf("  gcl -codegen <file.gcsf>        C source dump\n");
            printf("  gcl -ast <file.gcsf>            AST dump\n");
            printf("  gcl -lexer <file.gcsf>          token stream\n");
            printf("  gcl                              interactive shell\n");
            return 0;
        }
        if (strcmp(argv[i],"-lexer")==0)   { opts.mode=MODE_LEXER; continue; }
        if (strcmp(argv[i],"-parser")==0)  { opts.mode=MODE_PARSER; continue; }
        if (strcmp(argv[i],"-ast")==0)     { opts.mode=MODE_AST; continue; }
        if (strcmp(argv[i],"-codegen")==0) { opts.mode=MODE_CODEGEN; continue; }
        if (strcmp(argv[i],"-ir")==0)      { opts.mode=MODE_IR; continue; }
        if (strcmp(argv[i],"-debug")==0)   { opts.debug_flag=1; continue; }
        if (strcmp(argv[i],"-linclude")==0){ NEXT_ARG_OR_ERR("-linclude"); linclude_dir=argv[++i]; continue; }
        if (strcmp(argv[i],"-llib")==0)    { NEXT_ARG_OR_ERR("-llib");     llib_dir=argv[++i]; continue; }
        if (strcmp(argv[i],"-lextend")==0) { NEXT_ARG_OR_ERR("-lextend");  lextend_dir=argv[++i]; continue; }
        if (strcmp(argv[i],"-o")==0)       { NEXT_ARG_OR_ERR("-o"); opts.base_name=argv[++i]; continue; }
        if (argv[i][0]=='-') { fprintf(stderr,CLR_RED "error:" CLR_RESET " unknown flag '%s'\n",argv[i]); return 1; }
        input_file = argv[i];
    }
    #undef NEXT_ARG_OR_ERR

    /* --- REPL --- */
    if (!input_file) {
        return shell_run(&opts);
    }

    /* --- load source --- */
    char *source = file_read(input_file);
    if (!source) {
        /* NOTE: Must use heap allocation so input_file stays valid outside this block */
        size_t ap_len = strlen(input_file) + 6; /* ".gcsf" + NUL */
        char *auto_path = malloc(ap_len);
        if (!auto_path) { fprintf(stderr, "gcl: malloc failed\n"); return 1; }
        snprintf(auto_path, ap_len, "%s.gcsf", input_file);
        source = file_read(auto_path);
        if (source) {
            input_file = auto_path;
            input_file_heap = auto_path;
        } else {
            free(auto_path);
        }
    }
    if (!source) { fprintf(stderr,"gcl: cannot open '%s'\n",input_file); return 1; }

    char src_dir[1024] = "";
    {
        const char *s=strrchr(input_file,'/'), *b=strrchr(input_file,'\\'), *l=s>b?s:b;
        if (l) { size_t d=l-input_file+1; if (d>=sizeof(src_dir)) d=sizeof(src_dir)-1; memcpy(src_dir,input_file,d); src_dir[d]='\0'; }
    }

    /* Set source for error line display */
    error_set_source(source);

    /* --- parse --- */
    Lexer lexer; lexer_init(&lexer,source,input_file);

    if (opts.mode==MODE_LEXER && !opts.base_name) {
        Token t; do { t=lexer_next(&lexer);
            printf("TOK[%d] line=%zu col=%zu '%.*s'\n",t.kind,t.line,t.col,(int)t.len,t.text);
        } while (t.kind!=TOK_EOF);
        free(source); return 0;
    }

    Parser *parser = parser_new(&lexer);
    AstNode *prog = parser_parse(parser);
    preprocess_load(prog, src_dir, linclude_dir, llib_dir);

    if (opts.mode == MODE_EXEC && !opts.base_name)
        prog->left = preprocess_inline(prog);
    else
        prog->left = preprocess_codegen(prog);

    /* --- export or display --- */
    if (opts.base_name) {
        export_project(prog, opts.base_name, lextend_dir);
        ast_free(prog); defines_free(); preprocess_free_included(); free(source); free(parser);
        free((void*)input_file_heap);
        return 0;
    }

    opts.output = stdout; codegen_emit(prog, &opts);
    ast_free(prog); defines_free(); preprocess_free_included(); free(source); free(parser);
    free((void*)input_file_heap);
    return 0;
}
