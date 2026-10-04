/* notif_image.c — a picture in a bubble, as one pixmap. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
/* XCreateImage, XPutPixel, XPutImage and XDestroyImage live here rather than
   in Xlib.h, which does not pull this in on its own. */
#include <X11/Xutil.h>
#include <Imlib2.h>

#include "notif_config.h"
#include "notif_image.h"

#define NOTIF_IMAGE_MAX_SIDE 512

static void mask_span(unsigned long mask, int *shift, int *bits) {
    int s = 0;
    int b = 0;
    if (mask != 0) {
        while ((mask & 1UL) == 0) {
            mask >>= 1;
            s++;
        }
        while (mask & 1UL) {
            mask >>= 1;
            b++;
        }
    }
    *shift = s;
    *bits = b;
}

static unsigned long channel_pixel(unsigned int value, int shift, int bits) {
    if (bits <= 0) {
        return 0;
    }
    if (bits >= 8) {
        return (unsigned long)value << (shift + (bits - 8));
    }
    return (unsigned long)(value >> (8 - bits)) << shift;
}

static unsigned long pixel_from_masks(Visual *visual, unsigned int red,
                                      unsigned int green, unsigned int blue) {
    int shift = 0;
    int bits = 0;
    unsigned long pixel = 0;
    mask_span(visual->red_mask, &shift, &bits);
    pixel |= channel_pixel(red, shift, bits);
    mask_span(visual->green_mask, &shift, &bits);
    pixel |= channel_pixel(green, shift, bits);
    mask_span(visual->blue_mask, &shift, &bits);
    pixel |= channel_pixel(blue, shift, bits);
    return pixel;
}

static unsigned int unmask_channel(unsigned long pixel, unsigned long mask) {
    int shift = 0;
    int bits = 0;
    mask_span(mask, &shift, &bits);
    if (bits <= 0) {
        return 0;
    }
    unsigned long span = (1UL << bits) - 1UL;
    unsigned long value = (pixel >> shift) & span;
    if (bits >= 8) {
        return (unsigned int)(value >> (bits - 8));
    }
    return (unsigned int)(value << (8 - bits));
}

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
            return access(out, R_OK) == 0;
        }
    }
    if (name[0] == '/') {
        snprintf(out, size, "%s", name);
        return access(out, R_OK) == 0;
    }
    char config_path[NOTIF_TEXT_LENGTH];
    notif_config_path(config_path, sizeof(config_path));
    char *slash = strrchr(config_path, '/');
    if (slash) {
        *slash = '\0';
        size_t used = strlen(config_path);
        if (used + 1 < size) {
            memcpy(out, config_path, used);
            out[used] = '/';
            snprintf(out + used + 1, size - used - 1, "%s", name);
            if (access(out, R_OK) == 0) {
                return 1;
            }
        }
    }
    snprintf(out, size, "%s", name);
    return access(out, R_OK) == 0;
}

static unsigned int difference(unsigned int left, unsigned int right) {
    return left > right ? left - right : right - left;
}

/* A file with no transparency of its own — an icon drawn on a flat field —
   would otherwise be pasted as a square of that field over the bubble. The
   field's colour is the colour the top-left pixel carries, so a pixel close to
   it is turned transparent and the drawn shape is left. Only done when the file
   brought no alpha of its own. */
static void key_out_background(Imlib_Image image) {
    imlib_context_set_image(image);
    if (imlib_image_has_alpha()) {
        return;
    }
    int width = imlib_image_get_width();
    int height = imlib_image_get_height();
    if (width < 1 || height < 1) {
        return;
    }

    imlib_image_set_has_alpha(1);

    DATA32 *data = imlib_image_get_data();
    if (!data) {
        return;
    }
    unsigned int red = (data[0] >> 16) & 0xff;
    unsigned int green = (data[0] >> 8) & 0xff;
    unsigned int blue = data[0] & 0xff;

    const unsigned int tolerance = 32;
    size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; i++) {
        unsigned int r = (data[i] >> 16) & 0xff;
        unsigned int g = (data[i] >> 8) & 0xff;
        unsigned int b = data[i] & 0xff;
        if (difference(r, red) <= tolerance &&
            difference(g, green) <= tolerance &&
            difference(b, blue) <= tolerance) {
            data[i] = 0;
        } else {
            data[i] |= 0xff000000u;
        }
    }
    imlib_image_put_back_data(data);
}

