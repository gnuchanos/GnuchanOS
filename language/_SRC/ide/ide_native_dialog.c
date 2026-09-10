/*
 * ide_native_dialog.c — Windows native file open/save dialog + folder picker.
 *
 * Because windows.h clashes with raylib's Rectangle/CloseWindow/DrawText names,
 * this file is a SEPARATE translation unit. ide_main.c only uses an extern prototype.
 */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *gcl_ide_native_dialog(int save, const char *initial_dir) {
    OPENFILENAMEA ofn;
    char file[2048] = "";
    char dir[2048] = "";
    if (initial_dir) snprintf(dir, sizeof(dir), "%s", initial_dir);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrInitialDir = dir[0] ? dir : NULL;
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.txt;*.gcsf;*.lua;*.c;*.h;*.md\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = save ? "Save File" : "Open File";
    ofn.lpstrDefExt = "txt";
    ofn.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST)
                     : (OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST);

    BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    if (ok) {
        size_t n = strlen(file) + 1;
        char *r = (char *)malloc(n);
        if (r) memcpy(r, file, n);
        return r;
    }
    return NULL;
}

char *gcl_ide_native_dialog_folder(char *out, size_t outsz) {
    BROWSEINFOA bi;
    memset(&bi, 0, sizeof(bi));
    bi.lpszTitle = "Select Project Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            snprintf(out, outsz, "%s", path);
            CoTaskMemFree(pidl);
            return out;
        }
        CoTaskMemFree(pidl);
    }
    return NULL;
}

#else

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Linux / WSL: no native file dialog. Zenity (GNOME) or kdialog (KDE) opens a
   real GTK/KDE dialog. If neither exists, a simple PATH-based file picker
   (ls + enter) is used. This way the "Open Project"/"Browse" buttons
   actually work on Linux too — in WSL, if zenity is installed it opens via X11. */

static char *run_capture(const char *cmd) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    char tmp[512];
    while (fgets(tmp, sizeof(tmp), p)) {
        size_t t = strlen(tmp);
        char *nb = (char *)realloc(buf, cap + t + 1);
        if (!nb) { free(buf); pclose(p); return NULL; }
        buf = nb;
        memcpy(buf + len, tmp, t);
        len += t;
        cap += t;
    }
    if (buf) buf[len] = '\0';
    pclose(p);
    /* trim the trailing newline in the output */
    if (buf) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = '\0';
    }
    return buf;
}

static int has_cmd(const char *cmd) {
    if (!cmd || !cmd[0]) return 0;
    char full[512];
    snprintf(full, sizeof(full), "command -v %s >/dev/null 2>&1", cmd);
    return system(full) == 0;
}

char *gcl_ide_native_dialog(int save, const char *initial_dir) {
    if (has_cmd("zenity")) {
        char cmd[4096];
        if (save) {
            snprintf(cmd, sizeof(cmd), "zenity --file-selection --save --confirm-overwrite --filename=\"%s/\" 2>/dev/null",
                     initial_dir ? initial_dir : ".");
        } else {
            snprintf(cmd, sizeof(cmd), "zenity --file-selection --filename=\"%s/\" 2>/dev/null",
                     initial_dir ? initial_dir : ".");
        }
        char *r = run_capture(cmd);
        if (r && r[0]) return r;
        free(r);
    }
    if (has_cmd("kdialog")) {
        char cmd[4096];
        if (save) {
            snprintf(cmd, sizeof(cmd), "kdialog --getsavefilename \"%s/\" \"*\" 2>/dev/null",
                     initial_dir ? initial_dir : ".");
        } else {
            snprintf(cmd, sizeof(cmd), "kdialog --getopenfilename \"%s/\" \"*\" 2>/dev/null",
                     initial_dir ? initial_dir : ".");
        }
        char *r = run_capture(cmd);
        if (r && r[0]) return r;
        free(r);
    }
    /* Fallback: simple text file path picker using printf */
    fprintf(stderr, "No zenity/kdialog found. Enter a file path (or leave empty to cancel): ");
    fflush(stderr);
    char line[4096];
    if (fgets(line, sizeof(line), stdin)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
        if (line[0]) {
            char *r = (char *)malloc(l + 1);
            if (r) memcpy(r, line, l + 1);
            return r;
        }
    }
    return NULL;
}

char *gcl_ide_native_dialog_folder(char *out, size_t outsz) {
    (void)outsz;
    if (has_cmd("zenity")) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --directory 2>/dev/null");
        char *r = run_capture(cmd);
        if (r && r[0]) {
            snprintf(out, outsz, "%s", r);
            free(r);
            return out;
        }
        free(r);
    }
    if (has_cmd("kdialog")) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "kdialog --getexistingdirectory . 2>/dev/null");
        char *r = run_capture(cmd);
        if (r && r[0]) {
            snprintf(out, outsz, "%s", r);
            free(r);
            return out;
        }
        free(r);
    }
    fprintf(stderr, "No zenity/kdialog found. Enter a folder path (or leave empty to cancel): ");
    fflush(stderr);
    char line[4096];
    if (fgets(line, sizeof(line), stdin)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
        if (line[0]) {
            snprintf(out, outsz, "%s", line);
            return out;
        }
    }
    return NULL;
}

#endif
