/*
 * settings_style.h — the panel's own look: its palette and its fonts.
 *
 * GnuChanSettings is a program of the session like any other, so it is drawn
 * in the desktop's palette: the same purples GnuChanWM, GnuChanDock and the
 * greeter use. This header and its .c are where those colours become pixels
 * and those names become open fonts — the one place that knows about Xft, so
 * the window code can ask for "the accent colour" and get a pixel back.
 */
#ifndef GNUCHANSETTINGS_STYLE_H
#define GNUCHANSETTINGS_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

/* The colours the panel is drawn in, resolved to pixels on this display. The
   names are what the panel MEANS by each one — a surface or a role — and not a
   description of the hue, so a different theme is a different set of values
   here and nothing else. */
typedef struct SettingsStyle {
    unsigned long background;     /* the window behind everything       */
    unsigned long sidebar;        /* the category column                */
    unsigned long sidebar_active; /* the chosen category                */
    unsigned long panel;          /* a row's field                      */
    unsigned long panel_edge;     /* lines                              */
    unsigned long text;           /* what a person reads                */
    unsigned long text_muted;     /* labels and hints                   */
    unsigned long accent;         /* the chosen category's mark, Save   */
    unsigned long danger;         /* a save that failed                 */

    XftFont *font;                /* the body font                      */
    XftFont *font_bold;           /* headings and the category titles   */
} SettingsStyle;

/* Build the style for this display and screen. Returns 0 on success, -1 when
   the display cannot provide what is needed; on failure nothing is left
   allocated. */
int settings_style_load(SettingsStyle *style, Display *display, int screen);

/* Release what settings_style_load() opened. Safe to call on a style that was
   never loaded. */
void settings_style_free(SettingsStyle *style, Display *display);

/* A colour name resolved to a pixel, falling back to `fallback` when the name
   is empty or the server has no such colour. Used here and by the colour
   picker's swatch. */
unsigned long settings_style_colour(Display *display, int screen,
                                    const char *name, unsigned long fallback);

#endif /* GNUCHANSETTINGS_STYLE_H */
