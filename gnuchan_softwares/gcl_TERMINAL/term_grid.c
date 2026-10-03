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
    grid->scroll_top = 0;
    grid->scroll_bottom = grid->rows - 1;
    grid->saved_x = 0;
    grid->saved_y = 0;
    /* There is no cursor flag here to leave alone. Whether the cursor is drawn
       is the TERMINAL's and not a screen's — one cursor, hidden or not, is
       hidden whatever screen is showing — so it lives on TermVt. See the
       comment there for the fault that made it so. */
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
    /* DECAWM starts SET, which is what every terminal does and what a shell
       depends on: a prompt that reaches the last column has to run on to the
       next line, not overwrite itself. A program that wants it off says so. */
    grid->autowrap = 1;
    /* No cursor flag is set here either; a grid is not what has a cursor. */
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
        /* The rows that are going away are freed FIRST, and this order is not
           a style choice.
         *
         * The obvious way is to realloc the table to the new (smaller) size and
           then free the tail — and it is a heap overflow: after the realloc the
           entries at index `rows` and above are OUTSIDE the allocation, so
           reading them to find the buffers to free reads freed memory, and the
           `free()` that follows is handed a pointer that was never a buffer.
           Maximising the window shrinks it from 24 rows to... whichever the
           window is, which is exactly the case that walks off the end, and the
           result is a segmentation fault on the way to a full screen.
         *
         * Freeing while the table is still whole is the fix, and it costs
           nothing: the same rows are freed either way. */
        if (rows < grid->rows) {
            for (int y = rows; y < grid->rows; y++) {
                free(grid->lines[y].cells);
                grid->lines[y].cells = NULL;
            }
        }

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
        }
        /* The shrunk case freed its rows above, before the table was
           reallocated — see the comment there. */
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

    /* A grid one column wide cannot hold a two-column glyph: the continuation
       cell would be written at column 1, which does not exist. The glyph is
       drawn as a single column instead — wrong as a picture and right as
       memory, which is the only answer that does not write past the end of the
       line buffer. This is the one place a narrow grid reaches it. */
    if (width == 2 && grid->cols < 2) {
        width = 1;
    }

    /* Wrap. The cursor sits one past the last column until the next character
       arrives; that is what lets a program fill the last column and then send
       CR LF without an extra line being scrolled out from under it.
     *
     * AUTOWRAP is DECAWM, which a program turns off with `ESC [ ? 7 l` when it
     * draws a line that must not run on — a progress bar's last cell, a box
     * whose right edge is the screen's. With it OFF the cursor stops at the
     * last column and the character is written over the one there, which is
     * what "the line does not wrap" means. It is ON unless a program says
     * otherwise, because a shell needs it. */
    if (grid->cursor_x >= grid->cols) {
        if (!grid->autowrap) {
            grid->cursor_x = grid->cols - 1;
        } else {
            grid->cursor_x = 0;
            term_grid_cursor_next_line(grid);
        }
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

/* Hand the lines that are leaving the top of the screen to the history, and
   give the grid blank ones to rotate in their place.
 *
 * The blank buffers are allocated BEFORE the old ones are given away, and that
 * order is the whole of the safety here: an allocation that failed after the
 * handover would leave a row with no cells at all, and every draw of it would
 * read a null pointer.
 *
 * A history that answers "I let go of my oldest line" is not acted on here.
   The grid owns no selection and nothing else anchored to an absolute line
   number; whoever does is told by the history itself, inside its own push. */
static void lines_to_history(TermGrid *grid, int first, int count) {
    if (grid->history == NULL || grid->history->push == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        int y = first + i;
        if (y < 0 || y >= grid->rows) {
            break;
        }
        TermCell *fresh = line_alloc(grid->cols);
        if (fresh == NULL) {
            break;
        }
        TermCell *going = grid->lines[y].cells;
        grid->lines[y].cells = fresh;
        grid->lines[y].dirty = 1;
        if (going != NULL) {
            grid->history->push(grid->history->user, going, grid->cols);
        }
    }
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
           the bottom of the region are the blank ones.
         *
         * Before they are rotated away, the lines leaving the TOP OF THE SCREEN
           go to the history — and only when the region begins at the top. A
           program that keeps a status line and scrolls the region below it
           never scrolls anything off the world, and pushing those rows would
           put lines the user can still see into the scrollback. */
        if (top == 0) {
            lines_to_history(grid, top, count);
        }
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

    /* Every row in the region holds NEW CONTENT after a scroll, and every one
       of them is marked to be drawn again.
     *
     * The dirty flags travel with the line records, and that is exactly why
     * this is needed: a row drawn before the scroll is still marked clean
     * after it has been handed the row below's content. The renderer skips
     * it, the window keeps the picture it already had, and a program's new
     * output appears to pile up on the last line while everything above it
     * stays frozen — a shell's prompt printing `top>` over itself instead of
     * scrolling.
     *
     * A full-screen program hides the fault: btop redraws every cell anyway,
     * so the marks never matter. A program that scrolls one line at a time is
     * the case that shows it.
     *
     * Only the REGION is marked, not the screen: a program that keeps a status
     * line outside its scroll region must not have that line redrawn once per
     * printed line — which on a slow display is the difference between a
     * terminal that keeps up with `yes` and one that does not.
     */
    for (int y = top; y <= bottom; y++) {
        grid->lines[y].dirty = 1;
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

/* --- the lines above ------------------------------------------------------ */

void term_grid_attach_history(TermGrid *grid, const TermGridHistory *history) {
    if (grid == NULL) {
        return;
    }
    grid->history = history;
    /* A history that arrives while the view is scrolled back would leave the
       view pointing into a ring it has never seen — a screen becoming the
       alternate one, and the alternate one becoming a screen again. The view
       starts at the live screen, which is where it belongs every time a grid
       becomes a different screen. */
    grid->view_offset = 0;
}

void term_grid_set_view_offset(TermGrid *grid, int offset) {
    if (grid != NULL) {
        grid->view_offset = offset > 0 ? offset : 0;
    }
}

int term_grid_view_offset(const TermGrid *grid) {
    return grid != NULL ? grid->view_offset : 0;
}

int term_grid_line_number(const TermGrid *grid, int row) {
    if (grid == NULL) {
        return row;
    }
    int remembered = (grid->history != NULL && grid->history->count != NULL)
                         ? grid->history->count(grid->history->user)
                         : 0;
    int top = remembered - grid->view_offset;
    if (top < 0) {
        top = 0;
    }
    return top + row;
}

const TermCell *term_grid_row_cells(const TermGrid *grid, int row, int *cols) {
    if (cols != NULL) {
        *cols = 0;
    }
    if (grid == NULL || row < 0 || row >= grid->rows) {
        return NULL;
    }

    /* The live screen: the view is at the bottom, or there is nothing above to
       scroll back through, and the row is the grid's own. */
    if (grid->view_offset <= 0 || grid->history == NULL ||
        grid->history->count == NULL || grid->history->line == NULL) {
        if (cols != NULL) {
            *cols = grid->cols;
        }
        return grid->lines[row].cells;
    }

    int remembered = grid->history->count(grid->history->user);
    int absolute = term_grid_line_number(grid, row);

    if (absolute < remembered) {
        /* A remembered line. Its width is the RING's and not the window's — a
           line kept from before a resize is a different length — so it is
           asked for rather than assumed. */
        return grid->history->line(grid->history->user, absolute, cols);
    }

    /* A line of the screen, pushed down the screen by how far the view is
       scrolled back. */
    int screen_row = absolute - remembered;
    if (screen_row < 0 || screen_row >= grid->rows) {
        return NULL;
    }
    if (cols != NULL) {
        *cols = grid->cols;
    }
    return grid->lines[screen_row].cells;
}

void term_grid_clear_history(TermGrid *grid) {
    if (grid != NULL && grid->history != NULL &&
        grid->history->clear != NULL) {
        grid->history->clear(grid->history->user);
    }
    term_grid_set_view_offset(grid, 0);
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

/* The 6x6x6 colour cube's levels. Every terminal that has a 256-colour palette
   uses these six values and no others: the cube is five steps of each channel
   plus black, and a program that asks for `38;5;196` is asking for the red at
   level 5 of red and 0 of the rest. */
static const uint8_t CUBE_LEVEL[6] = { 0, 95, 135, 175, 215, 255 };

/* Build the 256 colours a program names by number, in the layout every
   terminal uses:
 *
 *   0-15     the sixteen, which the THEME decides — so they are left as they
 *            are here and filled in by the caller from its own palette
 *   16-231   the 6x6x6 cube: 16 + 36*red + 6*green + blue, each 0 to 5
 *   232-255  the greyscale ramp: black to white in twenty-four even steps
 *
 * The cube and the ramp are NOT the theme's and are not configurable: a program
 * that asks for `38;5;196` is asking for a specific red and expects to get it,
 * whatever the theme's sixteen look like. Filling them here is what makes the
 * 256-colour form mean what a program means by it.
 *
 * The two past 255 — the theme's own text and background — are NOT touched;
 * they are the caller's to set, because they are the one pair in the palette
 * that has no standard value. */
void term_palette_fill_standard(uint32_t *colors) {
    if (colors == NULL) {
        return;
    }

    /* The cube. r, g and b each run 0 to 5 in the order the standard gives. */
    for (int index = 16; index <= 231; index++) {
        int value = index - 16;
        int r = value / 36;
        int g = (value / 6) % 6;
        int b = value % 6;
        colors[index] = ((uint32_t)CUBE_LEVEL[r] << 16) |
                        ((uint32_t)CUBE_LEVEL[g] << 8) |
                        (uint32_t)CUBE_LEVEL[b];
    }

    /* The greyscale ramp: 8, 18, 28, ... 238. It starts at 8 rather than 0
       because the cube already holds a black and a white, and the ramp is the
       twenty-four greys BETWEEN them that the cube's six levels are too coarse
       to give. */
    for (int index = 232; index <= 255; index++) {
        uint8_t level = (uint8_t)(8 + (index - 232) * 10);
        colors[index] = ((uint32_t)level << 16) |
                        ((uint32_t)level << 8) |
                        (uint32_t)level;
    }
}

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
    /* The default is the theme's own two colours, which sit PAST the 256 a
       program can name — see TERM_COLOR_INDEX_FG. A cell that asked for
       neither the program's colour nor a palette entry gets those, so a program
       that never sends SGR is drawn in the theme and not in black. */
    if (index == TERM_COLOR_DEFAULT) {
        index = foreground ? TERM_COLOR_INDEX_FG : TERM_COLOR_INDEX_BG;
    }
    if (index < 0 || index >= TERM_PALETTE_SIZE ||
        index >= grid->palette.count) {
        return foreground ? 0xFFFFFFFFu : 0x00000000u;
    }
    return grid->palette.colors[index];
}
