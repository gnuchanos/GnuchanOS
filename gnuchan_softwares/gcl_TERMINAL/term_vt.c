/*
 * term_vt.c — the escape sequence parser.
 *
 * The byte stream a program sends is read here and turned into edits of the
 * active screen. See term_vt.h for the two screens and for why a sequence that
 * is not understood is consumed rather than printed.
 *
 * --- the shape of this file ---
 *
 * A byte either draws a character or is part of a sequence, and which one it is
 * is decided by a state machine, because the same byte means different things
 * in different places: `0` is the digit in `ESC [ 1 0 H` and an ordinary
 * character in text. The machine is the one ECMA-48 specifies, reduced to the
 * states a terminal that runs real programs needs.
 *
 * A finished sequence is handed to a table. The table is a list of finals — the
 * letter at the end of a CSI sequence — and each entry knows one instruction.
 * Adding a sequence is adding a line to the table; the byte loop itself never
 * changes.
 *
 * --- the DEC graphics set ---
 *
 * One piece of the language is neither an edit nor text: a program may ask the
 * terminal to draw its letters as the box-drawing shapes a VT100 had. `ESC ( 0`
 * selects that set, and after it a `q` means a horizontal line and not a q.
 * Programs that predate UTF-8 use it, and a terminal that does not understand
 * it draws a box made of letters — which is worse than drawing nothing, because
 * it looks like the program is broken.
 *
 * It is a translation at the point a character is written, and the mapping is
 * the one the VT100 manual gives. A program that uses UTF-8 directly — btop,
 * and everything written this century — never selects it.
 */
 
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "term_vt.h"
#include "term_vt_osc.h"

/* --- replies ---------------------------------------------------------------
 *
 * The terminal speaks back in exactly three places, and each answer is a
 * sentence a program is waiting for: get no answer and a program concludes the
 * feature is absent. The write goes through the host so this file never has to
 * know what a PTY is.
 */
static void vt_reply(TermVt *vt, const char *text) {
    if (vt->host.write != NULL && text != NULL) {
        vt->host.write(vt->host.user, text, (int)strlen(text));
    }
}

/* An OSC string has ended whole. What is done with it depends on what it is,
   and only one family is acted on: the OSC 133 semantic-prompt markers.
 *
 * OSC 133 is the convention a shell uses to tell the terminal where its prompt
 * begins and ends, so the terminal can tell the PROMPT from what the user has
 * typed. `ESC ] 133 ; B BEL` means "the prompt is over, the input starts here",
 * and it is the one piece of information a suggestion cannot be computed
 * without — see term_suggest.h for why the line is read off the grid and why
 * that needs this marker to know where the line begins.
 *
 * Everything else is still consumed and dropped, which is what it always did:
 * a window title and a palette change are real strings with no meaning here,
 * and printing one would put a control sequence's text on the screen. */
static void vt_finish_osc(TermVt *vt) {
    vt->osc[vt->osc_len] = '\0';

    if (vt->osc_len > 0) {
        const char *body = vt->osc;

        /* OSC 133 — the semantic-prompt markers. Only this family is offered
           to the marker callback: every other OSC is a string with no meaning
           there, and handing one over would make the callback's own filter the
           second place that has to know what a marker looks like. */
        if (vt->host.osc != NULL &&
            strncmp(body, "133;", 4) == 0 && body[4] != '\0') {
            vt->host.osc(vt->host.user, body, vt->osc_len);
        }

        /* OSC 0, 1 and 2 — a program naming itself. All three carry a title and
           differ only in how much of the window they mean to change; a
           terminal with one title bar treats them the same, which is what
           every terminal does.

           The text is `0;<title>`, `1;<title>`, `2;<title>` — the number, a
           semicolon, and everything after it. It is taken from after the
           semicolon and passed as it is: a title is bytes and not cells, and
           carrying it without looking inside is what lets a title hold
           anything a program puts in one. */
        if (vt->host.title != NULL && body[1] == ';' &&
            (body[0] == '0' || body[0] == '1' || body[0] == '2')) {
            vt->host.title(vt->host.user, body + 2, vt->osc_len - 2);
        }

        /* OSC 4, 10, 11, 12, 104 and 110-112 — the colours a program sets and
           asks about. The callback is what holds the palette, so the parser
           reports the request and moves on; a QUERY is answered there and not
           here because only the caller can look the colour up. */
        term_vt_handle_color_osc(vt, body);

        /* OSC 1338 — a picture placed over the cells. This terminal's own
           sequence; see term_image.h for its shape and term_vt.h for why the
           picture is not decoded here. */
        term_vt_handle_image_osc(vt, body);
    }

    vt->osc_len = 0;
}

/* --- the DEC special graphics set ------------------------------------------
 *
 * The VT100's alternate character set, as a translation of the ASCII range that
 * carries it. 0x5F to 0x7E are the shapes; below that the set is ASCII. The
 * table is indexed from 0x5F.
 */
static const uint32_t DEC_GRAPHICS[0x20] = {
    0x00A0, /* _  blank                     */
    0x25C6, /* `  black diamond             */
    0x2592, /* a  checker board             */
    0x2409, /* b  HT symbol                 */
    0x240C, /* c  FF symbol                 */
    0x240D, /* d  CR symbol                 */
    0x240A, /* e  LF symbol                 */
    0x00B0, /* f  degree                    */
    0x00B1, /* g  plus/minus                */
    0x2424, /* h  NL symbol                 */
    0x240B, /* i  VT symbol                 */
    0x2518, /* j  lower right corner        */
    0x2510, /* k  upper right corner        */
    0x250C, /* l  upper left corner         */
    0x2514, /* m  lower left corner         */
    0x253C, /* n  crossing lines            */
    0x23BA, /* o  horizontal line 1         */
    0x23BB, /* p  horizontal line 2         */
    0x2500, /* q  horizontal line 5         */
    0x23BC, /* r  horizontal line 4         */
    0x23BD, /* s  horizontal line 3         */
    0x251C, /* t  left tee                  */
    0x2524, /* u  right tee                 */
    0x2534, /* v  bottom tee                */
    0x252C, /* w  top tee                   */
    0x2502, /* x  vertical line             */
    0x2264, /* y  less than or equal        */
    0x2265, /* z  greater than or equal     */
    0x03C0, /* {  pi                        */
    0x2260, /* |  not equal                 */
    0x00A3, /* }  pound sterling            */
    0x00B7, /* ~  centered dot              */
};

/* --- sequence helpers ------------------------------------------------------ */

