#include "gcl_ide.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- UTF-8 yardımcıları ----------
   GCL IDE metni UTF-8 olarak saklar. Türkçe karakterler (ğ, ş, ı, ç, ö, ü) ve
   diğer birçok sembol 2-4 bayt yer kaplar. Bayt bazlı imleç hareketi bu karakterlerin
   ortasında durur ve "cursor sağa kayıyor" hissi verir. Bu yardımcılar imleci her zaman
   karakter sınırına taşır; silme/backspace de tam karakteri siler.
*/
static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* pos öncesindeki karakterin başlangıç offset'ini bul (en fazla 4 bayt geriye bak) */
static size_t utf8_char_start(const GclIdeBuffer *b, size_t pos) {
    if (pos > b->size) pos = b->size;
    size_t start = pos;
    int back = 0;
    while (start > 0 && back < 4) {
        start--;
        back++;
        if ((b->content[start] & 0xC0) != 0x80) break; /* leading byte bulundu */
    }
    return start;
}

/* pos'taki karakterin bayt uzunluğu (buffer sınırına dikkat) */
static size_t utf8_char_len_at(const GclIdeBuffer *b, size_t pos) {
    if (pos >= b->size) return 1;
    int len = utf8_char_len((unsigned char)b->content[pos]);
    if (pos + (size_t)len > b->size) len = (int)(b->size - pos);
    return (size_t)len;
}

static size_t utf8_char_count(const char *s, size_t len) {
    size_t count = 0;
    for (size_t i = 0; i < len; ) {
        int l = utf8_char_len((unsigned char)s[i]);
        i += (size_t)l;
        count++;
    }
    return count;
}

static size_t utf8_pos_after_chars(const char *s, size_t len, size_t chars) {
    size_t i = 0, c = 0;
    while (i < len && c < chars) {
        int l = utf8_char_len((unsigned char)s[i]);
        i += (size_t)l;
        c++;
    }
    return i;
}

static char *gcl_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

/* ---------- history (undo/redo) ---------- */

#define GCL_UNDO_LIMIT 256

static void history_free(GclIdeHistory *h) {
    if (!h) return;
    for (int i = 0; i < h->count; i++) free(h->items[i].content);
    free(h->items);
    h->items = NULL;
    h->count = 0;
    h->cap = 0;
}

static void history_push(GclIdeHistory *h, const char *content, size_t size, size_t cursor) {
    if (h->count >= GCL_UNDO_LIMIT) {
        /* en eski snapshot'ı sil */
        free(h->items[0].content);
        memmove(&h->items[0], &h->items[1], (size_t)(h->count - 1) * sizeof(GclIdeSnapshot));
        h->count--;
    }
    if (h->count >= h->cap) {
        int nc = h->cap == 0 ? 32 : h->cap * 2;
        GclIdeSnapshot *n = (GclIdeSnapshot *)realloc(h->items, sizeof(GclIdeSnapshot) * nc);
        if (!n) return;
        h->items = n;
        h->cap = nc;
    }
    char *copy = (char *)malloc(size + 1);
    if (!copy) return;
    if (size > 0) memcpy(copy, content, size);
    copy[size] = '\0';
    h->items[h->count].content = copy;
    h->items[h->count].size = size;
    h->items[h->count].cursor = cursor;
    h->count++;
}

/* Mutasyondan ÖNCE çağrılır: mevcut durumu undo'ya it, redo'yu temizle. */
static void buffer_snapshot_before(GclIdeBuffer *b) {
    history_push(&b->undo, b->content ? b->content : "", b->size, b->cursor);
    history_free(&b->redo);
}

static int ensure_cap(GclIdeBuffer *b, size_t extra) {
    size_t needed = b->size + extra;
    if (needed <= b->cap) return 0;
    size_t nc = b->cap == 0 ? 256 : b->cap * 2;
    while (nc < needed) nc *= 2;
    char *n = (char *)realloc(b->content, nc);
    if (!n) return -1;
    b->content = n;
    b->cap = nc;
    return 0;
}

void gcl_ide_buffer_init(GclIdeBuffer *b) {
    memset(b, 0, sizeof(*b));
    b->cursor = 0;
    b->scroll_y = 0;
    b->scroll_x = 0;
    b->dirty = 0;
}

void gcl_ide_buffer_free(GclIdeBuffer *b) {
    free(b->path);
    free(b->content);
    b->path = NULL;
    b->content = NULL;
    b->size = 0;
    b->cap = 0;
    b->cursor = 0;
    b->scroll_y = 0;
    b->dirty = 0;
    history_free(&b->undo);
    history_free(&b->redo);
}

