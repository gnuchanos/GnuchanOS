/*
 * wm_image.c — a picture from disk, made into something the bar can draw.
 *
 * Three steps, in order, and each one can fail on its own:
 *
 *   resolve  the name the script wrote into a path that exists
 *   decode   that file into pixels (wm_png.c does the reading)
 *   prepare  those pixels into an X pixmap at the bar's height, ready to tile
 *
 * The result is kept in WmImage, and the file's name and the height are kept
 * with it. That is what makes the once-a-second repaint cheap: the bar asks
 * for the same picture at the same height on every redraw, and the answer is
 * already there, so the file is not read and decoded once a second.
 *
 * Like an icon, a picture is kept as a colour pixmap and a 1-bit mask. A plain
 * pixmap has no alpha, so an image with a transparent part would otherwise be
 * drawn as a rectangle of whatever its transparent pixels were set to. The
 * mask is what lets the bar's own colour — or nothing at all — show through.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xutil.h>

#include "wm_core.h"
#include "wm_image.h"
#include "wm_png.h"

/* A picture wider than this at bar height is clipped rather than tiled for
   ever: a bar is a few thousand pixels at most, and an image wider than the
   screen is a mistake rather than a background. */
#define WM_IMAGE_MAX_WIDTH 8192

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
 * An absolute name is used as it is. A relative one is looked for beside the
 * settings script first — the script and its picture are written together, so
 * `BackgroundImage="BG.png"` means the BG.png next to GnuChanWM.py — and then
 * relative to the working directory, which is what a name in a shell command
 * would mean.
 *
 * Returns 1 when a readable file was found, 0 otherwise; `out` is filled
 * either way, so a caller that fails can say which path it tried. */
static int resolve_path(const char *name, char *out, unsigned int size) {
    if (!name || !name[0]) {
        out[0] = '\0';
        return 0;
    }

    /* An absolute path is the whole answer. */
    if (name[0] == '/') {
        snprintf(out, size, "%s", name);
        return file_readable(out);
    }

    /* Beside the settings script. The config path is asked for through the
       same function the window manager reads the script through, so the two
       cannot disagree about where the script is — and the picture is looked
       for where the script is. */
    char config_path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(config_path, sizeof(config_path));
    if (config_path[0]) {
        /* The directory part of the config path: everything up to the last
           slash. The script's own file name is dropped, because the picture
           is a sibling of the script, not of the directory above it. */
        char directory[WM_CONFIG_TEXT_LENGTH * 4];
        snprintf(directory, sizeof(directory), "%s", config_path);
        char *slash = strrchr(directory, '/');
        if (slash) {
            *slash = '\0';
            snprintf(out, size, "%s/%s", directory, name);
            if (file_readable(out)) {
                return 1;
            }
        }
    }

    /* Relative to where the window manager was started. Last, because a name
       the script wrote almost always means the one beside the script. */
    snprintf(out, size, "%s", name);
    return file_readable(out);
}

/* --- preparing the pixels -------------------------------------------------- */

/* Allocate a pixel on the server's colour map for one RGB triple, remembering
   what has been allocated before. A picture is thousands of pixels and a
   handful of colours, so the table is what keeps the once-per-picture load
   from being thousands of round trips. */
static unsigned long image_pixel(WmCore *core, unsigned int red,
                                 unsigned int green, unsigned int blue) {
    XColor colour;
    colour.red   = (unsigned short)(red * 257);   /* 8 bits to 16 */
    colour.green = (unsigned short)(green * 257);
    colour.blue  = (unsigned short)(blue * 257);
    colour.flags = DoRed | DoGreen | DoBlue;
    if (!XAllocColor(core->display,
                     DefaultColormap(core->display, core->screen), &colour)) {
        return BlackPixel(core->display, core->screen);
    }
    return colour.pixel;
}

/* Where a channel's bits begin in a visual's mask. A TrueColor visual stores a
   pixel as the three channels packed into its own fields, and its masks say
   where each field is: this is the shift to that field's first bit. */
