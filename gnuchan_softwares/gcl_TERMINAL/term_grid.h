/*
 * term_grid.h — the screen as a grid of cells.
 *
 * A terminal is not a stream of text that has been drawn; it is a two
 * dimensional grid of cells, each holding one character and the colours it is
 * drawn in, and the escape sequences a program sends are instructions for
 * changing cells in it. Everything else — scrolling, the cursor, the status
 * line of an editor — is that grid being edited in place.
 *
 * So this is the structure the whole program is built around, and the one
 * that has to be right. A program that draws a box around its output is
 * writing characters into four sides of it and trusting that they line up;
 * a program like btop is doing that for the whole screen, many times a
 * second, and any disagreement about what a cell is shows up immediately as a
 * picture that does not match its own frame.
 *
 * --- what a cell is ---
 *
 * `ch` is a Unicode code point and NOT a byte, because the graphs btop draws
 * are braille patterns — U+2800..U+28FF — and a byte cannot hold one. The
 * same is true of every box-drawing character a modern program uses, and of
 * the two thirds of the world that do not write in ASCII. A terminal that
 * stores bytes is a terminal that draws those as mojibake, and that is the
 * fault this type exists to not have.
 *
 * A cell wider than one column — a CJK ideograph, an emoji — is stored as the
 * character plus a cell marked as its continuation, so the grid stays a
 * rectangle and the cursor arithmetic stays one cell per column.
 *
 * --- why the grid is a line table and not one array ---
 *
 * Scrolling is the single most common edit there is: a program that prints a
 * line does it by moving every other line up, and a full screen of output
 * does it hundreds of times a second. With one flat array that is a memmove
 * of the whole screen per line. With a table of lines it is a rotation of
 * POINTERS — the first line becomes the last and nothing is copied — and the
 * cost of a scroll stops depending on how big the window is. This is the
 * difference between a terminal that keeps up with `yes` and one that does
 * not, and it is why the grid owns its lines rather than owning its bytes.
 */
 
#ifndef GNUCHANTERM_GRID_H
#define GNUCHANTERM_GRID_H

#include <stddef.h>
#include <stdint.h>

/* The most columns and rows the grid will hold. A window larger than this is
   clamped rather than refused: a terminal that will not open on a large screen
   is worse than one that draws into the top-left corner of it, and the ceiling
   is the bound the line table's allocation is written against. */
#define TERM_GRID_MAX_COLS 512
#define TERM_GRID_MAX_ROWS 256

/* How a cell is drawn beyond its two colours. Bits rather than a list of
   values because a cell is any combination of them — bold and underlined is a
   thing a program may ask for — and because the renderer reads them as a set.
 */
enum {
    TERM_ATTR_BOLD      = 1 << 0,
    TERM_ATTR_DIM       = 1 << 1,
    TERM_ATTR_ITALIC    = 1 << 2,
    TERM_ATTR_UNDERLINE = 1 << 3,
    TERM_ATTR_BLINK     = 1 << 4,
    TERM_ATTR_REVERSE   = 1 << 5,
    TERM_ATTR_HIDDEN    = 1 << 6,
    TERM_ATTR_STRIKE    = 1 << 7,
};

/* The value a program reaches for with SGR 39 and 49 — "the normal text
   colour", "the normal background". It is an index and not a pixel because the
   palette is the session's and not the program's — see term_style.h — and a
   program that wants the terminal's own colour must get whatever the theme
   says that is. */
#define TERM_COLOR_DEFAULT (-1)

/* Where the theme's own text and background live in the palette.
 *
 * THEY SIT PAST THE 256 A PROGRAM CAN NAME, and that is not tidiness — it is
 * the fix for a collision that made the 256-colour form wrong. The palette is
 * laid out the way every terminal lays one out: 0-15 are the sixteen a program
 * names by number, 16-231 are the 6x6x6 colour cube the `38;5;N` form reaches
 * into, and 232-255 are the greyscale ramp.
 *
 * These two used to be 16 and 17. Index 16 is ALSO the first entry of the
 * cube — the cube's black — so a program that asked for colour 16 was handed
 * the theme's text colour instead, and every colour from 18 up had no entry at
 * all and fell back to the foreground or the background. A program using the
 * 256-colour form was drawn in the wrong colours, and the ones it could name
 * correctly stopped at 15.
 *
 * Moving them past the 256 removes the collision by construction: every index
 * a program can name means exactly what it named, and the theme's own two are
 * not in that range for anything to collide with. */
#define TERM_COLOR_INDEX_FG 256
#define TERM_COLOR_INDEX_BG 257

