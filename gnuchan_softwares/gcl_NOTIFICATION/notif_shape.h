/*
 * notif_shape.h — the rounded rectangle, drawn once and shared.
 *
 * A bubble and the icon plate inside it are rounded rectangles of the same
 * kind, and the bubble window itself is cut into one too, so the shape is
 * written once here rather than three times with a chance of the three drifting
 * apart. Core X has no rounded rectangle: the shape is the two crossing
 * rectangles with the four corners left round, and that is what lives behind
 * these functions — one to fill it, one to outline it, and one to cut a window
 * into it.
 */
#ifndef GNUCHANNOTIFICATION_SHAPE_H
#define GNUCHANNOTIFICATION_SHAPE_H

#include <X11/Xlib.h>

/* Fill a width x height rectangle at (x, y) with its corners rounded to
   `radius`. A radius of zero or less is a plain rectangle. */
void notif_shape_fill(Display *display, Drawable target, GC gc, int x, int y,
                      int width, int height, int radius);

/* Outline the same rectangle with a line `thickness` wide. */
void notif_shape_outline(Display *display, Drawable target, GC gc, int x, int y,
                         int width, int height, int radius, int thickness);

/* Cut a window itself into the rounded rectangle, so its own square corners —
   which would otherwise sit behind the drawn body as four sharp triangles —
   stop being part of the window at all. A radius of zero or less gives the
   window back its whole rectangle. */
void notif_shape_round(Display *display, Window window, int width, int height,
                       int radius);

#endif /* GNUCHANNOTIFICATION_SHAPE_H */