static unsigned int image_mask_shift(unsigned long mask) {
    unsigned int shift = 0;
    if (mask == 0) {
        return 0;
    }
    while ((mask & 1ul) == 0) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

/* How wide a channel is: the run of set bits in its mask. */
static unsigned int image_mask_bits(unsigned long mask) {
    unsigned int bits = 0;
    mask >>= image_mask_shift(mask);
    while (mask & 1ul) {
        bits++;
        mask >>= 1;
    }
    return bits;
}

/* One 8-bit colour as the pixel the server stores for it.
 *
 * This is what makes a picture the size of the screen possible at all. Asking
 * the server for a pixel — XAllocColor — is a round trip, and a picture has as
 * many pixels as the screen; the reader that used to do this for every pixel
 * spent minutes on a single wallpaper. On a TrueColor visual there is nothing
 * to ask the server: the colour is the three bytes packed into the visual's
 * own fields, which is a few shifts and no traffic at all. Only a palette
 * display has to allocate, and this desktop is never one. */
static unsigned long image_pixel_for(WmCore *core, Visual *visual,
                                     unsigned int red, unsigned int green,
                                     unsigned int blue) {
    if (visual && visual->class == TrueColor) {
        unsigned int red_bits   = image_mask_bits(visual->red_mask);
        unsigned int green_bits = image_mask_bits(visual->green_mask);
        unsigned int blue_bits  = image_mask_bits(visual->blue_mask);

        /* A channel narrower than eight bits has to be cut down; a wider one
           is left-aligned by the shift and needs no more. */
        unsigned long packed = 0;
        packed |= (unsigned long)(red_bits >= 8 ? red : red >> (8 - red_bits))
                  << image_mask_shift(visual->red_mask);
        packed |= (unsigned long)(green_bits >= 8
                                      ? green : green >> (8 - green_bits))
                  << image_mask_shift(visual->green_mask);
        packed |= (unsigned long)(blue_bits >= 8
                                      ? blue : blue >> (8 - blue_bits))
                  << image_mask_shift(visual->blue_mask);
        return packed;
    }
    return image_pixel(core, red, green, blue);
}

/* Turn the decoded pixels into a colour pixmap and a mask, at the given size.
 *
 * The source is read at (x * source_width / width, y * source_height / height)
 * — nearest-neighbour, the same scale the icon reader uses. For a bar
 * background that is a smooth gradient or a logo the difference from a
 * smoother filter is not worth a second resampling path, and keeping one
 * scaler means the drawing code never has to know which of the two it got.
 *
 * A pixel whose alpha is nearly zero is left out of the mask, which is what
 * makes a picture with a transparent part sit on the bar rather than in a box. */
static void image_prepare(WmCore *core, WmImage *image,
                          const unsigned int *pixels, int source_width,
                          int source_height, int width, int height) {
    if (width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0) {
        return;
    }
    int depth = DefaultDepth(core->display, core->screen);
    Visual *visual = DefaultVisual(core->display, core->screen);

    /* The colour copy is built here, in memory, and put on the server in one
       request.
     *
     * It used to be drawn a pixel at a time with XDrawPoint, and for the
       desktop wallpaper — a picture as tall as the screen — that is over a
       million requests, and a walk of the colour table for every one of them.
       Nothing is on the screen until it finishes, and the server is busy
       servicing the flood while it runs, which is what left a session black
       while its desktop was being built. One XPutImage of an image this side
       has built is the same pixels in one round trip.
     *
     * XPutPixel is used rather than writing bytes into the buffer directly, so
       the byte order and the row padding are Xlib's answer rather than this
       file's guess at the server's format. */
    XImage *colour = XCreateImage(core->display, visual, (unsigned int)depth,
                                  ZPixmap, 0, NULL, (unsigned int)width,
                                  (unsigned int)height, 32, 0);
    if (!colour) {
        return;
    }

    /* The pixel buffer itself, when XCreateImage did not make one.
     *
     * Whether it does is a detail of the Xlib build: the image structure is
     * always made, but `data` is only allocated when this build of Xlib
     * chooses to. XPutPixel writes through that pointer, so a NULL one is a
     * write to nothing and the process dies on the first pixel of the first
     * picture — which is exactly where the desktop wallpaper is built. Sizing
     * it from the image's own bytes_per_line rather than from a guess keeps
     * the layout Xlib's answer. */
    if (!colour->data) {
        colour->data = calloc((size_t)colour->bytes_per_line *
                                  (size_t)colour->height, 1);
        if (!colour->data) {
            XDestroyImage(colour);
            return;
        }
    }

    /* The mask is one bit per pixel, packed the way a bitmap is: rows padded
       out to the server's own bitmap unit, the first pixel of a row in the
       first bit of that row, in the order the server reads bits — which is
       taken from the server rather than assumed. A pixel that is not solid is
       left clear, so a transparent part of the picture shows what is
       underneath instead of a rectangle of whatever the file held there. */
    int bitmap_unit = BitmapUnit(core->display);
    if (bitmap_unit <= 0) {
        bitmap_unit = 32;
    }
    int mask_stride =
        ((width + bitmap_unit - 1) / bitmap_unit) * (bitmap_unit / 8);
    unsigned char *mask_bits =
        calloc((size_t)mask_stride * (size_t)height, 1);
    if (!mask_bits) {
        XDestroyImage(colour);
        return;
    }
    int mask_lsb_first = BitmapBitOrder(core->display) != MSBFirst;

    for (int y = 0; y < height; y++) {
        int sy = y * source_height / height;
        const unsigned int *source_row = pixels + (long)sy * source_width;
        unsigned char *mask_row = mask_bits + (long)y * mask_stride;

        for (int x = 0; x < width; x++) {
            int sx = x * source_width / width;
            unsigned int argb = source_row[sx];
            unsigned int alpha = (argb >> 24) & 0xff;
            if (alpha < 0x20) {
                continue;
            }
            XPutPixel(colour, x, y,
                      image_pixel_for(core, visual,
                                      (argb >> 16) & 0xff,
                                      (argb >> 8) & 0xff,
                                      argb & 0xff));
            mask_row[x >> 3] |=
                (unsigned char)(mask_lsb_first ? (1u << (x & 7))
                                               : (1u << (7 - (x & 7))));
        }
    }

    Pixmap colour_pixmap = XCreatePixmap(core->display, core->root,
                                         (unsigned int)width,
                                         (unsigned int)height,
                                         (unsigned int)depth);
    Pixmap mask_pixmap = XCreatePixmap(core->display, core->root,
                                       (unsigned int)width,
                                       (unsigned int)height, 1);

    /* The mask image reads the buffer this file made, and the buffer is
       detached from it before the image is destroyed so that it is freed
       once, here, rather than twice. */
    XImage *mask = NULL;
    if (colour_pixmap != None && mask_pixmap != None) {
        mask = XCreateImage(core->display, visual, 1, XYBitmap, 0,
                            (char *)mask_bits, (unsigned int)width,
                            (unsigned int)height, bitmap_unit, mask_stride);
    }

    if (colour_pixmap == None || mask_pixmap == None || !mask) {
        if (colour_pixmap != None) {
            XFreePixmap(core->display, colour_pixmap);
        }
        if (mask_pixmap != None) {
            XFreePixmap(core->display, mask_pixmap);
        }
        if (mask) {
            mask->data = NULL;
            XDestroyImage(mask);
        }
        free(mask_bits);
        XDestroyImage(colour);
        return;
    }

    GC gc = XCreateGC(core->display, colour_pixmap, 0, NULL);
    XPutImage(core->display, colour_pixmap, gc, colour, 0, 0, 0, 0,
              (unsigned int)width, (unsigned int)height);
    XFreeGC(core->display, gc);

    GC mask_gc = XCreateGC(core->display, mask_pixmap, 0, NULL);
    XPutImage(core->display, mask_pixmap, mask_gc, mask, 0, 0, 0, 0,
              (unsigned int)width, (unsigned int)height);
    XFreeGC(core->display, mask_gc);

    mask->data = NULL;   /* the buffer is freed here, not by the image */
    XDestroyImage(mask);
    free(mask_bits);
    XDestroyImage(colour);   /* frees the pixel buffer XCreateImage made */

    image->pixmap = colour_pixmap;
    image->mask = mask_pixmap;
    image->width = width;
    image->height = height;
    image->ok = 1;
}

/* --- the public entry points ----------------------------------------------- */

int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_height) {
    if (!core || !image) {
        return -1;
    }

    /* The same name at the same height is already loaded: the bar redraws once
       a second and the file almost never changes, so the second read is the
       one thing this cache exists to prevent.
     *
     * The test is against the name as written, not the resolved path: the
     * caller passes "BG.png" on every repaint, while `path` holds the absolute
     * place that name was found. Comparing the two never matched, so the file
     * was read, decoded and re-uploaded once a second — the flicker this cache
     * was written to remove. */
    if (image->ok && image->height_loaded == target_height && name &&
        strcmp(image->source_name, name) == 0) {
        return 0;
    }

    char path[sizeof(image->path)];
    if (!resolve_path(name, path, sizeof(path)) || target_height <= 0) {
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->height_loaded = target_height;
        return -1;
    }

    unsigned int *pixels = NULL;
    int source_width = 0;
    int source_height = 0;
    if (wm_png_load(path, &pixels, &source_width, &source_height) != 0) {
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->height_loaded = target_height;
        return -1;
    }

    /* Scale to the bar's height, keeping proportions. A picture taller than it
       is wide becomes a narrow tile and is repeated, which is what keeps a
       logo square; one wider than the bar is scaled to the bar's width instead
       so a single wide image is not cut off at the edge. */
    int width = source_width * target_height / source_height;
    if (width < 1) {
        width = 1;
    }
    if (width > WM_IMAGE_MAX_WIDTH) {
        width = WM_IMAGE_MAX_WIDTH;
    }
    int height = target_height;

    wm_image_free(core, image);
    image_prepare(core, image, pixels, source_width, source_height, width, height);
    free(pixels);

    snprintf(image->path, sizeof(image->path), "%s", path);
    snprintf(image->source_name, sizeof(image->source_name), "%s",
             name ? name : "");
    image->height_loaded = target_height;
    return image->ok ? 0 : -1;
}

