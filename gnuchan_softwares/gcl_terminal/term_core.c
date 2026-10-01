/*
 * term_core.c — the X connection, the window, and the event loop.
 *
 * See term_core.h for what the core owns and how the two halves of the program
 * meet. What is here is the window, the loop, and the module list.
 *
 * --- the loop waits on TWO things, and that is the whole of it ---
 *
 * The obvious loop is XPending() and XNextEvent(), and it is wrong: the PTY is
 * a file descriptor and its bytes are not X events. A loop that only watches
 * the display would read the child's output only when something happened on the
 * display — so `ls` would draw when the window was moved, a shell prompt would
 * appear when the mouse twitched, and btop would freeze for as long as the
 * pointer was still.
 *
 * So the loop selects on BOTH: the X connection's own file descriptor, which is
 * what XConnectionNumber() is for, and the PTY master. Whichever has something
 * wakes the loop. That is the one piece of this program that is not bookkeeping,
 * and getting it wrong produces a terminal that "works" as long as you keep
 * moving the mouse.
 *
 * --- the timeout is the module intervals ---
 *
 * select() has to stop eventually or a module's tick never runs, and the
 * renderer's tick is the cursor blink. The timeout is the smallest interval any
 * registered module asked for, and it is capped so a module that asked for a
 * very long one still lets the loop turn over.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
/* XClassHint, XSetClassHint and XSetWMProtocols live here and not in Xlib.h.
   Without this the window setup below does not compile at all — which a
   compiler reports as a handful of unrelated "unknown type" errors in the
   middle of the file, not as a missing include. */
#include <X11/Xutil.h>

#include "term_core.h"
#include "term_pty.h"
#include "term_style.h"
#include "term_render.h"
#include "term_input.h"
#include "term_select.h"
#include "term_suggest.h"

/* The scrollbar's width in pixels. It is a fixed number of pixels and not a
   cell: a bar sized in cells would be eight pixels on one font and twenty on
   another, and this is a piece of furniture rather than a column of text. */
#define TERM_SCROLLBAR_WIDTH 8

/* The shortest the thumb may be drawn, so a history of ten thousand lines
   still has something to see and something to grab. */
#define TERM_SCROLLBAR_MIN_THUMB 16

/* The window's own event mask. StructureNotify is what a resize arrives as and
   ExposureMask what a redraw request does; both are the core's to subscribe to
   because it is the core that knows a window exists. */
#define TERM_EVENT_MASK (KeyPressMask | KeyReleaseMask | \
                         ButtonPressMask | ButtonReleaseMask | \
                         PointerMotionMask | StructureNotifyMask | \
                         ExposureMask | FocusChangeMask)

/* --- the clock -------------------------------------------------------------
 *
 * CLOCK_MONOTONIC and not the wall clock: the debounce and the blink measure
 * how long something took, and a clock that jumps when the time is set would
 * make a blink hang for hours and a resize fire immediately. The choice is the
 * whole of the reason there is a function here at all.
 */
unsigned long term_core_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

/* --- the history ---------------------------------------------------------- */

/* The half of the contract the HISTORY answers: the grid hands a line over and
   asks for one back, and these are what the ring does with them.
 *
 * They are here rather than in term_scroll.c because the ring has no business
   knowing what a grid is, and they are not in the grid because the grid has no
   business keeping four thousand lines. The core owns both, so the core
   introduces them. */
static int history_push(void *user, TermCell *cells, int cols) {
    return term_scroll_push((TermScroll *)user, cells, cols);
}

static void history_clear(void *user) {
    term_scroll_clear((TermScroll *)user);
}

static int history_count(void *user) {
    return term_scroll_count((TermScroll *)user);
}

static const TermCell *history_line(void *user, int index, int *cols) {
    TermScroll *scroll = (TermScroll *)user;
    if (cols != NULL) {
        *cols = term_scroll_cols(scroll);
    }
    return term_scroll_cells(scroll, index);
}

/* The ring's offset and the grid's copy of it are the same number kept in two
   places, and this is the one place they are put in step.
 *
 * They are kept apart because their lifetimes differ — the ring moves when a
   program prints, the grid's copy is only read while drawing — and because the
   grid must not know what a TermScroll is. But two copies of anything drift,
   so every move of the view goes through here. */
