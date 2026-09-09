/* ide_clipboard.c — Windows sistem panosu erişimi.
 *
 * windows.h, raylib ile aynı translation unit'te derlenemez
 * (CloseWindow/ShowCursor/Rectangle çakışır). Bu yüzden pano kodu
 * ayrı bir TU'da tutulur; gcl_ide_editor.c yalnızca bu API'yi çağırır.
 *
 * Unicode desteği: CF_TEXT (ANSI) Türkçe karakterleri (ş, ğ, ı...) '?'
 * olarak bozar. Bu yüzden UTF-8 ↔ UTF-16 dönüşümüyle CF_UNICODETEXT kullanılır.
 */

#include "gcl_ide_clipboard.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* UTF-16 wchar_t dizisini UTF-8 byte dizisine çevir (malloc; çağıran free eder). */
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
    /* UTF-8 → UTF-16: Türkçe karakterler korunur ('?' bozulması yok). */
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
        /* Fallback: CF_TEXT (eski uygulamalar) */
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
