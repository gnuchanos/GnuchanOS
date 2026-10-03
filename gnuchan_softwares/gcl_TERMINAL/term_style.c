/*
 * term_style.c — the font, the palette, and the colours Xft draws with.
 *
 * See term_style.h for why Xft, and for why there is no glyph cache here.
 *
 * What is here is three things and nothing else: opening the font faces,
 * converting the palette into the type Xft wants, and handing out the face and
 * the colours a cell is drawn with. Everything the renderer needs and nothing
 * it does not.
 *
 * --- the two colour arrays ---
 *
 * `palette` holds packed 0xRRGGBB, which is what a theme is written in and what
 * a program's truecolor sequence carries. Xft draws with an XftColor, which is
 * an XRenderColor allocated against a colormap.
 *
 * The conversion is done once per index and the result kept, so a screen of ten
 * thousand cells in sixteen colours does sixteen conversions and ten thousand
 * array reads. Converting per cell would be ten thousand allocations per
 * frame, and the server would notice.
 */
 
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "term_style.h"
#include "gcl_palette.h"

/* The default font. It is a monospace family every Debian has through the
   fonts-dejavu-core package, at a size that fits an 80x24 window on the
   smallest screen this is likely to run on. A session changes it through the
   settings script; this is what happens when nothing said otherwise. */
static const char *DEFAULT_FONT = "monospace-11";

/* --- the built-in palette -------------------------------------------------
 *
 * THE SIXTEEN ARE PURPLE. This is the theme and not an accident of the sixteen
 * being close: every one of them is a shade of the same hue, from a deep
 * violet through the magenta-violets to a lavender, and none of them is white,
 * grey, green or blue.
 *
 * The reason to write it down is that the sixteen names exist so a program can
 * tell things apart with them — `ls` paints a directory one colour and an
 * executable another — and sixteen shades of one hue are harder to tell apart
 * than sixteen hues are. The compensation is SPREAD: neighbours are several
 * steps of lightness and saturation apart, so a directory and a file are still
 * two clearly different purples. A program that needs a sharper distinction
 * has the 256-colour and truecolor forms, and both are honoured.
 *
 * THE ARRAY IS NOT BUILT HERE. It used to be a static table of the whole
 * palette, and that stopped being possible when the palette grew past the
 * sixteen: entries 16 to 255 are the STANDARD's — the cube and the greyscale
 * ramp, which a program names by number and expects to get — and they are
 * built by term_palette_fill_standard(). The two past those are the theme's
 * own text and background. All three parts are put together in
 * style_build_palette() below.
 */
static const uint32_t THEME_ANSI[GCL_ANSI_COUNT] = GCL_PALETTE_INIT;

/* --- the bar's colours -----------------------------------------------------
 *
 * The terminal's own two, and the only colours in the program that are for the
 * furniture rather than for a program's output.
 *
 * The background is the SAME purple as the terminal's own background and not a
 * darker one, which is the whole of how the bar stops looking like a border: a
 * strip in another colour reads as a frame around the text, and a strip in the
 * same colour reads as part of the window. The one row of text on it is what
 * marks it, and that is enough.
 */
static const uint32_t DEFAULT_BAR_BG = GCL_BAR_BG;   /* the terminal's own  */
static const uint32_t DEFAULT_BAR_FG = GCL_BAR_FG;   /* and its text        */

/* The suggestion's ghost. It is the theme's own third colour and not one of the
   sixteen, for the same reason the bar has two of its own: the ghost is the
   terminal speaking, not the program, and a program cannot name it. */
static const uint32_t DEFAULT_SUGGEST = GCL_SUGGEST;

/* --- one packed colour becomes an XftColor --------------------------------
 *
 * Xft wants its colour allocated against a colormap — on a TrueColor display
 * that is a formality, but the field has to be filled or the server refuses the
 * draw. The three channels are the packed 0xRRGGBB split out and scaled from
 * eight bits to sixteen, which is what XRenderColor carries.
 *
 * The alpha is always opaque. A terminal's cells are drawn one over another —
 * a background filled, then text — and a transparent either would show the
 * layer underneath through the picture.
 */
