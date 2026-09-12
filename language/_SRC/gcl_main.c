/*
 * gcl_main.c — GCL CLI main entry point.
 *
 * Usage (simple_doc.md):
 *   gcl -ide [file]                 start the IDE
 *   gcl -debug -run file.gcsf       run a GCL script
 *   gcl -luarun file.lua            run a Lua script
 *   gcl -pyrun file.py              run a Python script
 *   gcl -build project.gcdata -o d  build a project (exe + gcBundle)
 *   gcl -new path [--lua|--python]  create a project skeleton
 */

#define _GNU_SOURCE   /* for the realpath prototype (POSIX.1-2008; required under -std=c99) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <process.h>
#include <direct.h>
#include <windows.h>
#define POPEN  _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <dirent.h>
#define POPEN  popen
#define PCLOSE pclose
#endif

/* global argv so the IDE can locate its own exe */
int g_gcl_argc = 0;
char **g_gcl_argv = NULL;
/* defined in gcl_runner.c — enabled by the -debug flag */
extern int gcl_debug;

/* GCL SimpleRunner — lexer→parser→runner */
#include "gcl_simple_runner.h"

/* Embedded runtimes (-luarun / -pyrun CLI modes) — now loaded DYNAMICALLY from Embed.dll */
#include "gcl_shared_state.h"

/* IDE — loaded as a separate DLL (Programs/ide.dll|.so). This keeps gcl.exe small. */
#ifdef _WIN32
typedef int (*GclIdeRunFn)(const char *path);
#else
typedef int (*GclIdeRunFn)(const char *path);
#endif

/* Directory containing the running gcl.exe.
   Windows: GetModuleFileNameA → always a FULL path (even if argv[0] is relative).
   Linux: if argv[0] is not an absolute path, resolve it with realpath relative
   to the working directory. The old code never trimmed exe_dir when argv[0] was
   "./gcl.exe" or "gcl.exe", producing WRONG paths like "gcl.exe\Programs\ide.dll". */
static void get_exe_dir(char *out, size_t outsz) {
    out[0] = '\0';
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)outsz);
    if (n > 0) {
        char *es = strrchr(out, '\\');
        if (!es) es = strrchr(out, '/');
        if (es) *es = '\0';
    }
#else
    if (g_gcl_argc > 0 && g_gcl_argv && g_gcl_argv[0] && g_gcl_argv[0][0]) {
        /* Always produce an absolute path: resolve with realpath.
           Otherwise, from a relative path like "./language/build/gnuLinux/gcl"
           "exe_dir" stays relative and LD_LIBRARY_PATH becomes relative — dlopen
           cannot find libpython3.14.so.1.0 ("unknown module 'Embed'"). */
        char abs_path[4096];
        if (realpath(g_gcl_argv[0], abs_path) != NULL) {
            snprintf(out, outsz, "%s", abs_path);
        } else {
            snprintf(out, outsz, "%s", g_gcl_argv[0]);
        }
        char *es = strrchr(out, '/');
        if (es) {
            *es = '\0';
        }
    }
#endif
    if (!out[0]) snprintf(out, outsz, ".");
}

static int run_ide(const char *path) {
    char exe_dir[4096] = { 0 };
    get_exe_dir(exe_dir, sizeof(exe_dir));

    /* Expose the gcl.exe path via the environment so the IDE can invoke it
       (gcl -run/-new/-build). ide.dll can no longer access g_gcl_argv; it locates
       it through this environment variable. */
    if (g_gcl_argc > 0 && g_gcl_argv && g_gcl_argv[0] && g_gcl_argv[0][0]) {
#ifdef _WIN32
        _putenv_s("GCL_EXE_PATH", g_gcl_argv[0]);
#else
        setenv("GCL_EXE_PATH", g_gcl_argv[0], 1);
#endif
    }

#ifdef _WIN32
    char ide_path[4096];
    snprintf(ide_path, sizeof(ide_path), "%s\\Programs\\ide.dll", exe_dir);
    HMODULE h = LoadLibraryA(ide_path);
    if (!h) {
        /* Fallback: try via PATH */
        h = LoadLibraryA("ide.dll");
        if (!h) {
            fprintf(stderr, "Error: could not load IDE library '%s'\n", ide_path);
            fprintf(stderr, "Programs/ide.dll is missing from this build. The build did not\n");
            fprintf(stderr, "include IDE sources (src/IDE/*.c). Rebuild with those sources present.\n");
            return 1;
        }
    }
    GclIdeRunFn ide_run = (GclIdeRunFn)(void *)GetProcAddress(h, "gcl_ide_run");
    if (!ide_run) {
        fprintf(stderr, "Error: gcl_ide_run not found in ide.dll\n");
        FreeLibrary(h);
        return 1;
    }
    int rc = ide_run(path);
    FreeLibrary(h);
    return rc;
#else
    char ide_path[4096];
    snprintf(ide_path, sizeof(ide_path), "%s/Programs/ide.so", exe_dir);
    void *h = dlopen(ide_path, RTLD_NOW);
    if (!h) {
        fprintf(stderr, "Error: could not load IDE library '%s': %s\n", ide_path, dlerror());
        fprintf(stderr, "Programs/ide.so is missing from this build. The build did not\n");
        fprintf(stderr, "include IDE sources (src/IDE/*.c). Rebuild with those sources present.\n");
        return 1;
    }
    GclIdeRunFn ide_run = (GclIdeRunFn)dlsym(h, "gcl_ide_run");
    if (!ide_run) {
        fprintf(stderr, "Error: gcl_ide_run not found in ide.so: %s\n", dlerror());
        dlclose(h);
        return 1;
    }
    int rc = ide_run(path);
    dlclose(h);
    return rc;
#endif
}

