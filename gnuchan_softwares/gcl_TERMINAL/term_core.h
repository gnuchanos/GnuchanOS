/*
 * term_core.h — the X connection, the window, and the event loop.
 *
 * The core owns exactly one of each X-wide thing: the Display, the window, the
 * event loop, and the state modules are given. Modules never open their own
 * display and never run their own loop — the same division the window manager
 * uses, and for the same reason: two loops would race to read events and one
 * of them would lose keys.
 *
 * --- how the two halves meet ---
 *
 * This program is two programs in one process. An X program on one side, with
 * a window and a keyboard and a set of colours; and a terminal on the other,
 * which is a PTY with a child process at the far end and a cell grid in
 * between. The core is where they meet, and the meeting is one-directional in
 * each:
 *
 *     the child's bytes       ->  term_vt_feed()   ->  the grid changes
 *     a key the user presses  ->  encode to bytes  ->  write to the PTY
 *
 * Nothing writes to the grid except the parser, and nothing writes to the PTY
 * except the input module. That is what keeps a keystroke from being able to
 * draw, and a program's output from being able to press a key.
 */
 
#ifndef GNUCHANTERM_CORE_H
#define GNUCHANTERM_CORE_H

#include <X11/Xlib.h>

#include "term_module.h"
#include "term_scroll.h"
#include "term_vt.h"

/* How many bytes are read from the PTY in one go. A program that prints a full
   screen — btop redrawing, or `cat` of a large file — sends far more than this
   per frame, and a smaller buffer means more system calls per byte. */
#define TERM_PTY_READ_SIZE 65536

/* A resize is applied at most this often, in milliseconds. A window being
   dragged produces a ConfigureNotify per pixel, and resizing the grid and
   telling the child each time is a storm the child cannot keep up with. The
   last one in the window wins, which is what the user sees anyway. */
#define TERM_RESIZE_DEBOUNCE_MS 50

/* The frame the terminal draws around the grid.
 *
 * One cell of margin left and right, and one row of bar BELOW showing where
 * the child process is. The margin is not decoration: text against the window
 * edge reads as clipped, and a shell prompt starting at x=0 looks jammed into
 * the corner. The bar is the terminal's own — the child knows nothing about it
 * and is told the grid is exactly the space left above it, so a full-screen
 * program still gets an honest size.
 *
 * The bar is at the BOTTOM because that is where a terminal's own furniture
 * belongs: the top-left cell is where the eye starts and where a program's
 * first line goes, and the row that costs least to lose is the last one.
 *
 * Both are counted in CELLS rather than pixels because both have to be
 * subtracted from the pixel size before it is divided into cells: a margin of
 * a fixed number of pixels would drift out of step with the font.
 */
#define TERM_PAD_CELLS 1
#define TERM_BAR_ROWS  1

/* The longest path the bar holds. A deeper path is shown from its start and
   cut; the bar is one line and the beginning of a path is still worth seeing. */
#define TERM_TITLE_MAX 512

/* How often the child's working directory is re-read, in milliseconds. It is a
   readlink() on /proc and cheap, but not free, and a shell's cwd changes when
   the user types `cd` — not between frames. */
#define TERM_TITLE_INTERVAL_MS 250

struct TermStyle;
struct TermPty;
struct TermRender;
struct TermSelect;
struct TermSuggest;

