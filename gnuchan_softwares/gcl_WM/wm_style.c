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
       desktop in a font nobody chose is better than no desktop. The names are
       Xft names, not XLFD patterns: Xft opens a font by family and size and
       substitutes one that has the glyphs, which is the whole point of drawing
       through it rather than through the server's 8-bit cells. */
    const char *candidates[] = {
        "monospace:pixelsize=13",
        "fixed:pixelsize=13",
        "sans:pixelsize=13",
        NULL,
    };
    style->font = NULL;
    for (int i = 0; candidates[i]; i++) {
        style->font = XftFontOpenName(display, screen, candidates[i]);
        if (style->font) break;
    }
    if (!style->font) {
        style->font = XftFontOpenName(display, screen, "fixed");
    }
    return 0;
}

void wm_style_free(WmStyle *style, Display *display) {
    if (style->font) {
        XftFontClose(display, style->font);
        style->font = NULL;
    }
}

XftFont *wm_style_open_font(Display *display, int screen, const char *spec) {
    if (!spec || !*spec) {
        return NULL;
    }
    return XftFontOpenName(display, screen, spec);
}

int wm_style_text_width(Display *display, XftFont *font, const char *text) {
    if (!display || !font || !text) {
        return 0;
    }
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (const FcChar8 *)text,
                       (int)strlen(text), &extents);
    return (int)extents.xOff;
}

void wm_style_text(Display *display, int screen, Drawable target, XftFont *font,
                   int x, int baseline, const char *text, unsigned long colour) {
    if (!display || !font || !text || !*text) {
        return;
    }

    Colormap cmap = DefaultColormap(display, screen);
    Visual *visual = DefaultVisual(display, screen);

    /* The palette is allocated as XLIB colours, because that is what a
       Colormap knows; Xft draws through a render colour, so the pixel has to
       be asked which red, green and blue it stands for before it can be
       drawn. The colour is allocated per call and released before returning,
       because it is the caller's pixel and not ours to keep. */
    XColor xcolor;
    xcolor.pixel = colour;
    XQueryColor(display, cmap, &xcolor);

    XRenderColor render;
    render.red = xcolor.red;
    render.green = xcolor.green;
    render.blue = xcolor.blue;
    render.alpha = 0xffff;

    XftColor xft_colour;
    if (!XftColorAllocValue(display, visual, cmap, &render, &xft_colour)) {
        return;
    }

    XftDraw *draw = XftDrawCreate(display, target, visual, cmap);
    if (draw) {
        XftDrawStringUtf8(draw, &xft_colour, font, x, baseline,
                          (const FcChar8 *)text, (int)strlen(text));
        XftDrawDestroy(draw);
    }

    XftColorFree(display, visual, cmap, &xft_colour);
}
