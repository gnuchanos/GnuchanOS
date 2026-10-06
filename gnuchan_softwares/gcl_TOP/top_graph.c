/*
 * top_graph.c — the braille history graph.
 *
 * The braille block U+2800..U+28FF encodes eight dots in one cell: the low six
 * bits are two columns of three (dots 1,2,3 on the left and 4,5,6 on the right)
 * and the top two bits (0x40 and 0x80) are dots 7 and 8, the two extra rows
 * under the first three. That is a 2x4 grid, and this file lays a graph out on
 * it: a cell column is two sample columns, a cell row is four dot rows.
 *
 * The whole graph is one block of `rows` text rows, drawn newest-on-the-right.
 * Each sample has an x position (its place in the window) and its value picks a
 * distance up from the bottom, 0..(rows*4). A dot is set when a sample's height
 * reaches that dot row: that fills a bar under each sample rather than a single
 * point, which is what makes a busy history read as a filled shape rather than a
 * scatter of dots.
 *
 * The window holds the last `2*width` samples, so a narrow graph shows less of
 * the past rather than a shrunk copy of it.
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

/* The dot bit for the cell grid position (column 0/1, row 0..3). This is the
   braille dot numbering, which is not a simple bit position. */
static unsigned char dot_bit(int column, int row) {
    static const unsigned char bits[2][4] = {
        {0x01, 0x02, 0x04, 0x40},  /* left column: dots 1,2,3,7 */
        {0x08, 0x10, 0x20, 0x80},  /* right column: dots 4,5,6,8 */
    };
    return bits[column][row];
}

/* Write a single UTF-8 braille character for the code point 0x2800 + pattern. */
static void write_braille(unsigned char pattern) {
    unsigned int code = 0x2800u + pattern;
    char out[4];
    out[0] = (char)(0xE0 | (code >> 12));
    out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    out[2] = (char)(0x80 | (code & 0x3F));
    out[3] = '\0';
    fputs(out, stdout);
}

void top_graph_draw(const TopHistory *history, int width, int rows,
                    int red, int green, int blue) {
    if (width < 1 || rows < 1) {
        return;
    }
    int dot_rows = rows * 4;   /* vertical dots over the whole block */
    int sample_columns = width * 2;

    /* The window: the newest `sample_columns` samples, oldest at the left. */
    for (int row = 0; row < rows; row++) {
        printf("\033[38;2;%d;%d;%dm", red, green, blue);
        for (int cell = 0; cell < width; cell++) {
            unsigned char pattern = 0;
            for (int sub_column = 0; sub_column < 2; sub_column++) {
                int sample_column = cell * 2 + sub_column;
                /* The sample that belongs at this x, counted back from the
                   newest which sits at the right edge. */
                int age = sample_columns - 1 - sample_column;
                double value = top_history_at(history, age);
                int height = (int)floor((value / 100.0) * (double)dot_rows + 0.5);
                if (height > dot_rows) {
                    height = dot_rows;
                }
                /* A dot at cell row `row`, sub-row `sub` is lit when the
                   sample's height from the bottom reaches it. */
                for (int sub = 0; sub < 4; sub++) {
                    int dot_from_bottom = (rows - 1 - row) * 4 + (4 - 1 - sub);
                    if (dot_from_bottom < height) {
                        pattern |= dot_bit(sub_column, sub);
                    }
                }
            }
            write_braille(pattern);
        }
        fputs("\033[0m", stdout);
        if (row + 1 < rows) {
            fputs("\n", stdout);
        }
    }
}