typedef struct TermCore {
    Display *display;
    int      screen;
    Window   window;
    GC       gc;
    Visual  *visual;
    int      depth;

    /* The window's size in pixels, and the grid's in cells. Both are needed:
       the renderer draws in pixels and the program is told the cell count, and
       the conversion between them is the font's cell size. */
    int width;
    int height;
    int cols;
    int rows;

    int running;

    /* The cell grid and the escape parser. The renderer reads the active
       screen out of it; nothing else touches the grids. */
    TermVt vt;

    /* The lines above the screen. It belongs to the CORE and not to the
       parser, because the alternate screen must not have one: the parser
       switches screens, and it is the core that decides that only the main
       screen's lines are worth keeping.
     *
     * It lives here rather than in a module because two modules need it and
     * neither owns the other: the renderer draws it and the input module
     * scrolls it. The core is the thing they already share. */
    TermScroll scroll;

    /* Whether the scrollbar's thumb is being DRAGGED. It is the core's because
       the button went down in one event and the pointer moves in others: the
       state has to outlive the call that began the drag, and the core is the
       only thing every module already shares.

       The three fields that used to be here — scrollbar_visible, scrollbar_x
       and scrollbar_width — were written by nothing and read by nothing: the
       geometry is computed on demand by term_core_scrollbar_rect(), which is
       the one place it can disagree with the input module's hit test. They are
       gone rather than left behind as three names for a scrollbar that never
       was. */
    int scrollbar_dragging;

    /* What the main grid asks when a line leaves the top of it, and when it
       wants one back. term_core_init() fills this in with the ring above and
       the four calls in term_core.c — see term_grid.h for TermGridHistory. It
       lives in the core because the core owns both halves of that contract. */
    TermGridHistory history;

    /* Set by a module that has consumed an event, so the modules after it in
       the list do not act on it a second time.
     *
     * The modules are given every event in order and there is no other way for
     * one to say "this click was mine": without a flag, a click on the
     * scrollbar would scroll the view AND be sent to the program as a mouse
     * click, because the input module has no way to know the select module
     * already took it. The core clears it before each dispatch, so a module
     * only ever claims the event in front of it. */
    int event_claimed;

    /* The palette and the font — see term_style.h. One of each, shared by
       everything that draws, so a second set of colours cannot appear. */
    struct TermStyle *style;

    /* The child process and the pseudo terminal — see term_pty.h. */
    struct TermPty *pty;

    /* The frame buffer and the drawing — see term_render.h. */
    struct TermRender *render;

    /* What the mouse has selected, and the clipboard it feeds — see
       term_select.h. It is the core's because the drag reaches into the
       history and the clipboard has to outlive the key that filled it, neither
       of which is a module's business. */
    struct TermSelect *select;

    /* The fish-style suggestion and the command history it is drawn from — see
       term_suggest.h. It is the core's for the same reason the selection is:
       the input module is what acts on it (Tab, and the arrows for browsing)
       and the renderer is what draws the ghost, and neither owns the other. */
    struct TermSuggest *suggest;

    /* Where answers to the program's queries go. It is the parser's host and
       it is filled in by the PTY module. */
    TermVtHost vt_host;

    /* A redraw is asked for by setting this, not by calling the renderer: the
       renderer stays a module like any other and the core does not have to
       know that drawing happens. */
    int needs_draw;

    /* When the last size was applied, for the debounce above. */
    unsigned long last_resize_ms;

    /* The bar's text: the child's working directory with the home directory
       folded to `~`. It lives in the CORE and not in the renderer because
       reading it means knowing the child's pid, and the core owns the PTY. */
    char title[TERM_TITLE_MAX];
    int  title_dirty;
    unsigned long last_title_ms;

    /* What the PROGRAM asked the window to be called, from OSC 0, 1 or 2. It
       is separate from `title` above and not the same thing: `title` is the
       bar's text, which the terminal reads from the child's directory, and
       this is the text the child sent to be the WINDOW's name — what the
       window manager shows in a title bar and what a task switcher lists.
       Empty means the program has not named itself, and the window keeps the
       name it was opened with. */
    char program_title[TERM_TITLE_MAX];

    /* The pending size, applied when the debounce expires. */
    int pending_width;
    int pending_height;

    struct TermModuleList {
        const TermModule *items[TERM_MAX_MODULES];
        int count;
    } modules;
} TermCore;

/* --- life ----------------------------------------------------------------- */

/* Open the display, make a window for an 80x24 grid, and set up the parser.
   Returns 0 on success. The caller registers modules before this and starts
   them after it — see term_core_start().

   `font_name` is the Xft font description to open, and the order it is chosen
   in is: this argument, then $GCL_TERMINAL_FONT, then the built-in default.
   It is a parameter rather than something the core looks up because the
   settings script is read by the caller — before there is a display, since
   the font is what sizes the window — and the core has no business opening
   the file a second time.

   The COLOURS from that same script are applied afterwards, by the caller,
   through term_config_apply_style(): they need the style to exist, and the
   style is made in here. */
int  term_core_init(TermCore *core, const char *title, const char *font_name);

/* Run every registered module's init, in registration order. Separate from
   term_core_init() because a module is registered before the core has a
   display, and a module's init needs that display to exist. Returns 0 if every
   module started; -1 if any of them did not. */
