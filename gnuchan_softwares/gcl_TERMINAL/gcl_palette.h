/*
 * gcl_palette.h — the desktop's colours, in one place.
 *
 * Every colour the terminal draws with is named here, once, and the numbers are
 * the purple theme the rest of the system is themed with — the same editor
 * background, the same text, and the same sixteen ANSI names that the theme's
 * terminal section lists. A terminal window and an editor window are therefore
 * two views of one palette rather than two palettes that happen to be purple.
 *
 * It is a header of its own and not a block inside term_style.c because the
 * values are the part a person changes. Nothing here knows what a cell or an
 * XftColor is; the file is a list of numbers with names.
 *
 * --- the one cost, written down ---
 *
 * The sixteen names exist so that a program can tell things apart with them:
 * `ls` paints a directory one colour and an executable another, and a compiler
 * paints an error one colour and a warning another. Sixteen shades of violet
 * are harder to tell apart than sixteen hues are, and no amount of taste
 * changes that. What is done about it is SPREAD — the entries step across
 * lightness and saturation rather than sitting close together — so a directory
 * and a file are still two clearly different purples. A program that needs a
 * sharper distinction has the 256-colour and truecolor forms, and both are
 * honoured.
 */

 #ifndef GNUCHANTERM_PALETTE_H
#define GNUCHANTERM_PALETTE_H

#include <stdint.h>

/* The terminal's own background and text. These two are the terminal's face:
   every cell a program never coloured is drawn in them, which on a terminal is
   most of what is on the screen. They are the theme's editor colours so that a
   terminal window sits beside an editor window without either looking wrong. */
#define GCL_BG     0x09030Du

/* The terminal's own text. The theme says #EAD7FF here, and #EAD7FF is 92% red
   and 84% green — a near-white that reads as plain white on a dark background
   and not as a violet at all. Every character a program never coloured is drawn
   in this, so it is the colour of the text a person types and reads, and it is
   the one place a white cannot be left in a violet theme. The value below is
   the same lightness with the green pulled down and the blue kept up. */
#define GCL_FG     0xDDB3FFu

/* The cell the cursor sits on, and the block drawn over it. */
#define GCL_CURSOR 0xDDB3FFu

/* The fish-style suggestion: the ghost of a command taken from the history,
   drawn faintly after the cursor — see term_suggest.h. It is the terminal's own
   colour and not a palette entry, for the same reason the cursor's is: the
   ghost is the terminal offering something, not a program drawing, and no SGR
   can name it. It is DIMMER than the text — the same hue pulled towards the
   background — so a suggestion reads as something offered and not as something
   already typed, which is the whole of what makes it usable rather than
   confusing. */
#define GCL_SUGGEST 0x7A5A96u

/* The bar along the bottom. Its background is the terminal's OWN background and
   not a second colour, which is the whole of how it stops looking like a
   border: a strip in another colour reads as a frame around the text, and a
   strip in the same colour reads as part of the window. The one row of text on
   it is what marks it, and that is enough. The same is true of the margin down
   each side, which is why there is no second background constant for it. */
#define GCL_BAR_BG GCL_BG
#define GCL_BAR_FG GCL_FG

/* The sixteen colours a program names by number, in the order every terminal
   has used since the eighties. The values are the theme's terminal.ansi* and
   terminal.ansiBright* entries, unchanged. */
#define GCL_ANSI_BLACK          0x170A20u   /* the theme's own dark        */
#define GCL_ANSI_RED            0xC084FCu
#define GCL_ANSI_GREEN          0xB56CFFu
#define GCL_ANSI_YELLOW         0xD8A4FFu
#define GCL_ANSI_BLUE           0x9D4EDDu
#define GCL_ANSI_MAGENTA        0xC77DFFu
#define GCL_ANSI_CYAN           0xB76EFFu
#define GCL_ANSI_WHITE          0xD8A4FFu   /* the theme's #EAD7FF is white  */
#define GCL_ANSI_BRIGHT_BLACK   0x70458Au   /* the theme's comment grey    */
#define GCL_ANSI_BRIGHT_RED     0xD8A4FFu
#define GCL_ANSI_BRIGHT_GREEN   0xC084FCu
#define GCL_ANSI_BRIGHT_YELLOW  0xE0AAFFu
#define GCL_ANSI_BRIGHT_BLUE    0xB76EFFu
#define GCL_ANSI_BRIGHT_MAGENTA 0xE0AAFFu
#define GCL_ANSI_BRIGHT_CYAN    0xD8A4FFu
#define GCL_ANSI_BRIGHT_WHITE   0xE0AAFFu   /* the theme's #FFFFFF is white  */

/* The SIXTEEN a program names by number, and nothing else. The theme's own text
   and background are NOT here: they sit past the 256 a program can name — see
   TERM_COLOR_INDEX_FG in term_grid.h — and the 6x6x6 cube and greyscale ramp
   between are the standard's and are built by term_palette_fill_standard().
 *
 * This is the whole of what a theme decides about the palette: the sixteen it
 * chose. Everything from 16 to 255 means what a program means by it, whatever
 * the theme looks like, and the two past that are the theme's face. */
#define GCL_ANSI_COUNT 16
#define GCL_PALETTE_INIT {                                       \
    GCL_ANSI_BLACK, GCL_ANSI_RED, GCL_ANSI_GREEN, GCL_ANSI_YELLOW, \
    GCL_ANSI_BLUE, GCL_ANSI_MAGENTA, GCL_ANSI_CYAN, GCL_ANSI_WHITE, \
    GCL_ANSI_BRIGHT_BLACK, GCL_ANSI_BRIGHT_RED,                   \
    GCL_ANSI_BRIGHT_GREEN, GCL_ANSI_BRIGHT_YELLOW,                \
    GCL_ANSI_BRIGHT_BLUE, GCL_ANSI_BRIGHT_MAGENTA,                \
    GCL_ANSI_BRIGHT_CYAN, GCL_ANSI_BRIGHT_WHITE }

#endif /* GNUCHANTERM_PALETTE_H */

