/*
 * term_scroll.h — the lines above the screen, and where the view sits in them.
 *
 * A terminal that keeps nothing above its top row has no scrollback: the line
 * that scrolls off is gone, and there is nothing for a scrollbar to be a
 * scrollbar OF. So this is the first of the two things a scrollbar needs, and
 * the scrollbar itself is only a drawing of it.
 *
 * --- what it holds ---
 *
 * Lines, detached from the grid. A push does not COPY cells: the line buffer
 * the grid was about to blank is handed over, and the grid is given nothing
 * back. That is the same trick the grid's own scroll uses — a line owns its
 * buffer, so moving the buffer moves the content — and it is why keeping four
 * thousand lines of history costs four thousand pointer moves and no copying.
 *
 * --- absolute lines ---
 *
 * A line is named by ONE number: the history comes first and the screen
 * follows, so absolute line 0 is the oldest thing still remembered and
 * absolute line `count + rows - 1` is the last row of the screen.
 *
 * The virtue of that ordering is that it does not move when the screen
 * scrolls. A line that was the screen's first row becomes the newest history
 * line, and its absolute number is the SAME before and after — which is what
 * lets a selection be remembered as two line numbers and still cover the same
 * text after the program has printed another twenty lines.
 *
 * --- and the one thing the scrollbar needs from it ---
 *
 * How many lines there are, and how far back the view is. Those two numbers
 * are the thumb's length and its position, and nothing else in this file is
 * the scrollbar's business.
 */
#ifndef GNUCHANTERM_SCROLL_H
#define GNUCHANTERM_SCROLL_H

#include "term_grid.h"

/* The lines kept above the screen.
 *
 * It is a ceiling and not an allocation: the ring's table is made when the
 * first line is pushed and holds this many records, and each record is a
 * pointer to a buffer the GRID allocated and gave away. Four thousand lines of
 * an 80-column terminal is about 13 MB of cells, which is what every terminal
 * spends on scrollback and is the number that makes `ls -l` scrollable through
 * rather than lost.
 *
 * At the ceiling the oldest line is let go, which is what makes it a ring and
 * not a growing list: a session that has been open for a week has a scrollback
 * of four thousand lines, not of a week. */
#define TERM_SCROLL_MAX_LINES 4096

typedef struct TermScroll {
    /* The ring. `slots` holds `capacity` records; the OLDEST is at `head` and
       they run from there, wrapping. Allocated on the first push, because a
       screen that is only ever the alternate one — a full-screen program with
       no shell in front of it — never pushes and never needs the table. */
    TermLine *slots;
    int capacity;
    int count;     /* how many slots hold a line                */
    int head;      /* the slot holding the OLDEST of them       */
    int cols;      /* the width every buffer here is; see .c    */

    /* How far the view is scrolled back from the live screen, in lines. Zero
       is the live screen, which is where it always returns to; `count` is as
       far back as there is to go, with the oldest remembered line at the top.
       It can never exceed `count`, and it does not have to be clamped by a
       reader because every function here keeps it there. */
    int offset;
} TermScroll;

/* --- life ----------------------------------------------------------------- */

void term_scroll_init(TermScroll *scroll);
void term_scroll_free(TermScroll *scroll);

/* Throw the history away and go back to the live screen. What a reset of the
   screen means: the program asked for the screen to be cleared, and history
   that is no longer above anything is history nobody can reach. */
void term_scroll_clear(TermScroll *scroll);

/* --- history -------------------------------------------------------------- */

/* Take a line's buffer into the history, giving up the oldest line when the
   ring is full. The pointer is TAKEN, not copied: the caller hands over what
   it had and must not use it afterwards — it is the grid's line buffer, moved.
 *
 * Returns how many lines were LET GO to make room — zero or one — because a
 * selection anchored to absolute line numbers shifts when the oldest goes and
 * the caller is the only one that can fix it. */
int term_scroll_push(TermScroll *scroll, TermCell *cells, int cols);

/* How many lines there are above the screen. */
int term_scroll_count(const TermScroll *scroll);

/* How wide the lines here are. A reader uses it to know how much of a returned
   line it may draw, and it is the same for every line — see the .c file. */
int term_scroll_cols(const TermScroll *scroll);

/* One of the remembered lines' cells, 0 being the oldest. NULL outside the
   range. The cells belong to the ring and must not be kept. */
const TermCell *term_scroll_cells(const TermScroll *scroll, int index);

/* --- the view ------------------------------------------------------------- */

/* How far back the view is, and how far it may go. */
int term_scroll_offset(const TermScroll *scroll);
int term_scroll_max_offset(const TermScroll *scroll);

/* Move the view by a number of lines, clamped to [0, count]. A positive
   number scrolls BACK — towards older lines — which is the direction of a
   wheel pushed away from the user and of a Page Up. Returns 1 when the view
   actually moved, so a caller that has to redraw only redraws when it did. */
int term_scroll_by(TermScroll *scroll, int lines);

/* Put the view back on the live screen. This is what anything arriving from
   the program does: a terminal that stayed scrolled back while output poured
   in would be a terminal whose prompt was somewhere unseen. */
void term_scroll_to_bottom(TermScroll *scroll);

/* The absolute index of the top visible row. Absolute line 0 is the oldest
   remembered line; the screen's first row is absolute `count`. */
int term_scroll_view_top(const TermScroll *scroll);

#endif /* GNUCHANTERM_SCROLL_H */