int notif_image_load_file(Display *display, Window root, Visual *visual,
                          int depth, NotifImage *image, const char *name,
                          int side, unsigned long background) {
    if (!display || !image || side <= 0) {
        return -1;
    }
    notif_image_free(display, image);

    char path[NOTIF_TEXT_LENGTH * 2];
    if (!resolve_path(name, path, sizeof(path))) {
        return -1;
    }

    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(DefaultColormap(display,
                                               DefaultScreen(display)));

    Imlib_Load_Error error = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image loaded = imlib_load_image_with_error_return(path, &error);
    if (!loaded || error != IMLIB_LOAD_ERROR_NONE) {
        if (loaded) {
            imlib_context_set_image(loaded);
            imlib_free_image();
        }
        return -1;
    }

    key_out_background(loaded);

    Pixmap pixmap = XCreatePixmap(display, root, (unsigned int)side,
                                  (unsigned int)side, (unsigned int)depth);
    if (pixmap == None) {
        imlib_context_set_image(loaded);
        imlib_free_image();
        return -1;
    }

    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XSetForeground(display, gc, background);
    XFillRectangle(display, pixmap, gc, 0, 0, (unsigned int)side,
                   (unsigned int)side);
    XFreeGC(display, gc);

    imlib_context_set_image(loaded);
    imlib_context_set_drawable(pixmap);
    imlib_context_set_blend(1);
    imlib_render_image_on_drawable_at_size(0, 0, side, side);
    imlib_free_image_and_decache();

    image->pixmap = pixmap;
    image->side = side;
    image->ok = 1;
    return 0;
}

int notif_image_load_argb(Display *display, Window root, Visual *visual,
                          int depth, NotifImage *image, const long *cards,
                          int card_count, int width, int height, int side,
                          unsigned long background) {
    if (!display || !image || side <= 0 || !cards) {
        return -1;
    }
    notif_image_free(display, image);

    if (width <= 0 || height <= 0 || width > NOTIF_IMAGE_MAX_SIDE ||
        height > NOTIF_IMAGE_MAX_SIDE) {
        return -1;
    }
    long pixels = (long)width * (long)height;
    if (pixels <= 0 || pixels > card_count) {
        return -1;
    }

    Pixmap pixmap = XCreatePixmap(display, root, (unsigned int)side,
                                  (unsigned int)side, (unsigned int)depth);
    if (pixmap == None) {
        return -1;
    }

    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XSetForeground(display, gc, background);
    XFillRectangle(display, pixmap, gc, 0, 0, (unsigned int)side,
                   (unsigned int)side);

    unsigned int br = unmask_channel(background, visual->red_mask);
    unsigned int bg = unmask_channel(background, visual->green_mask);
    unsigned int bb = unmask_channel(background, visual->blue_mask);

    /* The picture is built in memory and put down in ONE request rather than
     * drawn a pixel at a time.
     *
     * A bubble icon is a `side` square, so drawing it with XDrawPoint is a few
     * thousand SEPARATE requests to the server, and the server's 2D
     * acceleration copies every one of them into the GL command buffer it
     * shares with the rest of the session. A notification that carries an icon
     * — which is most of them — was therefore a burst big enough to fill and
     * hang that buffer on an old Intel GPU, and the display went black with
     * the window manager. It is exactly the fault the dock's
     * dock_icon_load_window() had, and it is fixed the same way: one XPutImage
     * instead of one request per pixel. */
    XImage *canvas = XCreateImage(display, visual, (unsigned int)depth,
                                  ZPixmap, 0, NULL, (unsigned int)side,
                                  (unsigned int)side, 32, 0);
    if (!canvas) {
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        return -1;
    }
    canvas->data = calloc((size_t)side, (size_t)canvas->bytes_per_line);
    if (!canvas->data) {
        XDestroyImage(canvas);
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        return -1;
    }

    for (int y = 0; y < side; y++) {
        long sy = (long)y * height / side;
        for (int x = 0; x < side; x++) {
            long sx = (long)x * width / side;
            unsigned long argb = (unsigned long)cards[sy * width + sx];
            unsigned int alpha = (unsigned int)((argb >> 24) & 0xff);
            unsigned int red = (unsigned int)((argb >> 16) & 0xff);
            unsigned int green = (unsigned int)((argb >> 8) & 0xff);
            unsigned int blue = (unsigned int)(argb & 0xff);

            if (alpha != 0xff) {
                red = (unsigned int)((red * alpha + br * (255 - alpha)) / 255);
                green = (unsigned int)((green * alpha +
                                        bg * (255 - alpha)) / 255);
                blue = (unsigned int)((blue * alpha +
                                       bb * (255 - alpha)) / 255);
            }
            XPutPixel(canvas, x, y,
                      pixel_from_masks(visual, red, green, blue));
        }
    }

    XPutImage(display, pixmap, gc, canvas, 0, 0, 0, 0,
              (unsigned int)side, (unsigned int)side);
    XDestroyImage(canvas);

    XFreeGC(display, gc);

    image->pixmap = pixmap;
    image->side = side;
    image->ok = 1;
    return 0;
}

void notif_image_free(Display *display, NotifImage *image) {
    if (!image) {
        return;
    }
    if (display && image->pixmap != None) {
        XFreePixmap(display, image->pixmap);
    }
    image->pixmap = None;
    image->side = 0;
    image->ok = 0;
}

int notif_image_draw(Display *display, GC gc, const NotifImage *image,
                     Drawable target, int x, int y) {
    if (!image || !image->ok || image->pixmap == None || image->side <= 0) {
        return 0;
    }
    XCopyArea(display, image->pixmap, target, gc, 0, 0,
              (unsigned int)image->side, (unsigned int)image->side, x, y);
    return 1;
}
