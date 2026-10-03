/*
 * fetch_color.h — a colour written as text, as three numbers.
 *
 * The settings file writes colours the way every style file on this desktop
 * does — "#rrggbb", with the "#" optional and three digits accepted as a short
 * form of six — and both the swatch and the picture's background need them as
 * plain numbers. This is the one place that reading happens, so the two callers
 * agree on what a colour is.
 *
 * An unclearable colour is not an error worth stopping for: parse_color answers
 * with a fallback, because a swatch with one bad entry still draws the rest and
 * a background that cannot be read is the same as no background at all.
 */
#ifndef GNUCHANFETCH_COLOR_H
#define GNUCHANFETCH_COLOR_H

typedef struct FetchColor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} FetchColor;

/* Parse "#rgb", "#rrggbb", "rgb" or "rrggbb" into a colour. Anything else, and
   a NULL, gives the fallback. */
FetchColor fetch_color_parse(const char *text, FetchColor fallback);

/* A readable default: the desktop's darkest purple, used when a colour cannot be
   read and a value is still needed. */
FetchColor fetch_color_default(void);

#endif /* GNUCHANFETCH_COLOR_H */
