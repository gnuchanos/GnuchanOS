/*
 * term_select.c — the drag, the highlight, and the clipboard.
 *
 * See term_select.h for why a selection is remembered in absolute lines. What
 * is here is the arithmetic that turns a pixel into one, the test the renderer
 * asks per cell, and the two halves of the X selection protocol.
 *
 * --- the two halves of a copy ---
 *
 * Setting the selection owner looks like the whole of it and is only half. X
 * does not copy the text anywhere: the terminal says "I own this" and then, at
 * some later moment, whoever wants the text sends a SelectionRequest and waits
 * for the terminal to answer with the data. That is why the string is kept in
 * TermSelect rather than handed over and forgotten, and why this file has an
 * event handler at all.
 *
 * A terminal that set the owner and then closed — or freed the string — would
 * leave every paste on the desktop empty for as long as the requestor waited,
 * which is between five and thirty seconds of a program doing nothing. That is
 * the fault the string's lifetime here exists to not have.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>

#include "term_select.h"
#include "term_style.h"

/* Room for the extracted text, and the ceiling on it.
 *
 * A selection is a run of lines and it can be the whole scrollback if the user
 * drags from the top: four thousand lines of eighty columns is about 330 KB,
 * which is exactly the size of thing a clipboard holds and is why this is a
 * buffer that doubles rather than a fixed one. The ceiling is there so a drag
 * across a pathological history cannot take the machine's memory. */
#define SELECT_INITIAL_CAPACITY 1024
#define SELECT_MAX_BYTES (4 * 1024 * 1024)

void term_select_init(TermSelect *select) {
    if (select == NULL) {
        return;
    }
    memset(select, 0, sizeof(*select));
}

void term_select_free(TermSelect *select) {
    if (select == NULL) {
        return;
    }
    free(select->clipboard);
    memset(select, 0, sizeof(*select));
}

/* --- the cells of an absolute line ---------------------------------------- */

/* The cells of a line by its absolute number: the history answers for what it
   kept and the grid for the screen, which is the same split the grid's own
   resolver makes. This is the one other caller that needs a line by number
   rather than by screen row, because a selection can reach into the history. */
static const TermCell *cells_of_line(const TermGrid *grid, int absolute,
                                     int *cols) {
    if (cols != NULL) {
        *cols = 0;
    }
    if (grid == NULL) {
        return NULL;
    }

    int remembered = (grid->history != NULL && grid->history->count != NULL)
                         ? grid->history->count(grid->history->user)
                         : 0;

    if (absolute < remembered) {
        if (grid->history == NULL || grid->history->line == NULL) {
            return NULL;
        }
        return grid->history->line(grid->history->user, absolute, cols);
    }

    int row = absolute - remembered;
    if (row < 0 || row >= grid->rows) {
        return NULL;
    }
    if (cols != NULL) {
        *cols = grid->cols;
    }
    return grid->lines[row].cells;
}

/* --- pixels become absolute positions ------------------------------------- */

/* Where a point in the window is, as a line and a column.
 *
 * The frame comes off first and the division by the cell size is the whole of
 * the conversion — the same arithmetic the input module does for a mouse
 * report, and for the same reason: the grid does not start at the window's
 * corner.
 *
 * A point outside the grid is CLAMPED rather than refused. A drag that leaves
 * the window — which every drag of more than a line does — must keep extending
 * the selection, and the natural thing for it to do at the edge is to stop
 * there and hold. */
static void point_to_position(const TermCore *core, int x, int y,
                              int *line, int *col) {
    int origin_x = 0;
    int origin_y = 0;
    term_core_grid_origin(core, &origin_x, &origin_y);

    int cell_w = core->style != NULL ? core->style->cell_width : 8;
    int cell_h = core->style != NULL ? core->style->cell_height : 16;
    if (cell_w < 1) cell_w = 8;
    if (cell_h < 1) cell_h = 16;

    int local_x = x - origin_x;
    int local_y = y - origin_y;
    if (local_x < 0) local_x = 0;
    if (local_y < 0) local_y = 0;

    int col_value = local_x / cell_w;
    int row = local_y / cell_h;

    const TermGrid *grid = &core->vt.grid;
    if (col_value >= grid->cols) col_value = grid->cols - 1;
    if (row >= grid->rows) row = grid->rows - 1;
    if (col_value < 0) col_value = 0;
    if (row < 0) row = 0;

    if (line != NULL) {
        *line = term_grid_line_number(grid, row);
    }
    if (col != NULL) {
        *col = col_value;
    }
}

