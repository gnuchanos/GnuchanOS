/*
 * term_render_internal.h — what the renderer owns, and nothing else sees.
 *
 * The frame buffer, and the numbers that describe it. It is a separate header
 * because the renderer is the one module with private state: the core holds a
 * pointer and never looks inside, which is what keeps the core free of pixels.
 */
#ifndef GNUCHANTERM_RENDER_INTERNAL_H
#define GNUCHANTERM_RENDER_INTERNAL_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "term_core.h"

/* The longest run of characters drawn in one Xft call. Xft lays out a run of
   code points in a single call, so a row of text in one colour costs one call
   instead of one per character. A row of a terminal is at most a few hundred
   cells, and this is a stack bound rather than a limit anyone reaches. */
#define TERM_RENDER_MAX_RUN 256

typedef struct TermRender {
    /* The frame is composed here and copied to the window in one operation.
       Drawing straight to the window shows the frame being assembled, which on
       a screen being redrawn is a very visible flicker. */
    Pixmap buffer;
    int    width;
    int    height;

    /* The cells, on a surface of their own, drawn with no offset at all — cell
       (0,0) is at pixel (0,0) of THIS pixmap. The window's buffer holds the
       frame: this surface copied down at the frame's offset, and the bar over
       it. See term_render.c for why the two are separate.
     *
     * The size is the grid's and not the window's: it is what the child was
     * told it had, so a program's last column is inside it. */
    Pixmap grid;
    int    grid_width;
    int    grid_height;

    /* Xft draws into exactly one drawable, and there are two surfaces. The
       style's own drawable is the GRID's, because that is where the cells go;
       the bar's text goes on the WINDOW's surface, and drawing it through the
       grid's drawable would put the path on row 0 of the grid instead — where
       it would sit until that row was repainted, and then turn into whatever
       the program had printed there. That is the garbage that appeared along
       the first line the moment a key was pressed. So the bar gets a drawable
       of its own, made against the window surface and remade when that surface
       is. */
    XftDraw *window_draw;

    /* The cursor's blink. The module's tick flips it, and a tick only happens
       when the display has been quiet — so the cursor stops blinking the
       moment the program is drawing, which is what every terminal does. */
    int cursor_on;

    /* Where the cursor was when the last frame was drawn.
     *
     * The cursor is drawn as a block OVER a cell and it does not erase itself
     * as it moves: a frame redraws only the rows the program marked, and the
     * cursor's own movement marks nothing. So typing a character left the block
     * behind on the old cell and drew a new one — after a few characters the
     * typed text was buried under a run of cursor blocks, which is exactly what
     * "the input erases itself and does not go on" looked like.
     *
     * Keeping the old position is what lets the frame that moves the cursor
     * redraw the row it left as well as the row it went to. */
    int cursor_x;
    int cursor_y;
    /* Whether the cursor was drawn at all last frame, so hiding it also
       redraws the row it was last seen on. */
    int cursor_was_drawn;

    /* Whether the whole window needs redrawing on the next frame: after a
       resize or an expose, nothing is known about what is on the window. */
    int needs_full;
} TermRender;

#endif /* GNUCHANTERM_RENDER_INTERNAL_H */
