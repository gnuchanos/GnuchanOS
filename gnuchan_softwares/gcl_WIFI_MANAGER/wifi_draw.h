/*
 * wifi_draw.h — painting one frame, and the measurements the drawing and the
 * input both need.
 *
 * The whole window is redrawn in one function, from the state in WifiUi. There
 * is no partial repaint and no damage tracking: the window is small, the draw is
 * a few dozen rectangles and some text, and redrawing it whole on every key is
 * what keeps the code something a person can follow — the picture is a function
 * of the state, so a change to the state that is not painted is not possible.
 *
 * What it paints depends on the mode: the list (with its button bar), the
 * password prompt, the forget confirmation, or the restart confirmation.
 *
 * Two things live here rather than in one file because two files need them: the
 * row count, which the drawing and the input both measure the list with, and the
 * button rectangles, which the drawing fills in and the click handler reads. A
 * rectangle drawn and a rectangle clicked have to be the same rectangle, and
 * having one function produce both is what keeps them so.
 */
#ifndef GNUCHANWIFI_DRAW_H
#define GNUCHANWIFI_DRAW_H

#include "wifi_ui.h"

/* Paint the whole window from the ui's state. Nothing here changes the state;
   it reads it (except the scroll, which it clamps to keep the selection on
   screen — a drawing that did not do that would draw the selection nowhere). */
void wifi_draw(WifiUi *ui);

/* How many network rows fit in the window before the list scrolls. */
int wifi_visible_rows(const WifiUi *ui);

/* Lay out the button bar for the current window size, filling ui->buttons and
   ui->button_count. Called before the list is drawn, so the same rectangles are
   there for the drawing and for a click. */
void wifi_layout_buttons(WifiUi *ui);

#endif /* GNUCHANWIFI_DRAW_H */
