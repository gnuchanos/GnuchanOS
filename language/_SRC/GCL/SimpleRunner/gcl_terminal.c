/*
 * gcl_terminal.c — "#pragma commandline" (terminal uygulamasi) isleyisi.
 *
 *   #pragma commendline      (example dosyasindaki yazim)
 *   #pragma commandline
 *   #pragma cmdline
 *
 * Pragma varsa program OTOMATIK olarak isletim sisteminin terminalinde
 * calisir; terminal yoksa (IDE/GUI cocuk sureci, CREATE_NO_WINDOW) program
 * kendini YENI bir terminal penceresinde yeniden baslatir. Boylece
 * Stdio.scanf girdi bekleyebilir ve IDE icinden de kullanilabilir.
 *
 * Ayrica GCL_TERMINAL=1 isaretlenir; Embed.Run(type="lua"/"python") alt
 * surecleri bu isareti gorup KENDI terminal pencerelerinde acilirlar.
 */

#include "gcl_terminal.h"
#include "gcl_terminal_spawn.h"   /* _SRC/include — ayri terminal baslatma */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#endif

/* Yeniden baslatilan surec isareti — sonsuz yeniden baslatmayi onler. */
#define GCL_TERM_RELAUNCH_ENV "GCL_TERMINAL_RELAUNCHED"

#define GCL_TERM_LINE_MAX 1024

/* ---------- ortam degiskeni yardimcilari ---------- */

static void term_env_set(const char *name, const char *value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, 1);
#endif
}

static int term_env_has(const char *name) {
#ifdef _WIN32
    char buf[16];
    DWORD n = GetEnvironmentVariableA(name, buf, (DWORD)sizeof(buf));
    return n > 0 && n < sizeof(buf);
#else
    const char *v = getenv(name);
    return v && v[0];
#endif
}

/* ---------- #pragma commandline tespiti ---------- */

int gcl_terminal_pragma_present(const char *src) {
    if (!src) return 0;
    const char *p = src;
    char line[GCL_TERM_LINE_MAX];
    while (*p) {
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
        if (*p == '\n') p++;
        line[n] = '\0';

        const char *q = line;
        while (*q == ' ' || *q == '\t') q++;

        /* GCL cok satirli yorum: #| ... |# — icindeki metin pragma sayilmaz.
           (Dokumantasyon bloklari "#pragma commendline" YAZISINI icerebilir.) */
        if (q[0] == '#' && q[1] == '|') {
            if (!strstr(q, "|#")) {
                while (*p) {
                    size_t m = 0;
                    while (*p && *p != '\n' && m < sizeof(line) - 1) line[m++] = *p++;
                    if (*p == '\n') p++;
                    line[m] = '\0';
                    if (strstr(line, "|#")) break;
                }
            }
            continue;
        }

        if (*q != '#') continue;
        q++;
        while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "pragma", 6) != 0) continue;
        q += 6;
        while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "commendline", 11) == 0 ||
            strncmp(q, "commandline", 11) == 0 ||
            strncmp(q, "cmdline", 7) == 0)
            return 1;
    }
    return 0;
}

/* ---------- terminal/console ---------- */

/* Bu surec icin AYRI bir terminal penceresi acilmali mi?
   - Konsol YOKSA (IDE: CREATE_NO_WINDOW / GUI cocuk surec) → evet.
   - Konsol baska sureclerle PAYLASILIYORSA (orn. cmd/pwsh icinden
     `gcl -run file.gcsf`) → evet: Visual Studio davranisi; terminal
     programi KENDI penceresinde acilir, cagiran kabugu isgal etmez.
   - Konsol YALNIZCA bize aitse (exe'ye cift tiklanmis) → hayir; zaten
     kendi penceremizdeyiz, ikinci bir pencere acmak gereksiz olur. */
static int gcl_terminal_should_relaunch(void) {
#ifdef _WIN32
    if (GetConsoleWindow() == NULL) return 1;   /* konsol yok */
    /* Konsola bagli surec sayisi > 1 ise konsol paylasimli demektir. */
    DWORD procs[16];
    DWORD n = GetConsoleProcessList(procs, 16);
    return n != 1;                              /* 1 degilse (0 veya >1) ayri pencere */
#else
    /* POSIX: her zaman ayri terminal penceresi iste (VS davranisi).
       Terminal emulatoru yoksa cagiran taraf mevcut konsolda devam eder. */
    return 1;
#endif
}

/* Son care: ayni surecte konsol ac (yeni pencere baslatilamazsa). */
static void gcl_terminal_alloc_console(void) {
#ifdef _WIN32
    if (GetConsoleWindow() != NULL) return;
    if (AllocConsole()) {
        FILE *f;
        f = freopen("CONIN$", "r", stdin);   (void)f;
        f = freopen("CONOUT$", "w", stdout); (void)f;
        f = freopen("CONOUT$", "w", stderr); (void)f;
    }
#endif
}

