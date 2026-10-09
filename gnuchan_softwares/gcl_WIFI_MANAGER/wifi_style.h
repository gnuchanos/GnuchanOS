/*
 * wifi_style.h — the colours, the font, and the one way text is drawn.
 *
 * The config holds colour NAMES — "#c77dff", "white" — because a name is what
 * a person writes and what a file can hold without a display. Only code with a
 * connection to the server can turn a name into the pixel the server draws
 * with, and that happens once, here, at start: a name is an XAllocNamedColor
 * round trip, and doing it on every repaint of every row would make the list
 * stutter when the network list is refreshed.
 *
 * The font is Xft's rather than the core protocol's. The core protocol draws
 * text through a server-side font of 8-bit cells, which is enough for Latin-1
 * and nothing else: an SSID outside that font's encoding is sent as several
 * bytes and drawn as several boxes. Xft shapes the string, substitutes a glyph
 * from a font that has it, and antialiases the result — which is how the rest
 * of the session draws, so an SSID with an accent in it reads as its name.
 */
#ifndef GNUCHANWIFI_STYLE_H
#define GNUCHANWIFI_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "wifi_config.h"

typedef struct WifiStyle {
    /* The two surfaces: what is behind everything, and the panel the rows sit
       on. */
    unsigned long background;
    unsigned long panel;

    /* The line around the panel, and the fill a highlighted row carries. */
    unsigned long panel_edge;
    unsigned long field;

    /* Text: the ordinary colour, the quieter one a detail is drawn in, and the
       accent the selected row's name is drawn in. */
    unsigned long text;
    unsigned long text_muted;
    unsigned long accent;

    /* The two state colours: a locked network's marker, and the joined
       network's row. */
    unsigned long secured;
    unsigned long connected;

    /* The fill of the row the cursor is on. Its own colour rather than the
       panel's, because the panel is nearly the background and a selection drawn
       in it is a selection nobody can see. */
    unsigned long selection;

    /* The one font the whole window is drawn in. */
    XftFont *font;

    /* How tall one row is, measured from the font so a row is never shorter
       than its own text, and the room a row's text is inset from the edge. */
    int row_height;
    int padding;
} WifiStyle;

/* Resolve one colour name to the pixel the server will draw with, or the
   fallback when the name is empty or unknown. Public because the config holds
   names and only code with a display can turn one into a pixel. */
unsigned long wifi_style_colour(Display *display, int screen,
                                const char *name, unsigned long fallback);

/* Resolve every colour and open the font, from the config. Returns 0 on
   success. A colour that cannot be resolved keeps the manager's own default,
   and a font that cannot be opened is not fatal either: a manager drawn in the
   wrong font is one nobody notices, and one that refuses to start is one
   nobody uses. */
int wifi_style_load(WifiStyle *style, Display *display, int screen,
                    const WifiConfig *config);

void wifi_style_free(WifiStyle *style, Display *display);

/* Write a UTF-8 string at its baseline, in colour, in a font.
 *
 * The target is a drawable rather than an XftDraw, so a caller that paints
 * into the window directly does not have to know how Xft binds a draw to a
 * surface: the draw is made for the target here and released before returning.
 * The string is counted as UTF-8, not as bytes. */
void wifi_style_text(Display *display, int screen, Drawable target,
                     XftFont *font, int x, int baseline, const char *text,
                     unsigned long colour);

/* The width the text would take, so a caller can clip it to the room a row
   has. */
int wifi_style_text_width(Display *display, XftFont *font, const char *text);

#endif /* GNUCHANWIFI_STYLE_H */
