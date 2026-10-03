/*
 * term_render.c — the cell grid drawn as pixels.
 *
 * See term_render.h for why this is a module and why a frame is cheaper than it
 * looks. What is here is the drawing itself, and it has three ideas in it:
 * draw what changed, draw it in runs, and draw the furniture in its own layer.
 *
 * --- why there are TWO pixmaps ---
 *
 * The grid is one surface and nothing else: every cell is drawn at
 * (column * cell_width, row * cell_height) from the top-left corner of the grid,
 * with no offset anywhere. The frame around it — the margin and the bar — is
 * another surface, and the grid is copied onto it at the frame's offset.
 *
 * The obvious alternative is to add the offset to every coordinate in the three
 * passes below, and it is the wrong one: a coordinate that is offset in one
 * place and not another is a picture that is a cell out, and there would be a
 * dozen such places. With two surfaces there is exactly one place that knows
 * where the grid sits, and it is the XCopyArea() in term_render_frame().
 *
 * It also makes the bar cheap. A bar redrawn over the window would have to be
 * drawn before the grid is copied, or the grid would cover it; with the grid on
 * its own surface the order is simply: grid, copy it down, bar on top. That is
 * the order they are visible in.
 *
 * --- what a frame clears, and what it does not ---
 *
 * The temptation is to fill the whole buffer with the background and draw the
 * whole screen, and it is wrong: the cursor blinks while nothing else is
 * happening, and a frame that cleared everything would erase the line the
 * cursor is on and put it back — a screen that flickers once every half second
 * for no reason visible to anyone.
 *
 * So a frame that is not a full redraw clears only the rows it is about to
 * draw, and the row the cursor is on is one of them. A blink therefore costs
 * one row of drawing and not a screen.
 *
 * A FULL redraw is what happens after an expose or a resize, and only then is
 * the whole of both surfaces filled — because after either of those nothing is
 * known about what is on the window.
 *
 * --- a row, in three passes ---
 *
 *   1. backgrounds   adjacent cells asking for the same colour become ONE
 *                    rectangle. A btop graph bar is a run of one colour across
 *                    many cells, so this is one fill per bar instead of one per
 *                    character.
 *
 *   2. text          adjacent cells sharing a face and a colour become ONE Xft
 *                    call, because Xft takes an array of code points.
 *
 *   3. underlines    adjacent underlined cells become one line.
 *
 * Three passes and not one, because a run of text and a run of background do
 * not have the same boundaries — a line of text in one colour sits on a
 * background that changes under it. Two clean passes beat one clever one.
 *
 * --- why a run stops at a wide character ---
 *
 * XftDrawString32() advances by each glyph's own width, which is right for a
 * monospace font where every glyph advances one cell — and wrong for a wide
 * character, whose glyph is twice as wide. So a run stops there and the wide
 * character is drawn on its own. That is the whole reason a run has a boundary
 * condition at all.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term_render.h"
#include "term_render_internal.h"
#include "term_style.h"
#include "term_select.h"
#include "term_suggest.h"

/* The blink of the cursor, in milliseconds. It is the terminfo default and the
   rate every terminal uses; a cursor that blinks at another rate looks broken
   next to the rest of the desktop. */
#define TERM_CURSOR_BLINK_MS 530

/* How far the bar's text sits from the left edge of the window, in cells. One
   is enough to stop it touching the border and is the same margin the grid
   gets, which is what makes the two line up. */
#define TERM_BAR_INSET_CELLS 1

static TermRender *render_of(TermCore *core) {
    return (TermRender *)core->render;
}

/* --- the buffers ---------------------------------------------------------- */

static void render_free_buffer(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL) {
        return;
    }
    /* The Xft drawable points at the grid surface and has to let go before that
       surface is freed, or it is holding a drawable the server has already
       destroyed. */
    if (core->style != NULL && core->style->xft_draw != NULL) {
        XftDrawDestroy(core->style->xft_draw);
        core->style->xft_draw = NULL;
    }
    /* The bar's drawable points at the window surface and goes the same way. */
    if (render->window_draw != NULL) {
        XftDrawDestroy(render->window_draw);
        render->window_draw = NULL;
    }
    if (render->buffer != None) {
        XFreePixmap(core->display, render->buffer);
        render->buffer = None;
    }
    if (render->grid != None) {
        XFreePixmap(core->display, render->grid);
        render->grid = None;
    }
}

int term_render_resize(TermCore *core, int width, int height) {
    TermRender *render = render_of(core);
    if (render == NULL || width < 1 || height < 1) {
        return -1;
    }
    if (render->buffer != None && render->width == width &&
        render->height == height) {
        return 0;
    }

    render_free_buffer(core);

    render->buffer = XCreatePixmap(core->display, core->window,
                                   (unsigned)width, (unsigned)height,
                                   (unsigned)core->depth);
    if (render->buffer == None) {
        return -1;
    }

    /* The grid's own surface is exactly the cells and nothing else, measured
       from the size the core decided the child gets — NOT from the window. If
       it were the window the grid would be clipped by whatever the frame took,
       which is a program losing its last column. */
    render->grid_width = core->cols * core->style->cell_width;
    render->grid_height = core->rows * core->style->cell_height;
    if (render->grid_width < 1) render->grid_width = 1;
    if (render->grid_height < 1) render->grid_height = 1;

    render->grid = XCreatePixmap(core->display, core->window,
                                 (unsigned)render->grid_width,
                                 (unsigned)render->grid_height,
                                 (unsigned)core->depth);
    if (render->grid == None) {
        XFreePixmap(core->display, render->buffer);
        render->buffer = None;
        return -1;
    }

    render->width = width;
    render->height = height;
    render->needs_full = 1;

    /* Xft draws into the GRID surface, because that is where the text goes.
       One Xft drawable per surface, and no way to move an existing one. */
    if (term_style_attach(core->style, render->grid) != 0) {
        return -1;
    }

    /* And the bar needs a second drawable, against the WINDOW surface. Drawing
       the path through the grid's drawable is what put it on row 0 of the
       grid, where the next repaint of that row turned it into garbage. */
    render->window_draw = XftDrawCreate(core->display, render->buffer,
                                        core->style->visual,
                                        core->style->colormap);
    return render->window_draw != NULL ? 0 : -1;
}

