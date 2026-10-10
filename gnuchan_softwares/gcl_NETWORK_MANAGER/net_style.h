/*
 * net_style.h — the colours, the font, and the one way text is drawn.
 *
 * The config holds colour NAMES — "#c77dff", "white" — because a name is what a
 * person writes and what a file can hold without a display. Only code with a
 * connection to the server can turn a name into the pixel the server draws
 * with, and that happens once, here, at start: a name is an XAllocNamedColor
 * round trip, and doing it on every repaint would make the window stutter.
 *
 * The font is Xft's rather than the core protocol's, the same as the rest of
 * the session, so an interface name or an address is drawn with the shaping and
 * antialiasing every other Gnuchan program uses.
 */
#ifndef GNUCHANNET_STYLE_H
#define GNUCHANNET_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "net_config.h"

typedef struct NetStyle {
    /* The two surfaces: what is behind everything, and the panel the rows sit
       on. */
    unsigned long background;
    unsigned long panel;

    /* The line around the panel, and the fill a highlighted row carries. */
    unsigned long panel_edge;
    unsigned long field;

    /* Text: the ordinary colour, the quieter one a detail is drawn in, and the
       accent a chosen label is drawn in. */
    unsigned long text;
    unsigned long text_muted;
    unsigned long accent;

    /* The two state colours: a device that is up, and one that is down or
       blocked. */
    unsigned long connected;
    unsigned long disabled;

    /* The fill of the chosen row. Its own colour rather than the panel's,
       because the panel is nearly the background and a selection drawn in it is
       a selection nobody can see. */
    unsigned long selection;

    /* The one font the whole window is drawn in. */
    XftFont *font;

    /* How tall one row is, measured from the font so a row is never shorter
       than its own text, and the room a row's text is inset from the edge. */
    int row_height;
    int padding;
} NetStyle;

/* Resolve one colour name to the pixel the server will draw with, or the
   fallback when the name is empty or unknown. */
unsigned long net_style_colour(Display *display, int screen,
                               const char *name, unsigned long fallback);

/* Resolve every colour and open the font, from the config. Returns 0 on
   success. A colour that cannot be resolved keeps the manager's own default,
   and a font that cannot be opened is not fatal either. */
int net_style_load(NetStyle *style, Display *display, int screen,
                   const NetConfig *config);

void net_style_free(NetStyle *style, Display *display);

/* Write a UTF-8 string at its baseline, in colour, in a font. The target is a
   drawable rather than an XftDraw, so a caller that paints into a buffer does
   not have to know how Xft binds a draw to a surface. */
void net_style_text(Display *display, int screen, Drawable target,
                    XftFont *font, int x, int baseline, const char *text,
                    unsigned long colour);

/* The width the text would take, so a caller can clip it to the room a row
   has. */
int net_style_text_width(Display *display, XftFont *font, const char *text);

/* Copy `text` into `out`, cut so it fits `max_width` with a trailing "...".
   Bytes are kept whole, so a cut never splits a UTF-8 character. */
void net_style_fit(Display *display, XftFont *font, const char *text,
                   int max_width, char *out, unsigned int size);

#endif /* GNUCHANNET_STYLE_H */