static void core_sync_view(TermCore *core) {
    TermGrid *grid = &core->vt.grid;
    int was = term_grid_view_offset(grid);
    int now = term_scroll_offset(&core->scroll);
    if (was == now) {
        return;
    }
    term_grid_set_view_offset(grid, now);

    /* Every row of the picture changes identity when the view moves, so every
       row has to be drawn again.
     *
     * This is the same trap the scroll region has and it is worth naming: the
     * dirty marks belong to the LINES, and a scroll of the view moves no line
     * at all — the rows simply read from somewhere else. A frame that trusted
     * the marks would find them all clean and draw nothing, and the wheel would
     * appear to do nothing while the ring's offset changed underneath it.
     *
     * Marking the rows is what says "what is on the window is no longer what
     * these rows hold". */
    for (int y = 0; y < grid->rows; y++) {
        grid->lines[y].dirty = 1;
    }
}

void term_core_scroll_by(TermCore *core, int lines) {
    if (term_scroll_by(&core->scroll, lines)) {
        core_sync_view(core);
        term_core_damage(core);
    }
}

void term_core_scroll_to_bottom(TermCore *core) {
    if (term_scroll_offset(&core->scroll) == 0) {
        return;
    }
    term_scroll_to_bottom(&core->scroll);
    core_sync_view(core);
    term_core_damage(core);
}

int term_core_scrolled_back(const TermCore *core) {
    return term_scroll_offset(&core->scroll) > 0;
}

/* --- the scrollbar -------------------------------------------------------- */

void term_core_scrollbar_rect(const TermCore *core, int *x, int *width,
                              int *track_y, int *track_h,
                              int *thumb_y, int *thumb_h) {
    int kept = term_scroll_count(&core->scroll);
    int rows = core->vt.grid.rows;
    int cell_h = core->style != NULL ? core->style->cell_height : 16;
    if (cell_h < 1) cell_h = 16;

    /* No history means no scrollbar. It is not drawn greyed out or drawn full:
       a terminal that has printed nothing has nothing above it, and a bar that
       appeared anyway would be a bar that does nothing when dragged. */
    int visible = kept > 0;

    int track_h_value = rows > 0 ? rows * cell_h : 0;
    int thumb_h_value = 0;
    int thumb_y_value = 0;

    if (visible && track_h_value > 0) {
        /* The thumb's length is the screen's share of the whole: how much of
           everything there is, is on the screen. A history of nine hundred
           lines with twenty-four on the screen gives a thumb a fortieth of the
           track, which is what tells the eye there is a lot above. */
        int whole = kept + rows;
        thumb_h_value = track_h_value * rows / whole;
        if (thumb_h_value < TERM_SCROLLBAR_MIN_THUMB) {
            thumb_h_value = TERM_SCROLLBAR_MIN_THUMB;
        }
        if (thumb_h_value > track_h_value) {
            thumb_h_value = track_h_value;
        }

        /* Where the thumb goes. The offset counts BACK from the newest line
           and the scrollbar counts DOWN from the top, so the two are opposite:
           at the top of the history the offset is at its largest and the thumb
           is at the top. */
        int travel = track_h_value - thumb_h_value;
        int offset = term_scroll_offset(&core->scroll);
        int most = kept > 0 ? kept : 1;
        thumb_y_value = travel * (most - offset) / most;
    }

    if (x != NULL) {
        *x = core->width - TERM_SCROLLBAR_WIDTH;
    }
    if (width != NULL) *width = visible ? TERM_SCROLLBAR_WIDTH : 0;
    if (track_y != NULL) *track_y = 0;
    if (track_h != NULL) *track_h = track_h_value;
    if (thumb_y != NULL) *thumb_y = thumb_y_value;
    if (thumb_h != NULL) *thumb_h = thumb_h_value;
}