/* A parameter of a sequence, with the standard's default when the program left
   it out. `ESC [ H` and `ESC [ 1 ; 1 H` are the same request, and that is what
   this is for: the caller asks for meaning, not for what was on the wire. */
static int seq_param(const TermVtSequence *seq, int index, int fallback) {
    if (index < 0 || index >= seq->param_count || !seq->has_param[index]) {
        return fallback;
    }
    return seq->params[index];
}

/* --- the pen: SGR ----------------------------------------------------------
 *
 * `ESC [ ... m` sets the colours and attributes every later character is
 * written with, and it is the sequence a program sends most. btop sends one per
 * coloured run of its graphs, which on a full screen is thousands a second — so
 * this is on the hot path and is written to do nothing it does not have to.
 *
 * The parameters are read in order because the extended colours take a variable
 * number of them: `38;5;N` is a palette index and `38;2;R;G;B` is a colour, and
 * which one it is is decided by the number after the 38. That is why the loop
 * advances by what it read rather than by one.
 */
static void seq_sgr(TermGrid *grid, const TermVtSequence *seq) {
    /* A bare `ESC [ m` is a reset, and it is the same as `ESC [ 0 m`. */
    if (seq->param_count == 0) {
        grid->pen_attrs = 0;
        grid->pen_fg = TERM_COLOR_DEFAULT;
        grid->pen_bg = TERM_COLOR_DEFAULT;
        return;
    }

    int i = 0;
    while (i < seq->param_count) {
        int p = seq->has_param[i] ? seq->params[i] : 0;

        /* --- the extended colours, which consume extra parameters --- */
        if (p == 38 || p == 48) {
            int extended = (i + 1 < seq->param_count) ? seq->params[i + 1] : 0;
            if (extended == 5 && i + 2 < seq->param_count) {
                int index = seq->params[i + 2];
                if (p == 38) {
                    grid->pen_fg = index;
                } else {
                    grid->pen_bg = index;
                }
                i += 3;
                continue;
            }
            if (extended == 2 && i + 4 < seq->param_count) {
                uint32_t rgb = ((uint32_t)(seq->params[i + 2] & 0xFF) << 16) |
                               ((uint32_t)(seq->params[i + 3] & 0xFF) << 8) |
                               (uint32_t)(seq->params[i + 4] & 0xFF);
                if (p == 38) {
                    grid->pen_fg = TERM_COLOR_TRUECOLOR;
                    grid->pen_truecolor_fg = rgb;
                } else {
                    grid->pen_bg = TERM_COLOR_TRUECOLOR;
                    grid->pen_truecolor_bg = rgb;
                }
                i += 5;
                continue;
            }
            /* A truncated extended colour is dropped whole rather than
               half-applied: setting one of the three numbers would leave the
               pen in a colour no program asked for. */
            i++;
            continue;
        }

        switch (p) {
        case 0:
            grid->pen_attrs = 0;
            grid->pen_fg = TERM_COLOR_DEFAULT;
            grid->pen_bg = TERM_COLOR_DEFAULT;
            break;
        case 1:  grid->pen_attrs |= TERM_ATTR_BOLD;      break;
        case 2:  grid->pen_attrs |= TERM_ATTR_DIM;       break;
        case 3:  grid->pen_attrs |= TERM_ATTR_ITALIC;    break;
        case 4:  grid->pen_attrs |= TERM_ATTR_UNDERLINE; break;
        case 5:  grid->pen_attrs |= TERM_ATTR_BLINK;     break;
        case 7:  grid->pen_attrs |= TERM_ATTR_REVERSE;   break;
        case 8:  grid->pen_attrs |= TERM_ATTR_HIDDEN;    break;
        case 9:  grid->pen_attrs |= TERM_ATTR_STRIKE;    break;
        /* 21 is "doubly underlined" in the standard and "not bold" in what
           programs actually send, and every terminal treats it as the latter.
           Being literal here would leave a program's bold text bold. */
        case 21:
        case 22: grid->pen_attrs &= ~((uint32_t)(TERM_ATTR_BOLD | TERM_ATTR_DIM)); break;
        case 23: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_ITALIC;    break;
        case 24: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_UNDERLINE; break;
        case 25: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_BLINK;     break;
        case 27: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_REVERSE;   break;
        case 28: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_HIDDEN;    break;
        case 29: grid->pen_attrs &= ~(uint32_t)TERM_ATTR_STRIKE;    break;

        case 39: grid->pen_fg = TERM_COLOR_DEFAULT; break;
        case 49: grid->pen_bg = TERM_COLOR_DEFAULT; break;

        default:
            if (p >= 30 && p <= 37) {
                grid->pen_fg = p - 30;
            } else if (p >= 40 && p <= 47) {
                grid->pen_bg = p - 40;
            } else if (p >= 90 && p <= 97) {
                grid->pen_fg = p - 90 + 8;
            } else if (p >= 100 && p <= 107) {
                grid->pen_bg = p - 100 + 8;
            }
            /* Anything else is a parameter this terminal does not implement,
               and it is left alone rather than guessed at: a program that asked
               for something we do not do must not get something else. */
            break;
        }
        i++;
    }
}

/* --- cursor position -------------------------------------------------------
 *
 * `ESC [ row ; col H` counts from one, and a parameter left out is one. A row
 * or column of zero — which some programs send meaning "one" — is clamped by
 * the grid.
 */
static void seq_cursor_position(TermGrid *grid, const TermVtSequence *seq) {
    int row = seq_param(seq, 0, 1);
    int col = seq_param(seq, 1, 1);
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    term_grid_move_to(grid, col - 1, row - 1);
}

/* --- erasing --------------------------------------------------------------- */

static void seq_erase_display(TermGrid *grid, const TermVtSequence *seq) {
    switch (seq_param(seq, 0, 0)) {
    case 0: term_grid_erase_to_end(grid, grid->cursor_x, grid->cursor_y);   break;
    case 1: term_grid_erase_to_start(grid, grid->cursor_x, grid->cursor_y); break;
    /* 2 is the whole screen and 3 is "the screen and the scrollback", and on
     * THIS terminal both do the SAME thing: each empties the screen and each
     * takes the scrollback with it.
     *
     * That is deliberate and it is what `clear` means here. A shell's `clear`
     * sends 2, and the whole point of typing it is to start from nothing:
     * leaving the history above — where the wheel could still bring the old
     * output, and the fetch logo with it, back — is a screen that is not
     * actually clear. 3 asks for the same thing and is already satisfied by it.
     *
     * The pictures are dropped by do_ed(), which is where a full erase also
     * takes the pictures the program placed. */
    case 2:
    case 3:
        term_grid_erase_screen(grid);
        term_grid_clear_history(grid);
        break;
    default: break;
    }
}

