/*
 * dm_style.c — resolve the palette and load the fonts.
 *
 * Colours are named, not numbered: "#c77dff" is readable and reviewable in a
 * way 0x00c77dff is not, and XAllocNamedColor does the parsing. A colour the
 * server cannot allocate falls back to a safe pixel rather than leaving the
 * greeter with a black hole where a control should be.
 */
#include <stdio.h>

#include "dm_style.h"

/* The fonts, best first. Any of these is present on a normal Debian with
   xfonts-base; the X server's own default is the last resort. */
static const char *FONT_LABEL[] = {
    "-*-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1",
    "-*-dejavu sans-medium-r-normal--14-*-*-*-*-*-iso8859-1",
    "fixed",
    NULL,
};
static const char *FONT_FIELD[] = {
    "-*-helvetica-medium-r-normal--18-*-*-*-*-*-iso8859-1",
    "-*-dejavu sans-medium-r-normal--18-*-*-*-*-*-iso8859-1",
    "fixed",
    NULL,
};
static const char *FONT_TITLE[] = {
    "-*-helvetica-bold-r-normal--28-*-*-*-*-*-iso8859-1",
    "-*-dejavu sans-bold-r-normal--28-*-*-*-*-*-iso8859-1",
    "fixed",
    NULL,
};

/* Allocate one named colour. `fallback` is used when the server refuses it,
   which happens on a display with no colour at all. */
static unsigned long colour(Display *display, int screen, const char *name,
                            unsigned long fallback) {
    Colormap map = DefaultColormap(display, screen);
    XColor exact;
    XColor cell;
    if (XAllocNamedColor(display, map, name, &cell, &exact)) {
        return cell.pixel;
    }
    fprintf(stderr, "gnuchandm: cannot allocate %s; using a default\n", name);
    return fallback;
}

/* Load the first font in the list that the server can open. Returns NULL only
   when not even the server's default exists, which does not happen. */
static XFontStruct *font(Display *display, const char *const *names) {
    for (int i = 0; names[i]; i++) {
        XFontStruct *loaded = XLoadQueryFont(display, names[i]);
        if (loaded) {
            return loaded;
        }
    }
    return NULL;
}

int dm_style_load(DmStyle *style, Display *display, int screen) {
    style->background = colour(display, screen, "#1a0b2e", BlackPixel(display, screen));
    style->panel = colour(display, screen, "#32143f", BlackPixel(display, screen));
    style->panel_edge = colour(display, screen, "#7b2cbf", WhitePixel(display, screen));
    style->field = colour(display, screen, "#241033", BlackPixel(display, screen));
    style->field_focus = colour(display, screen, "#3a1a52", BlackPixel(display, screen));
    style->text = colour(display, screen, "#e0c3fc", WhitePixel(display, screen));
    style->text_muted = colour(display, screen, "#9d7bba", WhitePixel(display, screen));
    style->accent = colour(display, screen, "#c77dff", WhitePixel(display, screen));
    style->accent_dim = colour(display, screen, "#7b2cbf", WhitePixel(display, screen));
    style->danger = colour(display, screen, "#ff5d8f", WhitePixel(display, screen));

    style->font_label = font(display, FONT_LABEL);
    style->font_field = font(display, FONT_FIELD);
    style->font_title = font(display, FONT_TITLE);

    style->margin = 16;
    style->gap = 12;
    style->panel_width = 420;
    style->field_height = 40;
    style->button_height = 40;
    return 0;
}

void dm_style_free(DmStyle *style, Display *display) {
    if (style->font_label) {
        XFreeFont(display, style->font_label);
        style->font_label = NULL;
    }
    if (style->font_field) {
        XFreeFont(display, style->font_field);
        style->font_field = NULL;
    }
    if (style->font_title) {
        XFreeFont(display, style->font_title);
        style->font_title = NULL;
    }
}
