/*
 * term_select.h — selecting text with the mouse, and putting it on the
 * clipboard.
 *
 * Two things that are one thing to the hand: a drag with the left button marks
 * a run of text, and Ctrl+Shift+C puts it where another program can paste it.
 * They are together here because the second is meaningless without the first
 * and neither belongs to the renderer or to the input module.
 *
 * --- the selection is remembered in ABSOLUTE lines ---
 *
 * Not in screen rows, and that is the whole point of the numbering the scroll
 * history uses: the program keeps printing while the user reads, lines move up
 * the screen, and a selection remembered as "rows 4 to 7" would cover different
 * text a second later. Remembered as "absolute lines 214 to 217" it stays over
 * the words that were picked out, wherever those words have since moved to —
 * including off the top of the screen and into the history, where it can still
 * be copied.
 *
 * --- what "selected" means for a cell ---
 *
 * Whether the cell is inside the run, asked one cell at a time by the renderer
 * as it walks a row. The alternative — building a list of runs once — is a list
 * that has to be rebuilt every time the program prints, and the walk is over
 * cells the renderer is already visiting.
 */
 
#ifndef GNUCHANTERM_SELECT_H
#define GNUCHANTERM_SELECT_H

#include "term_core.h"

/* What the selection is, and the clipboard it feeds.
 *
 * The text of the clipboard is HELD here rather than handed to the server once:
 * X is asynchronous and the requestor may ask for it long after the key was
 * pressed — minutes, if it is a program that was busy. Keeping the string is
 * what makes the answer possible at all; a terminal that freed it would answer
 * a later request with nothing. */
typedef struct TermSelect {
    int  active;
    int  dragging;

    /* The two ends, in absolute line numbers and columns. Either may be the
       earlier one: the user can drag up as easily as down, and the code that
       extracts text sorts them rather than the code that records them. */
    int  anchor_line;
    int  anchor_col;
    int  cursor_line;
    int  cursor_col;

    /* The text last copied, and how much of it there is. It is what a
       SelectionRequest is answered with. */
    char *clipboard;
    int   clipboard_len;

    /* Whether this terminal currently owns the X selection. Owning it is what
       obliges us to answer requests; a terminal that set the owner and then
       forgot would be a terminal that breaks every paste on the desktop for as
       long as it ran. */
    int  owns_selection;

    /* A paste that has been asked for and not yet answered. X answers
       asynchronously, so between the key and the text there is a round trip
       through the selection's owner; this says one is in flight, so an answer
       that arrives with a property of None is read as "the owner had nothing"
       rather than being mistaken for some other program's traffic. */
    int  paste_pending;
    Atom paste_property;
} TermSelect;

void term_select_init(TermSelect *select);
void term_select_free(TermSelect *select);

/* --- the drag ------------------------------------------------------------- */

/* Begin, extend and end a selection, from a position in WINDOW pixels.
 *
 * The pixel becomes an absolute line and a column through the grid, which knows
 * where the view is and which of its rows is a remembered line — so a drag over
 * history selects history, and a drag over the live screen selects the live
 * screen, without either caller knowing the difference. */
void term_select_begin(TermCore *core, int x, int y);
void term_select_extend(TermCore *core, int x, int y);
void term_select_end(TermCore *core);
void term_select_clear(TermCore *core);

/* --- for the renderer ----------------------------------------------------- */

/* Whether the cell at a SCREEN row and column is inside the selection. The
   renderer asks this for every cell it draws: it is two comparisons and it is
   the only thing the drawing code has to know about selections. */
int term_select_covers(const TermSelect *select, const TermGrid *grid,
                       int row, int col);

/* --- the clipboard -------------------------------------------------------- */

/* Put the selection on the X clipboard. Nothing happens when there is nothing
   selected, so the key is safe to press at any time.
 *
 * XA_PRIMARY is used, which is what a middle-click pastes and what a terminal's
 * own selection has meant since before there was a clipboard; CLIPBOARD is set
 * too, because everything written this century pastes from that one. Both are
 * owned by the same string, so either gesture gets the same text. */
void term_select_copy(TermCore *core);

/* --- the other direction: pasting ----------------------------------------- */

/* Ask the CLIPBOARD's owner for its text, and hand what comes back to the
 * program. This is Ctrl+Shift+V.
 *
 * It is a two-part operation and that is X's doing, not a choice made here: a
 * program does not READ a selection, it ASKS for it and waits for a
 * SelectionNotify carrying the answer. So this half only sends the request;
 * the answer arrives later as an event and is finished by
 * term_select_event(). A terminal that sent the request and never listened
 * would be a terminal whose paste key did nothing.
 *
 * The text is written to the PTY through term_input_send(), which is the same
 * path a keystroke takes — so a pasted newline is an Enter and a pasted page of
 * text is many characters, not one. A program that asked for bracketed paste
 * gets the text wrapped in the markers that say so, which is what lets it tell
 * a paste from typing and not run the lines as commands. */
void term_select_paste(TermCore *core);

/* Answer the events the two halves of a selection use: a SelectionRequest from
   another program asking for our text, a SelectionClear when ours is taken
   away, and a SelectionNotify carrying the answer to a paste. Called by the
   core's own dispatch; returns 1 when the event was one of these and has been
   dealt with, so nothing further acts on it. */
int term_select_event(TermCore *core, XEvent *event);

/* --- selecting from the keyboard ------------------------------------------ */

/* Move the moving end of the selection, and start one at the terminal's cursor
   when there is none yet. `lines` and `cols` are how far to go — negative is up
   and left — and `word` says to move by whole words rather than by characters,
   which is what the ctrl in Ctrl+Shift+arrow means.
 *
 * This is the keyboard's half of what a drag does. It exists because a mouse is
 * not always the thing in hand and because a selection made with the keys can
 * be made precisely, which dragging over a fixed-width grid is not. */
void term_select_key(TermCore *core, int lines, int cols, int word);

#endif /* GNUCHANTERM_SELECT_H */
