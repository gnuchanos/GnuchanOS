/*
 * ide_proc.c — Asenkron dış süreç yönetimi.
 *
 * "Run" komutu senkron popen/fgets ile çalıştırıldığında IDE ana thread'i
 * child süreç çıkana kadar bloke olur (UI kilitlenir) ve child'in PID'i hiç
 * tutulmadığı için IDE kapanınca süreç açık kalır.
 *
 * Bu dosya süreci asenkron başlatır, çıktıyı her frame'de toplar ve
 * IDE kapanınca/Stop'a basılınca süreci sonlandırır.
 *
 *   Windows : CreateProcess + anonymous pipe + Job Object (süreç ağacını öldürür)
 *   POSIX   : fork + exec + pipe
 *
 * DIKKAT (kök neden): Windows'ta pipe'tan okuma hemşehrimiz `ReadFile` bloke
 * olur — pipe'ta veri yokken ve child hâlâ çalışırken IDE donar. Bu yüzden
 * `ReadFile`'dan önce `PeekNamedPipe` ile veri olup olmadığını kontrol ederiz.
 */

/* usleep: -std=c99 + _POSIX_C_SOURCE=200809L altında glibc'de gizli; _DEFAULT_SOURCE açar.
   TÜM sistem başlıklarından ÖNCE tanımlanmalı (gcl_ide_internal.h <unistd.h> içerebilir). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "gcl_ide_internal.h"

#ifdef _WIN32
/* windows.h'in CloseWindow/ShowCursor fonksiyonları raylib.h'in
   `void CloseWindow(void) / void ShowCursor(void)` bildirimleriyle aynı isimde
   farklı imzayı taşır → derleme hatası. Bu iki sembolü makroyla gizliyoruz;
   windows.h içinde CloseWindow_/ShowCursor_ olarak dönüşür, raylib'in
   fonksiyonları sorunsuz kalır. NOGDI (Rectangle) + WIN32_LEAN_AND_MEAN da
   eklenir. */
#ifndef NOGDI
#define NOGDI
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define CloseWindow CloseWindow_
#define ShowCursor ShowCursor_
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef Rectangle
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#endif

#include "gcl_ide_internal.h"

struct GclIdeProcess {
#ifdef _WIN32
    HANDLE hProc;
    HANDLE hRead;
    HANDLE hWrite;
    HANDLE hJob;            /* Job Object — süreç ağacını (cmd + gcl) birlikte öldürür */
#else
    pid_t pid;
    int pipe_fd[2];
#endif
    int active;
    size_t out_len;          /* ed->output'a eklenmiş bayt sayısı */
    double started_at;
    char label[256];
    FILE *logf;              /* project.gclog — çıktı buraya da yazılır (NULL = log yok) */
};

/* Mevcut child'ı temizle (yeni başlatmadan önce). */
void gcl_ide_proc_kill(Editor *ed) {
    if (!ed->run_proc) return;
    GclIdeProcess *p = ed->run_proc;
#ifdef _WIN32
    /* Job Object önce: içindeki TÜM süreçleri (cmd.exe + başlattığı gcl.exe) tek seferde öldür.
       Böylece "IDE kapanınca proje penceresi açık kalıyor" sorunu çözülür. */
    if (p->hJob) {
        TerminateJobObject(p->hJob, 1);
        CloseHandle(p->hJob);
        p->hJob = NULL;
    }
    if (p->hProc) {
        TerminateProcess(p->hProc, 1);
        WaitForSingleObject(p->hProc, 100);
        CloseHandle(p->hProc);
    }
    if (p->hRead) CloseHandle(p->hRead);
    if (p->hWrite) CloseHandle(p->hWrite);
#else
    if (p->pid > 0) {
        kill(p->pid, SIGTERM);
        for (int i = 0; i < 10; i++) {
            int st; pid_t r = waitpid(p->pid, &st, WNOHANG);
            if (r == p->pid || r == -1) break;
            usleep(20000);
        }
        kill(p->pid, SIGKILL);
        waitpid(p->pid, NULL, 0);
    }
    if (p->pipe_fd[0] >= 0) close(p->pipe_fd[0]);
    if (p->pipe_fd[1] >= 0) close(p->pipe_fd[1]);
#endif
    if (p->logf) fclose(p->logf);
    free(p);
    ed->run_proc = NULL;
    ed->running = 0;
}

