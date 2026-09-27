/*
 * wm_image.h — the wallpaper: a picture from disk, made into a pixmap.
 *
 * The settings script names a file — `gcl_Window.BackgroundImage = "bg.png"`
 * — and this turns that name into pixels, the way feh does: the file is handed
 * to Imlib2, Imlib2 draws it stretched onto a pixmap of the root's size, and
 * that pixmap is what the desktop puts behind everything.
 *
 * There is no mask and no alpha surface kept. A picture is put on the screen
 * by being stretched to the whole screen and made the root's background, so a
 * transparent pixel of the file shows the desktop colour: Imlib2 blends it
 * while it draws, and what comes back is one plain pixmap the server keeps.
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

/* A picture, rendered at the size it will be shown at.
 *
 * `pixmap` is the picture itself, already (width, height) and already with its
 * transparent parts blended onto the desktop colour by Imlib2. `ok` says
 * whether it holds anything — a file that could not be read leaves it clear,
 * and the caller falls back to the flat desktop colour rather than drawing
 * nothing. */
typedef struct WmImage {
    Pixmap pixmap;

    int width;
    int height;
    int ok;

    /* What was loaded, so that the same name at the same size is not read and
       drawn again: the desktop repaints its background on every expose, and
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
   not there, or one Imlib2 cannot read clears the image and returns -1, so the
   caller keeps its colour.
 *
 * The name may be absolute, may begin with "~" for the home directory, or may
 * be relative — in which case it is looked for beside the settings script
 * first, because that is where a person who wrote
 * `BackgroundImage="bg.png"` put the file. */
int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_width, int target_height);

/* Drop the picture and free the pixmap it holds. */
void wm_image_free(WmCore *core, WmImage *image);

#endif /* GNUCHANWM_IMAGE_H */
