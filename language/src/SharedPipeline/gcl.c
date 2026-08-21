#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcl.h"
#include "Common/gcl_common.h"
#include "Common/gcl_token.h"
#include "Lexer/gcl_lexer.h"
#include "AST/gcl_ast.h"
#include "Parser/gcl_parser.h"
#include "Ir/gcl_ir.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define GCL_MERGED_MAX (1024 * 1024)

/* ── #include preprocessing ──────────────────────── */

static int gcl_read_file(const char *path, char *dst, size_t cap) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    long end;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    end = ftell(fp);
    if (end < 0 || (size_t)end >= cap) { fclose(fp); return -1; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }
    size_t n = (size_t)end;
    if (n > 0 && fread(dst, 1, n, fp) != n) { fclose(fp); return -1; }
    dst[n] = '\0';
    fclose(fp);
    return (int)n;
}

static int gcl_file_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

static void gcl_derive_dir(const char *filepath, char *dir, size_t cap) {
    dir[0] = '\0';
    if (!filepath) return;
    const char *last_sep = filepath;
    for (const char *p = filepath; *p; p++) {
        if (*p == '/' || *p == '\\') last_sep = p + 1;
    }
    size_t dlen = (size_t)(last_sep - filepath);
    if (dlen > 0 && dlen < cap) {
        memcpy(dir, filepath, dlen);
        dir[dlen] = '\0';
    }
}

/* Merges #include/#lib file contents into the source buffer.
 * Returns 0 on success, -1 on missing include file. */