int wm_image_draw(WmCore *core, WmImage *image, Drawable target,
                  int x, int y, int width) {
    if (!core || !image || !image->ok || image->pixmap == None ||
        image->mask == None || image->width <= 0 || width <= 0) {
        return 0;
    }

    GC gc = core->gc;
    int drawn = 0;
    for (int at = 0; at < width; at += image->width) {
        int piece = image->width;
        if (at + piece > width) {
            piece = width - at;
        }
        /* The mask clips the colour copy, so a transparent pixel lets whatever
           is under the bar show through rather than a rectangle of the
           picture's own colour. The same source rectangle is copied at every
           step, so the last copy is a partial one — the rightmost tile is cut
           at the bar's edge. */
        XSetClipMask(core->display, gc, image->mask);
        XSetClipOrigin(core->display, gc, x + at, y);
        XCopyArea(core->display, image->pixmap, target, gc, 0, 0,
                  (unsigned int)piece, (unsigned int)image->height,
                  x + at, y);
        drawn = 1;
    }
    XSetClipMask(core->display, gc, None);
    XSetClipOrigin(core->display, gc, 0, 0);
    return drawn;
}

void wm_image_free(WmCore *core, WmImage *image) {
    if (!image) {
        return;
    }
    if (core && core->display) {
        if (image->pixmap != None) {
            XFreePixmap(core->display, image->pixmap);
        }
        if (image->mask != None) {
            XFreePixmap(core->display, image->mask);
        }
    }
    image->pixmap = None;
    image->mask = None;
    image->width = 0;
    image->height = 0;
    image->ok = 0;
}
