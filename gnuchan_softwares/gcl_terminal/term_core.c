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

int term_core_init(TermCore *core, const char *title) {
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
                        getenv("GCL_TERMINAL_FONT")) != 0) {
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
    core->width = core->cols * cell_w;
    core->height = core->rows * cell_h;

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

    /* The parser's answers go back through the PTY, which does not exist yet —
       the module that spawns the child fills this in. */
    core->vt_host.write = NULL;
    core->vt_host.user = NULL;
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
    int cols = width / cell_w;
    int rows = height / cell_h;
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

    int got = term_pty_pump(pty, &core->vt);
    if (got > 0) {
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
            module->event(core, event);
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
