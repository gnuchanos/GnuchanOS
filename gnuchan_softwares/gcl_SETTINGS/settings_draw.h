/*
 * settings_draw.h — paint the panel.
 *
 * One function: settings_draw() paints the whole window from the state
 * settings_ui.c keeps. It is separate from the behaviour because painting is
 * one act over a state that someone else owns, and keeping it separate is what
 * lets the geometry the two share live in one header rather than being worked
 * out twice.
 */
#ifndef GNUCHANSETTINGS_DRAW_H
#define GNUCHANSETTINGS_DRAW_H

#include "settings_ui.h"

/* Paint the whole window: the sidebar, the header, the rows of the open
   category, and the footer. Reads the state and writes no more of it than the
   colours it caches. */
void settings_draw(SettingsUi *ui);

#endif /* GNUCHANSETTINGS_DRAW_H */