/* gcBundle build */
#ifndef GCL_SKIP_BUNDLE
int gcb_build_project(const char *project_dir, const char *project_name,
                      const char *out_dir);
int gcb_build_project_runtime(const char *project_dir, const char *runtime_dir,
                              const char *project_name, const char *out_dir);
#endif

/* Platform helpers */
const char *gcl_os_name(void);
int gcl_ensure_dir(const char *dir);

/* ---------- Read file ---------- */
static int path_is_dir(const char *p) {
#ifdef _WIN32
    struct _stat st;
    return (_stat(p, &st) == 0 && (st.st_mode & _S_IFDIR));
#else
    struct stat st;
    return (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* ---------- Run an external interpreter (feed via stdin, no extract) ---------- */
static int run_interpreter(const char *path, const char *interp) {
    char *src = read_file(path);
    if (!src) { fprintf(stderr, "Error: cannot read '%s'\n", path); return 1; }
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s -", interp);
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "%s -", interp);
    FILE *p = POPEN(cmd, "w");
    if (!p) { free(src); return 1; }
    fwrite(src, 1, strlen(src), p);
    int rc = PCLOSE(p);
    free(src);
    return rc;
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) { free(src); return 1; }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execlp(interp, interp, "-", (char *)NULL);
        _exit(127);
    }
    close(pipefd[0]);
    size_t len = strlen(src);
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(pipefd[1], src + written, len - written);
        if (n <= 0) break;
        written += (size_t)n;
    }
    close(pipefd[1]);
    free(src);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
#endif
}

/* ---------- Run a GCL script: the real lexer→parser→runner pipeline ---------- */
static int run_gcl(const char *path, int script_argc, char **script_argv) {
    char *src = read_file(path);
    if (!src) { fprintf(stderr, "Error: cannot read '%s'\n", path); return 1; }
    /* base_dir = the directory containing the file (for include resolution).
       Always produce an ABSOLUTE path: when a relative path or just a file name
       is given, resolve it against the working directory. Embed.Run subprocesses
       look for the script via GCL_PROJECT_DIR; if the path stays relative, wrong
       paths like "D:/scripts/main.lua" are produced. */
    char base_dir[4096];
    {
        char dir_tmp[4096];
        snprintf(dir_tmp, sizeof(dir_tmp), "%s", path);
        char *slash = strrchr(dir_tmp, '\\');
        if (!slash) slash = strrchr(dir_tmp, '/');
        if (slash) *slash = '\0';
        else {
            /* file name only → working directory */
            snprintf(dir_tmp, sizeof(dir_tmp), ".");
        }
#ifdef _WIN32
        _fullpath(base_dir, dir_tmp, sizeof(base_dir));
#else
        if (realpath(dir_tmp, base_dir) == NULL)
            snprintf(base_dir, sizeof(base_dir), "%s", dir_tmp);
#endif
    }
    /* Pass the project root and the shared store path to Embed.Run subprocesses
       (gcl -luarun/-pyrun) via environment variables.
       Project Run (IDE) already sets GCL_PROJECT_DIR to the project root; in
       standalone -run, base_dir is used when this env is not set. */
    char shared_file[4096];
#ifdef _WIN32
    const char *pre_proj = getenv("GCL_PROJECT_DIR");
    if (!pre_proj || !pre_proj[0])
        _putenv_s("GCL_PROJECT_DIR", base_dir);
    snprintf(shared_file, sizeof(shared_file), "%s\\gcl_shared.state", base_dir);
    _putenv_s("GCL_SHARED_FILE", shared_file);
#else
    const char *pre_proj2 = getenv("GCL_PROJECT_DIR");
    if (!pre_proj2 || !pre_proj2[0])
        setenv("GCL_PROJECT_DIR", base_dir, 1);
    snprintf(shared_file, sizeof(shared_file), "%s/gcl_shared.state", base_dir);
    setenv("GCL_SHARED_FILE", shared_file, 1);
#endif
    int rc = gcl_simple_run_source(src, strlen(src), base_dir, script_argc, script_argv);
    gcl_shared_cleanup();
    free(src);
    return rc == 0 ? 0 : 1;
}
/* ---------- Run a program built from a gcBundle ---------- */
#ifndef GCL_SKIP_BUNDLE
#include "gcbundle.h"

