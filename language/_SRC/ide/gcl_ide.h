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
    size_t scroll_y;
    size_t scroll_x;
    int dirty;
    char *path;
    GclIdeHistory undo;
    GclIdeHistory redo;
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

#endif /* GCL_IDE_H */
