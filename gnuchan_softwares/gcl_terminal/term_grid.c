/*
 * term_grid.c — the cell grid, and every edit a program can make to it.
 *
 * See term_grid.h for what the grid is and why it is a table of lines. What is
 * here is the operations, and two of them carry most of the weight:
 *
 *   term_grid_put()     one character at the cursor, which is what almost
 *                       every byte a program sends ends up doing
 *
 *   term_grid_scroll()  the lines inside the scroll region, moved by rotating
 *                       the line records rather than copying any cells
 *
 * A line record OWNS its cell buffer, so rotating the records rotates the
 * buffers with them: nothing is allocated, nothing is freed, and no cell is
 * copied. That is what makes a full-screen redraw of ten thousand cells cheap
 * — the alternative would be a memmove of the whole screen per printed line,
 * which is a terminal that cannot keep up with its own output.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "term_grid.h"

/* --- wide characters -------------------------------------------------------
 *
 * Whether a code point takes two columns. Terminals have to know this to lay
 * out a line at all: a program that prints a Chinese character and then a
 * bracket is expecting the bracket one column after the character, not one
 * column after the first half of it.
 *
 * The ranges are the ones Unicode marks Wide (W) or Fullwidth (F), which is
 * what every terminal uses. It is a table and not a library call on purpose —
 * this runs once per character a program prints, and a terminal is not the
 * place to pay for a locale lookup on every byte of `yes`.
 */
static int cell_is_wide(uint32_t ch) {
    return (ch >= 0x1100 &&
            (ch <= 0x115F ||                       /* Hangul Jamo          */
             (ch >= 0x2E80 && ch <= 0x303E) ||     /* CJK radicals, punct  */
             (ch >= 0x3041 && ch <= 0x33FF) ||     /* Hiragana .. CJK sym  */
             (ch >= 0x3400 && ch <= 0x4DBF) ||     /* CJK ext A            */
             (ch >= 0x4E00 && ch <= 0x9FFF) ||     /* CJK unified          */
             (ch >= 0xA000 && ch <= 0xA4CF) ||     /* Yi                   */
             (ch >= 0xAC00 && ch <= 0xD7A3) ||     /* Hangul syllables     */
             (ch >= 0xF900 && ch <= 0xFAFF) ||     /* CJK compat           */
             (ch >= 0xFE30 && ch <= 0xFE6F) ||     /* CJK compat forms     */
             (ch >= 0xFF00 && ch <= 0xFF60) ||     /* Fullwidth forms      */
             (ch >= 0xFFE0 && ch <= 0xFFE6) ||
             (ch >= 0x1F300 && ch <= 0x1F64F) ||   /* emoji                */
             (ch >= 0x1F900 && ch <= 0x1F9FF) ||
             (ch >= 0x20000 && ch <= 0x3FFFD)));   /* CJK ext B+           */
}

/* --- the pen ---------------------------------------------------------------
 *
 * A cell written now takes the colours and attributes the pen holds. The pen
 * is what an SGR sequence changes and what every later character is drawn
 * with, which is why it lives on the grid and not in the parser: the parser is
 * one of several things that write to the grid, and all of them have to write
 * in the same pen.
 */
static void cell_apply_pen(const TermGrid *grid, TermCell *cell) {
    cell->fg = grid->pen_fg;
    cell->bg = grid->pen_bg;
    cell->truecolor_fg = grid->pen_truecolor_fg;
    cell->truecolor_bg = grid->pen_truecolor_bg;
    cell->attrs = (uint16_t)grid->pen_attrs;
    cell->wide = 0;
    cell->wide_cont = 0;
    cell->dirty = 1;
}

/* The line at a row, or NULL. Rows outside the window are not an error to a
   caller walking a rectangle — see term_grid_at() — so every reader of a row
   goes through here rather than indexing the table directly. */
static TermLine *line_at(TermGrid *grid, int y) {
    if (y < 0 || y >= grid->rows) {
        return NULL;
    }
    return &grid->lines[y];
}

