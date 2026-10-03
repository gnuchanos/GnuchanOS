/*
 * fetch_image.c — a picture read from a file, as plain pixels.
 *
 * Imlib2 reads the file, and this module scales it down and turns it into a
 * flat array of RGBA pixels for fetch_draw.c to turn into text. Nothing here
 * draws, and nothing here needs a display: a picture is decoded and read as
 * data, which is what lets GnuChanFetch run in a plain terminal with no X
 * server at all.
 *
 * --- why the scaling is done here, and not by Imlib2 ---
 *
 * A fetch program shows its picture at a few dozen pixels wide, so a logo that
 * is 400 pixels wide is reduced by a factor of ten or more. Imlib2's own
 * scaling is bilinear: each output pixel is a weighted sample of a handful of
 * input pixels near the centre of the region it covers. At a tenth of the size
 * that reads one pixel in ten and throws the other nine away, so the result
 * keeps the aliasing of the original — the little dark and light speckles of a
 * detailed logo land in the output as speckles — and the picture comes out as
 * noise rather than a smaller copy of itself.
 *
 * So the reduction is done here by AREA: every output pixel is the average of
 * all the input pixels that fall under it. Nothing is skipped, the speckles
 * average out, and what is left is the picture as it would look seen from
 * further away. It is a little more work than one call into Imlib2, and it is
 * the difference between a logo and a smudge.
 *
 * The average is taken over the colour WEIGHTED BY ALPHA, and the alpha is
 * averaged on its own — the same thing as averaging the premultiplied pixels
 * and then dividing the colour by the alpha. Averaging straight colour instead
 * would drag the colour of a fully transparent pixel (which is usually black,
 * or whatever the format left there) into the count and fringe a soft edge with
 * a dark line.
 *
 * --- why the empty border is cut off first ---
 *
 * A logo is almost never drawn edge to edge in its own file: the image is the
 * shape plus the padding the artist left around it, and that padding is fully
 * transparent. Scaling the file as it stands keeps that padding as empty ROWS at
 * the top and bottom of the drawn picture and empty columns at the sides, so the
 * logo sits in a band of nothing — smaller than it needs to be, and with a gap
 * above it. So the picture is trimmed to the pixels that are actually visible —
 * the transparent border is cut off — and only then reduced. The logo fills the
 * height it was asked for, and there is no empty row anywhere in it.
 *
 * The picture keeps its own proportions. The settings name a height in text
 * ROWS; the pixel height is twice that — a terminal cell shows two pixel rows
 * (see fetch_draw.c) — and the width is whatever that height makes it, so a logo
 * is never stretched.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <Imlib2.h>

#include "fetch_image.h"
#include "fetch_config.h"

/* --- where the file is ----------------------------------------------------- */

/* Whether a file can be opened for reading. */
static int file_readable(const char *path) {
    if (!path || !path[0]) {
        return 0;
    }
    return access(path, R_OK) == 0;
}

/* The path a picture name refers to, written into `out`.
 *
 * An absolute name is used as it is. A name beginning with "~" is relative to
 * the home directory, which is how the shipped script writes its picture —
 * `Image="~/.config/GnuChanFetch/logo.png"`. A relative name is looked for beside
 * the settings script first, because the script and its picture are written
 * together, and then relative to the working directory, which is what a name in
 * a shell command would mean. Returns 1 when a readable file was found, 0
 * otherwise; `out` is filled either way, so a caller that fails can say which
 * path it tried. */
