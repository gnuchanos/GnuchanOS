/*
 * wm_style.h — the colours and sizes the desktop falls back to.
 *
 * This is the desktop a machine with no settings script gets, and the base
 * the script's own colours are resolved against: wm_config_apply() replaces
 * the two window-border entries with whatever gcl_Window named, and everything
 * else stays as it is written here. So the values below are defaults, not the
 * whole of the appearance — the script is.
 *
 * Keeping the palette in one place is also what keeps the window manager and
 * the greeter looking like one system: the accent here is the same #c77dff the
 * greeter draws its focused control in.
 */
#ifndef GNUCHANWM_STYLE_H
#define GNUCHANWM_STYLE_H

#include <X11/Xlib.h>

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

    XFontStruct *font;           /* the one font the desktop draws in        */

    int border_width;            /* the frame drawn around a window          */
} WmStyle;

/* Resolve one colour name to the pixel the server will draw with, or the
   fallback when the name is empty or unknown. Public because the settings file
   holds colour names, not pixels: only code with a display can turn one into
   the other, and the config module is what has a display when it needs to. */
unsigned long wm_style_colour(Display *display, int screen,
                              const char *name, unsigned long fallback);

/* Resolve every colour and load a font. Returns 0 on success. A font that
   cannot be loaded is not fatal: a missing font falls back to whatever the X
   server offers, because a desktop that refuses to draw is worse than one
   drawn in a font the user did not choose. */
int  wm_style_load(WmStyle *style, Display *display, int screen);

void wm_style_free(WmStyle *style, Display *display);

/* Write text at its baseline, in colour, with the desktop's font. */
void wm_style_text(Display *display, Drawable drawable, GC gc, XFontStruct *font,
                   int x, int baseline, const char *text, unsigned long colour);

/* The width the text would take, so a caller can centre or clip it. */
int  wm_style_text_width(XFontStruct *font, const char *text);

#endif /* GNUCHANWM_STYLE_H */
