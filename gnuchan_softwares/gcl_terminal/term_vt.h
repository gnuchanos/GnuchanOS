/*
 * term_vt.h — the escape sequence parser, and the second screen.
 *
 * A program does not draw on the terminal; it sends a stream of bytes in which
 * the drawing is written in a language of its own. A letter is a byte and means
 * itself; everything else is introduced by ESC — 0x1B — and is an instruction
 * to move the cursor, change the colour, clear a line, or switch to a whole
 * second screen. This file reads that language.
 *
 * --- the two screens ---
 *
 * There are two cell grids and a program switches between them with an escape
 * sequence. The first screen is the one lines scroll up through. The second is
 * a clean screen with no history, and a program that wants to draw and then
 * leave the terminal exactly as it found it — every full-screen program ever
 * written, btop among them — draws there and switches back on exit. That is
 * why the shell prompt is still where it was after quitting btop: nothing was
 * ever written over it.
 *
 * Both grids are kept and swapped between, because the first screen's contents
 * are part of the promise. A program is allowed to be killed without restoring
 * it, and the terminal then puts the first screen back anyway.
 *
 * --- what is parsed and what is not ---
 *
 * The sequences are the ones a program that runs on a terminal actually sends
 * — the subset every terminal implements, and nothing else. A sequence that is
 * not understood is consumed and ignored: it has a known length by its own
 * grammar, so ignoring one is safe, and a terminal that instead printed
 * unknown sequences to the screen would draw garbage every time a program
 * asked about a feature it does not have.
 *
 * The query sequences are answered, and that is the one place the terminal
 * speaks back. A program that asks "what is this terminal" and gets no answer
 * assumes the worst and disables colour; answering is what makes btop and
 * every other modern program cooperate, and it costs one write.
 */
#ifndef GNUCHANTERM_VT_H
#define GNUCHANTERM_VT_H

#include <stdint.h>

#include "term_grid.h"

/* The most parameters one sequence may carry. Sequences that take more than a
   handful are not written by real programs, and a sequence that sends more has
   its extra parameters ignored rather than its way into a bigger array. */
#define TERM_VT_MAX_PARAMS 32

/* The longest OSC string that is collected. It is a single line a program
   sends — a window title, or a colour it wants changed — and the ceiling is
   there so a broken program cannot make the parser allocate. */
#define TERM_VT_OSC_MAX 1024

/* What the parser is in the middle of. The names are the ones the standard
   gives them — ECMA-48 — and the spelling is kept so a reader who knows the
   sequences recognises the states. */
typedef enum TermVtState {
    TERM_VT_GROUND = 0,       /* ordinary text                              */
    TERM_VT_ESCAPE,           /* ESC seen, waiting for what it introduces   */
    TERM_VT_CSI_ENTRY,        /* ESC [ — parameters and intermediates follow*/
    TERM_VT_CSI_PARAM,        /* reading digits and ;                       */
    TERM_VT_CSI_INTERMEDIATE, /* reading the bytes before the final         */
    TERM_VT_OSC,              /* ESC ] — a string, ended by BEL or ST       */
    TERM_VT_OSC_ESCAPE,       /* ESC seen inside a string, waiting for `\`  */
    TERM_VT_DCS,              /* ESC P — a string that will be ignored      */
    TERM_VT_CHARSET,          /* ESC ( — which charset a G slot becomes     */
} TermVtState;

/* One parsed sequence, handed to the sequence table. The parser fills this and
   the table looks at it; neither owns the other's state. */
typedef struct TermVtSequence {
    int   params[TERM_VT_MAX_PARAMS];
    int   param_count;

    /* A parameter that was left out between two semicolons is 0 in the array
       and means 1 in most of the standards — a program that sends
       `ESC [ ; 5 H` means row 1. `has_param[i]` is what tells the two apart,
       which matters for the sequences where 0 and "not given" differ. */
    unsigned char has_param[TERM_VT_MAX_PARAMS];

    char  intermediate;    /* the byte before the final, ' ', '!' or '"'     */
    char  final;           /* the letter that ends the sequence              */
    char  private_marker;  /* '?', '>' or '<', or 0                          */
} TermVtSequence;