int term_render_init(TermCore *core) {
    TermRender *render = (TermRender *)calloc(1, sizeof(TermRender));
    if (render == NULL) {
        return -1;
    }
    render->buffer = None;
    render->grid = None;
    render->cursor_on = 1;
    render->needs_full = 1;
    render->cursor_x = -1;
    render->cursor_y = -1;
    /* The pictures a program places — see term_image.h. An empty list is made
       even when none is ever placed, so the frame's draw call needs no NULL
       check of its own. */
    render->images = term_image_list_new();
    core->render = render;

    if (term_render_resize(core, core->width, core->height) != 0) {
        term_image_list_free(render->images, core->display);
        free(render);
        core->render = NULL;
        return -1;
    }
    return 0;
}

void term_render_free(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL) {
        return;
    }
    render_free_buffer(core);
    /* The pictures' pixmaps are freed with the display, before it closes. */
    term_image_list_free(render->images, core->display);
    render->images = NULL;
    free(render);
    core->render = NULL;
}

/* --- one row -------------------------------------------------------------- */

/* The packed colour a cell's background is drawn in, with the reverse attribute
   applied: a reversed cell's background is its foreground colour.
 *
 * The STYLE is what resolves the colour and not the grid. Every cell used to
 * be resolved through the grid's own palette, which nothing ever filled, so
 * every value fell through to the fallback — black for a background and white
 * for a foreground, whatever the theme said. The screen was a black field with
 * the theme visible only in the bar, and a reversed bar came out white with its
 * text lost in it. See term_style_cell_color(). */
static uint32_t cell_bg(const TermStyle *style, const TermCell *cell) {
    return term_style_cell_color(style, cell,
                                 term_style_is_reversed(cell) ? 1 : 0);
}

static uint32_t cell_fg(const TermStyle *style, const TermCell *cell) {
    return term_style_cell_color(style, cell,
                                 term_style_is_reversed(cell) ? 0 : 1);
}

/* Whether the cell at a screen row and column is inside the selection.
 *
 * The renderer is the only thing that asks, and it asks for every cell it
 * draws: the answer is a comparison or two inside term_select_covers(), and
 * the alternative — a list of selected runs built once per frame — would be
 * rebuilt every time the program prints. */
static int cell_is_selected(TermCore *core, int row, int col) {
    if (core->select == NULL) {
        return 0;
    }
    return term_select_covers((const TermSelect *)core->select,
                              &core->vt.grid, row, col);
}

/* A cell's background, with the SELECTION applied as well as the reverse
   attribute.
 *
 * A selected cell is drawn reversed — its foreground becomes its background —
 * which is what a selection has looked like since there were selections, and
 * is the only highlight that works over a cell that already has a colour: a
 * fixed blue would vanish against a program's own blue.
 *
 * It is folded into the same expression as the reverse attribute because the
 * two are the same operation, and doing it twice — once as an attribute and
 * once as a colour — would cancel out on an already-reversed cell, which is
 * exactly the kind of cell a full-screen program's title bar is made of. */
static uint32_t cell_bg_with(const TermStyle *style, const TermCell *cell,
                             int selected) {
    int reversed = term_style_is_reversed(cell) ^ (selected ? 1 : 0);
    return term_style_cell_color(style, cell, reversed ? 1 : 0);
}

/* Whether a cell's background is the theme's, and so already on the surface. */
static int cell_bg_is_default(const TermStyle *style, const TermCell *cell,
                              int selected) {
    if (selected) {
        /* A selected cell is reversed over whatever it was, so it is never
           already there. */
        return 0;
    }
    if (term_style_is_reversed(cell)) {
        /* A reversed cell paints its foreground as the background whatever the
           colours are, so it is never "already there" either. */
        return 0;
    }
    return cell_bg(style, cell) == term_style_default_bg(style);
}

/* The cells to draw at a screen row, and how many there are.
 *
 * They come from the GRID and not from its line table, and that is the whole
 * of what makes the scrollback drawable: the grid answers with a remembered
 * line when the view is scrolled back, and with one of its own when it is not.
 * Reading lines[y].cells directly — which this did — drew the live screen
 * whatever the view was set to, so scrolling back appeared to do nothing at
 * all.
 *
 * A remembered line can be a different width from the window if the window has
 * been resized since, so the count comes back with the cells rather than being
 * taken from grid->cols. */
static const TermCell *row_cells(const TermGrid *grid, int y, int *cols) {
    return term_grid_row_cells(grid, y, cols);
}

