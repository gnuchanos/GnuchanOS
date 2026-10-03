/*
 * notif_image.h — a picture in a bubble, as one pixmap.
 *
 * A notification may carry a picture two ways: as a file on disk — the path
 * handed in the notification's `app_icon` field or in its `icon` hint — or as
 * pixels put straight into the `image-data` hint, which is what a program sends
 * when it has already decoded the picture and does not want to write it to a
 * file first. Both are turned into the same thing here: a colour pixmap, one
 * `side` square, with the picture composited over the bubble's own background
 * colour, so a transparent pixel becomes the bubble rather than a box.
 *
 * Imlib2 is what reads a file, the same library GnuChanWM draws its wallpaper
 * with and GnuChanFetch draws its logo with — so a machine that runs this
 * desktop already has it.
 */
#ifndef GNUCHANNOTIFICATION_IMAGE_H
#define GNUCHANNOTIFICATION_IMAGE_H

#include <X11/Xlib.h>

typedef struct NotifImage {
    Pixmap pixmap;   /* the picture, one `side` square, or None        */
    int side;        /* the side it was made at                        */
    int ok;          /* 1 once it holds something worth drawing        */
} NotifImage;

/* Load the picture at `name` scaled to a `side` square, over `background`. A
   leading "~" is the home directory; a relative name is looked for beside the
   settings script. Returns 0 on success, -1 when the file cannot be found or
   read, in which case the image is left not-ok. */
int notif_image_load_file(Display *display, Window root, Visual *visual,
                          int depth, NotifImage *image, const char *name,
                          int side, unsigned long background);

/* Build a picture from the ARGB cards a client sent in its `image-data` hint —
   the width and height it named, and the pixels that follow. Returns 0 on
   success, -1 when the data is not a usable picture. */
int notif_image_load_argb(Display *display, Window root, Visual *visual,
                          int depth, NotifImage *image, const long *cards,
                          int card_count, int width, int height, int side,
                          unsigned long background);

/* Release the pixmap and mark the image empty. Safe on an empty image. */
void notif_image_free(Display *display, NotifImage *image);

/* Put the image on a drawable at (x, y). Returns 1 when something was drawn
   and 0 when the image is empty. */
int notif_image_draw(Display *display, GC gc, const NotifImage *image,
                     Drawable target, int x, int y);

#endif /* GNUCHANNOTIFICATION_IMAGE_H */
