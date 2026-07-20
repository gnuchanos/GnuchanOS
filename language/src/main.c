#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "errors.h"
#include "cli/cli.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "semantic/semantic.h"

// ============================================================
// GCL Compiler — Main Entry Point
// ============================================================

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "error[E031]: file not found: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void lexer_dump_file(const char *filename) {
    char *source = read_file(filename);
    if (!source) return;

    GclLexer lexer;
    lexer_init(&lexer, source);

    printf("=== Token Stream: %s ===\n\n", filename);
    printf("%-6s %-20s %-10s %s\n", "LINE", "TOKEN", "VALUE", "LEXEME");
    printf("------ -------------------- ---------- ----------\n");

    for (;;) {
        GclToken tok = lexer_next_token(&lexer);
        if (tok.type == TOK_EOF) break;
        if (tok.type == TOK_ERROR) {
            printf("%-6d %-20s (error)\n", tok.line, "ERROR");
            continue;
        }
        // Skip comment tokens in dump
        if (tok.type == TOK_GCL_COMMENT || tok.type == TOK_GCL_COMMENT_BLOCK ||
            tok.type == TOK_GCL_COMMENT_CPP) continue;

        const char *tname = token_name(tok.type);
        printf("%-6d %-20s ", tok.line, tname);

        // Print value based on type
        switch (tok.type) {
        case TOK_INT_LITERAL:
            printf("%-10lld ", tok.value.int_val);
            break;
        case TOK_FLOAT_LITERAL:
            printf("%-10g ", tok.value.float_val);
            break;
        case TOK_CHAR_LITERAL:
            printf("'%c'      ", tok.value.char_val);
            break;
        case TOK_STRING_LITERAL:
        case TOK_IDENTIFIER:
        case TOK_PREP_INCLUDE:
        case TOK_PREP_LIB:
        case TOK_PREP_DEFINE:
            printf("%-10.*s ", tok.length > 10 ? 10 : tok.length, tok.lexeme);
            break;
        default:
            printf("%-10s ", "");
            break;
        }

        // Print lexeme snippet
        printf("%.*s\n", tok.length > 20 ? 20 : tok.length, tok.lexeme);
    }

    printf("\n=== End Token Stream ===\n");
    free(source);
}

static void compile_file(const char *filename, const char *output_file, int run_mode) {
    char *source = read_file(filename);
    if (!source) return;

    // ── Lex + Parse ───────────────────────────────────────
    GclParser parser;
    parser_init(&parser, source, filename);
    GclAstNode *ast = parser_parse(&parser);

    if (parser_had_error(&parser) || error_count() > 0) {
        fprintf(stderr, "Compilation failed with %d error(s)\n", error_count());
        ast_free_all(ast);
        free(source);
        exit(1);
    }

    // ── Semantic Analysis ─────────────────────────────────
    int sem_errs = semantic_analyze(ast);
    if (sem_errs > 0) {
        fprintf(stderr, "Compilation failed with %d semantic error(s)\n", sem_errs);
        ast_free_all(ast);
        free(source);
        exit(1);
    }

    // ── Codegen ───────────────────────────────────────────
    // Determine output C file
    char out_c_file[512];
    if (output_file) {
        snprintf(out_c_file, sizeof(out_c_file), "%s", output_file);
    } else {
        // Default: replace .gcsf with .c
        snprintf(out_c_file, sizeof(out_c_file), "%s", filename);
        char *dot = strrchr(out_c_file, '.');
        if (dot) *dot = '\0';
        strcat(out_c_file, ".c");
    }

    codegen_generate_to_file(ast, out_c_file);
    printf("Generated: %s\n", out_c_file);

    // ── Cleanup ───────────────────────────────────────────
    ast_free_all(ast);
    free(source);

    // ── Run mode: compile C to binary + execute ────────────
    if (run_mode) {
        char exe_file[512];
        snprintf(exe_file, sizeof(exe_file), "%s", out_c_file);
        char *dot = strrchr(exe_file, '.');
        if (dot) *dot = '\0';
#ifdef _WIN32
        strcat(exe_file, ".exe");
#endif

        char cmd[2048];
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "gcc -std=gnu99 -o \"%s\" \"%s\" -lm", exe_file, out_c_file);
#else
        snprintf(cmd, sizeof(cmd), "gcc -std=gnu99 -o %s %s -lm", exe_file, out_c_file);
#endif
        printf("Compiling: %s\n", cmd);
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "C compilation failed (exit %d)\n", rc);
            exit(1);
        }

        printf("Running: %s\n", exe_file);
        printf("---\n");
        rc = system(exe_file);
        printf("---\nExit code: %d\n", rc);
    }
}