/* Pass 1: the backgrounds that are not the theme's. */
static void render_row_backgrounds(TermCore *core, TermRender *render,
                                   const TermGrid *grid,
                                   const TermCell *cells, int cols, int y) {
    int cell_w = core->style->cell_width;
    int cell_h = core->style->cell_height;

    int x = 0;
    while (x < cols && x < core->cols) {
        const TermCell *cell = &cells[x];
        if (cell->wide_cont) {
            x++;
            continue;
        }
        int selected = cell_is_selected(core, y, x);
        if (cell_bg_is_default(core->style, cell, selected)) {
            x += cell->wide ? 2 : 1;
            continue;
        }

        /* A run of cells asking for the same background is one rectangle. It
           stops at a cell that asks for another, and at a cell whose reverse
           flag differs — a reversed cell's background is not the same KIND of
           colour even when the value matches, and mixing them would make the
           text of one draw over the background of the other. */
        uint32_t bg = cell_bg_with(core->style, cell, selected);
        int reversed = term_style_is_reversed(cell) ^ (selected ? 1 : 0);
        int run_start = x;
        int run_cells = 0;

        while (x < cols && x < core->cols) {
            const TermCell *rc = &cells[x];
            if (rc->wide_cont) {
                break;
            }
            int run_selected = cell_is_selected(core, y, x);
            if ((term_style_is_reversed(rc) ^ (run_selected ? 1 : 0)) !=
                reversed) {
                break;
            }
            if (cell_bg_with(core->style, rc, run_selected) != bg) {
                break;
            }
            run_cells += rc->wide ? 2 : 1;
            x += rc->wide ? 2 : 1;
        }

        if (run_cells > 0) {
            XSetForeground(core->display, core->style->gc, (unsigned long)bg);
            XFillRectangle(core->display, render->grid,
                           core->style->gc,
                           run_start * cell_w, y * cell_h,
                           (unsigned)(run_cells * cell_w),
                           (unsigned)cell_h);
        }
    }
}

/* The face and the colour a run of text is drawn with. A run breaks when either
   changes, and comparing two pointers is the whole test. */
typedef struct RenderPen {
    XftFont  *face;
    XftColor *color;
} RenderPen;

static RenderPen pen_for(TermCore *core, const TermCell *cell, int selected) {
    RenderPen pen;
    pen.face = term_style_face_for(core->style, cell);

    /* The two colours are looked up BY THE FIELD THEY CAME FROM, and only then
       is the reverse applied by choosing between them.
     *
     * Swapping the fields first and looking the result up as "the foreground"
       is what this does not do, and the difference is the whole of a bug that
       made a full-screen program unreadable. TERM_COLOR_DEFAULT is not one
       colour: in the fg field it means the theme's text colour and in the bg
       field it means the theme's background. A reversed cell whose background
       is DEFAULT — which is every cell of nano's title bar, because nano
       reverses the default pair rather than naming two colours — was therefore
       drawn with the theme's TEXT colour as its background and the theme's text
       colour again as the glyph on top: pale grey on near-white, which measures
       as 16,700 pixels of pure white with the words lost inside it.
     *
     * Resolving each field where it lives keeps DEFAULT meaning what the program
     * meant by it, and the reverse stays a choice between two finished
     * colours. */
    /* The SELECTION reverses too, and it has to be folded in here as well as in
       the background pass or the two disagree.
     *
     * The background of a selected cell is painted in its FOREGROUND colour —
       that is what cell_bg_with() does — so a glyph left in the foreground
       colour would be drawn in the same ink as the box behind it and vanish
       completely: a selection of invisible text. Flipping the pen as well puts
       the characters in the cell's background colour, which is what makes a
       selection READ as reversed text. */
    int reversed = term_style_is_reversed(cell) ^ (selected ? 1 : 0);
    XftColor *text = term_style_color(core->style, cell, 1);
    XftColor *behind = term_style_color(core->style, cell, 0);
    pen.color = reversed ? behind : text;
    return pen;
}

static int pen_same(const RenderPen *a, const RenderPen *b) {
    return a->face == b->face && a->color == b->color;
}

/* --- the block elements ---------------------------------------------------
 *
 * A solid block — U+2580 and its family, the characters a picture drawn in
 * text is made of — is drawn as a RECTANGLE and not as a glyph.
 *
 * The font's glyph for a block does not fill the cell: the em box it is drawn
 * in stops a little short of the cell's own height, so a column of half blocks
 * is a column of half blocks with a stripe of the background showing between
 * every pair of rows. Drawn at the size a fetch picture is drawn, that is a
 * picture full of horizontal stripes — and no font size removes it, because the
 * glyph is simply not the height of the cell. The cell IS a rectangle, and a
 * block element is a description of a rectangle inside it, so it is drawn as
 * one: flush with the cell's edges, in the cell's ink, and a column of them is
 * continuous.
 *
 * The shaded blocks (U+2591..U+2593) are left to the font: they are DITHERED,
 * not solid, and a rectangle would draw them wrong the other way.
 */
static int is_block_element(uint32_t ch) {
    if (ch < 0x2580 || ch > 0x2595) {
        return 0;
    }
    if (ch >= 0x2591 && ch <= 0x2593) {
        return 0;   /* the shaded blocks: the font's dither is right */
    }
    return 1;
}

/* Fill one rectangle of a cell on the grid surface. */
static void fill_cell_rect(TermCore *core, int x, int y, int w, int h,
                           uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    TermRender *render = render_of(core);
    XSetForeground(core->display, core->style->gc, (unsigned long)color);
    XFillRectangle(core->display, render->grid, core->style->gc,
                   x, y, (unsigned)(w), (unsigned)(h));
}

/* The ink a block element is drawn in, with the reverse attribute and the
   selection folded in the same way pen_for() folds them. */
static uint32_t block_ink(const TermStyle *style, const TermCell *cell,
                          int selected) {
    int reversed = term_style_is_reversed(cell) ^ (selected ? 1 : 0);
    return term_style_cell_color(style, cell, reversed ? 0 : 1);
}

/* Draw a block element as the rectangle it describes. Answers 1 when it drew
   one — the caller then moves on and never hands the character to the font.
   The background the block sits on has already been painted by the background
   pass, so only the lit part is filled here. */