int gcl_ide_buffer_load(GclIdeBuffer *b, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    /* +1: null terminator için gereken alan — heap overflow önlenir */
    if (ensure_cap(b, (size_t)sz + 1) != 0) { fclose(f); return -1; }
    size_t rd = fread(b->content, 1, (size_t)sz, f);
    fclose(f);
    b->content[rd] = '\0';
    b->size = rd;
    b->cursor = 0;
    b->scroll_y = 0;
    b->scroll_x = 0;
    b->dirty = 0;
    free(b->path);
    b->path = gcl_strdup(path);
    /* \r\n -> \n normalize */
    char *out = b->content;
    size_t o = 0;
    for (size_t i = 0; i < b->size; i++) {
        if (b->content[i] == '\r') {
            if (i + 1 < b->size && b->content[i + 1] == '\n') {
                if (o == 0 || out[o - 1] != '\n') out[o++] = '\n';
                i++;
            } else {
                out[o++] = '\n';
            }
        } else {
            out[o++] = b->content[i];
        }
    }
    b->size = o;
    if (b->size < b->cap) b->content[b->size] = '\0';
    return 0;
}

int gcl_ide_buffer_save(GclIdeBuffer *b) {
    if (!b->path) return -1;
    FILE *f = fopen(b->path, "wb");
    if (!f) return -1;
    if (b->size > 0) fwrite(b->content, 1, b->size, f);
    fclose(f);
    b->dirty = 0;
    return 0;
}

void gcl_ide_buffer_insert_char(GclIdeBuffer *b, char c) {
    if (ensure_cap(b, 1) != 0) return;
    if (b->cursor > b->size) b->cursor = b->size;
    buffer_snapshot_before(b);
    memmove(b->content + b->cursor + 1, b->content + b->cursor, b->size - b->cursor);
    b->content[b->cursor] = c;
    b->size += 1;
    b->cursor += 1;
    b->dirty = 1;
}

/* UTF-8 karakter dizisini imlecin olduğu yere ekle (Türkçe/ok karakterler için).
   ide_editor.c'deki GetCharPressed'ten gelen Unicode codepoint bu fonksiyonla
   tam UTF-8 olarak yazılır; böylece "ğ" gibi 2 baytlı karakterler bozulmaz. */
void gcl_ide_buffer_insert_utf8(GclIdeBuffer *b, const char *utf8) {
    size_t len = utf8 ? strlen(utf8) : 0;
    if (len == 0) return;
    if (ensure_cap(b, len) != 0) return;
    if (b->cursor > b->size) b->cursor = b->size;
    buffer_snapshot_before(b);
    memmove(b->content + b->cursor + len, b->content + b->cursor, b->size - b->cursor);
    memcpy(b->content + b->cursor, utf8, len);
    b->size += len;
    b->cursor += len;
    b->dirty = 1;
}

void gcl_ide_buffer_insert_newline(GclIdeBuffer *b) {
    gcl_ide_buffer_insert_char(b, '\n');
}

void gcl_ide_buffer_backspace(GclIdeBuffer *b) {
    if (b->cursor == 0 || b->size == 0) return;
    if (b->cursor > b->size) b->cursor = b->size;
    buffer_snapshot_before(b);
    /* Tam karakteri sil: cursor'ı karakter başlangıcına taşı */
    size_t start = utf8_char_start(b, b->cursor);
    size_t clen = b->cursor - start;
    memmove(b->content + start, b->content + b->cursor, b->size - b->cursor);
    b->size -= clen;
    b->cursor = start;
    b->dirty = 1;
}

void gcl_ide_buffer_delete(GclIdeBuffer *b) {
    if (b->cursor >= b->size) return;
    buffer_snapshot_before(b);
    size_t clen = utf8_char_len_at(b, b->cursor);
    memmove(b->content + b->cursor, b->content + b->cursor + clen, b->size - b->cursor - clen);
    b->size -= clen;
    b->dirty = 1;
}

void gcl_ide_buffer_cursor_left(GclIdeBuffer *b) {
    if (b->cursor == 0) return;
    /* Önceki karakterin başlangıcına git (UTF-8 güvenli) */
    size_t start = utf8_char_start(b, b->cursor);
    b->cursor = start;
}

void gcl_ide_buffer_cursor_right(GclIdeBuffer *b) {
    if (b->cursor >= b->size) return;
    size_t clen = utf8_char_len_at(b, b->cursor);
    b->cursor += clen;
}

static size_t line_start_index(const GclIdeBuffer *b, size_t pos) {
    if (pos > b->size) pos = b->size;
    while (pos > 0 && b->content[pos - 1] != '\n') pos--;
    return pos;
}

static size_t line_end_index(const GclIdeBuffer *b, size_t pos) {
    if (pos > b->size) pos = b->size;
    while (pos < b->size && b->content[pos] != '\n') pos++;
    return pos;
}

void gcl_ide_buffer_cursor_line_start(GclIdeBuffer *b) {
    if (!b->content) return;
    b->cursor = line_start_index(b, b->cursor);
}

void gcl_ide_buffer_cursor_line_end(GclIdeBuffer *b) {
    if (!b->content) return;
    b->cursor = line_end_index(b, b->cursor);
}

size_t gcl_ide_buffer_line_of_cursor(const GclIdeBuffer *b) {
    if (!b->content) return 0;
    size_t line = 0;
    for (size_t i = 0; i < b->cursor && i < b->size; i++) {
        if (b->content[i] == '\n') line++;
    }
    return line;
}

size_t gcl_ide_buffer_cursor_in_line(const GclIdeBuffer *b) {
    if (!b->content) return 0;
    size_t start = line_start_index(b, b->cursor);
    return b->cursor - start;
}

