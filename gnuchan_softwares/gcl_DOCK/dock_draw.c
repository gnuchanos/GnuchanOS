/*
 * dock_draw.c — the whole dock, painted into its window.
 *
 * One function, because the painting is one act: the body, the icons, and the
 * labels all go into a buffer and the buffer is copied up in one operation. The
 * reason is the one dock_draw.h gives — the dock is repainted every time the
 * pointer moves, and drawing straight to the window would be seen as a flicker
 * as the body is cleared, then partly redrawn, then finished.
 *
 * The buffer is the window's own size and is kept across repaints; only its
 * contents are redrawn each time. It is rebuilt when the dock changes size,
 * which happens whenever the row of items grows or shrinks.
 *
 * The icons grow as the pointer passes over them, which is what makes the row a
 * dock rather than a strip. An icon is stored at its resting side once (see
 * dock_icon.c) and cannot be copied larger, so the grown icons are drawn through
 * XRender with a bilinear filter — the one thing here that is not plain Xlib.
 * Everything else — the body, the plates, the letters — is core X, and the
 * labels are Xft, the same as the window manager's title bar.
 */
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

#include "dock_core.h"
#include "dock_draw.h"

/* The edge drawn around the body, in pixels. */
#define DOCK_EDGE_THICKNESS 2
/* The air between an icon's bottom and its label. */
#define DOCK_LABEL_GAP 2
/* How far a hovered icon's plate reaches past the icon it sits behind. */
#define DOCK_PLATE_PAD 4
/* The side of the round badge a category wears on its shoulder, in pixels. */
#define DOCK_BADGE_SIDE 16

/* The buffer the dock is composed into before it is shown. It belongs to one
   display at one size; either changing rebuilds it. */
typedef struct DockBuffer {
    Display *display;
    Pixmap pixmap;
    Picture picture;
    XRenderPictFormat *format;
    int width;
    int height;
    int depth;
} DockBuffer;

static DockBuffer dock_buffer;