static int draw_block_element(TermCore *core, const TermCell *cell,
                              int col, int row, int selected) {
    if (!is_block_element(cell->ch)) {
        return 0;
    }

    int cell_w = core->style->cell_width;
    int cell_h = core->style->cell_height;
    int x = col * cell_w;
    int y = row * cell_h;
    uint32_t ink = block_ink(core->style, cell, selected);

    switch (cell->ch) {
    case 0x2580: {                       /* upper half block            */
        fill_cell_rect(core, x, y, cell_w, cell_h / 2, ink);
        break;
    }
    case 0x2590: {                       /* right half block            */
        int w = cell_w / 2;
        fill_cell_rect(core, x + cell_w - w, y, w, cell_h, ink);
        break;
    }
    case 0x2588:                          /* full block                  */
        fill_cell_rect(core, x, y, cell_w, cell_h, ink);
        break;
    case 0x2594: {                       /* upper one eighth block      */
        int h = cell_h / 8;
        if (h < 1) h = 1;
        fill_cell_rect(core, x, y, cell_w, h, ink);
        break;
    }
    case 0x2595: {                       /* right one eighth block      */
        int w = cell_w / 8;
        if (w < 1) w = 1;
        fill_cell_rect(core, x + cell_w - w, y, w, cell_h, ink);
        break;
    }
    default:
        if (cell->ch >= 0x2581 && cell->ch <= 0x2587) {
            /* Lower eighths: U+2581 is one eighth up from the bottom. */
            int eighths = cell->ch - 0x2580;
            int h = (cell_h * eighths) / 8;
            if (h < 1) h = 1;
            fill_cell_rect(core, x, y + cell_h - h, cell_w, h, ink);
        } else if (cell->ch >= 0x2589 && cell->ch <= 0x258F) {
            /* Left eighths: U+2589 is seven eighths and U+258F one. */
            int eighths = 0x2590 - cell->ch;
            int w = (cell_w * eighths) / 8;
            if (w < 1) w = 1;
            fill_cell_rect(core, x, y, w, cell_h, ink);
        } else {
            return 0;
        }
        break;
    }
    return 1;
}

/* Pass 2: the text. */
static void render_row_text(TermCore *core, const TermGrid *grid,
                            const TermCell *cells, int cols, int y) {
    int cell_w = core->style->cell_width;
    int baseline = y * core->style->cell_height + core->style->ascent;

    int x = 0;
    while (x < cols && x < core->cols) {
        const TermCell *cell = &cells[x];

        /* An empty cell draws nothing, and a continuation cell belongs to the
           wide character before it. */
        if (cell->ch == 0 || cell->wide_cont) {
            x++;
            continue;
        }

        int selected = cell_is_selected(core, y, x);

        /* A block element is a rectangle, not a glyph — see above. */
        if (draw_block_element(core, cell, x, y, selected)) {
            x += cell->wide ? 2 : 1;
            continue;
        }

        RenderPen pen = pen_for(core, cell, selected);

        /* A wide character is drawn alone. */
        if (cell->wide) {
            XftDrawString32(core->style->xft_draw, pen.color, pen.face,
                            x * cell_w, baseline, &cell->ch, 1);
            x += 2;
            continue;
        }

        uint32_t codes[TERM_RENDER_MAX_RUN];
        int run_len = 0;
        int run_x = x;

        while (x < cols && x < core->cols &&
               run_len < TERM_RENDER_MAX_RUN) {
            const TermCell *rc = &cells[x];
            if (rc->ch == 0 || rc->wide_cont || rc->wide ||
                is_block_element(rc->ch)) {
                break;
            }
            /* The run breaks where the SELECTION changes as well as where the
               face or the colour does: a run drawn with one pen cannot have
               half of itself reversed, and a selection edge in the middle of a
               line is exactly that. Without this the cells after the cursor's
               column were drawn with the pen the run started with, so a
               selection looked like it covered a whole line whatever part of
               it was dragged. */
            RenderPen rp = pen_for(core, rc, cell_is_selected(core, y, x));
            if (!pen_same(&pen, &rp)) {
                break;
            }
            codes[run_len++] = rc->ch;
            x++;
        }

        if (run_len > 0) {
            XftDrawString32(core->style->xft_draw, pen.color, pen.face,
                            run_x * cell_w, baseline, codes, run_len);
        } else {
            /* Nothing was collected, which can only happen when the loop broke
               on its first cell — a wide or empty cell the checks above did not
               claim. Stepping past it keeps the walk moving. */
            x++;
        }
    }
}

/* Pass 3: the underlines. A contiguous group of underlined cells is one line. */
static void render_row_underlines(TermCore *core, TermRender *render,
                                  const TermGrid *grid,
                                  const TermCell *cells, int cols, int y) {
    int cell_w = core->style->cell_width;
    int cell_h = core->style->cell_height;
    int line_y = y * cell_h + cell_h - 2;

    int x = 0;
    while (x < cols && x < core->cols) {
        const TermCell *cell = &cells[x];
        if (cell->wide_cont || !(cell->attrs & TERM_ATTR_UNDERLINE)) {
            x++;
            continue;
        }

        int run_start = x;
        int run_cells = 0;
        while (x < cols && x < core->cols) {
            const TermCell *rc = &cells[x];
            if (!(rc->attrs & TERM_ATTR_UNDERLINE)) {
                break;
            }
            run_cells += rc->wide ? 2 : 1;
            x += rc->wide ? 2 : 1;
        }

        if (run_cells > 0) {
            XSetForeground(core->display, core->style->gc,
                           (unsigned long)cell_fg(core->style, cell));
            XDrawLine(core->display, render->grid, core->style->gc,
                      run_start * cell_w, line_y,
                      (run_start + run_cells) * cell_w - 1, line_y);
        }
    }
}

/* --- the cursor ----------------------------------------------------------- */

/* The cursor, drawn as a block over the cell it sits on.
 *
 * It is drawn over the finished cell, and the character underneath is drawn
 * again in the background colour so the block does not hide what it sits on —
 * which is what a cursor is for. A terminal that instead swapped the cell's own
 * colours would have to redraw the cell on every blink and could not blink over
 * a cell that had its own background.
 *
 * The caller has already checked that the program wants it drawn: a program
 * that sent `?25l` — btop among them — has turned it off and it stays off. */