/* Satır içi kolonu UTF-8 KARAKTER sayısı olarak döndür — görsel imleç/scroll için.
   İmleç bayt offset'i karakter başlangıcında olduğundan, satır başından imlece kadarki
   metindeki karakterleri sayarak görsel konumu MeasureText ile eşleştirir. */
size_t gcl_ide_buffer_cursor_in_line_chars(const GclIdeBuffer *b) {
    if (!b->content) return 0;
    size_t start = line_start_index(b, b->cursor);
    size_t len = b->cursor - start;
    return utf8_char_count(&b->content[start], len);
}

void gcl_ide_buffer_cursor_up(GclIdeBuffer *b) {
    if (!b->content) return;
    size_t line = gcl_ide_buffer_line_of_cursor(b);
    if (line == 0) return;
    size_t col_bytes = gcl_ide_buffer_cursor_in_line(b);
    /* önceki satırın başını bul */
    size_t prev_end = line_start_index(b, b->cursor);
    if (prev_end > 0 && b->content[prev_end - 1] == '\n') prev_end -= 1;
    size_t prev_start = line_start_index(b, prev_end);
    size_t prev_len = prev_end - prev_start;
    /* Hedef kolonu KARAKTER sayısı olarak tut ve UTF-8 güvenli ilerle */
    size_t col_chars = utf8_char_count(&b->content[prev_start], col_bytes < prev_len ? col_bytes : prev_len);
    b->cursor = prev_start + utf8_pos_after_chars(&b->content[prev_start], prev_len, col_chars);
}

void gcl_ide_buffer_cursor_down(GclIdeBuffer *b) {
    if (!b->content) return;
    size_t line = gcl_ide_buffer_line_of_cursor(b);
    size_t total = gcl_ide_buffer_line_count(b);
    if (line + 1 >= total) return;
    size_t col_bytes = gcl_ide_buffer_cursor_in_line(b);
    size_t cur_start = line_start_index(b, b->cursor);
    size_t cur_end = line_end_index(b, cur_start);
    size_t next_start = cur_end < b->size ? cur_end + 1 : cur_end;
    size_t next_end = line_end_index(b, next_start);
    size_t next_len = next_end - next_start;
    /* Hedef kolonu KARAKTER sayısı olarak tut ve UTF-8 güvenli ilerle */
    size_t col_chars = utf8_char_count(&b->content[next_start], col_bytes < next_len ? col_bytes : next_len);
    b->cursor = next_start + utf8_pos_after_chars(&b->content[next_start], next_len, col_chars);
}

const char *gcl_ide_buffer_line_at(const GclIdeBuffer *b, size_t line, size_t *len) {
    if (!b->content) {
        *len = 0;
        return "";
    }
    size_t cur = 0;
    size_t start = 0;
    for (size_t i = 0; i < b->size; i++) {
        if (b->content[i] == '\n') {
            if (cur == line) {
                *len = i - start;
                return &b->content[start];
            }
            cur++;
            start = i + 1;
        }
    }
    if (cur == line) {
        *len = b->size - start;
        return &b->content[start];
    }
    *len = 0;
    return "";
}

size_t gcl_ide_buffer_line_count(const GclIdeBuffer *b) {
    if (!b->content) return 1;
    size_t count = 1;
    for (size_t i = 0; i < b->size; i++) {
        if (b->content[i] == '\n') count++;
    }
    return count;
}

void gcl_ide_buffer_scroll_up(GclIdeBuffer *b) {
    if (b->scroll_y > 0) b->scroll_y -= 1;
}

void gcl_ide_buffer_scroll_down(GclIdeBuffer *b) {
    size_t total = gcl_ide_buffer_line_count(b);
    if (b->scroll_y + 1 < total) b->scroll_y += 1;
}

/* ---------- undo / redo ---------- */

void gcl_ide_buffer_undo(GclIdeBuffer *b) {
    if (b->undo.count == 0) return;
    /* mevcut durumu redo'ya it */
    history_push(&b->redo, b->content ? b->content : "", b->size, b->cursor);
    /* undo'dan snapshot'ı al */
    GclIdeSnapshot *snap = &b->undo.items[b->undo.count - 1];
    free(b->content);
    b->content = snap->content;       /* ownership transfer */
    b->size = snap->size;
    b->cap = b->size + 1;             /* kapasiteyi yeniden ayarla */
    b->cursor = snap->cursor > b->size ? b->size : snap->cursor;
    snap->content = NULL;             /* artık buffer'da */
    b->undo.count--;
    b->dirty = 1;
}

void gcl_ide_buffer_redo(GclIdeBuffer *b) {
    if (b->redo.count == 0) return;
    history_push(&b->undo, b->content ? b->content : "", b->size, b->cursor);
    GclIdeSnapshot *snap = &b->redo.items[b->redo.count - 1];
    free(b->content);
    b->content = snap->content;
    b->size = snap->size;
    b->cap = b->size + 1;
    b->cursor = snap->cursor > b->size ? b->size : snap->cursor;
    snap->content = NULL;
    b->redo.count--;
    b->dirty = 1;
}
