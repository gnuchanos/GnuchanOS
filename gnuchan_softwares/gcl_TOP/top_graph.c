/*
 * top_graph.c — the block-character history graph.
 *
 * One text cell per sample. A cell is eight eighths tall in the block ramp,
 * so a `height`-row graph has `height * 8` levels and a sample of 100 fills it
 * all. For a given cell and text row the amount to fill is the sample's level
 * minus the levels already taken by the rows below it, clamped to the cell:
 *
 *     level       = value / 100 * height * 8            (0 .. height*8)
 *     cell_fill   = level - row * 8                     (clamped to 0..8)
 *
 * and a fill of 0 is a space, 8 is the full block, and 1..7 is that many
 * eighths — the ramp `▁▂▃▄▅▆▇`. The bottom-up count is what makes a taller
 * value reach a higher row, which is what a graph the right way up means.
 *
 * The ramp is U+2581..U+2588, three bytes each in UTF-8, so the graph is
 * written as those bytes by hand rather than through a wide-character library:
 * the program sets LC_ALL at start-up and every terminal here is UTF-8, and
 * doing the three bytes directly keeps this file free of a locale dependency it
 * does not otherwise need.
 */
#include "top_graph.h"

#include <math.h>
#include <stdio.h>

void top_history_init(TopHistory *history) {
    history->count = 0;
    history->head = 0;
}

void top_history_push(TopHistory *history, double value) {
    if (value < 0.0) {
        value = 0.0;
    }
    if (value > 100.0) {
        value = 100.0;
    }
    history->samples[history->head] = value;
    history->head = (history->head + 1) % TOP_HISTORY;
    if (history->count < TOP_HISTORY) {
        history->count++;
    }
}

double top_history_at(const TopHistory *history, int index) {
    /* index 0 is the newest. */
    if (index < 0 || index >= history->count) {
        return 0.0;
    }
    int position = history->head - 1 - index;
    while (position < 0) {
        position += TOP_HISTORY;
    }
    return history->samples[position];
}

/* Write the block for `eighths` filled eighths of a cell: 0 is a space, 8 is
   the full block, 1..7 is the matching partial block. */
static void write_block(int eighths) {
    if (eighths <= 0) {
        putchar(' ');
        return;
    }
    if (eighths > 8) {
        eighths = 8;
    }
    /* U+2588 is the full block; U+2581..U+2587 are one to seven eighths, which
       sit at 0x2580 + eighths. */
    unsigned int code = (eighths == 8) ? 0x2588u : (unsigned int)(0x2580 + eighths);
    char out[4];
    out[0] = (char)(0xE0 | (code >> 12));
    out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    out[2] = (char)(0x80 | (code & 0x3F));
    out[3] = '\0';
    fputs(out, stdout);
}

void top_graph_draw(const TopHistory *history, int width, int height, int row,
                    int red, int green, int blue) {
    if (width < 1 || height < 1 || row < 0 || row >= height) {
        return;
    }
    double levels = (double)height * 8.0;

    printf("\033[38;2;%d;%d;%dm", red, green, blue);
    for (int cell = 0; cell < width; cell++) {
        /* The newest sample sits at the right edge, so a cell counts back from
           the end of the window by how far it is from that edge. */
        int age = width - 1 - cell;
        double value = top_history_at(history, age);
        double level = (value / 100.0) * levels;
        /* How much of this cell is filled: the level less the rows under it. */
        double fill = level - (double)row * 8.0;
        int eighths = (int)floor(fill + 0.5);
        write_block(eighths);
    }
    fputs("\033[0m", stdout);
}
