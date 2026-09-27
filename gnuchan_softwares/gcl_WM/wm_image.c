/*
 * wm_image.c — the wallpaper, drawn with Imlib2.
 *
 * This is the same shape as feh's wallpaper.c: the file is handed to Imlib2,
 * Imlib2 renders it stretched onto a pixmap the size of the screen, and the
 * desktop (wm_desktop.c) makes that pixmap the root's background on a
 * connection of its own. Nothing here writes a PNG decoder or an XImage by
 * hand, and nothing writes pixels through XPutPixel: the earlier version did
 * both, and both are the kind of code that is right on one build and skewed
 * colour on another. Imlib2 already knows how to read the file and how to put
 * it on a pixmap, so the work is not redone here.
 *
 * The picture is stretched, not tiled: it is rendered at exactly the size it
 * is to be shown at, so a wallpaper fills the screen whatever its own
 * proportions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <Imlib2.h>

#include "wm_core.h"
#include "wm_image.h"

/* --- where the file is ----------------------------------------------------- */

/* Whether a file can be opened for reading. */
static int file_readable(const char *path) {
    if (!path || !path[0]) {
        return 0;
    }
    return access(path, R_OK) == 0;
}

/* The path a picture name refers to, written into `out`.
 *
 * An absolute name is used as it is. A name beginning with "~" is one relative
 * to the home directory, which is how the shipped script writes its picture —
 * `BackgroundImage = "~/.config/GnuChanWM/bg.png"` — and the shell it was
 * copied from expands it, so it is expanded here too. A relative name is
 * looked for beside the settings script first — the script and its picture are
 * written together, so `BackgroundImage="bg.png"` means the bg.png next to
 * GnuChanWM.py — and then relative to the working directory, which is what a
 * name in a shell command would mean.
 *
 * Returns 1 when a readable file was found, 0 otherwise; `out` is filled
 * either way, so a caller that fails can say which path it tried. */
