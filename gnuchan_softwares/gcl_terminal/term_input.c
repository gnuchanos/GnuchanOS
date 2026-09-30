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

/* Send a key that has a sequence. The modifier is written into the template
   with snprintf, which is why the templates carry a `%d` — a sequence with no
   modifier has its `1` written in literally, and the same code path serves
   both. */
static void send_key_sequence(TermCore *core, const KeyEntry *entry,
                              unsigned int state, int app_cursor) {
    char out[TERM_KEY_MAX];
    int mods = modifier_number(state);

    const char *templ = entry->normal;
    if (app_cursor && entry->app != NULL) {
        templ = entry->app;
    }

    /* A template with no `%d` is a fixed string and is sent as it is; one with
       it has the modifier written in. Both cases are one snprintf and the
       result is the same kind of thing. */
    int len = snprintf(out, sizeof(out), templ, mods);
    if (len <= 0 || len >= (int)sizeof(out)) {
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
       conversion. */
    int cell_w = core->style != NULL ? core->style->cell_width : 1;
    int cell_h = core->style != NULL ? core->style->cell_height : 1;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;
    int cx = button->x / cell_w + 1;
    int cy = button->y / cell_h + 1;

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

/* --- the module ----------------------------------------------------------- */

static void input_module_event(TermCore *core, XEvent *event) {
    int app_cursor = term_input_application_cursor(core);

    switch (event->type) {
    case KeyPress: {
        XKeyEvent *key = &event->xkey;
        KeySym keysym = XLookupKeysym(key, 0);

        /* A key with a sequence is looked up first, because some of them are
           also printable — the keypad's enter is a newline and a letter to
           XLookupString — and the sequence is the more specific answer. */
        const KeyEntry *entry = key_lookup(keysym);
        if (entry != NULL && !(key->state & ControlMask)) {
            send_key_sequence(core, entry, key->state, app_cursor);
            return;
        }

        /* A modified arrow is still an arrow: shift-up has a sequence, and it
           is the same one with the modifier's number in it. The lookup above
           accepts it without control; control turns a letter into a control
           character and that path is below. */
        if (entry != NULL && (key->state & ControlMask)) {
            send_key_sequence(core, entry, key->state, app_cursor);
            return;
        }

        if (send_printable(core, key, key->state)) {
            return;
        }

        /* A key with no sequence and no character. Nothing is sent — see the
           file comment in term_input.h for why a guess would be worse. */
        return;
    }

    case ButtonPress:
        send_mouse(core, &event->xbutton, 1);
        return;

    case ButtonRelease:
        send_mouse(core, &event->xbutton, 0);
        return;

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
