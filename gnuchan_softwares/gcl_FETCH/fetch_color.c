/*
 * fetch_color.c — a colour written as text, as three numbers.
 *
 * The whole of this file is one reader, and it is here rather than beside either
 * of its callers because there are two of them: the swatch in fetch_draw.c and
 * the picture's background. They must agree on what "#d400ff" means, and the way
 * to make them agree is to have one place that says it.
 *
 * The forms accepted are the ones a person actually writes in a style file:
 * "#rrggbb", the same with three digits ("#f0a", short for "#ff00aa"), and the
 * same two without the hash, because a colour written in a call may be quoted
 * as either. Anything else, and a NULL, give the caller's fallback — a colour
 * that will not parse is not worth stopping a fetch program over.
 */
#include <ctype.h>
#include <string.h>

#include "fetch_color.h"

/* One hex digit as its value, or -1 when the character is not one. */
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* The value of a run of digits, or -1 when any of them is not hex. */
static int hex_run(const char *text, int count, int *out) {
    int value = 0;
    for (int i = 0; i < count; i++) {
        int digit = hex_digit(text[i]);
        if (digit < 0) {
            return 0;
        }
        value = (value << 4) | digit;
    }
    *out = value;
    return 1;
}

FetchColor fetch_color_parse(const char *text, FetchColor fallback) {
    if (!text) {
        return fallback;
    }

    /* The hash is optional, and only the first one is skipped: "##fff" is not a
       colour, and a leading space is not part of one either. */
    const char *digits = text;
    if (*digits == '#') {
        digits++;
    }

    size_t length = strlen(digits);
    FetchColor colour;

    if (length == 6) {
        int red, green, blue;
        if (hex_run(digits, 2, &red) && hex_run(digits + 2, 2, &green) &&
            hex_run(digits + 4, 2, &blue)) {
            colour.red = (unsigned char)red;
            colour.green = (unsigned char)green;
            colour.blue = (unsigned char)blue;
            return colour;
        }
    } else if (length == 3) {
        /* The short form: each digit is doubled, so "#f0a" is "#ff00aa". That
           is the same rule CSS uses, and the rule a person writing a colour by
           hand expects. */
        int red, green, blue;
        if (hex_run(digits, 1, &red) && hex_run(digits + 1, 1, &green) &&
            hex_run(digits + 2, 1, &blue)) {
            colour.red = (unsigned char)(red * 17);
            colour.green = (unsigned char)(green * 17);
            colour.blue = (unsigned char)(blue * 17);
            return colour;
        }
    }

    return fallback;
}

FetchColor fetch_color_default(void) {
    /* The desktop's darkest purple, the same value the shipped settings script
       names for a picture's background. It is written out here as numbers so
       that a caller with no colour at all still has one. */
    FetchColor colour;
    colour.red = 0x0b;
    colour.green = 0x03;
    colour.blue = 0x10;
    return colour;
}