static int find_bundle_in_dir(const char *dir, char *out, size_t outsz) {
#ifdef _WIN32
    char pattern[8192];
    snprintf(pattern, sizeof(pattern), "%s\\*.gcBundle", dir);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    snprintf(out, outsz, "%s\\%s", dir, ffd.cFileName);
    FindClose(h);
    return 1;
#else
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        const char *ext = strrchr(ent->d_name, '.');
        if (ext && strcmp(ext, ".gcBundle") == 0) {
            snprintf(out, outsz, "%s/%s", dir, ent->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
#endif
}

static int run_bundle_program(const char *bundle_path) {
    GcbBundle *b = gcb_open(bundle_path);
    if (!b) {
        fprintf(stderr, "Error: cannot open bundle '%s': %s\n", bundle_path, gcb_last_error());
        return 1;
    }
    /* Godot PCK logic: the exe extracts the bundle next to itself.
       Library/Embed.dll, scripts/, assets/ are written to disk — so that
       #native <Embed> + Embed.Run (Lua/Python) windows can open.
       Programs/ is NOT in the bundle anyway (only scripts + Library are
       embedded at build time). */
    char base_dir[4096];
    snprintf(base_dir, sizeof(base_dir), "%s", bundle_path);
    char *slash = strrchr(base_dir, '\\');
    if (!slash) slash = strrchr(base_dir, '/');
    if (slash) *slash = '\0';
    if (gcb_extract_all(b, base_dir) != 0) {
        fprintf(stderr, "Error: could not extract bundle to '%s'\n", base_dir);
        gcb_close(b);
        return 1;
    }
    uint32_t sz = 0;
    const void *bundle_src = gcb_read_path(b, "main.gcsf", &sz);
    if (!bundle_src) {
        fprintf(stderr, "Error: main.gcsf not found in bundle '%s'\n", bundle_path);
        gcb_close(b);
        return 1;
    }
    /* CRITICAL: the bundle blob is NOT null-terminated. The lexer/parser reads
       up to a null terminator; without one, random data from the blob is read
       and an "unexpected character" error occurs. Copy the source into a
       null-terminated buffer. */
    char *src = (char *)malloc((size_t)sz + 1);
    if (!src) { gcb_close(b); return 1; }
    memcpy(src, bundle_src, sz);
    src[sz] = '\0';
#ifdef _WIN32
    _putenv_s("GCL_PROJECT_DIR", base_dir);
    {
        char shared_file[4096];
        snprintf(shared_file, sizeof(shared_file), "%s\\gcl_shared.state", base_dir);
        _putenv_s("GCL_SHARED_FILE", shared_file);
    }
#else
    setenv("GCL_PROJECT_DIR", base_dir, 1);
    {
        char shared_file[4096];
        snprintf(shared_file, sizeof(shared_file), "%s/gcl_shared.state", base_dir);
        setenv("GCL_SHARED_FILE", shared_file, 1);
    }
#endif
    int rc = gcl_simple_run_source(src, strlen(src), base_dir, 0, NULL);
    free(src);
    gcl_shared_cleanup();
    gcb_close(b);
    return rc == 0 ? 0 : 1;
}
#endif /* GCL_SKIP_BUNDLE */

/* ---------- Dynamic loading of Embed.dll (-luarun / -pyrun) ---------- */
/* Embed.dll now contains the Lua/Python runtime plus the LuaRaylib/LuaRaygui
   bindings. gcl.exe is only the GCL language; -luarun/-pyrun are loaded from
   Embed.dll. */

typedef int (*GclLuaRunFileFn)(const char *path);
typedef int (*GclPyRunFileFn)(const char *path);

static int embed_library_path(char *out, size_t outsz, const char *name) {
    char exe_dir[4096] = { 0 };
    get_exe_dir(exe_dir, sizeof(exe_dir));
#ifdef _WIN32
    snprintf(out, outsz, "%s\\Library\\%s", exe_dir, name);
#else
    snprintf(out, outsz, "%s/Library/%s", exe_dir, name);
#endif
    return 0;
}

/* ---------- Run a Lua script (-luarun) — from Embed.dll ---------- */
static int run_lua(const char *path) {
#ifdef _WIN32
    char lib[4096];
    embed_library_path(lib, sizeof(lib), "Embed.dll");
    HMODULE h = LoadLibraryA(lib);
    if (!h) h = LoadLibraryA("Embed.dll");
    if (!h) {
        fprintf(stderr, "Error: could not load Embed.dll for -luarun\n");
        return 1;
    }
    GclLuaRunFileFn fn = (GclLuaRunFileFn)(void *)GetProcAddress(h, "gcl_lua_run_file");
    if (!fn) {
        fprintf(stderr, "Error: gcl_lua_run_file not found in Embed.dll\n");
        FreeLibrary(h);
        return 1;
    }
    int rc = fn(path);
    FreeLibrary(h);
    return rc;
#else
    char lib_so[4096];
    embed_library_path(lib_so, sizeof(lib_so), "Embed.so");
    void *h = dlopen(lib_so, RTLD_NOW);
    if (!h) h = dlopen("Embed.so", RTLD_NOW);
    if (!h) {
        fprintf(stderr, "Error: could not load Embed.so for -luarun: %s\n", dlerror());
        return 1;
    }
    GclLuaRunFileFn fn = (GclLuaRunFileFn)dlsym(h, "gcl_lua_run_file");
    if (!fn) {
        fprintf(stderr, "Error: gcl_lua_run_file not found in Embed.so: %s\n", dlerror());
        dlclose(h);
        return 1;
    }
    int rc = fn(path);
    dlclose(h);
    return rc;
#endif
}

/* ---------- Run a Python script (-pyrun) — from Embed.dll ---------- */
static int run_python(const char *path) {
#ifdef _WIN32
    char lib[4096];
    embed_library_path(lib, sizeof(lib), "Embed.dll");
    HMODULE h = LoadLibraryA(lib);
    if (!h) h = LoadLibraryA("Embed.dll");
    if (!h) {
        fprintf(stderr, "Error: could not load Embed.dll for -pyrun\n");
        return 1;
    }
    GclPyRunFileFn fn = (GclPyRunFileFn)(void *)GetProcAddress(h, "gcl_py_run_file");
    if (!fn) {
        fprintf(stderr, "Error: gcl_py_run_file not found in Embed.dll\n");
        FreeLibrary(h);
        return 1;
    }
    int rc = fn(path);
    FreeLibrary(h);
    return rc;
#else
    char lib_so[4096];
    embed_library_path(lib_so, sizeof(lib_so), "Embed.so");
    void *h = dlopen(lib_so, RTLD_NOW);
    if (!h) h = dlopen("Embed.so", RTLD_NOW);
    if (!h) {
        fprintf(stderr, "Error: could not load Embed.so for -pyrun: %s\n", dlerror());
        return 1;
    }
    GclPyRunFileFn fn = (GclPyRunFileFn)dlsym(h, "gcl_py_run_file");
    if (!fn) {
        fprintf(stderr, "Error: gcl_py_run_file not found in Embed.so: %s\n", dlerror());
        dlclose(h);
        return 1;
    }
    int rc = fn(path);
    dlclose(h);
    return rc;
#endif
}

/* ---------- Create a project skeleton (-new) ---------- */
/* simple_doc.md: gcl -new project_name
   Project_name/
       assets/         --> also used by raylib (gcl, lua, python must find it automatically)
       external/       --> looks for .so, .dll and similar here (#external <file.dll>)
       include/        --> .gcsf
       out/            --> project_name, project_name.gcBundle
       scripts/        --> lua, python files
       main.gcsf
       project.gcdata  --> JSON (schema below)
*/
static int new_project(const char *path, int with_lua, int with_luaraylib, int with_python, int with_pyraylib) {
    if (!path || !path[0]) { fprintf(stderr, "Error: project path required\n"); return 1; }
    gcl_ensure_dir(path);
    const char *dirs[] = { "scripts", "assets", "include", "external", "out", NULL };
    for (int i = 0; dirs[i]; i++) {
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s/%s", path, dirs[i]);
        gcl_ensure_dir(buf);
    }
    const char *proj_name = strrchr(path, '\\') ? strrchr(path, '\\') + 1 : (strrchr(path, '/') ? strrchr(path, '/') + 1 : "project");

    /* main.gcsf — simple_doc.md: #native <Stdio> + Stdio.printf
       If Lua/Python is selected, Embed.Run calls; if LuaRaylib/PyRaylib is
       selected, a Raylib window example; if none is selected, only a print
       example. */
    char main_path[4096];
    snprintf(main_path, sizeof(main_path), "%s/main.gcsf", path);
    FILE *f = fopen(main_path, "wb");
    if (f) {
        fputs("#native <Stdio>\n", f);
        if (with_lua || with_python) {
            fputs("#native <Embed>\n", f);
        }
        if (with_luaraylib || with_pyraylib) {
            fputs("#native <Raylib>\n", f);
        }
        fputs("\nint main() {\n", f);
        fputs("    Stdio.printf(\"Hello, GCL!\\n\");\n", f);
        /* Embed windows open FIRST — the Lua/Python raylib windows do not
           wait for the main window */
        if (with_lua) fputs("    Embed.Run(type=\"lua\");\n", f);
        if (with_python) fputs("    Embed.Run(type=\"python\");\n", f);
        /* GCL raylib window: stays open until the user closes it */
        if (with_luaraylib || with_pyraylib) {
            fputs("    Raylib.InitWindow(800, 600, \"GCL Project\");\n", f);
            fputs("    Raylib.SetTargetFPS(60);\n", f);
            fputs("    while (!Raylib.WindowShouldClose()) {\n", f);
            fputs("        Raylib.BeginDrawing();\n", f);
            fputs("        Raylib.ClearBackground(Raylib.BLACK);\n", f);
            fputs("        Raylib.DrawText(\"GCL Project\", 20, 20, 20, Raylib.WHITE);\n", f);
            fputs("        Raylib.EndDrawing();\n", f);
            fputs("    }\n", f);
            fputs("    Raylib.CloseWindow();\n", f);
        }
        fputs("    return 0;\n", f);
        fputs("}\n", f);
        fclose(f);
    }
    /* project.gcdata — JSON (simple_doc.md schema) */
    char gcdata[4096];
    snprintf(gcdata, sizeof(gcdata), "%s/project.gcdata", path);
    f = fopen(gcdata, "wb");
    if (f) {
        fprintf(f, "{\n");
        fprintf(f, "  \"project_name\": \"%s\",\n", proj_name);
        fprintf(f, "  \"developer_name\": \"developer\",\n");
        fprintf(f, "  \"version\": 0.100,\n");
        fprintf(f, "  \"open_lua\": %s,\n", with_lua ? "true" : "false");
        fprintf(f, "  \"open_lua_raylib\": %s,\n", with_luaraylib ? "true" : "false");
        fprintf(f, "  \"open_python\": %s,\n", with_python ? "true" : "false");
        fprintf(f, "  \"open_python_raylib\": %s,\n", with_pyraylib ? "true" : "false");
        fprintf(f, "  \"single_bundle\": false,\n");
        fprintf(f, "  \"lua_main_file\": \"main.lua\",\n");
        fprintf(f, "  \"python_main_file\": \"main.py\",\n");
        fprintf(f, "  \"gcl_include_path\": \"include\",\n");
        fprintf(f, "  \"gcl_lib_path\": \"lib\",\n");
        fprintf(f, "  \"gcl_external_path\": \"external\",\n");
        fprintf(f, "  \"python_import_path\": \"scripts\",\n");
        fprintf(f, "  \"lua_import_path\": \"scripts\",\n");
        fprintf(f, "  \"raylib_asset_directory_path\": \"assets\"\n");
        fprintf(f, "}\n");
        fclose(f);
    }
    if (with_lua) {
        char lp[4096];
        snprintf(lp, sizeof(lp), "%s/scripts/main.lua", path);
        f = fopen(lp, "wb");
        if (f) {
            if (with_luaraylib) {
                /* Lua 2D example — a real Camera2D table object (same as the IDE) */
                const char *tpl =
                    "-- GCL Lua 2D example\n"
                    "gcl.init()\n"
                    "\n"
                    "raylib.InitWindow(800, 600, \"GCL Lua 2D Project\")\n"
                    "raylib.SetTargetFPS(60)\n"
                    "\n"
                    "local camera = raylib.Camera2D()\n"
                    "camera.offset = { x = 400.0, y = 300.0 }\n"
                    "camera.target = { x = 0.0, y = 0.0 }\n"
                    "camera.rotation = 0.0\n"
                    "camera.zoom = 1.0\n"
                    "\n"
                    "while not raylib.WindowShouldClose() do\n"
                    "    raylib.BeginDrawing()\n"
                    "    raylib.ClearBackground(raylib.BLACK)\n"
                    "    raylib.BeginMode2D(camera)\n"
                    "    raylib.DrawCircle(400, 300, 50, raylib.RED)\n"
                    "    raylib.DrawText(\"GCL Lua 2D Project\", 20, 20, 20, raylib.WHITE)\n"
                    "    raylib.EndMode2D()\n"
                    "    raylib.EndDrawing()\n"
                    "end\n"
                    "\n"
                    "raylib.CloseWindow()\n";
                fputs(tpl, f);
            } else {
                const char *tpl =
                    "-- GCL Lua example (print)\n"
                    "print(\"Hello from Lua embedded in GCL!\")\n";
                fputs(tpl, f);
            }
            fclose(f);
        }
    }
    if (with_python) {
        char pp[4096];
        snprintf(pp, sizeof(pp), "%s/scripts/main.py", path);
        f = fopen(pp, "wb");
        if (f) {
            if (with_pyraylib) {
                /* Python 2D example — a real Camera2D dict object (same as the IDE) */
                const char *tpl =
                    "# GCL Python 2D example\n"
                    "import raylib\n"
                    "import gcl\n"
                    "\n"
                    "gcl.init()\n"
                    "\n"
                    "raylib.InitWindow(800, 600, \"GCL Python 2D Project\")\n"
                    "raylib.SetTargetFPS(60)\n"
                    "\n"
                    "camera = raylib.Camera2D()\n"
                    "camera[\"offset\"] = { \"x\": 400.0, \"y\": 300.0 }\n"
                    "camera[\"target\"] = { \"x\": 0.0, \"y\": 0.0 }\n"
                    "camera[\"rotation\"] = 0.0\n"
                    "camera[\"zoom\"] = 1.0\n"
                    "\n"
                    "while not raylib.WindowShouldClose():\n"
                    "    raylib.BeginDrawing()\n"
                    "    raylib.ClearBackground(raylib.BLACK)\n"
                    "    raylib.BeginMode2D(camera)\n"
                    "    raylib.DrawCircle(400, 300, 50, raylib.RED)\n"
                    "    raylib.DrawText(\"GCL Python 2D Project\", 20, 20, 20, raylib.WHITE)\n"
                    "    raylib.EndMode2D()\n"
                    "    raylib.EndDrawing()\n"
                    "\n"
                    "raylib.CloseWindow()\n";
                fputs(tpl, f);
            } else {
                const char *tpl =
                    "# GCL Python example (print)\n"
                    "print(\"Hello from Python embedded in GCL!\")\n";
                fputs(tpl, f);
            }
            fclose(f);
        }
    }
    printf("Project created: %s\n", path);
    return 0;
}
/* ---------- Build (-build) ---------- */
#ifndef GCL_SKIP_BUNDLE
static int build_project(const char *gcdata, const char *out_dir_arg) {
    if (!gcdata || !out_dir_arg) { fprintf(stderr, "Error: build requires project.gcdata and -o dir\n"); return 1; }
    /* Copy the const out_dir parameter into a local mutable buffer — trimming happens here */
    char out_dir[4096];
    snprintf(out_dir, sizeof(out_dir), "%s", out_dir_arg);
    /* Read the project name from project.gcdata (JSON: "project_name": "X") */
    char *data = read_file(gcdata);
    char name[256] = "project";
    if (data) {
        char *p = strstr(data, "project_name");
        if (p) {
            p = strchr(p, ':');
            if (p) p = strchr(p, '"');
            if (p) {
                p++;
                char *e = strchr(p, '"');
                if (e) {
                    size_t n = (size_t)(e - p);
                    if (n >= sizeof(name)) n = sizeof(name) - 1;
                    memcpy(name, p, n);
                    name[n] = '\0';
                }
            }
        }
        free(data);
    }
    /* The project name must be ONLY a file name — trim leading / \ path separators.
       Otherwise a subpath is created inside out_dir (e.g. "test_project//_31_final.exe"). */
    {
        char *nm = name;
        while (*nm == '/' || *nm == '\\' || *nm == '.') nm++;
        if (!*nm) nm = "project";
        memmove(name, nm, strlen(nm) + 1);
        /* also clean invalid characters: space and : ? * " < > | */
        for (char *q = name; *q; q++) {
            if (*q == ' ' || *q == ':' || *q == '?' || *q == '*' || *q == '"' || *q == '<' || *q == '>' || *q == '|')
                *q = '_';
        }
    }
    /* trim trailing / from out_dir */
    {
        size_t odl = strlen(out_dir);
        while (odl > 1 && (out_dir[odl - 1] == '/' || out_dir[odl - 1] == '\\')) { out_dir[odl - 1] = '\0'; odl--; }
    }
    /* project root = the parent of gcdata */
    char project_dir[4096];
    snprintf(project_dir, sizeof(project_dir), "%s", gcdata);
    char *slash = strrchr(project_dir, '\\');
    if (!slash) slash = strrchr(project_dir, '/');
    if (slash) *slash = '\0';

    /* Create the output directory (otherwise the gcBundle cannot be written) */
    gcl_ensure_dir(out_dir);

    /* Runtime directory = build/<os>/ where the running gcl.exe lives (under Library/).
       Doc: ".gcBundle — all runtime and dll or so in this place". */
    char runtime_dir[4096] = "";
    get_exe_dir(runtime_dir, sizeof(runtime_dir));

    /* Produce the gcBundle (project + runtime/Library) */
    printf("[Build] 1/5 Packaging project + runtime...\n");
    fflush(stdout);
    if (gcb_build_project_runtime(project_dir, runtime_dir, name, out_dir) != 0) {
        fprintf(stderr, "Error: could not build project\n");
        return 1;
    }
    printf("[Build] 2/5 Copying the executable...\n");
    fflush(stdout);

    /* Doc: Project_name.exe — copy the currently running gcl exe into out_dir */
    {
        const char *src_exe = (g_gcl_argc > 0 && g_gcl_argv && g_gcl_argv[0]) ? g_gcl_argv[0] : NULL;
        if (src_exe && src_exe[0]) {
#ifdef _WIN32
            char dest[4096];
            snprintf(dest, sizeof(dest), "%s\\%s.exe", out_dir, name);
            if (CopyFileA(src_exe, dest, FALSE)) {
                printf("Built: %s\n", dest);
            } else {
                fprintf(stderr, "Warning: could not copy %s -> %s\n", src_exe, dest);
            }
#else
            char dest[4096];
            snprintf(dest, sizeof(dest), "%s/%s", out_dir, name);
            char cmd[8192];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_exe, dest);
            if (system(cmd) == 0) {
                printf("Built: %s\n", dest);
            } else {
                fprintf(stderr, "Warning: could not copy %s -> %s\n", src_exe, dest);
            }
#endif
        }
    }

    printf("[Build] 3/5 Done.\n");
    fflush(stdout);
    printf("Built: %s.gcBundle -> %s\n", name, out_dir);
    return 0;
}
#endif /* GCL_SKIP_BUNDLE (build_project) */

/* ---------- Python HTTP host (-pyhost) ---------- */
static int pyhost(const char *port_str) {
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: invalid port '%s'\n", port_str ? port_str : "");
        return 1;
    }
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "python -m http.server %d", port);
#else
    snprintf(cmd, sizeof(cmd), "python3 -m http.server %d", port);
