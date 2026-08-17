/*
 * gcl — Gnuchan C-Like Language: compiler driver (main.c)
 *
 * Scope (only this):
 *   gcl -lexer  file.gcsf | file.gclib
 *   gcl -parser file.gcsf | file.gclib
 *   gcl -ast    file.gcsf | file.gclib
 *   gcl -ir     file.gcsf | file.gclib
 *   gcl -debug -run file.gcsf
 *   gcl -linclude path -llib path -lextern path -run main.gcsf
 *   gcl file.gcsf -o output          (or just "gcl file.gcsf")
 *   gcl -m pip --version             (run python module — embedded runtime)
 *   gcl -pyrun -resolve <mod>|<pre>  (full-system introspection — gcl-lsp)
 *   gcl -libs                        (list Library modules)
 *   gcl -lib|-libcheck <module>      (.gcDL exists? — EXISTS: 0, MISSING: 1)
 *
 * OOM + null + bounds + overflow checks everywhere.
 */

#define _CRT_SECURE_NO_WARNINGS

/* Needed for glibc to expose PATH_MAX/readlink under strict -std=c11.
 * Must appear BEFORE the first include: features.h processes this flag. */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define GCL_MAX_PATH   4096
#define GCL_MAX_PATHS  64
#define GCL_VERSION    "0.1.0"

/* Colors (the 5 purple tones the user chose, 24-bit truecolor) */
#define GCL_COLOR_MAGENTA_DIM    "\033[38;2;47;3;87m"     /* 47,3,87    darkest: detail */
#define GCL_COLOR_MAGENTA_DARK   "\033[38;2;71;4;133m"    /* 71,4,133   dark: secondary */
#define GCL_COLOR_MAGENTA        "\033[38;2;95;5;179m"    /* 95,5,179   mid: info */
#define GCL_COLOR_MAGENTA_BRIGHT "\033[38;2;111;6;209m"   /* 111,6,209  bright: heading */
#define GCL_COLOR_ERROR          "\033[38;2;160;59;255m"  /* 160,59,255 brightest: error/accent */
#define GCL_COLOR_RESET          "\033[0m"

#ifdef _WIN32
#include <windows.h>
#include "src/shell_windows.c"
#else
#include "src/shell_gnuLinux.c"
#endif

/* Lua embed (-luarun): gcl.exe does NOT embed it. Lua 5.4.7 runtime lives
 * in the Library/Lua/lua.gcDL module; at run time it is loaded
 * via gcdl_loader and gcdl_lua_run is called. */
#include "src/gcBuild_System/gcdl_loader.h"
#include "src/gcBuild_System/gclib_utils.h"

/* ── Stage ──────────────────────────────────────────────── */

typedef enum {
    STAGE_FULL = 0,   /* default: full pipeline (build) */
    STAGE_LEXER,
    STAGE_PARSER,
    STAGE_AST,
    STAGE_IR,
    STAGE_RUN,
    STAGE_LUA,
    STAGE_LUAVER,     /* gcl -luarun -version — embedded lua version */
    STAGE_PYTHON,     /* gcl -pyrun script.py */
    STAGE_PYVER,      /* gcl -pyrun -version — embedded python version */
    STAGE_PYRESOLVE,  /* gcl -pyrun -resolve <module>|<prefix> — introspection */
    STAGE_PYMOD       /* gcl -m <module> [args...] — run python module */
} Stage;

/* ── Config ─────────────────────────────────────────────── */

typedef struct {
    char  input_file[GCL_MAX_PATH];
    bool  has_input;
    char  output_path[GCL_MAX_PATH];
    bool  has_output;
    Stage stage;
    bool  debug;
    bool  has_lua;
    int   pymod_start;      /* gcl -m <module> [args...] : argv index of module args */
    bool  do_libs;          /* gcl -libs: list Library modules */
    bool  do_libcheck;      /* gcl -lib|-libcheck <module>: .gcDL exists check */
    char  libcheck_name[GCL_MAX_PATH];
    size_t include_count;
    char  include_paths[GCL_MAX_PATHS][GCL_MAX_PATH];
    size_t lib_count;
    char  lib_paths[GCL_MAX_PATHS][GCL_MAX_PATH];
    size_t extern_count;
    char  extern_paths[GCL_MAX_PATHS][GCL_MAX_PATH];
} Config;

/* ── Console init (UTF-8 + color) ───────────────────────── */