static void seq_erase_line(TermGrid *grid, const TermVtSequence *seq) {
    switch (seq_param(seq, 0, 0)) {
    case 0: term_grid_erase_line_to_end(grid, grid->cursor_x, grid->cursor_y);   break;
    case 1: term_grid_erase_line_to_start(grid, grid->cursor_x, grid->cursor_y); break;
    case 2: term_grid_erase_line(grid, grid->cursor_y); break;
    default: break;
    }
}

/* --- the scroll region -----------------------------------------------------
 *
 * `ESC [ top ; bottom r`. The home cursor is part of the sequence and not a
 * courtesy: the standard says the cursor goes to the top left afterwards, and a
 * program that sets a region and then writes without moving would write
 * wherever the cursor happened to be — which for a program drawing a status
 * line is inside the region it just excluded.
 */
static void seq_scroll_region(TermGrid *grid, const TermVtSequence *seq) {
    int top = seq_param(seq, 0, 1);
    int bottom = seq_param(seq, 1, grid->rows);
    if (top < 1) top = 1;
    if (bottom > grid->rows) bottom = grid->rows;
    /* An inverted or empty region is refused, because a region of no lines would
       make every scroll a no-op and the program would silently stop scrolling.
       The full screen is what it gets instead. */
    if (top >= bottom) {
        top = 1;
        bottom = grid->rows;
    }
    grid->scroll_top = top - 1;
    grid->scroll_bottom = bottom - 1;
    term_grid_move_to(grid, 0, 0);
}

/* --- modes -----------------------------------------------------------------
 *
 * `ESC [ ? ... h` sets and `l` clears. These are the ones a program that draws
 * a full screen needs, and every one is a promise the terminal has to keep:
 *
 *   1      the arrow keys send SS3 rather than CSI — the program reads keys
 *          itself and expects the older form
 *   3      the window is 132 columns, which is refused rather than obeyed: the
 *          window is the user's
 *   25     the cursor is drawn or hidden; a program that draws its own must be
 *          able to turn ours off, or there are two cursors on the screen
 *   47     the alternate screen, the older spelling
 *   1000   mouse clicks are reported as escape sequences
 *   1002   and mouse drags
 *   1003   and every move, with no button held
 *   1006   and the coordinates are sent in the SGR form, which unlike the
 *          original cannot express a column past 223
 *   1047   the alternate screen, with the old screen kept
 *   1048   save and restore the cursor across the switch
 *   1049   the alternate screen as everything means it: 1047 plus 1048 plus a
 *          clear, and the one btop and every other full-screen program send
 *   2004   pasted text is bracketed, so the program can tell a paste from
 *          typing
 *   2026   the terminal may hold drawing and show it in one piece, which is
 *          btop's `terminal_sync` setting asking us to tear. THIS terminal
 *          already draws a frame in one operation — see term_render.c — so the
 *          mode is accepted and needs nothing done for it, which is the honest
 *          answer and not a lie.
 */
/* Show one of the two screens, marking ALL of it to be drawn.
 *
 * The marking is the whole point and not a detail. A frame clears the dirty
 * flag of every row it draws, so the screen being switched AWAY from is clean
 * and so is the one being switched TO — it was drawn, long ago, before the
 * program took over. Switching the pointer alone therefore changed nothing on
 * the window: the renderer found no dirty row, drew nothing, and btop's last
 * picture stayed up after btop had exited, over the shell prompt that was
 * underneath it. Marking every row is what makes the screen that comes back
 * actually be drawn.
 */
static void vt_show_screen(TermVt *vt, TermGrid *screen) {
    vt->active = screen;
    for (int y = 0; y < screen->rows; y++) {
        screen->lines[y].dirty = 1;
    }
}

static void seq_set_mode(TermVt *vt, const TermVtSequence *seq, int enable) {
    TermGrid *grid = vt->active;
    for (int i = 0; i < seq->param_count; i++) {
        if (!seq->has_param[i]) {
            continue;
        }
        switch (seq->params[i]) {
        case 1:
            vt->application_cursor = enable;
            break;
        /* DECAWM. `?7l` turns wrapping off, which a program drawing a line that
           must not run on does — a progress bar's last cell, a box whose right
           edge is the screen's. Nothing else about the grid changes: the flag
           is read in term_grid_put(). */
        case 7:
            grid->autowrap = enable;
            break;
        case 25:
            /* The cursor is the TERMINAL's, not the screen's. btop hides it
               with `?25l` and THEN switches to the alternate screen; with the
               flag on the grid the switch handed the program a screen whose own
               copy still said "visible", and a cursor blinked over a picture
               that had explicitly asked for none. */
            vt->cursor_visible = enable;
            break;
        case 47:
        case 1047:
            vt->alt_active = enable;
            if (enable) {
                /* The alternate screen starts clean, which is what makes it
                   alternate: a program that switches to it and draws must not
                   find the shell's prompt underneath its picture. */
                term_grid_reset(&vt->alt);
                vt_show_screen(vt, &vt->alt);
            } else {
                vt_show_screen(vt, &vt->grid);
            }
            break;
        case 1048:
            if (enable) {
                grid->saved_x = grid->cursor_x;
                grid->saved_y = grid->cursor_y;
            } else {
                term_grid_move_to(grid, grid->saved_x, grid->saved_y);
            }
            break;
        case 1049:
            if (enable) {
                vt->grid.saved_x = vt->grid.cursor_x;
                vt->grid.saved_y = vt->grid.cursor_y;
                vt->alt_active = 1;
                term_grid_reset(&vt->alt);
                vt_show_screen(vt, &vt->alt);
            } else {
                vt->alt_active = 0;
                term_grid_move_to(&vt->grid, vt->grid.saved_x,
                                  vt->grid.saved_y);
                vt_show_screen(vt, &vt->grid);
            }
            break;
        case 1000:
        case 1002:
        case 1003:
            vt->mouse_mode = enable ? seq->params[i] : 0;
            break;
        case 1006:
            vt->mouse_sgr = enable;
            break;
        case 2004:
            vt->bracketed_paste = enable;
            break;
        /* Everything else is a mode this terminal does not have, and saying
           nothing about it is the correct answer: a program that asks for a
           mode and is not refused assumes nothing and carries on, while a
           program told "no" goes looking for a fallback it does not have. */
        default:
            break;
        }
    }
}

