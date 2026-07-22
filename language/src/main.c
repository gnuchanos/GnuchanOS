#include "lexer.h"
#include "parser.h"
#include "defines.h"
#include "codegen.h"
#include "codegen_c.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- file I/O ---------- */

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
    char full[2048];

    snprintf(full, sizeof(full), "%.*s%.*s", (int)dlen, src_dir, (int)flen, f);
    char *content = read_file(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.gcsf", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.gclib", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full); if (content) return content;
    snprintf(full, sizeof(full), "%.*s%.*s.h", (int)dlen, src_dir, (int)flen, f);
    content = read_file(full); if (content) return content;
    return NULL;
}

/* ---------- symbol table helpers ---------- */

static int exec_define(AstNode *n) {
    if (!n || n->kind != NODE_DEFINE) return 0;
    defines_set(n->value, n->left ? n->left->value : "");
    return 1;
}

static void exec_extern_block(AstNode *n) {
    if (!n || n->kind != NODE_EXTERN_C_BLOCK) return;
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

/* ---------- condition evaluation ---------- */

static int eval_name(const char *name) {
    return name ? defines_exists(name) : 0;
}

static int smart_compare(const char *lv, const char *rv) {
    if (!lv) lv = "0";
    if (!rv) rv = "0";
    char *le = NULL, *re = NULL;
    long ln = strtol(lv, &le, 10), rn = strtol(rv, &re, 10);
    if (le && *le == '\0' && re && *re == '\0') {
        if (ln < rn) return -1;
        if (ln > rn) return 1;
        return 0;
    }
    return strcmp(lv, rv);
}

static int eval_condition(AstNode *n) {
    if (!n) return 0;
    if (n->left && n->left->kind == NODE_IDENT && strcmp(n->left->value, "defined") == 0)
        return defines_exists(n->value);
    if (n->left && n->left->kind == NODE_IDENT) {
        const char *op = n->left->value;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "<") == 0 ||
            strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
            const char *lv = defines_get(n->value);
            const char *rv = n->right ? n->right->value : NULL;
            if (!lv) lv = "0";
            if (!rv) rv = "0";
            int cmp = smart_compare(lv, rv);
            if (strcmp(op, "==") == 0) return cmp == 0;
            if (strcmp(op, "!=") == 0) return cmp != 0;
            if (strcmp(op, "<")  == 0) return cmp < 0;
            if (strcmp(op, ">")  == 0) return cmp > 0;
            if (strcmp(op, "<=") == 0) return cmp <= 0;
            if (strcmp(op, ">=") == 0) return cmp >= 0;
            return 0;
        }
    }
    if (eval_name(n->value)) return 1;
    AstNode *alt = n->right;
    while (alt) { if (eval_name(alt->value)) return 1; alt = alt->next; }
    return 0;
}

/* ---------- inline preprocessor ---------- */

