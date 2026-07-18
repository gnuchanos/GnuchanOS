/*
 * gcl — Gnuchan C-Like Language Compiler
 * Pipeline: Source → Lexer → Parser → AST → Semantic → IR → Codegen → C → GCC
 *
 * Preprocessor:
 *   #include <file.gcsf>   — include GCL source file
 *   #lib <file.gclib>      — include GCL library module
 *   #extern <file.dll>     — link external C library (.dll/.so/.a)
 *
 *   extern void my_c_func(int);  — C ABI declaration
 */

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "ir/ir.h"
#include "ir/ir_builder.h"
#include "codegen/codegen.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GCL_VERSION "0.1.0"

/* ----------------------------------------------------------------- */
/*  Helpers                                                          */
/* ----------------------------------------------------------------- */

static const char *path_basename(const char *path) {
    const char *p = strrchr(path, '/');
    const char *q = strrchr(path, '\\');
    if (q > p) p = q;
    if (!p) return path;
    return p + 1;
}

static void safe_name(const char *input, char *out, int n) {
    const char *base = path_basename(input);
    int i = 0;
    for (; *base && i < n - 1; base++) {
        char c = *base;
        if (c == '/' || c == '\\' || c == '.' || c == ':') c = '_';
        if (c == '"' || c == '<' || c == '>') c = '_';
        out[i++] = c;
    }
    out[i] = '\0';
}

static char *read_file(const char *path, long *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = (char*)malloc(sz + 1);
    if (!src) { fclose(fp); return NULL; }
    fread(src, 1, sz, fp);
    src[sz] = '\0';
    fclose(fp);
    if (out_len) *out_len = sz;
    return src;
}

/* ----------------------------------------------------------------- */
/*  Preprocessor: collect includes, libs, externs                     */
/* ----------------------------------------------------------------- */

typedef struct {
    char   **filenames;
    int      fcount;
    char   **libs;
    int      lcount;
    char   **dlls;
    int      dcount;
} PreprocInfo;

static PreprocInfo preproc_collect(const char *src, const char *src_name) {
    PreprocInfo pi = {0};
    Lexer l = lexer_new(src, src_name);
    Token t;
    do {
        t = lexer_next(&l);
        if (t.type == TOKEN_PREPROC_INCLUDE) {
            char fname[512];
            int flen = (int)t.len < 510 ? (int)t.len : 510;
            memcpy(fname, t.start, flen); fname[flen] = '\0';
            char full[1024];
            const char *last_slash = strrchr(src_name, '/');
            const char *last_bs = strrchr(src_name, '\\');
            const char *sep = (last_bs > last_slash) ? last_bs : last_slash;
            if (sep) {
                int dirlen = (int)(sep - src_name + 1);
                memcpy(full, src_name, dirlen);
                memcpy(full + dirlen, fname, flen + 1);
            } else {
                snprintf(full, sizeof(full), "%s", fname);
            }
            pi.filenames = realloc(pi.filenames, (pi.fcount + 1) * sizeof(char*));
            pi.filenames[pi.fcount++] = strdup(full);
        }
        if (t.type == TOKEN_PREPROC_LIB) {
            char fname[512];
            int flen = (int)t.len < 510 ? (int)t.len : 510;
            memcpy(fname, t.start, flen); fname[flen] = '\0';
            pi.libs = realloc(pi.libs, (pi.lcount + 1) * sizeof(char*));
            pi.libs[pi.lcount++] = strdup(fname);
        }
        if (t.type == TOKEN_PREPROC_EXTERN) {
            char fname[512];
            int flen = (int)t.len < 510 ? (int)t.len : 510;
            memcpy(fname, t.start, flen); fname[flen] = '\0';
            pi.dlls = realloc(pi.dlls, (pi.dcount + 1) * sizeof(char*));
            pi.dlls[pi.dcount++] = strdup(fname);
        }
    } while (t.type != TOKEN_EOF);
    return pi;
}

static void preproc_free(PreprocInfo *pi) {
    for (int i = 0; i < pi->fcount; i++) free(pi->filenames[i]);
    free(pi->filenames);
    for (int i = 0; i < pi->lcount; i++) free(pi->libs[i]);
    free(pi->libs);
    for (int i = 0; i < pi->dcount; i++) free(pi->dlls[i]);
    free(pi->dlls);
    memset(pi, 0, sizeof(*pi));
}

/* ----------------------------------------------------------------- */
/*  Pipeline: source → ast → codegen → gcc                           */
/* ----------------------------------------------------------------- */

