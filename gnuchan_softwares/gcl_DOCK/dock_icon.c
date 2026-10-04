/*
 * dock_icon.c — a picture from a file or a window, as one pixmap.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Imlib2.h>

#include "dock_config.h"
#include "dock_icon.h"
#include "dock_theme.h"

#define DOCK_ICON_MAX_CARDS 32768
#define DOCK_ICON_MAX_SIDE 128

/* How far a pixel may sit from a file's corner colour and still be read as that
   file's background, when it brought no transparency of its own. Wide enough
   for the slight shading a flat field carries, tight enough that a drawn shape
   is not eaten into. */
#define DOCK_ICON_KEY_TOLERANCE 32

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

static void unpack_background(Visual *visual, unsigned long background,
                              unsigned int *red, unsigned int *green,
                              unsigned int *blue) {
    int shift = 0;
    int bits = 0;
    unsigned long span;
    mask_span(visual->red_mask, &shift, &bits);
    span = bits ? ((1UL << bits) - 1) : 0;
    *red = bits ? (unsigned int)(((background >> shift) & span) << (8 - bits)) : 0;
    mask_span(visual->green_mask, &shift, &bits);
    span = bits ? ((1UL << bits) - 1) : 0;
    *green = bits ? (unsigned int)(((background >> shift) & span) << (8 - bits)) : 0;
    mask_span(visual->blue_mask, &shift, &bits);
    span = bits ? ((1UL << bits) - 1) : 0;
    *blue = bits ? (unsigned int)(((background >> shift) & span) << (8 - bits)) : 0;
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
    char config_path[DOCK_TEXT_LENGTH];
    dock_config_path(config_path, sizeof(config_path));
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

/* The gap between two channel values, without a sign. */
static unsigned int difference(unsigned int left, unsigned int right) {
    return left > right ? left - right : right - left;
}

/* A file with no transparency of its own — the settings gear is drawn on a
   flat white field — would otherwise be pasted as a square of that field over
   the dock. The file's background is the colour its corner carries, so a pixel
   close to that colour is turned transparent and the drawn shape is left. Only
   done when the file brought no alpha of its own: a picture that already says
   what is transparent is believed, and nothing is guessed at. */
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

    /* The alpha byte has to be made real before it can carry anything, and the
       channel has to exist before the pixels are read — set_has_alpha() gives
       the image its alpha, reallocating the pixel store, so it is called first
       and the data fetched after, never the other way round. An image loaded
       without one has its top byte ignored, which is why zeroing the
       background's colour alone showed black: the alpha said "invisible" and
       nothing read it. */
    imlib_image_set_has_alpha(1);

    DATA32 *data = imlib_image_get_data();
    if (!data) {
        return;
    }
    unsigned int red = (data[0] >> 16) & 0xff;
    unsigned int green = (data[0] >> 8) & 0xff;
    unsigned int blue = data[0] & 0xff;

    size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; i++) {
        unsigned int r = (data[i] >> 16) & 0xff;
        unsigned int g = (data[i] >> 8) & 0xff;
        unsigned int b = data[i] & 0xff;
        if (difference(r, red) <= DOCK_ICON_KEY_TOLERANCE &&
            difference(g, green) <= DOCK_ICON_KEY_TOLERANCE &&
            difference(b, blue) <= DOCK_ICON_KEY_TOLERANCE) {
            data[i] = 0;                 /* the field: fully clear   */
        } else {
            data[i] |= 0xff000000u;      /* the shape: fully solid   */
        }
    }
    imlib_image_put_back_data(data);
}

/* A window's own class — the res_class half of WM_CLASS, "GnuChanTerm" — the
   name a program is installed under. Returns 1 when the window carried one. */
static int dock_icon_window_class(Display *display, Window client, char *out,
                                  unsigned int size) {
    if (out && size) {
        out[0] = '\0';
    }
    if (!display || !out || size == 0) {
        return 0;
    }
    XClassHint hint;
    memset(&hint, 0, sizeof(hint));
    if (!XGetClassHint(display, client, &hint)) {
        return 0;
    }
    if (hint.res_class) {
        snprintf(out, size, "%s", hint.res_class);
        XFree(hint.res_class);
    }
    if (hint.res_name) {
        XFree(hint.res_name);
    }
    return out[0] ? 1 : 0;
}