/* Windows: programi AYNI komut satiriyla YENI bir konsol penceresinde baslat
   ve pencerenin omru boyunca bekle (IDE "calisiyor" durumunu dogru gosterir,
   Stop/job-object tum surec agacini birlikte kapatir). */
#ifdef _WIN32
static int gcl_terminal_relaunch(void) {
    /* Bu gcl.exe'nin TAM komut satiri: "C:\...\gcl.exe" -run "file.gcsf" */
    char self[8192];
    snprintf(self, sizeof(self), "%s", GetCommandLineA());
    if (!self[0]) return 0;

    /* Madde 4: terminal, program bittikten SONRA da acik kalir. Programi
       dogrudan baslatmak yerine `cmd /k` ile baslatiyoruz: gcl programi
       bittiginde kabuk prompt'ta bekler; kullanici pencereyi x ile ya da
       `exit` yazarak kapatana kadar terminal acik kalir, boylece ciktilar
       ekranda gorunur kalir. */
    const char *comspec = getenv("COMSPEC");
    if (!comspec || !comspec[0]) comspec = "cmd.exe";
    char cmdline[8192 + 64];
    /* cmd /k tirnak kurali:
         self tirnakli basliyorsa → cmd /k ""prog" args"  (cmd dis tirnagi soyar)
         self tirnaksiz basliyorsa → cmd /k prog args
       Tirnaksiz komuta ek tirnak EKLEMEK tum satiri tek program adi yapar ve
       "cannot find the path specified" hatasi verir. */
    if (self[0] == '"')
        snprintf(cmdline, sizeof(cmdline), "\"%s\" /k \"%s\"", comspec, self);
    else
        snprintf(cmdline, sizeof(cmdline), "\"%s\" /k %s", comspec, self);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                        NULL, NULL, &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return 1;
}
#else
/* POSIX: /proc/self/cmdline → argv; argv'yi terminal emulatorunde baslat. */
static int gcl_terminal_relaunch(void) {
    char raw[8192];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return 0;
    ssize_t rd = read(fd, raw, sizeof(raw) - 1);
    close(fd);
    if (rd <= 0) return 0;
    raw[rd] = '\0';

    /* argv'yi tek tirnakli TEK bir kabuk komutuna cevir. */
    char shellcmd[8192 * 2];
    size_t o = 0;
    for (ssize_t i = 0; i < rd && o + 8 < sizeof(shellcmd); ) {
        const char *arg = raw + i;
        size_t len = strlen(arg);
        if (len == 0) break;
        if (o) shellcmd[o++] = ' ';
        shellcmd[o++] = '\'';
        for (size_t k = 0; k < len && o + 5 < sizeof(shellcmd); k++) {
            if (arg[k] == '\'') { memcpy(shellcmd + o, "'\\''", 4); o += 4; }
            else shellcmd[o++] = arg[k];
        }
        shellcmd[o++] = '\'';
        i += (ssize_t)len + 1;
    }
    if (o == 0) return 0;
    /* Madde 4: program bittikten sonra bekle — pencere kullanici kapatana
       kadar acik kalir. */
    snprintf(shellcmd + o, sizeof(shellcmd) - o,
             "; printf '\\n[GCL] Kapatmak icin Enter... '; read _");

    const char *cargv[4];
    cargv[0] = "sh";
    cargv[1] = "-c";
    cargv[2] = shellcmd;
    cargv[3] = NULL;

    pid_t pid = gcl_term_spawn_argv(3, cargv);
    if (pid <= 0) return 0;
    int status = 0;
    waitpid(pid, &status, 0);
    return 1;
}
#endif

int gcl_terminal_enter(void) {
    /* Alt surecler (Lua/Python) ayri terminal acsin diye isaretle. */
    term_env_set(GCL_TERMINAL_ENV, "1");

    /* Zaten yeniden baslatilan surecteyiz (artik yeni terminalin icinde) →
       tekrar acma, programi burada calistir. */
    if (term_env_has(GCL_TERM_RELAUNCH_ENV)) return 0;

    /* Konsol yoksa ya da paylasimliysa (cmd/pwsh icinden calistirma) KENDI
       terminal penceremizi ac — Visual Studio davranisi. */
    if (!gcl_terminal_should_relaunch()) return 0;

    term_env_set(GCL_TERM_RELAUNCH_ENV, "1");
    if (gcl_terminal_relaunch()) return 1;          /* bu surec hemen cikmali */

    /* Yeni terminal acilamadi: ayni surecte konsol ac (scanf yine calissin). */
    gcl_terminal_alloc_console();
    return 0;
}