/* One line's cells from `from` to `to`, filled with a blank in `color`, marked
   dirty. The bounds are inclusive and clamped, because an erase request that
   runs past the edge of the window is normal and must not walk off the end.

   The foreground goes back to the default and the attributes are cleared,
   because an erased cell has no text: leaving the old attributes on a blank
   would make a background drawn later think it had to underline nothing. */
static void erase_span(TermGrid *grid, int y, int from, int to,
                       int32_t color) {
    TermLine *line = line_at(grid, y);
    if (line == NULL) {
        return;
    }
    if (from < 0) from = 0;
    if (to >= grid->cols) to = grid->cols - 1;
    for (int x = from; x <= to; x++) {
        TermCell *cell = &line->cells[x];
        cell->ch = 0;
        cell->fg = TERM_COLOR_DEFAULT;
        cell->bg = color;
        cell->truecolor_fg = 0;
        cell->truecolor_bg = 0;
        cell->attrs = 0;
        cell->wide = 0;
        cell->wide_cont = 0;
        cell->dirty = 1;
    }
    line->dirty = 1;
}

/* --- life ----------------------------------------------------------------- */

/* One line's buffer: `cols` cells, all empty, in the default colours. Calloc
   is what makes "empty" mean blank rather than whatever the heap held — an
   uninitialised cell would be drawn as character 0 in colour 0, which is a
   black box on the screen and not a blank. */
static TermCell *line_alloc(int cols) {
    TermCell *cells = (TermCell *)calloc((size_t)cols, sizeof(TermCell));
    if (cells == NULL) {
        return NULL;
    }
    for (int x = 0; x < cols; x++) {
        cells[x].fg = TERM_COLOR_DEFAULT;
        cells[x].bg = TERM_COLOR_DEFAULT;
        cells[x].ch = 0;
    }
    return cells;
}

/* An empty line in the current pen's background: a new line at the bottom of a
   scroll is the colour the program was writing in, which is what makes a
   coloured box survive scrolling. */
static void line_blank(TermGrid *grid, int y) {
    erase_span(grid, y, 0, grid->cols - 1, grid->pen_bg);
}

void term_grid_reset(TermGrid *grid) {
    for (int y = 0; y < grid->rows; y++) {
        line_blank(grid, y);
    }
    grid->cursor_x = 0;
    grid->cursor_y = 0;
    grid->cursor_visible = 1;
    grid->scroll_top = 0;
    grid->scroll_bottom = grid->rows - 1;
    grid->saved_x = 0;
    grid->saved_y = 0;
    /* The pen is NOT reset here. DECSTR clears the screen without changing the
       colours a program is in the middle of using, and RIS resets the pen
       itself before calling this. */
}

int term_grid_init(TermGrid *grid, int cols, int rows) {
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > TERM_GRID_MAX_COLS) cols = TERM_GRID_MAX_COLS;
    if (rows > TERM_GRID_MAX_ROWS) rows = TERM_GRID_MAX_ROWS;

    memset(grid, 0, sizeof(*grid));
    grid->lines = (TermLine *)calloc((size_t)rows, sizeof(TermLine));
    if (grid->lines == NULL) {
        return -1;
    }
    grid->rows = rows;
    grid->cols = cols;

    for (int y = 0; y < rows; y++) {
        grid->lines[y].cells = line_alloc(cols);
        if (grid->lines[y].cells == NULL) {
            term_grid_free(grid);
            return -1;
        }
        grid->lines[y].dirty = 1;
    }

    grid->pen_fg = TERM_COLOR_DEFAULT;
    grid->pen_bg = TERM_COLOR_DEFAULT;
    grid->cursor_style = 0;
    term_grid_reset(grid);
    return 0;
}

void term_grid_free(TermGrid *grid) {
    if (grid->lines != NULL) {
        for (int y = 0; y < grid->rows; y++) {
            free(grid->lines[y].cells);
        }
        free(grid->lines);
    }
    grid->lines = NULL;
    grid->rows = 0;
    grid->cols = 0;
}

/* Give a line a buffer of a new width, keeping as much of what was on it as
   still fits. The old buffer is freed and the new one is blank beyond what was
   copied, which is what a narrower window makes of a long line: the text that
   no longer fits is gone, and the rest is where it was. */