static int clamp_int(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

/* The icon a slot draws. The two fixed icons are loaded once into the core and
   do not live in the item, so a window's own icon is the only one that does. */
static const DockIcon *item_icon(const DockCore *core, const DockItem *item) {
    switch (item->kind) {
    case DOCK_ITEM_SETTINGS:
        return &core->settings_icon;
    case DOCK_ITEM_TERMINAL:
        return &core->terminal_icon;
    case DOCK_ITEM_WINDOW:
    case DOCK_ITEM_GROUP:
        /* A group wears the icon of the first window gathered into it, which
           is the program's own icon: every window of a class is the same
           program, so one stands for them all. */
        return &item->icon;
    }
    return NULL;
}

/* How much the icon at `index` grows, in pixels, for the current pointer.
   Only the icon directly under the pointer grows. Growing the neighbours as
   well turns a row of a few icons into a single blob the moment one of them is
   touched, and pulls them into each other, which is not what a dock should do:
   the one the pointer is on is the one that answers. */
static int magnify_amount(const DockCore *core, int index) {
    int magnify = core->config.magnify;
    if (magnify <= 0 || core->hover_index < 0) {
        return 0;
    }
    return index == core->hover_index ? magnify : 0;
}

/* Make sure the buffer is the dock's current size on the current display. */
static int buffer_ready(DockCore *core) {
    if (dock_buffer.display == core->display &&
        dock_buffer.width == core->width &&
        dock_buffer.height == core->height &&
        dock_buffer.depth == core->depth &&
        dock_buffer.pixmap != None) {
        return 1;
    }

    if (dock_buffer.display) {
        if (dock_buffer.picture != None) {
            XRenderFreePicture(dock_buffer.display, dock_buffer.picture);
        }
        if (dock_buffer.pixmap != None) {
            XFreePixmap(dock_buffer.display, dock_buffer.pixmap);
        }
    }
    dock_buffer.display = core->display;
    dock_buffer.picture = None;
    dock_buffer.pixmap = None;
    dock_buffer.format = NULL;
    dock_buffer.width = 0;
    dock_buffer.height = 0;
    dock_buffer.depth = 0;

    if (core->width <= 0 || core->height <= 0) {
        return 0;
    }

    dock_buffer.pixmap = XCreatePixmap(core->display, core->window,
                                       (unsigned int)core->width,
                                       (unsigned int)core->height,
                                       (unsigned int)core->depth);
    if (dock_buffer.pixmap == None) {
        return 0;
    }
    dock_buffer.format = XRenderFindVisualFormat(core->display, core->visual);
    if (dock_buffer.format) {
        dock_buffer.picture = XRenderCreatePicture(core->display,
                                                   dock_buffer.pixmap,
                                                   dock_buffer.format, 0, NULL);
    }
    dock_buffer.width = core->width;
    dock_buffer.height = core->height;
    dock_buffer.depth = core->depth;
    return 1;
}

/* A rectangle with the four corners rounded to `radius`. Done as the two
   crossing rectangles plus four quarter discs, because core X has no rounded
   rectangle and the shape shave nothing off the picture. */
static void fill_rounded(Display *display, Drawable target, GC gc, int x, int y,
                         int width, int height, int radius) {
    if (radius <= 0) {
        XFillRectangle(display, target, gc, x, y, (unsigned int)width,
                       (unsigned int)height);
        return;
    }
    radius = clamp_int(radius, 0, width / 2);
    radius = clamp_int(radius, 0, height / 2);
    int span = radius * 2;
    int angle = 90 * 64;

    XFillRectangle(display, target, gc, x + radius, y,
                   (unsigned int)(width - span), (unsigned int)height);
    XFillRectangle(display, target, gc, x, y + radius, (unsigned int)radius,
                   (unsigned int)(height - span));
    XFillRectangle(display, target, gc, x + width - radius, y + radius,
                   (unsigned int)radius, (unsigned int)(height - span));

    XFillArc(display, target, gc, x, y, (unsigned int)span, (unsigned int)span,
             angle, angle);
    XFillArc(display, target, gc, x + width - span, y, (unsigned int)span,
             (unsigned int)span, 0, angle);
    XFillArc(display, target, gc, x, y + height - span, (unsigned int)span,
             (unsigned int)span, 180 * 64, angle);
    XFillArc(display, target, gc, x + width - span, y + height - span,
             (unsigned int)span, (unsigned int)span, 270 * 64, angle);
}

/* The outline of the same rounded rectangle, for the body's edge. */
static void outline_rounded(Display *display, Drawable target, GC gc, int x,
                            int y, int width, int height, int radius) {
    XSetLineAttributes(display, gc, DOCK_EDGE_THICKNESS, LineSolid, CapButt,
                       JoinRound);
    if (radius <= 0) {
        XDrawRectangle(display, target, gc, x, y, (unsigned int)width,
                       (unsigned int)height);
        XSetLineAttributes(display, gc, 0, LineSolid, CapButt, JoinMiter);
        return;
    }
    radius = clamp_int(radius, 0, width / 2);
    radius = clamp_int(radius, 0, height / 2);
    int span = radius * 2;
    int angle = 90 * 64;

    XDrawLine(display, target, gc, x + radius, y, x + width - radius, y);
    XDrawLine(display, target, gc, x + radius, y + height, x + width - radius,
              y + height);
    XDrawLine(display, target, gc, x, y + radius, x, y + height - radius);
    XDrawLine(display, target, gc, x + width, y + radius, x + width,
              y + height - radius);

    XDrawArc(display, target, gc, x, y, (unsigned int)span, (unsigned int)span,
             angle, angle);
    XDrawArc(display, target, gc, x + width - span, y, (unsigned int)span,
             (unsigned int)span, 0, angle);
    XDrawArc(display, target, gc, x, y + height - span, (unsigned int)span,
             (unsigned int)span, 180 * 64, angle);
    XDrawArc(display, target, gc, x + width - span, y + height - span,
             (unsigned int)span, (unsigned int)span, 270 * 64, angle);

    XSetLineAttributes(display, gc, 0, LineSolid, CapButt, JoinMiter);
}

/* The edge line that reads as the dock's rounded body. The window is already
   the background colour, so this is the whole of the body: the fill and the
   rounding, drawn as one outline around the whole window. */
static void draw_body(DockCore *core, Drawable target) {
    Display *display = core->display;
    GC gc = core->gc;
    int inset = DOCK_EDGE_THICKNESS / 2;

    XSetForeground(display, gc, core->background);
    XFillRectangle(display, target, gc, 0, 0, (unsigned int)core->width,
                   (unsigned int)core->height);

    XSetForeground(display, gc, core->edge);
    outline_rounded(display, target, gc, inset, inset,
                    core->width - DOCK_EDGE_THICKNESS,
                    core->height - DOCK_EDGE_THICKNESS,
                    core->config.corner);
}

/* Put one icon down at `size`. The stored icon is `side` wide; when the two
   differ — always, for a magnified icon — it is scaled through XRender with a
   bilinear filter. When they are equal a plain copy is enough. */
static int draw_icon_scaled(DockCore *core, const DockIcon *icon, int x, int y,
                            int size) {
    if (!icon || !icon->ok || icon->pixmap == None || size <= 0) {
        return 0;
    }
    if (size == icon->side) {
        return dock_icon_draw(core->display, core->gc, icon,
                              dock_buffer.pixmap, x, y);
    }
    if (dock_buffer.picture == None || !dock_buffer.format) {
        return dock_icon_draw(core->display, core->gc, icon,
                              dock_buffer.pixmap, x, y);
    }

    Picture source = XRenderCreatePicture(core->display, icon->pixmap,
                                          dock_buffer.format, 0, NULL);
    if (source == None) {
        return 0;
    }
    double scale = (double)icon->side / (double)size;
    XTransform transform;
    memset(&transform, 0, sizeof(transform));
    transform.matrix[0][0] = XDoubleToFixed(scale);
    transform.matrix[1][1] = XDoubleToFixed(scale);
    transform.matrix[2][2] = XDoubleToFixed(1.0);
    XRenderSetPictureTransform(core->display, source, &transform);
    XRenderSetPictureFilter(core->display, source, FilterBilinear, NULL, 0);
    XRenderComposite(core->display, PictOpSrc, source, None,
                     dock_buffer.picture, 0, 0, 0, 0, x, y,
                     (unsigned int)size, (unsigned int)size);
    XRenderFreePicture(core->display, source);
    return 1;
}

static int make_colour(DockCore *core, const char *name, XftColor *out) {
    if (!name || !name[0]) {
        return 0;
    }
    return XftColorAllocName(core->display, core->visual, core->colormap, name,
                             out) ? 1 : 0;
}

/* The count a category wears: how many windows its slot stands for. Drawn on
   the icon's top-right shoulder, a solid disc of the badge colour with the
   number in the accent, so a glance says "there are more here" without the
   count shouting over the icon it belongs to. */
static void draw_badge(DockCore *core, XftDraw *draw, XftColor *text_colour,
                       int count, int icon_x, int icon_y, int size) {
    if (count < 1 || !core->font) {
        return;
    }
    int side = DOCK_BADGE_SIDE;
    if (side > size) {
        side = size;
    }
    int bx = icon_x + size - side + DOCK_PLATE_PAD;
    int by = icon_y - DOCK_PLATE_PAD;

    char number[8];
    snprintf(number, sizeof(number), "%d", count);

    Display *display = core->display;
    GC gc = core->gc;
    XSetForeground(display, gc, core->badge);
    XFillArc(display, dock_buffer.pixmap, gc, bx, by, (unsigned int)side,
             (unsigned int)side, 0, 360 * 64);
    XSetForeground(display, gc, core->accent);
    XDrawArc(display, dock_buffer.pixmap, gc, bx, by, (unsigned int)side,
             (unsigned int)side, 0, 360 * 64);

    if (!draw || !text_colour) {
        return;
    }
    XGlyphInfo extent;
    XftTextExtentsUtf8(display, core->font, (const FcChar8 *)number,
                       (int)strlen(number), &extent);
    int tx = bx + (side - (int)extent.xOff) / 2;
    int ty = by + (side + (core->font->ascent - core->font->descent)) / 2;
    XftDrawStringUtf8(draw, text_colour, core->font, tx, ty,
                      (const FcChar8 *)number, (int)strlen(number));
}

/* A line of text centred on `centre_x`, sitting on `baseline`. */
static void draw_text_centred(XftDraw *draw, DockCore *core, XftColor *colour,
                              const char *text, int centre_x, int baseline) {
    if (!draw || !core->font || !colour || !text || !text[0]) {
        return;
    }
    int length = (int)strlen(text);
    XGlyphInfo extent;
    XftTextExtentsUtf8(core->display, core->font, (const FcChar8 *)text, length,
                       &extent);
    int x = centre_x - (int)extent.xOff / 2;
    XftDrawStringUtf8(draw, colour, core->font, x, baseline,
                      (const FcChar8 *)text, length);
}

/* Trim a label to a width, so a long window title cannot run out of its slot
   and into its neighbours'. Bytes are kept whole, so a cut never splits a
   UTF-8 character. */
static void fit_label(DockCore *core, const char *label, int max_width,
                      char *out, unsigned int size) {
    snprintf(out, size, "%s", label);
    if (!core->font || max_width <= 0) {
        return;
    }
    XGlyphInfo extent;
    int length = (int)strlen(out);
    XftTextExtentsUtf8(core->display, core->font, (const FcChar8 *)out, length,
                       &extent);
    if ((int)extent.xOff <= max_width) {
        return;
    }

    char trial[DOCK_TEXT_LENGTH];
    for (int cut = length; cut > 0; cut--) {
        if (((unsigned char)out[cut] & 0xc0) == 0x80) {
            continue;
        }
        snprintf(trial, sizeof(trial), "%.*s...", cut, out);
        XftTextExtentsUtf8(core->display, core->font, (const FcChar8 *)trial,
                           (int)strlen(trial), &extent);
        if ((int)extent.xOff <= max_width) {
            snprintf(out, size, "%s", trial);
            return;
        }
    }
    out[0] = '\0';
}

void dock_draw(DockCore *core) {
    if (!core || !core->display || core->window == None) {
        return;
    }
    if (!buffer_ready(core)) {
        return;
    }

    Display *display = core->display;
    Drawable target = dock_buffer.pixmap;
    GC gc = core->gc;

    draw_body(core, target);

    int padding = core->config.padding;
    int gap = core->config.gap;
    int icon = core->config.icon_size;
    int magnify = core->config.magnify;
    int pitch = icon + gap;
    /* The row is offset by half the magnify room the layout reserves on the
       left, so a hovered end icon grows into that room instead of out of the
       body. The same offset is used by the hit test in dock_items.c. */
    int row_left = padding + magnify / 2;
    int icon_bottom = padding + magnify + icon;
    int label_band = 0;
    if (core->config.label_enabled && core->font) {
        label_band = core->font->ascent + core->font->descent + 4;
    }
    int label_baseline = icon_bottom + DOCK_LABEL_GAP +
                         (core->font ? core->font->ascent : 0);

    XftColor text_colour;
    XftColor accent_colour;
    int have_text = make_colour(core, core->config.text, &text_colour);
    int have_accent = make_colour(core, core->config.accent, &accent_colour);

    XftDraw *draw = NULL;
    if (label_band > 0 || core->font) {
        draw = XftDrawCreate(display, target, core->visual, core->colormap);
    }

    for (int i = 0; i < core->item_count; i++) {
        DockItem *item = &core->items[i];
        int slot_x = row_left + i * pitch;
        int centre_x = slot_x + icon / 2;

        int size = icon + magnify_amount(core, i);
        int ix = slot_x + (icon - size) / 2;
        int iy = icon_bottom - size;

        if (size > icon) {
            XSetForeground(display, gc, core->field);
            fill_rounded(display, target, gc, ix - DOCK_PLATE_PAD,
                         iy - DOCK_PLATE_PAD, size + DOCK_PLATE_PAD * 2,
                         size + DOCK_PLATE_PAD * 2, core->config.corner);
        }

        const DockIcon *icon_data = item_icon(core, item);
        if (!draw_icon_scaled(core, icon_data, ix, iy, size)) {
            /* No picture: fall back to a plate and the label's first letter. */
            XSetForeground(display, gc, core->field);
            fill_rounded(display, target, gc, ix, iy, size, size,
                         core->config.corner);
            if (draw && have_text && item->label[0] && core->font) {
                char letter[2] = { item->label[0], '\0' };
                draw_text_centred(draw, core, &text_colour, letter, centre_x,
                                  iy + size / 2 + core->font->ascent / 2);
            }
        }

        /* A category wears how many windows it holds, so the row says "more
           than one" without the list being opened to find out. */
        if (dock_item_is_category(item)) {
            draw_badge(core, draw, have_text ? &text_colour : NULL,
                       item->window_count, ix, iy, size);
        }

        if (draw && label_band > 0 && item->label[0]) {
            char shown[DOCK_TEXT_LENGTH];
            /* The label may use the whole slot and not the slot minus the
               magnify padding: at the shipped font a program's name is wider
               than the icon, and trimming it to the icon's own width turned
               "terminal" into "ter..." and "settings" into "set...". The gap
               between slots is what keeps two labels from touching; it is not
               taken out of the label's own room. */
            fit_label(core, item->label, pitch, shown, sizeof(shown));
            if (shown[0]) {
                int hovered = i == core->hover_index;
                XftColor *colour = (hovered && have_accent) ? &accent_colour
                                    : (have_text ? &text_colour : NULL);
                draw_text_centred(draw, core, colour, shown, centre_x,
                                  label_baseline);
            }
        }
    }

    if (draw) {
        XftDrawDestroy(draw);
    }
    if (have_text) {
        XftColorFree(display, core->visual, core->colormap, &text_colour);
    }
    if (have_accent) {
        XftColorFree(display, core->visual, core->colormap, &accent_colour);
    }

    XCopyArea(display, target, core->window, gc, 0, 0,
              (unsigned int)core->width, (unsigned int)core->height, 0, 0);
    XFlush(display);
}
