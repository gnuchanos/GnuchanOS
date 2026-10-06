/*
 * top_ui.h — the main loop: sample, draw, read a key, repeat.
 *
 * This is the piece that ties the modules together, and it is deliberately the
 * only one that knows about all of them. The loop is: wait for the update
 * interval or a key, whichever comes first; sample every source; draw a frame;
 * act on the key. Nothing blocks: the wait is a poll with a timeout, so a key
 * arrives the moment it is pressed and a sample happens the moment it is due.
 *
 * The keys it acts on are the ones a monitor needs: the arrows and page keys
 * move the process cursor, `k` kills the selected process (after a confirmation
 * so a fat finger does not), `s` cycles the sort, `q` or Escape quits. Anything
 * else is ignored.
 */
#ifndef GNUCHANTOP_UI_H
#define GNUCHANTOP_UI_H

#include "top_config.h"
#include "top_cpu.h"
#include "top_mem.h"
#include "top_gpu.h"
#include "top_proc.h"
#include "top_graph.h"

/* Everything the loop owns between frames. */
typedef struct {
    TopConfig  config;
    TopCpu     cpu;
    TopMem     mem;
    TopGpu     gpu;
    TopProcs   procs;
    TopHistory cpu_history;
    TopHistory gpu_history;
    int        samples;
} TopState;

/* Set the state up before the loop: the config read, the histories empty, the
   GPU's presence probed. Returns 0, or -1 when the terminal is not usable. */
int top_ui_init(TopState *state);

/* Run until the person quits. The terminal must already be in raw mode (see
   top_term_enter); this returns when a quit key or a quit signal arrives, and
   the caller restores the terminal. Returns 0 always. */
int top_ui_run(TopState *state);

#endif /* GNUCHANTOP_UI_H */