static void render_cursor(TermCore *core, TermRender *render,
                          const TermGrid *grid) {
    int cell_w = core->style->cell_width;
    int cell_h = core->style->cell_height;
    int visible = render->cursor_on &&
                  term_vt_cursor_visible(&core->vt) &&
                  grid->cursor_x >= 0 && grid->cursor_x < core->cols &&
                  grid->cursor_y >= 0 && grid->cursor_y < core->rows;

    if (visible) {
        int x = grid->cursor_x * cell_w;
        int y = grid->cursor_y * cell_h;
        unsigned long ink = (unsigned long)term_style_cursor(core->style);

        /* A bar cursor is what a program that asked for one gets with DECSCUSR;
           the block is what every terminal draws otherwise. */
        if (grid->cursor_style == 1) {
            XSetForeground(core->display, core->style->gc, ink);
            XFillRectangle(core->display, render->grid, core->style->gc,
                           x, y, 2, (unsigned)cell_h);
        } else {
            XSetForeground(core->display, core->style->gc, ink);
            XFillRectangle(core->display, render->grid, core->style->gc,
                           x, y, (unsigned)cell_w, (unsigned)cell_h);

            const TermCell *cell =
                term_grid_at(grid, grid->cursor_x, grid->cursor_y);
            if (cell != NULL && cell->ch != 0 && !cell->wide_cont) {
                XftColor *under = term_style_color_rgb(
                    core->style, term_style_default_bg(core->style));
                XftFont *face = term_style_face_for(core->style, cell);
                XftDrawString32(core->style->xft_draw, under, face,
                                x, y + core->style->ascent, &cell->ch, 1);
            }
        }
    }

    /* Where it ended up, and whether it was drawn, for the frame that moves it
       next. It is recorded even when it was NOT drawn, so that showing it again
       knows it has a row to paint. */
    render->cursor_x = grid->cursor_x;
    render->cursor_y = grid->cursor_y;
    render->cursor_was_drawn = visible;
}

/* --- the suggestion -------------------------------------------------------
 *
 * The fish-style ghost: the tail of a remembered command, drawn faintly after
 * the cursor on the line the user is typing — see term_suggest.h.
 *
 * It is drawn HERE and not as a cell, and that is the whole reason it can exist
 * at all. The line belongs to the shell's readline: the terminal may paint over
 * it but must not write into the grid, or the shell would regard the ghost as
 * typed text and run it. Drawing it as pixels on top leaves the grid — and so
 * the shell's own idea of the line — untouched.
 *
 * The position comes from where the suggestion module last PUT it, and not from
 * the cursor now: the two can differ by the frame it takes the cursor to catch
 * up. It is drawn only when the module says a ghost is standing, which is zero
 * lines while there is nothing to offer.
 */
static void render_suggestion(TermCore *core, TermRender *render) {
    if (core->suggest == NULL) {
        return;
    }
    const TermSuggest *suggest = (const TermSuggest *)core->suggest;
    int cell_h = core->style->cell_height > 0 ? core->style->cell_height : 1;
    if (suggest->drawn_length <= 0 || suggest->drawn_y < 0 ||
        suggest->drawn_y >= render->grid_height / cell_h) {
        return;
    }

    int cell_w = core->style->cell_width;

    /* The ghost starts where the cursor is: everything the user has typed
       already occupies the cells to its left, and the suggestion is the part
       after it. */
    const TermGrid *grid = term_vt_screen(&core->vt);
    int start_x = grid->cursor_x;
    if (start_x < 0) {
        start_x = 0;
    }
    int baseline = suggest->drawn_y * cell_h + core->style->ascent;

    XftColor *ink = term_style_color_rgb(core->style,
                                         term_style_suggestion(core->style));
    XftFont *face = core->style->faces[TERM_FACE_NORMAL];

    /* Xft takes code points, so the UTF-8 is decoded here, exactly as the bar
       does. The ghost holds what a command holds — mostly ASCII — and the
       decode is what makes a directory with a non-ASCII name draw. */
    uint32_t codes[TERM_RENDER_MAX_RUN];
    int count = 0;
    const char *p = suggest->suggestion;
    for (; *p != '\0' && count < TERM_RENDER_MAX_RUN; ) {
        unsigned char c = (unsigned char)*p;
        uint32_t cp = c;
        int step = 1;
        if (c >= 0xF0) {
            cp = (uint32_t)(c & 0x07);
            step = 4;
        } else if (c >= 0xE0) {
            cp = (uint32_t)(c & 0x0F);
            step = 3;
        } else if (c >= 0xC0) {
            cp = (uint32_t)(c & 0x1F);
            step = 2;
        }
        int have = 1;
        for (int i = 1; i < step && p[i] != '\0'; i++) {
            cp = (cp << 6) | (uint32_t)((unsigned char)p[i] & 0x3F);
            have++;
        }
        if (have != step) {
            break;
        }
        codes[count++] = cp;
        p += step;
    }

    if (count > 0) {
        XftDrawString32(core->style->xft_draw, ink, face,
                        start_x * cell_w, baseline, codes, count);
    }
}

/* --- the two surfaces ----------------------------------------------------- */

/* Fill a band of rows of the GRID surface with the theme's background. Used to
   clear the whole grid on a full redraw, and a single row before it is
   redrawn. */
static void clear_grid_band(TermCore *core, TermRender *render, int y,
                            int rows) {
    XSetForeground(core->display, core->style->gc,
                   (unsigned long)term_style_default_bg(core->style));
    XFillRectangle(core->display, render->grid, core->style->gc, 0,
                   y * core->style->cell_height,
                   (unsigned)render->grid_width,
                   (unsigned)(rows * core->style->cell_height));
}