static int xft_color_from_rgb(TermStyle *style, XftColor *out, uint32_t rgb) {
    XRenderColor render;
    render.red   = (unsigned short)(((rgb >> 16) & 0xFF) * 257);
    render.green = (unsigned short)(((rgb >> 8) & 0xFF) * 257);
    render.blue  = (unsigned short)((rgb & 0xFF) * 257);
    render.alpha = 0xFFFF;
    return XftColorAllocValue(style->display, style->visual, style->colormap,
                              &render, out) ? 0 : -1;
}

/* --- the font faces --------------------------------------------------------
 *
 * A bold request is a different font name — `:bold` appended to the family — and
 * a family that has no bold face answers with the normal one rather than
 * failing. A slot that did not load is left NULL and the renderer falls back,
 * which is why a missing face is not an error.
 *
 * The normal face is the one exception: without it there is nothing to draw at
 * all, and the terminal fails to start rather than drawing blanks.
 */
static int style_open_faces(TermStyle *style, const char *font_name) {
    static const char *SUFFIX[TERM_STYLE_MAX_FACES] = {
        "",             /* normal        */
        ":bold",        /* bold          */
        ":italic",      /* italic        */
        ":bold:italic", /* both          */
    };

    style->face_count = 0;
    for (int i = 0; i < TERM_STYLE_MAX_FACES; i++) {
        char pattern[512];
        snprintf(pattern, sizeof(pattern), "%s%s", font_name, SUFFIX[i]);
        XftFont *face = XftFontOpenName(style->display, style->screen, pattern);
        if (face == NULL) {
            if (i == TERM_FACE_NORMAL) {
                return -1;
            }
            continue;
        }
        style->faces[i] = face;
        style->face_count = i + 1;
    }

    XftFont *normal = style->faces[TERM_FACE_NORMAL];
    style->cell_width = normal->max_advance_width;
    style->cell_height = normal->ascent + normal->descent;
    style->ascent = normal->ascent;

    /* A font that reports no advance would give a grid of zero-width cells, and
       every division by the cell size would be a division by zero. A minimum is
       set rather than the font being refused: a wrong glyph width still draws
       something, and a terminal that will not open is worse. */
    if (style->cell_width < 1) style->cell_width = 8;
    if (style->cell_height < 1) style->cell_height = 16;
    return 0;
}

/* Assemble the palette the style starts with, from its three parts:
 *
 *   0-15     THEME_ANSI — what the theme chose for the sixteen a program names
 *   16-255   the standard cube and greyscale ramp, from
 *            term_palette_fill_standard()
 *   256-257  the theme's own text and background, from gcl_palette.h
 *
 * The order matters and is the reason this is one function rather than three
 * lines in three places: the standard part is written first and the theme's
 * sixteen over it, so the sixteen WIN where they overlap — and they do not
 * overlap, because they are below 16. It is written this way so that a reader
 * seeing `term_palette_fill_standard()` knows it cannot touch what the theme
 * chose.
 *
 * The two past 255 have no standard value and are the theme's; they are set
 * last so nothing can be written over them. */
static void style_build_palette(TermStyle *style) {
    uint32_t built[TERM_PALETTE_SIZE];
    memset(built, 0, sizeof(built));

    /* The standard first: everything a program can name by number. */
    term_palette_fill_standard(built);

    /* The theme's sixteen over the top of it. */
    for (int i = 0; i < GCL_ANSI_COUNT && i < TERM_PALETTE_SIZE; i++) {
        built[i] = THEME_ANSI[i];
    }

    /* The theme's own two, past everything a program can name. */
    built[TERM_COLOR_INDEX_FG] = GCL_FG;
    built[TERM_COLOR_INDEX_BG] = GCL_BG;

    term_style_set_palette(style, built, TERM_PALETTE_SIZE);
}

int term_style_init(TermStyle *style, Display *display, int screen,
                    const char *font_name) {
    memset(style, 0, sizeof(*style));
    style->display = display;
    style->screen = screen;
    style->visual = DefaultVisual(display, screen);
    style->depth = DefaultDepth(display, screen);
    style->colormap = DefaultColormap(display, screen);

    /* The XftDraw needs the drawable, which does not exist until the buffer
       does — see term_style_attach(). */
    style->xft_draw = NULL;

    if (style_open_faces(style, font_name != NULL ? font_name : DEFAULT_FONT) != 0) {
        return -1;
    }

    /* Whether the display can blend. The renderer asks before it decides how to
       put a glyph over a background, and asking once here is cheaper than
       asking per cell. */
    int major = 0;
    int minor = 0;
    style->has_render = XRenderQueryVersion(display, &major, &minor);

    style_build_palette(style);
    style->bar_bg = DEFAULT_BAR_BG;
    style->bar_fg = DEFAULT_BAR_FG;
    style->cursor = GCL_CURSOR;
    style->suggestion = DEFAULT_SUGGEST;
    return 0;
}