static int line_rewidth(TermGrid *grid, int y, int new_cols) {
    TermCell *fresh = line_alloc(new_cols);
    if (fresh == NULL) {
        return -1;
    }
    TermLine *line = &grid->lines[y];
    int keep = grid->cols < new_cols ? grid->cols : new_cols;
    if (line->cells != NULL) {
        memcpy(fresh, line->cells, (size_t)keep * sizeof(TermCell));
        free(line->cells);
    }
    line->cells = fresh;
    line->dirty = 1;
    return 0;
}

int term_grid_resize(TermGrid *grid, int cols, int rows) {
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > TERM_GRID_MAX_COLS) cols = TERM_GRID_MAX_COLS;
    if (rows > TERM_GRID_MAX_ROWS) rows = TERM_GRID_MAX_ROWS;

    if (cols == grid->cols && rows == grid->rows) {
        return 0;
    }

    if (rows != grid->rows) {
        TermLine *fresh = (TermLine *)realloc(
            grid->lines, (size_t)rows * sizeof(TermLine));
        if (fresh == NULL) {
            return -1;
        }
        grid->lines = fresh;
        if (rows > grid->rows) {
            /* Grown: the new rows at the bottom start as empty lines. They are
               allocated at the OLD width and rewidened below, so there is one
               place that knows how wide a line is. */
            for (int y = grid->rows; y < rows; y++) {
                grid->lines[y].cells = line_alloc(grid->cols);
                if (grid->lines[y].cells == NULL) {
                    return -1;
                }
                grid->lines[y].dirty = 1;
            }
        } else {
            /* Shrunk: the rows that no longer exist have to be freed, or the
               memory goes with the window. */
            for (int y = rows; y < grid->rows; y++) {
                free(grid->lines[y].cells);
                grid->lines[y].cells = NULL;
            }
        }
        grid->rows = rows;
    }

    if (cols != grid->cols) {
        for (int y = 0; y < grid->rows; y++) {
            if (line_rewidth(grid, y, cols) != 0) {
                return -1;
            }
        }
        grid->cols = cols;
    }

    /* The scroll region and the cursor were valid for the old size and may not
       be for the new one. Both are clamped rather than reset: a program that
       set a region and then had its window resized is still in the middle of
       what it was doing, and throwing its state away would be worse than
       squeezing it. */
    if (grid->scroll_bottom >= grid->rows) {
        grid->scroll_bottom = grid->rows - 1;
    }
    if (grid->scroll_top > grid->scroll_bottom) {
        grid->scroll_top = 0;
        grid->scroll_bottom = grid->rows - 1;
    }
    if (grid->cursor_x >= grid->cols) grid->cursor_x = grid->cols - 1;
    if (grid->cursor_y >= grid->rows) grid->cursor_y = grid->rows - 1;
    if (grid->cursor_x < 0) grid->cursor_x = 0;
    if (grid->cursor_y < 0) grid->cursor_y = 0;

    grid->resized = 1;
    return 0;
}

/* --- writing -------------------------------------------------------------- */

void term_grid_clear_cell(TermGrid *grid, int x, int y) {
    erase_span(grid, y, x, x, grid->pen_bg);
}