/* Fill the whole window surface with the theme's background. A full redraw does
   this so the frame — the margin and the strip the bar is not in — is the
   terminal's colour and not whatever was left in the pixmap. */
static void clear_window(TermCore *core, TermRender *render) {
    XSetForeground(core->display, core->style->gc,
                   (unsigned long)term_style_default_bg(core->style));
    XFillRectangle(core->display, render->buffer, core->style->gc, 0, 0,
                   (unsigned)render->width, (unsigned)render->height);
}

/* The bar: the terminal's own strip along the bottom, holding the child's
   working directory.
 *
 * It is the terminal's and not the program's, which is why it is drawn here and
 * not as cells: a bar made of cells would be rows the program could overwrite,
 * would scroll with its output, and would be wrong the moment the program
 * switched to the alternate screen and cleared everything.
 *
 * The path is cut from the LEFT when it does not fit, and an ellipsis is put
 * where the cut is. The end of a path — the directory the user is actually in —
 * is the part worth reading; the beginning, `/home/user/Documents`, is the part
 * that is the same in every path on the machine. */
static void render_bar(TermCore *core, TermRender *render) {
    int cell_w = core->style->cell_width;
    int cell_h = core->style->cell_height;
    int bar_h = TERM_BAR_ROWS * cell_h;
    if (bar_h < 1) {
        return;
    }

    uint32_t bar_bg = term_style_bar_bg(core->style);
    uint32_t bar_fg = term_style_bar_fg(core->style);

    /* The bar is at the BOTTOM: its top edge is one bar-height up from the
       window's bottom edge. The grid is drawn from the top, so the two do not
       overlap and nothing has to be clipped. */
    int bar_y = render->height - bar_h;
    if (bar_y < 0) {
        bar_y = 0;
    }

    XSetForeground(core->display, core->style->gc, (unsigned long)bar_bg);
    XFillRectangle(core->display, render->buffer, core->style->gc, 0, bar_y,
                   (unsigned)render->width, (unsigned)bar_h);

    const char *title = term_core_title(core);
    if (title == NULL || title[0] == '\0') {
        return;
    }

    int inset = TERM_BAR_INSET_CELLS * cell_w;
    int room_px = render->width - 2 * inset;
    int fits = room_px / cell_w;
    if (fits <= 0) {
        return;
    }

    /* A wide character or a multi-byte sequence makes the byte count differ
       from the cell count, so the cut is made by cells and the result is what
       is drawn — the bar reads left to right and a byte-accurate cut is not
       what the eye is measuring. */
    const char *shown = title;
    char trimmed[TERM_TITLE_MAX + 4];
    int title_cells = 0;
    for (const char *p = title; *p != '\0'; ) {
        unsigned char c = (unsigned char)*p;
        int step = 1;
        if (c >= 0xF0) step = 4;
        else if (c >= 0xE0) step = 3;
        else if (c >= 0xC0) step = 2;
        title_cells++;
        p += step;
    }

    if (title_cells > fits && fits > 1) {
        /* How many cells of the END to keep, with one cell for the ellipsis. */
        int keep = fits - 1;
        int total = title_cells;
        const char *p = title;
        int skip = total - keep;
        while (skip > 0 && *p != '\0') {
            unsigned char c = (unsigned char)*p;
            int step = 1;
            if (c >= 0xF0) step = 4;
            else if (c >= 0xE0) step = 3;
            else if (c >= 0xC0) step = 2;
            p += step;
            skip--;
        }
        snprintf(trimmed, sizeof(trimmed), "\xE2\x80\xA6%s", p);
        shown = trimmed;
    }

    XftColor *ink = term_style_color_rgb(core->style, bar_fg);
    XftFont *face = core->style->faces[TERM_FACE_NORMAL];

    /* The text is put on the bar's baseline, and the bar is at the bottom, so
       the baseline is measured from the bar's own top edge. */
    int baseline = bar_y + core->style->ascent + (bar_h - cell_h) / 2;
    if (baseline < bar_y + core->style->ascent) {
        baseline = bar_y + core->style->ascent;
    }

    /* Xft takes code points, so the UTF-8 is decoded here. The bar holds what a
       path holds — ASCII in every case that matters, and the decode is what
       makes a directory with a non-ASCII name draw rather than be dropped. */
    uint32_t codes[TERM_RENDER_MAX_RUN];
    int count = 0;
    for (const char *p = shown; *p != '\0' && count < TERM_RENDER_MAX_RUN; ) {
        unsigned char c = (unsigned char)*p;
        uint32_t cp = c;
        int step = 1;
        if (c >= 0xF0) {
            cp = (uint32_t)(c & 0x07);
            step = 4;
        } else if (c >= 0xE0) {
            cp = (uint32_t)(c & 0x0F);
            step = 3;
        } else if (c >= 0xC0) {
            cp = (uint32_t)(c & 0x1F);
            step = 2;
        }
        int have = 1;
        for (int i = 1; i < step && p[i] != '\0'; i++) {
            cp = (cp << 6) | (uint32_t)((unsigned char)p[i] & 0x3F);
            have++;
        }
        if (have != step) {
            break;
        }
        codes[count++] = cp;
        p += step;
    }

    if (count > 0) {
        /* The WINDOW's drawable and not the style's: the style's points at the
           grid surface, and the path would land on row 0 of the cells. */
        XftDrawString32(render->window_draw, ink, face,
                        inset, baseline, codes, count);
    }
}

/* The scrollbar: a track down the right edge with a thumb showing how much of
   the whole is on the screen.
 *
 * It is drawn only when there is something above the screen, which is what
 * `width == 0` from term_core_scrollbar_rect() means. A bar drawn on an empty
 * history would be a bar that does nothing when grabbed, and the first thing a
 * person does with one of those is drag it and conclude the terminal is broken.
 *
 * The geometry comes from the CORE and not from here, because the input module
 * hit-tests the same rectangle: two computations of it would be two chances to
 * disagree, and the symptom would be a thumb an inch from where the click
 * lands. */
