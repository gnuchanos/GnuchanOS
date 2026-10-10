/*
 * net_style.c — resolve the palette and open the font.
 *
 * Every colour name is parsed and allocated once, at start, so the drawing code
 * never touches the colour map again. A name that cannot be resolved falls back
 * to the manager's own default rather than to black: the default is a colour
 * chosen to be readable against the panel, and a mistyped name should leave the
 * window legible rather than paint a row in the colour of the thing behind it.
 */
#include "net_style.h"

#include <stdio.h>
#include <string.h>

unsigned long net_style_colour(Display *display, int screen,
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

/* The font, opened from the family and size the config named. The first attempt
   is family and size together; a font not installed at that size still has a
   family, so the family alone is asked for second, and the manager's own font
   last — three attempts, in the order that gets closest to what was written. */
static XftFont *open_font(Display *display, int screen,
                          const NetConfig *config) {
    char spec[NET_TEXT + 32];

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

int net_style_load(NetStyle *style, Display *display, int screen,
                   const NetConfig *config) {
    memset(style, 0, sizeof(*style));

    unsigned long black = BlackPixel(display, screen);
    unsigned long white = WhitePixel(display, screen);

    unsigned long background = net_style_colour(display, screen,
                                                "#1a0b2e", black);
    unsigned long panel = net_style_colour(display, screen, "#32143f", black);
    unsigned long edge = net_style_colour(display, screen, "#7b2cbf", white);
    unsigned long field = net_style_colour(display, screen, "#241033", black);
    unsigned long text = net_style_colour(display, screen, "#e0c3fc", white);
    unsigned long muted = net_style_colour(display, screen, "#9d7bba", white);
    unsigned long accent = net_style_colour(display, screen, "#c77dff", white);
    unsigned long connected = net_style_colour(display, screen, "#9ece6a",
                                               white);
    unsigned long disabled = net_style_colour(display, screen, "#ff9e64",
                                              white);
    unsigned long selection = net_style_colour(display, screen, "#5a2a8f",
                                               black);

    style->background = net_style_colour(display, screen, config->background,
                                         background);
    style->panel = net_style_colour(display, screen, config->panel, panel);
    style->panel_edge = net_style_colour(display, screen, config->panel_edge,
                                         edge);
    style->field = net_style_colour(display, screen, config->field, field);
    style->text = net_style_colour(display, screen, config->text, text);
    style->text_muted = net_style_colour(display, screen, config->text_muted,
                                         muted);
    style->accent = net_style_colour(display, screen, config->accent, accent);
    style->connected = net_style_colour(display, screen, config->connected,
                                        connected);
    style->disabled = net_style_colour(display, screen, config->disabled,
                                       disabled);
    style->selection = net_style_colour(display, screen, config->selection,
                                        selection);

    style->font = open_font(display, screen, config);

    int line = style->font ? (style->font->ascent + style->font->descent) : 16;
    style->padding = 10;
    style->row_height = line + 12;
    if (style->row_height < 26) {
        style->row_height = 26;
    }

    return 0;
}

void net_style_free(NetStyle *style, Display *display) {
    if (style->font) {
        XftFontClose(display, style->font);
        style->font = NULL;
    }
}

int net_style_text_width(Display *display, XftFont *font, const char *text) {
    if (!display || !font || !text) {
        return 0;
    }
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (const FcChar8 *)text,
                       (int)strlen(text), &extents);
    return (int)extents.xOff;
}

void net_style_text(Display *display, int screen, Drawable target,
                    XftFont *font, int x, int baseline, const char *text,
                    unsigned long colour) {
    if (!display || !font || !text || !*text || target == None) {
        return;
    }

    Colormap cmap = DefaultColormap(display, screen);
    Visual *visual = DefaultVisual(display, screen);

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

void net_style_fit(Display *display, XftFont *font, const char *text,
                   int max_width, char *out, unsigned int size) {
    if (!out || size == 0) {
        return;
    }
    snprintf(out, size, "%s", text ? text : "");
    if (!display || !font || max_width <= 0 ||
        net_style_text_width(display, font, out) <= max_width) {
        return;
    }
    int length = (int)strlen(out);
    for (int cut = length; cut > 0; cut--) {
        if (((unsigned char)out[cut] & 0xc0) == 0x80) {
            continue;                     /* never split a UTF-8 character */
        }
        out[cut] = '\0';
        snprintf(out + cut, size - (unsigned int)cut, "...");
        if (net_style_text_width(display, font, out) <= max_width) {
            return;
        }
        out[cut] = '\0';
    }
    out[0] = '\0';
}