int term_core_scrollbar_offset_at(const TermCore *core, int y) {
    int x = 0;
    int width = 0;
    int track_y = 0;
    int track_h = 0;
    int thumb_y = 0;
    int thumb_h = 0;
    term_core_scrollbar_rect(core, &x, &width, &track_y, &track_h,
                             &thumb_y, &thumb_h);
    if (track_h <= 0 || thumb_h <= 0) {
        return 0;
    }

    int kept = term_scroll_count(&core->scroll);
    int travel = track_h - thumb_h;
    if (travel <= 0 || kept <= 0) {
        return 0;
    }

    /* The thumb is grabbed by its middle: a drag puts the line under the
       pointer at the middle of the thumb, which is where the hand thinks it
       is holding it. */
    int place = y - track_y - thumb_h / 2;
    if (place < 0) place = 0;
    if (place > travel) place = travel;

    /* The inverse of where the thumb was put: the top of the track is the
       oldest line, and so names the LARGEST offset. */
    int from_top = kept * place / travel;
    int offset = kept - from_top;
    if (offset < 0) offset = 0;
    if (offset > kept) offset = kept;
    return offset;
}

void term_core_set_view_offset(TermCore *core, int offset) {
    if (offset < 0) offset = 0;
    if (offset > term_scroll_count(&core->scroll)) {
        offset = term_scroll_count(&core->scroll);
    }
    if (offset == term_scroll_offset(&core->scroll)) {
        return;
    }
    /* The ring owns the number, so it is moved to it rather than assigned —
       term_scroll_by() clamps against the history as it is NOW, which is the
       only clamp that stays correct as lines arrive. */
    term_scroll_by(&core->scroll, offset - term_scroll_offset(&core->scroll));
    core_sync_view(core);
    term_core_damage(core);
}

void term_core_claim_event(TermCore *core) {
    core->event_claimed = 1;
}

int term_core_event_claimed(const TermCore *core) {
    return core->event_claimed;
}

/* --- the parser's two ways of speaking ------------------------------------
 *
 * The parser writes ANSWERS back to the program (a device-attributes reply, a
 * cursor position) and it REPORTS the strings the terminal has a use for. Both
 * are function pointers on the host so the parser never learns what a PTY or a
 * suggestion is.
 *
 * Both take the CORE as their user pointer, and the write reaches the PTY
 * through core->pty rather than being handed it. That is what lets one user
 * pointer serve two callbacks: the alternative is a second pointer on the host,
 * and the two would have to be kept in step for no gain. core->pty is NULL
 * until the shell has been spawned, and the check below is why an answer asked
 * for before then is dropped rather than written into nothing. */
static void core_vt_write(void *user, const char *bytes, int len) {
    TermCore *core = (TermCore *)user;
    if (core != NULL && core->pty != NULL) {
        term_pty_write((TermPty *)core->pty, bytes, len);
    }
}

/* An OSC 133 marker: the shell saying where its prompt ended.
 *
 * The body is `133;<letter>` and the letter is what matters — see
 * term_suggest.h. Only the marker is looked at; every other OSC is still
 * consumed by the parser and dropped, so a window title never reaches here. */
static void core_vt_osc(void *user, const char *body, int len) {
    TermCore *core = (TermCore *)user;
    if (core == NULL || core->suggest == NULL || len < 5) {
        return;
    }
    term_suggest_marker((TermSuggest *)core->suggest, core, body[4]);
}

/* --- life ----------------------------------------------------------------- */

/* Transform a cell count into a pixel count, and back. Both directions are
   here because both are needed: the window is made from a cell count and a
   resize gives pixels that have to become cells. */
void term_core_cell_size(const TermCore *core, int *cell_width,
                         int *cell_height) {
    *cell_width = core->style != NULL ? core->style->cell_width : 8;
    *cell_height = core->style != NULL ? core->style->cell_height : 16;
    if (*cell_width < 1) *cell_width = 8;
    if (*cell_height < 1) *cell_height = 16;
}

/* --- the frame and the bar ------------------------------------------------ */

void term_core_grid_origin(const TermCore *core, int *x, int *y) {
    int cell_w = 0;
    int cell_h = 0;
    term_core_cell_size(core, &cell_w, &cell_h);
    /* The bar is at the BOTTOM, so the grid starts at the very top and only the
       margin pushes it in from the left. Its bottom edge is then where the bar
       begins, which is why nothing has to be added for it here: the grid's own
       height is the window less the margin and the bar, as the core computed
       when it sized it. */
    if (x != NULL) *x = TERM_PAD_CELLS * cell_w;
    if (y != NULL) *y = 0;
}

