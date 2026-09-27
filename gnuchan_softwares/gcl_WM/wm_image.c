/*
 * wm_image.c — a picture from disk, stretched to the size it is shown at.
 *
 * Three steps, in order, and each one can fail on its own:
 *
 *   resolve   the name the script wrote into a path that exists
 *   decode    that file into pixels (wm_png.c does the reading)
 *   prepare   stretch those pixels to the asked-for size, in one pixmap
 *
 * There is no mask and no alpha surface kept: a transparent pixel of the file
 * is blended with the desktop colour here, while the pixels are still in
 * memory, and the result is a plain colour pixmap the desktop copies to the
 * screen in one operation. The version before this kept a 1-bit mask beside
 * the colours and clipped through it when drawing, and that is what made a
 * wallpaper come out as coloured noise on a real X server — the mask is packed
 * the way the server's own bitmaps are, and that packing is the one part of
 * this that the code had to guess. A picture that is put on the screen by
 * being stretched has no use for it.
 *
 * Stretching, not tiling: the picture is made exactly the size of the surface
 * it is for, so a wallpaper fills the screen whatever its own proportions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xutil.h>

#include "wm_core.h"
#include "wm_image.h"
#include "wm_png.h"

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
 * `BackgroundImage="bg.png"` means the bg.png next to GnuChanWM.py — and then
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

    if (name[0] == '/') {
        snprintf(out, size, "%s", name);
        return file_readable(out);
    }

    char config_path[WM_CONFIG_TEXT_LENGTH * 4];
    wm_config_path(config_path, sizeof(config_path));
    if (config_path[0]) {
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

    snprintf(out, size, "%s", name);
    return file_readable(out);
}

/* --- turning pixels into a pixmap ------------------------------------------ */

/* Where a channel's bits begin in a visual's mask, and how wide the channel
   is. A TrueColor visual stores a pixel as the three channels packed into its
   own fields, and its masks say where each field is; these two read the shifts
   and widths out of it, so the packing is the server's answer and not this
   file's guess. */