/* Asenkron süreç başlat. label: başlatılan komut (output'a "> cmd" yazılır).
   logf: project.gclog — süreç çıktısı buraya da yazılır (NULL = log yok). */
void gcl_ide_proc_start(Editor *ed, const char *cmd, const char *label, FILE *logf) {
    if (!ed || !cmd) return;
    /* Önce eski süreç varsa sonlandır */
    gcl_ide_proc_kill(ed);

    GclIdeProcess *p = (GclIdeProcess *)calloc(1, sizeof(GclIdeProcess));
    if (!p) return;
    p->active = 1;
    p->out_len = 0;
    p->started_at = GetTime();
    p->logf = logf;
    if (label) snprintf(p->label, sizeof(p->label), "%s", label);
    else snprintf(p->label, sizeof(p->label), "%s", cmd);

#ifdef _WIN32
    p->hProc = NULL;
    p->hRead = NULL;
    p->hWrite = NULL;
    p->hJob = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&p->hRead, &p->hWrite, &sa, 0)) {
        free(p);
        return;
    }
    SetHandleInformation(p->hRead, HANDLE_FLAG_INHERIT, 0);

    /* CreateProcess ilk argümanı executable — komut satırı olarak ver. */
    char cmdline[8192];
    snprintf(cmdline, sizeof(cmdline), "%s", cmd);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.hStdError = p->hWrite;
    si.hStdOutput = p->hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) {
        CloseHandle(p->hRead);
        CloseHandle(p->hWrite);
        free(p);
        snprintf(ed->output, sizeof(ed->output), "Error: could not start process %s\n", label ? label : cmd);
        ed->output_len = strlen(ed->output);
        ed->output_visible = 1;
        return;
    }
    CloseHandle(p->hWrite);
    p->hWrite = NULL;
    p->hProc = pi.hProcess;
    CloseHandle(pi.hThread);

    /* Job Object: cmd.exe + onun başlattığı gcl.exe'yi AYNI job'a koy.
       Job kapatılınca (kill/IDE kapanışı) içindeki tüm süreçler sonlanır. */
    p->hJob = CreateJobObjectA(NULL, NULL);
    if (p->hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        memset(&jeli, 0, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (SetInformationJobObject(p->hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            AssignProcessToJobObject(p->hJob, p->hProc);
        }
    }
#else
    int fd[2];
    if (pipe(fd) != 0) {
        free(p);
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        free(p);
        return;
    }
    if (pid == 0) {
        /* child */
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    close(fd[1]);
    p->pid = pid;
    p->pipe_fd[0] = fd[0];
    p->pipe_fd[1] = -1;
    int flags = fcntl(fd[0], F_GETFL, 0);
    fcntl(fd[0], F_SETFL, flags | O_NONBLOCK);
#endif

    ed->run_proc = p;
    ed->running = 1;
    ed->output[0] = '\0';
    ed->output_len = 0;
    ed->output_visible = 1;
    /* Yeni build/run başlarken eski sonuç mesajını temizle (Build OK görünmesin) */
    ed->status_msg[0] = '\0';
    ed->status_msg_time = 0;
    char hdr[512];
    snprintf(hdr, sizeof(hdr), "> %s\n", label ? label : cmd);
    size_t hl = strlen(hdr);
    if (hl < sizeof(ed->output) - 1) {
        memcpy(ed->output, hdr, hl);
        ed->output_len = hl;
        ed->output[hl] = '\0';
    }
}

    /* Her frame'de pipe'dan okunabilir veriyi topla; süreç bittiyse temizle. */
void gcl_ide_proc_poll(Editor *ed) {
    if (!ed->run_proc) return;
    GclIdeProcess *p = ed->run_proc;
    if (!p->active) return;

#ifdef _WIN32
    char buf[1024];
    DWORD readn = 0;
    DWORD avail = 0;
    /* KRİTİK: `ReadFile` senkron pipe'ta BLOKE olur — veri yokken + süreç çalışırken
       IDE donar. Önce `PeekNamedPipe` ile gerçekten okunacak bayt var mı bak. */
    while (PeekNamedPipe(p->hRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        DWORD to_read = avail < sizeof(buf) - 1 ? avail : sizeof(buf) - 1;
        if (!ReadFile(p->hRead, buf, to_read, &readn, NULL) || readn == 0) break;
        buf[readn] = '\0';
        size_t n = readn;
        /* Çıktı hem ed->output'a hem project.gclog'a yazılır (boş kalmasın). */
        if (p->logf) fwrite(buf, 1, n, p->logf);
        if (ed->output_len + n < sizeof(ed->output) - 1) {
            memcpy(ed->output + ed->output_len, buf, n);
            ed->output_len += n;
            ed->output[ed->output_len] = '\0';
            /* Son çıktı satırını status'ta göster — hangi aşamada olduğu belli olsun */
            char *last_nl = strrchr(ed->output, '\n');
            const char *last_line = last_nl ? last_nl + 1 : ed->output;
            if (last_line[0]) {
                snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", last_line);
                ed->status_msg_time = GetTime();
            }
        } else break;
    }
    if (WaitForSingleObject(p->hProc, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(p->hProc, &code);
        int ec = (int)code;
        p->active = 0;
        /* Build/Run sonucunu status'ta göster: "Build OK" / "Build FAILED (exit N)" */
        char res[256];
        if (ec == 0) snprintf(res, sizeof(res), "%s OK", p->label[0] ? p->label : "Process");
        else snprintf(res, sizeof(res), "%s FAILED (exit %d)", p->label[0] ? p->label : "Process", ec);
        snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", res);
        ed->status_msg_time = GetTime();
        gcl_ide_proc_kill(ed);
    }
#else
    char buf[1024];
    ssize_t rn;
    while ((rn = read(p->pipe_fd[0], buf, sizeof(buf) - 1)) > 0) {
        buf[rn] = '\0';
        size_t n = (size_t)rn;
        /* Çıktı hem ed->output'a hem project.gclog'a yazılır. */
        if (p->logf) fwrite(buf, 1, n, p->logf);
        if (ed->output_len + n < sizeof(ed->output) - 1) {
            memcpy(ed->output + ed->output_len, buf, n);
            ed->output_len += n;
            ed->output[ed->output_len] = '\0';
            /* Son çıktı satırını status'ta göster */
            char *last_nl = strrchr(ed->output, '\n');
            const char *last_line = last_nl ? last_nl + 1 : ed->output;
            if (last_line[0]) {
                snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", last_line);
                ed->status_msg_time = GetTime();
            }
        } else break;
    }
    int st;
    pid_t r = waitpid(p->pid, &st, WNOHANG);
    if (r == p->pid || r == -1) {
        int ec = (r == p->pid && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
        p->active = 0;
        /* Build/Run sonucunu status'ta göster */
        char res[256];
        if (ec == 0) snprintf(res, sizeof(res), "%s OK", p->label[0] ? p->label : "Process");
        else snprintf(res, sizeof(res), "%s FAILED (exit %d)", p->label[0] ? p->label : "Process", ec);
        snprintf(ed->status_msg, sizeof(ed->status_msg), "%s", res);
        ed->status_msg_time = GetTime();
        gcl_ide_proc_kill(ed);
    }
#endif
}