const char *term_core_title(const TermCore *core) {
    return core->title;
}

/* Read the child's working directory and put it in the bar.
 *
 * /proc/<pid>/cwd is a symbolic link to the directory, so readlink() on it is
 * one syscall and not a parse of anything. It is read rather than asked for
 * through the PTY on purpose: asking means sending an escape sequence the shell
 * has to answer, and a shell that is busy — or running btop — never will. The
 * bar would then freeze on whatever directory the shell was in when it stopped
 * reading, which is the one moment its answer is stale.
 *
 * The home directory is folded to `~`, which is what a shell prompt does and
 * what makes a path fit: /home/kubi/Projects/GnuchanOS is four fifths of a line
 * and ~/Projects/GnuchanOS is half of one.
 */
void term_core_refresh_title(TermCore *core) {
    if (core->pty == NULL) {
        return;
    }
    TermPty *pty = (TermPty *)core->pty;
    if (pty->child <= 0) {
        return;
    }

    char link[64];
    snprintf(link, sizeof(link), "/proc/%ld/cwd", (long)pty->child);

    char target[TERM_TITLE_MAX];
    ssize_t got = readlink(link, target, sizeof(target) - 1);
    if (got <= 0) {
        return;
    }
    target[got] = '\0';

    /* The home directory is matched on a boundary and not on a prefix: /home/k
       must not fold to ~ubi. */
    const char *home = getenv("HOME");
    char built[TERM_TITLE_MAX];
    size_t home_len = (home != NULL) ? strlen(home) : 0;
    if (home_len > 0 && strncmp(target, home, home_len) == 0 &&
        (target[home_len] == '/' || target[home_len] == '\0')) {
        snprintf(built, sizeof(built), "~%s", target + home_len);
    } else {
        snprintf(built, sizeof(built), "%s", target);
    }

    if (strcmp(built, core->title) != 0) {
        snprintf(core->title, sizeof(core->title), "%s", built);
        core->title_dirty = 1;
        /* The bar has to be drawn again, and the frame the renderer would
           otherwise skip on an idle screen is exactly the one that draws it. */
        term_core_damage(core);
    }
}

int term_core_register(TermCore *core, const TermModule *module) {
    if (core->modules.count >= TERM_MAX_MODULES) {
        return -1;
    }
    core->modules.items[core->modules.count++] = module;
    return 0;
}

/* The smallest interval any module asked for, capped. Zero when none did, which
   makes the loop block until something happens. */
static int module_timeout_ms(const TermCore *core) {
    int smallest = 0;
    for (int i = 0; i < core->modules.count; i++) {
        const TermModule *module = core->modules.items[i];
        int interval = module->tick != NULL ? module->interval_ms : 0;
        if (interval <= 0) {
            continue;
        }
        if (interval > TERM_MAX_TICK_MS) {
            interval = TERM_MAX_TICK_MS;
        }
        if (smallest == 0 || interval < smallest) {
            smallest = interval;
        }
    }
    return smallest;
}

/* The font to open, in the order it is chosen: what the settings script named,
   then $GCL_TERMINAL_FONT, then nothing — and nothing means the style's own
   built-in, which is monospace-11.
 *
 * The environment variable is second and not first because a settings file is
 * a deliberate act and an exported variable is usually an accident of the
 * session that started the terminal. A person who writes a font in their
 * settings means it; a shell that has GCL_TERMINAL_FONT exported from
 * somewhere is offering a suggestion. */
static const char *core_font_choice(const char *from_config) {
    if (from_config != NULL && from_config[0] != '\0') {
        return from_config;
    }
    const char *from_env = getenv("GCL_TERMINAL_FONT");
    if (from_env != NULL && from_env[0] != '\0') {
        return from_env;
    }
    return NULL;
}

