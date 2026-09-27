/*
 * wm_icon.c — a window's icon, read from the client and put on the bar.
 *
 * A window manager that lists the open programs needs something to list them
 * by. The title is a poor choice on a bar: two terminals both say "xterm", and
 * a title changes to whatever the program is doing, so a row of titles is a
 * row of sentences that rearrange themselves. An icon is the one thing a
 * program publishes about itself that is meant to identify it.
 *
 * The source is _NET_WM_ICON, which is how a client hands a window manager its
 * icons under the extended window manager hints. The property is a list, not
 * an image: each entry is a width, a height, and then width*height ARGB cards,
 * and a client may publish several sizes. The order is not agreed on, so the
 * largest one that is still no bigger than a bar icon is chosen rather than
 * the first or the last — that is what makes the same program look right
 * whether its toolkit lists 16 first or 256 first.
 *
 * A plain X pixmap has no alpha, so the icon is kept in two pieces: a colour
 * pixmap of the pixels and a 1-bit mask of where they are solid. The two are
 * clipped together when the icon is drawn, which is what puts the icon on the
 * bar's own colour instead of in a rectangle of whatever the client's image
 * carried. Scaling is done once, here, by nearest pixel when the image is
 * copied down to bar size; a plain pixmap cannot scale and the drawing code
 * never has to.
 *
 * --- why this file is written the way it is ------------------------------
 *
 * Everything a client sends here is untrusted: the property is a list the
 * program wrote, it can be any size, and any part of it can be wrong. And every
 * call that waits on the server — an XAllocColor, an XGetWindowProperty with a
 * long length — is a moment the window manager is not reading events, not
 * answering a map or a key, and not repainting anything. On a machine whose GPU
 * is already the weak part, a client that opens a window with a heavy or broken
 * icon is exactly the program that must not be allowed to freeze the desktop.
 *
 * So two rules run through this file:
 *
 *   1. No round trip in the drawing path. A pixel's colour is computed from the
 *      screen's own visual mask — the arithmetic a TrueColor screen defines and
 *      needs no server to answer — and the whole icon goes onto its pixmap in
 *      one XPutImage. The older code asked the server to allocate every distinct
 *      colour (up to 512 times) and drew every pixel with its own XDrawPoint;
 *      each alloc is a round trip and the whole thing ran inside the event loop,
 *      so a busy client made the manager stutter with every window it opened.
 *
 *   2. Read no more than a bar icon can be made from. The property is read with
 *      a ceiling on its length, and the length is the whole cost of the read,
 *      because XGetWindowProperty waits for all of it. Without a ceiling a
 *      client could make the manager pull an unbounded property in one call.
 */
#include <stdlib.h>
#include <string.h>

/* XCreateImage, XPutPixel, XPutImage and XDestroyImage live here rather than
   in Xlib.h, which does not pull this in on its own. */
#include <X11/Xutil.h>

#include "wm_core.h"
#include "wm_frame.h"

/* The most colours kept in the fallback table, for a screen that is not
   TrueColor. It is a table and not the drawing path: see
   pixel_from_palette(). */
#define WM_ICON_COLOUR_CACHE 256

typedef struct IconColour {
    unsigned int key;        /* (r << 16) | (g << 8) | b                    */
    unsigned long pixel;
} IconColour;

static IconColour icon_colours[WM_ICON_COLOUR_CACHE];
static int icon_colour_count = 0;

/* --- the pixels ----------------------------------------------------------- */

/* The number of bits the screen's mask keeps, and how far it is shifted. */
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

/* One 8-bit channel placed where the visual's mask wants it. */
static unsigned long channel_pixel(unsigned int value, int shift, int bits) {
    if (bits <= 0) {
        return 0;
    }
    if (bits >= 8) {
        return (unsigned long)value << (shift + (bits - 8));
    }
    return (unsigned long)(value >> (8 - bits)) << shift;
}

/* Whether the screen's visual is one whose pixel value can be computed from the
   mask. TrueColor and DirectColor both answer yes; a PseudoColor screen is a
   palette and has to be asked, which is the slow path below. */
static int visual_is_direct(Visual *visual) {
    if (!visual) {
        return 0;
    }
    if (visual->class != TrueColor && visual->class != DirectColor) {
        return 0;
    }
    int shift = 0;
    int bits = 0;
    mask_span(visual->red_mask, &shift, &bits);
    if (bits == 0) {
        return 0;
    }
    mask_span(visual->green_mask, &shift, &bits);
    if (bits == 0) {
        return 0;
    }
    mask_span(visual->blue_mask, &shift, &bits);
    return bits != 0;
}

/* A colour as a pixel, computed from the visual's masks — no server call. Only
   used for a direct-colour screen, which every screen that runs a compositor
   or a piece of GL software is. */
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

/* A colour as a pixel on a screen that is a palette rather than masks. This is
   the one place a round trip can happen, so it is kept off the common path: it
   is only reached on a PseudoColor screen, and the answers are remembered so a
   bar of icons costs a handful of allocations rather than one per pixel. */