static int resolve_path(const char *name, char *out, unsigned int size) {
    if (!name || !name[0]) {
        out[0] = '\0';
        return 0;
    }

    if (name[0] == '~' && (name[1] == '/' || name[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            const char *rest = name[1] == '/' ? name + 1 : "";
            snprintf(out, size, "%s%s", home, rest);
            if (file_readable(out)) {
                return 1;
            }
        }
    }

    if (name[0] == '/') {
        snprintf(out, size, "%s", name);
        return file_readable(out);
    }

    /* A relative name: beside the settings script first. */
    char config_path[FETCH_TEXT_LENGTH];
    fetch_config_path(config_path, sizeof(config_path));
    char *slash = strrchr(config_path, '/');
    if (slash) {
        *slash = '\0';
        size_t used = strlen(config_path);
        if (used + 1 < size) {
            memcpy(out, config_path, used);
            out[used] = '/';
            snprintf(out + used + 1, size - used - 1, "%s", name);
            if (file_readable(out)) {
                return 1;
            }
        }
    }

    snprintf(out, size, "%s", name);
    return file_readable(out);
}

/* --- the reduction, by area ------------------------------------------------ */

/* The box of pixels that are actually visible: the smallest rectangle holding
   every pixel whose alpha is not zero — the picture with the transparent border
   the artist left around it cut off. Written into the out parameters as a
   half-open rectangle; answers 0 when the picture is entirely transparent, in
   which case there is nothing to draw. */
static int visible_bounds(const DATA32 *source, int width, int height,
                          int *out_x0, int *out_y0, int *out_x1, int *out_y1) {
    int min_x = width;
    int min_y = height;
    int max_x = -1;
    int max_y = -1;

    for (int y = 0; y < height; y++) {
        const DATA32 *row = source + (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) {
            if (((row[x] >> 24) & 0xff) == 0) {
                continue;
            }
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (max_x < 0 || max_y < 0) {
        return 0;
    }
    *out_x0 = min_x;
    *out_y0 = min_y;
    *out_x1 = max_x + 1;
    *out_y1 = max_y + 1;
    return 1;
}

/* One output pixel: the average of the input pixels in [x0,x1) by [y0,y1),
   colour weighted by alpha and alpha averaged on its own — see the file comment
   for why. Writes the result into `out`. */
static void average_region(const DATA32 *source, int source_width,
                           int x0, int y0, int x1, int y1, FetchPixel *out) {
    unsigned long sum_alpha = 0;
    unsigned long sum_red = 0;
    unsigned long sum_green = 0;
    unsigned long sum_blue = 0;
    unsigned long count = 0;

    for (int y = y0; y < y1; y++) {
        const DATA32 *row = source + (size_t)y * (size_t)source_width;
        for (int x = x0; x < x1; x++) {
            unsigned int alpha = (row[x] >> 24) & 0xff;
            sum_alpha += alpha;
            sum_red += ((row[x] >> 16) & 0xff) * alpha;
            sum_green += ((row[x] >> 8) & 0xff) * alpha;
            sum_blue += (row[x] & 0xff) * alpha;
            count++;
        }
    }

    if (count == 0 || sum_alpha == 0) {
        out->red = 0;
        out->green = 0;
        out->blue = 0;
        out->alpha = 0;
        return;
    }

    out->red = (unsigned char)(sum_red / sum_alpha);
    out->green = (unsigned char)(sum_green / sum_alpha);
    out->blue = (unsigned char)(sum_blue / sum_alpha);
    out->alpha = (unsigned char)(sum_alpha / count);
}

/* Build the reduced picture: `target_width` by `target_height`, each output
   pixel the average of the input pixels under it. Only the region at
   [x_off,x_off+crop_width) by [y_off,y_off+crop_height) is read — the rest is
   the transparent border visible_bounds() cut off — and every pixel of that
   region is walked, so no part of the visible picture is skipped however far it
   is reduced. */
static FetchPixel *reduce_area(const DATA32 *source, int source_width,
                               int x_off, int y_off,
                               int crop_width, int crop_height,
                               int target_width, int target_height) {
    FetchPixel *pixels = malloc(sizeof(FetchPixel) *
                                (size_t)target_width * (size_t)target_height);
    if (!pixels) {
        return NULL;
    }

    for (int ty = 0; ty < target_height; ty++) {
        int y0 = (int)((long)ty * crop_height / target_height) + y_off;
        int y1 = (int)((long)(ty + 1) * crop_height / target_height) + y_off;
        if (y1 <= y0) {
            y1 = y0 + 1;
        }
        if (y1 > y_off + crop_height) {
            y1 = y_off + crop_height;
        }
        for (int tx = 0; tx < target_width; tx++) {
            int x0 = (int)((long)tx * crop_width / target_width) + x_off;
            int x1 = (int)((long)(tx + 1) * crop_width / target_width) + x_off;
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (x1 > x_off + crop_width) {
                x1 = x_off + crop_width;
            }
            average_region(source, source_width, x0, y0, x1, y1,
                           &pixels[(size_t)ty * target_width + tx]);
        }
    }
    return pixels;
}

/* --- the picture ----------------------------------------------------------- */

int fetch_image_load(FetchImage *image, const char *path, int rows) {
    if (!image) {
        return -1;
    }
    /* A fresh load replaces whatever was there, so nothing is held twice. */
    fetch_image_free(image);

    char resolved[FETCH_TEXT_LENGTH * 2];
    if (!resolve_path(path, resolved, sizeof(resolved))) {
        fprintf(stderr, "gnuchanfetch: image '%s' was not found\n",
                path ? path : "");
        return -1;
    }

    if (rows < 1) {
        rows = 1;
    }
    int target_height = rows * 2;

    Imlib_Load_Error error = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image loaded = imlib_load_image_with_error_return(resolved, &error);
    if (!loaded || error != IMLIB_LOAD_ERROR_NONE) {
        if (loaded) {
            imlib_context_set_image(loaded);
            imlib_free_image();
        }
        fprintf(stderr, "gnuchanfetch: image '%s' could not be read\n",
                resolved);
        return -1;
    }

    imlib_context_set_image(loaded);
    int source_width = imlib_image_get_width();
    int source_height = imlib_image_get_height();
    if (source_width < 1 || source_height < 1) {
        imlib_free_image();
        fprintf(stderr, "gnuchanfetch: image '%s' has no size\n", resolved);
        return -1;
    }

    /* The pixels as a flat ARGB array, read in place — nothing is copied out of
       Imlib2 until the reduction has averaged the region it needs. */
    DATA32 *source = imlib_image_get_data_for_reading_only();
    if (!source) {
        imlib_free_image();
        fprintf(stderr, "gnuchanfetch: image '%s' could not be read into "
                        "memory\n", resolved);
        return -1;
    }

    /* The transparent border the artist left around the artwork is cut off
       first — see the file comment — so the logo fills the height it was asked
       for instead of sitting in a band of empty rows. A picture that is entirely
       transparent has nothing to draw. */
    int crop_x0 = 0;
    int crop_y0 = 0;
    int crop_x1 = source_width;
    int crop_y1 = source_height;
    if (!visible_bounds(source, source_width, source_height,
                        &crop_x0, &crop_y0, &crop_x1, &crop_y1)) {
        imlib_free_image_and_decache();
        fprintf(stderr, "gnuchanfetch: image '%s' is empty (nothing is "
                        "opaque)\n", resolved);
        return -1;
    }
    int crop_width = crop_x1 - crop_x0;
    int crop_height = crop_y1 - crop_y0;

    /* The width follows from the VISIBLE picture's own shape at the height asked
       for, so the logo is never stretched — see the file comment. */
    int target_width = (int)((double)crop_width * (double)target_height /
                             (double)crop_height + 0.5);
    if (target_width < 1) {
        target_width = 1;
    }

    FetchPixel *pixels = reduce_area(source, source_width, crop_x0, crop_y0,
                                     crop_width, crop_height,
                                     target_width, target_height);

    /* The source is freed as soon as the reduced copy exists: a fetch program
       shows one picture one time, and holding the original too would be a
       second copy of a file that can be several megabytes. */
    imlib_free_image_and_decache();

    if (!pixels) {
        fprintf(stderr, "gnuchanfetch: out of memory reading '%s'\n", resolved);
        return -1;
    }

    image->pixels = pixels;
    image->width = target_width;
    image->height = target_height;
    image->ok = 1;
    return 0;
}

void fetch_image_free(FetchImage *image) {
    if (!image) {
        return;
    }
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
    image->ok = 0;
}
