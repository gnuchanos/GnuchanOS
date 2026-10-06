/*
 * top_ui.c — the main loop.
 *
 * One turn of the loop is: work out how long until the next sample, wait that
 * long for a key, sample, draw, act on the key. The wait is a poll, so a key
 * that arrives early cuts the wait short and redraws at once, which is what
 * makes the arrow keys feel immediate while the numbers still update on their
 * own timer.
 *
 * The kill is confirmed first. `k` puts a question on the status line and the
 * following key answers it: `y` sends SIGTERM, `n` or any other key cancels.
 * That is the whole of the safety on a destructive key, and it is the same shape
 * btop uses.
 */
#include "top_ui.h"
#include "top_term.h"
#include "top_draw.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The machine's uptime in seconds, from /proc/uptime. */
static unsigned long long uptime_seconds(void) {
    FILE *file = fopen("/proc/uptime", "r");
    if (!file) {
        return 0;
    }
    double seconds = 0.0;
    int matched = fscanf(file, "%lf", &seconds);
    fclose(file);
    if (matched != 1 || seconds < 0.0) {
        return 0;
    }
    return (unsigned long long)seconds;
}

/* The total CPU jiffies the last sample read, so processes can be measured
   against it. The CPU module keeps its own copy; this reads /proc/stat once more
   for the process share, which is one small file and keeps the two modules from
   having to share a parse. */
static long long total_jiffies(void) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        return 0;
    }
    long long total = 0;
    char line[512];
    if (fgets(line, sizeof(line), file)) {
        const char *cursor = line;
        while (*cursor && *cursor != ' ') {
            cursor++;
        }
        while (*cursor) {
            char *end = NULL;
            long long value = strtoll(cursor, &end, 10);
            if (end == cursor) {
                break;
            }
            total += value;
            cursor = end;
        }
    }
    fclose(file);
    return total;
}

int top_ui_init(TopState *state) {
    memset(state, 0, sizeof(*state));
    top_config_load(&state->config);
    top_procs_init(&state->procs);
    top_history_init(&state->cpu_history);
    top_history_init(&state->gpu_history);
    state->samples = 0;

    /* One sample of each before the loop so the first frame has numbers and the
       GPU's presence is known. The CPU's first sample is a baseline and its
       usage stays zero until the second; that is expected. */
    top_cpu_sample(&state->cpu);
    top_mem_sample(&state->mem);
    top_gpu_sample(&state->gpu);
    top_procs_sample(&state->procs, total_jiffies(), state->cpu.core_count,
                     &state->config);
    return 0;
}

/* Take every sample for one frame and push the histories. */
static void take_sample(TopState *state) {
    top_cpu_sample(&state->cpu);
    top_mem_sample(&state->mem);
    top_gpu_sample(&state->gpu);
    top_procs_sample(&state->procs, total_jiffies(), state->cpu.core_count,
                     &state->config);
    top_history_push(&state->cpu_history, state->cpu.overall);
    if (state->gpu.present && state->gpu.has_reading) {
        top_history_push(&state->gpu_history, state->gpu.usage);
    }
    top_procs_sort(&state->procs, state->config.sort);
    state->samples++;
}

/* Build and draw the frame for the current state. */
static void draw(TopState *state) {
    TopSize size = top_term_size();
    TopFrame frame;
    frame.config = &state->config;
    frame.cpu = &state->cpu;
    frame.mem = &state->mem;
    frame.gpu = &state->gpu;
    frame.procs = &state->procs;
    frame.cpu_history = &state->cpu_history;
    frame.gpu_history = &state->gpu_history;
    frame.size = size;
    frame.uptime_seconds = uptime_seconds();
    frame.sample_number = state->samples;
    top_draw_frame(&frame);
}

/* How many process rows fit, which the cursor movement needs. */
static int visible_rows(void) {
    TopSize size = top_term_size();
    /* The header(1) + band(PANEL_ROWS) + memory(1) + columns(1) + status(1). The
       draw code uses PANEL_ROWS of 6; mirroring it here keeps the cursor from
       scrolling off the visible list. */
    int used = 1 + 6 + 1 + 1 + 1;
    int rows = size.rows - used;
    return rows > 0 ? rows : 1;
}