/* --- the drag ------------------------------------------------------------- */

void term_select_begin(TermCore *core, int x, int y) {
    if (core->select == NULL) {
        return;
    }
    TermSelect *select = (TermSelect *)core->select;

    int line = 0;
    int col = 0;
    point_to_position(core, x, y, &line, &col);
    select->anchor_line = line;
    select->anchor_col = col;
    select->cursor_line = line;
    select->cursor_col = col;
    select->active = 1;
    select->dragging = 1;
    term_core_damage(core);
}

void term_select_extend(TermCore *core, int x, int y) {
    if (core->select == NULL) {
        return;
    }
    TermSelect *select = (TermSelect *)core->select;
    if (!select->dragging) {
        return;
    }

    int line = 0;
    int col = 0;
    point_to_position(core, x, y, &line, &col);
    if (line == select->cursor_line && col == select->cursor_col) {
        return;   /* nothing moved, so a redraw would be for nothing */
    }
    select->cursor_line = line;
    select->cursor_col = col;
    term_core_damage(core);
}

void term_select_end(TermCore *core) {
    if (core->select == NULL) {
        return;
    }
    TermSelect *select = (TermSelect *)core->select;
    select->dragging = 0;

    /* A click with no drag selects one cell, which is a selection of nothing.
       Dropping it is what makes a plain click behave like a click — putting
       the cursor where it was pointed at — instead of leaving a one-character
       highlight behind for the next thing to trip over. */
    if (select->anchor_line == select->cursor_line &&
        select->anchor_col == select->cursor_col) {
        select->active = 0;
        term_core_damage(core);
    }
}

void term_select_clear(TermCore *core) {
    if (core->select == NULL) {
        return;
    }
    TermSelect *select = (TermSelect *)core->select;
    if (!select->active) {
        return;
    }
    select->active = 0;
    select->dragging = 0;
    term_core_damage(core);
}

/* --- what the renderer asks ----------------------------------------------- */

int term_select_covers(const TermSelect *select, const TermGrid *grid,
                       int row, int col) {
    if (select == NULL || !select->active) {
        return 0;
    }

    int line = term_grid_line_number(grid, row);

    int first_line = select->anchor_line;
    int first_col = select->anchor_col;
    int last_line = select->cursor_line;
    int last_col = select->cursor_col;

    /* The drag may have gone either way, so the ends are sorted HERE rather
       than being kept in order as they are recorded: the hand does not know
       which end it started at, and the recording code has no business
       guessing. */
    if (first_line > last_line ||
        (first_line == last_line && first_col > last_col)) {
        int swap = first_line; first_line = last_line; last_line = swap;
        swap = first_col; first_col = last_col; last_col = swap;
    }

    if (line < first_line || line > last_line) {
        return 0;
    }
    if (first_line == last_line) {
        return col >= first_col && col <= last_col;
    }
    if (line == first_line) {
        return col >= first_col;
    }
    if (line == last_line) {
        return col <= last_col;
    }
    /* A line in the middle is selected whole, which is what dragging over
       several lines means. */
    return 1;
}

/* --- the text ------------------------------------------------------------- */

/* A buffer that doubles, because the answer's size is not known until the
   lines have been walked, and walking them twice to find out would double the
   work for nothing. */
typedef struct SelectText {
    char *bytes;
    int   length;
    int   capacity;
    int   failed;
} SelectText;

