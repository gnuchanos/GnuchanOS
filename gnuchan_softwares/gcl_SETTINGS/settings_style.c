/*
 * settings_style.c — the panel's palette and fonts, resolved on this display.
 *
 * Two jobs, and both are the same kind: turn a name into the thing the server
 * draws with. A colour name — "#c77dff" — becomes a pixel; a font description
 * — "monospace:pixelsize=13" — becomes an open XftFont. Everything the window
 * draws through goes through here, so the palette lives in one place and a
 * theme change is a change to this file.
 */
#include <stdio.h>

#include "settings_style.h"

/* The panel's own palette, as written. The desktop's purples, the same ramp
   GnuChanWM and GnuChanDock are built from, so the control panel looks like
   the rest of the session. */
#define COLOUR_BACKGROUND     "#1a0b2e"
#define COLOUR_SIDEBAR        "#140826"
#define COLOUR_SIDEBAR_ACTIVE "#32143f"
#define COLOUR_PANEL          "#241033"
#define COLOUR_PANEL_EDGE     "#7b2cbf"
#define COLOUR_TEXT           "#e0c3fc"
#define COLOUR_TEXT_MUTED     "#9d7bba"
#define COLOUR_ACCENT         "#c77dff"
#define COLOUR_DANGER         "#ff5c8a"

/* The two fonts: the body text and the heavier one headings and the category
   titles are drawn in. Both are Xft descriptions, so they scale with the
   system's fixed-width font rather than naming a size in pixels that would be
   wrong on a dense screen. */
#define FONT_BODY "monospace:pixelsize=13"
#define FONT_BOLD "monospace:bold:pixelsize=16"

unsigned long settings_style_colour(Display *display, int screen,
                                    const char *name, unsigned long fallback) {
    if (!display || !name || !name[0]) {
        return fallback;
    }
    Colormap colormap = DefaultColormap(display, screen);
    XColor colour;
    XColor exact;
    if (XAllocNamedColor(display, colormap, name, &colour, &exact)) {
        return colour.pixel;
    }
    return fallback;
}

static XftFont *open_font(Display *display, int screen, const char *name,
                          const char *fallback) {
    XftFont *font = XftFontOpenName(display, screen, name);
    if (!font && fallback) {
        font = XftFontOpenName(display, screen, fallback);
    }
    return font;
}

int settings_style_load(SettingsStyle *style, Display *display, int screen) {
    style->background = 0x1a0b2e;
    style->sidebar = 0x140826;
    style->sidebar_active = 0x32143f;
    style->panel = 0x241033;
    style->panel_edge = 0x7b2cbf;
    style->text = 0xe0c3fc;
    style->text_muted = 0x9d7bba;
    style->accent = 0xc77dff;
    style->danger = 0xff5c8a;
    style->font = NULL;
    style->font_bold = NULL;

    if (!display) {
        return -1;
    }

    style->background =
        settings_style_colour(display, screen, COLOUR_BACKGROUND, style->background);
    style->sidebar =
        settings_style_colour(display, screen, COLOUR_SIDEBAR, style->sidebar);
    style->sidebar_active =
        settings_style_colour(display, screen, COLOUR_SIDEBAR_ACTIVE, style->sidebar_active);
    style->panel =
        settings_style_colour(display, screen, COLOUR_PANEL, style->panel);
    style->panel_edge =
        settings_style_colour(display, screen, COLOUR_PANEL_EDGE, style->panel_edge);
    style->text =
        settings_style_colour(display, screen, COLOUR_TEXT, style->text);
    style->text_muted =
        settings_style_colour(display, screen, COLOUR_TEXT_MUTED, style->text_muted);
    style->accent =
        settings_style_colour(display, screen, COLOUR_ACCENT, style->accent);
    style->danger =
        settings_style_colour(display, screen, COLOUR_DANGER, style->danger);

    style->font = open_font(display, screen, FONT_BODY, "fixed");
    style->font_bold = open_font(display, screen, FONT_BOLD, "fixed");
    if (!style->font || !style->font_bold) {
        settings_style_free(style, display);
        return -1;
    }
    return 0;
}

void settings_style_free(SettingsStyle *style, Display *display) {
    if (!style || !display) {
        return;
    }
    if (style->font) {
        XftFontClose(display, style->font);
        style->font = NULL;
    }
    if (style->font_bold) {
        XftFontClose(display, style->font_bold);
        style->font_bold = NULL;
    }
}
