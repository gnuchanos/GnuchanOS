/*
 * wifi_style.c — resolve the palette and open the font.
 *
 * Every colour name is parsed and allocated once, at start, so the drawing
 * code never touches the colour map again. The window manager and the launcher
 * do the same, and for the same reason: a name resolved per row per repaint is
 * the difference between a list that appears at once and one that redraws
 * slowly when a scan lands.
 *
 * A name that cannot be resolved falls back to the manager's own default
 * rather than to black: the default is a colour chosen to be readable against
 * the panel, and a mistyped name should leave the window legible rather than
 * paint a row in the colour of the thing behind it.
 */
#include "wifi_style.h"

#include <stdio.h>
#include <string.h>

unsigned long wifi_style_colour(Display *display, int screen,
                                const char *name, unsigned long fallback) {
    if (!name || !name[0]) {
        return fallback;
    }
    Colormap cmap = DefaultColormap(display, screen);
    XColor colour;
    XColor exact;
    if (XAllocNamedColor(display, cmap, name, &colour, &exact)) {
        return colour.pixel;
    }
    return fallback;
}

/* The font, opened from the family and size the config named.
 *
 * The first attempt is family and size together — "monospace:pixelsize=14" —
 * which is what the config meant. A font that is not installed at that size
 * still has a family, so the family alone is asked for second, and the
 * manager's own font last: three attempts, in the order that gets closest to
 * what was written, and a font that is never NULL because the server always
 * has one. */
static XftFont *open_font(Display *display, int screen,
                          const WifiConfig *config) {
    char spec[WIFI_TEXT + 32];

    if (config->font_family[0]) {
        snprintf(spec, sizeof(spec), "%s:pixelsize=%d",
                 config->font_family, config->font_size);
        XftFont *font = XftFontOpenName(display, screen, spec);
        if (font) {
            return font;
        }
        font = XftFontOpenName(display, screen, config->font_family);
        if (font) {
            return font;
        }
    }

    const char *candidates[] = {
        "monospace:pixelsize=14",
        "fixed:pixelsize=14",
        "sans:pixelsize=14",
        NULL,
    };
    for (int i = 0; candidates[i]; i++) {
        XftFont *font = XftFontOpenName(display, screen, candidates[i]);
        if (font) {
            return font;
        }
    }
    return XftFontOpenName(display, screen, "fixed");
}

int wifi_style_load(WifiStyle *style, Display *display, int screen,
                    const WifiConfig *config) {
    memset(style, 0, sizeof(*style));

    /* The fallbacks are the manager's own palette in pixels — the walk below
       resolves the config's names over them — so a machine that named no
       colour at all is drawn in the palette the shipped config describes. */
    unsigned long black = BlackPixel(display, screen);
    unsigned long white = WhitePixel(display, screen);

    unsigned long background = wifi_style_colour(display, screen,
                                                 "#1a0b2e", black);
    unsigned long panel = wifi_style_colour(display, screen, "#32143f", black);
    unsigned long edge = wifi_style_colour(display, screen, "#7b2cbf", white);
    unsigned long field = wifi_style_colour(display, screen, "#241033", black);
    unsigned long text = wifi_style_colour(display, screen, "#e0c3fc", white);
    unsigned long muted = wifi_style_colour(display, screen, "#9d7bba", white);
    unsigned long accent = wifi_style_colour(display, screen, "#c77dff", white);
    unsigned long secured = wifi_style_colour(display, screen, "#ff9e64", white);
    unsigned long connected = wifi_style_colour(display, screen, "#9ece6a",
                                                white);
    unsigned long selection = wifi_style_colour(display, screen, "#5a2a8f",
                                                black);

    style->background = wifi_style_colour(display, screen,
                                          config->background, background);
    style->panel = wifi_style_colour(display, screen, config->panel, panel);
    style->panel_edge = wifi_style_colour(display, screen,
                                          config->panel_edge, edge);
    style->field = wifi_style_colour(display, screen, config->field, field);
    style->text = wifi_style_colour(display, screen, config->text, text);
    style->text_muted = wifi_style_colour(display, screen,
                                          config->text_muted, muted);
    style->accent = wifi_style_colour(display, screen, config->accent, accent);
    style->secured = wifi_style_colour(display, screen, config->secured,
                                       secured);
    style->connected = wifi_style_colour(display, screen, config->connected,
                                         connected);
    style->selection = wifi_style_colour(display, screen, config->selection,
                                         selection);

    style->font = open_font(display, screen, config);

    /* A row is as tall as a line of text plus room above and below it, so the
       rows never touch and the list reads as a list rather than a block of
       words. Measured from the font rather than fixed, so a manager configured
       with a large font grows its rows with it. */
    int line = style->font ? (style->font->ascent + style->font->descent) : 16;
    style->padding = 10;
    style->row_height = line + 12;
    if (style->row_height < 26) {
        style->row_height = 26;
    }

    return 0;
}

void wifi_style_free(WifiStyle *style, Display *display) {
    if (style->font) {
        XftFontClose(display, style->font);
        style->font = NULL;
    }
}

int wifi_style_text_width(Display *display, XftFont *font, const char *text) {
    if (!display || !font || !text) {
        return 0;
    }
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (const FcChar8 *)text,
                       (int)strlen(text), &extents);
    return (int)extents.xOff;
}

void wifi_style_text(Display *display, int screen, Drawable target,
                     XftFont *font, int x, int baseline, const char *text,
                     unsigned long colour) {
    if (!display || !font || !text || !*text || target == None) {
        return;
    }

    Colormap cmap = DefaultColormap(display, screen);
    Visual *visual = DefaultVisual(display, screen);

    /* The palette is allocated as XLIB colours, because that is what a
       Colormap knows; Xft draws through a render colour, so the pixel has to
       be asked which red, green and blue it stands for before it can be
       drawn. The question is asked per call and the answer not kept: the
       colour is the caller's pixel, and a cache of them here would be a
       second place the palette lives. */
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
