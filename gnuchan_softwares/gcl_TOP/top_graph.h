/*
 * top_graph.h — the little history graphs.
 *
 * A graph is a rolling window of the last TOP_HISTORY samples of a 0..100
 * value. It is drawn with the Unicode block characters — the eighth-height
 * ramp `▁▂▃▄▅▆▇█` and the space between them — one text cell per sample, so a
 * `width`-wide block shows the last `width` samples with the newest at the
 * right.
 *
 * Blocks rather than braille on purpose. Braille is denser (a cell holds a 2x4
 * grid) but it is a font that has to have the U+28xx characters, and a terminal
 * without them shows a row of empty boxes — a graph that looks like a
 * placeholder. The block ramp is in every console font, so the graph reads the
 * same everywhere, and one cell per sample is resolution enough for a spark of
 * the last few seconds.
 *
 * The history itself lives with whoever owns the value — the CPU's own history
 * is kept by the CPU, the GPU's by the GPU — and the drawing takes a pointer to
 * it. This module only turns numbers into a coloured block of cells.
 */
#ifndef GNUCHANTOP_GRAPH_H
#define GNUCHANTOP_GRAPH_H

#include "top_common.h"

/* A ring of samples in 0..100, oldest at `start`. */
typedef struct {
    double samples[TOP_HISTORY];
    int    count;   /* how many are filled, up to TOP_HISTORY */
    int    head;    /* where the next sample goes */
} TopHistory;

/* Start with an empty history. */
void top_history_init(TopHistory *history);

/* Add a sample, dropping the oldest when full. */
void top_history_push(TopHistory *history, double value);

/* The value `index` samples back from the newest, 0..count-1. */
double top_history_at(const TopHistory *history, int index);

/* Draw one text row of a `height`-tall graph, `width` cells wide, newest on the
   right. `row` is 0 at the top and `height - 1` at the bottom. One sample per
   cell; a sample's value picks how many of the cell's eight eighths are filled,
   so a value of 50 in a 5-row graph fills two and a half rows. The caller has
   already positioned the cursor; this writes the colour, the cells and a
   reset. */
void top_graph_draw(const TopHistory *history, int width, int height, int row,
                    int red, int green, int blue);

#endif /* GNUCHANTOP_GRAPH_H */
