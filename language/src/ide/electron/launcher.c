/*
 * GnuChanIDE launcher - makefile.py tarafindan derlenir.
 *
 * Amac: build kokunu temiz tutmak (gnuchanos.md FILE TREE).
 *   build/windows/
 *     gcl.exe            <- GCL
 *     GnuChanIDE.exe     <- bu launcher
 *     GnuChanIDE_JUNKS/  <- electron copu (dll, pak, locales,
 *                           resources + asil electron binary)
 *     Library/
 *
 * Launcher, GnuChanIDE_JUNKS icindeki asil Electron binary'sini baslatir ve
 * cwd'yi DEGISTIRMEZ - boylece IDE gcl.exe'yi kendi yaninda bulur.
 *
 * Windows: CreateProcessW (gui subsystem, -mwindows ile derlenir)
 * Linux  : readlink(/proc/self/exe) + fork/exec
 */

#define _CRT_SECURE_NO_WARNINGS

#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    wchar_t self[MAX_PATH];
    wchar_t *slash;
    DWORD n;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;

    (void)hInst; (void)hPrev; (void)lpCmd; (void)nShow;

    n = GetModuleFileNameW(NULL, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return 1;

    slash = wcsrchr(self, L'\\');
    if (slash == NULL)
        return 1;

    /* <exe-dir>/GnuChanIDE_JUNKS/GnuChanIDE.exe -> asil electron binary */
    wcscpy(slash + 1, L"GnuChanIDE_JUNKS\\GnuChanIDE.exe");

    /* exe-dir'i ayir (build/windows = gcl.exe'nin yani) */
    wchar_t workdir[MAX_PATH];
    wcscpy(workdir, self);
    wchar_t *last = wcsrchr(workdir, L'\\');
    if (last != NULL)
        *last = L'\0';

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    ZeroMemory(&pi, sizeof pi);

    /* cwd'yi build kokune set et: boylece IDE process.cwd() uzerinden
     * yanindaki gcl.exe'yi her durumda bulur. */
    if (!CreateProcessW(self, NULL, NULL, NULL, FALSE, 0, NULL, workdir,
                        &si, &pi))
        return 1;

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return (int)code;
}

/* MinGW: -mwindows altinda WinMain arar; guvenli giris noktasi sagla. */
int main(void) {
    return wWinMain(GetModuleHandleW(NULL), NULL, GetCommandLineW(), SW_SHOW);
}

#else  /* Linux */

#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

int main(void) {
    char link[PATH_MAX];
    char *slash;
    char real[PATH_MAX];
    ssize_t n;
    pid_t pid;
    int st = 1;

    n = readlink("/proc/self/exe", link, sizeof link - 1);
    if (n <= 0)
        return 1;
    link[n] = '\0';

    slash = strrchr(link, '/');
    if (slash == NULL)
        return 1;
    *slash = '\0';

    /* <exe-dir>/GnuChanIDE_JUNKS/GnuChanIDE -> asil electron binary */
    snprintf(real, sizeof real, "%s/GnuChanIDE_JUNKS/GnuChanIDE", link);

    pid = fork();
    if (pid < 0)
        return 1;
    if (pid == 0) {
        execl(real, real, (char *)NULL);
        _exit(1);
    }
    if (waitpid(pid, &st, 0) < 0)
        return 1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
}

#endif