/* A cell whose colour was sent as 0xRRGGBB rather than named. The program's
   colour is in truecolor_fg / truecolor_bg and the index fields hold this
   marker, which is what tells the renderer to use the program's own colour
   instead of looking an index up in the palette.

   It is a sentinel and not a separate "is truecolor" flag because the two
   would be able to disagree, and a cell that read differently to two readers
   would be drawn differently by them. */
#define TERM_COLOR_TRUECOLOR (-2)

/* The most colours a palette has: the 256 a program names — the sixteen, the
   6x6x6 cube, the greyscale ramp — plus the theme's own text and background at
   TERM_COLOR_INDEX_FG and _BG. The two are past the 256 because those indices
   are the program's and a theme's colour must not be reachable by one. */
#define TERM_PALETTE_SIZE 258

/* One column of the screen. */
typedef struct TermCell {
    uint32_t ch;      /* the code point; 0 is an empty cell               */
    int32_t  fg;      /* a palette index, TERM_COLOR_DEFAULT or _TRUECOLOR*/
    int32_t  bg;
    uint32_t truecolor_fg;   /* the 0xRRGGBB a program sent, when it did  */
    uint32_t truecolor_bg;
    uint16_t attrs;   /* TERM_ATTR_* bits                                 */
    uint8_t  wide;    /* 1 on the first column of a wide character        */
    uint8_t  wide_cont; /* 1 on the second, which holds no character     */
    uint8_t  dirty;   /* 1 while this cell has not been drawn             */
} TermCell;

/* One row, and the flag that goes with it.

   `dirty` is what makes the terminal cheap on a slow GPU: redrawing the whole
   screen every frame costs one Xft draw per cell of the window, and on an
   Intel GMA 965 that is most of a frame budget for a picture that mostly has
   not changed. A row is marked when a cell in it is written, and the renderer
   walks the marked rows and the marked cells inside them. */
typedef struct TermLine {
    TermCell *cells;
    uint8_t   dirty;
} TermLine;

/* --- the palette ----------------------------------------------------------
 *
 * The colours a program names by number, plus the theme's own two. The struct
 * exists so the renderer has one place to look a colour up and the theme has
 * one place to put it, and so a cell only has to carry an index rather than a
 * colour.
 */
typedef struct TermPalette {
    uint32_t colors[TERM_PALETTE_SIZE];
    int      count;    /* how many are actually set; the rest are black    */
} TermPalette;

/* --- the grid -------------------------------------------------------------
 *
 * cols and rows are what the WINDOW is, and they are the numbers everything
 * else is written against: the scroll region, the cursor, and the width the
 * program is told when it asks.
 *
 * `scroll_top` and `scroll_bottom` are the region DECSTBM sets, and they are
 * not the same thing as the window: a program that keeps a status line at the
 * bottom sets the region to everything above it, and then every scroll — and
 * every new line — moves text inside that region and leaves the status line
 * where it is. A terminal without a scroll region cannot run an editor.
 */
typedef struct TermGrid {
    TermLine *lines;
    int       rows;
    int       cols;

    int       scroll_top;      /* inclusive, 0 based                          */
    int       scroll_bottom;   /* inclusive                                   */

    int       cursor_x;        /* 0 based column the next character goes to   */
    int       cursor_y;
    /* Whether the cursor is drawn is NOT here. It is the terminal's and not a
       screen's — there is one cursor and hiding it hides it, whatever screen
       is showing — so it lives on TermVt. See term_vt.h. */

    /* Where the cursor was before the current sequence saved it — ESC 7 and
       ESC 8, and the `s` / `u` pair. One slot is enough: the sequences are a
       pair, and a program that nests them is a program that is guessing. */
    int       saved_x;
    int       saved_y;

    /* The colours and attributes the next written character gets. Carried here
       rather than passed to every writing function because they are the state
       of the terminal, not an argument to a call: a program sends SGR once
       and then writes a hundred characters under it. */
    int32_t   pen_fg;
    int32_t   pen_bg;
    uint32_t  pen_truecolor_fg;
    uint32_t  pen_truecolor_bg;
    uint32_t  pen_attrs;

    /* Whether a character written in the last column wraps to the next line.
       It is DECAWM (`ESC [ ? 7 h` / `l`), and every terminal starts with it
       SET — a shell relies on it — while a program that draws a line which
       must not run on clears it. See term_grid_put(). */
    int       autowrap;

    /* What the cursor is drawn as, from DECSCUSR. A block cursor and a bar
       cursor are different pictures over the same cell. */
    int       cursor_style;

    TermPalette palette;

    /* Set when the window has been resized and the table has not been rebuilt
       yet. The PTY has to be told the new size before the program draws again,
       so the two are not done in the same place: the grid settles itself and
       the module that owns the PTY sends the signal — see term_pty.h. */
    int       resized;

    /* --- the lines above, and where the view sits in them ---
     *
     * The grid does NOT own the history and does not know what it is: it knows
     * that when a line leaves the top of the screen, somebody may want to keep
     * it, and that a reader may want a remembered line back. That is one
     * pointer to a small table of calls — the same shape the escape parser
     * uses for its answers, and for the same reason.
     *
     * Keeping the ring inside the grid instead would give every grid four
     * thousand line records, including the alternate screen, which by
     * definition has no history at all: a full-screen program's screen is not
     * something to scroll back through. NULL here means exactly that.
     *
     * `view_offset` is how far the view is scrolled back. It is SET by whoever
     * owns the history — the history knows it and the grid is told — so that
     * the one reader that resolves a screen row into cells
     * (term_grid_row_cells) can do so without reaching for anything. */
    const struct TermGridHistory *history;
    int view_offset;
} TermGrid;