int term_core_init(TermCore *core, const char *title, const char *font_name) {
    /* The module list is INCLUDED in the clear below, so it is taken out
       first. The header says modules are registered before this is called,
       and a memset over the whole struct would erase the list that was just
       built — leaving a window that opens, draws nothing, and ignores every
       key, because nothing is left to draw or to listen. */
    struct TermModuleList modules = core->modules;

    memset(core, 0, sizeof(*core));

    core->modules = modules;
    core->running = 1;
    core->cols = 80;
    core->rows = 24;
    term_scroll_init(&core->scroll);

    /* The selection is made here and not by a module because two things that
       are not modules need it: the renderer asks it what to highlight and the
       input module asks it to start a drag. It needs no display of its own —
       the clipboard is only touched when a copy is asked for — so it can be
       made before the display is opened below. */
    core->select = (TermSelect *)calloc(1, sizeof(TermSelect));
    if (core->select == NULL) {
        return -1;
    }
    term_select_init((TermSelect *)core->select);

    core->display = XOpenDisplay(NULL);
    if (core->display == NULL) {
        return -1;
    }
    core->screen = DefaultScreen(core->display);
    core->visual = DefaultVisual(core->display, core->screen);
    core->depth = DefaultDepth(core->display, core->screen);

    /* The font comes before the window, because the font is what says how big
       a cell is and the window is sized in cells. */
    core->style = (TermStyle *)calloc(1, sizeof(TermStyle));
    if (core->style == NULL) {
        XCloseDisplay(core->display);
        core->display = NULL;
        return -1;
    }
    if (term_style_init(core->style, core->display, core->screen,
                        core_font_choice(font_name)) != 0) {
        /* No font means nothing can be drawn, and a terminal that opens to a
           blank window is worse than one that does not open. */
        fprintf(stderr, "gcl_terminal: cannot open a font\n");
        free(core->style);
        core->style = NULL;
        XCloseDisplay(core->display);
        core->display = NULL;
        return -1;
    }

    int cell_w = 0;
    int cell_h = 0;
    term_core_cell_size(core, &cell_w, &cell_h);
    /* The window is the grid PLUS the frame, or the bar and the margin would
       cover the first row and the last column of what the child was told it
       had. */
    core->width = core->cols * cell_w + 2 * TERM_PAD_CELLS * cell_w;
    core->height = core->rows * cell_h + TERM_BAR_ROWS * cell_h;

    core->window = XCreateSimpleWindow(
        core->display, RootWindow(core->display, core->screen),
        0, 0, (unsigned)core->width, (unsigned)core->height, 0,
        BlackPixel(core->display, core->screen),
        BlackPixel(core->display, core->screen));
    if (core->window == None) {
        term_style_free(core->style);
        free(core->style);
        XCloseDisplay(core->display);
        core->display = NULL;
        return -1;
    }

    XStoreName(core->display, core->window,
               title != NULL ? title : "GnuChanTerm");
    XSelectInput(core->display, core->window, TERM_EVENT_MASK);

    /* The window manager is told to treat this as a terminal rather than to
       guess from its size: without the hint, a terminal opened at 80x24 is a
       dialog, and it gets no title bar of its own. */
    XClassHint class_hint;
    class_hint.res_name = (char *)"gcl_terminal";
    class_hint.res_class = (char *)"GnuChanTerm";
    XSetClassHint(core->display, core->window, &class_hint);

    core->gc = XCreateGC(core->display, core->window, 0, NULL);

    if (term_vt_init(&core->vt, core->cols, core->rows) != 0) {
        XFreeGC(core->display, core->gc);
        XDestroyWindow(core->display, core->window);
        term_style_free(core->style);
        free(core->style);
        XCloseDisplay(core->display);
        core->display = NULL;
        return -1;
    }

    /* The MAIN screen keeps its lines above it; the alternate one does not.
     *
     * That asymmetry is the whole of what a scrollback is. A shell prints and
     * scrolls and the user wants to read what went by. btop paints every cell
     * of a screen that has no history at all — the "lines above" it are last
     * frame's picture, and keeping them would make scrolling back through a
     * full-screen program show garbage the program never meant anyone to see.
     * So the history is attached to one grid and not the other. */
    core->history.user = &core->scroll;
    core->history.push = history_push;
    core->history.clear = history_clear;
    core->history.count = history_count;
    core->history.line = history_line;
    term_grid_attach_history(&core->vt.grid, &core->history);

    /* The parser's answers go back through the PTY, which does not exist yet —
       the module that spawns the child fills the PTY in, and the callbacks
       above read core->pty at the moment they are called rather than now. */
    core->vt_host.write = core_vt_write;
    core->vt_host.osc = core_vt_osc;
    core->vt_host.user = core;
    core->vt.host = core->vt_host;

    XMapWindow(core->display, core->window);
    XFlush(core->display);
    return 0;
}

