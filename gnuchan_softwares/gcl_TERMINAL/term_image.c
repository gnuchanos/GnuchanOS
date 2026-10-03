/*
 * term_image.c — the pictures a program places in the terminal.
 *
 * See term_image.h for the escape a program uses and for why a picture is kept
 * as pixels rather than turned into characters. What is here is the list, the
 * loading, and the drawing.
 *
 * --- the block the picture is fitted into ---
 *
 * A program names a block of cells — so many columns wide, so many rows tall —
 * and the picture is fitted INSIDE it with its own proportions kept. It is
 * centred in the block, so a block that is wider than the picture leaves even
 * margins and the picture does not jump to one side. That is what lets a fetch
 * tool reserve a column of the screen for its logo and have the logo land in it
 * whatever shape the file is.
 *
 * --- why the picture is scaled here and once ---
 *
 * Imlib2's own scaling reduces by AREA when asked, which is what keeps a large
 * logo from turning to noise; it is done once, at placement, into a pixmap the
 * size of the block, and a frame after that is one XCopyArea. Rescaling per
 * frame would resample the file on every keystroke for a picture that does not
 * change.
 *
 * --- the alpha ---
 *
 * A logo with a transparent background is composited over the background colour
 * the program named, so a see-through picture sits on the terminal rather than
 * on a black rectangle. The colour is filled into the pixmap before the picture
 * is drawn over it, which is the one operation X does well and needs no blend
 * of its own.
 */
 
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <Imlib2.h>

#include "term_image.h"

/* The most pictures placed at once. A program places the ones it is showing;
   a number past this is a program redrawing a slideshow faster than the screen
   can be read, and the oldest is dropped rather than the list growing without
   end. */
#define TERM_IMAGE_MAX 16

typedef struct TermImage {
    /* The cell it is anchored at, and the block of cells it fills. */
    int x;
    int y;
    int cols;
    int rows;

    /* The pixels, the size of the block it fills, on the server. */
    Pixmap pixmap;
    int    width;
    int    height;
} TermImage;

struct TermImageList {
    TermImage items[TERM_IMAGE_MAX];
    int       count;
};

TermImageList *term_image_list_new(void) {
    TermImageList *list = calloc(1, sizeof(TermImageList));
    return list;
}

/* Drop one picture's pixels, keeping its place in the array. */
static void drop_pixels(Display *display, TermImage *image) {
    if (image->pixmap != None) {
        XFreePixmap(display, image->pixmap);
        image->pixmap = None;
    }
    image->width = 0;
    image->height = 0;
}

void term_image_list_free(TermImageList *list, Display *display) {
    if (list == NULL) {
        return;
    }
    /* The pixmaps belong to the display, so the display has to be here to give
       them back: a picture's pixels are a server resource and freeing only the
       list would leave them on the server until the connection closed. */
    if (display != NULL) {
        for (int i = 0; i < list->count; i++) {
            drop_pixels(display, &list->items[i]);
        }
    }
    free(list);
}

void term_image_list_clear(TermImageList *list, Display *display) {
    if (list == NULL) {
        return;
    }
    /* The pixels go back to the display and the count is reset, so the list is
       empty and ready to hold the next picture. The array itself is kept: it is
       a fixed set of slots and re-making it would be work for nothing. */
    if (display != NULL) {
        for (int i = 0; i < list->count; i++) {
            drop_pixels(display, &list->items[i]);
        }
    }
    memset(list->items, 0, sizeof(list->items));
    list->count = 0;
}

/* --- the file -------------------------------------------------------------- */

static int file_readable(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    return access(path, R_OK) == 0;
}

/* The path a program named, written into `out`: "~" expands to the home
   directory, and anything else is used as written. Answers 1 when the file can
   be read. */