static int resolve_path(const char *name, char *out, unsigned int size) {
    if (!name || !name[0]) {
        out[0] = '\0';
        return 0;
    }

    if (name[0] == '~' && (name[1] == '/' || name[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            const char *rest = name[1] == '/' ? name + 1 : "";
            snprintf(out, size, "%s%s", home, rest);
            return file_readable(out);
        }
        /* No home to expand into: fall through and try the name as written,
           which at least names the file that could not be found. */
    }

    if (name[0] == '/') {
        snprintf(out, size, "%s", name);
        return file_readable(out);
    }

    char config_path[WM_IMAGE_PATH_LENGTH];
    wm_config_path(config_path, sizeof(config_path));
    char *slash = strrchr(config_path, '/');
    if (slash) {
        /* Everything before the last slash is the directory the script sits
           in, and the name is looked for under it. The two are joined in
           place, the name given exactly the room the output has left, so no
           part of the join can be written past the end — which is what the
           compiler was right to point at when the directory was copied into a
           buffer of its own and the name was then added to it. */
        *slash = '\0';
        size_t used = strlen(config_path);
        if (used + 1 < size) {
            memcpy(out, config_path, used);
            out[used] = '/';
            snprintf(out + used + 1, size - used - 1, "%s", name);
            if (file_readable(out)) {
                return 1;
            }
        }
    }

    snprintf(out, size, "%s", name);
    return file_readable(out);
}

/* --- the picture ----------------------------------------------------------- */

int wm_image_load(WmCore *core, WmImage *image, const char *name,
                  int target_width, int target_height) {
    if (!core || !image) {
        return -1;
    }

    /* The same name at the same size is already loaded: the desktop repaints
       its background on every expose and the file almost never changes. The
       test is against the name as written, not the resolved path — the caller
       passes "bg.png" on every repaint while `path` holds the absolute place
       that name was found. */
    if (image->ok && image->width_loaded == target_width &&
        image->height_loaded == target_height && name &&
        strcmp(image->source_name, name) == 0) {
        return 0;
    }

    char path[sizeof(image->path)];
    if (!resolve_path(name, path, sizeof(path)) ||
        target_width <= 0 || target_height <= 0) {
        fprintf(stderr, "gnuchanwm: image '%s' not found (looked for '%s')\n",
                name ? name : "", path);
        wm_config_show_message(core,
                               "GnuChanWM: the wallpaper was not found",
                               name ? name : "(no name given)", path);
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->width_loaded = target_width;
        image->height_loaded = target_height;
        return -1;
    }

    /* Imlib2 is told which display, visual and colormap to draw for before
       anything is loaded: it reads them from this context when it renders, so
       they have to be set on the context that will do the drawing, which is
       this one. */
    imlib_context_set_display(core->display);
    imlib_context_set_visual(DefaultVisual(core->display, core->screen));
    imlib_context_set_colormap(DefaultColormap(core->display, core->screen));

    Imlib_Load_Error error = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image loaded = imlib_load_image_with_error_return(path, &error);
    if (!loaded || error != IMLIB_LOAD_ERROR_NONE) {
        if (loaded) {
            imlib_context_set_image(loaded);
            imlib_free_image();
        }
        fprintf(stderr,
                "gnuchanwm: image '%s' could not be read by Imlib2 (error %d)\n",
                path, (int)error);
        wm_config_show_message(core,
                               "GnuChanWM: the wallpaper could not be read",
                               "Imlib2 could not read the file", path);
        wm_image_free(core, image);
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->width_loaded = target_width;
        image->height_loaded = target_height;
        return -1;
    }

    /* The old picture goes before the new one is made, so a reload does not
       hold two pictures on the server at once. */
    wm_image_free(core, image);

    Pixmap pixmap = XCreatePixmap(core->display, core->root,
                                  (unsigned int)target_width,
                                  (unsigned int)target_height,
                                  (unsigned int)DefaultDepth(core->display,
                                                             core->screen));
    if (pixmap == None) {
        fprintf(stderr, "gnuchanwm: cannot make a pixmap for '%s'\n", path);
        imlib_context_set_image(loaded);
        imlib_free_image();
        snprintf(image->path, sizeof(image->path), "%s", path);
        snprintf(image->source_name, sizeof(image->source_name), "%s",
                 name ? name : "");
        image->width_loaded = target_width;
        image->height_loaded = target_height;
        return -1;
    }

    /* The pixmap is filled with the desktop colour first, so a picture with a
       transparent pixel lets that colour show rather than whatever the server
       had there. It is then drawn over — stretched to the whole pixmap, with
       blending on — which is the same render feh's scaled wallpaper does. */
    GC gc = XCreateGC(core->display, pixmap, 0, NULL);
    XSetForeground(core->display, gc, core->style.background);
    XFillRectangle(core->display, pixmap, gc, 0, 0,
                   (unsigned int)target_width, (unsigned int)target_height);
    XFreeGC(core->display, gc);

    imlib_context_set_image(loaded);
    imlib_context_set_drawable(pixmap);
    imlib_context_set_blend(1);
    imlib_render_image_on_drawable_at_size(0, 0, target_width, target_height);

    imlib_free_image_and_decache();

    image->pixmap = pixmap;
    image->width = target_width;
    image->height = target_height;
    image->ok = 1;

    snprintf(image->path, sizeof(image->path), "%s", path);
    snprintf(image->source_name, sizeof(image->source_name), "%s",
             name ? name : "");
    image->width_loaded = target_width;
    image->height_loaded = target_height;
    return 0;
}

void wm_image_free(WmCore *core, WmImage *image) {
    if (!image) {
        return;
    }
    if (core && core->display && image->pixmap != None) {
        XFreePixmap(core->display, image->pixmap);
    }
    image->pixmap = None;
    image->width = 0;
    image->height = 0;
    image->ok = 0;
}