/* Ask about a kill with the status line, read one key, and act. Returns 0, or 1
   when the process was killed (so the caller can redraw at once). */
static int confirm_kill(TopState *state) {
    TopProc *proc = top_procs_selected(&state->procs);
    if (!proc) {
        return 0;
    }
    TopSize size = top_term_size();
    char question[TOP_TEXT];
    snprintf(question, sizeof(question), "Kill %d (%.200s)? [y/N]", proc->pid,
             proc->name);
    top_draw_status(question, 0, size.cols, size.rows);

    TopKey key;
    if (!top_term_read_key(&key, 5000)) {
        top_draw_status("Kill cancelled", 0, size.cols, size.rows);
        return 0;
    }
    if (key.kind == TOP_KEY_CHAR && (key.ch == 'y' || key.ch == 'Y')) {
        char error[TOP_TEXT];
        if (top_procs_signal(proc->pid, SIGTERM, error, sizeof(error)) == 0) {
            char done[TOP_TEXT];
            snprintf(done, sizeof(done), "Sent SIGTERM to %d (%.200s)", proc->pid,
                     proc->name);
            top_draw_status(done, 0, size.cols, size.rows);
        } else {
            top_draw_status(error, 1, size.cols, size.rows);
        }
        return 1;
    }
    top_draw_status("Kill cancelled", 0, size.cols, size.rows);
    return 0;
}

/* Cycle the sort key: cpu -> mem -> pid -> name -> cpu, and remember it in the
   config so a running program sorts the way the last `s` asked. */
static void cycle_sort(TopState *state) {
    switch (state->config.sort) {
        case TOP_SORT_CPU:  state->config.sort = TOP_SORT_MEM;  break;
        case TOP_SORT_MEM:  state->config.sort = TOP_SORT_PID;  break;
        case TOP_SORT_PID:  state->config.sort = TOP_SORT_NAME; break;
        case TOP_SORT_NAME:
        default:            state->config.sort = TOP_SORT_CPU;  break;
    }
    top_procs_sort(&state->procs, state->config.sort);
}

int top_ui_run(TopState *state) {
    int interval_ms = (int)(state->config.update_time * 1000.0);
    if (interval_ms < 100) {
        interval_ms = 100;
    }

    int needs_draw = 1;
    int running = 1;
    while (running) {
        if (needs_draw) {
            draw(state);
            needs_draw = 0;
        }

        TopKey key;
        int got = top_term_read_key(&key, interval_ms);

        if (got) {
            switch (key.kind) {
                case TOP_KEY_UP:
                    top_procs_move(&state->procs, -1, visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_DOWN:
                    top_procs_move(&state->procs, 1, visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_PAGE_UP:
                    top_procs_move(&state->procs, -visible_rows(), visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_PAGE_DOWN:
                    top_procs_move(&state->procs, visible_rows(), visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_HOME:
                    top_procs_move(&state->procs, -state->procs.count,
                                   visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_END:
                    top_procs_move(&state->procs, state->procs.count,
                                   visible_rows());
                    needs_draw = 1;
                    break;
                case TOP_KEY_ESCAPE:
                    running = 0;
                    break;
                case TOP_KEY_CHAR:
                    if (key.ch == 'q' || key.ch == 'Q') {
                        running = 0;
                    } else if (key.ch == 'k' || key.ch == 'K') {
                        confirm_kill(state);
                        needs_draw = 1;
                    } else if (key.ch == 's' || key.ch == 'S') {
                        cycle_sort(state);
                        needs_draw = 1;
                    }
                    break;
                default:
                    break;
            }
        } else {
            /* The wait timed out: a sample is due. */
            take_sample(state);
            needs_draw = 1;
        }

        if (top_term_take_quit()) {
            running = 0;
        }
        if (top_term_take_resized()) {
            needs_draw = 1;
        }
    }
    return 0;
}