/* --- insert and delete ----------------------------------------------------
 *
 * Four sequences that move what is already on the screen out of the way, or
 * close the gap it left: whole lines inserted or removed at the cursor, and
 * single characters inserted or removed.
 *
 * btop does not use them — it redraws the screen each frame — but every editor
 * does, and an editor that sends them to a terminal that ignores them draws
 * text over itself instead of pushing it aside.
 */
static void seq_insert_lines(TermGrid *grid, const TermVtSequence *seq) {
    int count = seq_param(seq, 0, 1);
    if (count < 1) count = 1;
    if (grid->cursor_y < grid->scroll_top ||
        grid->cursor_y > grid->scroll_bottom) {
        return;
    }
    int remaining = grid->scroll_bottom - grid->cursor_y + 1;
    if (count > remaining) count = remaining;

    /* Inserting at the cursor pushes lines out of the bottom of the region,
       which is a scroll — so it is done as one, with the region temporarily
       moved down to the cursor. */
    int saved_top = grid->scroll_top;
    int saved_bottom = grid->scroll_bottom;
    grid->scroll_top = grid->cursor_y;
    term_grid_scroll(grid, -count);
    grid->scroll_top = saved_top;
    grid->scroll_bottom = saved_bottom;
}

static void seq_delete_lines(TermGrid *grid, const TermVtSequence *seq) {
    int count = seq_param(seq, 0, 1);
    if (count < 1) count = 1;
    if (grid->cursor_y < grid->scroll_top ||
        grid->cursor_y > grid->scroll_bottom) {
        return;
    }
    int remaining = grid->scroll_bottom - grid->cursor_y + 1;
    if (count > remaining) count = remaining;

    int saved_top = grid->scroll_top;
    int saved_bottom = grid->scroll_bottom;
    grid->scroll_top = grid->cursor_y;
    term_grid_scroll(grid, count);
    grid->scroll_top = saved_top;
    grid->scroll_bottom = saved_bottom;
}

/* DCH — delete characters at the cursor, closing the gap from the right. */
static void seq_delete_chars(TermGrid *grid, const TermVtSequence *seq) {
    int count = seq_param(seq, 0, 1);
    if (count < 1) count = 1;
    int room = grid->cols - grid->cursor_x;
    if (count > room) count = room;
    if (count <= 0) return;

    TermLine *line = &grid->lines[grid->cursor_y];
    memmove(&line->cells[grid->cursor_x],
            &line->cells[grid->cursor_x + count],
            (size_t)(room - count) * sizeof(TermCell));
    for (int x = grid->cols - count; x < grid->cols; x++) {
        line->cells[x].ch = 0;
        line->cells[x].wide = 0;
        line->cells[x].wide_cont = 0;
        line->cells[x].fg = TERM_COLOR_DEFAULT;
        line->cells[x].bg = grid->pen_bg;
        line->cells[x].attrs = 0;
        line->cells[x].dirty = 1;
    }
    line->dirty = 1;
}

/* ICH — insert blanks at the cursor, pushing what is there to the right. */
static void seq_insert_chars(TermGrid *grid, const TermVtSequence *seq) {
    int count = seq_param(seq, 0, 1);
    if (count < 1) count = 1;
    int room = grid->cols - grid->cursor_x;
    if (count > room) count = room;
    if (count <= 0) return;

    TermLine *line = &grid->lines[grid->cursor_y];
    memmove(&line->cells[grid->cursor_x + count],
            &line->cells[grid->cursor_x],
            (size_t)(room - count) * sizeof(TermCell));
    for (int x = grid->cursor_x; x < grid->cursor_x + count; x++) {
        line->cells[x].ch = 0;
        line->cells[x].wide = 0;
        line->cells[x].wide_cont = 0;
        line->cells[x].fg = TERM_COLOR_DEFAULT;
        line->cells[x].bg = grid->pen_bg;
        line->cells[x].attrs = 0;
        line->cells[x].dirty = 1;
    }
    line->dirty = 1;
}

/* ECH — erase characters in place, without moving anything. */
static void seq_erase_chars(TermGrid *grid, const TermVtSequence *seq) {
    int count = seq_param(seq, 0, 1);
    if (count < 1) count = 1;
    for (int i = 0; i < count && grid->cursor_x + i < grid->cols; i++) {
        term_grid_clear_cell(grid, grid->cursor_x + i, grid->cursor_y);
    }
}

/* --- the DECSCUSR cursor shape ---------------------------------------------
 *
 * `ESC [ n SP q`. btop does not ask, but a shell or an editor that wants a bar
 * cursor where the terminal draws a block does, and the number it sends is what
 * the renderer draws. The values are the standard's: 1 and 2 are a block (the
 * default), 3 and 4 an underline, 5 and 6 a bar.
 */
static void seq_cursor_style(TermGrid *grid, const TermVtSequence *seq) {
    int style = seq_param(seq, 0, 0);
    if (style < 1) {
        style = 1;
    }
    /* The renderer knows two shapes; the rest are folded onto them. A shape it
       cannot draw is better drawn as one it can than as nothing at all. */
    if (style >= 5 && style <= 6) {
        grid->cursor_style = 1;   /* a bar */
    } else if (style >= 3 && style <= 4) {
        grid->cursor_style = 2;   /* an underline */
    } else {
        grid->cursor_style = 0;   /* a block */
    }
}

/* --- device attributes and status ------------------------------------------
 *
 * A program asks what the terminal is, and the answer is what makes it use
 * colour at all. The two forms are `ESC [ c` (the primary) and `ESC [ > c`
 * (the secondary); btop sends neither, but every program that checks sends one
 * of them, and a terminal that does not answer is treated as one that cannot do
 * anything.
 *
 * The reply claims the VT220 with the extensions every modern terminal has,
 * which is true of this one: colour, and the sequences a program needs to draw.
 * It deliberately does NOT claim to be an xterm by name — the number is what a
 * program reads, and pretending to be an xterm invites an xterm-only feature
 * this terminal does not have.
 */
static void seq_device_attributes(TermVt *vt, const TermVtSequence *seq) {
    if (seq->private_marker == '>') {
        vt_reply(vt, "\x1b[>41;330;0c");
    } else {
        vt_reply(vt, "\x1b[?62;1;2;3;4;6;9;15;18;21;22;29c");
    }
}

