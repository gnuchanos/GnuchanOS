/* Main: pipeline orkestrasyonu */
#include "gcl.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "linker.h"
#include "semantic.h"
#include "shell.h"
#include "version.h"
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n == 0) { fclose(f); return strdup(""); }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return 0; }
    size_t rlen = fread(b, 1, (size_t)n, f);
    if ((long)rlen != n) { free(b); fclose(f); return 0; }
    fclose(f); b[n] = 0;
    return b;
}

static char *change_ext(const char *path, const char *new_ext) {
    const char *dot = strrchr(path, '.');
    size_t base_len = dot ? (size_t)(dot - path) : strlen(path);
    size_t ext_len = strlen(new_ext);
    char *r = malloc(base_len + ext_len + 1);
    if (!r) return 0;
    memcpy(r, path, base_len);
    memcpy(r + base_len, new_ext, ext_len + 1);
    return r;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        shell_run();
        return 0;
    }
    GCLConfig cfg;
    if (parse_cli(argc, argv, &cfg)) return 1;
    if (!cfg.input_file) return 1;

    char *src = read_file(cfg.input_file);
    if (!src) return 1;

    Lexer lx;
    lexer_init(&lx, src, cfg.input_file, cfg.debug);
    if (cfg.stop_lexer) { lexer_dump(&lx); free(src); return 0; }

    Parser ps;
    parser_init(&ps, &lx, src, cfg.debug);
    Node *ast = parser_parse(&ps);
    if (ps.errors > 0) {
        fprintf(stderr, "compilation failed with %d error(s)\n", ps.errors);
        free(src); return 1;
    }
    if (cfg.stop_parser) { parser_dump(ast, 0); free(src); return 0; }
    if (cfg.stop_ast) { parser_dump(ast, 0); free(src); return 0; }

    /* semantic analysis */
    SemState sem;
    sem_init(&sem);
    sem_enter_scope(&sem);
    if (sem_analyze(&sem, ast, src) > 0) {
        fprintf(stderr, "semantic analysis failed\n");
        free(src); return 1;
    }

    /* codegen: AST -> C */
    char *c_file = 0;
    if (cfg.output_file) {
        const char *out_ext = strrchr(cfg.output_file, '.');
        int do_link = 1;
        if (out_ext && strcmp(out_ext, ".c") == 0) do_link = 0;

        if (do_link) {
            c_file = change_ext(cfg.output_file, ".c");
            FILE *out = fopen(c_file, "w");
            if (!out) { fprintf(stderr, "cannot write %s\n", c_file); free(src); return 1; }
            codegen_generate(out, ast);
            fclose(out);
            printf("OK: %s -> %s\n", cfg.input_file, c_file);

            if (linker_link(&cfg, c_file, cfg.output_file) != 0) {
                fprintf(stderr, "linking failed\n");
                free(c_file); free(src); return 1;
            }
            printf("OK: %s -> %s\n", c_file, cfg.output_file);
        } else {
            FILE *out = fopen(cfg.output_file, "w");
            if (!out) { fprintf(stderr, "cannot write %s\n", cfg.output_file); free(src); return 1; }
            codegen_generate(out, ast);
            fclose(out);
            printf("OK: %s -> %s\n", cfg.input_file, cfg.output_file);
        }
    } else {
        codegen_generate(stdout, ast);
    }

    if (cfg.do_run && cfg.output_file) {
        /* run the compiled executable */
        const char *run_cmd = cfg.output_file;
        printf("Running: %s\n", run_cmd);
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s", run_cmd);
        system(buf);
    } else if (cfg.do_run && c_file) {
        /* -run without -o: compile to temp, run, clean */
        char *exe = change_ext(c_file, ".exe");
        /* codegen was already done above if output_file was set */
        /* If no output_file and do_run, not handled here yet */
        free(exe);
    }

    if (c_file) free(c_file);
    free(src);
    return 0;
}
