/*
 * fetch_draw.h — the whole output, drawn to standard output.
 *
 * One function, because the printing is one act: it takes the settings and the
 * picture that was loaded from them and writes the finished block — the picture
 * as text on the left, the facts on the right — to stdout. It returns nothing,
 * because there is nothing for a caller to do with a fetch program's output but
 * let it stand: a picture that would not load is already not-drawn (see
 * fetch_image.h), and the facts are printed either way.
 */
#ifndef GNUCHANFETCH_DRAW_H
#define GNUCHANFETCH_DRAW_H

#include "fetch_config.h"
#include "fetch_image.h"

/* Print the whole thing: the picture, when `image` holds one, and the fields
   the settings name, in the order they are named. Colours are written as 24-bit
   escapes.

   `use_graphics` is what the picture becomes. When it is 0 — a terminal that is
   not GnuChanTerm — the picture is drawn with half-block characters, which is
   what every terminal understands. When it is 1 — GnuChanTerm, which draws a
   picture as pixels — the picture is handed to the terminal with the OSC 1338
   escape and drawn as the image it is, and the fields are printed beside it.
   See fetch_draw.c and term_image.h. */
void fetch_draw(const FetchConfig *config, const FetchImage *image,
                int use_graphics);

#endif /* GNUCHANFETCH_DRAW_H */
