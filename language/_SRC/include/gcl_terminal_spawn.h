/*
 * gcl_terminal_spawn.h — Bir komutu AYRI bir terminal penceresinde baslatma.
 *
 * "#pragma commandline" ile isaretlenmis programlar terminal uygulamasidir.
 * Boyle bir program Embed.Run(type="lua"/"python") cagirdiginda Lua/Python
 * runtime'lari ayni konsolu paylasmaz; her biri KENDI terminal penceresinde
 * acilir (ornek: examples/gcl_simple_syntax/11_scanf_commandline_test.gcsf ->
 * "eger Embeded.run ile python ve lua var ise onlar icinde ayri terminal
 *  acilir ve orada baslatilir").
 *
 *   Windows : CreateProcessA(..., CREATE_NEW_CONSOLE, ...)
 *   POSIX   : x-terminal-emulator / konsole / xfce4-terminal / gnome-terminal /
 *             xterm sirayla denenir (GCL_TERMINAL_CMD ile kullanici secer).
 *
 * Bu baslik header-only'dir: hem gcl.exe (kendini yeniden baslatma) hem de
 * Library altindaki .dll / .so modulleri (Embed) ayni kodu kullanir.
 */

#ifndef GCL_TERMINAL_SPAWN_H
#define GCL_TERMINAL_SPAWN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Terminal uygulamasi modu isareti — #pragma commandline gorulunce set edilir
   ve alt sureclere (Lua/Python) miras kalir. */
#define GCL_TERMINAL_ENV "GCL_TERMINAL"

#ifdef _WIN32
#include <windows.h>

/* Windows: komut satirini AYRI bir konsol penceresinde baslat. */
static int gcl_term_spawn_cmdline(const char *cmdline) {
    if (!cmdline || !cmdline[0]) return 0;
    char buf[8192];
    snprintf(buf, sizeof(buf), "%s", cmdline);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, buf, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                        NULL, NULL, &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}
#else
#include <unistd.h>
#include <sys/types.h>

/* $PATH icinde calistirilabilir dosya ara. */
static int gcl_term_find_in_path(const char *name, char *out, size_t outsz) {
    const char *path = getenv("PATH");
    if (!name || !name[0] || !out || outsz == 0) return 0;
    if (name[0] == '/') {
        snprintf(out, outsz, "%s", name);
        return access(out, X_OK) == 0;
    }
    if (!path) return 0;
    const char *p = path;
    while (*p) {
        const char *sep = strchr(p, ':');
        size_t dlen = sep ? (size_t)(sep - p) : strlen(p);
        if (dlen > 0 && dlen + 1 + strlen(name) + 1 <= outsz) {
            snprintf(out, outsz, "%.*s/%s", (int)dlen, p, name);
            if (access(out, X_OK) == 0) return 1;
        }
        p = sep ? sep + 1 : p + dlen;
    }
    return 0;
}

/* Kullanilabilir terminal emulatorunu bul. */
static const char *gcl_term_emulator(char *out, size_t outsz) {
    static const char *candidates[] = {
        "x-terminal-emulator", "konsole", "xfce4-terminal", "gnome-terminal",
        "xterm", NULL
    };
    const char *pref = getenv("GCL_TERMINAL_CMD");
    if (pref && pref[0] && gcl_term_find_in_path(pref, out, outsz)) return out;
    for (int i = 0; candidates[i]; i++) {
        if (gcl_term_find_in_path(candidates[i], out, outsz)) return out;
    }
    return NULL;
}

/* POSIX: argv'yi yeni bir terminal penceresinde baslat.
   Donus: baslatilan terminal surecinin pid'i (>0) veya 0 (basarisiz). */
static pid_t gcl_term_spawn_argv(int argc, const char *const *argv) {
    char emu[4096];
    const char *found = gcl_term_emulator(emu, sizeof(emu));
    if (!found || argc <= 0) return 0;

    /* gnome-terminal `-e` yerine `--wait --` bekler; digerleri `-e` kullanir. */
    int gnome = strstr(found, "gnome-terminal") != NULL;
    const char *child[264];
    int ci = 0;
    child[ci++] = found;
    if (gnome) { child[ci++] = "--wait"; child[ci++] = "--"; }
    else child[ci++] = "-e";
    for (int i = 0; i < argc && ci < 260; i++) child[ci++] = argv[i];
    child[ci] = NULL;

    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        setsid();                                   /* terminali bagimsiz oturuma al */
        execv(found, (char *const *)child);
        _exit(127);
    }
    return pid;
}
#endif /* _WIN32 */

#endif /* GCL_TERMINAL_SPAWN_H */