static unsigned long pixel_from_palette(WmCore *core, unsigned int red,
                                        unsigned int green, unsigned int blue) {
    unsigned int key = (red << 16) | (green << 8) | blue;

    for (int i = 0; i < icon_colour_count; i++) {
        if (icon_colours[i].key == key) {
            return icon_colours[i].pixel;
        }
    }

    XColor colour;
    colour.red   = (unsigned short)(red * 257);   /* 8 bits to 16 */
    colour.green = (unsigned short)(green * 257);
    colour.blue  = (unsigned short)(blue * 257);
    colour.flags = DoRed | DoGreen | DoBlue;
    unsigned long pixel = BlackPixel(core->display, core->screen);
    if (XAllocColor(core->display,
                    DefaultColormap(core->display, core->screen), &colour)) {
        pixel = colour.pixel;
    }

    if (icon_colour_count < WM_ICON_COLOUR_CACHE) {
        icon_colours[icon_colour_count].key = key;
        icon_colours[icon_colour_count].pixel = pixel;
        icon_colour_count++;
    }
    return pixel;
}

/* --- the mask and the colours --------------------------------------------- */

/* Drop whatever icon this frame holds, so a client that changed its icon does
   not keep the old one beside the new. */
void wm_frame_free_icon(WmCore *core, WmFrame *frame) {
    if (frame->icon_source != None) {
        XFreePixmap(core->display, frame->icon_source);
        frame->icon_source = None;
    }
    if (frame->icon_mask != None) {
        XFreePixmap(core->display, frame->icon_mask);
        frame->icon_mask = None;
    }
    frame->icon_side = 0;
    frame->icon_ok = 0;
}

/* --- reading -------------------------------------------------------------- */

