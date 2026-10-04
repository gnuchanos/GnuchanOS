/*
 * term_vt_osc.c — the grammar of the OSC strings this terminal acts on.
 *
 * See term_vt_osc.h for what is here and why it is its own file. The short
 * version: the colours a program sets (OSC 4, 10, 11, 12, 104, 110-112) and the
 * picture it places (OSC 1338) are read by a grammar of their own, and that
 * grammar is kept with the family rather than among the cursor sequences.
 *
 * Nothing here draws and nothing here owns a palette: each string is parsed and
 * handed to the host callback the terminal installed, because what a colour or
 * a picture MEANS is the half of the terminal that has a style and a display.
 */
#include <string.h>

#include "term_vt_osc.h"

/* --- small helpers -------------------------------------------------------- */

static int osc_hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* A decimal integer at `*cursor`, advanced past it. Returns 0 and the number,
   or -1 when there is not a digit there. The parser's own parameters are read
   by the state machine; this is for the text INSIDE an OSC, which is not the
   same grammar. */
static int osc_parse_int(const char **cursor, int *out) {
    const char *p = *cursor;
    if (*p < '0' || *p > '9') {
        return -1;
    }
    int value = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        if (value > 100000) {
            value = 100000;
        }
        p++;
    }
    *cursor = p;
    *out = value;
    return 0;
}

/* One component of a colour, scaled from however many hex digits were written
   to the eight bits the palette holds. `digits` is 1 to 4 and the scaling is
   proportional: F and FF both mean full, which is what makes #abc and #aabbcc
   the same colour and not two that differ by rounding. */
static unsigned int osc_scale_component(unsigned int value, int digits) {
    unsigned int max = (1u << (4 * digits)) - 1u;
    if (max == 0) {
        return 0;
    }
    return (value * 255u) / max;
}

/* A colour written the way an OSC carries one: `rgb:RR/GG/BB`,
   `rgb:RRRR/GGGG/BBBB` with any even number of digits from one to four per
   component, or the `#` forms `#RGB`, `#RRGGBB` and `#RRRRGGGGBBBB`.
 *
 * Returns 0 and fills `out` with 0xRRGGBB, and `*end` with where the colour
 * stopped; -1 when the text is not a colour. `end` may be NULL when the caller
 * does not care where it ended. */
static int osc_parse_color(const char *text, const char **end, uint32_t *out) {
    if (text == NULL || out == NULL) {
        return -1;
    }
    const char *p = text;
    unsigned int comp[3];

    if (strncmp(p, "rgb:", 4) == 0) {
        p += 4;
        for (int i = 0; i < 3; i++) {
            unsigned int value = 0;
            int digits = 0;
            while (digits < 4) {
                int h = osc_hex((unsigned char)*p);
                if (h < 0) {
                    break;
                }
                value = value * 16u + (unsigned int)h;
                digits++;
                p++;
            }
            if (digits == 0) {
                return -1;
            }
            comp[i] = osc_scale_component(value, digits);
            if (i < 2) {
                if (*p != '/') {
                    return -1;
                }
                p++;
            }
        }
        *out = (comp[0] << 16) | (comp[1] << 8) | comp[2];
        if (end != NULL) {
            *end = p;
        }
        return 0;
    }

    if (*p == '#') {
        p++;
        const char *start = p;
        int digits = 0;
        while (osc_hex((unsigned char)*p) >= 0) {
            p++;
            digits++;
        }
        /* Three, six or twelve: one, two or four hex digits per component.
           Anything else is a colour this does not understand and is refused
           rather than guessed at. */
        if (digits != 3 && digits != 6 && digits != 12) {
            return -1;
        }
        int per = digits / 3;
        p = start;
        for (int i = 0; i < 3; i++) {
            unsigned int value = 0;
            for (int d = 0; d < per; d++) {
                value = value * 16u + (unsigned int)osc_hex((unsigned char)*p);
                p++;
            }
            comp[i] = osc_scale_component(value, per);
        }
        *out = (comp[0] << 16) | (comp[1] << 8) | comp[2];
        if (end != NULL) {
            *end = p;
        }
        return 0;
    }

    return -1;
}

/* --- the colours a program sets ------------------------------------------- */

