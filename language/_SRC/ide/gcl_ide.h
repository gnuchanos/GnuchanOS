/*
 * gcl_ide.h — GCL IDE public tip ve buffer API.
 */

#ifndef GCL_IDE_H
#define GCL_IDE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *content;
    size_t size;
    size_t cursor;
} GclIdeSnapshot;

typedef struct {
    GclIdeSnapshot *items;
    int count;
    int cap;
} GclIdeHistory;

typedef struct {
    char *content;
    size_t size;
    size_t cap;
    size_t cursor;
    /* Secim capasi (anchor) - imlec ile birlikte secimi tanimlar. Eskiden
       Editor icinde tek bir alandi; bu yuzden sekmeler arasi siziyor ve
       undo/redo capayi guncellemediginde hayalet secim olusup Delete yanlis
       metni siliyordu (todo #6). Artik her buffer kendi capasini tutar. */
    size_t sel_anchor;
    size_t scroll_y;
    size_t scroll_x;
    int dirty;
    char *path;
    GclIdeHistory undo;
    GclIdeHistory redo;
    /* Duzenleme grubu (begin_edit..end_edit) durumu: ic ice gruplar sayilir
       (undo_suspend) ve grup icindeki ilk gercek degisiklikte yalnizca BIR
       kez snapshot alinir (undo_armed). Cok adimli islemler (secimi sil+yaz,
       yapistir, otomatik girinti) tek Ctrl+Z ile geri alinir (todo #6). */
    int undo_suspend;
    int undo_armed;
} GclIdeBuffer;

/* buffer API */
void gcl_ide_buffer_init(GclIdeBuffer *b);
void gcl_ide_buffer_free(GclIdeBuffer *b);
int gcl_ide_buffer_load(GclIdeBuffer *b, const char *path);
int gcl_ide_buffer_save(GclIdeBuffer *b);
void gcl_ide_buffer_insert_char(GclIdeBuffer *b, char c);
void gcl_ide_buffer_insert_utf8(GclIdeBuffer *b, const char *utf8);
void gcl_ide_buffer_insert_newline(GclIdeBuffer *b);
void gcl_ide_buffer_backspace(GclIdeBuffer *b);
void gcl_ide_buffer_delete(GclIdeBuffer *b);
void gcl_ide_buffer_cursor_left(GclIdeBuffer *b);
void gcl_ide_buffer_cursor_right(GclIdeBuffer *b);
void gcl_ide_buffer_cursor_line_start(GclIdeBuffer *b);
void gcl_ide_buffer_cursor_line_end(GclIdeBuffer *b);
size_t gcl_ide_buffer_line_of_cursor(const GclIdeBuffer *b);
size_t gcl_ide_buffer_cursor_in_line(const GclIdeBuffer *b);
size_t gcl_ide_buffer_cursor_in_line_chars(const GclIdeBuffer *b);
void gcl_ide_buffer_cursor_up(GclIdeBuffer *b);
void gcl_ide_buffer_cursor_down(GclIdeBuffer *b);
const char *gcl_ide_buffer_line_at(const GclIdeBuffer *b, size_t line, size_t *len);
size_t gcl_ide_buffer_line_count(const GclIdeBuffer *b);
void gcl_ide_buffer_scroll_up(GclIdeBuffer *b);
void gcl_ide_buffer_scroll_down(GclIdeBuffer *b);
void gcl_ide_buffer_undo(GclIdeBuffer *b);
void gcl_ide_buffer_redo(GclIdeBuffer *b);
/* Mutasyondan ONCE mevcut durumu undo'ya iter (delete_range gibi alt
   seviye degisiklikleri de Ctrl+Z ile geri alinabilsin diye). */
void gcl_ide_buffer_snapshot(GclIdeBuffer *b);
/* Duzenleme grubu: begin...end arasindaki TUM mutasyonlar TEK undo adimi
   olur. begin_edit ilk gercek degisiklikte (arm) bir kez snapshot alir;
   grup icindeki insert/delete cagrilari ek snapshot almaz. Ic ice gruplar
   desteklenir (sayac). Boylece secim uzerine yazma, yapistirma ve otomatik
   girinti tek Ctrl+Z ile geri alinir (todo #6). */
void gcl_ide_buffer_begin_edit(GclIdeBuffer *b);
void gcl_ide_buffer_end_edit(GclIdeBuffer *b);

#endif /* GCL_IDE_H */
