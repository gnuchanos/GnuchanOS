/*
 * term_input.c — the keyboard and the mouse, turned into the bytes a program
 * reads.
 *
 * See term_input.h for what a key becomes and why the tables are copied rather
 * than derived. What is here is the tables, the modifier arithmetic, and the
 * write to the PTY.
 *
 * --- why the modifier number is where it is ---
 *
 * X hands us a keysym and a state mask. A program on the other end wants a
 * sequence, and the sequence for a modified key is the unmodified one with the
 * LAST parameter replaced: `ESC [ A` becomes `ESC [ 1 ; 5 A` for ctrl, and the
 * `5` is a bit field where shift is 1, alt is 2 and ctrl is 4, plus one.
 *
 * That arithmetic is the whole of the modifier handling, and it is why every
 * sequence below is written with a `1` where the modifier goes: a sequence
 * without a modifier and a sequence with one differ by exactly that number.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "term_input.h"
#include "term_pty.h"
/* For the cell size, which is how a mouse position becomes a column. The style
   is the core's and this module only reads two numbers out of it. */
#include "term_style.h"
/* For the selection and the clipboard, which are the terminal's own and not
   the program's: a drag with the left button and Ctrl+Shift+C. */
#include "term_select.h"

/* The most bytes one key press can produce. A function key with every modifier
   is the longest at fourteen; this is that with room to spare. */
#define TERM_KEY_MAX 32

/* --- the bit field a modifier becomes -------------------------------------
 *
 * The order is the standard's: shift is 1, alt is 2, ctrl is 4. The `+ 1` is
 * part of the encoding — a parameter of 1 means "no modifier" and cannot be
 * sent, so every value is one higher than the bits.
 *
 * Alt is taken from Mod1 and not from a keysym. On every window manager worth
 * the name, Alt IS Mod1 — it is the modifier the user's config binds — and
 * asking for the Alt_L keysym instead would miss a session that remapped it.
 */
static int modifier_number(unsigned int state) {
    int mods = 0;
    if (state & ShiftMask)   mods |= 1;
    if (state & Mod1Mask)    mods |= 2;
    if (state & ControlMask) mods |= 4;
    return mods + 1;
}

/* --- how a code point becomes bytes --------------------------------------- */

