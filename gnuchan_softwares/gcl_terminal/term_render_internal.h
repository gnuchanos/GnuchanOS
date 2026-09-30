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

    /* The cursor's blink. The module's tick flips it, and a tick only happens
       when the display has been quiet — so the cursor stops blinking the
       moment the program is drawing, which is what every terminal does. */
    int cursor_on;

    /* Whether the whole window needs redrawing on the next frame: after a
       resize or an expose, nothing is known about what is on the window. */
    int needs_full;
} TermRender;

#endif /* GNUCHANTERM_RENDER_INTERNAL_H */