/* `ESC [ 5 n` — "are you there" — and `ESC [ 6 n` — "where is the cursor". A
   program that sends the second is asking so it can put something back
   afterwards, and a reply of the wrong shape leaves it waiting for an answer
   that never comes. */
static void seq_device_status(TermVt *vt, const TermVtSequence *seq) {
    int what = seq_param(seq, 0, 0);
    char reply[64];
    if (what == 5) {
        vt_reply(vt, "\x1b[0n");
    } else if (what == 6) {
        snprintf(reply, sizeof(reply), "\x1b[%d;%dR",
                 vt->active->cursor_y + 1, vt->active->cursor_x + 1);
        vt_reply(vt, reply);
    }
}

/* --- moving by counts ------------------------------------------------------ */

static void seq_cursor_up(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, 0, -seq_param(seq, 0, 1));
}
static void seq_cursor_down(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, 0, seq_param(seq, 0, 1));
}
static void seq_cursor_forward(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, seq_param(seq, 0, 1), 0);
}
static void seq_cursor_back(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, -seq_param(seq, 0, 1), 0);
}
/* CNL and CPL move to the first column of a line above or below, which is not
   the same as CUU or CUD plus a carriage return: a program that sends one of
   them is not expecting the cursor to keep its column. */
static void seq_cursor_next_line(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, 0, seq_param(seq, 0, 1));
    grid->cursor_x = 0;
}
static void seq_cursor_prev_line(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_by(grid, 0, -seq_param(seq, 0, 1));
    grid->cursor_x = 0;
}
/* CHA and VPA put the cursor on an absolute column or row and leave the other
   axis alone. */
static void seq_cursor_column(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_to(grid, seq_param(seq, 0, 1) - 1, grid->cursor_y);
}
static void seq_cursor_row(TermGrid *grid, const TermVtSequence *seq) {
    term_grid_move_to(grid, grid->cursor_x, seq_param(seq, 0, 1) - 1);
}

/* --- the sequences that end in a letter ------------------------------------
 *
 * One row per sequence. The table is read by the final byte, with the private
 * marker and the intermediate matched where they matter. A sequence that
 * reaches the end of the table is counted and dropped — see
 * term_vt_unknown_count().
 */
typedef void (*TermVtHandler)(TermVt *vt, const TermVtSequence *seq);

typedef struct TermVtEntry {
    char final;             /* the byte that ends the sequence                 */
    char private_marker;    /* required '?' or '>', or 0 for none              */
    char intermediate;      /* required intermediate byte, or 0 for none       */
    TermVtHandler handler;
} TermVtEntry;

/* Handlers that take the active screen rather than the parser, so the table is
   uniform while the two screens stay the parser's business. */
static void do_sgr(TermVt *vt, const TermVtSequence *seq) { seq_sgr(vt->active, seq); }
static void do_cup(TermVt *vt, const TermVtSequence *seq) { seq_cursor_position(vt->active, seq); }
/* ED — erase in display. A FULL erase (2 or 3) also drops the pictures the
   program placed, because a picture is not part of the grid and would otherwise
   stand over the cells that were just emptied — `clear` in a shell sends 2, and
   a logo left over the fresh prompt is the one thing a picture must not do.
   A partial erase (0 or 1) leaves them: it clears half a screen, and a picture
   sitting in the other half was not touched. */
static void do_ed(TermVt *vt, const TermVtSequence *seq) {
    int mode = seq_param(seq, 0, 0);
    seq_erase_display(vt->active, seq);
    if ((mode == 2 || mode == 3) && vt->host.clear != NULL) {
        vt->host.clear(vt->host.user);
    }
}
static void do_el(TermVt *vt, const TermVtSequence *seq) { seq_erase_line(vt->active, seq); }
static void do_scroll_region(TermVt *vt, const TermVtSequence *seq) { seq_scroll_region(vt->active, seq); }
static void do_cuu(TermVt *vt, const TermVtSequence *seq) { seq_cursor_up(vt->active, seq); }
static void do_cud(TermVt *vt, const TermVtSequence *seq) { seq_cursor_down(vt->active, seq); }
static void do_cuf(TermVt *vt, const TermVtSequence *seq) { seq_cursor_forward(vt->active, seq); }
static void do_cub(TermVt *vt, const TermVtSequence *seq) { seq_cursor_back(vt->active, seq); }
static void do_cnl(TermVt *vt, const TermVtSequence *seq) { seq_cursor_next_line(vt->active, seq); }
static void do_cpl(TermVt *vt, const TermVtSequence *seq) { seq_cursor_prev_line(vt->active, seq); }
static void do_cha(TermVt *vt, const TermVtSequence *seq) { seq_cursor_column(vt->active, seq); }
static void do_vpa(TermVt *vt, const TermVtSequence *seq) { seq_cursor_row(vt->active, seq); }
static void do_il(TermVt *vt, const TermVtSequence *seq) { seq_insert_lines(vt->active, seq); }
static void do_dl(TermVt *vt, const TermVtSequence *seq) { seq_delete_lines(vt->active, seq); }
static void do_dch(TermVt *vt, const TermVtSequence *seq) { seq_delete_chars(vt->active, seq); }
static void do_ich(TermVt *vt, const TermVtSequence *seq) { seq_insert_chars(vt->active, seq); }
static void do_ech(TermVt *vt, const TermVtSequence *seq) { seq_erase_chars(vt->active, seq); }
static void do_deccusr(TermVt *vt, const TermVtSequence *seq) { seq_cursor_style(vt->active, seq); }
static void do_da(TermVt *vt, const TermVtSequence *seq) { seq_device_attributes(vt, seq); }
static void do_dsr(TermVt *vt, const TermVtSequence *seq) { seq_device_status(vt, seq); }
static void do_mode_set(TermVt *vt, const TermVtSequence *seq) { seq_set_mode(vt, seq, 1); }
static void do_mode_clear(TermVt *vt, const TermVtSequence *seq) { seq_set_mode(vt, seq, 0); }

/* DECSTR is defined below with the other resets, and it is named here because
   the table is what reaches it. */
static void do_decstr(TermVt *vt, const TermVtSequence *seq);

/* SU and SD scroll the region without moving the cursor. */
static void do_su(TermVt *vt, const TermVtSequence *seq) {
    term_grid_scroll(vt->active, seq_param(seq, 0, 1));
}
static void do_sd(TermVt *vt, const TermVtSequence *seq) {
    term_grid_scroll(vt->active, -seq_param(seq, 0, 1));
}