#endif
    printf("Serving HTTP on http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);
    return system(cmd);
}

/* ---------- Usage ---------- */
static void usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  gcl -ide [file]\n"
        "  gcl -debug -run file.gcsf\n"
        "  gcl -luarun file.lua\n"
        "  gcl -pyrun file.py\n"
        "  gcl -build project.gcdata -o dir\n"
        "  gcl -new path [--lua] [--luaraylib] [--python] [--pyraylib]\n"
        "  gcl -pyhost <port>\n");
}

int main(int argc, char **argv) {
    g_gcl_argc = argc;
    g_gcl_argv = argv;

#ifdef _WIN32
    /* Make console output UTF-8 — so non-ASCII characters display correctly */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /* Linux: to find the Python embed library at runtime, add the
       Embeded/Python_Runtime directory from the build output to LD_LIBRARY_PATH.
       Embed.so, gcl.so, etc. carry a DT_NEEDED dependency on
       libpython3.14.so.1.0; without this directory dlopen fails and the
       "unknown module 'Embed'" error occurs.
       NOTE: setenv has no effect after the dynamic linker has started — so we
       preload libpython manually with dlopen(RTLD_GLOBAL). That way the
       dependency is already resolved in memory when Embed.so and gcl.so open. */
#ifdef __linux__
    {
        char exe_dir[4096] = { 0 };
        get_exe_dir(exe_dir, sizeof(exe_dir));
        const char *ld = getenv("LD_LIBRARY_PATH");
        char new_ld[8192];
        snprintf(new_ld, sizeof(new_ld), "%s/Library/Embeded/Python_Runtime%s%s",
                 exe_dir, ld ? ":" : "", ld ? ld : "");
        setenv("LD_LIBRARY_PATH", new_ld, 1);
        /* preload libpython manually */
        char py_lib[4096];
        snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.14.so.1.0", exe_dir);
        if (access(py_lib, 0) == 0) {
            void *ph = dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
            if (!ph) {
                snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.14.so", exe_dir);
                if (access(py_lib, 0) == 0) dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
                else {
                    snprintf(py_lib, sizeof(py_lib), "%s/Library/Embeded/Python_Runtime/libpython3.so", exe_dir);
                    if (access(py_lib, 0) == 0) dlopen(py_lib, RTLD_NOW | RTLD_GLOBAL);
                }
            }
        }
    }
#endif

    if (argc < 2) {
        /* A built project exe (a copy of gcl.exe): if there is a *.gcBundle next
           to it, run main.gcsf from the bundle — it does NOT start the IDE.
           A normal gcl.exe with no args opens the IDE (original behavior). */
#ifndef GCL_SKIP_BUNDLE
        char exe_dir[4096] = { 0 };
        get_exe_dir(exe_dir, sizeof(exe_dir));
        char bundle_path[8192];
        if (find_bundle_in_dir(exe_dir, bundle_path, sizeof(bundle_path))) {
            return run_bundle_program(bundle_path);
        }
#endif
        /* default: IDE */
        return run_ide(NULL);
    }

    if (strcmp(argv[1], "-ide") == 0) {
        return run_ide(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "-run") == 0 && argc >= 3) {
        /* GCL script arguments: gcl -run file.gcsf a1 a2 → argc/argv a1,a2 */
        return run_gcl(argv[2], argc - 3, argv + 3);
    }
    if (strcmp(argv[1], "-debug") == 0 && argc >= 4 && strcmp(argv[2], "-run") == 0) {
        gcl_debug = 1;
        return run_gcl(argv[3], argc - 4, argv + 4);
    }
    if (strcmp(argv[1], "-luarun") == 0 && argc >= 3) {
        return run_lua(argv[2]);
    }
    if (strcmp(argv[1], "-pyrun") == 0 && argc >= 3) {
        return run_python(argv[2]);
    }
#ifndef GCL_SKIP_BUNDLE
    if (strcmp(argv[1], "-build") == 0 && argc >= 3) {
        const char *out = NULL;
        for (int i = 3; i < argc - 1; i++) {
            if (strcmp(argv[i], "-o") == 0) { out = argv[i + 1]; break; }
        }
        if (!out) out = "build";
        return build_project(argv[2], out);
    }
#endif
    if (strcmp(argv[1], "-new") == 0 && argc >= 3) {
        int lua = 1, luaraylib = 1, py = 1, pyraylib = 1;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--no-lua") == 0) lua = 0;
            else if (strcmp(argv[i], "--lua") == 0) lua = 1;
            else if (strcmp(argv[i], "--no-luaraylib") == 0) luaraylib = 0;
            else if (strcmp(argv[i], "--luaraylib") == 0) luaraylib = 1;
            else if (strcmp(argv[i], "--no-python") == 0) py = 0;
            else if (strcmp(argv[i], "--python") == 0) py = 1;
            else if (strcmp(argv[i], "--no-pyraylib") == 0) pyraylib = 0;
            else if (strcmp(argv[i], "--pyraylib") == 0) pyraylib = 1;
        }
        return new_project(argv[2], lua, luaraylib, py, pyraylib);
    }
    if (strcmp(argv[1], "-pyhost") == 0 && argc >= 3) {
        return pyhost(argv[2]);
    }
    /* "language ..." — simple_doc.md: gcl language <dirs> → build (delegates to makefile.py) */
    if (strcmp(argv[1], "language") == 0) {
        const char *cmd =
#ifdef _WIN32
            "python language\\makefile.py";
#else
            "python3 language/makefile.py";
#endif
        int rc = system(cmd);
        return rc == 0 ? 0 : 1;
    }

    usage();
    return 1;
}