static void gcl_init_console(void) {
#ifdef _WIN32
    /* Windows console: enable UTF-8 output + ANSI color support */
    SetConsoleOutputCP(CP_UTF8);
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode) != 0)
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#else
    (void)0;
#endif
}

/* ── Diagnostics ────────────────────────────────────────── */

static void gcl_error(const char *fmt, ...) {
    va_list ap;
    fputs(GCL_COLOR_ERROR, stderr);          /* error body also in purple (no white) */
    fputs("gcl: error: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(GCL_COLOR_RESET "\n", stderr);
}

static void usage(void) {
    printf(GCL_COLOR_MAGENTA_BRIGHT "gcl %s - Gnuchan C-Like Language compiler driver\n\n" GCL_COLOR_RESET, GCL_VERSION);
    printf(GCL_COLOR_MAGENTA_BRIGHT "Usage:\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -version                          show gcl version\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl file.gcsf [-o out]                build source file\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -run file.gcsf                    run source file\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -lexer|-parser|-ast|-ir file      dump pipeline stage\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -linclude P -llib P -lextern P    extra search paths\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -libs                              list Library modules\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -lib|-libcheck <module>            .gcDL exists? (EXISTS:0 MISSING:1)\n\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -version after embed     embedded runtime version\n\n" GCL_COLOR_RESET);

    printf(GCL_COLOR_MAGENTA_BRIGHT "Compiler flags:\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -lexer <file>    dump token stream\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -parser <file>   dump parse tree\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -ast <file>      dump AST\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -ir <file>       dump IR\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -debug           debug mode\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -run <file>      run source\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -linclude <path> extra include path\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -llib <path>     extra library path\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -lextern <path>  extra extern path\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -o <output>      output path\n\n" GCL_COLOR_RESET);

    printf(GCL_COLOR_MAGENTA_BRIGHT "Library flags:\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -libs              list all Library modules\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -lib <module>      .gcDL exists? (exit 0/1)\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA_DIM "  -libcheck <module> .gcDL exists? (exit 0/1)\n\n" GCL_COLOR_RESET);

    printf(GCL_COLOR_MAGENTA_BRIGHT "Embed flags (Lua / Python):\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -luarun script.lua                run Lua embed (gcl.raylib attached)\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -pyrun  script.py                 run Python embed (gcl_raylib attached)\n" GCL_COLOR_RESET);
    printf(GCL_COLOR_MAGENTA "  gcl -m <module> [args...]              run Python module (gcl -m pip --version)\n" GCL_COLOR_RESET);
}

/* ── Safe helpers (OOM + null + bounds + overflow) ───────── */

static void *gcl_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, GCL_COLOR_ERROR "gcl: fatal: out of memory (%zu bytes)\n" GCL_COLOR_RESET, size);
        exit(1);
    }
    return ptr;
}

static int set_path(char *dst, size_t cap, const char *src) {
    size_t len;
    if (src == NULL) {
        gcl_error("empty path");
        return -1;
    }
    len = strlen(src);
    if (len == 0) {
        gcl_error("path cannot be empty");
        return -1;
    }
    if (len >= cap) {
        gcl_error("path too long (%zu >= %zu): %s", len, cap, src);
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

static int add_search_path(char paths[][GCL_MAX_PATH], size_t *count, const char *value) {
    if (*count >= GCL_MAX_PATHS) {
        gcl_error("too many search paths (max %d)", GCL_MAX_PATHS);
        return -1;
    }
    if (set_path(paths[*count], GCL_MAX_PATH, value) != 0)
        return -1;
    (*count)++;
    return 0;
}

static const char *next_value(int argc, const char **argv, int *index, const char *flag) {
    if (*index + 1 >= argc || argv[*index + 1] == NULL) {
        gcl_error("flag '%s' expects a value", flag);
        return NULL;
    }
    (*index)++;
    return argv[*index];
}

/* ── Validation ─────────────────────────────────────────── */

static int check_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (dot == NULL) {
        gcl_error("input file has no extension: '%s'", path);
        gcl_error("expected extensions: .gcsf or .gclib");
        return -1;
    }
    if (strcmp(dot, ".gcsf") == 0 || strcmp(dot, ".gclib") == 0)
        return 0;
    gcl_error("unsupported extension '%s' (expected: .gcsf or .gclib)", dot);
    return -1;
}

static int default_output(const char *input, char *out, size_t cap) {
    size_t len = strlen(input);
    size_t start = 0;
    size_t rest;
    char *dot;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '/' || input[i] == '\\')
            start = i + 1;
    }
    rest = len - start;
    if (rest == 0 || rest >= cap) {
        gcl_error("overflow while deriving output name from '%s'", input);
        return -1;
    }
    memcpy(out, input + start, rest);
    out[rest] = '\0';
    dot = strrchr(out, '.');
    if (dot != NULL)
        *dot = '\0';
    if (out[0] == '\0') {
        gcl_error("error while deriving output name from '%s'", input);
        return -1;
    }
    return 0;
}

