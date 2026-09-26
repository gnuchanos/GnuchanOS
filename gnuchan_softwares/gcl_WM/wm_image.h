/*
 * wm_image.h — a picture from disk, made into something the bar can draw.
 *
 * The settings script names a file — `gcl_BAR.call(BackgroundImage="BG.png")`
 * — and this is what turns that name into pixels on the screen. Three jobs,
 * and they are here together because each depends on the last: find the file
 * the name refers to, decode it (wm_png.c), and hold the result as an X pixmap
 * so that the bar's once-a-second repaint copies a picture instead of
 * decoding a file.
 *
 * The picture is scaled to the bar's height once, keeping its proportions, and
 * that scaled strip is repeated across the bar. Repeating rather than
 * stretching is what keeps a square logo square on a bar that is two thousand
 * pixels wide and twenty-four tall: a stretched picture is a smear, and a bar
 * is the one surface where the difference is unmissable.
 */
#ifndef GNUCHANWM_IMAGE_H
#define GNUCHANWM_IMAGE_H

#include <X11/Xlib.h>

/* WmCore lives in wm_core.h, which includes this file's siblings; the pointers
   here are only ever passed on, never inspected. */
typedef struct WmCore WmCore;

/* The longest path a picture name may resolve to. A relative name is looked
   for beside the settings script, so this has to hold the script's directory
   and the name after it — wider than the name buffer the config holds,
   because the directory part is added when the path is built. */
#define WM_IMAGE_PATH_LENGTH 4096

/* A picture, decoded and scaled, ready to be tiled across the bar.
 *
 * `pixmap` is the tile: one copy of the picture at the bar's height, no wider
 * than the bar itself. `ok` says whether it holds anything — a file that could
 * not be read leaves it clear, and the caller falls back to the flat colour
 * rather than drawing nothing. */
typedef struct WmImage {
    Pixmap pixmap;    /* the tile's colours                             */
    Pixmap mask;      /* where the tile is solid; 1-bit, like an icon   */
    int width;
    int height;
    int ok;

    /* What was loaded, so that the same name at the same height is not read
       and decoded again on every repaint. The path is kept resolved, not as it
       was written, so that a reload which changes only the working directory
       still tries the file the script actually meant. */
    char path[WM_IMAGE_PATH_LENGTH];
    int height_loaded;

    /* The name as the script wrote it. The cache tests this and the height,
       not the resolved path: the caller passes "BG.png" on every repaint,
       while `path` holds the absolute place that name was found, so comparing
       the two never matched and the file was read and decoded once a second —
       the flicker the cache was written to prevent. */
    char source_name[WM_IMAGE_PATH_LENGTH];
} WmImage;

/* Load the picture the name refers to, scaled to `target_height` and ready to
   tile. A name that is empty, a file that is not there, or one that does not
   decode clears the image and returns -1, so the caller keeps its colour.
 *
 * The name may be absolute, or relative — in which case it is looked for
 * beside the settings script first, because that is where a person who wrote
 * `BackgroundImage="BG.png"` put the file. */
int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_height);

/* Tile the picture across `width` pixels starting at (x, y) on a drawable.
   Returns 1 when something was drawn. A picture that is not loaded draws
   nothing and returns 0, which is how the caller knows to keep its colour. */
int wm_image_draw(WmCore *core, WmImage *image, Drawable target,
                  int x, int y, int width);

/* Drop the picture and the pixmap it was drawn into. */
void wm_image_free(WmCore *core, WmImage *image);

#endif /* GNUCHANWM_IMAGE_H */