/* Answers the terminal writes back, and the strings a program sends that the
   rest of the terminal cares about.
 *
 * `write` is how the parser answers a query, and it is a function so it does
 * not have to know what a PTY is — see term_pty.h.
 *
 * `osc` is handed the body of every OSC string the parser has finished
 * collecting. Two families are acted on and they are enough to be worth the
 * callback: the OSC 133 semantic-prompt markers, which say where a shell's
 * prompt ended and its input begins, and the OSC 0, 1 and 2 TITLE strings,
 * which are how a program names itself in the window manager's title bar.
 *
 * `title` is separate from `osc` because what a title needs is not what a
 * marker needs: the marker is a position the terminal records, and the title
 * is text another program draws. Keeping them as two callbacks is what lets
 * each do its one thing without the other knowing it exists.
 *
 * `color` is the third family and it is a callback of its own for the same
 * reason: a colour a program sets is the STYLE's and not the parser's, and the
 * parser must not grow a palette to be able to talk about one. Everything else
 * — an OSC this terminal has no use for — is still consumed and dropped. */
typedef struct TermVtHost {
    void (*write)(void *user, const char *bytes, int len);
    void (*osc)(void *user, const char *body, int len);
    void (*title)(void *user, const char *text, int len);
    void (*color)(void *user, int slot, uint32_t rgb, int action);
    /* A PICTURE, placed by the `ESC ] 1338 ; ...` sequence this terminal
       defines — see term_image.h. The parser hands over the cell, how many
       rows tall, and the path; it does not decode the file, because decoding a
       picture is not something a parser of bytes can do. */
    void (*image)(void *user, int x, int y, int cols, int rows,
                  const char *path);
    /* The screen has been CLEARED — `ESC [ 2 J`, `ESC [ 3 J`, or a full reset.
       A picture placed over the cells is not part of the grid and would
       otherwise outlive the cells it covered: `clear` in a shell sends ED, and
       a logo left standing over the fresh prompt is the one thing a picture in
       a terminal must not do. The caller drops the pictures it holds. */
    void (*clear)(void *user);
    void *user;
} TermVtHost;

/* --- the colours a program sets: OSC 4, 10, 11 and 12 ---------------------
 *
 * A program may repaint the terminal's palette and its three personal colours:
 * `ESC ] 4 ; index ; colour` sets a palette entry, 10 the default foreground,
 * 11 the background, and 12 the cursor. Programs that ship a colourscheme of
 * their own use it — a mail client that wants its own blue, a fetch tool that
 * wants a logo's exact colours — and a terminal that ignores it draws them in
 * the theme instead, which for such a program is every colour wrong.
 *
 * `slot` is a palette index 0 to 255, or one of the three below. They are
 * NEGATIVE for a reason: every entry a program can name by number is 0 to 255,
 * so one comparison against 0 tells a palette entry from a personal colour,
 * and a positive number is never mistaken for a special one. The three sit
 * past the 256 a program names in the PALETTE — see TERM_COLOR_INDEX_FG — and
 * the two numberings are deliberately not shared: this one is "which thing did
 * the program mean" and that one is "where does the theme's colour live".
 *
 * `action` is what the program asked for, and it is not decoration: the same
 * sequence shape means three different things, and a reader that assumed SET
 * would answer a question by overwriting the colour it was asked about.
 *
 *   SET    draw this colour from now on
 *   QUERY  "what is this colour" — the program wrote `?` where a colour goes
 *   RESET  "put it back" — OSC 104, 110, 111 and 112, which take no colour
 *
 * A QUERY is answered by the CALLER and not by the parser, because only the
 * caller has the palette to look the answer up in. The parser reports the
 * question and forgets it. */
#define TERM_VT_COLOR_FOREGROUND (-1)
#define TERM_VT_COLOR_BACKGROUND (-2)
#define TERM_VT_COLOR_CURSOR     (-3)

/* "Every entry a program can name", for OSC 104 with no index: the whole
   palette goes back to what the theme built. It is a slot of its own and not
   a loop in the parser, because restoring a palette means the STYLE's baseline
   and the parser does not have one. */
#define TERM_VT_COLOR_ALL_PALETTE (-4)

enum {
    TERM_VT_COLOR_SET = 0,
    TERM_VT_COLOR_QUERY,
    TERM_VT_COLOR_RESET,
};