/* ── Source loading ─────────────────────────────────────── */

typedef struct {
    char  *data;
    size_t size;
    size_t lines;
} Source;

static void source_free(Source *src) {
    if (src == NULL)
        return;
    free(src->data);
    src->data = NULL;
    src->size = 0;
    src->lines = 0;
}

static int load_source(const char *path, Source *src) {
    FILE *fp;
    long end;
    size_t size;
    size_t lines = 0;

    src->data = NULL;
    src->size = 0;
    src->lines = 0;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        gcl_error("cannot open input file: '%s'", path);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        gcl_error("cannot seek to end of file: '%s'", path);
        fclose(fp);
        return -1;
    }
    end = ftell(fp);
    if (end < 0) {
        gcl_error("cannot get file size: '%s'", path);
        fclose(fp);
        return -1;
    }
    size = (size_t)end;
    if ((long)size != end) {
        gcl_error("file too large (overflow): '%s'", path);
        fclose(fp);
        return -1;
    }
    if (size == SIZE_MAX) {
        gcl_error("file too large (overflow): '%s'", path);
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        gcl_error("cannot seek to start of file: '%s'", path);
        fclose(fp);
        return -1;
    }
    src->data = gcl_malloc(size + 1);
    if (size > 0 && fread(src->data, 1, size, fp) != size) {
        gcl_error("cannot read file: '%s'", path);
        source_free(src);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    src->data[size] = '\0';
    src->size = size;
    for (size_t i = 0; i < size; i++) {
        if (src->data[i] == '\n')
            lines++;
    }
    src->lines = lines;
    return 0;
}

/* ── Stages (modules to be wired) ───────────────────────── */

static int stage_dump(const Config *cfg, const Source *src) {
    const char *module = NULL;
    const char *label = NULL;

    switch (cfg->stage) {
        case STAGE_LEXER:  module = "Lexer";  label = "Lexer";  break;
        case STAGE_PARSER: module = "Parser"; label = "Parser"; break;
        case STAGE_AST:    module = "AST";    label = "AST";    break;
        case STAGE_IR:     module = "Ir";     label = "IR";     break;
        default:
            gcl_error("unknown stage");
            return -1;
    }

    printf(GCL_COLOR_MAGENTA_BRIGHT "-- %s --\n" GCL_COLOR_RESET, label);
    printf(GCL_COLOR_MAGENTA "  input : %s\n" GCL_COLOR_RESET, cfg->input_file);
    printf(GCL_COLOR_MAGENTA "  size  : %zu bytes / %zu lines\n" GCL_COLOR_RESET, src->size, src->lines);
    if (cfg->debug) {
        printf(GCL_COLOR_MAGENTA_DIM "  stage : src/SharedPipeline/%s\n" GCL_COLOR_RESET, module);
        printf(GCL_COLOR_MAGENTA_DIM "  paths : include=%zu lib=%zu extern=%zu\n" GCL_COLOR_RESET,
               cfg->include_count, cfg->lib_count, cfg->extern_count);
    }
    gcl_error("pipeline stage '%s' not implemented yet (src/SharedPipeline/%s)",
              label, module);
    return 1;
}

