/*
 * dock_draw.h — the whole dock, painted into its window.
 *
 * One function, because the painting is one act: the body, the icons, and the
 * labels all go into a buffer and the buffer is copied up in one operation. The
 * reason is the same one wm_frame.c gives for its title bar — the dock is
 * repainted every time the pointer moves, and drawing straight to the window
 * would be seen as a flicker as the body is cleared, then partly redrawn, then
 * finished.
 *
 * The buffer is the window's own size and is kept across repaints; only its
 * contents are redrawn each time. See dock_draw.c.
 */
#ifndef GNUCHANDOCK_DRAW_H
#define GNUCHANDOCK_DRAW_H

#include "dock_core.h"

/* Paint the whole dock: the rounded body, the edge, every icon at its slot,
   and every label. The icon under the pointer — `core->hover_index` — and its
   neighbours are drawn larger, which is what makes the row a dock. */
void dock_draw(DockCore *core);

#endif /* GNUCHANDOCK_DRAW_H */