int term_core_start(TermCore *core) {
    int failed = 0;
    for (int i = 0; i < core->modules.count; i++) {
        const TermModule *module = core->modules.items[i];
        if (module->init == NULL) {
            continue;
        }
        if (module->init(core) != 0) {
            fprintf(stderr, "gcl_terminal: module '%s' failed to start\n",
                    module->name);
            failed = 1;
        }
    }
    return failed ? -1 : 0;
}

/* --- the grid and the window ---------------------------------------------- */

void term_core_request_size(TermCore *core, int width, int height) {
    if (width < 1 || height < 1) {
        return;
    }
    core->pending_width = width;
    core->pending_height = height;
    core->last_resize_ms = term_core_now_ms();

    /* The first size is applied at once and not debounced: a window that has
       just been mapped would otherwise show its old grid for the length of the
       debounce, which at start-up is the whole first screen. */
    if (core->width != width || core->height != height) {
        term_core_apply_resize(core);
    }
}

void term_core_apply_resize(TermCore *core) {
    int width = core->pending_width;
    int height = core->pending_height;
    if (width < 1 || height < 1) {
        return;
    }
    if (width == core->width && height == core->height) {
        return;
    }

    core->width = width;
    core->height = height;

    int cell_w = 0;
    int cell_h = 0;
    term_core_cell_size(core, &cell_w, &cell_h);

    /* The frame comes off BEFORE the division into cells, because it is cells
       that are being subtracted and the child is told the count that is left.
       A program told the full window would draw its last row under the bar and
       its last column against the edge, and btop would lose the bottom of every
       panel it draws.
     *
     * A window dragged smaller than the frame clamps to one cell rather than to
       zero: a grid of no cells is a grid every write into it has to be guarded
       against, and the terminal on a 20 pixel window has bigger problems. */
    int area_w = width - 2 * TERM_PAD_CELLS * cell_w;
    int area_h = height - TERM_BAR_ROWS * cell_h;
    if (area_w < cell_w) area_w = cell_w;
    if (area_h < cell_h) area_h = cell_h;

    int cols = area_w / cell_w;
    int rows = area_h / cell_h;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    core->cols = cols;
    core->rows = rows;

    /* The grid follows the window, and BOTH screens are resized: a program
       that switches to the other one after a resize must find it the same
       size. */
    term_vt_resize(&core->vt, cols, rows);

    /* The child is told, and the kernel raises SIGWINCH to it as part of that —
       see term_pty_set_size(). A program that redraws on the signal redraws
       now, and one that polls the size sees the same number. */
    if (core->pty != NULL) {
        term_pty_set_size((TermPty *)core->pty, cols, rows);
    }

    /* The buffer is the window's size in pixels and has to be rebuilt, which
       also means the whole of it needs drawing again. */
    if (core->render != NULL) {
        term_render_resize(core, width, height);
    }
    term_core_damage(core);
}

/* --- redraw --------------------------------------------------------------- */

void term_core_damage(TermCore *core) {
    core->needs_draw = 1;
}

int term_core_needs_draw(const TermCore *core) {
    return core->needs_draw;
}

void term_core_draw_done(TermCore *core) {
    core->needs_draw = 0;
}

/* --- the loop -------------------------------------------------------------- */

/* The PTY, read and drawn.
 *
 * This is what the loop does when the CHILD has something to say, and it is
 * separate from the X path because the two are woken by different sources. A
 * terminal that read the PTY only on an X event would draw the shell's output
 * when the user moved the mouse. */
