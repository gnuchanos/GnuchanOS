/*
 * wm_image.h — a picture from disk, made into a pixmap the desktop can show.
 *
 * The settings script names a file — `gcl_Window.BackgroundImage = "bg.png"`
 * — and this is what turns that name into pixels: find the file, decode it
 * (wm_png.c), and stretch it to the size it is to be shown at.
 *
 * There is no mask and no alpha channel here on purpose. A picture is put on
 * the screen by being stretched to the whole screen and made the root
 * window's background; a transparent pixel of the file shows the desktop
 * colour, which is done once, here, while the pixels are still in memory. The
 * earlier version kept a 1-bit mask beside the colours and clipped through it
 * at draw time, and that is what made a wallpaper come out as noise on a real
 * X server: a mask is one bit per pixel packed the way the server's bitmaps
 * are, and a picture that is almost entirely solid does not need that second
 * surface or the server-dependent packing it requires.
 */
#ifndef GNUCHANWM_IMAGE_H
#define GNUCHANWM_IMAGE_H

#include <X11/Xlib.h>

/* WmCore lives in wm_core.h, which includes this file's siblings; the pointer
   here is only ever passed on, never inspected. */
typedef struct WmCore WmCore;

/* The longest path a picture name may resolve to: a relative name is looked
   for beside the settings script, so this holds the script's directory and the
   name after it. */
#define WM_IMAGE_PATH_LENGTH 4096

/* A picture, decoded and stretched to the size it will be shown at.
 *
 * `pixmap` is the picture itself, already at (width, height) and already with
 * its transparent parts composited onto the desktop colour. `ok` says whether
 * it holds anything — a file that could not be read leaves it clear, and the
 * caller falls back to the flat desktop colour rather than drawing nothing. */
typedef struct WmImage {
    Pixmap pixmap;

    int width;
    int height;
    int ok;

    /* What was loaded, so that the same name at the same size is not read and
       decoded again: the desktop repaints its background on every expose, and
       the file almost never changes. The name as the script wrote it is kept
       beside the resolved path, because the caller passes "bg.png" on every
       repaint while `path` holds the absolute place that name was found, and
       comparing the two would never match. */
    char path[WM_IMAGE_PATH_LENGTH];
    char source_name[WM_IMAGE_PATH_LENGTH];
    int width_loaded;
    int height_loaded;
} WmImage;

/* Load the picture the name refers to, stretched to exactly target_width by
   target_height and ready to be shown. A name that is empty, a file that is
   not there, or one that does not decode clears the image and returns -1, so
   the caller keeps its colour.
 *
 * The name may be absolute, or relative — in which case it is looked for
 * beside the settings script first, because that is where a person who wrote
 * `BackgroundImage="bg.png"` put the file. */
int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_width, int target_height);

/* Put the picture at (x, y) on a drawable, at the size it was loaded. Returns
   1 when something was drawn and 0 when there is nothing to draw. */
int wm_image_draw(WmCore *core, WmImage *image, Drawable target, int x, int y);

/* Drop the picture and the pixmap it was drawn into. */
void wm_image_free(WmCore *core, WmImage *image);

#endif /* GNUCHANWM_IMAGE_H */