typedef struct TermVt {
    TermGrid  grid;         /* the first screen: what scrolls              */
    TermGrid  alt;          /* the second: a full-screen program's screen  */
    int       alt_active;

    /* The grid a write goes to: &grid or &alt, kept so no writer has to test
       alt_active. It is set by the sequence that switches screens. */
    TermGrid *active;

    TermVtState state;
    TermVtSequence seq;

    /* The next digit being read. Parameters are built up here and pushed onto
       the sequence when a `;` or the final byte ends the number. */
    int  param_value;
    int  param_seen;

    /* An OSC string, and how much of it has been collected. */
    char osc[TERM_VT_OSC_MAX];
    int  osc_len;

    /* UTF-8. A code point above 0x7F arrives as several bytes, and the parser
       cannot decide what a byte is until it has all of them: the same byte
       starts a sequence in one position and continues a character in another.
       The count of bytes still wanted lives here, and the code point is built
       up as they come. */
    uint32_t utf8_cp;
    int      utf8_need;
    int      utf8_seen;

    TermVtHost host;

    /* How many sequences arrived that this parser does not implement. It is a
       counter and not a flag so the question "did we drop something btop sent"
       has an answer that is not a guess. */
    long unknown_sequences;

    /* Whether the cursor is drawn, on the TERMINAL and not on a screen.
     *
     * There is one cursor, and a program that hides it hides it — switching to
     * the alternate screen must not bring it back. Keeping the flag on the grid
     * meant the two screens each had their own copy, so a program that hid the
     * cursor and THEN switched screens (which is the order btop sends: `?25l`
     * before `?1049h`) left the alternate screen's copy still saying "visible",
     * and a cursor blinked over a picture that had explicitly turned it off.
     */
    int cursor_visible;

    /* What the program asked for. The input module reads these: a program that
       did not ask for mouse events must not be sent them, or it receives text
       it never wanted when the user drags over the window. */
    int mouse_mode;
    int mouse_sgr;
    int bracketed_paste;
    int application_cursor;   /* DECCKM: arrow keys send SS3, not CSI       */

    /* The DEC graphics set, which `ESC ( 0` selects. It is a flag and not a
       table here because the translation lives in the parser. */
    int dec_graphics;
} TermVt;

/* --- life ----------------------------------------------------------------- */

/* Set up the parser on a grid of this size. Both screens are made. Returns 0
   on success. */
int  term_vt_init(TermVt *vt, int cols, int rows);

void term_vt_free(TermVt *vt);

/* Reshape BOTH screens. A window resize is the terminal's size and not a
   program's choice, so the screen that is not showing is resized too: a
   program that switches screens after a resize must find the other one the
   same size, or its cursor arithmetic is against a window that no longer
   exists. */
int  term_vt_resize(TermVt *vt, int cols, int rows);

/* --- the one entry point -------------------------------------------------- */

/* Feed bytes from the program through the parser. This is the only function
   the rest of the terminal calls to make a program's output appear: the bytes
   arrive from the PTY and leave as edits to the grid.
 *
 * A character is not written until the bytes that make it up have all arrived,
   so a call may leave the parser holding the start of a multi-byte character
   or of an escape sequence. That is the normal case and not an error: a read
   from a PTY may end anywhere, and a parser that needed whole sequences per
   call would fail on a program whose output was split across two reads.
 */
void term_vt_feed(TermVt *vt, const char *bytes, int len);

/* The screen that is showing. The renderer draws this one. */
const TermGrid *term_vt_screen(const TermVt *vt);

/* Whether the cursor is drawn. It is the terminal's and not the screen's —
   see TermVt.cursor_visible — so the renderer asks here and not of the grid. */
int term_vt_cursor_visible(const TermVt *vt);

/* --- what a program asks for --------------------------------------------- */

/* Whether the program asked for mouse reporting, and in which protocol — the
   SGR form (`?1006`) can express a column past 223 and the original cannot. */
int  term_vt_wants_mouse(const TermVt *vt);
int  term_vt_mouse_is_sgr(const TermVt *vt);

/* Whether the program wants pasted text bracketed, so it can tell a paste from
   typing. It is forwarded to the shell as the two sequences around the text
   and not interpreted here. */
int  term_vt_wants_bracketed_paste(const TermVt *vt);

/* Whether the arrow keys should send SS3 (`ESC O A`) rather than CSI
   (`ESC [ A`). A program that reads keys in a mode of its own asks for this,
   and sending the wrong form makes its arrow keys do nothing. */
int  term_vt_wants_application_cursor(const TermVt *vt);

/* How many sequences this terminal was sent and did not implement. */
long term_vt_unknown_count(const TermVt *vt);

#endif /* GNUCHANTERM_VT_H */