static int gcl_merge_includes(const char *source, const char *filepath,
                              char *out, size_t cap) {
    char dir[4096];
    gcl_derive_dir(filepath, dir, sizeof dir);

    size_t pos = 0;
    const char *p = source;
    while (*p && pos + 16 < cap) {
        /* Detect a directive at line start */
        const char *nl = strchr(p, '\n');
        size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
        const char *t = p;
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '#') {
            const char *k = t + 1;
            while (*k == ' ' || *k == '\t') k++;
            if (strncmp(k, "include", 7) == 0 && (k[7] == ' ' || k[7] == '\t' || k[7] == '<' || k[7] == '"')) {
                const char *v = k + 7;
                while (*v == ' ' || *v == '\t') v++;
                char name[512] = {0};
                if (*v == '<' || *v == '"') {
                    char closec = (*v == '<') ? '>' : '"';
                    const char *s = v + 1;
                    const char *e = s;
                    while (*e && *e != closec) e++;
                    size_t nlen = (size_t)(e - s);
                    if (nlen == 0 || nlen >= sizeof name) goto next_line;
                    memcpy(name, s, nlen);
                    name[nlen] = '\0';
                } else {
                    /* #include filename (no quotes/angle) */
                    const char *s = v;
                    const char *e = s;
                    while (*e && *e != '\n' && *e != '\r' && *e != ' ' && *e != '\t') e++;
                    size_t nlen = (size_t)(e - s);
                    if (nlen == 0 || nlen >= sizeof name) goto next_line;
                    memcpy(name, s, nlen);
                    name[nlen] = '\0';
                }

                /* Try: name, name.gcsf, name.gclib */
                char candidate[1024];
                char full[1088];
                size_t dlen = strlen(dir);
                int found = 0;
                const char *exts[3] = {"", ".gcsf", ".gclib"};
                for (int ei = 0; ei < 3; ei++) {
                    if (dlen + strlen(name) + strlen(exts[ei]) + 1 >= sizeof full) continue;
                    memcpy(full, dir, dlen);
                    strcpy(full + dlen, name);
                    if (ei > 0) strcat(full, exts[ei]);
                    if (gcl_file_exists(full)) {
                        if (gcl_read_file(full, candidate, sizeof candidate) < 0) {
                            return -1; /* exists but unreadable */
                        }
                        /* Process merged content: make all #define directives public by
                         * putting "public" on its own line before #define */
                        const char *src = candidate;
                        fprintf(stderr, "[gcl_merge] Processing include file: %s\n", full);
                        while (*src && pos + 1 < cap) {
                            const char *line_start = src;
                            const char *nl = strchr(src, '\n');
                            size_t line_len = nl ? (size_t)(nl - src) : strlen(src);
                            
                            /* Check if line is a #define without public/private prefix */
                            const char *t = line_start;
                            while (*t && (*t == ' ' || *t == '\t')) t++;
                            if (*t == '#') {
                                const char *k = t + 1;
                                while (*k && (*k == ' ' || *k == '\t')) k++;
                                if (strncmp(k, "define", 6) == 0 && 
                                    (k[6] == ' ' || k[6] == '\t' || k[6] == '\n' || k[6] == '\0')) {
                                    /* Check if already has public/private prefix in previous context */
                                    const char *prefix_check = line_start;
                                    while (*prefix_check && (*prefix_check == ' ' || *prefix_check == '\t')) prefix_check++;
                                    bool has_qualifier = (strncmp(prefix_check, "public", 6) == 0 || 
                                                         strncmp(prefix_check, "private", 7) == 0);
                                    
                                    fprintf(stderr, "[gcl_merge] Found #define line, has_qualifier=%d: %.*s\n", 
                                            has_qualifier, (int)line_len, line_start);
                                    
                                    if (!has_qualifier) {
                                        /* Insert "public" on its own line before #define */
                                        if (pos + 8 >= cap) return -1; /* "public\n" = 7 + \n */
                                        fprintf(stderr, "[gcl_merge] Adding public prefix\n");
                                        memcpy(out + pos, "public\n", 7);
                                        pos += 7;
                                    }
                                }
                            }
                            
                            /* Copy the line */
                            if (pos + line_len + 1 >= cap) return -1;
                            memcpy(out + pos, line_start, line_len);
                            pos += line_len;
                            out[pos++] = '\n';
                            
                            if (!nl) break;
                            src = nl + 1;
                        }
                        fprintf(stderr, "[gcl_merge] Include file processed\n");
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    /* Missing include: emit comment so parse can proceed */
                    if (pos + 64 >= cap) return -1;
                    pos += (size_t)snprintf(out + pos, cap - pos - 1,
                                            "#warning \"missing include: %s\"\n", name);
                }
                goto next_line;
            }
        }
        /* Copy the whole line verbatim */
        if (pos + linelen + 2 >= cap) return -1;
        memcpy(out + pos, p, linelen);
        pos += linelen;
        out[pos++] = '\n';
next_line:
        if (!nl) break;
        p = nl + 1;
    }
    if (pos >= cap) return -1;
    out[pos] = '\0';
    return 0;
}

/* External declarations for modules */
extern void gcl_semantic_init(GclDiagBag *diag, const char *filepath);
extern int  gcl_semantic_check(const GclAstNode *root);
extern void gcl_typecheck_init(GclDiagBag *diag, const char *filepath);
extern int  gcl_typecheck_walk(const GclAstNode *root);
extern void gcl_mem_init(void);
extern void gcl_gc_init(void);
extern void gcl_gc_shutdown(void);
extern void gcl_linker_init(void);
extern void gcl_mem_free_all(void);

/* FastIR interpreter (in src/FastIR/) */
extern int gcl_interp_run(const GclIrProgram *prog);
extern int gcl_interp_run_with_path(const GclIrProgram *prog, const char *filepath);

/* ── gcl_dump_tokens ─────────────────────────────── */