/* What the grid asks of whoever holds the lines above it.
 *
 * `push` is handed the CELLS of a line that is leaving the top, and takes
 * ownership of them: the grid never touches that buffer again. `clear` is
 * called when the screen is reset in the sense that means the history is no
 * longer above anything — ESC [ 3 J, and a terminal reset.
 *
 * `line` resolves an ABSOLUTE number into cells, filling in their width,
 * because a line remembered before a resize is a different length from
 * today's. The grid answers for the screen itself by subtracting `count`.
 *
 * Every one of these may be a pointer to a terminal's own state; the grid
 * never looks at `user`. */
typedef struct TermGridHistory {
    void *user;
    int (*push)(void *user, TermCell *cells, int cols);
    void (*clear)(void *user);
    int (*count)(void *user);
    const TermCell *(*line)(void *user, int index, int *cols);
} TermGridHistory;

/* --- life ----------------------------------------------------------------- */

/* Make the grid and set it to `cols` by `rows`. Returns 0 on success. The
   lines come back empty and the cursor at the top left. */
int  term_grid_init(TermGrid *grid, int cols, int rows);

void term_grid_free(TermGrid *grid);

/* Reshape the grid to a new size. Lines that no longer fit are dropped and new
   ones are added empty at the bottom; the cursor is pulled inside the new
   size. This is what a window resize does, and it is deliberately lossy — the
   history a program wants to keep is the program's own business, and no
   terminal can know how it wanted it reflowed. */
int  term_grid_resize(TermGrid *grid, int cols, int rows);

/* --- writing -------------------------------------------------------------- */

/* Erase a cell: the character is gone, the colours it would be written in stay
   the pen's. This is what a space at the end of a line looks like, and what
   ECH (erase character) does. */
void term_grid_clear_cell(TermGrid *grid, int x, int y);

/* Put `ch` at the cursor and move the cursor one column on, wrapping to the
   next line at the right edge. The character is drawn in the current pen.
 *
 * A wide character occupies two columns: the cell holds the character and the
   one after it is marked as its continuation. When the cursor is in the last
   column there is no room for the second half, so the character goes on the
   next line, which is what makes a program that prints CJK at the edge of the
   screen wrap rather than draw half a glyph. */
void term_grid_put(TermGrid *grid, uint32_t ch);

/* Move the cursor. The position is CLAMPED to the grid and not refused: the
   sequences a program sends move a cursor that may already be anywhere, and a
   sequence that would put it outside must leave it at the edge rather than
   make the grid inconsistent. */
void term_grid_move_to(TermGrid *grid, int x, int y);
void term_grid_move_by(TermGrid *grid, int dx, int dy);
void term_grid_cursor_next_line(TermGrid *grid);
void term_grid_carriage_return(TermGrid *grid);
void term_grid_backspace(TermGrid *grid);

/* --- scrolling ------------------------------------------------------------ */

/* Move the lines inside the scroll region up by `count`, filling the bottom of
   the region with empty lines; a negative count moves them down. The rows
   outside the region do not move at all — that is the point of having one.

   The move is a rotation of line records: nothing inside a line is copied, and
   the cost is the same whatever the window is. */
void term_grid_scroll(TermGrid *grid, int count);

/* --- erasing -------------------------------------------------------------- */

