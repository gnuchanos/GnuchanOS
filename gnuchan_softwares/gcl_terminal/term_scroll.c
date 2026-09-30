/*
 * term_scroll.c — the history ring, and the view into it.
 *
 * See term_scroll.h for what is kept and why a line is named by one absolute
 * number. What is here is a fixed-size ring of line buffers, and three
 * arithmetic operations on it.
 *
 * --- why a ring, and why one width ---
 *
 * A ring is bounded in memory and always remembers the MOST RECENT lines,
 * which is what a scrollback is for: nobody scrolls back four thousand lines
 * to find a line from before they started.
 *
 * Every line here is the SAME width — the width the terminal had when the
 * first line was handed over. A line that arrives at another width after a
 * resize is copied into a buffer of the ring's width, which costs one memcpy
 * of at most a few hundred cells and buys something worth more than it costs:
 * a reader never has to ask how wide this particular line is. The alternative
 * — a width per line — is a second array to keep in step and a field for every
 * reader to check, for a case that only happens when the window is resized.
 *
 * The copy is ONLY on the resize path. In the ordinary case the grid hands
 * over a line and the ring takes the pointer: a scrollback of four thousand
 * lines costs four thousand pointer moves and no copying at all.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "term_scroll.h"

void term_scroll_init(TermScroll *scroll) {
    if (scroll == NULL) {
        return;
    }
    memset(scroll, 0, sizeof(*scroll));
}

void term_scroll_free(TermScroll *scroll) {
    if (scroll == NULL) {
        return;
    }
    if (scroll->slots != NULL) {
        for (int i = 0; i < scroll->count; i++) {
            int slot = (scroll->head + i) % scroll->capacity;
            free(scroll->slots[slot].cells);
            scroll->slots[slot].cells = NULL;
        }
        free(scroll->slots);
    }
    memset(scroll, 0, sizeof(*scroll));
}

void term_scroll_clear(TermScroll *scroll) {
    if (scroll == NULL) {
        return;
    }
    /* The buffers go back and the ring stays: a program that clears the screen
       ten times in a session must not allocate the table ten times. */
    for (int i = 0; i < scroll->count; i++) {
        int slot = (scroll->head + i) % scroll->capacity;
        free(scroll->slots[slot].cells);
        scroll->slots[slot].cells = NULL;
    }
    scroll->count = 0;
    scroll->head = 0;
    scroll->offset = 0;
}

/* Drop the OLDEST line: free its buffer and step the head past it. Returns 1,
   which is what a caller counts when it has to fix a selection. */
static int drop_oldest(TermScroll *scroll) {
    if (scroll->count == 0) {
        return 0;
    }
    int slot = scroll->head;
    free(scroll->slots[slot].cells);
    scroll->slots[slot].cells = NULL;
    scroll->slots[slot].dirty = 0;

    scroll->head = (scroll->head + 1) % scroll->capacity;
    scroll->count--;

    /* The view is anchored to the oldest line, so losing one moves everything
       under it back by one — and the view has to come back with them or it
       would be showing a different line than the one asked for. */
    if (scroll->offset > 0) {
        scroll->offset--;
    }
    return 1;
}

/* A line that is not the ring's width, copied into one that is.
 *
 * This is the resize path and nothing else. The text that fits is kept and the
 * rest is blank, which is what a narrower window makes of a wide line: the
 * cells that no longer have a column are gone, and the ones that do are where
 * they were. */
static TermCell *cells_at_ring_width(int cols, int ring_cols,
                                     const TermCell *from) {
    TermCell *fresh = (TermCell *)calloc((size_t)ring_cols, sizeof(TermCell));
    if (fresh == NULL) {
        return NULL;
    }
    int keep = cols < ring_cols ? cols : ring_cols;
    if (from != NULL && keep > 0) {
        memcpy(fresh, from, (size_t)keep * sizeof(TermCell));
    }
    return fresh;
}

