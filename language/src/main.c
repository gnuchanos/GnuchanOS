/* main.c — GCL HEADERS compiler entry */
#include "include/gcl.h"
#include "lexer/lexer.h"
#include "directive/directive.h"

int main(int argc, char **argv) {
    char *in = NULL, *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) out = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            printf("GCL v0.1 HEADERS\nUsage: gcl <file.gcsf> [-o out.c]\n"); return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-version") == 0) {
            printf("GCL v0.1\n"); return 0;
        }
        else if (argv[i][0] != '-') in = argv[i];
    }
    if (!in) { fprintf(stderr, "gcl <file> [-o out.c]\n"); return 1; }

    TokenCtx ctx = lexer_run(in);
    if (!ctx.tokens) return 1;

    FILE *fout = out ? fopen(out, "w") : stdout;
    if (!fout) { fprintf(stderr, "gcl: cannot write '%s'\n", out); return 1; }

    int ret = directive_process(&ctx, fout);
    if (out) fclose(fout);
    free(ctx.tokens);
    return ret;
}
