/*
 * runner_style.h — the colours, the font, and the one way the launcher draws
 * text.
 *
 * The config holds colour NAMES — "#c77dff", "white" — because a name is what
 * a person writes and what a file can hold without a display. Only code with a
 * connection to the server can turn a name into the pixel the server draws
 * with, and that happens once, here, at start: loading a name is an
 * XAllocNamedColor round trip, and doing it on every repaint of every row
 * would be the difference between a launcher that appears the moment a key is
 * pressed and one that does not.
 *
 * The font is Xft's rather than the core protocol's. The core protocol draws
 * text through a server-side font of 8-bit cells, which is enough for Latin-1
 * and nothing else: a name outside the font's single-byte encoding is sent as
 * several bytes and drawn as several boxes. Xft shapes the string, substitutes
 * a glyph from a font that has it, and antialiases the result — which is how
 * the rest of the session draws, so the launcher stops being the one surface
 * with a different idea of what a character is.
 */
#ifndef GNUCHANRUNNER_STYLE_H
#define GNUCHANRUNNER_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "runner_config.h"

typedef struct RunnerStyle {
    /* The window's own two surfaces: what is behind everything, and the panel
       the rows sit on. */
    unsigned long background;
    unsigned long panel;

    /* The line around the panel, and the fill a highlighted row carries. */
    unsigned long panel_edge;
    unsigned long field;

    /* Text: the ordinary colour, the quieter one a description is drawn in,
       and the accent the selected row's name is drawn in. */
    unsigned long text;
    unsigned long text_muted;
    unsigned long accent;

    /* The one font the whole window is drawn in. */
    XftFont *font;

    /* How tall one row is, measured from the font so a row is never shorter
       than its own text. */
    int row_height;

    /* The room a row's text is inset from the panel's edge. */
    int padding;
} RunnerStyle;

/* Resolve one colour name to the pixel the server will draw with, or the
   fallback when the name is empty or unknown. Public because the config holds
   names and only code with a display can turn one into a pixel. */
unsigned long runner_style_colour(Display *display, int screen,
                                  const char *name, unsigned long fallback);

/* Resolve every colour and open the font, from the config. Returns 0 on
   success. A colour that cannot be resolved keeps the launcher's own default,
   and a font that cannot be opened is not fatal either: a launcher drawn in
   the wrong font is one nobody notices, and one that refuses to start is one
   nobody uses. */
int runner_style_load(RunnerStyle *style, Display *display, int screen,
                      const RunnerConfig *config);

void runner_style_free(RunnerStyle *style, Display *display);

/* Write a UTF-8 string at its baseline, in colour, in a font.
 *
 * The target is a drawable rather than an XftDraw, so a caller that paints
 * into the window directly does not have to know how Xft binds a draw to a
 * surface: the draw is made for the target here and released again before
 * returning. The string is counted in characters as UTF-8, not in bytes. */
void runner_style_text(Display *display, int screen, Drawable target,
                       XftFont *font, int x, int baseline, const char *text,
                       unsigned long colour);

/* The width the text would take, so a caller can clip it to the room a row
   has. */
int runner_style_text_width(Display *display, XftFont *font, const char *text);

#endif /* GNUCHANRUNNER_STYLE_H */
