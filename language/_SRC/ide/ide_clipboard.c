/* ide_clipboard.c — Windows system clipboard access.
 *
 * windows.h cannot be compiled in the same translation unit as raylib
 * (CloseWindow/ShowCursor/Rectangle collide). That is why the clipboard code
 * is kept in a separate TU; gcl_ide_editor.c only calls this API.
 *
 * Unicode support: CF_TEXT (ANSI) corrupts Turkish characters (ş, ğ, ı...)
 * into '?'. That is why CF_UNICODETEXT is used with UTF-8 ↔ UTF-16 conversion.
 */

#include "gcl_ide_clipboard.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Convert a UTF-16 wchar_t array to a UTF-8 byte array (malloc; caller frees). */
static char *utf16_to_utf8(const wchar_t *src, size_t srclen) {
    if (!src) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, src, (int)srclen, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, src, (int)srclen, out, len, NULL, NULL);
    out[len] = '\0';
    return out;
}

#endif

void gcl_ide_clipboard_set_text(const char *text, size_t len) {
#ifdef _WIN32
    if (!text) return;
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    /* UTF-8 → UTF-16: Turkish characters are preserved (no '?' corruption). */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, (int)len, NULL, 0);
    if (wlen > 0) {
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
        if (hg) {
            wchar_t *dst = (wchar_t *)GlobalLock(hg);
            if (dst) {
                MultiByteToWideChar(CP_UTF8, 0, text, (int)len, dst, wlen);
                dst[wlen] = L'\0';
                GlobalUnlock(hg);
                SetClipboardData(CF_UNICODETEXT, hg);
            } else {
                GlobalFree(hg);
            }
        }
    }
    CloseClipboard();
#endif
}

char *gcl_ide_clipboard_get_text(void) {
#ifdef _WIN32
    if (!OpenClipboard(NULL)) return NULL;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        /* Fallback: CF_TEXT (legacy applications) */
        h = GetClipboardData(CF_TEXT);
        if (!h) { CloseClipboard(); return NULL; }
        const char *data = (const char *)GlobalLock(h);
        if (!data) { CloseClipboard(); return NULL; }
        size_t len = strlen(data);
        char *out = (char *)malloc(len + 1);
        if (out) memcpy(out, data, len + 1);
        GlobalUnlock(h);
        CloseClipboard();
        return out;
    }
    const wchar_t *data = (const wchar_t *)GlobalLock(h);
    if (!data) { CloseClipboard(); return NULL; }
    size_t wlen = wcslen(data);
    char *out = utf16_to_utf8(data, wlen);
    GlobalUnlock(h);
    CloseClipboard();
    return out;
#else
    return NULL;
#endif
}
