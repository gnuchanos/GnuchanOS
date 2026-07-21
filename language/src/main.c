#include "lexer.h"
#include "parser.h"
#include "defines.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static char *resolve_path(const char *src_dir, const char *filename) {
    const char *f = filename;
    if (f[0] == '"' || f[0] == '<') f++;
    size_t flen = strlen(f);
    if (flen > 0 && (f[flen - 1] == '"' || f[flen - 1] == '>')) flen--;

    size_t dlen = strlen(src_dir);
    char *full = malloc(dlen + flen + 16);
    sprintf(full, "%.*s%.*s", (int)dlen, src_dir, (int)flen, f);
    char *content = read_file(full);
    if (content) return content;
    free(full);

    full = malloc(dlen + flen + 8);
    sprintf(full, "%.*s%.*s.gcsf", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full);
    if (content) { free(full); return content; }
    free(full);

    full = malloc(dlen + flen + 8);
    sprintf(full, "%.*s%.*s.gclib", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full);
    if (content) { free(full); return content; }
    free(full);

    full = malloc(dlen + flen + 4);
    sprintf(full, "%.*s%.*s.h", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full);
    if (content) { free(full); return content; }
    free(full);

    return NULL;
}

static void process_defines(AstNode *prog) {
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_DEFINE) {
            const char *val = n->left ? n->left->value : "";
            defines_set(n->value, val);
        }
        if (n->kind == NODE_EXTERN_C_BLOCK) {
            AstNode *inner = n->left;
            while (inner) {
                if (inner->kind == NODE_DEFINE) {
                    const char *sym = inner->left ? inner->left->value : "";
                    defines_set(inner->value, sym);
                    defines_add_extern(sym);
                }
                inner = inner->next;
            }
        }
        n = n->next;
    }
}

/* append node to end of program's child list */
static void prog_append(AstNode *prog, AstNode *node) {
    if (!prog || !node) return;
    node->next = NULL;
    AstNode *tail = prog->left;
    if (!tail) { prog->left = node; return; }
    while (tail->next) tail = tail->next;
    tail->next = node;
}

/* merge all defines and extern blocks from included file into main AST */
static void merge_includes(AstNode *prog, AstNode *iprog) {
    AstNode *cn = iprog->left;
    while (cn) {
        AstNode *next = cn->next;
        if (cn->kind == NODE_DEFINE || cn->kind == NODE_EXTERN_C_BLOCK || cn->kind == NODE_EXTERN) {
            cn->next = NULL;
            prog_append(prog, cn);
        }
        cn = next;
    }
}

static void preprocess_load(AstNode *prog, const char *src_dir) {
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_INCLUDE || n->kind == NODE_LIB) {
            const char *fname = n->left ? n->left->value : NULL;
            if (fname) {
                char *content = resolve_path(src_dir, fname);
                if (content) {
                    Lexer il;
                    lexer_init(&il, content, fname);
                    Parser *ip = parser_new(&il);
                    AstNode *iprog = parser_parse(ip);
                    process_defines(iprog);
                    merge_includes(prog, iprog);
                    free(ip);
                    free(content);
                }
            }
        }
        n = n->next;
    }
}