static void text_append(SelectText *text, const char *bytes, int count) {
    if (text->failed || count <= 0) {
        return;
    }
    if (text->length + count > SELECT_MAX_BYTES) {
        text->failed = 1;
        return;
    }
    if (text->length + count > text->capacity) {
        int grown = text->capacity ? text->capacity : SELECT_INITIAL_CAPACITY;
        while (grown < text->length + count) {
            grown *= 2;
        }
        char *bigger = realloc(text->bytes, (size_t)grown);
        if (bigger == NULL) {
            text->failed = 1;
            return;
        }
        text->bytes = bigger;
        text->capacity = grown;
    }
    memcpy(text->bytes + text->length, bytes, (size_t)count);
    text->length += count;
}

/* A code point as the bytes a paste expects. It is written out here rather
   than taken from a library because it is four lines and this program has no
   other need for a UTF-8 encoder. */
static void text_append_codepoint(SelectText *text, uint32_t cp) {
    char utf8[4];
    int len = 0;
    if (cp < 0x80) {
        utf8[len++] = (char)cp;
    } else if (cp < 0x800) {
        utf8[len++] = (char)(0xC0 | (cp >> 6));
        utf8[len++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        utf8[len++] = (char)(0xE0 | (cp >> 12));
        utf8[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        utf8[len++] = (char)(0x80 | (cp & 0x3F));
    } else {
        utf8[len++] = (char)(0xF0 | (cp >> 18));
        utf8[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        utf8[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        utf8[len++] = (char)(0x80 | (cp & 0x3F));
    }
    text_append(text, utf8, len);
}

/* The trailing blanks of a line are not part of what was selected.
 *
 * They are on the screen because a line has a fixed width, and copying them
 * would put a run of spaces after every line of a paste — which is invisible
 * until it is pasted into something that counts columns, and then it is a
 * complaint about the terminal. */
static int line_text_end(const TermCell *cells, int cols) {
    int end = cols;
    while (end > 0 && cells[end - 1].ch == 0) {
        end--;
    }
    return end;
}

/* Sort the two ends, which everything that reads them has to do. It is one
   function so there is one place that decides which end comes first. */
static void sort_ends(const TermSelect *select, int *first_line, int *first_col,
                      int *last_line, int *last_col) {
    *first_line = select->anchor_line;
    *first_col = select->anchor_col;
    *last_line = select->cursor_line;
    *last_col = select->cursor_col;

    if (*first_line > *last_line ||
        (*first_line == *last_line && *first_col > *last_col)) {
        int swap = *first_line; *first_line = *last_line; *last_line = swap;
        swap = *first_col; *first_col = *last_col; *last_col = swap;
    }
}

static void extract_text(const TermGrid *grid, const TermSelect *select,
                         SelectText *text) {
    int first_line = 0;
    int first_col = 0;
    int last_line = 0;
    int last_col = 0;
    sort_ends(select, &first_line, &first_col, &last_line, &last_col);

    for (int line = first_line; line <= last_line && !text->failed; line++) {
        int cols = 0;
        const TermCell *cells = cells_of_line(grid, line, &cols);
        if (cells == NULL || cols <= 0) {
            /* A line that is no longer anywhere — it was let go from the top
               of the ring while the drag was in progress. A blank line is the
               honest answer; stopping would truncate the paste silently. */
            text_append(text, "\n", 1);
            continue;
        }

        int from = 0;
        int to = cols - 1;
        if (line == first_line) {
            from = first_col;
        }
        if (line == last_line) {
            to = last_col < cols ? last_col : cols - 1;
        }

        /* Only the last line has its padding trimmed, because it is the only
           one whose end is the end of the selection: a middle line is selected
           whole and its trailing spaces are inside the run. */
        if (line == last_line) {
            int end = line_text_end(cells, cols);
            if (to > end - 1) {
                to = end - 1;
            }
        }

        for (int x = from; x <= to && x < cols; x++) {
            if (x < 0) {
                continue;
            }
            const TermCell *cell = &cells[x];
            if (cell->wide_cont) {
                continue;   /* the second half of a glyph is not a character */
            }
            if (cell->ch == 0) {
                text_append(text, " ", 1);
            } else {
                text_append_codepoint(text, cell->ch);
            }
        }

        if (line != last_line) {
            text_append(text, "\n", 1);
        }
    }
}

/* --- the X selection ------------------------------------------------------ */

void term_select_copy(TermCore *core) {
    if (core->select == NULL || core->display == NULL) {
        return;
    }
    TermSelect *select = (TermSelect *)core->select;
    if (!select->active) {
        return;
    }

    SelectText text;
    memset(&text, 0, sizeof(text));
    extract_text(&core->vt.grid, select, &text);
    if (text.failed || text.length == 0) {
        free(text.bytes);
        return;
    }

    free(select->clipboard);
    select->clipboard = text.bytes;
    select->clipboard_len = text.length;

    /* Owning BOTH of the selections is what makes the copy work whichever way
       the pasting program asks. XA_PRIMARY is the one a middle-click uses and
       the one a terminal's own selection has meant since before there was a
       clipboard; CLIPBOARD is the one Ctrl+V uses and the one every program
       written this century asks for. The same string answers both, so the two
       gestures cannot give different text. */
    Display *display = core->display;
    Window window = core->window;
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    XSetSelectionOwner(display, XA_PRIMARY, window, CurrentTime);
    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    select->owns_selection = 1;
    XFlush(display);
}

/* One SelectionRequest, answered.
 *
 * The reply has three fields that matter and one that is easy to get wrong: the
 * `property` the requestor named is where the data goes, and a request that
 * transfers nothing — the requestor asked for a type this holds nothing in —
 * must still be answered, with an event whose property is None. An unanswered
 * request leaves the other program waiting for the full timeout, which is the
 * difference between a paste that is instant and one that hangs for half a
 * minute and then does nothing. */
static void answer_selection_request(TermCore *core,
                                     XSelectionRequestEvent *request) {
    TermSelect *select = (TermSelect *)core->select;
    Display *display = core->display;

    XSelectionEvent reply;
    memset(&reply, 0, sizeof(reply));
    reply.type = SelectionNotify;
    reply.display = display;
    reply.requestor = request->requestor;
    reply.selection = request->selection;
    reply.target = request->target;
    reply.time = request->time;
    reply.property = None;

    Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    Atom targets = XInternAtom(display, "TARGETS", False);
    Atom property = request->property != None ? request->property
                                             : request->target;

    if (select != NULL && select->clipboard != NULL &&
        select->clipboard_len > 0) {
        if (request->target == targets) {
            /* "What can you give me?" — the list is what makes a well-behaved
               requestor ask for UTF8_STRING rather than for TEXT, which on a
               modern desktop is the difference between correct text and
               mojibake for anything not in ASCII. */
            Atom offered[2];
            offered[0] = utf8;
            offered[1] = XA_STRING;
            XChangeProperty(display, request->requestor, property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)offered, 2);
            reply.property = property;
        } else if (request->target == utf8 || request->target == XA_STRING) {
            XChangeProperty(display, request->requestor, property,
                            request->target, 8, PropModeReplace,
                            (unsigned char *)select->clipboard,
                            select->clipboard_len);
            reply.property = property;
        }
    }

    XSendEvent(display, request->requestor, False, 0, (XEvent *)&reply);
    XFlush(display);
}

/* The selection was taken by another program while this one held it. Owning a
   selection is a promise to answer, and this is the other half of it: the
   promise is over and no further request will come. */
static void selection_lost(TermCore *core) {
    if (core->select != NULL) {
        TermSelect *select = (TermSelect *)core->select;
        select->owns_selection = 0;
    }
}

int term_select_event(TermCore *core, XEvent *event) {
    if (event->type == SelectionRequest) {
        answer_selection_request(core, &event->xselectionrequest);
        return 1;
    }
    if (event->type == SelectionClear) {
        selection_lost(core);
        return 1;
    }
    return 0;
}