static unsigned int mask_shift(unsigned long mask) {
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

static unsigned int mask_bits(unsigned long mask) {
    unsigned int bits = 0;
    mask >>= mask_shift(mask);
    while (mask & 1ul) {
        bits++;
        mask >>= 1;
    }
    return bits;
}

/* One colour, as the pixel the server stores for it. On a TrueColor visual
   this is a few shifts and no traffic at all, which is what lets a picture as
   large as the screen be built without a round trip per pixel. */
static unsigned long pixel_for(Visual *visual, unsigned int red,
                               unsigned int green, unsigned int blue) {
    unsigned int red_bits   = mask_bits(visual->red_mask);
    unsigned int green_bits = mask_bits(visual->green_mask);
    unsigned int blue_bits  = mask_bits(visual->blue_mask);

    unsigned long packed = 0;
    packed |= (unsigned long)(red_bits >= 8 ? red : red >> (8 - red_bits))
              << mask_shift(visual->red_mask);
    packed |= (unsigned long)(green_bits >= 8 ? green : green >> (8 - green_bits))
              << mask_shift(visual->green_mask);
    packed |= (unsigned long)(blue_bits >= 8 ? blue : blue >> (8 - blue_bits))
              << mask_shift(visual->blue_mask);
    return packed;
}

/* Build the pixmap: read the decoded pixels at the sample each output pixel
 * falls on — nearest neighbour — blend a transparent pixel with the desktop
 * colour, and put the whole thing on the server in one request.
 *
 * The blend is why there is no mask. A pixel the file left transparent is not
 * a hole to be cut at draw time; it is simply the desktop colour, decided
 * here, while the picture is still an array in this process. What comes out is
 * one opaque pixmap of exactly the size it will be shown at. */
static void image_prepare(WmCore *core, WmImage *image,
                          const unsigned int *pixels, int source_width,
                          int source_height, int width, int height) {
    if (width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0) {
        return;
    }
    int depth = DefaultDepth(core->display, core->screen);
    Visual *visual = DefaultVisual(core->display, core->screen);

    /* The desktop colour a transparent pixel becomes. It is what the desktop
       shows when there is no picture at all, so a picture with a transparent
       part sits on the desktop rather than in a box of its own. */
    unsigned long backdrop = core->style.background;

    XImage *canvas = XCreateImage(core->display, visual, (unsigned int)depth,
                                  ZPixmap, 0, NULL, (unsigned int)width,
                                  (unsigned int)height, 32, 0);
    if (!canvas) {
        return;
    }
    /* XCreateImage does not always allocate the pixel buffer: whether it does
       is a detail of the Xlib build, and XPutPixel writes through the pointer,
       so a NULL one is a write to nothing. Sizing it from the image's own
       bytes_per_line keeps the layout Xlib's answer. */
    if (!canvas->data) {
        canvas->data = calloc((size_t)canvas->bytes_per_line *
                                  (size_t)canvas->height, 1);
        if (!canvas->data) {
            XDestroyImage(canvas);
            return;
        }
    }

    for (int y = 0; y < height; y++) {
        int sy = y * source_height / height;
        const unsigned int *source_row = pixels + (long)sy * source_width;
        for (int x = 0; x < width; x++) {
            int sx = x * source_width / width;
            unsigned int argb = source_row[sx];
            unsigned int alpha = (argb >> 24) & 0xff;

            unsigned char red   = (unsigned char)((argb >> 16) & 0xff);
            unsigned char green = (unsigned char)((argb >> 8) & 0xff);
            unsigned char blue  = (unsigned char)(argb & 0xff);

            /* Each output pixel takes the source pixel it falls on whole: no
               filtering, so the picture keeps its own edges. A transparent
               source pixel is replaced by the desktop colour, whole, which is
               what makes a transparent part read as the desktop. */
            unsigned long value;
            if (alpha < 0x20) {
                value = backdrop;
            } else {
                value = pixel_for(visual, red, green, blue);
            }
            XPutPixel(canvas, x, y, value);
        }
    }

    Pixmap pixmap = XCreatePixmap(core->display, core->root,
                                  (unsigned int)width, (unsigned int)height,
                                  (unsigned int)depth);
    if (pixmap == None) {
        XDestroyImage(canvas);
        return;
    }

    GC gc = XCreateGC(core->display, pixmap, 0, NULL);
    XPutImage(core->display, pixmap, gc, canvas, 0, 0, 0, 0,
              (unsigned int)width, (unsigned int)height);
    XFreeGC(core->display, gc);
    XDestroyImage(canvas);   /* frees the pixel buffer XCreateImage made */

    image->pixmap = pixmap;
    image->width = width;
    image->height = height;
    image->ok = 1;
}

/* --- the public entry points ----------------------------------------------- */

int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_width, int target_height) {
    if (!core || !image) {
        return -1;
    }

    /* The same name at the same size is already loaded: the desktop repaints
       its background on every expose and the file almost never changes. The
       test is against the name as written, not the resolved path — the caller
       passes "bg.png" on every repaint while `path` holds the absolute place
       that name was found. */
    if (image->ok && image->width_loaded == target_width &&
        image->height_loaded == target_height && name &&
        strcmp(image->source_name, name) == 0) {
        return 0;
    }

    char path[sizeof(image->path)];
    if (!resolve_path(name, path, sizeof(path)) ||
        target_width <= 0 || target_height <= 0) {
        fprintf(stderr, "gnuchanwm: image '%s' not found (looked for '%s')\n",
                name ? name : "", path);
        wm_config_show_message(core,
                               "GnuChanWM: the wallpaper was not found",
                               name ? name : "(no name given)", path);
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->width_loaded = target_width;
        image->height_loaded = target_height;
        return -1;
    }

    unsigned int *pixels = NULL;
    int source_width = 0;
    int source_height = 0;
    if (wm_png_load(path, &pixels, &source_width, &source_height) != 0) {
        fprintf(stderr,
                "gnuchanwm: image '%s' could not be decoded "
                "(only 8-bit, non-interlaced PNG is read)\n", path);
        wm_config_show_message(core,
                               "GnuChanWM: the wallpaper could not be read",
                               "only 8-bit, non-interlaced PNG is read", path);
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->width_loaded = target_width;
        image->height_loaded = target_height;
        return -1;
    }

    wm_image_free(core, image);
    image_prepare(core, image, pixels, source_width, source_height,
                  target_width, target_height);
    free(pixels);

    if (!image->ok) {
        fprintf(stderr,
                "gnuchanwm: image '%s' (%dx%d) could not be prepared at %dx%d\n",
                path, source_width, source_height, target_width, target_height);
    }

    snprintf(image->path, sizeof(image->path), "%s", path);
    snprintf(image->source_name, sizeof(image->source_name), "%s",
             name ? name : "");
    image->width_loaded = target_width;
    image->height_loaded = target_height;
    return image->ok ? 0 : -1;
}

int wm_image_draw(WmCore *core, WmImage *image, Drawable target, int x, int y) {
    if (!core || !image || !image->ok || image->pixmap == None) {
        return 0;
    }
    XCopyArea(core->display, image->pixmap, target, core->gc, 0, 0,
              (unsigned int)image->width, (unsigned int)image->height, x, y);
    return 1;
}

void wm_image_free(WmCore *core, WmImage *image) {
    if (!image) {
        return;
    }
    if (core && core->display && image->pixmap != None) {
        XFreePixmap(core->display, image->pixmap);
    }
    image->pixmap = None;
    image->width = 0;
    image->height = 0;
    image->ok = 0;
}