static int encode_utf8(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

void term_input_send(TermCore *core, const char *bytes, int len) {
    if (core->pty == NULL) {
        return;
    }
    term_pty_write((TermPty *)core->pty, bytes, len);
}

void term_input_send_codepoint(TermCore *core, uint32_t codepoint) {
    char bytes[4];
    int len = encode_utf8(codepoint, bytes);
    term_input_send(core, bytes, len);
}

int term_input_application_cursor(const TermCore *core) {
    return term_vt_wants_application_cursor(&core->vt);
}

/* --- the keys that are not letters ----------------------------------------
 *
 * One row per key: the keysym X reports, and the sequence to send when it is
 * pressed with no modifier. A key with a modifier is the same row with the
 * modifier's number written into it, which is why the sequences are templates
 * and not finished strings.
 *
 * `app` marks the keys whose sequence has a second form when the program asked
 * for application cursor keys. Only the four arrows and home and end have one;
 * the rest are the same either way, which is a fact about the standard and not
 * a choice made here.
 */
typedef struct KeyEntry {
    KeySym keysym;
    const char *normal;    /* the escape sequence, `%d` where the modifier goes */
    const char *app;       /* the form for application cursor mode, or NULL     */
} KeyEntry;

static const KeyEntry KEYS[] = {
    /* The arrows. `ESC [ A` and `ESC O A` are the two forms. */
    { XK_Up,        "\x1b[%dA", "\x1bO%dA" },
    { XK_Down,      "\x1b[%dB", "\x1bO%dB" },
    { XK_Right,     "\x1b[%dC", "\x1bO%dC" },
    { XK_Left,      "\x1b[%dD", "\x1bO%dD" },
    { XK_Home,      "\x1b[%dH", "\x1bO%dH" },
    { XK_End,       "\x1b[%dF", "\x1bO%dF" },

    /* The editing keys. The `~` group is the one xterm uses for everything
       that had no letter left. */
    { XK_Insert,    "\x1b[2~", NULL },
    { XK_Delete,    "\x1b[3~", NULL },
    { XK_Page_Up,   "\x1b[5~", NULL },
    { XK_Page_Down, "\x1b[6~", NULL },
    { XK_BackSpace, "\x7f",    NULL },

    /* The function keys. F1 to F4 are the old SS3 group; the rest are the `~`
       numbers, and there is no pattern to them. */
    { XK_F1,  "\x1bOP",   NULL },
    { XK_F2,  "\x1bOQ",   NULL },
    { XK_F3,  "\x1bOR",   NULL },
    { XK_F4,  "\x1bOS",   NULL },
    { XK_F5,  "\x1b[15~", NULL },
    { XK_F6,  "\x1b[17~", NULL },
    { XK_F7,  "\x1b[18~", NULL },
    { XK_F8,  "\x1b[19~", NULL },
    { XK_F9,  "\x1b[20~", NULL },
    { XK_F10, "\x1b[21~", NULL },
    { XK_F11, "\x1b[23~", NULL },
    { XK_F12, "\x1b[24~", NULL },
};

#define KEYS_LEN ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

/* Which entry a keysym is, or NULL. */
static const KeyEntry *key_lookup(KeySym keysym) {
    for (int i = 0; i < KEYS_LEN; i++) {
        if (KEYS[i].keysym == keysym) {
            return &KEYS[i];
        }
    }
    return NULL;
}

/* Write a modifier into a sequence template.
 *
 * The templates are `"\x1b[%dA"` and the like: a prefix, the number, a suffix.
 * They are all static strings written a few lines above, so the format is
 * known good — but snprintf cannot check a format that is not a literal, and
 * a compiler told to look (-Wformat=2) says so, because the day one of those
 * templates gains a `%s` the call would read an `int` as a pointer.
 *
 * So the template is SPLIT at its `%d` and the three pieces are formatted
 * against a literal instead. That is not a workaround: it is the same output
 * by a call the compiler can check, and a template with no `%d` at all — the
 * fixed sequences, `\x7f` and the function keys — takes the branch that copies
 * it whole.
 *
 * Returns the length, or -1 when it would not fit. */
static int format_sequence(char *out, unsigned int size, const char *templ,
                           int mods) {
    const char *mark = strstr(templ, "%d");
    if (mark == NULL) {
        int len = snprintf(out, size, "%s", templ);
        return (len >= 0 && (unsigned)len < size) ? len : -1;
    }
    /* `%.*s` takes the prefix by length, so nothing has to be copied out of
       the template to cut it short. */
    int head = (int)(mark - templ);
    int len = snprintf(out, size, "%.*s%d%s", head, templ, mods, mark + 2);
    return (len >= 0 && (unsigned)len < size) ? len : -1;
}

/* Send a key that has a sequence. The modifier is written into the template,
   which is why the templates carry a `%d`; a sequence with no modifier slot is
   sent as it stands, and one code path serves both. */
static void send_key_sequence(TermCore *core, const KeyEntry *entry,
                              unsigned int state, int app_cursor) {
    char out[TERM_KEY_MAX];
    int mods = modifier_number(state);

    const char *templ = entry->normal;
    if (app_cursor && entry->app != NULL) {
        templ = entry->app;
    }

    int len = format_sequence(out, sizeof(out), templ, mods);
    if (len <= 0) {
        return;
    }
    term_input_send(core, out, len);
}

/* --- the keys that are letters --------------------------------------------
 *
 * A printable key sends its character as UTF-8. The code point comes from
 * XLookupString, which is what knows the layout — a session with a Turkish
 * keyboard gets `ş` where an American one gets `s`, and a terminal that
 * hardcoded a layout would be wrong for both.
 *
 * The buffer is eight bytes because XLookupString may return a UTF-8 sequence
 * of up to four bytes for one key, and it needs a terminator.
 */
static int send_printable(TermCore *core, XKeyEvent *key, unsigned int state) {
    char buffer[8];
    KeySym keysym = NoSymbol;
    int len = XLookupString(key, buffer, (int)sizeof(buffer) - 1, &keysym, NULL);
    if (len <= 0) {
        return 0;
    }

    /* Ctrl with a letter is the character with its top bits cleared, which is
       how every terminal has sent it since the beginning: ctrl-a is 0x01,
       ctrl-z is 0x1a. The buffer holds the letter itself in that case. */
    if ((state & ControlMask) && len == 1 && buffer[0] >= 'a' && buffer[0] <= 'z') {
        char ctrl = (char)(buffer[0] - 'a' + 1);
        term_input_send(core, &ctrl, 1);
        return 1;
    }
    if ((state & ControlMask) && len == 1 && buffer[0] >= 'A' && buffer[0] <= 'Z') {
        char ctrl = (char)(buffer[0] - 'A' + 1);
        term_input_send(core, &ctrl, 1);
        return 1;
    }

    term_input_send(core, buffer, len);
    return 1;
}

/* --- the mouse -------------------------------------------------------------
 *
 * A mouse event is an escape sequence too, and its shape depends on which
 * protocol the program asked for. btop asks for the SGR form (`?1006`), which
 * is the one that can express a column past 223 — the original form packed the
 * position into a single byte and stopped working on any window wider than
 * that, which is every window now.
 *
 * The forms, and what each number means:
 *
 *   ESC [ M Cb Cx Cy      the original, with the three bytes offset by 32
 *   ESC [ < Cb ; Cx ; Cy M the SGR form, numbers in decimal, `M` for a press
 *                         and `m` for a release
 *
 * The button number is a bit field: 0, 1 and 2 are the three buttons, 32 is
 * added for a drag, 64 for a wheel. That is why the arithmetic below adds
 * rather than switches.
 */
static int mouse_button_bits(unsigned int button, unsigned int state,
                             int *is_wheel) {
    *is_wheel = 0;
    switch (button) {
    /* The three buttons, with the modifier bits set on top. Shift is 4 in the
       mouse protocol's own bit field — it is a different field from the SGR
       modifier number, and the two are not interchangeable. */
    case Button1: return (state & ShiftMask) ? 4 : 0;
    case Button2: return 1;
    case Button3: return 2;
    case Button4: *is_wheel = 1; return 64;    /* wheel up   */
    case Button5: *is_wheel = 1; return 65;    /* wheel down */
    default: return -1;
    }
}

static void send_mouse(TermCore *core, XButtonEvent *button, int pressed) {
    if (!term_vt_wants_mouse(&core->vt)) {
        return;
    }

    int is_wheel = 0;
    int bits = mouse_button_bits(button->button, button->state, &is_wheel);
    if (bits < 0) {
        return;
    }

    /* A wheel event has no release, and sending one would tell the program a
       button came up that never went down. */
    if (!pressed && is_wheel) {
        return;
    }
    if (pressed && !is_wheel) {
        bits += 32;   /* a drag: the button is down while the pointer moves */
    }

    /* The position is in CELLS, not pixels: the program's own idea of where the
       mouse is is its own grid, and a program that got pixels would put its
       selection in the wrong place. The division is the whole of the
       conversion.
     *
     * The frame comes off first. The grid does not start at the window's corner
     * any more — there is a margin to its left and the bar above it — and a
     * position measured from the window's corner would name a cell one row and
     * one column away from the one under the pointer. That is a click on the
     * wrong thing, which for a mouse-driven program is the worst kind of
     * wrong.
     *
     * A position ABOVE the grid — in the bar or the margin — is clamped to the
     * first cell rather than refused: a program that gets a click at row 0 has
     * an answer it can act on, and one that gets no event at all has a click
     * that went nowhere. */
    int origin_x = 0;
    int origin_y = 0;
    term_core_grid_origin(core, &origin_x, &origin_y);

    int cell_w = core->style != NULL ? core->style->cell_width : 1;
    int cell_h = core->style != NULL ? core->style->cell_height : 1;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    int local_x = button->x - origin_x;
    int local_y = button->y - origin_y;
    if (local_x < 0) local_x = 0;
    if (local_y < 0) local_y = 0;

    int cx = local_x / cell_w + 1;
    int cy = local_y / cell_h + 1;

    char out[TERM_KEY_MAX];
    int len;
    if (term_vt_mouse_is_sgr(&core->vt)) {
        len = snprintf(out, sizeof(out), "\x1b[<%d;%d;%d%c",
                       bits, cx, cy, pressed ? 'M' : 'm');
    } else {
        /* The original form: three bytes offset by 32, and the release is the
           button number with 3 in the low bits. */
        int cb = bits + 32;
        int release_cb = 3 + 32;
        len = snprintf(out, sizeof(out), "\x1b[M%c%c%c",
                       pressed ? cb : release_cb,
                       cx + 32, cy + 32);
    }
    if (len > 0 && len < (int)sizeof(out)) {
        term_input_send(core, out, len);
    }
}

/* --- the terminal's own gestures ------------------------------------------
 *
 * Three things are the TERMINAL's and not the program's, and each is taken off
 * the event stream before the program can see it:
 *
 *   the scrollbar   a click on it scrolls the view, and sending it to the
 *                   program as well would move a mouse in a program that never
 *                   saw the click
 *   the wheel       scrolls the view when the program has not asked for mouse
 *                   reporting — which is every shell, and the reason scrolling
 *                   back through `ls -l` works at all
 *   Ctrl+Shift+C    copies the selection
 *
 * Shift is what tells the two kinds of arrow apart. A plain Page Up belongs to
 * whatever program is running — that is what an editor expects — and
 * Shift+Page Up is the terminal's own, which is the convention every terminal
 * has used since xterm. Without the shift an editor would lose its own Page Up
 * to a feature it never asked for.
 */

/* Whether a click at this position is on the scrollbar. The geometry comes
   from the core, which is also what draws it — see term_core.h. */
static int click_is_on_scrollbar(TermCore *core, int x, int y) {
    int bar_x = 0;
    int bar_width = 0;
    int track_y = 0;
    int track_h = 0;
    int thumb_y = 0;
    int thumb_h = 0;
    term_core_scrollbar_rect(core, &bar_x, &bar_width, &track_y, &track_h,
                             &thumb_y, &thumb_h);
    if (bar_width <= 0 || track_h <= 0) {
        return 0;
    }
    return x >= bar_x && x < bar_x + bar_width &&
           y >= track_y && y < track_y + track_h;
}

/* A key the terminal keeps for itself. Returns 1 when the key was handled. */
static int handle_terminal_key(TermCore *core, XKeyEvent *key, KeySym keysym) {
    int shift = (key->state & ShiftMask) != 0;
    int ctrl = (key->state & ControlMask) != 0;

    /* Ctrl+Shift+C is a copy. Shift is required and is not a formality: plain
       Ctrl+C is an interrupt and belongs to whatever is running, so taking it
       would make it impossible to stop a program from the terminal. */
    if (ctrl && shift && (keysym == XK_C || keysym == XK_c)) {
        term_select_copy(core);
        return 1;
    }

    /* Anything typed at the prompt is the user being done reading: the view
       goes back to the live screen. It is only done when the view is actually
       back, so typing costs nothing on the ordinary screen. */
    if (term_core_scrolled_back(core) && !ctrl && !shift) {
        /* Only the keys that are characters or editing: a function key is a
           program's own and must not also bring the screen down. */
        if (keysym == XK_Return || keysym == XK_KP_Enter ||
            keysym == XK_BackSpace || keysym == XK_Escape ||
            (keysym >= 0x20 && keysym <= 0x7E)) {
            term_core_scroll_to_bottom(core);
        }
    }

    if (!shift) {
        return 0;
    }

    /* Shift with a scrolling key is the terminal's. The amount is a screenful
       less one line, which is what keeps a line of context between the two
       views — scrolled by exactly a screenful, the reader would have to find
       their place again every page. */
    int page = core->vt.grid.rows > 1 ? core->vt.grid.rows - 1 : 1;
    switch (keysym) {
    case XK_Page_Up:
        term_core_scroll_by(core, page);
        return 1;
    case XK_Page_Down:
        term_core_scroll_by(core, -page);
        return 1;
    case XK_Home:
        /* The top of the scrollback: as far back as there is to go. */
        term_core_scroll_by(core, term_scroll_max_offset(&core->scroll));
        return 1;
    case XK_End:
        term_core_scroll_to_bottom(core);
        return 1;
    case XK_Up:
        term_core_scroll_by(core, 1);
        return 1;
    case XK_Down:
        term_core_scroll_by(core, -1);
        return 1;
    default:
        return 0;
    }
}

/* The wheel and the buttons.
 *
 * The wheel scrolls the view when the program does not want the mouse. A
 * program that asked for mouse reporting gets the wheel instead, because it
 * asked for it to draw or to page its own content — btop scrolls its process
 * list with it — and a terminal that kept the wheel would leave that program
 * unable to respond to it.
 *
 * Shift is the escape hatch in both directions: Shift+wheel always scrolls the
 * view, so a program that grabbed the mouse can still be scrolled past.
 */
static int handle_button(TermCore *core, XButtonEvent *button, int pressed) {
    int shift = (button->state & ShiftMask) != 0;

    if (button->button == Button4 || button->button == Button5) {
        if (!pressed) {
            return 1;   /* a wheel event has no release */
        }
        /* Three lines a notch, which is what every other scroll surface on the
           desktop does and what the hand expects. */
        if (shift || !term_vt_wants_mouse(&core->vt)) {
            int lines = (button->button == Button4) ? 3 : -3;
            term_core_scroll_by(core, lines);
            return 1;
        }
        return 0;
    }

    if (button->button == Button1) {
        if (pressed && click_is_on_scrollbar(core, button->x, button->y)) {
            /* A click anywhere on the track puts the view there, which is what
               a person means by clicking below the thumb: not "down one line"
               but "show me that part". */
            term_core_set_view_offset(
                core, term_core_scrollbar_offset_at(core, button->y));
            return 1;
        }
        /* A drag on the grid selects text, and only when the program has not
           asked for the mouse: a program that wants clicks — btop, a file
           manager in a terminal — must get them. */
        if (!term_vt_wants_mouse(&core->vt)) {
            if (pressed) {
                term_select_begin(core, button->x, button->y);
            } else {
                term_select_end(core);
            }
            return 1;
        }
    }

    return 0;
}

/* --- the module ----------------------------------------------------------- */

static void input_module_event(TermCore *core, XEvent *event) {
    int app_cursor = term_input_application_cursor(core);

    switch (event->type) {
    case KeyPress: {
        XKeyEvent *key = &event->xkey;
        KeySym keysym = XLookupKeysym(key, 0);

        /* The terminal's own keys come first, before the program's: they are
           the ones with a modifier that means "the terminal, not you", and
           letting a program's sequence win would make them unreachable. */
        if (handle_terminal_key(core, key, keysym)) {
            term_core_claim_event(core);
            return;
        }

        /* CLEARING THE SELECTION, and it is here rather than in the select
           module because it is a keystroke's meaning: typing while text is
           highlighted is the user done with it. A selection that stayed would
           keep a highlight through a whole session's typing. */
        term_select_clear(core);

        /* A key with a sequence is looked up first, because some of them are
           also printable — the keypad's enter is a newline and a letter to
           XLookupString — and the sequence is the more specific answer. */
        const KeyEntry *entry = key_lookup(keysym);
        if (entry != NULL && !(key->state & ControlMask)) {
            send_key_sequence(core, entry, key->state, app_cursor);
            term_core_claim_event(core);
            return;
        }

        /* A modified arrow is still an arrow: shift-up has a sequence, and it
           is the same one with the modifier's number in it. The lookup above
           accepts it without control; control turns a letter into a control
           character and that path is below. */
        if (entry != NULL && (key->state & ControlMask)) {
            send_key_sequence(core, entry, key->state, app_cursor);
            term_core_claim_event(core);
            return;
        }

        if (send_printable(core, key, key->state)) {
            term_core_claim_event(core);
            return;
        }

        /* A key with no sequence and no character. Nothing is sent — see the
           file comment in term_input.h for why a guess would be worse. */
        term_core_claim_event(core);
        return;
    }

    case ButtonPress:
        if (handle_button(core, &event->xbutton, 1)) {
            term_core_claim_event(core);
            return;
        }
        send_mouse(core, &event->xbutton, 1);
        term_core_claim_event(core);
        return;

    case ButtonRelease:
        if (handle_button(core, &event->xbutton, 0)) {
            term_core_claim_event(core);
            return;
        }
        send_mouse(core, &event->xbutton, 0);
        term_core_claim_event(core);
        return;

    /* A drag extends the selection. It is claimed only while one is in
       progress, so a program that wants motion events — `?1003`, which reports
       every move — still gets them when the user is not selecting. */
    case MotionNotify: {
        TermSelect *select = (TermSelect *)core->select;
        if (select != NULL && select->dragging) {
            term_select_extend(core, event->xmotion.x, event->xmotion.y);
            term_core_claim_event(core);
        }
        return;
    }

    default:
        return;
    }
}

const TermModule term_input_module = {
    .name = "input",
    .init = NULL,
    .event = input_module_event,
    .tick = NULL,
    /* No interval: a terminal that is not being typed into has nothing to
       send, and a wake to send nothing would be a wake for its own sake. */
    .interval_ms = 0,
    .cleanup = NULL,
};
