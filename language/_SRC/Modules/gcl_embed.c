/*
 * gcl_embed.c — GCL Embed modülü (.dll/.so).
 *
 * simple_doc.md:
 *   Embed.Run(type="python");  Embed.Run(type="lua");
 *   Embed.Stop(type="python");
 *   Embed.GetValue(type="python", value);
 *   Embed.SendValue(type="python", value);
 *   (lua varyantları da aynı)
 *
 * Gerçek backend: gcl_embed_python.c / gcl_embed_lua.c
 */

#include "gcl_module.h"

#include "gcl_embed_python.h"
#include "gcl_embed_lua.h"
#include "gcl_shared_state.h"
#include "gcl_terminal_spawn.h"   /* _SRC/include — ayri terminal penceresi */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#if !defined(GCL_HAVE_PYTHON) && !defined(GCL_EMBED_PYTHON_DYNAMIC)
/* Python backend'i yoksa stub — çağrılar 0 döndürür */
int gcl_py_runtime_init(void) { return 0; }
void gcl_py_runtime_shutdown(void) {}
int gcl_py_runtime_is_active(void) { return 0; }
int gcl_py_set_global(const char *name, const char *value) { (void)name; (void)value; return 0; }
int gcl_py_get_global(const char *name, char *out, size_t out_size) { (void)name; (void)out; (void)out_size; return 0; }
int gcl_py_run_simple(const char *code) { (void)code; return 0; }
#endif

/* Alt süreç başlat (async): raylib tek pencere desteklediği için
   Embed.Run(type="lua"/"python") ilgili script'i ayrı gcl sürecinde çalıştırır.
   GCL_SHARED_FILE ortam değişkeni üzerinden aynı shared store paylaşılır.
   Çocuk süreç handle'ları saklanır; Embed.Stop(type) bunları bekler/temizler. */

#ifdef _WIN32
static HANDLE g_child_lua = NULL;
static HANDLE g_child_python = NULL;

static int spawn_runner(const char *type) {
    const char *proj_dir = getenv("GCL_PROJECT_DIR");
    if (!proj_dir || !proj_dir[0]) proj_dir = ".";
    char proj_abs[4096];
#ifdef _WIN32
    _fullpath(proj_abs, proj_dir, sizeof(proj_abs));
#else
    if (realpath(proj_dir, proj_abs) == NULL)
        snprintf(proj_abs, sizeof(proj_abs), "%s", proj_dir);
#endif
    proj_dir = proj_abs;

    char script[4096];
    if (strcmp(type, "python") == 0)
        snprintf(script, sizeof(script), "%s/scripts/main.py", proj_dir);
    else if (strcmp(type, "lua") == 0)
        snprintf(script, sizeof(script), "%s/scripts/main.lua", proj_dir);
    else
        return 0;

    char exe[4096] = "";
    GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));

    char cmd[8192];
    if (strcmp(type, "python") == 0)
        snprintf(cmd, sizeof(cmd), "\"%s\" -pyrun \"%s\"", exe, script);
    else
        snprintf(cmd, sizeof(cmd), "\"%s\" -luarun \"%s\"", exe, script);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    /* #pragma commandline terminal uygulaması: ana program GCL_TERMINAL=1 ile
       işaretlenmişse Lua/Python KENDİ terminal penceresinde açılır (ornek dosya:
       "eger Embeded.run ile python ve lua var ise onlar icinde ayri terminal
       acilir ve orada baslatilir"). CREATE_NEW_CONSOLE vermezsek çocuk süreç
       ana terminali paylaşır ve aynı pencerede karışır. */
    DWORD flags = 0;
    {
        const char *term = getenv(GCL_TERMINAL_ENV);
        if (term && term[0] && strcmp(term, "0") != 0) flags = CREATE_NEW_CONSOLE;
    }
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    if (strcmp(type, "python") == 0) g_child_python = pi.hProcess;
    else g_child_lua = pi.hProcess;
    return 1;
}

static double fn_stop(int argc, const char **argv) {
    const char *type = argc > 0 ? argv[0] : "";
    HANDLE *h = NULL;
    if (strcmp(type, "python") == 0) h = &g_child_python;
    else if (strcmp(type, "lua") == 0) h = &g_child_lua;
    if (h && *h) {
        WaitForSingleObject(*h, INFINITE);
        CloseHandle(*h);
        *h = NULL;
    }
    gcl_shared_cleanup();
    return 1.0;
}
#else
static pid_t g_child_lua = -1;
static pid_t g_child_python = -1;