/* --- the bar -------------------------------------------------------------- */

uint32_t term_style_bar_bg(const TermStyle *style) {
    return style->bar_bg;
}

uint32_t term_style_bar_fg(const TermStyle *style) {
    return style->bar_fg;
}

void term_style_set_bar(TermStyle *style, uint32_t bg, uint32_t fg) {
    style->bar_bg = bg;
    style->bar_fg = fg;
}

uint32_t term_style_cursor(const TermStyle *style) {
    return style->cursor;
}

void term_style_set_cursor(TermStyle *style, uint32_t rgb) {
    style->cursor = rgb;
}

uint32_t term_style_suggestion(const TermStyle *style) {
    return style->suggestion;
}

void term_style_set_suggestion(TermStyle *style, uint32_t rgb) {
    style->suggestion = rgb;
}

void term_style_free(TermStyle *style) {
    for (int i = 0; i < TERM_STYLE_MAX_FACES; i++) {
        if (style->faces[i] != NULL) {
            XftFontClose(style->display, style->faces[i]);
            style->faces[i] = NULL;
        }
    }
    for (int i = 0; i < style->xft_color_count; i++) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->xft_colors[i]);
    }
    style->xft_color_count = 0;

    for (int i = 0; i < style->truecolor_count; i++) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->truecolor_colors[i]);
    }
    style->truecolor_count = 0;

    if (style->xft_draw != NULL) {
        XftDrawDestroy(style->xft_draw);
        style->xft_draw = NULL;
    }
    if (style->gc != NULL) {
        XFreeGC(style->display, style->gc);
        style->gc = NULL;
    }
}

int term_style_attach(TermStyle *style, Drawable drawable) {
    if (style->xft_draw != NULL) {
        XftDrawDestroy(style->xft_draw);
    }
    style->xft_draw = XftDrawCreate(style->display, drawable, style->visual,
                                    style->colormap);
    if (style->xft_draw == NULL) {
        return -1;
    }
    /* The GC is what a module fills a rectangle with — the cursor, a selection
       — and it is made against the same drawable, so it is replaced here when
       a resize replaces the buffer. */
    if (style->gc != NULL) {
        XFreeGC(style->display, style->gc);
    }
    style->gc = XCreateGC(style->display, drawable, 0, NULL);
    return style->gc != NULL ? 0 : -1;
}

/* --- the palette ---------------------------------------------------------- */

void term_style_set_palette(TermStyle *style, const uint32_t *colors, int count) {
    term_palette_set(&style->palette, colors, count);
    /* The converted colours no longer describe the palette, so they are
       dropped and remade on demand — a theme that changes while a screen is
       drawn would otherwise keep the old colours. */
    for (int i = 0; i < style->xft_color_count; i++) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->xft_colors[i]);
    }
    style->xft_color_count = 0;
}

void term_style_set_color(TermStyle *style, int index, uint32_t rgb) {
    if (index < 0 || index >= TERM_PALETTE_SIZE) {
        return;
    }
    style->palette.colors[index] = rgb;
    if (index >= style->palette.count) {
        style->palette.count = index + 1;
    }
    /* A colour that was already converted has to be converted again, or a
       program that changes a palette entry would keep drawing in the old one. */
    if (index < style->xft_color_count) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->xft_colors[index]);
        xft_color_from_rgb(style, &style->xft_colors[index], rgb);
    }
}

/* --- the baseline a program's changes go back to --------------------------
 *
 * See TermStyle.baseline for why this is a snapshot and not the built-in
 * palette. What is here is the three calls that take it and the three that
 * restore from it, and they are small because the baseline is a plain copy of
 * what the theme already built.
 *
 * The snapshot is taken ONCE, after term_config_apply_style() — see
 * GnuChanTerm.c. Taking it at any other time captures the wrong thing: before
 * the theme, the shipped defaults; after a program has run, the program's own
 * colours. Neither is what "put it back" means.
 */
void term_style_snapshot(TermStyle *style) {
    if (style == NULL) {
        return;
    }
    style->baseline = style->palette;
    style->baseline_cursor = style->cursor;
}

