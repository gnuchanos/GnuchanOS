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

struct TermStyle;
struct TermPty;
struct TermRender;

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

    /* The palette and the font — see term_style.h. One of each, shared by
       everything that draws, so a second set of colours cannot appear. */
    struct TermStyle *style;

    /* The child process and the pseudo terminal — see term_pty.h. */
    struct TermPty *pty;

    /* The frame buffer and the drawing — see term_render.h. */
    struct TermRender *render;

    /* Where answers to the program's queries go. It is the parser's host and
       it is filled in by the PTY module. */
    TermVtHost vt_host;

    /* A redraw is asked for by setting this, not by calling the renderer: the
       renderer stays a module like any other and the core does not have to
       know that drawing happens. */
    int needs_draw;

    /* When the last size was applied, for the debounce above. */
    unsigned long last_resize_ms;

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
   them after it — see term_core_start(). */
int  term_core_init(TermCore *core, const char *title);

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

#endif /* GNUCHANTERM_CORE_H */
