/*
 * runner_draw.h — putting the launcher on the screen.
 *
 * One function, and it paints the whole window: the query line at the top, the
 * list of matches under it, and the highlight on the chosen one. It is kept
 * apart from the window and the event loop because drawing is the one part
 * worth being able to read on its own — a launcher whose rows are the wrong
 * height or whose text is the wrong colour is a launcher that looks broken
 * whatever else works, and this is where that is decided.
 *
 * The drawing goes through the style, never through a colour of its own: every
 * pixel is one of the palette's, so a person who changed a colour in the
 * settings file gets that colour in every part of the window and not three of
 * the four.
 */
#ifndef GNUCHANRUNNER_DRAW_H
#define GNUCHANRUNNER_DRAW_H

#include "runner_ui.h"

/* Paint the whole window. Called at open and after every key that changed
   anything — a letter, an arrow, Enter — because the window is small and
   redrawing it in one pass is simpler and no slower than working out which
   part changed. */
void runner_draw(const RunnerUi *ui);

#endif /* GNUCHANRUNNER_DRAW_H */