static int compile_pipeline(const char *input, const char *output,
                             int extra_includes, PreprocInfo *pp_extra) {
    (void)extra_includes;
    char *src = read_file(input, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", input); return 1; }

    PreprocInfo pp = preproc_collect(src, input);

    runtime_init(0);

    char *combined = strdup(src);
    size_t comb_len = strlen(combined);
    for (int i = 0; i < pp.fcount; i++) {
        long inclen;
        char *incsrc = read_file(pp.filenames[i], &inclen);
        if (incsrc) {
            combined = realloc(combined, comb_len + inclen + 256);
            snprintf(combined + comb_len, 256, "\n/* #include %s */\n", pp.filenames[i]);
            comb_len += strlen(combined + comb_len);
            memcpy(combined + comb_len, incsrc, inclen);
            comb_len += inclen;
            combined[comb_len] = '\0';
            free(incsrc);
        } else {
            fprintf(stderr, "Warning: cannot open #include '%s'\n", pp.filenames[i]);
        }
    }

    /* Lexer → Parser → AST */
    Parser parser = parser_new(combined, input);
    AstNode *ast = parser_parse(&parser);
    if (parser.error_count > 0) {
        fprintf(stderr, "Parsing failed with %d errors\n", parser.error_count);
        ast_free(ast); free(combined); free(src); runtime_cleanup(); preproc_free(&pp);
        return 1;
    }

    /* Semantic Analysis */
    SemanticResult sr = semantic_analyze(ast);
    if (sr.errors > 0) {
        fprintf(stderr, "Semantic analysis found %d errors\n", sr.errors);
        ast_free(ast); free(combined); free(src); runtime_cleanup(); preproc_free(&pp);
        return 1;
    }

    /* IR Generation (for completeness, even if not directly used in codegen path) */
    IRModule *ir_mod = ir_module_new();
    IRBuilder *builder = ir_builder_new(ir_mod);
    builder->src_file = input;
    ir_build_program(builder, ast);

    /* Codegen → C file */
    char safe[256], c_path[1024], exe_path[1024];
    safe_name(input, safe, sizeof(safe));
    snprintf(c_path, sizeof(c_path), "_gcl_%s.c", safe);
    snprintf(exe_path, sizeof(exe_path), "%s", output && output[0] ? output : "a.out");

    FILE *cfp = fopen(c_path, "w");
    if (!cfp) { perror(c_path); ir_module_free(ir_mod); ir_builder_free(builder); ast_free(ast); free(combined); free(src); runtime_cleanup(); preproc_free(&pp); return 1; }
    codegen_emit(ast, cfp);
    fclose(cfp);

    /* GCC compile */
    char cmd[8192];
    int cmdlen = snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -mconsole -o \"%s\" \"%s\"", exe_path, c_path);

    for (int i = 0; i < pp.dcount; i++)
        cmdlen += snprintf(cmd + cmdlen, sizeof(cmd) - cmdlen, " \"%s\"", pp.dlls[i]);
    for (int i = 0; i < pp.lcount; i++) {
        const char *lib = pp.libs[i];
        while (*lib == '#' || *lib == ' ' || *lib == '\t') lib++;
        const char *sp = lib;
        while (*sp && *sp != ' ' && *sp != '\t' && *sp != '\n') sp++;
        if (sp > lib)
            cmdlen += snprintf(cmd + cmdlen, sizeof(cmd) - cmdlen, " \"%.*s\"", (int)(sp - lib), lib);
    }

    /* Extra libs from -llib */
    if (pp_extra) {
        for (int i = 0; i < pp_extra->lcount; i++)
            cmdlen += snprintf(cmd + cmdlen, sizeof(cmd) - cmdlen, " \"%s\"", pp_extra->libs[i]);
    }

    snprintf(cmd + cmdlen, sizeof(cmd) - cmdlen, " 2>&1");
    int ret = system(cmd);

    if (ret != 0)
        fprintf(stderr, "GCC compilation failed\n");
    else
        printf("Compiled: %s -> %s\n", path_basename(input), exe_path);

    remove(c_path);
    ir_module_free(ir_mod); ir_builder_free(builder);
    ast_free(ast); free(combined); free(src); runtime_cleanup(); preproc_free(&pp);
    return ret;
}

/* ----------------------------------------------------------------- */
/*  Full pipeline dump                                                */
/* ----------------------------------------------------------------- */