/* A sequence that means nothing here. `ESC [ ? n A` with n from 1 to 4 is a
   cursor key in application mode arriving from the program's own input, and
   swallowing it is what keeps it out of the screen. */
static void do_ignore(TermVt *vt, const TermVtSequence *seq) {
    (void)vt;
    (void)seq;
}

static const TermVtEntry CSI_TABLE[] = {
    /* Position. */
    { 'H', 0, 0, do_cup },
    { 'f', 0, 0, do_cup },
    { 'A', 0, 0, do_cuu },
    { 'B', 0, 0, do_cud },
    { 'C', 0, 0, do_cuf },
    { 'D', 0, 0, do_cub },
    { 'E', 0, 0, do_cnl },
    { 'F', 0, 0, do_cpl },
    { 'G', 0, 0, do_cha },
    { '`', 0, 0, do_cha },
    { 'd', 0, 0, do_vpa },
    /* The cursor keys in application mode, which are swallowed. */
    { 'A', '?', 0, do_ignore },
    { 'B', '?', 0, do_ignore },
    { 'C', '?', 0, do_ignore },
    { 'D', '?', 0, do_ignore },
    /* Drawing and erasing. */
    { 'm', 0, 0, do_sgr },
    { 'J', 0, 0, do_ed },
    { 'K', 0, 0, do_el },
    { 'X', 0, 0, do_ech },
    { '@', 0, 0, do_ich },
    { 'P', 0, 0, do_dch },
    { 'L', 0, 0, do_il },
    { 'M', 0, 0, do_dl },
    { 'S', 0, 0, do_su },
    { 'T', 0, 0, do_sd },
    /* The region, the modes, and the questions.
     *
     * h and l carry the `?` private marker — `ESC [ ? 25 l` is the only form
     * anything sends — and the marker is part of what is matched. Leaving it
     * at 0 here meant the two rows never matched a real sequence: EVERY mode a
     * program asked for was dropped as unknown, so the cursor was never hidden
     * (`?25l`), the alternate screen was never entered (`?1049h`), mouse
     * reporting was never enabled and bracketed paste was never turned on.
     * The whole mode mechanism of the terminal was dead, and the visible
     * symptom was a cursor blinking over btop's picture however many times
     * btop asked it not to.
     *
     * The cursor keys in application mode above have their `?` written out for
     * the same reason; these two were the pair that was missed. */
    { 'r', 0, 0, do_scroll_region },
    { 'h', '?', 0, do_mode_set },
    { 'l', '?', 0, do_mode_clear },
    /* DECSCUSR is `ESC [ n SP q` and DECSTR is `ESC [ ! p`; the space and the
       bang are what tell them from sequences that share the final byte. */
    { 'q', 0, ' ', do_deccusr },
    { 'p', 0, '!', do_decstr },
    { 'c', 0, 0, do_da },
    { 'c', '>', 0, do_da },
    { 'n', 0, 0, do_dsr },
};

#define CSI_TABLE_LEN ((int)(sizeof(CSI_TABLE) / sizeof(CSI_TABLE[0])))

/* --- the state machine ----------------------------------------------------- */

/* A soft reset, which is `ESC [ ! p` — DECSTR. The screen is cleared and the
   cursor homed, but the modes and the pen stay: it is the reset a program
   sends BEFORE it draws, which is why it must not take the colours with it.
   The renderer's own reset would undo the palette a program just set. */
static void do_decstr(TermVt *vt, const TermVtSequence *seq) {
    (void)seq;
    term_grid_reset(vt->active);
}

/* A full reset. Everything goes: both screens, the pen, the modes, and the
   region. A program that sends it is asking for the state a terminal has when
   it is switched on, and leaving any of it behind is a difference the program
   can see. */
static void vt_full_reset(TermVt *vt) {
    /* A reset is "as if switched on", and a terminal just switched on shows no
       picture. `ESC [ ! p` and RIS both come through here. */
    if (vt->host.clear != NULL) {
        vt->host.clear(vt->host.user);
    }
    term_grid_reset(&vt->grid);
    term_grid_reset(&vt->alt);
    vt->alt_active = 0;
    vt->active = &vt->grid;
    vt->grid.pen_attrs = 0;
    vt->grid.pen_fg = TERM_COLOR_DEFAULT;
    vt->grid.pen_bg = TERM_COLOR_DEFAULT;
    vt->mouse_mode = 0;
    vt->mouse_sgr = 0;
    vt->bracketed_paste = 0;
    vt->application_cursor = 0;
    vt->dec_graphics = 0;
    /* RIS is the ONLY reset that puts the cursor back, because it is the one
       sequence that means "as if switched on" — and a terminal that has just
       been switched on has a cursor. */
    vt->cursor_visible = 1;
}

/* A control byte in the middle of ordinary text. */
static void vt_control(TermVt *vt, unsigned char byte) {
    TermGrid *grid = vt->active;
    switch (byte) {
    case 0x08:   /* BS */
        term_grid_backspace(grid);
        break;
    case 0x09:   /* HT: to the next multiple of eight */
    {
        int next = (grid->cursor_x + 8) & ~7;
        if (next >= grid->cols) {
            next = grid->cols - 1;
        }
        grid->cursor_x = next;
        break;
    }
    case 0x0A:   /* LF */
    case 0x0B:   /* VT */
    case 0x0C:   /* FF */
        term_grid_cursor_next_line(grid);
        break;
    case 0x0D:   /* CR */
        term_grid_carriage_return(grid);
        break;
    default:
        /* BEL, SO, SI, DEL and everything else: nothing is drawn. The bell is
           the session's and not the screen's. */
        break;
    }
}

/* Finish a CSI sequence: push the last parameter and run the table. */
static void vt_finish_csi(TermVt *vt, char final_byte) {
    if (vt->param_seen || vt->seq.param_count > 0) {
        if (vt->seq.param_count < TERM_VT_MAX_PARAMS) {
            vt->seq.params[vt->seq.param_count] = vt->param_value;
            vt->seq.has_param[vt->seq.param_count] =
                (unsigned char)vt->param_seen;
            vt->seq.param_count++;
        }
    }
    vt->seq.final = final_byte;

    for (int i = 0; i < CSI_TABLE_LEN; i++) {
        const TermVtEntry *entry = &CSI_TABLE[i];
        if (entry->final != vt->seq.final) {
            continue;
        }
        /* The private marker has to match exactly: a row that requires one is
           not reached by a sequence without it, and a row that requires none is
           not reached by a sequence that sent one. That is what keeps
           `ESC [ 1 A` and `ESC [ ? 1 A` apart. */
        if (entry->private_marker != vt->seq.private_marker) {
            continue;
        }
        if (entry->intermediate != vt->seq.intermediate) {
            continue;
        }
        if (entry->handler != NULL) {
            entry->handler(vt, &vt->seq);
        }
        return;
    }
    /* Nothing claimed it. It is counted and dropped — see the file comment. */
    vt->unknown_sequences++;
}

