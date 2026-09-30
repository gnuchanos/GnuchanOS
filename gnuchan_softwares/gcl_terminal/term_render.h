/*
 * term_render.h — the cell grid drawn as pixels on the window.
 *
 * Everything up to here has been bookkeeping: a grid of cells, a parser that
 * edits it, a PTY that fills the parser. This is where it becomes a picture,
 * and it is the only place that knows a window is made of pixels at all.
 *
 * --- why it is a module and not part of the core ---
 *
 * Drawing is the part most likely to change — a shader layer, a different font
 * renderer, a bitmap dump for a test — and the rest of the terminal does not
 * depend on any of it. Keeping it behind this header is what lets the core stay
 * a window and an event loop, and lets a second implementation be swapped in
 * without touching anything else.
 *
 * --- why a frame is cheaper than it looks ---
 *
 * A 200 by 50 window is ten thousand cells, and btop redraws all of them
 * several times a second. Drawing ten thousand glyphs per frame is the naive
 * implementation and it is the wrong one:
 *
 *   * Xft lays out a string every time it is drawn, so the layout would be paid
 *     per frame even though nobody changed the text.
 *   * Each glyph is a round trip to the server, and on an Intel GMA 965 — the
 *     machine this is written to stay light on — that is the whole frame budget
 *     spent redrawing a picture that mostly did not change.
 *
 * So the renderer draws only what changed. The grid marks a line when a cell in
 * it is written, and the renderer walks the marked lines. A screen of coloured
 * output costs a whole screen once; the steady state — btop's graphs moving a
 * little every second — costs the fraction that actually moved. On a session
 * where nothing is happening, a frame is a walk of the line marks and no
 * drawing at all.
 *
 * What is NOT done is caching glyphs. Xft already caches them inside the font,
 * and a second cache here would be a second copy of something already cached.
 * See term_style.h.
 */
#ifndef GNUCHANTERM_RENDER_H
#define GNUCHANTERM_RENDER_H

#include "term_core.h"

/* --- life ----------------------------------------------------------------- */

/* Make the frame buffer and point Xft at it. Called by the renderer's own
   module init, after the core has a window and the style has a font. Returns 0
   on success.
 *
 * The buffer is a pixmap at the window's depth, because it is copied to the
 * window as a whole picture and a different depth would have to be converted
 * pixel by pixel. */
int  term_render_init(TermCore *core);

/* Free the buffer and everything that points at it. Called by the module's
   cleanup, in reverse order. */
void term_render_free(TermCore *core);

/* Rebuild the buffer at a new size. Called when the window changes size, and
   the old buffer is freed with it. The whole window needs redrawing afterwards,
   which is what it is marked for. */
int  term_render_resize(TermCore *core, int width, int height);

/* --- drawing -------------------------------------------------------------- */

/* Draw everything that changed, and put it on the window.
 *
 * It does nothing at all when no cell was written since the last frame, which
 * is the common case on an idle session: the loop can call this every turn
 * without the cost being a function of how often it is called. */
void term_render_frame(TermCore *core);

/* Draw the whole grid regardless of what changed. An expose or a font change is
   what this is for: the dirty marks describe what changed since the last frame,
   and after either of those nothing is known about what is on the window. */
void term_render_all(TermCore *core);

/* --- the module ----------------------------------------------------------- */

/* The renderer as the core sees it: it answers Expose and Configure, draws on a
   tick, and owns the buffer. Its interval is what makes the cursor blink. */
extern const TermModule term_render_module;

#endif /* GNUCHANTERM_RENDER_H */