static int class_is_gnuchan(const char *name) {
    return name && strncasecmp(name, "GnuChan", 7) == 0;
}

/* The desktop's own logo, which every GnuchanOS program shares: the same file
   the terminal slot is drawn from. A GnuchanOS program installs no icon of its
   own, so this is what it should wear. */
static int gnuchan_logo_path(char *out, unsigned int size) {
    char config[DOCK_TEXT_LENGTH];
    dock_config_path(config, sizeof(config));
    char *slash = strrchr(config, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    snprintf(out, size, "%s/logo.png", config);
    return access(out, R_OK) == 0;
}

int dock_icon_load_file(Display *display, Window root, Visual *visual,
                        int depth, DockIcon *icon, const char *name, int side,
                        unsigned long background) {
    if (!display || !icon || side <= 0) {
        return -1;
    }
    dock_icon_free(display, icon);

    char path[DOCK_TEXT_LENGTH * 2];
    if (!resolve_path(name, path, sizeof(path))) {
        return -1;
    }

    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(DefaultColormap(display, DefaultScreen(display)));

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

    icon->pixmap = pixmap;
    icon->side = side;
    icon->ok = 1;
    return 0;
}

static unsigned long icon_best(const unsigned long *cards, unsigned long items,
                               unsigned long *side_out) {
    unsigned long best = 0;
    unsigned long best_side = 0;
    unsigned long best_area = 0;

    for (unsigned long off = 0; off + 2 <= items;) {
        unsigned long side = cards[off];
        unsigned long other = cards[off + 1];
        if (side == 0 || side != other) {
            break;
        }
        if (side > DOCK_ICON_MAX_SIDE) {
            break;
        }
        unsigned long area = side * side;
        if (area > items || off + 2 + area > items) {
            break;
        }
        if (area > best_area) {
            best_area = area;
            best_side = side;
            best = off;
        }
        off += 2 + area;
    }

    *side_out = best_side;
    return best;
}

/* What to draw when a window published no icon of its own — which is the rule
   for the GnuchanOS programs. The window's class is asked what it is: a
   GnuchanOS program wears the desktop's logo, and anything else is looked up
   the way the desktop would, through its .desktop entry and the icon theme.
   Returns 0 when a picture was found, -1 when nothing was. */
static int window_fallback(Display *display, Window root, Visual *visual,
                           int depth, DockIcon *icon, Window client, int side,
                           unsigned long background) {
    char wm_class[DOCK_TEXT_LENGTH];
    if (!dock_icon_window_class(display, client, wm_class, sizeof(wm_class))) {
        return -1;
    }
    if (class_is_gnuchan(wm_class)) {
        char logo[DOCK_TEXT_LENGTH * 2];
        if (gnuchan_logo_path(logo, sizeof(logo))) {
            return dock_icon_load_file(display, root, visual, depth, icon,
                                       logo, side, background);
        }
    }
    char theme_path[DOCK_THEME_TEXT];
    if (dock_theme_icon_for_class(wm_class, side, theme_path,
                                  sizeof(theme_path))) {
        return dock_icon_load_file(display, root, visual, depth, icon,
                                   theme_path, side, background);
    }
    return -1;
}

int dock_icon_load_window(Display *display, Window root, Visual *visual,
                          int depth, DockIcon *icon, Window client, int side,
                          unsigned long background) {
    if (!display || !icon || side <= 0) {
        return -1;
    }
    dock_icon_free(display, icon);

    Atom icon_atom = XInternAtom(display, "_NET_WM_ICON", False);
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(display, client, icon_atom, 0, DOCK_ICON_MAX_CARDS,
                           False, AnyPropertyType, &actual_type,
                           &actual_format, &items, &after, &data) != Success) {
        return window_fallback(display, root, visual, depth, icon, client,
                               side, background);
    }
    if (!data || actual_format != 32 || items < 2) {
        if (data) {
            XFree(data);
        }
        return window_fallback(display, root, visual, depth, icon, client,
                               side, background);
    }

    unsigned long source_side = 0;
    unsigned long base = icon_best((const unsigned long *)data, items,
                                   &source_side);
    if (source_side == 0) {
        XFree(data);
        return window_fallback(display, root, visual, depth, icon, client,
                               side, background);
    }

    Pixmap pixmap = XCreatePixmap(display, root, (unsigned int)side,
                                  (unsigned int)side, (unsigned int)depth);
    if (pixmap == None) {
        XFree(data);
        return -1;
    }

    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XSetForeground(display, gc, background);
    XFillRectangle(display, pixmap, gc, 0, 0, (unsigned int)side,
                   (unsigned int)side);

    unsigned int br = 0;
    unsigned int bg = 0;
    unsigned int bb = 0;
    unpack_background(visual, background, &br, &bg, &bb);

    const unsigned long *cards = (const unsigned long *)data;

    /* The whole icon is built in memory and put down in ONE request.
     *
     * This is not a micro-optimisation, it is the difference between a dock
     * that works and one that takes the session down. The icon used to be
     * drawn a pixel at a time with XDrawPoint — a `side` square is a few
     * thousand SEPARATE requests to the server — and every one of them is
     * copied into the X server's GL command buffer by its 2D acceleration.
     * A burst of a few thousand requests from one window (chromium, whose
     * published icon is large) was enough to fill and hang that buffer on an
     * old Intel GPU: the display went black and the window manager was gone
     * with it. The pixels are identical; there is now one XPutImage instead of
     * one request per pixel. */
    XImage *canvas = XCreateImage(display, visual, (unsigned int)depth,
                                  ZPixmap, 0, NULL, (unsigned int)side,
                                  (unsigned int)side, 32, 0);
    if (!canvas) {
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        XFree(data);
        return -1;
    }
    canvas->data = calloc((size_t)side, (size_t)canvas->bytes_per_line);
    if (!canvas->data) {
        XDestroyImage(canvas);
        XFreeGC(display, gc);
        XFreePixmap(display, pixmap);
        XFree(data);
        return -1;
    }

    for (int y = 0; y < side; y++) {
        unsigned long sy = (unsigned long)y * source_side / (unsigned long)side;
        for (int x = 0; x < side; x++) {
            unsigned long sx = (unsigned long)x * source_side / (unsigned long)side;
            unsigned long argb = cards[base + 2 + sy * source_side + sx];
            unsigned int alpha = (unsigned int)((argb >> 24) & 0xff);
            unsigned int red = (unsigned int)((argb >> 16) & 0xff);
            unsigned int green = (unsigned int)((argb >> 8) & 0xff);
            unsigned int blue = (unsigned int)(argb & 0xff);

            if (alpha != 0xff) {
                red = (unsigned int)((red * alpha + br * (255 - alpha)) / 255);
                green = (unsigned int)((green * alpha + bg * (255 - alpha)) / 255);
                blue = (unsigned int)((blue * alpha + bb * (255 - alpha)) / 255);
            }
            XPutPixel(canvas, x, y,
                      pixel_from_masks(visual, red, green, blue));
        }
    }

    XPutImage(display, pixmap, gc, canvas, 0, 0, 0, 0,
              (unsigned int)side, (unsigned int)side);
    XDestroyImage(canvas);

    XFreeGC(display, gc);
    XFree(data);

    icon->pixmap = pixmap;
    icon->side = side;
    icon->ok = 1;
    return 0;
}

void dock_icon_free(Display *display, DockIcon *icon) {
    if (!icon) {
        return;
    }
    if (display && icon->pixmap != None) {
        XFreePixmap(display, icon->pixmap);
    }
    icon->pixmap = None;
    icon->side = 0;
    icon->ok = 0;
}

int dock_icon_draw(Display *display, GC gc, const DockIcon *icon,
                   Drawable target, int x, int y) {
    if (!icon || !icon->ok || icon->pixmap == None || icon->side <= 0) {
        return 0;
    }
    XCopyArea(display, icon->pixmap, target, gc, 0, 0,
              (unsigned int)icon->side, (unsigned int)icon->side, x, y);
    return 1;
}