static void render_scrollbar(TermCore *core, TermRender *render) {
    int x = 0;
    int width = 0;
    int track_y = 0;
    int track_h = 0;
    int thumb_y = 0;
    int thumb_h = 0;
    term_core_scrollbar_rect(core, &x, &width, &track_y, &track_h,
                             &thumb_y, &thumb_h);
    if (width <= 0 || track_h <= 0) {
        return;
    }

    /* The bar sits against the right edge of the window, in the margin the
       grid does not cover, and is drawn over the window surface for that
       reason. */
    XSetForeground(core->display, core->style->gc,
                   (unsigned long)term_style_bar_bg(core->style));
    XFillRectangle(core->display, render->buffer, core->style->gc,
                   x, track_y, (unsigned)width, (unsigned)track_h);

    XSetForeground(core->display, core->style->gc,
                   (unsigned long)term_style_bar_fg(core->style));
    XFillRectangle(core->display, render->buffer, core->style->gc,
                   x, track_y + thumb_y, (unsigned)width,
                   (unsigned)thumb_h);
}

/* --- one frame ------------------------------------------------------------ */

void term_render_frame(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL || render->buffer == None || render->grid == None) {
        return;
    }
    const TermGrid *grid = term_vt_screen(&core->vt);
    int full = render->needs_full;
    TermGrid *mutable_grid = (TermGrid *)grid;

    /* The row the cursor is LEAVING is drawn again, and this is why the check
       comes before the early-out below.
     *
     * The cursor is a block drawn over a cell and it does not erase itself: a
     * frame redraws the rows the PROGRAM marked, and the cursor moving marks
     * nothing. So each typed character left its block behind and put a new one
     * down, and after a few characters the line was a run of blocks with the
     * text buried under them — which is what "the input erases itself and does
     * not carry on down the screen" looked like.
     *
     * Comparing against where it was last frame is what finds the row to
     * redraw. It costs two integers and one comparison. */
    if (!full && (render->cursor_x != grid->cursor_x ||
                  render->cursor_y != grid->cursor_y ||
                  render->cursor_was_drawn !=
                      (render->cursor_on &&
                       term_vt_cursor_visible(&core->vt)))) {
        if (render->cursor_y >= 0 && render->cursor_y < mutable_grid->rows) {
            mutable_grid->lines[render->cursor_y].dirty = 1;
        }
        if (grid->cursor_y >= 0 && grid->cursor_y < mutable_grid->rows) {
            mutable_grid->lines[grid->cursor_y].dirty = 1;
        }
    }

    /* Nothing was written and nothing was asked for. This is the case on an idle
       session and the whole point of the dirty marks: the cost of calling this
       is a walk of the line marks, not a walk of the screen. */
    if (!full && !term_grid_is_dirty(grid) && !term_core_needs_draw(core)) {
        return;
    }

    /* SOMETHING ASKED FOR A REDRAW, and the rows it is about have to be marked
       or the request goes nowhere.
     *
     * term_core_damage() sets a flag meaning "draw again", and the loop below
       walks the DIRTY ROWS — of which there are none, because damage marks
       nothing. The flag therefore got as far as this function and no further:
       the early-out above let it through and then every row was skipped, so a
       selection highlight, a scrollbar move and everything else that damages
       without writing a cell drew NOTHING until some unrelated output happened
       to mark a row.
     *
     * Marking every row is the honest answer to "the picture changed and I do
       not know where": a caller that knows can mark the rows itself, and one
       that does not — a selection dragged across many lines — should not have
       to guess. The cost is one frame of the visible screen, which is what a
       redraw request means. */
    if (!full && term_core_needs_draw(core)) {
        for (int y = 0; y < mutable_grid->rows; y++) {
            mutable_grid->lines[y].dirty = 1;
        }
    }

    int rows = grid->rows < core->rows ? grid->rows : core->rows;

    if (full) {
        /* Nothing is known about what is on the window: the whole of both
           surfaces is cleared and every row is drawn. */
        clear_window(core, render);
        clear_grid_band(core, render, 0, core->rows);
    }

    /* Whether the view is scrolled back, which changes two things below: the
       rows are not the grid's own, and the cursor is not on them.
     *
     * The cursor belongs to the LIVE screen and to nothing else. A remembered
     * line has no cursor — the program that wrote it wrote it long ago and its
     * cursor is wherever it is now — so drawing one over a line from the
     * scrollback would put a block in the middle of history that no program
     * put there. */
    const TermScroll *scroll = &core->scroll;
    int scrolled = term_scroll_offset(scroll) > 0;

    for (int y = 0; y < rows; y++) {
        int from_history = scrolled &&
                           term_grid_line_number(grid, y) <
                               term_scroll_count(scroll);
        if (!full && !from_history && !grid->lines[y].dirty) {
            continue;
        }
        if (!full) {
            /* Only this row is being redrawn, so only this row is cleared. The
               cursor's row is one of these, which is why a blink does not
               disturb anything else. */
            clear_grid_band(core, render, y, 1);
        }

        /* The cells to draw come from the GRID, which answers with a
           remembered line when the view is scrolled back and with one of its
           own when it is not. The width comes back with them, because a
           remembered line may have been kept at another width after a
           resize. */
        int cols = 0;
        const TermCell *cells = row_cells(grid, y, &cols);
        if (cells == NULL || cols <= 0) {
            continue;
        }
        render_row_backgrounds(core, render, grid, cells, cols, y);
        render_row_text(core, grid, cells, cols, y);
        render_row_underlines(core, render, grid, cells, cols, y);
    }

    /* No cursor while scrolled back, for the reason above: the live screen is
       not what is being shown. The ghost goes with it — it belongs to the line
       at the prompt, and the prompt is on the live screen. */
    if (!scrolled) {
        render_cursor(core, render, grid);
        render_suggestion(core, render);
    }

    /* The pictures a program placed go over the finished cells — see
       term_image.h. They are drawn onto the GRID surface and not the window
       one, so a picture sits exactly where its cells are and follows the frame
       offset the one XCopyArea below already knows. Drawing them every frame
       rather than once is what keeps them from being erased by a partial
       redraw of the rows underneath. */
    term_image_draw(render->images, core->display, render->grid,
                    core->style->gc, core->style->cell_width,
                    core->style->cell_height);

    /* The grid is finished. It goes down onto the window at the frame's
       offset — this line is the ONLY place that knows the frame exists, which
       is what keeps every coordinate above free of it. */
    int origin_x = 0;
    int origin_y = 0;
    term_core_grid_origin(core, &origin_x, &origin_y);

    XCopyArea(core->display, render->grid, render->buffer, core->gc,
              0, 0,
              (unsigned)render->grid_width, (unsigned)render->grid_height,
              origin_x, origin_y);

    /* The bar goes on last, over the window surface, so the grid cannot cover
       it. */
    if (TERM_BAR_ROWS > 0) {
        render_bar(core, render);
    }

    /* The scrollbar goes over the window surface too, and after the bar: the
       two do not overlap — the bar is the bottom row and the track runs down
       the right edge above it — but the order is stated so a window narrow
       enough for them to meet is drawn the same way twice. */
    render_scrollbar(core, render);

    /* The frame is complete: one copy, one visible change. */
    XCopyArea(core->display, render->buffer, core->window, core->gc,
              0, 0, (unsigned)render->width, (unsigned)render->height, 0, 0);
    XFlush(core->display);

    render->needs_full = 0;
    term_grid_clear_dirty((TermGrid *)grid);
    term_core_draw_done(core);
}

