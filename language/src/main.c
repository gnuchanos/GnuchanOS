#include "lexer.h"
#include "parser.h"
#include "defines.h"
#include "codegen.h"
#include "colors.h"
#include "io.h"
#include "preprocessor.h"
#include "exporter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== main ========== */

int main(int argc, char **argv) {
    defines_init();
    preprocessor_init();

    CodegenOpts opts = { .mode = MODE_EXEC, .output = NULL, .base_name = NULL, .debug_flag = 0 };
    const char *input_file = NULL;
    const char *lextend_dir = NULL;

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
        if (strcmp(argv[i],"-linclude")==0){ NEXT_ARG_OR_ERR("-linclude"); (void)argv[++i]; continue; }
        if (strcmp(argv[i],"-llib")==0)    { NEXT_ARG_OR_ERR("-llib");     (void)argv[++i]; continue; }
        if (strcmp(argv[i],"-lextend")==0) { NEXT_ARG_OR_ERR("-lextend");  lextend_dir=argv[++i]; continue; }
        if (strcmp(argv[i],"-o")==0)       { NEXT_ARG_OR_ERR("-o"); opts.base_name=argv[++i]; continue; }
        if (argv[i][0]=='-') { fprintf(stderr,CLR_RED "error:" CLR_RESET " unknown flag '%s'\n",argv[i]); return 1; }
        input_file = argv[i];
    }
    #undef NEXT_ARG_OR_ERR

    /* --- REPL --- */
    if (!input_file) {
        printf(CLR_PURPLE "gcl v0.2" CLR_RESET " — interactive shell\n\n");
        char line[1024];
        while (1) {
            printf(CLR_PURPLE "gcl> " CLR_RESET); fflush(stdout);
            if (!fgets(line,sizeof(line),stdin)) break;
            size_t len = strlen(line);
            while (len>0&&(line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
            if (len==0) continue;
            if (strcmp(line,"exit")==0||strcmp(line,"quit")==0) break;
            Lexer sl; lexer_init(&sl,line,"<shell>");
            Parser *sp=parser_new(&sl);
            AstNode *spg=parser_parse(sp);
            spg->left=preprocess_inline(spg);
            for (AstNode *sn=spg->left;sn;sn=sn->next)
                if (sn->kind==NODE_DEBUG) { opts.mode=MODE_EXEC; opts.output=stdout; codegen_emit(spg,&opts); break; }
            free(sp);
        }
        printf("bye.\n"); return 0;
    }

    /* --- load source --- */
    char *source = file_read(input_file);
    if (!source) {
        char auto_path[1024];
        snprintf(auto_path,sizeof(auto_path),"%s.gcsf",input_file);
        source=file_read(auto_path); if (source) input_file=auto_path;
    }
    if (!source) { fprintf(stderr,"gcl: cannot open '%s'\n",input_file); return 1; }

    char src_dir[1024] = "";
    {
        const char *s=strrchr(input_file,'/'), *b=strrchr(input_file,'\\'), *l=s>b?s:b;
        if (l) { size_t d=l-input_file+1; memcpy(src_dir,input_file,d); src_dir[d]='\0'; }
    }

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
    preprocess_load(prog, src_dir);

    if (opts.mode==MODE_CODEGEN || opts.base_name)
        prog->left = preprocess_codegen(prog);
    else
        prog->left = preprocess_inline(prog);

    /* --- export or display --- */
    if (opts.base_name) {
        export_project(prog, opts.base_name, lextend_dir);
        defines_free(); preprocess_free_included(); free(source); free(parser); return 0;
    }

    opts.output = stdout; codegen_emit(prog, &opts);
    defines_free(); preprocess_free_included(); free(source); free(parser);
    return 0;
}
