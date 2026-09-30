/*
 * dm_style.c — resolve the palette and load the fonts, from the settings
 * script where it named them and from the built-ins where it did not.
 *
 * Colours are named, not numbered: "#c77dff" is readable and reviewable in a
 * way 0x00c77dff is not, and XAllocNamedColor does the parsing. A colour the
 * server cannot allocate falls back to a safe pixel rather than leaving the
 * greeter with a black hole where a control should be.
 *
 * --- the built-ins live HERE, and only here ---
 *
 * This is the one file that knows what the login screen looks like when nobody
 * has configured it, and it is the same list it has always drawn. The config
 * (dm_config.c) carries only what the SCRIPT said, with an empty string for
 * every setting it did not name — so "the script did not name this colour" and
 * "the script named this colour" are two different states, and the default is
 * chosen here for the first.
 *
 * That split is what keeps a settings file from having to be a whole theme. A
 * script that names three colours gets those three and these seven; a machine
 * with no script gets only these.
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

/* The colour the script named for one surface, or the built-in when it named
   none. An empty string is "not named" — see dm_config.h — so this is the one
   place that decides between the two. */
static const char *pick(const char *written, const char *fallback) {
    return (written != NULL && written[0] != '\0') ? written : fallback;
}

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

/* Load a font: the name the script asked for first, then the built-in list.
 *
 * The order is the whole point. A script that names a font means it, so its
 * name is tried before anything else — but a name the server cannot open must
 * not stop the greeter, because a login screen that refuses to start over a
 * font is a machine nobody can log into. So the list follows, and the server's
 * own `fixed` is in every list as the last resort.
 *
 * Returns NULL only when not even the server's default exists, which does not
 * happen. */
static XFontStruct *font(Display *display, const char *preferred,
                         const char *const *names) {
    if (preferred != NULL && preferred[0] != '\0') {
        XFontStruct *loaded = XLoadQueryFont(display, preferred);
        if (loaded) {
            return loaded;
        }
        fprintf(stderr,
                "gnuchandm: cannot open the font '%s'; using a default\n",
                preferred);
    }
    for (int i = 0; names[i]; i++) {
        XFontStruct *loaded = XLoadQueryFont(display, names[i]);
        if (loaded) {
            return loaded;
        }
    }
    return NULL;
}

/* A measurement: the number the script wrote, or the built-in when it wrote
   none. Zero is the "not written" sentinel — see DmConfig — so it is what this
   tests for. */
static int length_of(const int written, const int fallback) {
    return written > 0 ? written : fallback;
}

int dm_style_load(DmStyle *style, Display *display, int screen,
                  const DmConfig *config) {
    /* A NULL config is a machine whose file was never read at all — and it must
       draw exactly what a config that named nothing would, which is what pick()
       gives it. The two are not told apart here on purpose: an unconfigured
       greeter and a configured one that names nothing are the same greeter. */
    const DmConfig *c = config;

    style->background = colour(display, screen,
        c ? pick(c->background, "#1a0b2e") : "#1a0b2e",
        BlackPixel(display, screen));
    style->panel = colour(display, screen,
        c ? pick(c->panel, "#32143f") : "#32143f",
        BlackPixel(display, screen));
    style->panel_edge = colour(display, screen,
        c ? pick(c->panel_edge, "#7b2cbf") : "#7b2cbf",
        WhitePixel(display, screen));
    style->field = colour(display, screen,
        c ? pick(c->field, "#241033") : "#241033",
        BlackPixel(display, screen));
    style->field_focus = colour(display, screen,
        c ? pick(c->field_focus, "#3a1a52") : "#3a1a52",
        BlackPixel(display, screen));
    style->text = colour(display, screen,
        c ? pick(c->text, "#e0c3fc") : "#e0c3fc",
        WhitePixel(display, screen));
    style->text_muted = colour(display, screen,
        c ? pick(c->text_muted, "#9d7bba") : "#9d7bba",
        WhitePixel(display, screen));
    style->accent = colour(display, screen,
        c ? pick(c->accent, "#c77dff") : "#c77dff",
        WhitePixel(display, screen));
    style->accent_dim = colour(display, screen,
        c ? pick(c->accent_dim, "#7b2cbf") : "#7b2cbf",
        WhitePixel(display, screen));
    style->danger = colour(display, screen,
        c ? pick(c->danger, "#ff5d8f") : "#ff5d8f",
        WhitePixel(display, screen));

    style->font_label = font(display, c ? c->font_label : NULL, FONT_LABEL);
    style->font_field = font(display, c ? c->font_field : NULL, FONT_FIELD);
    style->font_title = font(display, c ? c->font_title : NULL, FONT_TITLE);

    style->margin = c ? length_of(c->margin, 16) : 16;
    style->gap = c ? length_of(c->gap, 12) : 12;
    style->panel_width = c ? length_of(c->panel_width, 420) : 420;
    style->field_height = c ? length_of(c->field_height, 40) : 40;
    style->button_height = c ? length_of(c->button_height, 40) : 40;
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
