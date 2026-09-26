/*
 * wm_style.h — the colours, the font, and the one way this desktop draws text.
 *
 * This is the desktop a machine with no settings script gets, and the base the
 * script's own colours are resolved against: wm_config_apply() replaces the two
 * window-border entries with whatever gcl_Window named, and everything else
 * stays as it is written here. So the values below are defaults, not the whole
 * of the appearance — the script is.
 *
 * Keeping the palette in one place is also what keeps the window manager and
 * the greeter looking like one system: the accent here is the same #c77dff the
 * greeter draws its focused control in.
 *
 * --- why the font is an XftFont and not an XFontStruct ---
 *
 * The core X protocol draws text through a server-side font that is an array of
 * 8-bit cells. That is enough for a window title in Latin-1 and nothing else: a
 * character outside the font's single-byte encoding is sent as several bytes
 * and drawn as several boxes, which is what a workspace symbol like "●" became.
 *
 * Xft is the same text drawn as text: a font is opened by name, a string is
 * shaped, a font that has a glyph is substituted where the chosen one has none,
 * and the result is antialiased. It is how the rest of the session draws, so
 * the bar stops being the one surface with a different idea of what a character
 * is.
 */
#ifndef GNUCHANWM_STYLE_H
#define GNUCHANWM_STYLE_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

typedef struct WmStyle {
    /* Purple, dark to bright. */
    unsigned long background;    /* #1a0b2e - the desktop behind everything  */
    unsigned long panel;         /* #32143f - the bar and the menus          */
    unsigned long panel_edge;    /* #7b2cbf - the line around them           */
    unsigned long field;         /* #241033 - a raised or hovered control    */
    unsigned long text;          /* #e0c3fc - normal text                    */
    unsigned long text_muted;    /* #9d7bba - secondary text                 */
    unsigned long accent;        /* #c77dff - the focused or active thing    */
    unsigned long accent_dim;    /* #7b2cbf - an accented thing, idle        */
    unsigned long border;        /* #c77dff - a focused window's border      */
    unsigned long border_unfocused; /* #32143f - any other window's          */

    XftFont *font;               /* the one font the desktop draws in        */

    int border_width;            /* the frame drawn around a window          */
} WmStyle;

/* Resolve one colour name to the pixel the server will draw with, or the
   fallback when the name is empty or unknown. Public because the settings file
   holds colour names, not pixels: only code with a display can turn one into
   the other, and the config module is what has a display when it needs to. */
unsigned long wm_style_colour(Display *display, int screen,
                              const char *name, unsigned long fallback);

/* Resolve every colour and open a font. Returns 0 on success. A font that
   cannot be opened is not fatal: drawing then produces no text, because a
   desktop that refuses to start is worse than one whose bar has no words. */
int  wm_style_load(WmStyle *style, Display *display, int screen);

void wm_style_free(WmStyle *style, Display *display);

/* Write a UTF-8 string at its baseline, in colour, in the given font.
 *
 * The target is a drawable rather than an XftDraw, so a caller that already
 * paints into a pixmap and copies it up does not have to know how Xft binds a
 * draw to one surface: the draw is made for the target here and released again
 * before returning. A string is counted in characters as UTF-8, not in bytes. */
void wm_style_text(Display *display, int screen, Drawable target, XftFont *font,
                   int x, int baseline, const char *text, unsigned long colour);

/* The width the text would take, so a caller can centre or clip it. */
int  wm_style_text_width(Display *display, XftFont *font, const char *text);

/* Open a font by an Xft name — "monospace:pixelsize=12" — for a caller that
   wants one of its own: the bar's widgets each name a family and a size. NULL
   when the name cannot be opened, which the caller treats as "use the
   desktop's font". */
XftFont *wm_style_open_font(Display *display, int screen, const char *spec);

#endif /* GNUCHANWM_STYLE_H */