static int spawn_runner(const char *type) {
    const char *proj_dir = getenv("GCL_PROJECT_DIR");
    if (!proj_dir || !proj_dir[0]) proj_dir = ".";

    char script[4096];
    if (strcmp(type, "python") == 0)
        snprintf(script, sizeof(script), "%s/scripts/main.py", proj_dir);
    else if (strcmp(type, "lua") == 0)
        snprintf(script, sizeof(script), "%s/scripts/main.lua", proj_dir);
    else
        return 0;

    char exe[4096] = "";
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) exe[n] = '\0';

    pid_t pid;
    {
        const char *term = getenv(GCL_TERMINAL_ENV);
        if (term && term[0] && strcmp(term, "0") != 0) {
            /* #pragma commandline terminal uygulaması: Lua/Python KENDİ terminal
               penceresinde başlatılır (gcl_term_spawn_argv bir terminal emülatörü
               bulup argv'yi onun içinde çalıştırır). */
            const char *cargv[4];
            int cargc = 0;
            cargv[cargc++] = exe;
            cargv[cargc++] = (strcmp(type, "python") == 0) ? "-pyrun" : "-luarun";
            cargv[cargc++] = script;
            cargv[cargc] = NULL;
            pid = gcl_term_spawn_argv(cargc, cargv);
            if (pid <= 0) return 0;
        } else {
            pid = fork();
            if (pid < 0) return 0;
            if (pid == 0) {
                if (strcmp(type, "python") == 0)
                    execlp(exe, exe, "-pyrun", script, (char *)NULL);
                else
                    execlp(exe, exe, "-luarun", script, (char *)NULL);
                _exit(127);
            }
        }
    }
    if (strcmp(type, "python") == 0) g_child_python = pid;
    else g_child_lua = pid;
    return 1;
}

static double fn_stop(int argc, const char **argv) {
    const char *type = argc > 0 ? argv[0] : "";
    pid_t *p = NULL;
    if (strcmp(type, "python") == 0) p = &g_child_python;
    else if (strcmp(type, "lua") == 0) p = &g_child_lua;
    if (p && *p > 0) {
        waitpid(*p, NULL, 0);
        *p = -1;
    }
    gcl_shared_cleanup();
    return 1.0;
}
#endif

static double fn_run(int argc, const char **argv) {
    const char *type = argc > 0 ? argv[0] : "";
    return spawn_runner(type) ? 1.0 : 0.0;
}

static double fn_get_value(int argc, const char **argv) {
    /* GetValue(type, name) → name değişkeninin değerini döndür (double) */
    if (argc < 2 || !argv[0] || !argv[1]) return 0.0;
    const char *type = argv[0];
    const char *name = argv[1];
    char buf[1024] = { 0 };

    if (strcmp(type, "python") == 0) {
        if (gcl_py_runtime_is_active() && gcl_py_get_global(name, buf, sizeof(buf)))
            return atof(buf);
    } else if (strcmp(type, "lua") == 0) {
        if (gcl_lua_runtime_is_active() && gcl_lua_get_global(name, buf, sizeof(buf)))
            return atof(buf);
    }
    /* Alt süreç paylaşımı: shared store'dan oku */
    if (gcl_shared_get(name, buf, sizeof(buf)))
        return atof(buf);
    return 0.0;
}

static double fn_send_value(int argc, const char **argv) {
    /* SendValue(type, name, value) → name = value */
    if (argc < 3 || !argv[0] || !argv[1] || !argv[2]) return 0.0;
    const char *type = argv[0];
    const char *name = argv[1];
    const char *value = argv[2];

    if (strcmp(type, "python") == 0) {
        if (gcl_py_runtime_is_active() && gcl_py_set_global(name, value))
            return 1.0;
    } else if (strcmp(type, "lua") == 0) {
        if (gcl_lua_runtime_is_active() && gcl_lua_set_global(name, value))
            return 1.0;
    }
    /* Alt süreç paylaşımı: shared store'a yaz */
    return gcl_shared_set(name, value) ? 1.0 : 0.0;
}

static double fn_is_active(int argc, const char **argv) {
    const char *type = argc > 0 ? argv[0] : "";
    if (strcmp(type, "python") == 0) {
        return gcl_py_runtime_is_active() ? 1.0 : 0.0;
    } else if (strcmp(type, "lua") == 0) {
        return gcl_lua_runtime_is_active() ? 1.0 : 0.0;
    }
    return 0.0;
}

static const GclNativeEntry g_entries[] = {
    {"Run", fn_run},
    {"Stop", fn_stop},
    {"GetValue", fn_get_value},
    {"SendValue", fn_send_value},
    {"IsActive", fn_is_active},
};

GCL_EXPORT const GclNativeEntry *gcl_embed_get_functions(int *count) {
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