int gcl_dump_tokens(const char *source) {
    GclArena arena;
    GclStringIntern intern;
    GclLexer lex;

    gcl_arena_init(&arena);
    gcl_intern_init(&intern, &arena);
    gcl_lexer_init(&lex, source, &arena, &intern);

    printf("--- Token Dump ---\n");
    for (;;) {
        GclToken tok = gcl_lexer_next(&lex);
        printf("  [%d:%d] %-14s ", tok.line, tok.col, gcl_token_kind_name(tok.kind));
        if (tok.length > 0 && tok.start) {
            printf("'%.*s'", (int)tok.length, tok.start);
        }
        printf("\n");
        if (tok.kind == TOK_EOF) break;
    }
    printf("--- End Tokens ---\n");

    gcl_intern_free(&intern);
    gcl_arena_free(&arena);
    return 0;
}

/* ── gcl_dump_ast ────────────────────────────────── */

int gcl_dump_ast(const char *source) {
    GclArena arena;
    GclStringIntern intern;
    GclDiagBag diag;
    GclParser parser;

    gcl_arena_init(&arena);
    gcl_intern_init(&intern, &arena);
    gcl_diag_bag_init(&diag);

    gcl_parser_init(&parser, source, &arena, &intern, &diag, "<input>");
    GclAstNode *ast = gcl_parser_parse(&parser);

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
    }

    printf("--- AST Dump ---\n");
    gcl_ast_dump(ast, 0);
    printf("--- End AST ---\n");

    gcl_intern_free(&intern);
    gcl_arena_free(&arena);
    return diag.error_count;
}

/* ── gcl_dump_ir ─────────────────────────────────── */

int gcl_dump_ir(const char *source) {
    GclArena arena;
    GclStringIntern intern;
    GclDiagBag diag;
    GclParser parser;
    GclIrProgram ir;

    gcl_arena_init(&arena);
    gcl_intern_init(&intern, &arena);
    gcl_diag_bag_init(&diag);

    gcl_parser_init(&parser, source, &arena, &intern, &diag, "<input>");
    GclAstNode *ast = gcl_parser_parse(&parser);

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
        gcl_intern_free(&intern);
        gcl_arena_free(&arena);
        return diag.error_count;
    }

    gcl_ir_init(&ir, &arena, &diag);
    gcl_ir_gen(&ir, ast);
    gcl_ir_dump(&ir);

    gcl_intern_free(&intern);
    gcl_arena_free(&arena);
    return diag.error_count;
}

/* ── gcl_full_pipeline (check only, no exec) ─────── */

int gcl_full_pipeline(const char *source, GclDiagBag *diag) {
    GclArena arena;
    GclStringIntern intern;
    GclParser parser;

    gcl_arena_init(&arena);
    gcl_intern_init(&intern, &arena);

    gcl_parser_init(&parser, source, &arena, &intern, diag, "<input>");
    GclAstNode *ast = gcl_parser_parse(&parser);

    if (diag->error_count == 0) {
        gcl_semantic_init(diag, "<input>");
        gcl_semantic_check(ast);
    }
    if (diag->error_count == 0) {
        gcl_typecheck_init(diag, "<input>");
        gcl_typecheck_walk(ast);
    }

    if (diag->error_count > 0) {
        gcl_diag_print_all(diag);
    }

    gcl_intern_free(&intern);
    gcl_arena_free(&arena);
    return diag->error_count;
}

/* ── preprocessor output directives (#warning/#debug/#error) ──
 * Walks the AST and prints the directive text: warning=yellow,
 * debug=blue, error=red (ANSI on Windows 10+ terminals).
 * #error increments the diag error count so the pipeline stops. */