static void core_pump_pty(TermCore *core) {
    if (core->pty == NULL) {
        return;
    }
    TermPty *pty = (TermPty *)core->pty;

    int was_back = term_core_scrolled_back(core);

    int got = term_pty_pump(pty, &core->vt);
    if (got > 0) {
        /* The program has printed, so the view comes back to the live screen.
         *
         * The rule is every terminal's and it is not a courtesy: a shell that
         * has answered a command while the user was reading history would
         * otherwise be a shell whose prompt is off the bottom of the screen,
         * unseen, and the user would type into a view that is not the prompt.
         * The scrollback is still there and the wheel still reaches it; the
         * screen simply follows the program, which is what a terminal IS.
         *
         * It is done only when the view was actually back, so a `yes` that
         * prints a million lines does not go through the ring's arithmetic a
         * million times for nothing. */
        if (was_back || term_core_scrolled_back(core)) {
            term_scroll_to_bottom(&core->scroll);
            core_sync_view(core);
        } else {
            /* The view was already live, but the lines that scrolled off still
               moved the ring — the grid's copy has to follow. */
            core_sync_view(core);
        }
        /* Anything the program printed means something to draw. It is not a
           judgement about whether the screen changed — the parser marked the
           cells that did — it is a request for a frame. */
        term_core_damage(core);
    }
    if (got < 0 || !term_pty_is_alive(pty)) {
        /* The shell exited. There is nothing left to show and nothing that will
           arrive, so the terminal closes: a window on an empty screen that
           cannot be typed into is a window that does nothing. */
        core->running = 0;
    }
}

/* One event from the display, to every module. */
static void core_dispatch_event(TermCore *core, XEvent *event) {
    /* The window manager may have closed us. It is the core's own event and it
       ends the loop rather than being handed on: a module that saw it would
       have nothing to do with it and the window is gone either way. */
    if (event->type == ClientMessage) {
        Atom wm_delete = XInternAtom(core->display, "WM_DELETE_WINDOW", False);
        if ((Atom)event->xclient.data.l[0] == wm_delete) {
            core->running = 0;
            return;
        }
    }

    if (event->type == DestroyNotify &&
        event->xdestroywindow.window == core->window) {
        core->running = 0;
        return;
    }

    /* The selection protocol is the core's own, like the window manager's
       close request above: it arrives on the display whether or not any
       module is interested, and the answer has to be sent or every paste on
       the desktop hangs. It is handled before the modules so a module never
       sees an event that was never about it. */
    /* The three selection events, and the third is the one that was missing:
       a PASTE does not arrive when the key is pressed, it arrives here, as a
       SelectionNotify carrying the text another program was asked for. Without
       this case the answer was read as an ordinary event and thrown away, so
       Ctrl+Shift+V asked a question nothing ever listened for. */
    if (event->type == SelectionRequest ||
        event->type == SelectionClear ||
        event->type == SelectionNotify) {
        term_select_event(core, event);
        return;
    }

    /* A resize is settled here and not by the renderer, because the grid and
       the PTY both depend on it and the renderer is only one of the two. */
    if (event->type == ConfigureNotify &&
        event->xconfigure.window == core->window) {
        term_core_request_size(core, event->xconfigure.width,
                              event->xconfigure.height);
    }

    for (int i = 0; i < core->modules.count; i++) {
        const TermModule *module = core->modules.items[i];
        if (module->event != NULL) {
            /* The flag is cleared BEFORE each module, not after: a module that
               claimed the event has said "nobody after me wants this", and a
               module that found it already set is looking at an event that is
               spoken for and must leave it alone. */
            core->event_claimed = 0;
            module->event(core, event);
            if (core->event_claimed) {
                break;
            }
        }
    }
}