static int stage_backend(const Config *cfg, const Source *src) {
    if (cfg->debug) {
        printf(GCL_COLOR_MAGENTA_BRIGHT "-- Debug --\n" GCL_COLOR_RESET);
        printf(GCL_COLOR_MAGENTA "  mode   : %s\n" GCL_COLOR_RESET, cfg->stage == STAGE_RUN ? "run" : "build");
        printf(GCL_COLOR_MAGENTA "  input  : %s\n" GCL_COLOR_RESET, cfg->input_file);
        printf(GCL_COLOR_MAGENTA "  output : %s\n" GCL_COLOR_RESET, cfg->output_path);
        printf(GCL_COLOR_MAGENTA "  size   : %zu bytes / %zu lines\n" GCL_COLOR_RESET, src->size, src->lines);
        printf(GCL_COLOR_MAGENTA_DIM "  include: %zu paths\n" GCL_COLOR_RESET, cfg->include_count);
        for (size_t i = 0; i < cfg->include_count; i++)
            printf(GCL_COLOR_MAGENTA_DIM "    %s\n" GCL_COLOR_RESET, cfg->include_paths[i]);
        printf(GCL_COLOR_MAGENTA_DIM "  lib    : %zu paths\n" GCL_COLOR_RESET, cfg->lib_count);
        for (size_t i = 0; i < cfg->lib_count; i++)
            printf(GCL_COLOR_MAGENTA_DIM "    %s\n" GCL_COLOR_RESET, cfg->lib_paths[i]);
        printf(GCL_COLOR_MAGENTA_DIM "  extern : %zu paths\n" GCL_COLOR_RESET, cfg->extern_count);
        for (size_t i = 0; i < cfg->extern_count; i++)
            printf(GCL_COLOR_MAGENTA_DIM "    %s\n" GCL_COLOR_RESET, cfg->extern_paths[i]);
    }
    gcl_error("backend pipeline not implemented yet (SharedPipeline -> FastIR)");
    return 1;
}

/* ── Entry point ────────────────────────────────────────── */