static int resolve_path(const char *name, char *out, unsigned int size) {
    if (name == NULL || name[0] == '\0') {
        out[0] = '\0';
        return 0;
    }
    if (name[0] == '~' && (name[1] == '/' || name[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home != NULL && home[0] != '\0') {
            snprintf(out, size, "%s%s", home, name + 1);
            return file_readable(out);
        }
    }
    snprintf(out, size, "%s", name);
    return file_readable(out);
}

/* --- placing --------------------------------------------------------------- */

void term_image_place(TermImageList *list, Display *display,
                      Visual *visual, Colormap colormap, int depth,
                      int cell_width, int cell_height,
                      int x, int y, int cols, int rows, const char *path,
                      uint32_t background) {
    if (list == NULL || display == NULL) {
        return;
    }

    char resolved[1024];
    if (!resolve_path(path, resolved, sizeof(resolved))) {
        fprintf(stderr, "gcl_terminal: image '%s' was not found\n",
                path != NULL ? path : "");
        return;
    }

    if (rows < 1) rows = 1;
    if (cell_width < 1) cell_width = 8;
    if (cell_height < 1) cell_height = 16;

    /* The block, in pixels: the box the picture is fitted into. A cols of ZERO
       means "as wide as the picture needs to be at this height", which is what
       a program that wants a picture at its natural proportions and does not
       know the terminal's cell size sends — it names only the rows and lets the
       terminal work the width out from the file. */
    int free_width = cols < 1;
    int box_height = rows * cell_height;
    int box_width = free_width ? 0 : cols * cell_width;
    if (box_height < 1) box_height = 1;
    if (box_width < 0) box_width = 0;

    /* Imlib2 is told which display and visual before anything is loaded, so a
       logo with an alpha channel is decoded against the colormap it will be
       drawn on. */
    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(colormap);

    Imlib_Load_Error error = IMLIB_LOAD_ERROR_NONE;
    Imlib_Image loaded = imlib_load_image_with_error_return(resolved, &error);
    if (loaded == NULL || error != IMLIB_LOAD_ERROR_NONE) {
        if (loaded != NULL) {
            imlib_context_set_image(loaded);
            imlib_free_image();
        }
        fprintf(stderr, "gcl_terminal: image '%s' could not be read (error %d)\n",
                resolved, (int)error);
        return;
    }

    imlib_context_set_image(loaded);
    int source_width = imlib_image_get_width();
    int source_height = imlib_image_get_height();
    if (source_width < 1 || source_height < 1) {
        imlib_free_image();
        return;
    }

    /* The picture is fitted with its own proportions kept. When the width is
       free the HEIGHT alone decides and the picture's own shape gives the
       width; otherwise the SMALLER of the two scale factors is what makes one
       edge touch the block and leaves the other inside it. Nothing is ever
       stretched. */
    double scale;
    if (free_width) {
        scale = (double)box_height / (double)source_height;
    } else {
        double scale_w = (double)box_width / (double)source_width;
        double scale_h = (double)box_height / (double)source_height;
        scale = scale_w < scale_h ? scale_w : scale_h;
    }

    int pixel_width = (int)(source_width * scale + 0.5);
    int pixel_height = (int)(source_height * scale + 0.5);
    if (pixel_width < 1) pixel_width = 1;
    if (pixel_height < 1) pixel_height = 1;

    /* A free width is the picture's own; a fixed one centres the picture in it,
       so a block wider or taller than the picture leaves even margins. */
    if (free_width) {
        box_width = pixel_width;
    }
    int offset_x = (box_width - pixel_width) / 2;
    int offset_y = (box_height - pixel_height) / 2;
    if (offset_x < 0) offset_x = 0;
    if (offset_y < 0) offset_y = 0;

    /* A picture already anchored at this cell is replaced, so a program
       redrawing a panel does not stack copies on top of one another. */
    int slot = -1;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].x == x && list->items[i].y == y) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (list->count < TERM_IMAGE_MAX) {
            slot = list->count++;
        } else {
            /* The list is full: the oldest is dropped for the new one, which
               is what a person watching the screen would rather see. */
            drop_pixels(display, &list->items[0]);
            memmove(&list->items[0], &list->items[1],
                    sizeof(TermImage) * (TERM_IMAGE_MAX - 1));
            slot = TERM_IMAGE_MAX - 1;
            memset(&list->items[slot], 0, sizeof(TermImage));
        }
    } else {
        drop_pixels(display, &list->items[slot]);
    }

    /* The pixmap is the BLOCK's size, so the picture sits at the right place
       inside it without the renderer having to know where it was centred: the
       margin around the picture is part of the pixmap and is filled with the
       program's background, so the picture stands on the terminal's colour. */
    Pixmap pixmap = XCreatePixmap(display, DefaultRootWindow(display),
                                  (unsigned)box_width, (unsigned)box_height,
                                  (unsigned)depth);
    if (pixmap == None) {
        imlib_free_image();
        return;
    }

    GC gc = XCreateGC(display, pixmap, 0, NULL);
    XSetForeground(display, gc, (unsigned long)background);
    XFillRectangle(display, pixmap, gc, 0, 0,
                   (unsigned)box_width, (unsigned)box_height);
    XFreeGC(display, gc);

    imlib_context_set_image(loaded);
    imlib_context_set_drawable(pixmap);
    imlib_context_set_blend(1);
    /* Drawn at the fit size, inside the block. Imlib2 reduces by area, which is
       what keeps the picture readable. */
    imlib_render_image_on_drawable_at_size(offset_x, offset_y,
                                           pixel_width, pixel_height);
    imlib_free_image_and_decache();

    TermImage *image = &list->items[slot];
    image->x = x;
    image->y = y;
    image->cols = cols;
    image->rows = rows;
    image->pixmap = pixmap;
    image->width = box_width;
    image->height = box_height;
}

/* --- drawing --------------------------------------------------------------- */

void term_image_draw(const TermImageList *list, Display *display,
                     Drawable grid, GC gc,
                     int cell_width, int cell_height) {
    if (list == NULL || display == NULL) {
        return;
    }
    if (cell_width < 1) cell_width = 8;
    if (cell_height < 1) cell_height = 16;

    for (int i = 0; i < list->count; i++) {
        const TermImage *image = &list->items[i];
        if (image->pixmap == None) {
            continue;
        }
        /* The cell the picture was anchored at becomes the pixel it goes to:
           the picture sits exactly where the program put it, lined up with the
           text around it. */
        int x = image->x * cell_width;
        int y = image->y * cell_height;
        XCopyArea(display, image->pixmap, grid, gc,
                  0, 0, (unsigned)image->width, (unsigned)image->height,
                  x, y);
    }
}