/* Erase from a point to the end of the line, or from the start of the line to
   it, or the whole line — the three modes of EL. The pen's background fills
   the cells because an erase paints a background as well as removing text: a
   program that clears a line inside a coloured box would otherwise punch a
   hole in it. */
void term_grid_erase_line_to_end(TermGrid *grid, int x, int y);
void term_grid_erase_line_to_start(TermGrid *grid, int x, int y);
void term_grid_erase_line(TermGrid *grid, int y);

/* The same three over rows: from a point to the bottom of the screen, down the
   screen, and the whole visible screen (ED). */
void term_grid_erase_to_end(TermGrid *grid, int x, int y);
void term_grid_erase_to_start(TermGrid *grid, int x, int y);
void term_grid_erase_screen(TermGrid *grid);

/* Drop everything on the screen and put the cursor at the top left. This is
   the state DECSTR and RIS leave behind. The pen is NOT touched: DECSTR clears
   the screen without changing the colours a program is in the middle of
   using, and RIS resets the pen itself before calling this. */
void term_grid_reset(TermGrid *grid);

/* --- the lines above ------------------------------------------------------ */

/* Give the grid somewhere to hand the lines that leave the top of the screen,
   and somewhere to ask about the view. NULL is the alternate screen: a
   full-screen program has no history because it never scrolls anything off
   the top of the world. */
void term_grid_attach_history(TermGrid *grid, const TermGridHistory *history);

/* How far the view is scrolled back, in lines. Set by the owner of the
   history; every reader here is resolved against it. */
void term_grid_set_view_offset(TermGrid *grid, int offset);
int  term_grid_view_offset(const TermGrid *grid);

/* The cells to DRAW at a screen row, and how many of them there are.
 *
 * With the view at the live screen this is the grid's own row and the answer
 * is a pointer into the table. Scrolled back it is a remembered line, or a
 * screen line further down the screen than the row would suggest — which is
 * what makes the picture scroll without moving a single cell: only the top row
 * of the picture changes identity.
 *
 * `cols` is what the answer is good for, and it is NOT always the grid's own
 * width: a line remembered from before a resize is a different length, and a
 * reader that assumed otherwise would read past the end of it. It is set even
 * when the answer is NULL.
 *
 * This is the ONLY reader that knows about the history. Everything else reads
 * the grid's lines as it always did, because everything else is about the
 * screen the program is drawing on, and the program is never scrolled back. */
const TermCell *term_grid_row_cells(const TermGrid *grid, int row, int *cols);

/* The absolute number of the line at a screen row: the history comes first and
   the screen follows, so the screen's first row is the history's count. Used
   by the selection, which is remembered in absolute numbers. */
int term_grid_line_number(const TermGrid *grid, int row);

/* Tell the history that the screen has been cleared, and go back to the live
   screen. What ESC [ 3 J means: the lines above are no longer above anything,
   so nobody can reach them. */
void term_grid_clear_history(TermGrid *grid);

/* --- reading -------------------------------------------------------------- */

/* The cell at a position, or NULL when the position is outside the grid. The
   renderer walks a rectangle that may be larger than the window at an edge, so
   the check is here rather than in every caller. */
const TermCell *term_grid_at(const TermGrid *grid, int x, int y);

/* Whether anything at all has been written since the last clear. The renderer
   asks before it walks the table: a session with nothing happening must not
   cost a screen walk per frame. */
int  term_grid_is_dirty(const TermGrid *grid);

/* Forget every dirty mark, after the renderer has drawn them. */
void term_grid_clear_dirty(TermGrid *grid);

/* --- the palette ---------------------------------------------------------- */

/* Set the palette's colours, the way the theme names them. */
void term_palette_set(TermPalette *palette, const uint32_t *colors, int count);

/* Fill entries 16 to 255 with the standard 256-colour table: the 6x6x6 cube
 * and the greyscale ramp. The sixteen below and the theme's own two past 255
 * are NOT touched — the first are the theme's to choose and the second are not
 * a program's to name.
 *
 * A caller building a palette calls this first and then overwrites the
 * sixteen, so that a program using the `38;5;N` form gets the colour it asked
 * for and the theme gets the sixteen it chose. */
void term_palette_fill_standard(uint32_t *colors);

/* The packed 0xRRGGBB a cell's colour is. `foreground` picks which of the
   cell's two. A truecolor cell returns the colour the program sent; anything
   else is looked up in the palette, and a cell that asked for the default gets
   the theme's. */
uint32_t term_palette_color(const TermGrid *grid, const TermCell *cell,
                            int foreground);

#endif /* GNUCHANTERM_GRID_H */
