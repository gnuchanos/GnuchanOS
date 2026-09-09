/*
 * gcl_main.c — GCL CLI ana girişi.
 *
 * Kullanım (simple_doc.md):
 *   gcl -ide [file]                 IDE başlat
 *   gcl -debug -run file.gcsf       GCL script çalıştır
 *   gcl -luarun file.lua            Lua script çalıştır
 *   gcl -pyrun file.py              Python script çalıştır
 *   gcl -build project.gcdata -o d  Proje build (exe + gcBundle)
 *   gcl -new path [--lua|--python]  Proje iskeleti oluştur
 */

#define _GNU_SOURCE   /* realpath prototipi için (POSIX.1-2008; -std=c99 altında gerekli) */
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

/* IDE'nin kendi exe'ni bulması için global argv */
int g_gcl_argc = 0;
char **g_gcl_argv = NULL;
/* gcl_runner.c'de tanımlı — -debug bayrağıyla etkinleşir */
extern int gcl_debug;

/* GCL SimpleRunner — lexer→parser→runner */
#include "gcl_simple_runner.h"

/* Embedded runtimes (-luarun / -pyrun CLI modları) — Artık Embed.dll'den DİNAMİK yüklenir */
#include "gcl_shared_state.h"

/* IDE — ayrı DLL olarak yüklenir (Programs/ide.dll|.so). gcl.exe küçülür. */
#ifdef _WIN32
typedef int (*GclIdeRunFn)(const char *path);
#else
typedef int (*GclIdeRunFn)(const char *path);
#endif

/* Çalışan gcl.exe'nin bulunduğu dizin.
   Windows: GetModuleFileNameA → her zaman TAM yol (argv[0] göreli olsa bile).
   Linux: argv[0] tam yol değilse realpath ile çalışma dizinine göre çöz.
   Eski kod argv[0]="./gcl.exe" veya "gcl.exe" iken exe_dir'i hiç kırpmıyor,
   "gcl.exe\Programs\ide.dll" gibi YANLIŞ yol üretiyordu. */
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
        /* Her durumda mutlak yol üret: realpath ile çöz.
           Aksi halde "./language/build/gnuLinux/gcl" gibi göreli yoldan
           "exe_dir" göreli kalır ve LD_LIBRARY_PATH göreli olur — dlopen
           libpython3.14.so.1.0'ı bulamaz ("unknown module 'Embed'"). */
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

    /* IDE'nin gcl.exe'yi çağırabilmesi için (gcl -run/-new/-build) gcl.exe yolunu ortama ver.
       ide.dll artık g_gcl_argv'ye erişemez; bu ortam değişkeni üzerinden bulur. */
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
        /* Fallback: PATH üzerinden dene */
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

/* Platform yardımcıları */
const char *gcl_os_name(void);
int gcl_ensure_dir(const char *dir);

/* ---------- Dosya oku ---------- */
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

/* ---------- Dış interpreter çalıştır (stdin'e ver, extract yok) ---------- */
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

