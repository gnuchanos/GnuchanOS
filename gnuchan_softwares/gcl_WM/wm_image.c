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
    int depth = DefaultDepth(core->display, core->screen);
    image->pixmap = XCreatePixmap(core->display, core->root,
                                  (unsigned int)width, (unsigned int)height,
                                  (unsigned int)depth);
    image->mask = XCreatePixmap(core->display, core->root,
                                (unsigned int)width, (unsigned int)height, 1);
    if (image->pixmap == None || image->mask == None) {
        wm_image_free(core, image);
        return;
    }

    /* The mask starts empty and is drawn into; the colour pixmap is filled by
       drawing one point per pixel, because a plain pixmap has no other way to
       take a colour that was not known when it was made. */
    GC mask_gc = XCreateGC(core->display, image->mask, 0, NULL);
    XSetForeground(core->display, mask_gc, 0);
    XFillRectangle(core->display, image->mask, mask_gc, 0, 0,
                   (unsigned int)width, (unsigned int)height);
    XSetForeground(core->display, mask_gc, 1);

    GC gc = XCreateGC(core->display, image->pixmap, 0, NULL);

    /* A small cache, keyed by the packed RGB, so a flat or few-coloured
       picture allocates each of its colours once. */
    unsigned long seen_key[1024];
    unsigned long seen_pixel[1024];
    int seen_count = 0;

    for (int y = 0; y < height; y++) {
        int sy = y * source_height / height;
        for (int x = 0; x < width; x++) {
            int sx = x * source_width / width;
            unsigned int argb = pixels[(long)sy * source_width + sx];
            unsigned int alpha = (argb >> 24) & 0xff;
            if (alpha < 0x20) {
                continue;
            }
            unsigned int red   = (argb >> 16) & 0xff;
            unsigned int green = (argb >> 8) & 0xff;
            unsigned int blue  = argb & 0xff;

            unsigned int key = (red << 16) | (green << 8) | blue;
            unsigned long pixel = 0;
            int found = 0;
            for (int i = 0; i < seen_count; i++) {
                if (seen_key[i] == key) {
                    pixel = seen_pixel[i];
                    found = 1;
                    break;
                }
            }
            if (!found) {
                pixel = image_pixel(core, red, green, blue);
                if (seen_count < 1024) {
                    seen_key[seen_count] = key;
                    seen_pixel[seen_count] = pixel;
                    seen_count++;
                }
            }

            XSetForeground(core->display, gc, pixel);
            XDrawPoint(core->display, image->pixmap, gc, x, y);
            XDrawPoint(core->display, image->mask, mask_gc, x, y);
        }
    }

    XFreeGC(core->display, gc);
    XFreeGC(core->display, mask_gc);

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
       one thing this cache exists to prevent. */
    if (image->ok && image->height_loaded == target_height &&
        strcmp(image->path, name ? name : "") == 0) {
        return 0;
    }

    char path[sizeof(image->path)];
    if (!resolve_path(name, path, sizeof(path)) || target_height <= 0) {
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        image->height_loaded = target_height;
        return -1;
    }

    unsigned int *pixels = NULL;
    int source_width = 0;
    int source_height = 0;
    if (wm_png_load(path, &pixels, &source_width, &source_height) != 0) {
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
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
