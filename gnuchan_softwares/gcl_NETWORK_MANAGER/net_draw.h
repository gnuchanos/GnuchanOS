/*
 * net_draw.h — painting one frame, and the measurements the drawing and the
 * input both need.
 *
 * The whole window is redrawn in one function, from the state in NetUi. There is
 * no partial repaint and no damage tracking: the window is small, the draw is a
 * few dozen rectangles and some text, and redrawing it whole on every key is
 * what keeps the picture a plain function of the state.
 *
 * What it paints depends on the mode: the device list (with its button bar) or
 * the DNS panel (with its field and button bar).
 *
 * The two functions here that both the drawing and the input need are the
 * visible-row count and the button rectangles: a rectangle drawn and a rectangle
 * clicked have to be the same rectangle, so one function produces both.
 */
#ifndef GNUCHANNET_DRAW_H
#define GNUCHANNET_DRAW_H

#include "net_ui.h"

/* Paint the whole window from the ui's state. Nothing here changes the state;
   it reads it (except the scroll, which it clamps to keep the selection on
   screen). */
void net_draw(NetUi *ui);

/* How many device rows fit in the window before the list scrolls. */
int net_visible_rows(const NetUi *ui);

/* Lay out the button bar for the current mode and window size, filling
   ui->buttons and ui->button_count. Called before the panel is drawn, so the
   same rectangles are there for the drawing and for a click. */
void net_layout_buttons(NetUi *ui);

/* The rectangle of DNS box `index` (0 = IPv4 preferred, 1 = IPv4 alternate,
   2 = IPv6 preferred, 3 = IPv6 alternate), for the DNS panel. Returns 1 and
   fills the four out-parameters, or 0 when there is no such box. Shared by the
   drawing and the click handler so a box that is drawn and a box that is
   clicked are the same rectangle. */
int net_dns_box_rect(const NetUi *ui, int index,
                     int *x, int *y, int *width, int *height);

#endif /* GNUCHANNET_DRAW_H */