int main(int argc, char **argv) {
    defines_init();

    CodegenOpts opts = { .mode = MODE_EXEC, .output = NULL, .base_name = NULL, .debug_flag = 0 };
    const char *input_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("gcl v0.1 — GnuchanOS C-like Language\n");
            return 0;
        }
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("gcl [flags] <file.gcsf>\n");
            printf("  -lexer       token stream\n");
            printf("  -parser      parse tree\n");
            printf("  -ast         AST dump\n");
            printf("  -codegen     generated C code\n");
            printf("  -ir          interactive (debug print)\n");
            printf("  -o <name>    export: name.c + name.o + name.exe + name.gcdebug\n");
            return 0;
        }
        if (strcmp(argv[i], "-lexer")  == 0) { opts.mode = MODE_LEXER; continue; }
        if (strcmp(argv[i], "-parser") == 0) { opts.mode = MODE_PARSER; continue; }
        if (strcmp(argv[i], "-ast")    == 0) { opts.mode = MODE_AST; continue; }
        if (strcmp(argv[i], "-codegen")== 0) { opts.mode = MODE_CODEGEN; continue; }
        if (strcmp(argv[i], "-ir")     == 0) { opts.mode = MODE_IR; continue; }
        if (strcmp(argv[i], "-debug")  == 0) { opts.debug_flag = 1; continue; }
        if (strcmp(argv[i], "-linclude") == 0) continue;
        if (strcmp(argv[i], "-llib")     == 0) continue;
        if (strcmp(argv[i], "-lextend")  == 0) continue;
        if (strcmp(argv[i], "-o")        == 0 && i + 1 < argc) {
            opts.base_name = argv[++i]; continue;
        }
        input_file = argv[i];
    }

    if (!input_file) {
        fprintf(stderr, "gcl: no input file\n");
        return 1;
    }

    char *source = read_file(input_file);
    if (!source) {
        fprintf(stderr, "gcl: cannot open '%s'\n", input_file);
        return 1;
    }

    char src_dir[1024] = "";
    {
        const char *slash = strrchr(input_file, '/');
        const char *bslash = strrchr(input_file, '\\');
        const char *last = slash > bslash ? slash : bslash;
        if (last) {
            size_t dlen = last - input_file + 1;
            memcpy(src_dir, input_file, dlen);
            src_dir[dlen] = '\0';
        }
    }

    Lexer lexer;
    lexer_init(&lexer, source, input_file);

    if (opts.mode == MODE_LEXER && !opts.base_name) {
        Token t;
        do {
            t = lexer_next(&lexer);
            printf("TOK[%d] line=%zu col=%zu '%.*s'\n", t.kind, t.line, t.col, (int)t.len, t.text);
        } while (t.kind != TOK_EOF);
        free(source);
        return 0;
    }

    Parser *parser = parser_new(&lexer);
    AstNode *prog = parser_parse(parser);

    preprocess_load(prog, src_dir);
    process_defines(prog);

    /* === EXPORT === */
    if (opts.base_name) {
        char path[2048], cmd[4096];

        snprintf(path, sizeof(path), "%s.c", opts.base_name);
        FILE *cf = fopen(path, "w");
        if (!cf) { fprintf(stderr,"gcl: cannot write '%s'\n",path); free(source);free(parser);return 1; }
        opts.output = cf;
        opts.mode = MODE_CODEGEN;
        codegen_emit(prog, &opts);
        fclose(cf);
        printf("[gcl] %s\n", path);

        snprintf(path, sizeof(path), "%s.gcdebug", opts.base_name);
        FILE *df = fopen(path, "w");
        if (df) { opts.output=df; opts.mode=MODE_IR; codegen_emit(prog,&opts); fclose(df); printf("[gcl] %s\n", path); }

        snprintf(path, sizeof(path), "%s.c", opts.base_name);
        char obj[1024], exe[1024];
        snprintf(obj, sizeof(obj), "%s.o", opts.base_name);
        snprintf(exe, sizeof(exe), "%s.exe", opts.base_name);

        /* collect all .dll references from #extern nodes */
        char dll_list[4096] = "";
        {
            AstNode *en = prog->left;
            while (en) {
                if (en->kind == NODE_EXTERN && en->left) {
                    const char *fn = en->left->value;
                    const char *f = fn;
                    if (f[0] == '"' || f[0] == '<') f++;
                    size_t fl = strlen(f);
                    if (fl > 0 && (f[fl - 1] == '"' || f[fl - 1] == '>')) fl--;
                    strncat(dll_list, " \"", sizeof(dll_list) - strlen(dll_list) - 1);
                    strncat(dll_list, f, fl);
                    strncat(dll_list, "\"", sizeof(dll_list) - strlen(dll_list) - 1);
                }
                en = en->next;
            }
        }

        snprintf(cmd, sizeof(cmd), "gcc -c \"%s\" -o \"%s\"", path, obj);
        if (system(cmd) == 0) {
            printf("[gcl] %s\n", obj);
            snprintf(cmd, sizeof(cmd), "gcc \"%s\"%s -o \"%s\"", obj, dll_list, exe);
            if (system(cmd) == 0) printf("[gcl] %s\n", exe);
        }
        defines_free(); free(source); free(parser);
        return 0;
    }

    /* === INTERACTIVE === */
    opts.output = stdout;
    codegen_emit(prog, &opts);

    defines_free(); free(source); free(parser);
    return 0;
}