int main(int argc, char *argv[]) {
    error_init();

    GclCliOptions opts;
    cli_parse(argc, argv, &opts);

    // ── No args → interactive shell (stub) ────────────────
    if (argc < 2) {
        printf("GCL Shell (stub) — Type 'exit' to quit\n");
        printf("Use 'gcl -help' for usage information.\n");
        return 0;
    }

    // ── Version / Help ────────────────────────────────────
    if (opts.version) {
        cli_print_version();
        return 0;
    }
    if (opts.help) {
        cli_print_help();
        return 0;
    }

    // ── Lexer dump ────────────────────────────────────────
    if (opts.lexer_dump && opts.input_file) {
        lexer_dump_file(opts.input_file);
        return 0;
    }

    // ── Parser dump ───────────────────────────────────────
    if (opts.parser_dump && opts.input_file) {
        char *source = read_file(opts.input_file);
        if (!source) return 1;
        GclParser parser;
        parser_init(&parser, source, opts.input_file);
        GclAstNode *ast = parser_parse(&parser);
        printf("=== Parse Tree: %s ===\n\n", opts.input_file);
        ast_dump(ast, 0);
        printf("\n=== End Parse Tree ===\n");
        ast_free_all(ast);
        free(source);
        return 0;
    }

    // ── AST dump ──────────────────────────────────────────
    if (opts.ast_dump && opts.input_file) {
        char *source = read_file(opts.input_file);
        if (!source) return 1;
        GclParser parser;
        parser_init(&parser, source, opts.input_file);
        GclAstNode *ast = parser_parse(&parser);
        printf("=== AST Dump: %s ===\n\n", opts.input_file);
        ast_dump(ast, 0);
        printf("\n=== End AST Dump ===\n");
        ast_free_all(ast);
        free(source);
        return 0;
    }

    // ── Codegen dump (print to stdout) ────────────────────
    if (opts.codegen_dump && opts.input_file) {
        char *source = read_file(opts.input_file);
        if (!source) return 1;
        GclParser parser;
        parser_init(&parser, source, opts.input_file);
        GclAstNode *ast = parser_parse(&parser);
        printf("=== Generated C Code: %s ===\n\n", opts.input_file);
        GclCodegen cg;
        codegen_init(&cg, stdout);
        codegen_generate(&cg, ast);
        printf("\n=== End Generated C Code ===\n");
        ast_free_all(ast);
        free(source);
        return 0;
    }

    // ── Full compile ──────────────────────────────────────
    if (opts.input_file) {
        // Default output
        char out_buf[512];
        const char *out = opts.output_file;
        if (!out) {
            snprintf(out_buf, sizeof(out_buf), "%s", opts.input_file);
            char *dot = strrchr(out_buf, '.');
            if (dot) *dot = '\0';
            strcat(out_buf, ".c");
            out = out_buf;
        }
        compile_file(opts.input_file, out, opts.run_mode);
        return 0;
    }

    // ── Nothing to do ─────────────────────────────────────
    fprintf(stderr, "No input file specified. Use 'gcl -help' for usage.\n");
    return 1;
}
