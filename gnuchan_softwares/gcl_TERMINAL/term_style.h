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

/* How many distinct 0xRRGGBB colours are kept converted, for cells that carry a
   colour rather than a palette index, and for the bar and the cursor.
 *
 * It is a cache of its own and NOT spare slots in the palette, which is the
 * fault it was written to fix: the conversion used to write the colour into the
 * next free PALETTE entry, so the first thing that asked for a truecolor — the
 * bar, on the very first frame — overwrote palette entry 18 for good. A program
 * that then asked for colour 18 was handed the bar's purple, and a syntax
 * highlighter or a file manager with its own palette is exactly what notices.
 */
 
#define TERM_TRUECOLOR_CACHE 256

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

    /* The colours cells carry outright, kept apart from the palette above so
       converting one cannot overwrite an indexed colour. See
       TERM_TRUECOLOR_CACHE. */
    uint32_t truecolor_values[TERM_TRUECOLOR_CACHE];
    XftColor truecolor_colors[TERM_TRUECOLOR_CACHE];
    int      truecolor_count;

    /* Whether the display can blend, which decides how the renderer puts text
       over a background. With Render it composites; without it the cell has
       already been filled and the glyph is drawn opaque over it. */
    int has_render;

    /* The bar along the bottom of the window. It belongs to the terminal and
       not to the program, so no cell can name these two: a program that
       painted its own purple would be choosing the terminal's furniture. The
       defaults are the theme's, and a settings file overrides them — see
       term_config.h and term_config_apply_style(). */
    uint32_t bar_bg;
    uint32_t bar_fg;

    /* The block drawn over the cursor's cell. It is the terminal's own for the
       same reason the bar's two are: a program cannot name it, because it is
       the terminal saying where the cursor is and not the program drawing. */
    uint32_t cursor;

    /* --- what a program changed, and what it changed it FROM --------------
     *
     * A program may repaint the palette and the cursor's colour — see the OSC
     * colour handling in term_vt.c — and it may then put them back: OSC 104 for
     * the palette, 110-112 for the three personal colours. Putting one back
     * means knowing what it was, and what it was is the THEME's, which is what
     * the settings file and gcl_palette.h built and not the built-in defaults
     * of the code.
     *
     * So the baseline is a SNAPSHOT, taken once the theme has been applied —
     * see term_style_snapshot() — and the colours above are the working copy a
     * program edits. Keeping the two apart is what makes "put it back" mean the
     * colour the user configured rather than the one the terminal shipped with,
     * which for anyone who wrote a settings file are different colours. */
    TermPalette baseline;
    uint32_t    baseline_cursor;

    /* The faint text the fish-style suggestion is drawn in — see
       term_suggest.h. It is the terminal's own and not a cell's, because the
       ghost is not a character the program wrote: it is the terminal saying
       "this is what you probably meant", and a program that could name it
       could not have meant it. It is dimmer than the text so the ghost reads
       as a suggestion and not as something already typed. */
    uint32_t suggestion;

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
   and `count` is how many of them are being set.

   This is for laying down the THEME — see term_config_apply_style() and
   style_build_palette() — and not for a program changing a colour: the
   baseline is NOT touched here, which is what "put it back" restores. */
void term_style_set_palette(TermStyle *style, const uint32_t *colors, int count);

/* Set one entry, which is what a program does when it wants a palette colour
   changed for its own output — OSC 4. It changes the working palette and not
   the baseline, so the change is reversible with OSC 104. */
void term_style_set_color(TermStyle *style, int index, uint32_t rgb);

/* Take the current palette and cursor as the baseline: what a program's own
   colour changes go back to. Called ONCE, after the theme has been applied and
   before any program has run — a snapshot taken later would capture the
   program's changes as if they were the theme, and "put it back" would restore
   the wrong colours. */
void term_style_snapshot(TermStyle *style);

/* Put the palette, one entry, or the cursor back to the baseline. These are
   what OSC 104, 110, 111 and 112 do. Entries the baseline never had are left
   alone rather than set to black, so a reset can never invent a colour. */
void term_style_restore_palette(TermStyle *style);
void term_style_restore_entry(TermStyle *style, int index);
void term_style_restore_cursor(TermStyle *style);

/* The default colours the theme chose, so a module that draws its own
   rectangle — the cursor, a selection — draws in the same ones. */
uint32_t term_style_default_fg(const TermStyle *style);
uint32_t term_style_default_bg(const TermStyle *style);

/* One palette entry as a packed 0xRRGGBB. It is for a caller that has to NAME
   a colour rather than draw with one: the LS_COLORS value handed to the shell
   is built from these, so a colour changed in the settings file reaches `ls`
   as well as the screen. An index outside the palette answers with the
   background, which is a colour that exists rather than a read past the end of
   the array. */
uint32_t term_style_palette_entry(const TermStyle *style, int index);

/* --- drawing -------------------------------------------------------------- */

/* The XftColor a cell's colour is drawn with. The cell carries an index rather
   than a colour so that this happens once per distinct colour on the screen
   instead of once per cell. `foreground` picks which of the cell's two. */
XftColor *term_style_color(TermStyle *style, const TermCell *cell,
                           int foreground);

/* The same cell's colour as a packed 0xRRGGBB, for a caller that hands the
   value to XSetForeground rather than to Xft — the renderer fills a run of
   backgrounds that way.
 *
 * It resolves against the STYLE's palette, and that is the whole reason it
 * exists. The grid carries a TermPalette of its own and nothing ever fills it,
   so reading a cell's colour through the grid returned the fallback for every
   cell: black for a background, white for a foreground. That painted a black
   field over the theme and turned a reversed bar white with its text lost
   inside it. The palette belongs to the STYLE — it is the configured look of
   the terminal — and one palette is one place to change it. */
uint32_t term_style_cell_color(const TermStyle *style, const TermCell *cell,
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

/* The bar's two colours. They are the terminal's own and no program can set
   them, which is why they are read here and not from a cell. */
uint32_t term_style_bar_bg(const TermStyle *style);
uint32_t term_style_bar_fg(const TermStyle *style);

/* Set them, from the theme. */
void term_style_set_bar(TermStyle *style, uint32_t bg, uint32_t fg);

/* The cursor's colour, which the renderer draws its block in. */
uint32_t term_style_cursor(const TermStyle *style);
void term_style_set_cursor(TermStyle *style, uint32_t rgb);

/* The suggestion's colour, which the renderer draws the ghost in. Dimmer than
   the text so it reads as something offered and not something typed. */
uint32_t term_style_suggestion(const TermStyle *style);
void term_style_set_suggestion(TermStyle *style, uint32_t rgb);

#endif /* GNUCHANTERM_STYLE_H */