/* Drop every converted colour so they are remade from the palette as it now
   is. Shared by the two whole-palette restores below, and it is the reason the
   restore is more than an array copy: `xft_colors` caches the OLD values and
   a screen drawn from it would keep the colours the program set. */
static void style_drop_converted(TermStyle *style) {
    for (int i = 0; i < style->xft_color_count; i++) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->xft_colors[i]);
    }
    style->xft_color_count = 0;
}

void term_style_restore_palette(TermStyle *style) {
    if (style == NULL) {
        return;
    }
    style->palette = style->baseline;
    style_drop_converted(style);
}

/* One entry back. An index past what the baseline holds is left alone rather
   than set to black: a program that resets an entry the theme never had gets
   the colour it already had, and a reset can never invent one. */
void term_style_restore_entry(TermStyle *style, int index) {
    if (style == NULL || index < 0 || index >= TERM_PALETTE_SIZE) {
        return;
    }
    if (index >= style->baseline.count) {
        return;
    }
    style->palette.colors[index] = style->baseline.colors[index];
    if (index >= style->palette.count) {
        style->palette.count = index + 1;
    }
    /* The converted copy is dropped for this entry so the next draw remakes it
       from the restored value — see style_drop_converted() for why a stale one
       would keep the program's colour on the screen. */
    if (index < style->xft_color_count) {
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->xft_colors[index]);
        xft_color_from_rgb(style, &style->xft_colors[index],
                           style->palette.colors[index]);
    }
}

void term_style_restore_cursor(TermStyle *style) {
    if (style != NULL) {
        style->cursor = style->baseline_cursor;
    }
}

uint32_t term_style_default_fg(const TermStyle *style) {
    if (TERM_COLOR_INDEX_FG < style->palette.count) {
        return style->palette.colors[TERM_COLOR_INDEX_FG];
    }
    return GCL_FG;
}

uint32_t term_style_default_bg(const TermStyle *style) {
    if (TERM_COLOR_INDEX_BG < style->palette.count) {
        return style->palette.colors[TERM_COLOR_INDEX_BG];
    }
    return GCL_BG;
}

uint32_t term_style_palette_entry(const TermStyle *style, int index) {
    if (style == NULL) {
        return GCL_BG;
    }
    if (index < 0 || index >= style->palette.count) {
        /* Out of range answers with the background rather than reading past
           the array: a caller that named a colour the palette does not have
           gets a colour that exists, which draws and is wrong rather than
           being invisible or being a crash. */
        return term_style_default_bg(style);
    }
    return style->palette.colors[index];
}

/* --- the colours a cell is drawn with ------------------------------------- */

/* Make sure every palette entry up to `index` has been converted.
 *
 * The conversion is done in order and the count says how far it got, so a
 * screen that uses entries 0 to 15 does sixteen conversions on the first frame
 * and none after. An entry that appears later — a program that asks for colour
 * 200 — is converted when it first appears and not before. */
static void style_fill_colors_to(TermStyle *style, int index) {
    while (style->xft_color_count <= index &&
           style->xft_color_count < TERM_PALETTE_SIZE) {
        int slot = style->xft_color_count;
        uint32_t rgb = (slot < style->palette.count)
                           ? style->palette.colors[slot]
                           : 0x00000000u;
        if (xft_color_from_rgb(style, &style->xft_colors[slot], rgb) != 0) {
            /* A display that refuses an allocation is a display this cannot
               draw on, and the slot has to hold SOMETHING or every draw after
               it reads uninitialised memory. The first colour is copied in,
               which is visible and wrong rather than invisible. */
            if (slot > 0) {
                style->xft_colors[slot] = style->xft_colors[0];
            } else {
                memset(&style->xft_colors[slot], 0, sizeof(XftColor));
            }
        }
        style->xft_color_count++;
    }
}