void term_core_step(TermCore *core) {
    if (core->display == NULL) {
        return;
    }

    /* Everything already queued is dispatched first, so a burst of events —
       a drag, or a paste arriving as many KeyPress — costs one select and not
       one per event. */
    while (core->running && XPending(core->display) > 0) {
        XEvent event;
        XNextEvent(core->display, &event);
        core_dispatch_event(core, &event);
    }
    if (!core->running) {
        return;
    }

    int x_fd = ConnectionNumber(core->display);
    int pty_fd = core->pty != NULL ? ((TermPty *)core->pty)->master : -1;

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(x_fd, &read_set);
    int max_fd = x_fd;
    if (pty_fd >= 0) {
        FD_SET(pty_fd, &read_set);
        if (pty_fd > max_fd) {
            max_fd = pty_fd;
        }
    }

    /* The timeout is how long the loop may wait with nothing happening, and it
       is what makes a module's tick happen at all. When no module wanted one it
       is NULL, which blocks until something arrives — a terminal on an idle
       session must not wake to do nothing. */
    int timeout = module_timeout_ms(core);
    struct timeval wait;
    struct timeval *wait_ptr = NULL;
    if (timeout > 0) {
        wait.tv_sec = timeout / 1000;
        wait.tv_usec = (timeout % 1000) * 1000;
        wait_ptr = &wait;
    }

    int ready = select(max_fd + 1, &read_set, NULL, NULL, wait_ptr);

    if (ready < 0) {
        if (errno == EINTR) {
            /* A signal — SIGWINCH from a resize, or SIGCHLD — interrupted the
               wait. It is not an error and the loop turns over. */
            return;
        }
        core->running = 0;
        return;
    }

    if (ready > 0 && pty_fd >= 0 && FD_ISSET(pty_fd, &read_set)) {
        core_pump_pty(core);
    }

    /* A pending resize is applied once the window has been quiet for the
       debounce, so a drag does not resize the child once per pixel. */
    if (core->pending_width > 0 &&
        term_core_now_ms() - core->last_resize_ms >= TERM_RESIZE_DEBOUNCE_MS) {
        term_core_apply_resize(core);
    }

    /* The modules' ticks. A module with an interval is called when its interval
       has passed; the renderer's is what draws the frame, and it is called even
       when select() returned an event — a frame that waited for the timeout
       would lag a keystroke by the interval. */
    unsigned long now = term_core_now_ms();

    /* The bar follows the child's directory, a few times a second. It is here
       and not in the PTY path because the directory changes with no output to
       show for it: `cd` prints nothing, and a bar that waited for a byte would
       still be showing the directory the shell started in. */
    if (now - core->last_title_ms >= TERM_TITLE_INTERVAL_MS) {
        core->last_title_ms = now;
        term_core_refresh_title(core);
    }

    static unsigned long last_tick[TERM_MAX_MODULES];
    for (int i = 0; i < core->modules.count; i++) {
        const TermModule *module = core->modules.items[i];
        if (module->tick == NULL || module->interval_ms <= 0) {
            continue;
        }
        int interval = module->interval_ms;
        if (interval > TERM_MAX_TICK_MS) {
            interval = TERM_MAX_TICK_MS;
        }
        if (i < TERM_MAX_MODULES &&
            now - last_tick[i] >= (unsigned long)interval) {
            module->tick(core);
            last_tick[i] = now;
        }
    }
}

void term_core_run(TermCore *core) {
    while (core->running) {
        term_core_step(core);
    }
}

void term_core_shutdown(TermCore *core) {
    /* The modules come down in reverse order, so a module that was registered
       last — and so may depend on the ones before it — is finished with
       first. */
    for (int i = core->modules.count - 1; i >= 0; i--) {
        const TermModule *module = core->modules.items[i];
        if (module->cleanup != NULL) {
            module->cleanup(core);
        }
    }

    if (core->pty != NULL) {
        term_pty_close((TermPty *)core->pty);
        free(core->pty);
        core->pty = NULL;
    }
    if (core->style != NULL) {
        term_style_free(core->style);
        free(core->style);
        core->style = NULL;
    }
    term_vt_free(&core->vt);
    /* The ring's buffers belong to the ring, and the grid that gave them away
       holds none of them — there is nothing double-freeing here. */
    term_scroll_free(&core->scroll);
    if (core->select != NULL) {
        term_select_free((TermSelect *)core->select);
        free(core->select);
        core->select = NULL;
    }

    if (core->display != NULL) {
        if (core->gc != NULL) {
            XFreeGC(core->display, core->gc);
            core->gc = NULL;
        }
        if (core->window != None) {
            XDestroyWindow(core->display, core->window);
            core->window = None;
        }
        XCloseDisplay(core->display);
        core->display = NULL;
    }
}
