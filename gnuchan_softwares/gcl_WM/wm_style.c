/*
 * wm_style.c — resolve the palette and load the font.
 *
 * Named colours are parsed and allocated once, so the drawing code never
 * touches the colour map again. A colour that cannot be resolved falls back to
 * the server's black and white: a desktop drawn in the wrong colour is usable,
 * one that refuses to start is not.
 */
#include <string.h>

#include "wm_style.h"

unsigned long wm_style_colour(Display *display, int screen,
                                  const char *name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(display, screen);
    XColor colour;
    XColor exact;
    if (XAllocNamedColor(display, cmap, name, &colour, &exact)) {
        return colour.pixel;
    }
    return fallback;
}

int wm_style_load(WmStyle *style, Display *display, int screen) {
    unsigned long black = BlackPixel(display, screen);
    unsigned long white = WhitePixel(display, screen);

    style->background       = wm_style_colour(display, screen, "#1a0b2e", black);
    style->panel            = wm_style_colour(display, screen, "#32143f", black);
    style->panel_edge       = wm_style_colour(display, screen, "#7b2cbf", white);
    style->field            = wm_style_colour(display, screen, "#241033", black);
    style->text             = wm_style_colour(display, screen, "#e0c3fc", white);
    style->text_muted       = wm_style_colour(display, screen, "#9d7bba", white);
    style->accent           = wm_style_colour(display, screen, "#c77dff", white);
    style->accent_dim       = wm_style_colour(display, screen, "#7b2cbf", white);
    style->border           = wm_style_colour(display, screen, "#c77dff", white);
    style->border_unfocused = wm_style_colour(display, screen, "#32143f", black);

    style->border_width = 2;

    /* A missing font is not fatal: the server always has at least one, and a
       desktop in a font nobody chose is better than no desktop. */
    const char *candidates[] = {
        "-*-fixed-medium-r-normal--14-*-*-*-*-*-*-*",
        "-*-helvetica-medium-r-normal--14-*-*-*-*-*-*-*",
        "fixed",
        NULL,
    };
    style->font = NULL;
    for (int i = 0; candidates[i]; i++) {
        style->font = XLoadQueryFont(display, candidates[i]);
        if (style->font) break;
    }
    if (!style->font) {
        style->font = XLoadQueryFont(display, "fixed");
    }
    return 0;
}

void wm_style_free(WmStyle *style, Display *display) {
    if (style->font) {
        XFreeFont(display, style->font);
        style->font = NULL;
    }
}

int wm_style_text_width(XFontStruct *font, const char *text) {
    if (!font || !text) {
        return 0;
    }
    return XTextWidth(font, text, (int)strlen(text));
}

void wm_style_text(Display *display, Drawable drawable, GC gc,
                   XFontStruct *font, int x, int baseline,
                   const char *text, unsigned long colour) {
    if (!text || !font) {
        return;
    }
    XSetFont(display, gc, font->fid);
    XSetForeground(display, gc, colour);
    XDrawString(display, drawable, gc, x, baseline, text, (int)strlen(text));
}
