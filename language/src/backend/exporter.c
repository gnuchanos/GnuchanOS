#include "exporter.h"
#include "codegen.h"
#include "codegen_c.h"
#include "io.h"
#include "preprocessor.h"
#include "defines.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int export_project(AstNode *prog, const char *base_name, const char *lextend_dir) {
    char path[4096], cmd[8192];
    char clean_base[1024];
    snprintf(clean_base, sizeof(clean_base), "%s", base_name);
    size_t blen = strlen(clean_base);
    if (blen > 2 && strcmp(clean_base + blen - 2, ".c") == 0) clean_base[blen - 2] = '\0';

    const char *base_only = strrchr(clean_base, '/');
    const char *base_bs = strrchr(clean_base, '\\');
    base_only = (base_bs > base_only) ? base_bs : base_only;
    base_only = base_only ? base_only + 1 : clean_base;

    char out_dir[1024];
    snprintf(out_dir, sizeof(out_dir), "%s", clean_base);
    {
        const char *slash = strrchr(clean_base, '/');
        const char *bslash = strrchr(clean_base, '\\');
        const char *sep = (bslash > slash) ? bslash : slash;
        if (sep) {
            size_t parent_len = sep - clean_base + 1;
            snprintf(out_dir, sizeof(out_dir), "%.*s%s", (int)parent_len, clean_base, base_only);
        } else {
            snprintf(out_dir, sizeof(out_dir), "%s", base_only);
        }
    }

    /* create output directory */
    {
        char mkcmd[2048];
#ifdef _WIN32
        snprintf(mkcmd, sizeof(mkcmd), "if not exist \"%.900s\" mkdir \"%.900s\"", out_dir, out_dir);
#else
        snprintf(mkcmd, sizeof(mkcmd), "mkdir -p \"%.1024s\"", out_dir);
#endif
        system(mkcmd);
    }

    CodegenOpts opts = { .mode = MODE_CODEGEN };
    char all_sources[4096] = "";

    /* Phase 1: .h + optional .c for each included file */
    int inc_count = preprocess_included_count();
    for (int i = 0; i < inc_count; i++) {
        AstNode *ast = preprocess_included_ast(i);
        if (!ast) continue;

        snprintf(path, sizeof(path), "%.1024s/%.256s.h", out_dir, preprocess_included_name(i));
        FILE *hdr = fopen(path, "w");
        if (!hdr) continue;
        g_codegen_out = hdr;
        codegen_c_emit_header(ast, preprocess_included_name(i));
        fclose(hdr);
        printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);

        if (!preprocess_included_is_lib(i)) {
            snprintf(path, sizeof(path), "%.1024s/%.256s.c", out_dir, preprocess_included_name(i));
            FILE *src = fopen(path, "w");
            if (!src) continue;
            g_codegen_out = src;
            fprintf(src, "#include \"%s.h\"\n\n", preprocess_included_name(i));
            codegen_c_emit_source(ast);
            fclose(src);
            printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);
            size_t rem = sizeof(all_sources) - strlen(all_sources) - 1;
            strncat(all_sources, " \"", rem);
            rem = sizeof(all_sources) - strlen(all_sources) - 1;
            strncat(all_sources, path, rem);
            rem = sizeof(all_sources) - strlen(all_sources) - 1;
            strncat(all_sources, "\"", rem);
        }
    }

    /* Phase 2: main .c */
    snprintf(path, sizeof(path), "%s/main.c", out_dir);
    FILE *cf = fopen(path, "w");
    if (!cf) return 1;
    opts.output = cf;

    for (int i = 0; i < inc_count; i++)
        fprintf(cf, "#include \"%s.h\"\n", preprocess_included_name(i));
    fprintf(cf, "\n");

    opts.mode = MODE_CODEGEN;
    codegen_emit(prog, &opts);
    fclose(cf);
    printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);

    /* Phase 3: .gcdebug */
    snprintf(path, sizeof(path), "%s/main.gcdebug", out_dir);
    FILE *df = fopen(path, "w");
    if (df) {
        opts.output = df; opts.mode = MODE_IR;
        codegen_emit(prog, &opts);
        fclose(df);
        printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);
    }

    /* Phase 4: copy extern files */
    for (AstNode *en = prog->left; en; en = en->next) {
        if (en->kind != NODE_EXTERN || !en->left) continue;
        const char *fn = en->left->value, *f = fn;
        if (f[0] == '"' || f[0] == '<') f++;
        size_t fl = strlen(f);
        if (fl > 0 && (f[fl-1] == '"' || f[fl-1] == '>')) fl--;
        char bare[256];
        if (fl >= sizeof(bare)) fl = sizeof(bare) - 1;
        memcpy(bare, f, fl);
        bare[fl] = '\0';

        char check[2048];
        snprintf(check, sizeof(check), "%s/%s", out_dir, bare);
        FILE *test = fopen(check, "rb");
        if (test) { fclose(test); continue; }

        int found = 0;
        const char *search_dirs[] = { lextend_dir, ".", NULL };
        for (int di = 0; search_dirs[di] && !found; di++) {
            char src_path[2048];
            snprintf(src_path, sizeof(src_path), "%s/%s", search_dirs[di], bare);
            if (file_copy_to_dir(src_path, out_dir)) found = 1;
        }
    }

    /* Phase 5: collect DLLs and compile */
    char dll_list[4096] = "";
    for (AstNode *en = prog->left; en; en = en->next) {
        if (en->kind == NODE_EXTERN && en->left) {
            const char *fn = en->left->value, *f = fn;
            if (f[0] == '"' || f[0] == '<') f++;
            size_t fl = strlen(f);
            if (fl > 0 && (f[fl-1] == '"' || f[fl-1] == '>')) fl--;
            size_t rem = sizeof(dll_list) - strlen(dll_list) - 1;
            strncat(dll_list, " \"./", rem);
            rem = sizeof(dll_list) - strlen(dll_list) - 1;
            strncat(dll_list, f, fl < rem ? fl : rem);
            rem = sizeof(dll_list) - strlen(dll_list) - 1;
            strncat(dll_list, "\"", rem);
        }
    }

    snprintf(path, sizeof(path), "%s/main.c", out_dir);
    char exe[2048]; snprintf(exe, sizeof(exe), "%.1024s/%.256s.exe", out_dir, base_only);
    snprintf(cmd, sizeof(cmd), "gcc \"%s\"%s%s -o \"%s\"", path, all_sources, dll_list, exe);
    printf("%s\n", cmd);
    system(cmd);
    /* Always check .exe existence — system() return value unreliable on Windows */
    {
        FILE *check = fopen(exe, "rb");
        if (check) { fclose(check); printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", exe); }
        else fprintf(stderr, CLR_RED "[gcl]" CLR_RESET " error: %s not created\n", exe);
    }

    return 0;
}