/* The best entry in a _NET_WM_ICON list: the one with the most pixels that is
   square and no larger than the ceiling. Returns the offset of its card pair
   and writes its side.
 *
 * The walk is the reason this is separate. The list is a run of (w, h, pixels)
 * triples, so where one image ends is only known by reading its width and
 * height first; a malformed list — a width of zero, or a height that is not
 * the width — is where the walk stops, because past it there is no way to know
 * where the next image begins.
 *
 * Every number here comes from the client and is treated as hostile. A width
 * larger than the ceiling is skipped without its area being multiplied out (a
 * width of four thousand million squares to a number that wraps, and a wrapped
 * area is an area that cannot be trusted to advance the walk), and the room a
 * triple claims is checked against what is left before the offset moves. The
 * walk therefore cannot leave the buffer and cannot run without end, whatever
 * the list says. */
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
        /* Past the ceiling, only the length of the triple can be checked, and
           that check is `side * side` itself — which would overflow. So the
           image is skipped by the one fact that is safe: an image larger than
           the ceiling is not wanted, and the list ends at the first triple
           that cannot be walked. */
        if (side > WM_ICON_MAX_SIDE) {
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

/* Copy one chosen image down to bar size, with no round trip to the server.
 *
 * The source is read at (x * source / side, y * source / side), which is
 * nearest-neighbour: for the sizes involved — a 48 pixel icon onto a 16 pixel
 * square — the difference from a smoother scale is a few pixels of edge, and
 * doing it here keeps every other part of the drawing code free of scaling. A
 * pixel whose alpha is nearly zero is left out of the mask, which is what
 * makes a rounded or irregular icon sit on the bar rather than in a box.
 *
 * Both images are built in memory and put on their pixmaps with one XPutImage
 * each. XPutPixel is what does the packing — the byte order and the bits per
 * pixel are the server's business and it answers them the same way here as it
 * would to any client — so this file never assumes a layout of its own. */
static void icon_scale(WmCore *core, WmFrame *frame, const unsigned long *cards,
                       unsigned long base, unsigned long source_side) {
    int side = WM_ICON_SIZE;
    int depth = DefaultDepth(core->display, core->screen);
    Visual *visual = DefaultVisual(core->display, core->screen);
    int direct = visual_is_direct(visual);

    /* An image per pixmap, both built in memory. The colour one holds a pixel
       per point and the mask one holds a single bit. */
    XImage *colour_image = XCreateImage(core->display, visual,
                                        (unsigned int)depth, ZPixmap, 0, NULL,
                                        (unsigned int)side, (unsigned int)side,
                                        32, 0);
    XImage *mask_image = XCreateImage(core->display, visual, 1, ZPixmap, 0,
                                      NULL, (unsigned int)side,
                                      (unsigned int)side, 8, 0);
    if (!colour_image || !mask_image) {
        if (colour_image) {
            XDestroyImage(colour_image);
        }
        if (mask_image) {
            XDestroyImage(mask_image);
        }
        return;
    }

    for (int y = 0; y < side; y++) {
        unsigned long sy =
            (unsigned long)y * source_side / (unsigned long)side;
        for (int x = 0; x < side; x++) {
            unsigned long sx =
                (unsigned long)x * source_side / (unsigned long)side;
            unsigned long argb = cards[base + 2 + sy * source_side + sx];
            unsigned int alpha = (unsigned int)((argb >> 24) & 0xff);
            if (alpha < 0x20) {
                /* A see-through pixel is left out of the mask, so the bar's own
                   colour shows there rather than a rectangle of the icon's. */
                XPutPixel(mask_image, x, y, 0);
                XPutPixel(colour_image, x, y, 0);
                continue;
            }
            unsigned int red   = (unsigned int)((argb >> 16) & 0xff);
            unsigned int green = (unsigned int)((argb >> 8) & 0xff);
            unsigned int blue  = (unsigned int)(argb & 0xff);

            unsigned long pixel = direct
                                      ? pixel_from_masks(visual, red, green, blue)
                                      : pixel_from_palette(core, red, green, blue);
            XPutPixel(colour_image, x, y, pixel);
            XPutPixel(mask_image, x, y, 1);
        }
    }

    GC gc = XCreateGC(core->display, frame->icon_source, 0, NULL);
    XPutImage(core->display, frame->icon_source, gc, colour_image, 0, 0, 0, 0,
              (unsigned int)side, (unsigned int)side);
    XFreeGC(core->display, gc);

    /* The mask needs a GC whose foreground is set, because XPutImage on a
       one-bit drawable draws the image through the GC's colours: a 1 in the
       image is the foreground and a 0 is the background. */
    GC mask_gc = XCreateGC(core->display, frame->icon_mask, 0, NULL);
    XSetForeground(core->display, mask_gc, 1);
    XSetBackground(core->display, mask_gc, 0);
    XPutImage(core->display, frame->icon_mask, mask_gc, mask_image, 0, 0, 0, 0,
              (unsigned int)side, (unsigned int)side);
    XFreeGC(core->display, mask_gc);

    /* XDestroyImage frees the data the image carried, which is why the two are
       destroyed here rather than left to the connection. */
    XDestroyImage(colour_image);
    XDestroyImage(mask_image);
}

void wm_frame_read_icon(WmCore *core, WmFrame *frame) {
    frame->icon_tried = 1;
    wm_frame_free_icon(core, frame);

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    Atom icon_atom = wm_atom(core, "_NET_WM_ICON");

    /* The ceiling is the whole cost of the read, because this call waits for
       the property as well as asking for it. WM_ICON_MAX_CARDS is sized to hold
       several of the sizes a bar can use and no more, so a client with a huge
       or broken icon cannot make the event loop pull a megabyte in one go. */
    if (XGetWindowProperty(core->display, frame->client, icon_atom,
                           0, WM_ICON_MAX_CARDS, False, AnyPropertyType,
                           &actual_type, &actual_format, &items, &after,
                           &data) != Success) {
        return;
    }
    if (!data || actual_format != 32 || items < 2) {
        if (data) {
            XFree(data);
        }
        return;
    }

    unsigned long source_side = 0;
    unsigned long base = icon_best((const unsigned long *)data, items,
                                   &source_side);
    if (source_side == 0) {
        XFree(data);
        return;
    }

    int side = WM_ICON_SIZE;
    int depth = DefaultDepth(core->display, core->screen);
    frame->icon_source = XCreatePixmap(core->display, core->root,
                                       (unsigned int)side, (unsigned int)side,
                                       (unsigned int)depth);
    frame->icon_mask = XCreatePixmap(core->display, core->root,
                                     (unsigned int)side, (unsigned int)side,
                                     1);
    if (frame->icon_source == None || frame->icon_mask == None) {
        XFree(data);
        wm_frame_free_icon(core, frame);
        return;
    }

    icon_scale(core, frame, (const unsigned long *)data, base, source_side);
    XFree(data);

    frame->icon_side = side;
    frame->icon_ok = 1;
}

/* --- drawing -------------------------------------------------------------- */

int wm_frame_draw_icon(WmCore *core, WmFrame *frame, Drawable target,
                       int x, int y, int side) {
    if (!frame->icon_ok || frame->icon_source == None ||
        frame->icon_mask == None || side <= 0) {
        return 0;
    }
    /* The icon was made at WM_ICON_SIZE. Drawn at that size it is put down
       whole; anything else would need scaling, which a plain X pixmap cannot
       do, so the caller is expected to ask for that size. The mask clips the
       colour pixmap, so the icon shows the bar through its transparent parts
       rather than a rectangle of its own background. */
    GC gc = core->gc;
    XSetClipMask(core->display, gc, frame->icon_mask);
    XSetClipOrigin(core->display, gc, x, y);
    XCopyArea(core->display, frame->icon_source, target, gc, 0, 0,
              (unsigned int)side, (unsigned int)side, x, y);
    XSetClipMask(core->display, gc, None);
    XSetClipOrigin(core->display, gc, 0, 0);
    return 1;
}
