/*
 * top_draw.h — drawing one frame.
 *
 * A frame is the whole screen, drawn top to bottom in one pass: the CPU and GPU
 * panels across the top (CPU left, GPU right, btop's shape), the memory line,
 * then the process list filling the rest. The drawing owns the cursor: it moves
 * it where it is needed and clears the lines it writes, so a shorter line does
 * not leave the tail of the previous one behind.
 *
 * Everything it needs comes in one struct — the samples, the list, the config,
 * the terminal size — so the function is a pure drawing step with no state of
 * its own between frames beyond the layout it computes from the size.
 */
#ifndef GNUCHANTOP_DRAW_H
#define GNUCHANTOP_DRAW_H

#include "top_config.h"
#include "top_cpu.h"
#include "top_mem.h"
#include "top_gpu.h"
#include "top_proc.h"
#include "top_graph.h"
#include "top_term.h"

/* Everything one frame is drawn from. */
typedef struct {
    const TopConfig   *config;
    const TopCpu      *cpu;
    const TopMem      *mem;
    const TopGpu      *gpu;
    const TopProcs    *procs;
    const TopHistory  *cpu_history;
    const TopHistory  *gpu_history;
    TopSize            size;
    unsigned long long uptime_seconds;
    int                sample_number;   /* frames drawn so far, for the header */
} TopFrame;

/* Draw one whole frame. The caller has already cleared or positioned nothing:
   this moves the cursor itself. */
void top_draw_frame(const TopFrame *frame);

/* Draw a one-line message at the foot of the screen — a confirmation, an error.
   `is_error` colours it red rather than the accent. `width` is the terminal
   width, so the line is padded to overwrite whatever was there. */
void top_draw_status(const char *message, int is_error, int width, int rows);

#endif /* GNUCHANTOP_DRAW_H */
