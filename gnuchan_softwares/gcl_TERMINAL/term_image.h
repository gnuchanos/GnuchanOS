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
 * program is looking and belongs to that place in the text. It is anchored to
 * a LINE and not to a screen cell, so it rises with the text above it as the
 * screen scrolls and follows the view when the user scrolls back — and it is
 * dropped outright when the screen is cleared, so it never sits over the fresh
 * prompt a program prints next. A picture placed on one screen is drawn only
 * while that screen is the one showing, so a logo on the shell's screen does
 * not stand over a full-screen program's.
 */
 
#ifndef GNUCHANTERM_IMAGE_H
#define GNUCHANTERM_IMAGE_H

#include <stdint.h>

#include <X11/Xlib.h>

#include "term_grid.h"

/* The opaque list of placed pictures, owned by the renderer. */
typedef struct TermImageList TermImageList;

/* An empty list, or NULL when there is no memory for one. */
TermImageList *term_image_list_new(void);

/* Drop every picture and the pixels each was scaled into, and the list itself.
   The display is needed to free the pixmaps, which belong to it. Safe on
   NULL. */
void term_image_list_free(TermImageList *list, Display *display);

/* Drop the pictures that belong to ONE screen and free their pixmaps. `alt` is
   the screen: 0 the main one, 1 the alternate. This is what an erase does —
   `clear` in a shell sends ED — and it is per SCREEN because an erase on the
   alternate screen must not take the logo the shell put on the main one, nor
   the other way round. The display frees the pixmaps. Safe on NULL. */
void term_image_clear_screen(TermImageList *list, Display *display, int alt);

/* Place the picture at `path` with its top-left CELL at column `x` on the
   screen `alt`, anchored to the ABSOLUTE line `line`, fitted into a block
   `cols` wide and `rows` tall.
 *
 * `line` is the line number the history numbers lines with — the value
 * term_grid_line_number() answers with — and not a screen row. That is what
 * makes the picture rise with the text above it as the screen scrolls and
 * follow the view when the user scrolls back: a picture anchored to a screen
 * row stays where it was put while the text moves out from under it, which is
 * a logo that slowly floats down the screen.
 *
 * `alt` records which screen it belongs to: 0 the main one, 1 the alternate. A
 * picture placed on the shell's screen is then drawn only while that screen is
 * showing, so it does not stand over a full-screen program's screen.
 *
 * The picture is loaded with Imlib2 — the same library the window manager draws
 * its wallpaper with — scaled once to a pixmap of its own, and kept. A picture
 * that cannot be read is not placed and is not an error the program is told
 * about: it asked for a thing that is not there, and the cells it named simply
 * stay whatever they were.
 *
 * A picture already anchored at this column on the same line and screen is
 * replaced, so a program redrawing the same panel does not stack copies. */
void term_image_place(TermImageList *list, Display *display,
                      Visual *visual, Colormap colormap, int depth,
                      int cell_width, int cell_height,
                      int x, int line, int alt, int cols, int rows,
                      const char *path, uint32_t background);

/* Draw every placed picture of the showing screen onto the grid surface, over
   whatever is there. The pictures are copied, not rescaled — see the file
   comment. `screen_grid` is the grid that IS showing, and it is what turns a
   picture's absolute line into the screen row to draw it at; `alt_active` is
   which screen that is, and a picture of the other one is skipped. The cell
   size is what turns a cell position into a pixel position. */
void term_image_draw(const TermImageList *list, Display *display,
                     Drawable grid, GC gc, const TermGrid *screen_grid,
                     int alt_active, int cell_width, int cell_height);

#endif /* GNUCHANTERM_IMAGE_H */
