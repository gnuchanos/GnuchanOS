/*
 * fetch_image.h — a picture read from a file, as plain pixels.
 *
 * The picture is loaded once and turned into a flat array of RGBA pixels here,
 * so that nothing further down needs to know about Imlib2, a file format, or a
 * display. fetch_draw.c is the only reader: it turns the pixels into text, and
 * this is the shape it expects — pixels in rows, top to bottom, and a height
 * that is an even number, because one text row of a terminal shows two pixel
 * rows (see fetch_draw.c).
 *
 * The alpha of a pixel is kept rather than resolved here, because whether it
 * matters is the drawer's question: a see-through logo is composited over the
 * background colour the settings name, and this module has no settings.
 */
#ifndef GNUCHANFETCH_IMAGE_H
#define GNUCHANFETCH_IMAGE_H

typedef struct FetchPixel {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} FetchPixel;

typedef struct FetchImage {
    /* The pixels, `height` rows of `width`, top row first. Owned here and freed
       by fetch_image_free(); NULL when no picture is loaded. */
    FetchPixel *pixels;

    int width;    /* in pixels, from the picture's own proportions */
    int height;   /* in pixels; always even — two pixel rows per text row */

    /* Whether a picture is loaded and ready to draw. A machine whose picture
       could not be read leaves this 0, and the facts are printed on their own
       rather than the program stopping. */
    int ok;
} FetchImage;

/* Load the picture at `path` scaled to `rows` text rows: the pixel height is
   `rows * 2`, and the width follows the picture's own shape so it is never
   stretched. A leading "~" in `path` is the home directory, and a relative name
   is looked for beside the settings script first. Returns 0 on success, -1 when
   the file cannot be found or read, in which case `image` is left not-ok.

   The caller owns `image` and must have zeroed it (or freed it) first. */
int fetch_image_load(FetchImage *image, const char *path, int rows);

/* Release the pixels and mark the image empty. Safe on an empty image. */
void fetch_image_free(FetchImage *image);

#endif /* GNUCHANFETCH_IMAGE_H */