int  term_core_start(TermCore *core);

/* The loop. It blocks on the display and calls every module's event handler
   with each event, and every module with an interval gets its tick when that
   many milliseconds pass with nothing else happening. */
void term_core_run(TermCore *core);

/* One turn: wait for an event and dispatch it. Split out of term_core_run()
   so a caller can stop between events. */
void term_core_step(TermCore *core);

void term_core_shutdown(TermCore *core);

/* --- the grid and the window ---------------------------------------------- */

/* Reshape the grid to fit the window, and tell the child. The size is in
   pixels and is turned into cells with the font's measurements.

   Debounced: a resize arriving while another is pending replaces it, and the
   result is applied TERM_RESIZE_DEBOUNCE_MS after the last one. */
void term_core_request_size(TermCore *core, int width, int height);

/* Apply a pending resize now, without waiting for the debounce. Called by the
   loop when the time has passed. */
void term_core_apply_resize(TermCore *core);

/* --- what modules use ----------------------------------------------------- */

/* Register a module. Call before term_core_init(). Returns 0 on success, and
   -1 when the list is full — which is a line in a log and not a crash, because
   a terminal that refuses to start over one module is worse than one that
   starts without it. */
int  term_core_register(TermCore *core, const TermModule *module);

/* The size of one character cell in pixels. */
void term_core_cell_size(const TermCore *core, int *cell_width, int *cell_height);

/* Where the grid starts inside the window, in pixels. The renderer draws every
   cell at this offset and the input module takes it off a mouse position, so
   there is one place that knows the frame exists. */
void term_core_grid_origin(const TermCore *core, int *x, int *y);

/* The bar's text, and a request to re-read it. The renderer calls the first to
   draw it; the core's own tick calls the second. */
const char *term_core_title(const TermCore *core);
void term_core_refresh_title(TermCore *core);

/* Ask for a redraw. */
void term_core_damage(TermCore *core);

/* Whether a redraw has been asked for since the last one was carried out. */
int  term_core_needs_draw(const TermCore *core);

/* Say the redraw has been carried out. */
void term_core_draw_done(TermCore *core);

/* The milliseconds since some fixed point, for the debounce and for module
   intervals. It is the core's because a module with its own clock would drift
   against the others. */
unsigned long term_core_now_ms(void);

/* --- the lines above, and the scrollbar ----------------------------------- */

/* Move the view by a number of lines, clamped, and redraw if it moved. A
   positive number scrolls back. This is what the wheel, Page Up and a drag of
   the scrollbar all call. */
void term_core_scroll_by(TermCore *core, int lines);

/* Go back to the live screen. Anything the program prints does this — see
   term_core_pump_pty() — and so does a key typed at the prompt: a person who
   has scrolled back and then types is done reading. */
void term_core_scroll_to_bottom(TermCore *core);

/* Whether the view is scrolled back at all, and how far. */
int term_core_scrolled_back(const TermCore *core);

/* The scrollbar's place in the window, in pixels: its x and how wide the whole
   of it is, or 0 when there is nothing to scroll and it is not drawn.
 *
 * It is the core's because it is the window's: the renderer draws it, the
 * input module hit-tests it, and both have to be talking about the same
 * rectangle. Two computations of it would be two chances to disagree.
 *
 * `thumb_y` and `thumb_h` come back as the moving part's place and length
 * inside the track; `track_y` and `track_h` are the whole track. A click below
 * the thumb pages down, which is what a person means by clicking there. */
void term_core_scrollbar_rect(const TermCore *core, int *x, int *width,
                              int *track_y, int *track_h,
                              int *thumb_y, int *thumb_h);

/* Turn a pixel y inside the window into the line offset it names on the
   scrollbar. Used by a click and by a drag, so both land on the same line. */
int term_core_scrollbar_offset_at(const TermCore *core, int y);

/* Put the view at an offset a scrollbar computation arrived at. Separate from
   term_core_scroll_by() because that one moves BY a number and this one moves
   TO one, which is what a thumb's position names. */
void term_core_set_view_offset(TermCore *core, int offset);

/* Say that the event in hand has been dealt with and must not be passed on. */
void term_core_claim_event(TermCore *core);
int  term_core_event_claimed(const TermCore *core);

#endif /* GNUCHANTERM_CORE_H */