/* One ESC-introduced sequence that is not a CSI. */
static void vt_finish_escape(TermVt *vt, unsigned char byte) {
    TermGrid *grid = vt->active;
    switch (byte) {
    case '7':   /* DECSC: save the cursor */
        grid->saved_x = grid->cursor_x;
        grid->saved_y = grid->cursor_y;
        break;
    case '8':   /* DECRC: put it back */
        term_grid_move_to(grid, grid->saved_x, grid->saved_y);
        break;
    case 'D':   /* IND: index, one line down, scrolling at the bottom */
        term_grid_cursor_next_line(grid);
        break;
    case 'M':   /* RI: reverse index, one line up */
        if (grid->cursor_y == grid->scroll_top) {
            term_grid_scroll(grid, -1);
        } else if (grid->cursor_y > 0) {
            grid->cursor_y--;
        }
        break;
    case 'E':   /* NEL: next line, and to the first column */
        term_grid_cursor_next_line(grid);
        grid->cursor_x = 0;
        break;
    case 'c':   /* RIS: a full reset */
        vt_full_reset(vt);
        break;
    case 'Z':   /* DECID: the same question as `ESC [ c` */
        vt_reply(vt, "\x1b[?62;1;2;3;4;6;9;15;18;21;22;29c");
        break;
    default:
        vt->unknown_sequences++;
        break;
    }
}

/* --- the byte loop ---------------------------------------------------------
 *
 * Every byte the program sends arrives here. What it means depends on the
 * state, and the order of the checks is the order of the grammar: a string
 * being collected, then a sequence, then a character.
 */
void term_vt_feed(TermVt *vt, const char *bytes, int len) {
    for (int i = 0; i < len; i++) {
        unsigned char byte = (unsigned char)bytes[i];

        /* --- inside a string that is being collected ---
         *
         * OSC is the one string this terminal takes an interest in, and the
         * interest is only to stop it reaching the screen: a window title, or a
         * colour a program wants changed, is not a cell. It is consumed whole
         * and its content is not acted on — see the file comment. */
        if (vt->state == TERM_VT_OSC) {
            if (byte == 0x07) {              /* BEL ends it */
                vt_finish_osc(vt);
                vt->state = TERM_VT_GROUND;
                continue;
            }
            if (byte == 0x1B) {
                vt->state = TERM_VT_OSC_ESCAPE;
                continue;
            }
            if (vt->osc_len < TERM_VT_OSC_MAX - 1) {
                vt->osc[vt->osc_len++] = (char)byte;
            }
            continue;
        }
        if (vt->state == TERM_VT_OSC_ESCAPE) {
            /* ESC \ is the string terminator; anything else abandons the
               string, which is what a broken program gets. */
            if (byte == '\\') {
                vt_finish_osc(vt);
                vt->state = TERM_VT_GROUND;
            } else {
                vt->state = TERM_VT_OSC;
            }
            continue;
        }
        /* DCS and its three siblings are strings with no meaning here, and they
           are consumed whole so their bytes never reach the screen. */
        if (vt->state == TERM_VT_DCS) {
            if (byte == 0x1B) {
                vt->state = TERM_VT_OSC_ESCAPE;
            } else if (byte == 0x07) {
                vt->state = TERM_VT_GROUND;
            }
            continue;
        }
        if (vt->state == TERM_VT_CHARSET) {
            /* The byte after `ESC (` names the set. '0' is the DEC graphics set
               and everything else is ASCII, so one flag is all that is needed —
               and it is cleared by any other name, which is what a program
               switching back to ASCII does. */
            vt->dec_graphics = (byte == '0');
            vt->state = TERM_VT_GROUND;
            continue;
        }

        /* --- an escape sequence --- */
        if (vt->state == TERM_VT_ESCAPE) {
            if (byte == '[') {
                vt->state = TERM_VT_CSI_ENTRY;
                memset(&vt->seq, 0, sizeof(vt->seq));
                vt->param_value = 0;
                vt->param_seen = 0;
                continue;
            }
            if (byte == ']') {
                vt->state = TERM_VT_OSC;
                vt->osc_len = 0;
                continue;
            }
            if (byte == 'P' || byte == '^' || byte == '_' || byte == 'X') {
                vt->state = TERM_VT_DCS;
                continue;
            }
            if (byte == '(' || byte == ')' || byte == '*' || byte == '+') {
                vt->state = TERM_VT_CHARSET;
                continue;
            }
            vt_finish_escape(vt, byte);
            vt->state = TERM_VT_GROUND;
            continue;
        }

        /* --- inside a CSI sequence --- */
        if (vt->state == TERM_VT_CSI_ENTRY ||
            vt->state == TERM_VT_CSI_PARAM ||
            vt->state == TERM_VT_CSI_INTERMEDIATE) {

            /* A private marker comes before any parameter. */
            if (vt->state == TERM_VT_CSI_ENTRY &&
                (byte == '?' || byte == '>' || byte == '<' || byte == '=')) {
                vt->seq.private_marker = (char)byte;
                vt->state = TERM_VT_CSI_PARAM;
                continue;
            }
            if (byte >= '0' && byte <= '9') {
                vt->param_value = vt->param_value * 10 + (byte - '0');
                if (vt->param_value > 99999) {
                    vt->param_value = 99999;
                }
                vt->param_seen = 1;
                vt->state = TERM_VT_CSI_PARAM;
                continue;
            }
            if (byte == ';') {
                /* The number just read is pushed and a new one starts. A `;`
                   with nothing before it pushes "not given", which is what
                   `ESC [ ; 5 H` means. */
                if (vt->seq.param_count < TERM_VT_MAX_PARAMS) {
                    vt->seq.params[vt->seq.param_count] = vt->param_value;
                    vt->seq.has_param[vt->seq.param_count] =
                        (unsigned char)vt->param_seen;
                    vt->seq.param_count++;
                }
                vt->param_value = 0;
                vt->param_seen = 0;
                continue;
            }
            if (byte >= 0x20 && byte <= 0x2F) {
                /* An intermediate byte, of which only DECSCUSR's space is used
                   here. It is kept because it is what tells `ESC [ 2 SP q` from
                   a `q` that is not a cursor shape at all. */
                vt->seq.intermediate = (char)byte;
                vt->state = TERM_VT_CSI_INTERMEDIATE;
                continue;
            }
            if (byte >= 0x40 && byte <= 0x7E) {
                vt_finish_csi(vt, (char)byte);
                vt->state = TERM_VT_GROUND;
                continue;
            }
            /* Anything else inside a sequence ends it as if it were malformed
               and the byte is dropped. A sequence that never ended would
               otherwise swallow the rest of the program's output. */
            vt->state = TERM_VT_GROUND;
            continue;
        }

        /* --- an ordinary byte --- */
        if (byte == 0x1B) {
            vt->state = TERM_VT_ESCAPE;
            continue;
        }
        if (byte < 0x20 || byte == 0x7F) {
            vt_control(vt, byte);
            continue;
        }

        /* --- UTF-8 ---
         *
         * A byte above 0x7F is part of a character, and how many bytes there are
         * is in its first one. They are collected until the character is whole
         * and only then written, because a terminal that wrote each byte as it
         * arrived would put three wrong characters on the screen for every
         * accented letter.
         */
        if (vt->utf8_need != 0) {
            if ((byte & 0xC0) != 0x80) {
                /* The character was cut short. Everything collected is dropped
                   and this byte is read again as the start of something new,
                   which is what a terminal does with a program that died
                   mid-character. */
                vt->utf8_need = 0;
                vt->utf8_seen = 0;
                i--;
                continue;
            }
            vt->utf8_cp = (vt->utf8_cp << 6) | (uint32_t)(byte & 0x3F);
            vt->utf8_seen++;
            if (vt->utf8_seen >= vt->utf8_need) {
                term_grid_put(vt->active, vt->utf8_cp);
                vt->utf8_need = 0;
                vt->utf8_seen = 0;
            }
            continue;
        }

        if (byte < 0x80) {
            uint32_t cp = byte;
            /* The DEC graphics set, if the program selected it. */
            if (vt->dec_graphics && byte >= 0x5F && byte <= 0x7E) {
                cp = DEC_GRAPHICS[byte - 0x5F];
            }
            term_grid_put(vt->active, cp);
            continue;
        }

        if ((byte & 0xE0) == 0xC0) {
            vt->utf8_cp = (uint32_t)(byte & 0x1F);
            vt->utf8_need = 2;
        } else if ((byte & 0xF0) == 0xE0) {
            vt->utf8_cp = (uint32_t)(byte & 0x0F);
            vt->utf8_need = 3;
        } else if ((byte & 0xF8) == 0xF0) {
            vt->utf8_cp = (uint32_t)(byte & 0x07);
            vt->utf8_need = 4;
        } else {
            /* A continuation byte with nothing to continue: the stream is not
               valid UTF-8 at this point, and the byte is dropped rather than
               guessed at. */
            continue;
        }
        vt->utf8_seen = 1;
    }
}

