/*
 * dock_shape.h — the rounded rectangle, drawn once and shared.
 *
 * Both the dock's body and the menu it opens are rounded rectangles of the same
 * kind, so the shape is written once here rather than twice with a chance of the
 * two drifting apart. Core X has no rounded rectangle: the shape is the two
 * crossing rectangles with the four corners left round, and that is what lives
 * behind these two functions — one to fill it, one to outline it.
 */
#ifndef GNUCHANDOCK_SHAPE_H
#define GNUCHANDOCK_SHAPE_H

#include <X11/Xlib.h>

/* Fill a width x height rectangle at (x, y) with its corners rounded to
   `radius`. A radius of zero or less is a plain rectangle. */
void dock_shape_fill(Display *display, Drawable target, GC gc, int x, int y,
                     int width, int height, int radius);

/* Outline the same rectangle with a line `thickness` wide. */
void dock_shape_outline(Display *display, Drawable target, GC gc, int x, int y,
                        int width, int height, int radius, int thickness);

/* Cut a window itself into the rounded rectangle, so its own square corners —
   which would otherwise sit behind the drawn body as four sharp triangles —
   stop being part of the window at all. A window whose bounding region is the
   round shape can no longer paint, and so cannot show, the corners outside it.
   A radius of zero or less gives the window back its whole rectangle. */
void dock_shape_round(Display *display, Window window, int width, int height,
                      int radius);

#endif /* GNUCHANDOCK_SHAPE_H */
