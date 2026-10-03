/*
 * term_image.h — pictures drawn in the terminal as pictures.
 *
 * A terminal normally carries text, and a "picture" in it is a grid of
 * characters. That is a lie about what was drawn: a photo, a logo and a graph
 * all come out as a field of blocks, and a detailed one comes out as noise.
 * This is the other way: a program tells the terminal where a picture goes,
 * and the terminal draws the PICTURE — the pixels that were in the file — over
 * those cells. Nothing is turned into characters and nothing is decoded by the
 * program.
 *
 * --- how a program asks ---
 *
 * With an OSC string of this terminal's own:
 *
 *     ESC ] 1338 ; xoff ; yoff ; cols ; rows ; path BEL
 *
 *   xoff, yoff  where the picture's top-left corner goes, in CELLS counted
 *               from the cursor. A program that draws a picture inline sends
 *               0;0 and the picture stands where its next character would.
 *   cols, rows  the block of cells the picture is fitted into, in columns and
 *               text rows. The picture keeps its own proportions inside that
 *               block, so it is never stretched — a block wider than the
 *               picture leaves the picture centred in it.
 *   path        the file, as a path a program would open — a leading "~" is
 *               the home directory.
 *
 * The escape is a string like every OSC, so a terminal that does not know it
 * consumes it whole and draws nothing — a program that sends one is not broken
 * on a terminal that ignores it, it simply has no picture there. The number is
 * past the range ECMA-48 assigns, which is where a private sequence belongs.
 *
 * --- what is kept ---
 *
 * Each placed picture is scaled ONCE, when it is placed, into an X pixmap of
 * its own, and a frame is then one XCopyArea per picture. Scaling per frame
 * would be Imlib2 resampling the file on every keystroke, and the picture is
 * the same picture every time.
 *
 * The list is small and short-lived by design: a picture is placed where the
 * program is looking, and the moment the screen scrolls or clears it is gone.
 * A terminal is not a document viewer, and a picture that outlived its cells
 * would sit over whatever the program drew next.
 */
#ifndef GNUCHANTERM_IMAGE_H
#define GNUCHANTERM_IMAGE_H

#include <stdint.h>

#include <X11/Xlib.h>

/* The opaque list of placed pictures, owned by the renderer. */
typedef struct TermImageList TermImageList;

/* An empty list, or NULL when there is no memory for one. */
TermImageList *term_image_list_new(void);

/* Drop every picture and the pixels each was scaled into, and the list itself.
   The display is needed to free the pixmaps, which belong to it. Safe on
   NULL. */
void term_image_list_free(TermImageList *list, Display *display);

/* Drop every picture but keep the list, so it can be filled again. This is what
   an erase does: `clear` in a shell sends ED, and a picture that outlived the
   cells it covered would sit over the fresh prompt the shell prints next. The
   display frees the pixmaps. Safe on NULL. */
void term_image_list_clear(TermImageList *list, Display *display);

/* Place the picture at `path` with its top-left CELL at (x, y), fitted into a
   block `cols` wide and `rows` tall. The picture keeps its own proportions
   inside the block.
 *
 * The picture is loaded with Imlib2 — the same library the window manager draws
 * its wallpaper with — scaled once to a pixmap of its own, and kept. A picture
 * that cannot be read is not placed and is not an error the program is told
 * about: it asked for a thing that is not there, and the cells it named simply
 * stay whatever they were.
 *
 * A picture already anchored at this cell is replaced, so a program redrawing
 * the same panel does not stack copies. */
void term_image_place(TermImageList *list, Display *display,
                      Visual *visual, Colormap colormap, int depth,
                      int cell_width, int cell_height,
                      int x, int y, int cols, int rows, const char *path,
                      uint32_t background);

/* Draw every placed picture onto the grid surface, over whatever is there.
   The pictures are copied, not rescaled — see the file comment. The cell size
   is what turns a picture's cell position into a pixel position. */
void term_image_draw(const TermImageList *list, Display *display,
                     Drawable grid, GC gc,
                     int cell_width, int cell_height);

#endif /* GNUCHANTERM_IMAGE_H */
