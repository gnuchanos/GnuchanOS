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
 */
#include <stdlib.h>
#include <string.h>

#include "wm_core.h"
#include "wm_frame.h"

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

/* A pixel of the icon, allocated on the server's colour map. The colour is a
   round trip to the server, which is why the caller keeps a small table of
   the ones it has already allocated: an icon is thousands of pixels and a
   hand-drawn one is a handful of colours. */
static unsigned long icon_pixel(WmCore *core, unsigned int red,
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

/* --- reading -------------------------------------------------------------- */

/* The best entry in a _NET_WM_ICON list: the one with the most pixels that is
   square and no larger than the ceiling. Returns the offset of its card pair
   and writes its side.
 *
 * The walk is the reason this is separate. The list is a run of (w, h, pixels)
 * triples, so where one image ends is only known by reading its width and
 * height first; a malformed list — a width of zero, or a height that is not
 * the width — is where the walk stops, because past it there is no way to know
 * where the next image begins. */
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
        unsigned long area = side * side;
        if (off + 2 + area > items) {
            break;
        }
        if (side <= WM_ICON_MAX_SIDE && area > best_area) {
            best_area = area;
            best_side = side;
            best = off;
        }
        off += 2 + area;
    }

    *side_out = best_side;
    return best;
}

/* Copy one chosen image down to bar size.
 *
 * The source is read at (x * source / side, y * source / side), which is
 * nearest-neighbour: for the sizes involved — a 48 pixel icon onto a 16 pixel
 * square — the difference from a smoother scale is a few pixels of edge, and
 * doing it here keeps every other part of the drawing code free of scaling. A
 * pixel whose alpha is nearly zero is left out of the mask, which is what
 * makes a rounded or irregular icon sit on the bar rather than in a box. */
static void icon_scale(WmCore *core, WmFrame *frame, const unsigned long *cards,
                       unsigned long base, unsigned long source_side) {
    int side = WM_ICON_SIZE;
    unsigned long *seen_key = calloc(512, sizeof(unsigned long));
    unsigned long *seen_pixel = calloc(512, sizeof(unsigned long));
    int seen_count = 0;
    if (!seen_key || !seen_pixel) {
        free(seen_key);
        free(seen_pixel);
        return;
    }

    /* Two graphics contexts, one per drawable.
     *
     * The mask is one bit deep and the colours are as deep as the screen, and
     * a GC carries the depth of the drawable it was made on. Using one GC for
     * both — which this did — is a PolyPoint with a colour the mask cannot
     * hold onto a window whose depth is not the GC's, and the server answers
     * every one of those with BadMatch: one error per pixel of every icon on
     * the bar, and the icon itself never drawn. */
    GC mask_gc = XCreateGC(core->display, frame->icon_mask, 0, NULL);
    XSetForeground(core->display, mask_gc, 0);
    XFillRectangle(core->display, frame->icon_mask, mask_gc, 0, 0,
                   (unsigned int)side, (unsigned int)side);
    XSetForeground(core->display, mask_gc, 1);

    GC gc = XCreateGC(core->display, frame->icon_source, 0, NULL);

    for (int y = 0; y < side; y++) {
        unsigned long sy =
            (unsigned long)y * source_side / (unsigned long)side;
        for (int x = 0; x < side; x++) {
            unsigned long sx =
                (unsigned long)x * source_side / (unsigned long)side;
            unsigned long argb = cards[base + 2 + sy * source_side + sx];
            unsigned int alpha = (unsigned int)((argb >> 24) & 0xff);
            if (alpha < 0x20) {
                continue;
            }
            unsigned int red   = (unsigned int)((argb >> 16) & 0xff);
            unsigned int green = (unsigned int)((argb >> 8) & 0xff);
            unsigned int blue  = (unsigned int)(argb & 0xff);

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
                pixel = icon_pixel(core, red, green, blue);
                if (seen_count < 512) {
                    seen_key[seen_count] = key;
                    seen_pixel[seen_count] = pixel;
                    seen_count++;
                }
            }
            XSetForeground(core->display, gc, pixel);
            XDrawPoint(core->display, frame->icon_source, gc, x, y);
            XDrawPoint(core->display, frame->icon_mask, mask_gc, x, y);
        }
    }

    XFreeGC(core->display, gc);
    XFreeGC(core->display, mask_gc);
    free(seen_key);
    free(seen_pixel);
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
