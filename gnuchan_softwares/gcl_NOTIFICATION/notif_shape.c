/* notif_shape.c — the rounded rectangle, filled, outlined, cut into windows. */
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include "notif_shape.h"

static int clamp_int(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

void notif_shape_fill(Display *display, Drawable target, GC gc, int x, int y,
                      int width, int height, int radius) {
    if (width <= 0 || height <= 0) {
        return;
    }
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

void notif_shape_outline(Display *display, Drawable target, GC gc, int x, int y,
                         int width, int height, int radius, int thickness) {
    if (width <= 0 || height <= 0) {
        return;
    }
    XSetLineAttributes(display, gc, thickness > 0 ? thickness : 1, LineSolid,
                       CapButt, JoinRound);
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

void notif_shape_round(Display *display, Window window, int width, int height,
                       int radius) {
    if (!display || window == None || width <= 0 || height <= 0) {
        return;
    }
    if (radius <= 0) {
        XShapeCombineMask(display, window, ShapeBounding, 0, 0, None, ShapeSet);
        return;
    }
    radius = clamp_int(radius, 0, width / 2);
    radius = clamp_int(radius, 0, height / 2);
    int span = radius * 2;
    int angle = 90 * 64;

    Pixmap mask = XCreatePixmap(display, window, (unsigned int)width,
                                (unsigned int)height, 1);
    if (mask == None) {
        return;
    }
    GC gc = XCreateGC(display, mask, 0, NULL);

    XSetForeground(display, gc, 0);
    XFillRectangle(display, mask, gc, 0, 0, (unsigned int)width,
                   (unsigned int)height);
    XSetForeground(display, gc, 1);
    XFillRectangle(display, mask, gc, radius, 0,
                   (unsigned int)(width - span), (unsigned int)height);
    XFillRectangle(display, mask, gc, 0, radius, (unsigned int)radius,
                   (unsigned int)(height - span));
    XFillRectangle(display, mask, gc, width - radius, radius,
                   (unsigned int)radius, (unsigned int)(height - span));
    XFillArc(display, mask, gc, 0, 0, (unsigned int)span, (unsigned int)span,
             angle, angle);
    XFillArc(display, mask, gc, width - span, 0, (unsigned int)span,
             (unsigned int)span, 0, angle);
    XFillArc(display, mask, gc, 0, height - span, (unsigned int)span,
             (unsigned int)span, 180 * 64, angle);
    XFillArc(display, mask, gc, width - span, height - span, (unsigned int)span,
             (unsigned int)span, 270 * 64, angle);

    XShapeCombineMask(display, window, ShapeBounding, 0, 0, mask, ShapeSet);

    XFreeGC(display, gc);
    XFreePixmap(display, mask);
}