void term_grid_put(TermGrid *grid, uint32_t ch) {
    int width = cell_is_wide(ch) ? 2 : 1;

    /* A control character is not a cell. Everything below 0x20 and DEL is an
       instruction, and the parser is what acts on them; one arriving here is
       one the parser did not claim, and drawing it would put an invisible box
       on the screen where a program sent a line feed. */
    if (ch < 0x20 || ch == 0x7F) {
        return;
    }

    /* Wrap. The cursor sits one past the last column until the next character
       arrives; that is what lets a program fill the last column and then send
       CR LF without an extra line being scrolled out from under it. */
    if (grid->cursor_x >= grid->cols) {
        grid->cursor_x = 0;
        term_grid_cursor_next_line(grid);
    }
    /* A wide character in the last column has no room for its second half, so
       it starts the next line — the alternative is half a glyph against the
       edge, which is what a program printing CJK would see. */
    if (width == 2 && grid->cursor_x + 1 >= grid->cols) {
        grid->cursor_x = 0;
        term_grid_cursor_next_line(grid);
    }

    TermLine *line = line_at(grid, grid->cursor_y);
    if (line == NULL) {
        return;
    }

    /* Whatever the cursor is about to land on is replaced, including the
       second half of a wide character that was there. Overwriting one half of
       it and leaving the other would leave a continuation cell with no
       character, which the renderer would draw as a hole. */
    TermCell *cell = &line->cells[grid->cursor_x];
    if (cell->wide_cont && grid->cursor_x > 0) {
        TermCell *first = &line->cells[grid->cursor_x - 1];
        first->ch = 0;
        first->wide = 0;
        first->dirty = 1;
    }
    if (cell->wide && grid->cursor_x + 1 < grid->cols) {
        line->cells[grid->cursor_x + 1].wide_cont = 0;
        line->cells[grid->cursor_x + 1].dirty = 1;
    }

    cell_apply_pen(grid, cell);
    cell->ch = ch;
    cell->wide = (uint8_t)(width == 2);
    line->dirty = 1;

    if (width == 2) {
        /* The column after it belongs to the character and holds none of its
           own. The cursor still steps over both, so the next character lands
           after the glyph rather than on top of its second half. */
        TermCell *cont = &line->cells[grid->cursor_x + 1];
        cell_apply_pen(grid, cont);
        cont->ch = 0;
        cont->wide_cont = 1;
    }

    grid->cursor_x += width;
}

/* --- moving --------------------------------------------------------------- */

void term_grid_move_to(TermGrid *grid, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= grid->cols) x = grid->cols - 1;
    if (y >= grid->rows) y = grid->rows - 1;
    grid->cursor_x = x;
    grid->cursor_y = y;
}

void term_grid_move_by(TermGrid *grid, int dx, int dy) {
    /* Clamped and not wrapped: CUD and CUU move within the screen, and a
       program that moves the cursor past the bottom is asking for the edge,
       not for the top. */
    term_grid_move_to(grid, grid->cursor_x + dx, grid->cursor_y + dy);
}

void term_grid_cursor_next_line(TermGrid *grid) {
    if (grid->cursor_y == grid->scroll_bottom) {
        term_grid_scroll(grid, 1);
    } else if (grid->cursor_y < grid->rows - 1) {
        grid->cursor_y++;
    }
}

void term_grid_carriage_return(TermGrid *grid) {
    grid->cursor_x = 0;
}

void term_grid_backspace(TermGrid *grid) {
    if (grid->cursor_x > 0) {
        grid->cursor_x--;
    }
}

/* --- scrolling ------------------------------------------------------------ */

/* Move the first `count` line records of a range to the bottom of it. The
   records move, and the cell buffers move WITH them because a record owns its
   buffer: nothing is allocated and no cell is copied, whatever the size of the
   window. That is the whole reason the grid is a table of records.

   A temporary array holds the records being moved across, and it is sized by
   the region rather than the window: a scroll is never longer than the region
   it happens in. */
static void rotate_lines(TermGrid *grid, int top, int bottom, int count) {
    int span = bottom - top + 1;
    if (count <= 0 || count >= span) {
        return;
    }
    TermLine *moved = (TermLine *)malloc((size_t)count * sizeof(TermLine));
    if (moved == NULL) {
        return;
    }
    memcpy(moved, &grid->lines[top], (size_t)count * sizeof(TermLine));
    memmove(&grid->lines[top], &grid->lines[top + count],
            (size_t)(span - count) * sizeof(TermLine));
    memcpy(&grid->lines[bottom - count + 1], moved,
           (size_t)count * sizeof(TermLine));
    free(moved);
}