static void gcl_emit_pp_directives(const GclAstNode *node, GclDiagBag *diag,
                                   const char *filepath) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        const GclAstNode *child = node->children[i];
        if (!child) continue;
        switch (child->kind) {
        case AST_PP_WARNING:
            if (child->str_value) {
                fprintf(stdout, "\x1b[33m%s:%d:%d: warning: %s\x1b[0m\n",
                        filepath ? filepath : "<input>", child->line, child->col,
                        child->str_value);
            }
            break;
        case AST_PP_DEBUG:
            if (child->str_value) {
                fprintf(stdout, "\x1b[34m%s:%d:%d: debug: %s\x1b[0m\n",
                        filepath ? filepath : "<input>", child->line, child->col,
                        child->str_value);
            }
            break;
        case AST_PP_ERROR:
            if (child->str_value) {
                fprintf(stderr, "\x1b[31m%s:%d:%d: error: %s\x1b[0m\n",
                        filepath ? filepath : "<input>", child->line, child->col,
                        child->str_value);
            }
            if (diag) diag->error_count++;
            break;
        default:
            break;
        }
        gcl_emit_pp_directives(child, diag, filepath);
    }
}

/* ── gcl_run_file (full pipeline + interpreter) ───── */

int gcl_run_file(const char *source, const char *filepath) {
    GclArena arena;
    GclStringIntern intern;
    GclDiagBag diag;
    GclParser parser;
    GclIrProgram ir;

    /* Çökme noktasını görmek için stdout'u unbuffered yap */
    setvbuf(stdout, NULL, _IONBF, 0);

    fprintf(stderr, "[gcl_debug] gcl_run_file entered (filepath=%s)\n",
            filepath ? filepath : "?");
    fflush(stderr);

    gcl_arena_init(&arena);
    gcl_intern_init(&intern, &arena);
    gcl_diag_bag_init(&diag);

    /* Init subsystems */
    gcl_mem_init();
    gcl_gc_init();
    gcl_linker_init();

    printf("gcl: stage 1/6  init pipeline\n");

    /* Stage 2: Parse (with #include merge) */
    static char merged[GCL_MERGED_MAX]; /* static: 1MB stack buffer taşmaz */
    const char *parse_src = source;
    if (gcl_merge_includes(source, filepath, merged, sizeof merged) == 0) {
        if (merged[0] != '\0') parse_src = merged;
    }
    printf("gcl: stage 2/6 - parsing (%zu bytes)\n", strlen(parse_src));
    gcl_parser_init(&parser, parse_src, &arena, &intern, &diag, filepath);
    GclAstNode *ast = gcl_parser_parse(&parser);
    printf("gcl: stage 2/6 - parse OK (%d decls)\n", ast ? ast->child_count : 0);

    /* Part 1: #warning (yellow) / #debug (blue) / #error (red, stops build) */
    gcl_emit_pp_directives(ast, &diag, filepath);

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
        gcl_intern_free(&intern);
        gcl_arena_free(&arena);
        return 1;
    }

    /* Stage 3: Semantic */
    printf("gcl: stage 3/6 - semantic check\n");
    gcl_semantic_init(&diag, filepath);
    gcl_semantic_check(ast);

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
        gcl_intern_free(&intern);
        gcl_arena_free(&arena);
        return 1;
    }

    /* Stage 4: Type check */
    printf("gcl: stage 4/6 - type check\n");
    gcl_typecheck_init(&diag, filepath);
    gcl_typecheck_walk(ast);
    printf("gcl: stage 4/6 - check OK\n");

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
        gcl_intern_free(&intern);
        gcl_arena_free(&arena);
        return 1;
    }

    /* Stage 5: IR Generation */
    printf("gcl: stage 5/6 - IR generation\n");
    gcl_ir_init(&ir, &arena, &diag);
    gcl_ir_gen(&ir, ast);
    printf("gcl: stage 5/6 - IR OK (%d instructions)\n", ir.count);

    if (diag.error_count > 0) {
        gcl_diag_print_all(&diag);
        gcl_intern_free(&intern);
        gcl_arena_free(&arena);
        return 1;
    }

    /* Stage 6: Interpreter */
    printf("gcl: stage 6/6 - interpreter run\n");
    int result = gcl_interp_run_with_path(&ir, filepath);

    /* Cleanup */
    gcl_gc_shutdown();
    gcl_mem_free_all();
    gcl_intern_free(&intern);
    gcl_arena_free(&arena);

    return result;
}