void term_render_place_image(TermCore *core, int x, int y, int cols, int rows,
                             const char *path) {
    TermRender *render = render_of(core);
    if (render == NULL || render->images == NULL || core->style == NULL) {
        return;
    }
    term_image_place(render->images, core->display, core->style->visual,
                     core->style->colormap, core->depth,
                     core->style->cell_width, core->style->cell_height,
                     x, y, cols, rows, path,
                     term_style_default_bg(core->style));
    /* A picture lands over cells the dirty marks know nothing about, so the
       whole screen is drawn again. It is one frame, and a picture is placed
       once. */
    term_render_all(core);
}

void term_render_clear_images(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL || render->images == NULL) {
        return;
    }
    term_image_list_clear(render->images, core->display);
    /* The whole screen is drawn again: the picture covered cells the dirty
       marks know nothing about, so a partial frame would leave the rows it
       stood on holding its pixels until something else wrote over them. */
    term_render_all(core);
}

void term_render_all(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL) {
        return;
    }
    /* A full redraw marks every line, so frame() walks all of them. Doing it
       this way rather than with a second drawing path means there is one place
       that knows how a cell becomes pixels. */
    TermGrid *grid = (TermGrid *)term_vt_screen(&core->vt);
    for (int y = 0; y < grid->rows; y++) {
        grid->lines[y].dirty = 1;
    }
    render->needs_full = 1;
    term_core_damage(core);
    term_render_frame(core);
}

/* --- the module ----------------------------------------------------------- */

static void render_module_event(TermCore *core, XEvent *event) {
    TermRender *render = render_of(core);
    if (render == NULL) {
        return;
    }
    if (event->type == Expose) {
        /* The window was covered or is new. Nothing is known about what is on
           it, so the whole thing is drawn again. */
        render->needs_full = 1;
        term_core_damage(core);
        return;
    }
    if (event->type == ConfigureNotify) {
        term_core_request_size(core, event->xconfigure.width,
                              event->xconfigure.height);
        return;
    }
}

static void render_module_tick(TermCore *core) {
    TermRender *render = render_of(core);
    if (render == NULL) {
        return;
    }

    const TermGrid *grid = term_vt_screen(&core->vt);
    unsigned long now = term_core_now_ms();
    static unsigned long last_blink;

    if (term_grid_is_dirty(grid)) {
        /* Something arrived from the program: the cursor is shown and the blink
           starts over, so it is never caught mid-off when the user looks. */
        render->cursor_on = 1;
        last_blink = now;
    } else if (now - last_blink >= TERM_CURSOR_BLINK_MS) {
        /* A hidden cursor has nothing to blink, and flipping the flag anyway
           would mark its row dirty every half second on a screen that is not
           changing — a wake and a redraw for a cursor that is not drawn. */
        if (term_vt_cursor_visible(&core->vt)) {
            render->cursor_on = !render->cursor_on;

            /* Only the cursor's own row has to be redrawn, and marking it is
               what says so — the same mark a character write makes. A blink
               therefore costs one row and not a screen. */
            TermGrid *mutable_grid = (TermGrid *)grid;
            if (grid->cursor_y >= 0 && grid->cursor_y < mutable_grid->rows) {
                mutable_grid->lines[grid->cursor_y].dirty = 1;
            }
        }
        last_blink = now;
    }

    term_render_frame(core);
}

static void term_render_module_cleanup(TermCore *core) {
    term_render_free(core);
}

const TermModule term_render_module = {
    .name = "render",
    .init = term_render_init,
    .event = render_module_event,
    .tick = render_module_tick,
    /* Twenty milliseconds, which is fifty frames a second. It is the ceiling on
       how often anything is drawn, and it is not lower because the wake itself
       costs more than the drawing on an idle screen. */
    .interval_ms = 20,
    .cleanup = term_render_module_cleanup,
};