/* ---------- GCL script çalıştırma: gerçek lexer→parser→runner pipeline ---------- */
static int run_gcl(const char *path, int script_argc, char **script_argv) {
    char *src = read_file(path);
    if (!src) { fprintf(stderr, "Error: cannot read '%s'\n", path); return 1; }
    /* base_dir = dosyanın bulunduğu dizin (include çözümlemesi için).
       Her durumda MUTLAK yol üret: göreli yol/sadece dosya adı verildiğinde
       çalışma dizinine göre çöz. Embed.Run alt süreçleri GCL_PROJECT_DIR
       üzerinden script'i arar; göreli yol kalırsa "D:/scripts/main.lua"
       gibi yanlış yollar oluşur. */
    char base_dir[4096];
    {
        char dir_tmp[4096];
        snprintf(dir_tmp, sizeof(dir_tmp), "%s", path);
        char *slash = strrchr(dir_tmp, '\\');
        if (!slash) slash = strrchr(dir_tmp, '/');
        if (slash) *slash = '\0';
        else {
            /* yalnızca dosya adı → çalışma dizini */
            snprintf(dir_tmp, sizeof(dir_tmp), ".");
        }
#ifdef _WIN32
        _fullpath(base_dir, dir_tmp, sizeof(base_dir));
#else
        if (realpath(dir_tmp, base_dir) == NULL)
            snprintf(base_dir, sizeof(base_dir), "%s", dir_tmp);
#endif
    }
    /* Embed.Run alt süreçleri (gcl -luarun/-pyrun) için proje kökünü ve
       shared store yolunu ortam değişkeniyle aktar.
       Project Run (IDE) GCL_PROJECT_DIR'i proje köküne zaten set eder;
       standalone -run'da bu env yoksa base_dir kullanılır. */
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

/* ---------- gcBundle'dan build edilmiş programı çalıştır ---------- */
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
    /* Godot PCK mantığı: exe, bundle'ı kendine yanına açar. Library/Embed.dll,
       scripts/, assets/ diske çıkar — böylece #native <Embed> + Embed.Run
       (Lua/Python) pencereleri açılır. Programs/ bundle'da ZATEN YOK (build'de
       sadece scripts + Library gömülür). */
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
    /* KRİTİK: bundle blob'u null-terminated DEĞİL. Lexer/parser null terminator'a
       kadar okur; terminator yoksa blob'daki rastgele veri okunur ve
       "unexpected character" hatası oluşur. Kaynağı null-terminated kopyaya al. */
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

/* ---------- Embed.dll dinamik yükleme (-luarun / -pyrun) ---------- */
/* Embed.dll artık Lua/Python runtime + LuaRaylib/LuaRaygui binding'lerini içerir.
   gcl.exe yalnızca GCL dilidir; -luarun/-pyrun Embed.dll'den yüklenir. */

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

/* ---------- Lua script çalıştır (-luarun) — Embed.dll'den ---------- */
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

/* ---------- Python script çalıştır (-pyrun) — Embed.dll'den ---------- */
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

/* ---------- Proje iskeleti oluştur (-new) ---------- */
/* simple_doc.md: gcl -new project_name
   Project_name/
       assets/         --> raylib de kullanilicak (gcl, lua, python otomatik gorebilmeli)
       external/       --> .so, .dll gibi seyleri burada ariycak (#external <file.dll>)
       include/        --> .gcsf
       out/            --> project_name, project_name.gcBundle
       scripts/        --> lua, python dosyalari
       main.gcsf
       project.gcdata  --> JSON (asagidaki sema)
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
       Lua/Python seçiliyse Embed.Run çağrıları; LuaRaylib/PyRaylib seçiliyse
       Raylib window örneği; hiçbiri seçili değilse yalnızca print örneği. */
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
        /* Embed pencereleri ÖNCE açılır — Lua/Python raylib pencereleri ana pencereyi beklemez */
        if (with_lua) fputs("    Embed.Run(type=\"lua\");\n", f);
        if (with_python) fputs("    Embed.Run(type=\"python\");\n", f);
        /* GCL raylib penceresi: kullanıcı kapatana kadar açık kalır */
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
    /* project.gcdata — JSON (simple_doc.md semasi) */
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
                /* Lua 2D örneği — gerçek Camera2D tablo nesnesi (IDE ile aynı) */
                const char *tpl =
                    "-- GCL Lua 2D ornegi\n"
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
                    "-- GCL Lua ornegi (print)\n"
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
                /* Python 2D örneği — gerçek Camera2D dict nesnesi (IDE ile aynı) */
                const char *tpl =
                    "# GCL Python 2D ornegi\n"
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
                    "# GCL Python ornegi (print)\n"
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
    /* out_dir const parametresini yerel mutable buffer'a al — kırpma burada yapılır */
    char out_dir[4096];
    snprintf(out_dir, sizeof(out_dir), "%s", out_dir_arg);
    /* project.gcdata'dan proje adını al (JSON: "project_name": "X") */
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
    /* Proje adı SADECE dosya adı olmalı — baştaki / \ yol ayraçlarını kırp.
       Aksi halde out_dir içinde alt yol oluşur (örn. "test_project//_31_final.exe"). */
    {
        char *nm = name;
        while (*nm == '/' || *nm == '\\' || *nm == '.') nm++;
        if (!*nm) nm = "project";
        memmove(name, nm, strlen(nm) + 1);
        /* olmayan karakterleri de temizle: boşluk ve : ? * " < > | */
        for (char *q = name; *q; q++) {
            if (*q == ' ' || *q == ':' || *q == '?' || *q == '*' || *q == '"' || *q == '<' || *q == '>' || *q == '|')
                *q = '_';
        }
    }
    /* out_dir sonundaki / yolları kırp */
    {
        size_t odl = strlen(out_dir);
        while (odl > 1 && (out_dir[odl - 1] == '/' || out_dir[odl - 1] == '\\')) { out_dir[odl - 1] = '\0'; odl--; }
    }
    /* proje kökü = gcdata'nın parent'ı */
    char project_dir[4096];
    snprintf(project_dir, sizeof(project_dir), "%s", gcdata);
    char *slash = strrchr(project_dir, '\\');
    if (!slash) slash = strrchr(project_dir, '/');
    if (slash) *slash = '\0';

    /* Çıktı dizinini oluştur (yoksa gcBundle yazılamaz) */
    gcl_ensure_dir(out_dir);

    /* Runtime dizini = çalışan gcl.exe'nin bulunduğu build/<os>/ (Library/ altında).
       Doc: ".gcBundle — all runtime and dll or so in this place". */
    char runtime_dir[4096] = "";
    get_exe_dir(runtime_dir, sizeof(runtime_dir));

    /* gcBundle üret (proje + runtime/Library) */
    printf("[Build] 1/5 Proje + runtime paketleniyor...\n");
    fflush(stdout);
    if (gcb_build_project_runtime(project_dir, runtime_dir, name, out_dir) != 0) {
        fprintf(stderr, "Error: could not build project\n");
        return 1;
    }
    printf("[Build] 2/5 Çalıştırılabilir kopyalanıyor...\n");
    fflush(stdout);

    /* Doc: Project_name.exe — mevcut çalışan gcl exe'yi out_dir'e kopyala */
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

    printf("[Build] 3/5 Tamamlandı.\n");
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
    /* Konsol çıktısını UTF-8 yap — Türkçe karakterler düzgün görünsün */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /* Linux: Python embed kütüphanesini runtime'da bulmak için LD_LIBRARY_PATH'e
       build çıktısındaki Embeded/Python_Runtime dizinini ekle. Embed.so, gcl.so vb.
       libpython3.14.so.1.0'a DT_NEEDED bağımlılığı taşır; bu dizin yoksa dlopen
       başarısız olur ve "unknown module 'Embed'" hatası alınır.
       NOT: setenv dinamik linker başladıktan sonra etkisizdir — bu yüzden
       libpython'u manuel dlopen(RTLD_GLOBAL) ile ön-yükleriz. Böylece
       Embed.so ve gcl.so açılırken bağımlılık bellekte çözülmüş olur. */
#ifdef __linux__
    {
        char exe_dir[4096] = { 0 };
        get_exe_dir(exe_dir, sizeof(exe_dir));
        const char *ld = getenv("LD_LIBRARY_PATH");
        char new_ld[8192];
        snprintf(new_ld, sizeof(new_ld), "%s/Library/Embeded/Python_Runtime%s%s",
                 exe_dir, ld ? ":" : "", ld ? ld : "");
        setenv("LD_LIBRARY_PATH", new_ld, 1);
        /* libpython'u manuel ön-yükle */
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
        /* Build edilen proje exe'si (gcl.exe kopyası): yanında *.gcBundle varsa
           bundle içindeki main.gcsf'i çalıştır — IDE'yi BAŞLATMAZ.
           Normal gcl.exe argsız çalışınca IDE açılır (orijinal davranış). */
#ifndef GCL_SKIP_BUNDLE
        char exe_dir[4096] = { 0 };
        get_exe_dir(exe_dir, sizeof(exe_dir));
        char bundle_path[8192];
        if (find_bundle_in_dir(exe_dir, bundle_path, sizeof(bundle_path))) {
            return run_bundle_program(bundle_path);
        }
#endif
        /* varsayılan: IDE */
        return run_ide(NULL);
    }

    if (strcmp(argv[1], "-ide") == 0) {
        return run_ide(argc > 2 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "-run") == 0 && argc >= 3) {
        /* GCL script argümanları: gcl -run file.gcsf a1 a2 → argc/argv a1,a2 */
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
    /* "language ..." — simple_doc.md: gcl language <dirs> → build (makefile.py delege) */
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