int main(int argc, const char **argv) {
    Config cfg;
    Source src;
    int rc;

    memset(&cfg, 0, sizeof cfg);
    memset(&src, 0, sizeof src);

    gcl_init_console();

    if (argc < 2) {
        /* no args: interactive shell (GnuchanOS will have its own shell system) */
        return gcl_shell_run();
    }

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if (arg == NULL) {
            gcl_error("invalid argument list");
            return 1;
        }

        if (arg[0] == '-' && arg[1] != '\0') {
            if (strcmp(arg, "-lexer") == 0) {
                cfg.stage = STAGE_LEXER;
            } else if (strcmp(arg, "-parser") == 0) {
                cfg.stage = STAGE_PARSER;
            } else if (strcmp(arg, "-ast") == 0) {
                cfg.stage = STAGE_AST;
            } else if (strcmp(arg, "-ir") == 0) {
                cfg.stage = STAGE_IR;
            } else if (strcmp(arg, "-run") == 0) {
                cfg.stage = STAGE_RUN;
            } else if (strcmp(arg, "-luarun") == 0 || strcmp(arg, "--luarun") == 0) {
                /* Lua embed: the next argument belongs to the embed — script.lua or
                 * -version (embedded Lua version). Remaining argv belongs to embed. */
                cfg.stage = STAGE_LUA;
                cfg.has_lua = true;
                if (i + 1 >= argc) {
                    gcl_error("embed flag '%s' expects a value (script.lua or -version)", arg);
                    return 1;
                }
                i++;
                if (strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "--version") == 0 ||
                    strcmp(argv[i], "-V") == 0) {
                    cfg.stage = STAGE_LUAVER;
                } else {
                    if (set_path(cfg.input_file, sizeof cfg.input_file, argv[i]) != 0)
                        return 1;
                    cfg.has_input = true;
                }
                break;   /* remaining argv belongs to the embed — not gcl build params */
            } else if (strcmp(arg, "-pyrun") == 0 || strcmp(arg, "--pyrun") == 0) {
                /* Python embed: the next argument belongs to the embed — script.py or
                 * -version (embedded Python version). Remaining argv belongs to embed. */
                cfg.stage = STAGE_PYTHON;
                if (i + 1 >= argc) {
                    gcl_error("embed flag '%s' expects a value (script.py or -version)", arg);
                    return 1;
                }
                i++;
                if (strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "--version") == 0 ||
                    strcmp(argv[i], "-V") == 0) {
                    cfg.stage = STAGE_PYVER;
                } else if (strcmp(argv[i], "-resolve") == 0 ||
                           strcmp(argv[i], "--resolve") == 0) {
                    /* gcl -pyrun -resolve <module>|<prefix> :
                     * full-system introspection for gcl-lsp. The next argument
                     * is the spec ("os|p", "os.path|joi", ...). */
                    cfg.stage = STAGE_PYRESOLVE;
                    if (i + 1 >= argc) {
                        gcl_error("'-pyrun -resolve' expects a spec '<module>|<prefix>'");
                        return 1;
                    }
                    i++;
                    if (set_path(cfg.input_file, sizeof cfg.input_file, argv[i]) != 0)
                        return 1;
                    cfg.has_input = true;
                } else {
                    if (set_path(cfg.input_file, sizeof cfg.input_file, argv[i]) != 0)
                        return 1;
                    cfg.has_input = true;
                }
                break;   /* remaining argv belongs to the embed — not gcl build params */
            } else if (strcmp(arg, "-m") == 0) {
                /* gcl -m <module> [args...] : module name + all remaining args
                 * ('-' flags like --version included) belong to the module. */
                cfg.stage = STAGE_PYMOD;
                if (i + 1 >= argc) {
                    gcl_error("-m expects a module name (gcl -m <module>)");
                    return 1;
                }
                i++;
                if (set_path(cfg.input_file, sizeof cfg.input_file, argv[i]) != 0)
                    return 1;
                cfg.has_input = true;
                cfg.pymod_start = i + 1;   /* start of module arguments */
                break;                     /* remaining argv belongs to the module */
            } else if (strcmp(arg, "-libs") == 0) {
                cfg.do_libs = true;
            } else if (strcmp(arg, "-libcheck") == 0 || strcmp(arg, "-lib") == 0) {
                /* gcl -lib <module> / gcl -libcheck <module>:
                 * SAFE query — .gcDL exists? (EXISTS -> 0, MISSING -> 1). */
                const char *modname = next_value(argc, argv, &i, arg);
                if (modname == NULL) return 1;
                if (set_path(cfg.libcheck_name, sizeof cfg.libcheck_name, modname) != 0)
                    return 1;
                cfg.do_libcheck = true;
            } else if (strcmp(arg, "-version") == 0 || strcmp(arg, "--version") == 0 ||
                       strcmp(arg, "-V") == 0) {
                /* gcl -version : gcl version (unless after an embed flag) */
                printf("gcl %s\n", GCL_VERSION);
                return 0;
            } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "-help") == 0 ||
                       strcmp(arg, "--help") == 0) {
                usage();
                return 0;
            } else if (strcmp(arg, "-debug") == 0) {
                cfg.debug = true;
            } else if (strcmp(arg, "-linclude") == 0) {
                value = next_value(argc, argv, &i, arg);
                if (value == NULL) return 1;
                if (add_search_path(cfg.include_paths, &cfg.include_count, value) != 0) return 1;
            } else if (strcmp(arg, "-llib") == 0) {
                value = next_value(argc, argv, &i, arg);
                if (value == NULL) return 1;
                if (add_search_path(cfg.lib_paths, &cfg.lib_count, value) != 0) return 1;
            } else if (strcmp(arg, "-lextern") == 0) {
                value = next_value(argc, argv, &i, arg);
                if (value == NULL) return 1;
                if (add_search_path(cfg.extern_paths, &cfg.extern_count, value) != 0) return 1;
            } else if (strcmp(arg, "-o") == 0) {
                value = next_value(argc, argv, &i, arg);
                if (value == NULL) return 1;
                if (set_path(cfg.output_path, sizeof cfg.output_path, value) != 0) return 1;
                cfg.has_output = true;
            } else {
                gcl_error("unknown flag '%s'", arg);
                usage();
                return 1;
            }
        } else {
            if (cfg.has_input) {
                gcl_error("multiple input files: '%s' and '%s'", cfg.input_file, arg);
                return 1;
            }
            if (set_path(cfg.input_file, sizeof cfg.input_file, arg) != 0)
                return 1;
            cfg.has_input = true;
        }
    }

    /* gcl -lib <module> / gcl -libcheck <module>: SAFE .gcDL query. */
    if (cfg.do_libcheck) {
        char found[4200];
        if (gclib_find_module(cfg.libcheck_name, found, sizeof found) == 1) {
            printf("gcl: %s EXISTS (%s)\n", cfg.libcheck_name, found);
            return 0;
        }
        printf("gcl: %s MISSING\n", cfg.libcheck_name);
        return 1;
    }

    /* gcl -libs: list Library modules. */
    if (cfg.do_libs) {
        char lerr[1024] = "";
        if (gclib_list_all(lerr, sizeof lerr) != 0) {
            gcl_error("%s", lerr);
            return 1;
        }
        return 0;
    }

    if (!cfg.has_input && cfg.stage != STAGE_PYVER && cfg.stage != STAGE_LUAVER) {
        gcl_error("no input file given");
        usage();
        return 1;
    }

    /* Embedded Lua version (gcl -luarun -version): lua.gcDL gcdl_lua_version */
    if (cfg.stage == STAGE_LUAVER) {
        typedef const char *(*lua_ver_fn)(void);
        const char *mod_rel = "Library/Lua/lua.gcDL";
        char gcdl_err[1024] = "";
        GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
        if (mod == NULL) {
            gcl_error("%s", gcdl_err);
            gcl_error("Lua module missing: %s", mod_rel);
            return 1;
        }
        {
            lua_ver_fn lua_ver = (lua_ver_fn)gcdl_get_proc(mod, "gcdl_lua_version");
            if (lua_ver == NULL) {
                gcl_error("symbol gcdl_lua_version not found in: %s", mod_rel);
                gcdl_unload(mod);
                return 1;
            }
            {
                const char *ver = lua_ver();
                printf("%s\n", ver != NULL ? ver : "Lua (unknown version)");
            }
        }
        gcdl_unload(mod);
        return 0;
    }

    /* Embedded Python version (gcl -pyrun -version): python.gcDL gcdl_python_version */
    if (cfg.stage == STAGE_PYVER) {
        typedef const char *(*py_ver_fn)(void);
        const char *mod_rel = "Library/Python/python.gcDL";
        char gcdl_err[1024] = "";
        GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
        if (mod == NULL) {
            gcl_error("%s", gcdl_err);
            gcl_error("Python module missing: %s", mod_rel);
            return 1;
        }
        {
            py_ver_fn py_ver = (py_ver_fn)gcdl_get_proc(mod, "gcdl_python_version");
            if (py_ver == NULL) {
                gcl_error("symbol gcdl_python_version not found in: %s", mod_rel);
                gcdl_unload(mod);
                return 1;
            }
            {
                const char *ver = py_ver();
                printf("%s\n", ver != NULL ? ver : "Python (unknown version)");
            }
        }
        gcdl_unload(mod);
        return 0;
    }

    /* Full-system introspection (gcl -pyrun -resolve <module>|<prefix>):
     * gcl-lsp asks the real embedded Python for module attributes — this
     * covers python314.zip stdlib, Lib/site-packages and C-extension .pyd
     * modules that static scanning cannot see. The embedded Python prints
     * one NDJSON completion item per matching attribute to stdout and gcl
     * forwards it verbatim (no gcl error prefix, no trailing noise). */
    if (cfg.stage == STAGE_PYRESOLVE) {
        typedef int (*py_resolve_fn)(const char *, char *, size_t);
        const char *mod_rel = "Library/Python/python.gcDL";
        char gcdl_err[1024] = "";
        char py_err[1024] = "";
        GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
        if (mod == NULL) {
            gcl_error("%s", gcdl_err);
            gcl_error("Python module missing: %s", mod_rel);
            return 1;
        }
        {
            py_resolve_fn py_resolve = (py_resolve_fn)gcdl_get_proc(mod, "gcdl_python_resolve");
            if (py_resolve == NULL) {
                gcl_error("symbol gcdl_python_resolve not found in: %s", mod_rel);
                gcdl_unload(mod);
                return 1;
            }
            if (py_resolve(cfg.input_file, py_err, sizeof py_err) != 0) {
                /* resolve hatasi: NDJSON uyumlu tek satir hata item'i
                 * (gcl-lsp bunu dogrudan yorumlar). */
                char json_err[1200];
                snprintf(json_err, sizeof json_err,
                         "{\"label\":\"%s\",\"kind\":\"module\",\"detail\":\"%s\"}",
                         cfg.input_file, py_err);
                printf("%s\n", json_err);
                gcdl_unload(mod);
                return 1;
            }
        }
        /* stdout'taki NDJSON item'lari zaten cikti: hicbir ek prefix yok. */
        gcdl_unload(mod);
        return 0;
    }

    /* Lua embed (gcl -luarun script.lua): gcl.exe does NOT embed it.
     * Lua runtime lives in the Library/Lua/lua.gcDL module;
     * it is loaded at run time via gcdl_loader and gcdl_lua_run is called. */
    if (cfg.stage == STAGE_LUA) {
        typedef int (*lua_run_fn)(const char *, int, char *, size_t);
        const char *mod_rel = "Library/Lua/lua.gcDL";
        char gcdl_err[1024] = "";
        char lua_err[1024] = "";
        /* Load from the Library/ folder NEXT TO gcl.exe:
         * works from any working directory. */
        GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
        if (mod == NULL) {
            gcl_error("%s", gcdl_err);
            gcl_error("Lua module missing: %s", mod_rel);
            gcl_error("Suggestions: run 'python makefile.py' (build) or check Library/Lua/");
            return 1;
        }
        {
            lua_run_fn lua_run = (lua_run_fn)gcdl_get_proc(mod, "gcdl_lua_run");
            if (lua_run == NULL) {
                gcl_error("symbol gcdl_lua_run not found in: %s", mod_rel);
                gcdl_unload(mod);
                return 1;
            }
            if (lua_run(cfg.input_file, cfg.debug, lua_err, sizeof lua_err) != 0) {
                gcl_error("%s", lua_err);
                gcdl_unload(mod);
                return 1;
            }
        }
        gcdl_unload(mod);
        return 0;
    }

    /* Python embed (gcl -pyrun script.py): same pattern as Lua.
     * python.gcDL lives in Library/Python/; the embedded runtime
     * (python314.dll + stdlib .pyd/.zip) lives in Library/Python/pyLibrary/. */
    if (cfg.stage == STAGE_PYTHON) {
        typedef int (*py_run_fn)(const char *, char *, size_t);
        const char *mod_rel = "Library/Python/python.gcDL";
        char gcdl_err[1024] = "";
        char py_err[1024] = "";
        GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
        if (mod == NULL) {
            gcl_error("%s", gcdl_err);
            gcl_error("Python module missing: %s", mod_rel);
            gcl_error("Suggestions: run 'python makefile.py python' (build) or check Library/Python/");
            return 1;
        }
        {
            py_run_fn py_run = (py_run_fn)gcdl_get_proc(mod, "gcdl_python_run");
            if (py_run == NULL) {
                gcl_error("symbol gcdl_python_run not found in: %s", mod_rel);
                gcdl_unload(mod);
                return 1;
            }
            if (py_run(cfg.input_file, py_err, sizeof py_err) != 0) {
                gcl_error("%s", py_err);
                gcdl_unload(mod);
                return 1;
            }
        }
        gcdl_unload(mod);
        return 0;
    }

    /* Python module (gcl -m <module> [args...]): module name in input_file. */
    if (cfg.stage == STAGE_PYMOD) {
#ifdef _WIN32
        /* Windows: we use the embedded python.exe as a child process.
         * The PyConfig isolated mode in gcl_python_embed.c cannot resolve
         * .pyd C extensions (e.g. _ctypes.pyd -> libffi-8.dll) through the
         * DLL search path; but python.exe loads next to its own directory and
         * all DLLs are in the same folder, so C extensions always resolve
         * (proof: python.exe -c "import socket" OK). Thus gcl -m pip really works. */
        {
            char exe_path[4096];
            char py_dir[4200];
            char py_exe[4600];
            char cmdline[16384];
            char site_pkgs[4700];
            const char *sep = NULL;
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            DWORD code = 1;
            size_t used;
            static const char pyrel[] = "Library\\Python\\pyLibrary";

            {
                DWORD n = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof exe_path);
                if (n == 0 || n >= (DWORD)sizeof exe_path) {
                    gcl_error("cannot find exe path");
                    return 1;
                }
            }
            for (char *p = exe_path; *p; p++) {
                if (*p == '\\' || *p == '/')
                    sep = p;
            }
            if (sep == NULL) {
                gcl_error("cannot derive pyLibrary path");
                return 1;
            }
            {
                size_t dlen = (size_t)(sep - exe_path) + 1;
                if (dlen + sizeof pyrel >= sizeof py_dir) {
                    gcl_error("pyLibrary path too long");
                    return 1;
                }
                memcpy(py_dir, exe_path, dlen);
                memcpy(py_dir + dlen, pyrel, sizeof pyrel);
            }
            snprintf(py_exe, sizeof py_exe, "%s\\python.exe", py_dir);
            snprintf(site_pkgs, sizeof site_pkgs, "%s\\Lib\\site-packages", py_dir);

            /* python.exe -c SCRIPT <module> [args...]
             * The first argument after -c becomes sys.argv[1]: we also append
             * the module name to the loop (module name is NOT embedded in script). */
            used = (size_t)snprintf(
                cmdline, sizeof cmdline,
                "\"%s\" -c \"import sys,runpy;"
                "sys.path.insert(0,r'%s');"
                "sys.argv=[sys.argv[1]]+sys.argv[2:];"
                "sys.exit(runpy.run_module(sys.argv[0],run_name='__main__',alter_sys=True))\"",
                py_exe, site_pkgs);
            if (used >= sizeof cmdline) {
                gcl_error("pip command too long");
                return 1;
            }
            {
                const char *a = cfg.input_file;
                int quoted = (a[0] == '\0' || strchr(a, ' ') != NULL ||
                              strchr(a, '\t') != NULL) ? 1 : 0;
                int n = snprintf(cmdline + used, sizeof cmdline - used,
                                 " %c%s%c", quoted ? '"' : ' ', a, quoted ? '"' : ' ');
                if (n < 0 || (size_t)n >= sizeof cmdline - used) {
                    gcl_error("arguments too long");
                    return 1;
                }
                used += (size_t)n;
            }
            for (int i = cfg.pymod_start; i < argc; i++) {
                const char *a = argv[i] != NULL ? argv[i] : "";
                int quoted = (a[0] == '\0' || strchr(a, ' ') != NULL ||
                              strchr(a, '\t') != NULL) ? 1 : 0;
                int n = snprintf(cmdline + used, sizeof cmdline - used,
                                 " %c%s%c", quoted ? '"' : ' ', a, quoted ? '"' : ' ');
                if (n < 0 || (size_t)n >= sizeof cmdline - used) {
                    gcl_error("arguments too long");
                    return 1;
                }
                used += (size_t)n;
            }

            ZeroMemory(&si, sizeof si);
            si.cb = sizeof si;
            ZeroMemory(&pi, sizeof pi);
            if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL,
                                py_dir, &si, &pi)) {
                gcl_error("cannot run python.exe: %s", py_exe);
                return 1;
            }
            CloseHandle(pi.hThread);
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            return (int)code;
        }