int term_scroll_push(TermScroll *scroll, TermCell *cells, int cols) {
    if (scroll == NULL || cells == NULL) {
        return 0;
    }

    /* The table comes up on the first push, at the width that push arrived
       with. It is the ring's width from then on — see the file comment. */
    if (scroll->slots == NULL) {
        scroll->slots = (TermLine *)calloc(TERM_SCROLL_MAX_LINES,
                                           sizeof(TermLine));
        if (scroll->slots == NULL) {
            free(cells);
            return 0;
        }
        scroll->capacity = TERM_SCROLL_MAX_LINES;
        scroll->cols = cols;
    }

    /* The grid's line is at the grid's width and the ring may be at another.
       The copy happens here rather than in the grid because the ring is what
       decided to be one width. */
    if (cols != scroll->cols) {
        TermCell *fresh = cells_at_ring_width(cols, scroll->cols, cells);
        free(cells);
        if (fresh == NULL) {
            /* Out of memory. The line is dropped rather than stored at the
               wrong width, where a reader would draw past the end of it. */
            return 0;
        }
        cells = fresh;
    }

    /* Where the top of the view is, BEFORE anything moves. It is a position in
       the whole and not an offset from the bottom, and that distinction is the
       whole of this function's second half: a view is anchored to the TEXT the
       reader is looking at, and new lines arriving below must not drag it down.
     *
     * Expressed the other way — keeping `offset` as it was — a push would move
     * the reader's line up the screen by one for every line the program
     * printed. Reading a build log while it built would be a log that crawled
     * away under the eye, which is the fault this arithmetic exists to not
     * have. */
    int anchored_top = scroll->count - scroll->offset;
    if (anchored_top < 0) {
        anchored_top = 0;
    }

    int released = 0;
    if (scroll->count == scroll->capacity) {
        released = drop_oldest(scroll);
    }

    int slot = (scroll->head + scroll->count) % scroll->capacity;
    /* A slot that still holds a buffer is one the ring wrapped onto before its
       line was released. Freeing it here is what keeps the ring from leaking
       one line per wrap. */
    free(scroll->slots[slot].cells);
    scroll->slots[slot].cells = cells;
    scroll->slots[slot].dirty = 0;
    scroll->count++;

    /* The view goes back to the text it was showing. On the live screen there
       is nothing to hold — the reader is following the program — and the
       offset stays at zero, which is also what makes output bring the screen
       back to life when a program prints after a long scroll back. */
    scroll->offset = scroll->count - anchored_top;
    if (scroll->offset < 0) {
        scroll->offset = 0;
    }
    if (scroll->offset > scroll->count) {
        scroll->offset = scroll->count;
    }
    return released;
}

int term_scroll_count(const TermScroll *scroll) {
    return scroll != NULL ? scroll->count : 0;
}

int term_scroll_cols(const TermScroll *scroll) {
    return scroll != NULL ? scroll->cols : 0;
}

const TermCell *term_scroll_cells(const TermScroll *scroll, int index) {
    if (scroll == NULL || index < 0 || index >= scroll->count) {
        return NULL;
    }
    int slot = (scroll->head + index) % scroll->capacity;
    return scroll->slots[slot].cells;
}

int term_scroll_offset(const TermScroll *scroll) {
    return scroll != NULL ? scroll->offset : 0;
}

int term_scroll_max_offset(const TermScroll *scroll) {
    /* As far back as there is something to show. Scrolling back by one more
       than the history would leave the screen empty below the oldest line,
       which is not what "the top of the scrollback" means: it means the
       oldest line at the top of a FULL screen. */
    return scroll != NULL ? scroll->count : 0;
}

int term_scroll_by(TermScroll *scroll, int lines) {
    if (scroll == NULL || lines == 0) {
        return 0;
    }
    int want = scroll->offset + lines;
    int most = term_scroll_max_offset(scroll);
    if (want < 0) want = 0;
    if (want > most) want = most;
    if (want == scroll->offset) {
        return 0;
    }
    scroll->offset = want;
    return 1;
}

void term_scroll_to_bottom(TermScroll *scroll) {
    if (scroll != NULL) {
        scroll->offset = 0;
    }
}

int term_scroll_view_top(const TermScroll *scroll) {
    if (scroll == NULL) {
        return 0;
    }
    int top = scroll->count - scroll->offset;
    return top < 0 ? 0 : top;
}