static int do_full_pipeline(const char *path, int run_mode) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return 1; }

    runtime_init(0);

    Parser parser = parser_new(src, path);
    AstNode *ast = parser_parse(&parser);
    if (parser.error_count > 0) {
        fprintf(stderr, "Parsing failed with %d errors\n", parser.error_count);
        ast_free(ast); free(src); runtime_cleanup(); return 1;
    }

    SemanticResult sr = semantic_analyze(ast);
    if (sr.errors > 0) {
        fprintf(stderr, "Semantic analysis found %d errors\n", sr.errors);
        ast_free(ast); free(src); runtime_cleanup(); return 1;
    }

    char safe[256], c_path[1024], exe_path[1024];
    safe_name(path, safe, sizeof(safe));
    snprintf(c_path, sizeof(c_path), "_gcl_%s.c", safe);
    snprintf(exe_path, sizeof(exe_path), "%s.exe", safe);

    FILE *cfp = fopen(c_path, "w");
    if (!cfp) { perror(c_path); ast_free(ast); free(src); runtime_cleanup(); return 1; }
    codegen_emit(ast, cfp);
    fclose(cfp);

    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "gcc -std=c99 -mconsole -o \"%s\" \"%s\" 2>&1", exe_path, c_path);
    int ret = system(cmd);

    if (ret != 0) {
        fprintf(stderr, "GCC compilation failed\n");
    } else {
        printf("Compiled: %s -> %s\n", path_basename(path), exe_path);
        if (run_mode) {
            printf("\n--- Running ---\n\n");
            char runcmd[4096];
            snprintf(runcmd, sizeof(runcmd), "%s", exe_path);
            ret = system(runcmd);
            printf("\n--- Exit code: %d ---\n", ret);
        }
    }

    remove(c_path);
    ast_free(ast); free(src); runtime_cleanup();
    return ret;
}

/* ----------------------------------------------------------------- */
/*  Dump modes                                                       */
/* ----------------------------------------------------------------- */

static void dump_lexer(const char *path) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return; }
    Lexer l = lexer_new(src, path);
    Token t;
    do {
        t = lexer_next(&l);
        printf("[%4d:%d] %-20s '", t.line, t.col, token_name(t.type));
        fwrite(t.start, 1, t.len, stdout);
        printf("'\n");
    } while (t.type != TOKEN_EOF);
    if (l.error_count) printf("(%d lexer errors)\n", l.error_count);
    free(src);
}

static void dump_parser_state(const char *path) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return; }
    Parser parser = parser_new(src, path);
    AstNode *ast = parser_parse(&parser);
    printf("Errors: %d\n", parser.error_count);
    if (ast) ast_dump(ast, stdout, 0);
    ast_free(ast); free(src);
}

static void dump_ast(const char *path) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return; }
    Parser parser = parser_new(src, path);
    AstNode *ast = parser_parse(&parser);
    if (parser.error_count) printf("Parse errors: %d\n", parser.error_count);
    else { printf(";; AST Dump:\n"); ast_dump(ast, stdout, 0); }
    ast_free(ast); free(src);
}

static void dump_ir(const char *path) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return; }
    Parser parser = parser_new(src, path);
    AstNode *ast = parser_parse(&parser);
    if (parser.error_count) { ast_free(ast); free(src); return; }
    SemanticResult sr = semantic_analyze(ast);
    if (sr.errors) { ast_free(ast); free(src); return; }
    IRModule *mod = ir_module_new();
    IRBuilder *b = ir_builder_new(mod);
    b->src_file = path;
    ir_build_program(b, ast);
    ir_dump_module(mod, stdout);
    ir_module_free(mod); ir_builder_free(b);
    ast_free(ast); free(src);
}

static void dump_codegen(const char *path) {
    char *src = read_file(path, NULL);
    if (!src) { fprintf(stderr, "Error: cannot open '%s'\n", path); return; }
    Parser parser = parser_new(src, path);
    AstNode *ast = parser_parse(&parser);
    if (parser.error_count) { ast_free(ast); free(src); return; }
    SemanticResult sr = semantic_analyze(ast);
    if (sr.errors) { ast_free(ast); free(src); return; }
    codegen_emit(ast, stdout);
    ast_free(ast); free(src);
}

/* ----------------------------------------------------------------- */
/*  Interactive Shell                                                */
/* ----------------------------------------------------------------- */

static void interactive_shell(void) {
    printf("GCL v%s - Interactive Shell\n", GCL_VERSION);
    printf("Type .help for commands, .quit to exit.\n\n");
    char line[4096];
    for (;;) {
        printf("gcl> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        if (line[0] == '.') {
            if (strcmp(line, ".quit") == 0 || strcmp(line, ".exit") == 0) break;
            if (strcmp(line, ".help") == 0) {
                printf("  .quit / .exit   Exit\n");
                printf("  .help           This help\n");
                printf("  <file.gcsf>     Compile and run\n");
                continue;
            }
            printf("Unknown: %s\n", line); continue;
        }
        FILE *t = fopen(line, "rb");
        if (t) { fclose(t); do_full_pipeline(line, 1); }
        else printf("File not found: %s\n", line);
    }
    printf("Bye.\n");
}