#else
        /* Linux: the embedded libpython C embed works (pip 26.2.1 verified)
         * — uses the gcdl_python_module export of python.gcDL. */
        {
            typedef int (*py_mod_fn)(const char *, int, const char **, char *, size_t);
            const char *mod_rel = "Library/Python/python.gcDL";
            char gcdl_err[1024] = "";
            char py_err[1024] = "";
            int mod_argc;
            const char **mod_argv;
            GclGcDl *mod = gcdl_load_adjacent(mod_rel, gcdl_err, sizeof gcdl_err);
            if (mod == NULL) {
                gcl_error("%s", gcdl_err);
                gcl_error("Python module missing: %s", mod_rel);
                return 1;
            }
            {
                py_mod_fn py_mod = (py_mod_fn)gcdl_get_proc(mod, "gcdl_python_module");
                if (py_mod == NULL) {
                    gcl_error("symbol gcdl_python_module not found in: %s", mod_rel);
                    gcdl_unload(mod);
                    return 1;
                }
                if (cfg.pymod_start < argc)
                    mod_argc = argc - cfg.pymod_start;
                else
                    mod_argc = 0;
                mod_argv = (cfg.pymod_start < argc) ? argv + cfg.pymod_start : NULL;
                if (py_mod(cfg.input_file, mod_argc, mod_argv, py_err, sizeof py_err) != 0) {
                    gcl_error("%s", py_err);
                    gcdl_unload(mod);
                    return 1;
                }
            }
            gcdl_unload(mod);
            return 0;
        }
#endif
    }

    if (check_extension(cfg.input_file) != 0)
        return 1;
    if (cfg.stage == STAGE_FULL && !cfg.has_output) {
        if (default_output(cfg.input_file, cfg.output_path, sizeof cfg.output_path) != 0)
            return 1;
        cfg.has_output = true;
    }
    if (load_source(cfg.input_file, &src) != 0)
        return 1;

    if (cfg.stage == STAGE_RUN || cfg.stage == STAGE_FULL)
        rc = stage_backend(&cfg, &src);
    else
        rc = stage_dump(&cfg, &src);

    source_free(&src);
    return rc;
}