static AstNode *preprocess_inline_ex(AstNode *prog, int keep_all) {
    AstNode dummy = {0}; AstNode *tail = &dummy;
    AstNode *n = prog->left;
    #define CSTACK_MAX 64
    int cstack[CSTACK_MAX] = {0}; int csp = 0; cstack[0] = 1;

    while (n) {
        AstNode *next = n->next;
        if (n->kind == NODE_IFDEF || n->kind == NODE_IFNDEF || n->kind == NODE_IF) {
            csp++; if (csp >= CSTACK_MAX) break;
            if (cstack[csp - 1] != 1) cstack[csp] = 0;
            else {
                int ok = 0;
                if (n->kind == NODE_IFDEF) ok = defines_exists(n->value);
                else if (n->kind == NODE_IFNDEF) ok = !defines_exists(n->value);
                else ok = eval_condition(n);
                cstack[csp] = ok ? 1 : 0;
            }
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ELIF) {
            if (csp <= 0) { n = next; continue; }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp - 1] == 1) cstack[csp] = eval_condition(n) ? 1 : 0;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ELSE) {
            if (csp <= 0) { n = next; continue; }
            if (cstack[csp] == 1 || cstack[csp] == 2) cstack[csp] = 2;
            else if (cstack[csp] == 0 && cstack[csp - 1] == 1) cstack[csp] = 1;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }
        if (n->kind == NODE_ENDIF) {
            if (csp > 0) csp--;
            n->next = NULL; tail->next = n; tail = n; n = next; continue;
        }

        int active = 1;
        for (int i = 0; i <= csp; i++) { if (cstack[i] != 1) { active = 0; break; } }
        if (!active && !keep_all) { n = next; continue; }

        if (active) {
            if (n->kind == NODE_DEFINE) exec_define(n);
            else if (n->kind == NODE_UNDEF) defines_undef(n->value);
            else if (n->kind == NODE_EXTERN_C_BLOCK) exec_extern_block(n);
            else if (n->kind == NODE_ERROR) {
                fprintf(stderr, CLR_RED "#error:" CLR_RESET " %s\n", n->left ? n->left->value : "");
            } else if (n->kind == NODE_MESSAGE && !keep_all && n->left && n->left->value) {
                printf(CLR_CYAN "#message:" CLR_RESET " %s\n", n->left->value);
            }
        }

        if (n->kind == NODE_DEFINE || n->kind == NODE_UNDEF || n->kind == NODE_EXTERN_C_BLOCK ||
            n->kind == NODE_ERROR || n->kind == NODE_MESSAGE || n->kind == NODE_EXTERN ||
            n->kind == NODE_INCLUDE || n->kind == NODE_LIB || n->kind == NODE_PRAGMA ||
            n->kind == NODE_DEBUG || n->kind == NODE_RAW || active) {
            n->next = NULL; tail->next = n; tail = n;
        }
        n = next;
    }
    #undef CSTACK_MAX
    return dummy.next;
}

static AstNode *preprocess_inline(AstNode *prog) { return preprocess_inline_ex(prog, 0); }
static AstNode *preprocess_codegen(AstNode *prog) { return preprocess_inline_ex(prog, 1); }

/* ---------- included file tracking ---------- */

#define MAX_INCLUDED 256
static struct {
    char        name[256];
    AstNode    *ast;
    int         is_lib;   /* 1 = .gclib (#lib), 0 = .gcsf (#include) */
} g_included[MAX_INCLUDED];
static int g_included_count = 0;

static int already_included(const char *name) {
    for (int i = 0; i < g_included_count; i++)
        if (strcmp(g_included[i].name, name) == 0) return 1;
    return 0;
}

static void register_included(const char *name, AstNode *ast, int is_lib) {
    if (g_included_count >= MAX_INCLUDED) return;
    /* trim brackets/quotes from name */
    const char *start = name;
    if (start[0] == '<' || start[0] == '"') start++;
    size_t len = strlen(start);
    if (len > 0 && (start[len-1] == '>' || start[len-1] == '"')) len--;
    if (len >= sizeof(g_included[0].name)) len = sizeof(g_included[0].name) - 1;
    memcpy(g_included[g_included_count].name, start, len);
    g_included[g_included_count].name[len] = '\0';
    g_included[g_included_count].ast    = ast;
    g_included[g_included_count].is_lib = is_lib;
    g_included_count++;
}

/* ---------- load all #include / #lib ---------- */

static void preprocess_load(AstNode *prog, const char *src_dir) {
    AstNode *n = prog->left;
    while (n) {
        if (n->kind == NODE_INCLUDE || n->kind == NODE_LIB) {
            const char *fname = n->left ? n->left->value : NULL;
            if (fname) {
                /* trim brackets/quotes */
                const char *start = fname;
                if (start[0] == '"' || start[0] == '<') start++;
                size_t len = strlen(start);
                if (len > 0 && (start[len-1] == '"' || start[len-1] == '>')) len--;
                char trimmed[256];
                if (len >= sizeof(trimmed)) len = sizeof(trimmed) - 1;
                memcpy(trimmed, start, len);
                trimmed[len] = '\0';

                if (!already_included(trimmed)) {
                    char *content = resolve_path(src_dir, fname);
                    if (content) {
                        Lexer il; lexer_init(&il, content, fname);
                        Parser *ip = parser_new(&il);
                        AstNode *iprog = parser_parse(ip);
                        iprog->left = preprocess_inline(iprog);
                        register_included(trimmed, iprog, n->kind == NODE_LIB ? 1 : 0);
                        free(ip); free(content);
                    }
                }
            }
        }
        n = n->next;
    }
}

