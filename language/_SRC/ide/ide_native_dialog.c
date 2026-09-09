/*
 * ide_native_dialog.c — Windows native file open/save dialog + folder picker.
 *
 * Windows.h, raylib'in Rectangle/CloseWindow/DrawText isimleriyle çakıştığı için
 * bu dosya AYRI translation unit'tedir. ide_main.c sadece extern prototip kullanır.
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

char *gcl_ide_native_dialog(int save, const char *initial_dir) {
    (void)save;
    (void)initial_dir;
    return NULL;
}

char *gcl_ide_native_dialog_folder(char *out, size_t outsz) {
    (void)out; (void)outsz;
    return NULL;
}

#endif
