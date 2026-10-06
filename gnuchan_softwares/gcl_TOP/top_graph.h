/*
 * top_graph.h — the little history graphs, drawn with braille.
 *
 * A graph is a rolling window of the last TOP_HISTORY samples of a 0..100
 * value. Drawing it in the terminal is the braille trick: the Unicode braille
 * block is a 2x4 grid of dots in one cell, so one text cell holds eight
 * samples — two columns of four. A row of cells is therefore 2*cols samples
 * wide and 4*samples_per_row tall, which is more resolution than a monitor
 * needs and comes out of characters every UTF-8 terminal already has.
 *
 * The history itself lives with whoever owns the value — the CPU's own history
 * is kept by the CPU, the GPU's by the GPU, and the drawing takes a pointer to
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

/* Draw `rows` text rows of the history into `out`, as braille, `width` cells
   wide. `red`,`green`,`blue` is the colour the dots are written in. The caller
   has already positioned the cursor; this writes the escapes, the cells and a
   reset. A row is drawn top to bottom, so the newest sample is at the right. */
void top_graph_draw(const TopHistory *history, int width, int rows,
                    int red, int green, int blue);

#endif /* GNUCHANTOP_GRAPH_H */