/* ---------- export: multi-file C project ---------- */

static int copy_file_to_dir(const char *src, const char *dst_dir) {
    /* open source, read all, write to dest dir with same basename */
    FILE *fs = fopen(src, "rb");
    if (!fs) return 0;
    fseek(fs, 0, SEEK_END);
    long sz = ftell(fs);
    fseek(fs, 0, SEEK_SET);
    char *buf = malloc(sz);
    if (!buf) { fclose(fs); return 0; }
    fread(buf, 1, sz, fs);
    fclose(fs);

    /* extract basename */
    const char *fname = strrchr(src, '/');
    const char *fname2 = strrchr(src, '\\');
    if (fname2 > fname) fname = fname2;
    fname = fname ? fname + 1 : src;

    char dst[2048];
    snprintf(dst, sizeof(dst), "%.1024s/%.256s", dst_dir, fname);
    FILE *fd = fopen(dst, "wb");
    if (!fd) { free(buf); return 0; }
    fwrite(buf, 1, sz, fd);
    fclose(fd);
    free(buf);
    printf(CLR_PURPLE "[gcl]" CLR_RESET " copied %s → %s\n", src, dst);
    return 1;
}

static int export_project(AstNode *prog, const char *base_name, const char *lextend_dir) {
    char path[4096], cmd[8192];
    char clean_base[1024];
    snprintf(clean_base, sizeof(clean_base), "%s", base_name);
    size_t blen = strlen(clean_base);
    if (blen > 2 && strcmp(clean_base + blen - 2, ".c") == 0) clean_base[blen - 2] = '\0';

    /* extract basename and create output folder */
    const char *base_only = strrchr(clean_base, '/');
    const char *base_bs = strrchr(clean_base, '\\');
    base_only = (base_bs > base_only) ? base_bs : base_only;
    base_only = base_only ? base_only + 1 : clean_base;

    char out_dir[1024];
    snprintf(out_dir, sizeof(out_dir), "%s", clean_base);
    /* remove trailing project name to get parent dir, then append project name */
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

    /* create output directory (cross-platform) */
    {
        char mkcmd[2048];
#ifdef _WIN32
        snprintf(mkcmd, sizeof(mkcmd), "if not exist \"%.900s\" mkdir \"%.900s\"", out_dir, out_dir);
#else
        snprintf(mkcmd, sizeof(mkcmd), "mkdir -p \"%.1024s\"", out_dir);
#endif
        system(mkcmd);
        /* also handle the case where out_dir exists as a regular file */
        { FILE *test = fopen(out_dir, "r"); if (test) { fclose(test); } else { /* ok */ } }
    }

    CodegenOpts opts = { .mode = MODE_CODEGEN };
    char all_sources[4096] = "";

    /* Phase 1: emit .h + optional .c for each included file */
    for (int i = 0; i < g_included_count; i++) {
        if (!g_included[i].ast) continue;

        /* ---- .h header ---- */
        snprintf(path, sizeof(path), "%.1024s/%.256s.h", out_dir, g_included[i].name);
        FILE *hdr = fopen(path, "w");
        if (!hdr) continue;
        g_codegen_out = hdr;
        codegen_c_emit_header(g_included[i].ast, g_included[i].name);
        fclose(hdr);
        printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);

        /* ---- .c source (only for .gcsf #includes, not .gclib #lib) ---- */
        if (!g_included[i].is_lib) {
            snprintf(path, sizeof(path), "%.1024s/%.256s.c", out_dir, g_included[i].name);
            FILE *src = fopen(path, "w");
            if (!src) continue;
            g_codegen_out = src;
            fprintf(src, "#include \"%s.h\"\n\n", g_included[i].name);
            codegen_c_emit_source(g_included[i].ast);
            fclose(src);
            printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);
            strncat(all_sources, " \"", sizeof(all_sources) - strlen(all_sources) - 1);
            strncat(all_sources, path, sizeof(all_sources) - strlen(all_sources) - 1);
            strncat(all_sources, "\"", sizeof(all_sources) - strlen(all_sources) - 1);
        }
    }

    /* Phase 2: main .c file */
    snprintf(path, sizeof(path), "%s/main.c", out_dir);
    FILE *cf = fopen(path, "w");
    if (!cf) return 1;
    opts.output = cf;

    /* #include header with #includes and runtime */
    for (int i = 0; i < g_included_count; i++)
        fprintf(cf, "#include \"%s.h\"\n", g_included[i].name);
    fprintf(cf, "\n");

    opts.mode = MODE_CODEGEN;
    codegen_emit(prog, &opts);
    fclose(cf);
    printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path);

    /* Phase 3: .gcdebug */
    snprintf(path, sizeof(path), "%s/main.gcdebug", out_dir);
    FILE *df = fopen(path, "w");
    if (df) { opts.output = df; opts.mode = MODE_IR; codegen_emit(prog, &opts); fclose(df);
        printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", path); }

    /* Phase 4: copy extern files (.dll/.so/.a) into project folder */
    {
        /* search each #extern reference in lextend directories */
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

            /* skip if already in output dir */
            char check[2048];
            snprintf(check, sizeof(check), "%s/%s", out_dir, bare);
            FILE *test = fopen(check, "rb");
            if (test) { fclose(test); continue; }

            /* try each lextend dir + src dir */
            int found = 0;
            const char *search_dirs[] = { lextend_dir, ".", NULL };
            for (int di = 0; search_dirs[di] && !found; di++) {
                if (!search_dirs[di]) continue;
                char src_path[2048];
                snprintf(src_path, sizeof(src_path), "%s/%s", search_dirs[di], bare);
                if (copy_file_to_dir(src_path, out_dir)) found = 1;
            }
        }
    }

    /* Phase 6: collect extern DLLs for linker */
    char dll_list[4096] = "";
    for (AstNode *en = prog->left; en; en = en->next) {
        if (en->kind == NODE_EXTERN && en->left) {
            const char *fn = en->left->value, *f = fn;
            if (f[0] == '"' || f[0] == '<') f++;
            size_t fl = strlen(f);
            if (fl > 0 && (f[fl-1] == '"' || f[fl-1] == '>')) fl--;
            strncat(dll_list, " \"./", sizeof(dll_list) - strlen(dll_list) - 1);
            strncat(dll_list, f, fl);
            strncat(dll_list, "\"", sizeof(dll_list) - strlen(dll_list) - 1);
        }
    }

    /* Phase 5: compile + link */
    snprintf(path, sizeof(path), "%s/main.c", out_dir);
    char exe[2048]; snprintf(exe, sizeof(exe), "%.1024s/%.256s.exe", out_dir, base_only);
    snprintf(cmd, sizeof(cmd), "gcc \"%s\"%s%s -o \"%s\"", path, all_sources, dll_list, exe);
    printf(CLR_DIM "%s" CLR_RESET "\n", cmd);
    if (system(cmd) == 0) printf(CLR_PURPLE "[gcl]" CLR_RESET " %s\n", exe);

    return 0;
}

/* ========== main ========== */

int main(int argc, char **argv) {
    defines_init();

    CodegenOpts opts = { .mode = MODE_EXEC, .output = NULL, .base_name = NULL, .debug_flag = 0 };
    const char *input_file = NULL;
    const char *linclude_dir = NULL; (void)linclude_dir;
    const char *llib_dir = NULL;     (void)llib_dir;
    const char *lextend_dir = NULL;  (void)lextend_dir;

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
    char *source = read_file(input_file);
    if (!source) {
        char auto_path[1024];
        snprintf(auto_path,sizeof(auto_path),"%s.gcsf",input_file);
        source=read_file(auto_path); if (source) input_file=auto_path;
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
        defines_free(); free(source); free(parser); return 0;
    }

    opts.output = stdout; codegen_emit(prog, &opts);
    defines_free(); free(source); free(parser);
    return 0;
}