int term_vt_handle_color_osc(TermVt *vt, const char *body) {
    const char *p = body;
    int code = 0;
    if (vt->host.color == NULL || osc_parse_int(&p, &code) != 0) {
        return 0;
    }

    /* OSC 4 — one or more `index;colour` pairs in the one string, which is how
       a program sets a whole colourscheme in a single write. A `?` where a
       colour goes is the program asking what the entry IS. */
    if (code == 4) {
        while (*p == ';') {
            p++;
            int index = 0;
            if (osc_parse_int(&p, &index) != 0 || *p != ';') {
                break;
            }
            p++;
            if (*p == '?') {
                p++;
                vt->host.color(vt->host.user, index, 0, TERM_VT_COLOR_QUERY);
                continue;
            }
            uint32_t rgb = 0;
            const char *colour_end = p;
            if (osc_parse_color(p, &colour_end, &rgb) != 0) {
                break;
            }
            vt->host.color(vt->host.user, index, rgb, TERM_VT_COLOR_SET);
            p = colour_end;
        }
        return 1;
    }

    /* OSC 10, 11 and 12 — the foreground, the background and the cursor. Each
       carries its colour after a semicolon, or a `?` to ask. */
    if (code == 10 || code == 11 || code == 12) {
        int slot = (code == 10) ? TERM_VT_COLOR_FOREGROUND
                 : (code == 11) ? TERM_VT_COLOR_BACKGROUND
                                : TERM_VT_COLOR_CURSOR;
        if (*p == ';') {
            p++;
            if (*p == '?') {
                vt->host.color(vt->host.user, slot, 0, TERM_VT_COLOR_QUERY);
            } else {
                uint32_t rgb = 0;
                const char *colour_end = p;
                if (osc_parse_color(p, &colour_end, &rgb) == 0) {
                    vt->host.color(vt->host.user, slot, rgb, TERM_VT_COLOR_SET);
                }
            }
        }
        return 1;
    }

    /* OSC 104 — put palette entries back. With an index it is that one; with
       none it is the whole palette, which is the one request that names every
       entry at once and so has a slot of its own. */
    if (code == 104) {
        if (*p != ';') {
            vt->host.color(vt->host.user, TERM_VT_COLOR_ALL_PALETTE, 0,
                           TERM_VT_COLOR_RESET);
            return 1;
        }
        while (*p == ';') {
            p++;
            int index = 0;
            if (osc_parse_int(&p, &index) != 0) {
                break;
            }
            vt->host.color(vt->host.user, index, 0, TERM_VT_COLOR_RESET);
        }
        return 1;
    }

    if (code == 110) {
        vt->host.color(vt->host.user, TERM_VT_COLOR_FOREGROUND, 0,
                       TERM_VT_COLOR_RESET);
        return 1;
    }
    if (code == 111) {
        vt->host.color(vt->host.user, TERM_VT_COLOR_BACKGROUND, 0,
                       TERM_VT_COLOR_RESET);
        return 1;
    }
    if (code == 112) {
        vt->host.color(vt->host.user, TERM_VT_COLOR_CURSOR, 0,
                       TERM_VT_COLOR_RESET);
        return 1;
    }

    return 0;
}

/* --- a picture placed in the terminal ------------------------------------- */

/* `ESC ] 1338 ; x ; y ; rows ; path BEL` — this terminal's own sequence for
 * putting a picture over the cells. See term_image.h for why it exists and for
 * what the numbers mean.
 *
 * The three numbers come first and the path is everything after the fourth
 * semicolon, taken as it is. That is the whole reason the path is last: a path
 * may hold a semicolon, an `=` or anything else, and a parser that split on
 * every one would cut a file's name in half. Taking the tail is what lets a
 * file be called whatever it is called.
 *
 * The callback is handed the four numbers and the path; the picture itself is
 * loaded by whoever owns the display, because a parser of bytes has no business
 * opening files. */
void term_vt_handle_image_osc(TermVt *vt, const char *body) {
    if (vt->host.image == NULL) {
        return;
    }
    const char *p = body;
    int code = 0;
    if (osc_parse_int(&p, &code) != 0 || code != 1338) {
        return;
    }

    /* xoff ; yoff ; cols ; rows — four numbers, then the path. */
    int values[4];
    for (int i = 0; i < 4; i++) {
        if (*p != ';') {
            return;
        }
        p++;
        if (osc_parse_int(&p, &values[i]) != 0) {
            return;
        }
    }
    if (*p != ';') {
        return;
    }
    p++;
    /* Everything left is the path, taken as it is. */
    vt->host.image(vt->host.user, values[0], values[1], values[2], values[3], p);
}