XftColor *term_style_color_rgb(TermStyle *style, uint32_t rgb) {
    /* A truecolor cell's colour is not in the palette, so it is kept in a cache
       of its own — and the separation is the whole point.
     *
     * This used to record the converted colour in the next free PALETTE entry,
     * so that the lookup could be the same linear scan. It appeared to work and
     * it quietly corrupted the palette: the bar is drawn in a truecolor on the
     * very first frame, so entry 18 was overwritten with the bar's colour
     * before any program had drawn a character, and a program that asked for
     * colour 18 got the bar's colour and not the palette's. Every colour above
     * the sixteen was a slot a truecolor could take.
     *
     * The cache below cannot do that: its entries are the packed values
     * themselves, matched against the value being asked for, and nothing writes
     * into `palette`.
     *
     * The search is linear, and it is not on the hot path: a cell reaches here
     * only when its colour arrived as 0xRRGGBB, and the common case — a
     * gradient or a syntax highlight — asks for the same few hundred colours
     * over and over, which is what the cache holds. */
    for (int i = 0; i < style->truecolor_count; i++) {
        if (style->truecolor_values[i] == rgb) {
            return &style->truecolor_colors[i];
        }
    }
    if (style->truecolor_count >= TERM_TRUECOLOR_CACHE) {
        /* The cache is full. The colour is converted into the last slot and
           displaces what was there rather than going unconverted: a wrong but
           live colour is readable, and the alternative is a cell drawn in the
           first entry's colour, which for two colours that are far apart is a
           cell that cannot be read at all. */
        int slot = TERM_TRUECOLOR_CACHE - 1;
        XftColorFree(style->display, style->visual, style->colormap,
                     &style->truecolor_colors[slot]);
        if (xft_color_from_rgb(style, &style->truecolor_colors[slot], rgb) != 0) {
            return &style->truecolor_colors[0];
        }
        style->truecolor_values[slot] = rgb;
        return &style->truecolor_colors[slot];
    }

    int slot = style->truecolor_count;
    if (xft_color_from_rgb(style, &style->truecolor_colors[slot], rgb) != 0) {
        /* A display that refuses the allocation. The entry is still counted, so
           the slot holds whatever `xft_color_from_rgb` left — which is a zeroed
           XftColor, and drawn as black rather than as uninitialised memory. */
        memset(&style->truecolor_colors[slot], 0,
               sizeof(style->truecolor_colors[slot]));
    }
    style->truecolor_values[slot] = rgb;
    style->truecolor_count++;
    return &style->truecolor_colors[slot];
}

uint32_t term_style_cell_color(const TermStyle *style, const TermCell *cell,
                               int foreground) {
    int32_t index = foreground ? cell->fg : cell->bg;

    if (index == TERM_COLOR_TRUECOLOR) {
        return foreground ? cell->truecolor_fg : cell->truecolor_bg;
    }
    if (index == TERM_COLOR_DEFAULT || index < 0 ||
        index >= style->palette.count) {
        index = foreground ? TERM_COLOR_INDEX_FG : TERM_COLOR_INDEX_BG;
    }
    if (index < 0 || index >= TERM_PALETTE_SIZE ||
        index >= style->palette.count) {
        return foreground ? GCL_FG : GCL_BG;
    }
    return style->palette.colors[index];
}

XftColor *term_style_color(TermStyle *style, const TermCell *cell,
                           int foreground) {
    int32_t index = foreground ? cell->fg : cell->bg;

    if (index == TERM_COLOR_TRUECOLOR) {
        return term_style_color_rgb(style,
                                    foreground ? cell->truecolor_fg
                                               : cell->truecolor_bg);
    }
    if (index == TERM_COLOR_DEFAULT || index < 0 ||
        index >= style->palette.count) {
        index = foreground ? TERM_COLOR_INDEX_FG : TERM_COLOR_INDEX_BG;
    }
    style_fill_colors_to(style, index);
    return &style->xft_colors[index];
}

/* --- the face and the reverse flag ---------------------------------------- */

XftFont *term_style_face_for(TermStyle *style, const TermCell *cell) {
    uint32_t face = TERM_FACE_NORMAL;
    int bold = (cell->attrs & TERM_ATTR_BOLD) != 0;
    int italic = (cell->attrs & TERM_ATTR_ITALIC) != 0;

    if (bold && italic) {
        face = TERM_FACE_BOLD_ITALIC;
    } else if (bold) {
        face = TERM_FACE_BOLD;
    } else if (italic) {
        face = TERM_FACE_ITALIC;
    }

    if (face < TERM_STYLE_MAX_FACES && style->faces[face] != NULL) {
        return style->faces[face];
    }
    return style->faces[TERM_FACE_NORMAL];
}

int term_style_is_reversed(const TermCell *cell) {
    return (cell->attrs & TERM_ATTR_REVERSE) != 0;
}
