/*
 * term_style.h — the font, the palette, and the graphics context.
 *
 * One of each, owned by the core and handed to anything that draws. A second
 * font is how one window starts drawing in two sizes; a second palette is how
 * one screen starts showing two sets of colours. Both are the kind of fault
 * that is invisible until it is everywhere, so there is one place they live.
 *
 * --- why Xft and not the core X font calls ---
 *
 * The core X text calls draw a font image the server holds. That is fine for a
 * terminal that only ever shows ASCII in one size, and wrong for this one:
 *
 *   * The glyphs this has to draw are braille patterns, box drawing characters,
 *     and the arrows and blocks a modern program's output is made of. Those are
 *     code points above 0x1000 and the core protocol cannot address them at all
 *     — it has no way to name a character, only a byte and a font that maps it.
 *   * Antialiasing is what makes a monospace font readable at the small size a
 *     terminal uses, and the core protocol has none.
 *
 * Xft draws an array of 32 bit code points with antialiasing, and it is what
 * every terminal on the system draws with. The renderer uses it for the same
 * reason they do.
 *
 * --- and why there is no glyph cache here ---
 *
 * The obvious optimisation is to draw each character once into a pixmap and
 * copy it from then on. Xft already does exactly that: an XftFont holds the
 * glyphs it has loaded, and a second draw of the same character composites the
 * same server-side glyph. A cache of our own would be a second copy of
 * something already cached, one more lookup per cell, and one more thing to
 * keep in step with the font.
 *
 * What actually makes the renderer cheap is not caching glyphs but not drawing
 * the ones that did not change — see term_render.h. That saves the whole call
 * and not just part of it.
 */
#ifndef GNUCHANTERM_STYLE_H
#define GNUCHANTERM_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

#include "term_grid.h"

/* How many faces the style holds. One is the normal face; a second is the bold
   one, when the font family has a real bold to offer. The two spare slots are
   for italic and bold italic. */
#define TERM_STYLE_MAX_FACES 4

/* Which face a cell is drawn in. */
enum {
    TERM_FACE_NORMAL = 0,
    TERM_FACE_BOLD,
    TERM_FACE_ITALIC,
    TERM_FACE_BOLD_ITALIC,
};

typedef struct TermStyle {
    Display *display;
    int      screen;
    Visual  *visual;
    int      depth;
    Colormap colormap;

    /* The font faces that came out of the requested family, and the
       measurements that turn a window size into a grid size. `ascent` is what
       puts a line of text on its baseline; `cell_width` and `cell_height` are
       the size one character occupies. */
    XftFont *faces[TERM_STYLE_MAX_FACES];
    int      face_count;
    int      cell_width;
    int      cell_height;
    int      ascent;

    /* The colours. `palette` is the packed 0xRRGGBB values a cell indexes
       into; `xft_colors` is the same values converted once into the type Xft
       draws with, because converting per cell would be a conversion per cell.
       The two arrays are kept in step by index. */
    TermPalette palette;
    XftDraw    *xft_draw;    /* what Xft draws into                        */
    XftColor    xft_colors[TERM_PALETTE_SIZE];
    int         xft_color_count;

    /* Whether the display can blend, which decides how the renderer puts text
       over a background. With Render it composites; without it the cell has
       already been filled and the glyph is drawn opaque over it. */
    int has_render;

    GC  gc;
} TermStyle;

/* --- life ----------------------------------------------------------------- */

/* Open the font and build the palette. `font_name` is an Xft font description
   — a family, a size, and the hinting and antialiasing settings — and NULL
   means the built-in default. Returns 0 on success. */
int  term_style_init(TermStyle *style, Display *display, int screen,
                     const char *font_name);

void term_style_free(TermStyle *style);

/* Point Xft at the drawable it draws into. Separate from init because the font
   is needed to size the window and the window is needed to draw into, so one
   of the two has to come second. Called again when the drawable changes — a
   resize replaces the buffer. */
int  term_style_attach(TermStyle *style, Drawable drawable);

/* Set the colours a program names by number, plus the theme's own default text
   and background at TERM_COLOR_INDEX_FG and _BG. `colors` is 0xRRGGBB packed,
   and `count` is how many of them are being set. */
void term_style_set_palette(TermStyle *style, const uint32_t *colors, int count);

/* Set one entry, which is what a program does when it wants a palette colour
   changed for its own output. */
void term_style_set_color(TermStyle *style, int index, uint32_t rgb);

/* The default colours the theme chose, so a module that draws its own
   rectangle — the cursor, a selection — draws in the same ones. */
uint32_t term_style_default_fg(const TermStyle *style);
uint32_t term_style_default_bg(const TermStyle *style);

/* --- drawing -------------------------------------------------------------- */

/* The XftColor a cell's colour is drawn with. The cell carries an index rather
   than a colour so that this happens once per distinct colour on the screen
   instead of once per cell. `foreground` picks which of the cell's two. */
XftColor *term_style_color(TermStyle *style, const TermCell *cell,
                           int foreground);

/* The XftColor for a packed 0xRRGGBB, which is what a truecolor cell and a
   module drawing its own rectangle both want. */
XftColor *term_style_color_rgb(TermStyle *style, uint32_t rgb);

/* The face a cell's attributes ask for, falling back to the normal face when
   the family did not supply the shape. Bold and italic change a glyph's SHAPE;
   every other attribute is drawn over it rather than making a different
   glyph. */
XftFont *term_style_face_for(TermStyle *style, const TermCell *cell);

/* Whether the cell is drawn with its colours swapped — the SGR reverse
   attribute, which is most of what a selection highlight is made of. */
int term_style_is_reversed(const TermCell *cell);

#endif /* GNUCHANTERM_STYLE_H */