void term_grid_scroll(TermGrid *grid, int count) {
    int top = grid->scroll_top;
    int bottom = grid->scroll_bottom;
    if (top < 0 || bottom >= grid->rows || top > bottom || count == 0) {
        return;
    }

    int span = bottom - top + 1;
    /* A scroll longer than the region leaves the region empty, which is what
       moving everything off the top does. */
    if (count >= span || count <= -span) {
        for (int y = top; y <= bottom; y++) {
            line_blank(grid, y);
        }
        return;
    }

    if (count > 0) {
        /* Up: the top records go to the bottom, and the rows they vacated at
           the bottom of the region are the blank ones. */
        rotate_lines(grid, top, bottom, count);
        for (int y = bottom - count + 1; y <= bottom; y++) {
            line_blank(grid, y);
        }
    } else {
        /* Down: the LAST `down` records have to end up at the top, which is
           the same rotation done from the other end — moving the first
           span-down records to the bottom leaves exactly those. */
        int down = -count;
        rotate_lines(grid, top, bottom, span - down);
        for (int y = top; y < top + down; y++) {
            line_blank(grid, y);
        }
    }
}

/* --- erasing -------------------------------------------------------------- */

void term_grid_erase_line_to_end(TermGrid *grid, int x, int y) {
    erase_span(grid, y, x, grid->cols - 1, grid->pen_bg);
}

void term_grid_erase_line_to_start(TermGrid *grid, int x, int y) {
    erase_span(grid, y, 0, x, grid->pen_bg);
}

void term_grid_erase_line(TermGrid *grid, int y) {
    erase_span(grid, y, 0, grid->cols - 1, grid->pen_bg);
}

void term_grid_erase_to_end(TermGrid *grid, int x, int y) {
    term_grid_erase_line_to_end(grid, x, y);
    for (int row = y + 1; row < grid->rows; row++) {
        term_grid_erase_line(grid, row);
    }
}

void term_grid_erase_to_start(TermGrid *grid, int x, int y) {
    term_grid_erase_line_to_start(grid, x, y);
    for (int row = 0; row < y; row++) {
        term_grid_erase_line(grid, row);
    }
}

void term_grid_erase_screen(TermGrid *grid) {
    for (int y = 0; y < grid->rows; y++) {
        term_grid_erase_line(grid, y);
    }
}

/* --- reading -------------------------------------------------------------- */

const TermCell *term_grid_at(const TermGrid *grid, int x, int y) {
    if (x < 0 || x >= grid->cols || y < 0 || y >= grid->rows) {
        return NULL;
    }
    return &grid->lines[y].cells[x];
}

int term_grid_is_dirty(const TermGrid *grid) {
    for (int y = 0; y < grid->rows; y++) {
        if (grid->lines[y].dirty) {
            return 1;
        }
    }
    return 0;
}

void term_grid_clear_dirty(TermGrid *grid) {
    for (int y = 0; y < grid->rows; y++) {
        grid->lines[y].dirty = 0;
        for (int x = 0; x < grid->cols; x++) {
            grid->lines[y].cells[x].dirty = 0;
        }
    }
}

/* --- the palette ---------------------------------------------------------- */

void term_palette_set(TermPalette *palette, const uint32_t *colors, int count) {
    if (count > TERM_PALETTE_SIZE) count = TERM_PALETTE_SIZE;
    if (count < 0) count = 0;
    for (int i = 0; i < count; i++) {
        palette->colors[i] = colors[i];
    }
    palette->count = count;
}

uint32_t term_palette_color(const TermGrid *grid, const TermCell *cell,
                            int foreground) {
    int32_t index = foreground ? cell->fg : cell->bg;

    if (index == TERM_COLOR_TRUECOLOR) {
        return foreground ? cell->truecolor_fg : cell->truecolor_bg;
    }
    /* The default is the theme's own two colours, which the theme is required
       to put at TERM_COLOR_INDEX_FG and _BG. A cell that asked for neither the
       program's colour nor a palette entry gets those, so a program that never
       sends SGR is drawn in the theme and not in black. */
    if (index == TERM_COLOR_DEFAULT) {
        index = foreground ? TERM_COLOR_INDEX_FG : TERM_COLOR_INDEX_BG;
    }
    if (index < 0 || index >= TERM_PALETTE_SIZE ||
        index >= grid->palette.count) {
        return foreground ? 0xFFFFFFFFu : 0x00000000u;
    }
    return grid->palette.colors[index];
}
