/*
 * runner_draw.c — the whole window, painted in one pass.
 *
 * The window is three stacked pieces:
 *
 *   +------------------------------------------+
 *   |  Run: fire|                              |   the query line
 *   +------------------------------------------+
 *   |  Firefox                                  |   a match
 *   |  Files                                    |   another
 *   |  ...                                      |
 *   +------------------------------------------+
 *
 * The query line is at the top and full width, because it is what a person
 * looks at while typing and its position never moves. The rows below scroll:
 * the chosen one is kept inside the visible set, and the list moves under it.
 *
 * Every colour comes from the style, and every size from the config or the
 * font. Nothing here is a number that was not either written down or measured,
 * which is what makes a font of a different size or a palette of different
 * colours come out looking deliberate rather than broken.
 */
#include <stdio.h>
#include <string.h>

#include "runner_draw.h"

/* The room between the query line and the first row, so the two do not run
   into one another. */
#define DRAW_GAP 2

/* The width the prompt text takes, which is where the typed query starts and
   so where the caret is drawn. Recomputed here rather than kept, because it is
   one measurement of a short string and keeping it would be one more thing for
   the loop to forget to update. */
static int prompt_width(const RunnerUi *ui) {
    return runner_style_text_width(ui->display, ui->style.font,
                                   ui->config.prompt) +
           ui->style.padding;
}

/* Draw one row: the program's name, and its description after it in the
   quieter colour when the name does not fill the row. The name is drawn in the
   accent when the row is the chosen one, which is the whole of what "chosen"
   looks like. */
static void draw_row(const RunnerUi *ui, int y, const char *name,
                     const char *comment, int chosen) {
    int left = ui->style.padding;
    int baseline = y + (ui->style.row_height +
                        (ui->style.font ? ui->style.font->ascent -
                                          ui->style.font->descent : 0)) / 2;

    unsigned long name_colour = chosen ? ui->style.accent : ui->style.text;
    runner_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                      left, baseline, name, name_colour);

    if (comment && comment[0]) {
        int name_width = runner_style_text_width(ui->display, ui->style.font,
                                                 name);
        int comment_x = left + name_width + ui->style.padding * 2;
        /* The description is only drawn when there is room for it: a row whose
           name already reaches the right edge would otherwise have the
           description drawn past the window, which the server clips into a
           smear at the edge. */
        int room = ui->width - ui->style.padding - comment_x;
        if (room > 20) {
            runner_style_text(ui->display, ui->screen, ui->window,
                              ui->style.font, comment_x, baseline, comment,
                              ui->style.text_muted);
        }
    }
}

/* The query line: the prompt, then what has been typed, then the caret.
 *
 * When nothing has been typed the placeholder is drawn in the muted colour
 * instead, which is what turns an empty line into one that says what to do. */
static void draw_query(const RunnerUi *ui) {
    int baseline = (ui->style.row_height +
                    (ui->style.font ? ui->style.font->ascent -
                                      ui->style.font->descent : 0)) / 2;

    runner_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                      ui->style.padding, baseline, ui->config.prompt,
                      ui->style.accent);

    int x = prompt_width(ui);

    if (ui->query_length == 0) {
        runner_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                          x, baseline, ui->config.placeholder,
                          ui->style.text_muted);
        return;
    }

    runner_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                      x, baseline, ui->query, ui->style.text);

    /* The caret, at the end of what was typed. It is a filled rectangle rather
       than a character, because no font's vertical bar is the height a caret
       should be and a bar drawn as text sits on the baseline instead of
       through it. */
    int caret_x = x + runner_style_text_width(ui->display, ui->style.font,
                                              ui->query);
    XSetForeground(ui->display, ui->gc, ui->style.accent);
    XFillRectangle(ui->display, ui->window, ui->gc, caret_x,
                   (ui->style.row_height - 14) / 2, 2, 14);
}

/* The list: one row per match, from the scroll position, as many as fit. */
static void draw_rows(const RunnerUi *ui) {
    int top = ui->style.row_height + DRAW_GAP;
    int visible = (ui->height - top) / ui->style.row_height;

    if (ui->matches.count == 0) {
        /* Nothing found. The message is drawn in the muted colour, centred in
           the room the list would have taken, so an empty result reads as an
           answer rather than as a window that failed to draw. */
        int baseline = top + (ui->style.row_height +
                              (ui->style.font ? ui->style.font->ascent -
                                                ui->style.font->descent : 0)) / 2;
        runner_style_text(ui->display, ui->screen, ui->window, ui->style.font,
                          ui->style.padding, baseline, ui->config.empty_message,
                          ui->style.text_muted);
        return;
    }

    for (int row = 0; row < visible; row++) {
        int index = ui->scroll + row;
        if (index >= ui->matches.count) {
            break;
        }
        int y = top + row * ui->style.row_height;
        int chosen = (index == ui->selected);

        if (chosen) {
            XSetForeground(ui->display, ui->gc, ui->style.field);
            XFillRectangle(ui->display, ui->window, ui->gc, 0, y,
                           (unsigned int)ui->width,
                           (unsigned int)ui->style.row_height);
        }

        if (index == ui->command_row) {
            /* The command line: what was typed, run as a command rather than
               looked up. It is shown as the command with a word in front, so
               it is not mistaken for a program of that exact name. */
            char line[RUNNER_MAX_QUERY + 16];
            snprintf(line, sizeof(line), "run: %s", ui->query);
            draw_row(ui, y, line, NULL, chosen);
        } else {
            const RunnerProgram *program =
                &ui->programs.items[ui->matches.items[index].program_index];
            draw_row(ui, y, program->name, program->comment, chosen);
        }
    }
}

void runner_draw(const RunnerUi *ui) {
    if (!ui->display || ui->window == None || !ui->gc) {
        return;
    }

    /* The window behind everything, then the panel the rows sit on. The
       background is painted over the whole window so no part of the previous
       frame shows through: the launcher redraws on every key, and a list that
       shortened without the window being cleared first would leave the last
       row of the longer list underneath. */
    XSetForeground(ui->display, ui->gc, ui->style.background);
    XFillRectangle(ui->display, ui->window, ui->gc, 0, 0,
                   (unsigned int)ui->width, (unsigned int)ui->height);

    XSetForeground(ui->display, ui->gc, ui->style.panel);
    XFillRectangle(ui->display, ui->window, ui->gc, 0, 0,
                   (unsigned int)ui->width,
                   (unsigned int)ui->style.row_height);

    /* The hairline between the query line and the list, which is what tells
       the two apart when they are the same colour. */
    XSetForeground(ui->display, ui->gc, ui->style.panel_edge);
    XDrawLine(ui->display, ui->window, ui->gc, 0, ui->style.row_height,
              ui->width, ui->style.row_height);

    /* The edge, last so it sits over the fills. */
    XSetForeground(ui->display, ui->gc, ui->style.panel_edge);
    XDrawRectangle(ui->display, ui->window, ui->gc, 0, 0,
                   (unsigned int)(ui->width - 1),
                   (unsigned int)(ui->height - 1));

    draw_query(ui);
    draw_rows(ui);

    XFlush(ui->display);
}
