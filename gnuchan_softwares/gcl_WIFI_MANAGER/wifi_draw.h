/*
 * wifi_draw.h — painting one frame.
 *
 * The whole window is redrawn in one function, from the state in WifiUi. There
 * is no partial repaint and no damage tracking: the window is small, the draw
 * is a few dozen rectangles and some text, and redrawing it whole on every key
 * is what keeps the code something a person can follow — the picture is a
 * function of the state, so a change to the state that is not painted is not
 * possible.
 *
 * What it paints depends on the mode. The list mode paints the title, the rows
 * of the scan, and the status line. The password mode paints the name of the
 * network being joined and the line being typed into.
 */
#ifndef GNUCHANWIFI_DRAW_H
#define GNUCHANWIFI_DRAW_H

#include "wifi_ui.h"

/* Paint the whole window from the ui's state. Nothing here changes the state;
   it reads it. */
void wifi_draw(WifiUi *ui);

#endif /* GNUCHANWIFI_DRAW_H */