/* ----------------------------------------------------------------- */
/*  Main — GCC-style: -flag value, order-independent                 */
/* ----------------------------------------------------------------- */

static void print_usage(void) {
    printf("GCL v%s - Usage:\n", GCL_VERSION);
    printf("  gcl                          Interactive shell\n");
    printf("  gcl <file.gcsf>              Compile and run\n");
    printf("  gcl -o <out> <file.gcsf>     Compile to <out>\n");
    printf("  gcl -v                       Version\n");
    printf("  gcl -h                       Help\n\n");
    printf("Dump modes (output to stdout):\n");
    printf("  gcl -lexer <file>            Tokens\n");
    printf("  gcl -parser <file>           Parse tree\n");
    printf("  gcl -ast <file>              AST\n");
    printf("  gcl -ir <file>               IR\n");
    printf("  gcl -codegen <file>          Generated C\n\n");
    printf("Extra:\n");
    printf("  gcl -linclude <path>  (not implemented)\n");
    printf("  gcl -llib <path>      (not implemented)\n");
    printf("  gcl -lextend <path>   (not implemented)\n");
}

int main(int argc, char **argv) {
    if (argc == 1) { interactive_shell(); return 0; }

    /* GCC-style flag parsing: scan all args once */
    const char *input_file = NULL;
    const char *output_file = NULL;
    int run_mode = 1; /* default: compile + run */
    int dump_mode = 0;
    const char *dump_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-version") == 0) {
            printf("GCL v%s\n", GCL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[i], "-run") == 0) {
            run_mode = 1;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
            run_mode = 0; /* -o ile sadece derle */
            continue;
        }
        if (strcmp(argv[i], "-lexer") == 0)   { dump_mode = 1; dump_arg = argv[++i]; break; }
        if (strcmp(argv[i], "-parser") == 0)  { dump_mode = 2; dump_arg = argv[++i]; break; }
        if (strcmp(argv[i], "-ast") == 0)     { dump_mode = 3; dump_arg = argv[++i]; break; }
        if (strcmp(argv[i], "-ir") == 0)      { dump_mode = 4; dump_arg = argv[++i]; break; }
        if (strcmp(argv[i], "-codegen") == 0) { dump_mode = 5; dump_arg = argv[++i]; break; }
        if (strcmp(argv[i], "-linclude") == 0) { if (i+1<argc) i++; printf("include path (not implemented)\n"); continue; }
        if (strcmp(argv[i], "-llib") == 0)     { if (i+1<argc) i++; printf("lib path (not implemented)\n"); continue; }
        if (strcmp(argv[i], "-lextend") == 0)  { if (i+1<argc) i++; printf("extend path (not implemented)\n"); continue; }
        /* -- means end of options */
        if (strcmp(argv[i], "--") == 0) {
            i++;
            while (i < argc) { input_file = argv[i]; i++; }
            break;
        }
        /* Not a flag starting with '-', treat as input file */
        if (argv[i][0] != '-') {
            input_file = argv[i];
            /* .exe/.dll/.so gibi binary dosyaları engelle */
            size_t len = strlen(input_file);
            if ((len >= 4 && (strcmp(input_file + len - 4, ".exe") == 0 ||
                              strcmp(input_file + len - 4, ".dll") == 0 ||
                              strcmp(input_file + len - 4, ".so")  == 0 ||
                              strcmp(input_file + len - 4, ".a")   == 0)) ||
                (len >= 2 && strcmp(input_file + len - 2, ".o") == 0)) {
                fprintf(stderr, "error: '%s' is a binary file, expected .gcsf source file\n", input_file);
                return 1;
            }
            continue;
        }
        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        return 1;
    }

    /* Dump modes */
    if (dump_mode) {
        if (!dump_arg) { fprintf(stderr, "Missing file argument\n"); return 1; }
        switch (dump_mode) {
        case 1: dump_lexer(dump_arg); break;
        case 2: dump_parser_state(dump_arg); break;
        case 3: dump_ast(dump_arg); break;
        case 4: dump_ir(dump_arg); break;
        case 5: dump_codegen(dump_arg); break;
        }
        return 0;
    }

    /* Need an input file */
    if (!input_file) {
        fprintf(stderr, "No input file specified.\n");
        return 1;
    }

    /* Compile */
    if (output_file)
        return compile_pipeline(input_file, output_file, 0, NULL);
    else
        return do_full_pipeline(input_file, run_mode);
}