/* --- life ----------------------------------------------------------------- */

int term_vt_init(TermVt *vt, int cols, int rows) {
    memset(vt, 0, sizeof(*vt));
    if (term_grid_init(&vt->grid, cols, rows) != 0) {
        return -1;
    }
    if (term_grid_init(&vt->alt, cols, rows) != 0) {
        term_grid_free(&vt->grid);
        return -1;
    }
    vt->active = &vt->grid;
    vt->state = TERM_VT_GROUND;
    /* A terminal that has just been made is one that has just been switched
       on, and that one shows its cursor. */
    vt->cursor_visible = 1;
    return 0;
}

void term_vt_free(TermVt *vt) {
    term_grid_free(&vt->grid);
    term_grid_free(&vt->alt);
}

int term_vt_resize(TermVt *vt, int cols, int rows) {
    /* Whether each screen's region was the WHOLE of it before the resize, which
       is the only case where the region has to follow the new size.
     *
     * This is read before the grids are resized, because afterwards the old
     * row count is gone. A region that was the whole screen is the ordinary
     * case — a shell scrolls the whole screen and nothing else — and a region
     * that was NOT the whole screen is a program's own choice, which must be
     * kept as it is. */
    int grid_was_full = (vt->grid.scroll_top == 0 &&
                         vt->grid.scroll_bottom == vt->grid.rows - 1);
    int alt_was_full = (vt->alt.scroll_top == 0 &&
                        vt->alt.scroll_bottom == vt->alt.rows - 1);

    if (term_grid_resize(&vt->grid, cols, rows) != 0) {
        return -1;
    }
    if (term_grid_resize(&vt->alt, cols, rows) != 0) {
        return -1;
    }

    /* A region that covered the screen covers the new screen.
     *
     * Leaving it at the old bottom was the fault behind a shell that stopped
     * writing downwards: the grid grew from 24 rows to the window's, but the
     * region stayed 0..23, so the cursor reached row 23 and every new line
     * scrolled rows 0..23 while the rows below — which the window now had — were
     * never written and never moved. Output appeared to pile up on one line
     * halfway down a window with blank space under it. */
    if (grid_was_full) {
        vt->grid.scroll_top = 0;
        vt->grid.scroll_bottom = vt->grid.rows - 1;
    } else if (vt->grid.scroll_bottom >= vt->grid.rows) {
        vt->grid.scroll_bottom = vt->grid.rows - 1;
    }
    if (alt_was_full) {
        vt->alt.scroll_top = 0;
        vt->alt.scroll_bottom = vt->alt.rows - 1;
    } else if (vt->alt.scroll_bottom >= vt->alt.rows) {
        vt->alt.scroll_bottom = vt->alt.rows - 1;
    }
    return 0;
}

const TermGrid *term_vt_screen(const TermVt *vt) {
    return vt->active;
}

int term_vt_cursor_visible(const TermVt *vt) {
    return vt->cursor_visible;
}

int term_vt_wants_mouse(const TermVt *vt) {
    return vt->mouse_mode != 0;
}

int term_vt_mouse_is_sgr(const TermVt *vt) {
    return vt->mouse_sgr != 0;
}

int term_vt_wants_bracketed_paste(const TermVt *vt) {
    return vt->bracketed_paste != 0;
}

int term_vt_wants_application_cursor(const TermVt *vt) {
    return vt->application_cursor != 0;
}

long term_vt_unknown_count(const TermVt *vt) {
    return vt->unknown_sequences;
}
