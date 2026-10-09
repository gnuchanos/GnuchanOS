/*
 * wifi_draw.h — painting one frame, and the one measurement the drawing and
 * the input both need.
 *
 * The whole window is redrawn in one function, from the state in WifiUi. There
 * is no partial repaint and no damage tracking: the window is small, the draw
 * is a few dozen rectangles and some text, and redrawing it whole on every key
 * is what keeps the code something a person can follow — the picture is a
 * function of the state, so a change to the state that is not painted is not
 * possible.
 *
 * What it paints depends on the mode: the list, the password prompt, or the
 * forget confirmation.
 *
 * wifi_visible_rows() lives here because it is the one number two files need:
 * the drawing, to know how many rows fit, and the input, to know how far the
 * selection may move before the list scrolls. Keeping it in the header is what
 * keeps the two from disagreeing.
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

#endif /* GNUCHANWIFI_DRAW_H */
